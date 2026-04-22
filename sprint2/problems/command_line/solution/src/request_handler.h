#pragma once
#include "http_server.h"
#include "app.h"
#include <boost/json.hpp>
#include <boost/asio/strand.hpp>
#include <filesystem>
#include <variant>

namespace http_handler {

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace fs = std::filesystem;

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    using Strand = net::strand<net::io_context::executor_type>;

    RequestHandler(fs::path www_root, Strand api_strand, app::Application& app)
        : www_root_(std::move(www_root))
        , api_strand_(api_strand)
        , app_(app) {}

    template <typename Body, typename Allocator, typename Send>
    void operator()(net::ip::tcp::endpoint ep, http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target(req.target());

        if (target.starts_with("/api/")) {
            auto handle = [self = shared_from_this(), send = std::forward<Send>(send), req = std::forward<decltype(req)>(req)]() mutable {
                send(self->HandleApiRequest(req));
            };
            net::dispatch(api_strand_, handle);
        } else {
            std::visit([&send](auto&& result) { send(std::forward<decltype(result)>(result)); }, HandleStaticRequest(req));
        }
    }

private:
    using FileRequestResult = std::variant<http::response<http::string_body>, http::response<http::file_body>>;

    FileRequestResult HandleStaticRequest(const http::request<http::string_body>& req) const;
    http::response<http::string_body> HandleApiRequest(const http::request<http::string_body>& req) const;

    fs::path www_root_;
    Strand api_strand_;
    app::Application& app_;
};

} // namespace http_handler
