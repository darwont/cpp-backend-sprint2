#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <random>
#include <sstream>
#include <iomanip>

namespace app {

struct Position {
    double x = 0.0;
    double y = 0.0;
};

struct Speed {
    double ux = 0.0;
    double uy = 0.0;
};

class Dog {
public:
    Dog(uint64_t id, std::string name)
        : id_(id), name_(std::move(name)), dir_("U") {}

    uint64_t GetId() const { return id_; }
    const std::string& GetName() const { return name_; }
    
    const Position& GetPosition() const { return pos_; }
    const Speed& GetSpeed() const { return speed_; }
    const std::string& GetDirection() const { return dir_; }

    void SetPosition(Position pos) { pos_ = pos; }
    void SetSpeed(Speed speed) { speed_ = speed; }
    void SetDirection(std::string dir) { dir_ = std::move(dir); }

private:
    uint64_t id_;
    std::string name_;
    Position pos_;
    Speed speed_;
    std::string dir_;
};

class Player {
public:
    Player(std::shared_ptr<Dog> dog) : dog_(std::move(dog)) {}
    uint64_t GetId() const { return dog_->GetId(); }
    const std::shared_ptr<Dog>& GetDog() const { return dog_; }
private:
    std::shared_ptr<Dog> dog_;
};

class PlayerTokens {
public:
    std::string GenerateToken() {
        std::stringstream ss;
        ss << std::setw(16) << std::setfill('0') << std::hex << generator1_();
        ss << std::setw(16) << std::setfill('0') << std::hex << generator2_();
        return ss.str();
    }

    void AddToken(const std::string& token, std::shared_ptr<Player> player) {
        tokens_[token] = std::move(player);
    }

    std::shared_ptr<Player> FindPlayer(const std::string& token) const {
        if (auto it = tokens_.find(token); it != tokens_.end()) {
            return it->second;
        }
        return nullptr;
    }

private:
    std::random_device random_device_;
    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    std::unordered_map<std::string, std::shared_ptr<Player>> tokens_;
};

} // namespace app
