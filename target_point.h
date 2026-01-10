#pragma once

#include <iostream>
#include <vector>
#include <random>
#include <algorithm> // 必须包含，用于 std::find

#include "collision.h"
#include "model.h"

struct Point {
    int x;
    int y;

    // 重载 == 用于查找
    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

struct CollisionResult {
    bool isHit;
	Point hitPoint;
};

class PointSystem {
private:
    int x_min, x_max;
    int y_min, y_max;
    std::vector<Point> activePoints;
    std::mt19937 rng;
    MODEL* m_pModel = nullptr;

public:
    PointSystem(int xMin, int xMax, int yMin, int yMax)
        : x_min(xMin), x_max(xMax), y_min(yMin), y_max(yMax) {
        std::random_device rd;
        rng.seed(rd());
    }

    Point GetRandomCoordinate();

    // 初始化
    void GenerateInitialPoints(int count);

    // --- 新功能: 消灭指定坐标的点，并生成新点 ---
    bool EliminateSpecificPointByCoord(int targetX, int targetY);
    bool EliminateSpecificPoint(const Point& point);

	CollisionResult CheckCollision(const Ray& playerRay) const;

    std::vector<Point> GetActivePoints() const {
        return activePoints;
	}

	void SetModel(MODEL* pModel) { m_pModel = pModel; }
    void DrawPoints() const;
 
    // 获取当前的某个点用于测试
    Point GetAnyActivePoint() const {
        if (!activePoints.empty()) return activePoints[0];
        return { -1, -1 };
    }
};
