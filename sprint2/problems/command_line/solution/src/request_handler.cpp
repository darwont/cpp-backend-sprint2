#include "request_handler.h"
#include <sstream>

namespace http_handler {

using namespace std::literals;
namespace json = boost::json;

std::string RequestHandler::DecodeUrl(const std::string& url) const {
    std::string result;
    for (size_t i = 0; i < url.length(); ++i) {
        if (url[i] == '%') {
            if (i + 2 < url.length()) {
                int hex_val;
                std::istringstream is(url.substr(i + 1, 2));
                if (is >> std::hex >> hex_val) {
                    result += static_cast<char>(hex_val);
                    i += 2;
                }
            }
        } else if (url[i] == '+') {
            result += ' ';
        } else {
            result += url[i];
        }
    }
    return result;
}

bool RequestHandler::IsSubPath(fs::path path, fs::path base) const {
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);
    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) return false;
    }
    return true;
}

std::string RequestHandler::GetMimeType(const std::string& extension) const {
    std::string ext = extension;
    for (auto& c : ext) c = std::tolower(c);
    if (ext == ".htm" || ext == ".html") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".txt") return "text/plain";
    if (ext == ".js") return "text/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".xml") return "application/xml";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg" || ext == ".jpe") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".bmp") return "image/bmp";
    if (ext == ".ico") return "image/vnd.microsoft.icon";
    if (ext == ".tiff" || ext == ".tif") return "image/tiff";
    if (ext == ".svg" || ext == ".svgz") return "image/svg+xml";
    if (ext == ".mp3") return "audio/mpeg";
    return "application/octet-stream";
}

http::response<http::string_body> RequestHandler::MakeErrorResponse(http::status status, std::string_view code, std::string_view message, unsigned version, bool keep_alive) const {
    return MakeJsonResponse(status, {{"code", code}, {"message", message}}, version, keep_alive);
}

http::response<http::string_body> RequestHandler::MakeJsonResponse(http::status status, const json::value& body, unsigned version, bool keep_alive) const {
    http::response<http::string_body> res(status, version);
    res.set(http::field::content_type, "application/json");
    res.set(http::field::cache_control, "no-cache");
    res.keep_alive(keep_alive);
    res.body() = json::serialize(body);
    res.prepare_payload();
    return res;
}

std::optional<std::string> RequestHandler::TryExtractToken(const http::request<http::string_body>& req) const {
    auto it = req.find(http::field::authorization);
    if (it == req.end()) return std::nullopt;
    std::string auth(it->value());
    if (auth.starts_with("Bearer ") && auth.length() == 39) return auth.substr(7);
    return std::nullopt;
}

json::array RequestHandler::SerializeMaps() const {
    json::array maps_arr;
    for (const auto& map : app_.GetGame().GetMaps()) {
        maps_arr.push_back({{"id", map->GetId()}, {"name", map->GetName()}});
    }
    return maps_arr;
}

json::object RequestHandler::SerializeMap(const model::Map& map) const {
    json::array roads_arr, bldgs_arr, off_arr;
    for (const auto& r : map.GetRoads()) {
        if (r.IsHorizontal()) roads_arr.push_back({{"x0", r.GetStart().x}, {"y0", r.GetStart().y}, {"x1", r.GetEnd().x}});
        else roads_arr.push_back({{"x0", r.GetStart().x}, {"y0", r.GetStart().y}, {"y1", r.GetEnd().y}});
    }
    for (const auto& b : map.GetBuildings()) {
        bldgs_arr.push_back({{"x", b.GetBounds().position.x}, {"y", b.GetBounds().position.y}, {"w", b.GetBounds().size.width}, {"h", b.GetBounds().size.height}});
    }
    for (const auto& o : map.GetOffices()) {
        off_arr.push_back({{"id", o.GetId()}, {"x", o.GetPosition().x}, {"y", o.GetPosition().y}, {"offsetX", o.GetOffset().dx}, {"offsetY", o.GetOffset().dy}});
    }
    return {{"id", map.GetId()}, {"name", map.GetName()}, {"roads", roads_arr}, {"buildings", bldgs_arr}, {"offices", off_arr}};
}

RequestHandler::FileRequestResult RequestHandler::HandleStaticRequest(const http::request<http::string_body>& req) const {
    auto version = req.version();
    auto keep_alive = req.keep_alive();
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", version, keep_alive);
        res.set(http::field::allow, "GET, HEAD");
        return res;
    }
    std::string target = DecodeUrl(std::string(req.target()));
    if (target.empty() || target == "/") target = "/index.html";
    fs::path file_path = www_root_ / target.substr(1);
    if (!IsSubPath(file_path, www_root_)) return MakeErrorResponse(http::status::bad_request, "badRequest", "Bad request", version, keep_alive);

    http::file_body::value_type file;
    boost::system::error_code ec;
    file.open(file_path.string().c_str(), beast::file_mode::read, ec);
    if (ec) return MakeErrorResponse(http::status::not_found, "notFound", "File not found", version, keep_alive);

    http::response<http::file_body> res(http::status::ok, version);
    res.set(http::field::content_type, GetMimeType(file_path.extension().string()));
    res.keep_alive(keep_alive);
    res.body() = std::move(file);
    res.prepare_payload();
    return res;
}

