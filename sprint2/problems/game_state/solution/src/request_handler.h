#pragma once
#include "sdk.h"
#include "model.h"
#include <filesystem>
#include <boost/beast/http.hpp>

namespace http_handler {
namespace net = boost::asio;
namespace fs = std::filesystem;
namespace http = boost::beast::http;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, fs::path static_path)
        : game_{game}, static_path_{std::move(static_path)} {}

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        /* Logic will be here */
    }

private:
    model::Game& game_;
    fs::path static_path_;
};
}
