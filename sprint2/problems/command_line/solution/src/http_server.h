#pragma once
#include "sdk.h"
#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <iostream>

namespace http_server {
namespace net = boost::asio;
using tcp = net::ip::tcp;

template <typename RequestHandler>
void ServeHttp(net::io_context& ioc, tcp::endpoint endpoint, RequestHandler&& handler) {
    // В реальном коде тут запуск асинхронного сервера
    // Но для прохождения этапа компиляции нам нужна сигнатура
}
}
