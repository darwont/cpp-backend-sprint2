#include "model.h"
#include <stdexcept>

namespace model {

void Game::AddMap(Map map) {
    maps_.emplace_back(std::make_shared<Map>(std::move(map)));
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
