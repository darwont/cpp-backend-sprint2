#pragma once
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <memory>
#include <iostream>

namespace http_server {

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

template <typename RequestHandler>
class Session : public std::enable_shared_from_this<Session<RequestHandler>> {
public:
    Session(tcp::socket&& socket, RequestHandler handler)
        : stream_(std::move(socket)), handler_(std::move(handler)) {}

    void Run() {
        net::dispatch(stream_.get_executor(), beast::bind_front_handler(&Session::Read, this->shared_from_this()));
    }

private:
    void Read() {
        request_ = {};
        stream_.expires_after(std::chrono::seconds(30));
        http::async_read(stream_, buffer_, request_, beast::bind_front_handler(&Session::OnRead, this->shared_from_this()));
    }

    void OnRead(beast::error_code ec, std::size_t) {
        if (ec == http::error::end_of_stream) {
            stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
            return;
        }
        if (ec) return;
        HandleRequest();
    }

    void HandleRequest() {
        auto endpoint = stream_.socket().remote_endpoint();
        handler_(endpoint, std::move(request_), [self = this->shared_from_this()](auto&& response) {
            auto safe_response = std::make_shared<std::decay_t<decltype(response)>>(std::move(response));
            http::async_write(self->stream_, *safe_response, [self, safe_response](beast::error_code ec, std::size_t) {
                if (!ec && !safe_response->need_eof()) {
                    self->Read();
                } else {
                    self->stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
                }
            });
        });
    }

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> request_;
    RequestHandler handler_;
};

template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
public:
    Listener(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler handler)
        : ioc_(ioc), acceptor_(net::make_strand(ioc)), handler_(std::move(handler)) {
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(net::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen(net::socket_base::max_listen_connections);
    }

    void Run() { DoAccept(); }

private:
    void DoAccept() {
        acceptor_.async_accept(net::make_strand(ioc_), beast::bind_front_handler(&Listener::OnAccept, this->shared_from_this()));
    }

    void OnAccept(beast::error_code ec, tcp::socket socket) {
        if (!ec) {
            std::make_shared<Session<RequestHandler>>(std::move(socket), handler_)->Run();
        }
        DoAccept();
    }

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    RequestHandler handler_;
};

template <typename RequestHandler>
void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler) {
    std::make_shared<Listener<std::decay_t<RequestHandler>>>(ioc, endpoint, std::forward<RequestHandler>(handler))->Run();
}

} // namespace http_server
