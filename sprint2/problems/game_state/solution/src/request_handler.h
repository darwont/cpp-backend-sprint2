#pragma once
#include "sdk.h"
#include "model.h"
#include <filesystem>

namespace http_handler {
namespace net = boost::asio;
namespace fs = std::filesystem;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, fs::path static_path)
        : game_{game}, static_path_{std::move(static_path)} {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(const boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>>& req, Send&& send) {
        // Логика обработки будет тут
    }

private:
    model::Game& game_;
    fs::path static_path_;
};
}  // namespace http_handler
