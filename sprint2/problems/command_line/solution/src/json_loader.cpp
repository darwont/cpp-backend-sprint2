#include "json_loader.h"
#include <fstream>
#include <sstream>
#include <boost/json.hpp>

namespace json_loader {
namespace json = boost::json;

void LoadRoads(model::Map& map, const json::array& roads_arr) {
    for (const auto& r_val : roads_arr) {
        const auto& r = r_val.as_object();
        int x0 = r.at("x0").as_int64();
        int y0 = r.at("y0").as_int64();
        if (r.contains("x1")) {
            map.AddRoad(model::Road(model::Road::HORIZONTAL, {x0, y0}, r.at("x1").as_int64()));
        } else {
            map.AddRoad(model::Road(model::Road::VERTICAL, {x0, y0}, r.at("y1").as_int64()));
        }
    }
}

void LoadBuildings(model::Map& map, const json::array& bldgs_arr) {
    for (const auto& b_val : bldgs_arr) {
        const auto& b = b_val.as_object();
        model::Rectangle rect{
            {static_cast<int>(b.at("x").as_int64()), static_cast<int>(b.at("y").as_int64())},
            {static_cast<int>(b.at("w").as_int64()), static_cast<int>(b.at("h").as_int64())}
        };
        map.AddBuilding(model::Building(rect));
    }
}

void LoadOffices(model::Map& map, const json::array& off_arr) {
    for (const auto& o_val : off_arr) {
        const auto& o = o_val.as_object();
        model::Office office(
            o.at("id").as_string().c_str(),
            {static_cast<int>(o.at("x").as_int64()), static_cast<int>(o.at("y").as_int64())},
            {static_cast<int>(o.at("offsetX").as_int64()), static_cast<int>(o.at("offsetY").as_int64())}
        );
        map.AddOffice(office);
    }
}

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
        
        LoadRoads(map, map_obj.at("roads").as_array());
        if (map_obj.contains("buildings")) LoadBuildings(map, map_obj.at("buildings").as_array());
        if (map_obj.contains("offices")) LoadOffices(map, map_obj.at("offices").as_array());
        
        game.AddMap(std::move(map));
    }
    return game;
}
} // namespace json_loader
