#include "request_handler.h"

namespace http_handler {

RequestHandler::RequestHandler(model::Game& game, fs::path static_path)
    : game_{game}, static_path_{fs::weakly_canonical(std::move(static_path))} {
}

std::string RequestHandler::UrlDecode(std::string_view url) const {
    std::string result;
    result.reserve(url.size());
    for (size_t i = 0; i < url.size(); ++i) {
        if (url[i] == '%' && i + 2 < url.size()) {
            int hex_val;
            std::sscanf(url.data() + i + 1, "%2x", &hex_val);
            result += static_cast<char>(hex_val);
            i += 2;
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
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

std::string RequestHandler::GetMimeType(std::string_view extension) const {
    if (extension == ".htm" || extension == ".html") return "text/html";
    if (extension == ".css") return "text/css";
    if (extension == ".txt") return "text/plain";
    if (extension == ".js") return "text/javascript";
    if (extension == ".json") return "application/json";
    if (extension == ".xml") return "application/xml";
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpe" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".gif") return "image/gif";
    if (extension == ".bmp") return "image/bmp";
    if (extension == ".ico") return "image/vnd.microsoft.icon";
    if (extension == ".tiff" || extension == ".tif") return "image/tiff";
    if (extension == ".svg" || extension == ".svgz") return "image/svg+xml";
    if (extension == ".mp3") return "audio/mpeg";
    return "application/octet-stream";
}

json::array RequestHandler::SerializeMaps() const {
    json::array maps_arr;
    for (const auto& map : game_.GetMaps()) {
        maps_arr.push_back({
            {"id", map->GetId()},
            {"name", map->GetName()}
        });
    }
    return maps_arr;
}

json::object RequestHandler::SerializeMap(const model::Map& map) const {
    json::array roads_arr;
    for (const auto& r : map.GetRoads()) {
        if (r.IsHorizontal()) {
            roads_arr.push_back({
                {"x0", r.GetStart().x},
                {"y0", r.GetStart().y},
                {"x1", r.GetEnd().x}
            });
        } else {
            roads_arr.push_back({
                {"x0", r.GetStart().x},
                {"y0", r.GetStart().y},
                {"y1", r.GetEnd().y}
            });
        }
    }
    
    json::array bldgs_arr;
    for (const auto& b : map.GetBuildings()) {
        bldgs_arr.push_back({
            {"x", b.GetBounds().position.x},
            {"y", b.GetBounds().position.y},
            {"w", b.GetBounds().size.width},
            {"h", b.GetBounds().size.height}
        });
    }
    
    json::array off_arr;
    for (const auto& o : map.GetOffices()) {
        off_arr.push_back({
            {"id", o.GetId()},
            {"x", o.GetPosition().x},
            {"y", o.GetPosition().y},
            {"offsetX", o.GetOffset().dx},
            {"offsetY", o.GetOffset().dy}
        });
    }

    return {
        {"id", map.GetId()},
        {"name", map.GetName()},
        {"roads", roads_arr},
        {"buildings", bldgs_arr},
        {"offices", off_arr}
    };
}

} // namespace http_handler