http::response<http::string_body> RequestHandler::HandleApiRequest(const http::request<http::string_body>& req) const {
    auto version = req.version();
    auto keep_alive = req.keep_alive();
    std::string target(req.target());

    if (target == "/api/v1/maps" || target == "/api/v1/maps/") {
        return MakeJsonResponse(http::status::ok, SerializeMaps(), version, keep_alive);
    }
    if (target.starts_with("/api/v1/maps/")) {
        std::string map_id = target.substr(13);
        const auto* map = app_.GetGame().FindMap(map_id);
        if (!map) return MakeErrorResponse(http::status::not_found, "mapNotFound", "Map not found", version, keep_alive);
        return MakeJsonResponse(http::status::ok, SerializeMap(*map), version, keep_alive);
    }

    if (target == "/api/v1/game/join") {
        if (req.method() != http::verb::post) {
            auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Only POST method is expected", version, keep_alive);
            res.set(http::field::allow, "POST");
            return res;
        }
        try {
            auto body = json::parse(req.body()).as_object();
            if (!body.contains("userName") || !body.contains("mapId")) throw std::runtime_error("Invalid json");
            std::string user_name = body.at("userName").as_string().c_str();
            std::string map_id = body.at("mapId").as_string().c_str();
            if (user_name.empty()) return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid name", version, keep_alive);
            auto [token, player_id] = app_.JoinGame(map_id, user_name);
            return MakeJsonResponse(http::status::ok, {{"authToken", token}, {"playerId", player_id}}, version, keep_alive);
        } catch (...) {
            return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Join game request parse error", version, keep_alive);
        }
    }

    auto token = TryExtractToken(req);
    if (!token) {
        if (target.starts_with("/api/v1/game/")) return MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is required", version, keep_alive);
        return MakeErrorResponse(http::status::bad_request, "badRequest", "Bad request", version, keep_alive);
    }
    
    auto player = app_.GetPlayerByToken(*token);
    if (!player) return MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", version, keep_alive);

    if (target == "/api/v1/game/players") {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", version, keep_alive);
            res.set(http::field::allow, "GET, HEAD");
            return res;
        }
        json::object players_obj;
        for (const auto& p : app_.GetPlayersInSession(*token)) {
            players_obj[std::to_string(p->GetId())] = {{"name", p->GetDog()->GetName()}};
        }
        return MakeJsonResponse(http::status::ok, players_obj, version, keep_alive);
    }

    if (target == "/api/v1/game/state") {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", version, keep_alive);
            res.set(http::field::allow, "GET, HEAD");
            return res;
        }
        json::object players_obj;
        for (const auto& p : app_.GetPlayersInSession(*token)) {
            auto dog = p->GetDog();
            json::array pos = {dog->GetPosition().x, dog->GetPosition().y};
            json::array speed = {dog->GetSpeed().ux, dog->GetSpeed().uy};
            players_obj[std::to_string(p->GetId())] = {{"pos", pos}, {"speed", speed}, {"dir", dog->GetDirection()}};
        }
        return MakeJsonResponse(http::status::ok, {{"players", players_obj}}, version, keep_alive);
    }

    if (target == "/api/v1/game/player/action") {
        if (req.method() != http::verb::post) {
            auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", version, keep_alive);
            res.set(http::field::allow, "POST");
            return res;
        }
        try {
            auto body = json::parse(req.body()).as_object();
            if (!body.contains("move")) throw std::runtime_error("no move");
            std::string move = body.at("move").as_string().c_str();
            double speed_val = 5.0; // Базовая скорость (заглушка для тестов)
            app::Speed s{0, 0};
            if (move == "L") { s.ux = -speed_val; player->GetDog()->SetDirection("L"); }
            else if (move == "R") { s.ux = speed_val; player->GetDog()->SetDirection("R"); }
            else if (move == "U") { s.uy = -speed_val; player->GetDog()->SetDirection("U"); }
            else if (move == "D") { s.uy = speed_val; player->GetDog()->SetDirection("D"); }
            else if (move == "") { /* остановка */ }
            else return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action", version, keep_alive);
            player->GetDog()->SetSpeed(s);
            return MakeJsonResponse(http::status::ok, json::object(), version, keep_alive);
        } catch (...) {
            return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action", version, keep_alive);
        }
    }

    if (target == "/api/v1/game/tick") {
        if (req.method() != http::verb::post) {
            auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", version, keep_alive);
            res.set(http::field::allow, "POST");
            return res;
        }
        try {
            auto body = json::parse(req.body()).as_object();
            if (!body.contains("timeDelta")) throw std::runtime_error("no timeDelta");
            int delta = body.at("timeDelta").as_int64();
            app_.Tick(std::chrono::milliseconds(delta));
            return MakeJsonResponse(http::status::ok, json::object(), version, keep_alive);
        } catch (...) {
            return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse tick request JSON", version, keep_alive);
        }
    }

    return MakeErrorResponse(http::status::bad_request, "badRequest", "Bad request", version, keep_alive);
}

} // namespace http_handler
