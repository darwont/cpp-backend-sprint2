#pragma once
#include <string>
#include <vector>
#include <memory>

namespace model {

struct Point {
    int x;
    int y;
};

struct Size {
    int width;
    int height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    int dx;
    int dy;
};

class Road {
public:
    enum Direction { HORIZONTAL, VERTICAL };

    Road(Direction dir, Point start, int end);

    bool IsHorizontal() const;
    Point GetStart() const;
    Point GetEnd() const;

private:
    Direction dir_;
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds);
    const Rectangle& GetBounds() const;

private:
    Rectangle bounds_;
};

class Office {
public:
    Office(std::string id, Point position, Offset offset);

    const std::string& GetId() const;
    Point GetPosition() const;
    Offset GetOffset() const;

private:
    std::string id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    Map(std::string id, std::string name);

    const std::string& GetId() const;
    const std::string& GetName() const;

    void AddRoad(const Road& road);
    void AddBuilding(const Building& b);
    void AddOffice(const Office& o);

    const std::vector<Road>& GetRoads() const;
    const std::vector<Building>& GetBuildings() const;
    const std::vector<Office>& GetOffices() const;

private:
    std::string id_;
    std::string name_;
    std::vector<Road> roads_;
    std::vector<Building> buildings_;
    std::vector<Office> offices_;
};

class Game {
public:
    void AddMap(Map map);
    const std::vector<std::shared_ptr<Map>>& GetMaps() const;
    const Map* FindMap(const std::string& id) const;

private:
    std::vector<std::shared_ptr<Map>> maps_;
};

} // namespace model
