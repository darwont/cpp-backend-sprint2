#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
#include <filesystem>
#include <variant>
#include <string>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace fs = std::filesystem;

struct Endpoints {
    static constexpr std::string_view MAPS = "/api/v1/maps";
    static constexpr std::string_view MAPS_PREFIX = "/api/v1/maps/";
    static constexpr std::string_view API_PREFIX = "/api/";
};

// Два типа ответов: текстовый (для JSON/ошибок) и файловый (для статики)
using StringResponse = http::response<http::string_body>;
using FileResponse = http::response<http::file_body>;
using ResponseVariant = std::variant<StringResponse, FileResponse>;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, fs::path static_path);

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        // Получаем вариант ответа и отправляем его через std::visit
        auto variant_response = HandleRequest(std::forward<decltype(req)>(req));
        std::visit([&send](auto&& result) {
            send(std::forward<decltype(result)>(result));
        }, variant_response);
    }

private:
    model::Game& game_;
    fs::path static_path_;

    // Главный маршрутизатор
    template <typename Body, typename Allocator>
    ResponseVariant HandleRequest(const http::request<Body, http::basic_fields<Allocator>>& req) const {
        if (req.target().starts_with(Endpoints::API_PREFIX)) {
            return HandleApiRequest(req);
        } else {
            return HandleStaticRequest(req);
        }
    }

    template <typename Body, typename Allocator>
    ResponseVariant HandleApiRequest(const http::request<Body, http::basic_fields<Allocator>>& req) const;

    template <typename Body, typename Allocator>
    ResponseVariant HandleStaticRequest(const http::request<Body, http::basic_fields<Allocator>>& req) const;

    // Помощник для создания текстовых ответов
    template <typename Body, typename Allocator>
    StringResponse MakeStringResponse(
        http::status status, const http::request<Body, http::basic_fields<Allocator>>& req,
        std::string_view body, std::string_view content_type = "application/json",
        std::string_view allow_methods = "") const {
        
        StringResponse res(status, req.version());
        res.set(http::field::content_type, content_type);
        if (!allow_methods.empty()) {
            res.set(http::field::allow, allow_methods);
        }
        res.body() = std::string(body);
        res.content_length(body.size());
        res.keep_alive(req.keep_alive());
        return res;
    }

    json::array SerializeMaps() const;
    json::object SerializeMap(const model::Map& map) const;
    
    std::string GetMimeType(std::string_view extension) const;
    bool IsSubPath(fs::path path, fs::path base) const;
    std::string UrlDecode(std::string_view url) const;
};

// Реализации шаблонных методов должны быть в заголовочном файле
template <typename Body, typename Allocator>
ResponseVariant RequestHandler::HandleApiRequest(const http::request<Body, http::basic_fields<Allocator>>& req) const {
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        return MakeStringResponse(http::status::method_not_allowed, req,
                                  "{\"code\": \"invalidMethod\", \"message\": \"Invalid method\"}",
                                  "application/json", "GET, HEAD");
    }

    auto target = req.target();
    if (target == Endpoints::MAPS || target == std::string(Endpoints::MAPS) + "/") {
        return MakeStringResponse(http::status::ok, req, json::serialize(SerializeMaps()));
    }

    if (target.starts_with(Endpoints::MAPS_PREFIX)) {
        std::string map_id(target.substr(Endpoints::MAPS_PREFIX.size()));
        const auto* map = game_.FindMap(map_id);
        if (!map) {
            return MakeStringResponse(http::status::not_found, req,
                                      "{\"code\": \"mapNotFound\", \"message\": \"Map not found\"}");
        }
        return MakeStringResponse(http::status::ok, req, json::serialize(SerializeMap(*map)));
    }

    return MakeStringResponse(http::status::bad_request, req,
                              "{\"code\": \"badRequest\", \"message\": \"Bad request\"}");
}

template <typename Body, typename Allocator>
ResponseVariant RequestHandler::HandleStaticRequest(const http::request<Body, http::basic_fields<Allocator>>& req) const {
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        return MakeStringResponse(http::status::method_not_allowed, req, "Invalid method", "text/plain", "GET, HEAD");
    }

    std::string target = UrlDecode(req.target());
    if (target.empty() || target == "/") {
        target = "/index.html"; // По умолчанию отдаем главную страницу
    }

    // Собираем абсолютный путь к файлу
    fs::path file_path = fs::weakly_canonical(static_path_ / target.substr(1));

    // ЗАЩИТА: Проверяем, не пытается ли хакер выйти за пределы папки static (Directory Traversal)
    if (!IsSubPath(file_path, static_path_)) {
        return MakeStringResponse(http::status::bad_request, req, "Bad Request", "text/plain");
    }

    if (!fs::exists(file_path) || fs::is_directory(file_path)) {
        return MakeStringResponse(http::status::not_found, req, "File not found", "text/plain");
    }

    http::file_body::value_type file;
    if (boost::system::error_code ec; file.open(file_path.string().c_str(), beast::file_mode::read, ec), ec) {
        return MakeStringResponse(http::status::internal_server_error, req, "Internal Server Error", "text/plain");
    }

    // Сохраняем размер файла до того, как переместим его
    auto file_size = file.size();

    FileResponse res(std::piecewise_construct, std::make_tuple(std::move(file)), std::make_tuple(http::status::ok, req.version()));
    res.set(http::field::content_type, GetMimeType(file_path.extension().string()));
    res.keep_alive(req.keep_alive());
    
    // Если это HEAD-запрос, отдаем только заголовки (без самого файла)
    if (req.method() == http::verb::head) {
        StringResponse empty_res(http::status::ok, req.version());
        empty_res.set(http::field::content_type, GetMimeType(file_path.extension().string()));
        empty_res.content_length(file_size); // <-- Вот тут мы передаем сохраненный размер!
        empty_res.keep_alive(req.keep_alive());
        return empty_res;
    }

    return res;
}

} // namespace http_handler