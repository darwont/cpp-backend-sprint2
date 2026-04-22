#include "json_loader.h"
#include <fstream>
#include <sstream>
#include <boost/json.hpp>
#include <stdexcept>

namespace json_loader {
namespace json = boost::json;

model::Game LoadGame(const std::filesystem::path& json_path) {
    model::Game game;
    std::ifstream file(json_path);
    if (!file) throw std::runtime_error("Cannot open config file");
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    auto value = json::parse(buffer.str());
    
    for (const auto& map_val : value.as_object().at("maps").as_array()) {
        const auto& map_obj = map_val.as_object();
        model::Map map(map_obj.at("id").as_string().c_str(), map_obj.at("name").as_string().c_str());
        
        // В рамках базовой заглушки парсим дороги
        for (const auto& r_val : map_obj.at("roads").as_array()) {
            const auto& r = r_val.as_object();
            int x0 = r.at("x0").as_int64();
            int y0 = r.at("y0").as_int64();
            if (r.contains("x1")) {
                map.AddRoad(model::Road(model::Road::HORIZONTAL, {x0, y0}, r.at("x1").as_int64()));
            } else {
                map.AddRoad(model::Road(model::Road::VERTICAL, {x0, y0}, r.at("y1").as_int64()));
            }
        }
        game.AddMap(std::move(map));
    }
    return game;
}
} // namespace json_loader
