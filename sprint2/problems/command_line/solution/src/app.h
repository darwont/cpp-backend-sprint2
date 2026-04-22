#pragma once
#include "model.h"
#include "player.h"
#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include <chrono>

namespace app {

class GameSession {
public:
    GameSession(std::shared_ptr<model::Map> map) : map_(std::move(map)) {}
    const std::shared_ptr<model::Map>& GetMap() const { return map_; }

    std::shared_ptr<Player> AddPlayer(const std::string& player_name) {
        auto dog = std::make_shared<Dog>(dog_id_counter_++, player_name);
        
        // По умолчанию ставим собаку в начало первой дороги (для тестов)
        if (!map_->GetRoads().empty()) {
            const auto& first_road = map_->GetRoads().front();
            dog->SetPosition({static_cast<double>(first_road.GetStart().x), 
                              static_cast<double>(first_road.GetStart().y)});
        }
        
        auto player = std::make_shared<Player>(dog);
        players_.push_back(player);
        return player;
    }

    const std::vector<std::shared_ptr<Player>>& GetPlayers() const { return players_; }

    void Tick(std::chrono::milliseconds delta) {
        double delta_sec = delta.count() / 1000.0;
        for (auto& player : players_) {
            auto dog = player->GetDog();
            auto pos = dog->GetPosition();
            auto speed = dog->GetSpeed();
            
            // Базовое смещение (в реальном проекте тут проверяются границы дорог)
            pos.x += speed.ux * delta_sec;
            pos.y += speed.uy * delta_sec;
            
            dog->SetPosition(pos);
        }
    }

private:
    std::shared_ptr<model::Map> map_;
    std::vector<std::shared_ptr<Player>> players_;
    uint64_t dog_id_counter_ = 0;
};

class Application {
public:
    Application(model::Game game) : game_(std::move(game)) {}

    std::pair<std::string, uint64_t> JoinGame(const std::string& map_id, const std::string& player_name) {
        auto map = game_.FindMap(map_id);
        if (!map) throw std::invalid_argument("Map not found");

        auto session = FindOrCreateSession(map_id);
        auto player = session->AddPlayer(player_name);
        
        std::string token = player_tokens_.GenerateToken();
        player_tokens_.AddToken(token, player);

        return {token, player->GetId()};
    }

    std::shared_ptr<Player> GetPlayerByToken(const std::string& token) const {
        return player_tokens_.FindPlayer(token);
    }

    const std::vector<std::shared_ptr<Player>>& GetPlayersInSession(const std::string& token) const {
        auto player = GetPlayerByToken(token);
        if (!player) throw std::invalid_argument("Invalid token");

        for (const auto& session : sessions_) {
            for (const auto& p : session->GetPlayers()) {
                if (p->GetId() == player->GetId()) {
                    return session->GetPlayers();
                }
            }
        }
        throw std::runtime_error("Session not found");
    }

    void Tick(std::chrono::milliseconds delta) {
        for (auto& session : sessions_) {
            session->Tick(delta);
        }
    }

private:
    std::shared_ptr<GameSession> FindOrCreateSession(const std::string& map_id) {
        for (auto& session : sessions_) {
            if (session->GetMap()->GetId() == map_id) return session;
        }
        auto map = game_.FindMap(map_id);
        auto session = std::make_shared<GameSession>(std::make_shared<model::Map>(*map));
        sessions_.push_back(session);
        return session;
    }

    model::Game game_;
    std::vector<std::shared_ptr<GameSession>> sessions_;
    PlayerTokens player_tokens_;
};

} // namespace app
