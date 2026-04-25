#pragma once
#include "http_server.h"
#include <boost/log/trivial.hpp>
#include <boost/json.hpp>
#include <chrono>

namespace server_logging {

extern boost::log::attributes::keyword<boost::json::value> additional_data;

template <typename RequestHandler>
class LoggingRequestHandler {
public:
    LoggingRequestHandler(RequestHandler handler) : handler_(std::move(handler)) {}

    template <typename Body, typename Allocator, typename Send>
    void operator()(boost::asio::ip::tcp::endpoint ep, boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>>&& req, Send&& send) {
        auto start_time = std::chrono::steady_clock::now();
        std::string uri = std::string(req.target());
        std::string method = std::string(req.method_string());
        std::string ip = ep.address().to_string();

        boost::json::value req_data{{"ip", ip}, {"URI", uri}, {"method", method}};
        BOOST_LOG_TRIVIAL(info) << boost::log::add_value(additional_data, req_data) << "request received"sv;

        auto logging_send = [send = std::forward<Send>(send), start_time, ip](auto&& response) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            
            boost::json::object res_data;
            res_data["ip"] = ip;
            res_data["response_time"] = duration;
            res_data["code"] = response.result_int();
            
            auto ct_it = response.find(boost::beast::http::field::content_type);
            if (ct_it != response.end()) {
                res_data["content_type"] = std::string(ct_it->value());
            } else {
                res_data["content_type"] = boost::json::value(nullptr);
            }

            BOOST_LOG_TRIVIAL(info) << boost::log::add_value(additional_data, res_data) << "response sent"sv;
            send(std::forward<decltype(response)>(response));
        };

        handler_(ep, std::move(req), logging_send);
    }

private:
    RequestHandler handler_;
};

} // namespace server_logging
