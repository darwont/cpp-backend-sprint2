#include "model.h"
#include <stdexcept>

namespace model {

Road::Road(Direction dir, Point start, int end)
    : dir_(dir)
    , start_(start)
    , end_{dir == HORIZONTAL ? end : start.x,
           dir == VERTICAL ? end : start.y} {
}

bool Road::IsHorizontal() const {
    return dir_ == HORIZONTAL;
}

Point Road::GetStart() const {
    return start_;
}

Point Road::GetEnd() const {
    return end_;
}

Building::Building(Rectangle bounds)
    : bounds_(bounds) {
}

const Rectangle& Building::GetBounds() const {
    return bounds_;
}

Office::Office(std::string id, Point position, Offset offset)
    : id_(std::move(id))
    , position_(position)
    , offset_(offset) {
}

const std::string& Office::GetId() const {
    return id_;
}

Point Office::GetPosition() const {
    return position_;
}

Offset Office::GetOffset() const {
    return offset_;
}

Map::Map(std::string id, std::string name)
    : id_(std::move(id))
    , name_(std::move(name)) {
}

const std::string& Map::GetId() const {
    return id_;
}

const std::string& Map::GetName() const {
    return name_;
}

void Map::AddRoad(const Road& road) {
    roads_.push_back(road);
}

void Map::AddBuilding(const Building& b) {
    buildings_.push_back(b);
}

void Map::AddOffice(const Office& o) {
    offices_.push_back(o);
}

const std::vector<Road>& Map::GetRoads() const {
    return roads_;
}

const std::vector<Building>& Map::GetBuildings() const {
    return buildings_;
}

const std::vector<Office>& Map::GetOffices() const {
    return offices_;
}

void Game::AddMap(Map map) {
    maps_.emplace_back(std::make_shared<Map>(std::move(map)));
}

const std::vector<std::shared_ptr<Map>>& Game::GetMaps() const {
    return maps_;
}

const Map* Game::FindMap(const std::string& id) const {
    for (const auto& map : maps_) {
        if (map->GetId() == id) {
            return map.get();
        }
    }
    return nullptr;
}

} // namespace model
