#include "request_handler.h"
#include <boost/log/trivial.hpp>

namespace http_handler {

using namespace std::literals;
namespace json = boost::json;

RequestHandler::FileRequestResult RequestHandler::HandleStaticRequest(const http::request<http::string_body>& req) const {
    http::response<http::string_body> res(http::status::not_found, req.version());
    res.set(http::field::content_type, "text/plain");
    res.body() = "File not found";
    res.prepare_payload();
    return res; // Заглушка для компиляции
}

http::response<http::string_body> RequestHandler::HandleApiRequest(const http::request<http::string_body>& req) const {
    http::response<http::string_body> res(http::status::bad_request, req.version());
    res.set(http::field::content_type, "application/json");
    res.body() = "{\"code\": \"badRequest\", \"message\": \"Bad request\"}";
    res.prepare_payload();
    return res; // Заглушка для компиляции
}

} // namespace http_handler
