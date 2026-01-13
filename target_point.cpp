#include "target_point.h"

Point PointSystem::GetRandomCoordinate()
{
    std::uniform_int_distribution<int> dist_x(x_min, x_max);
    std::uniform_int_distribution<int> dist_y(y_min, y_max);
    return { dist_x(rng), dist_y(rng) };
}

void PointSystem::GenerateInitialPoints(int count)
{
    activePoints.clear();
    while (activePoints.size() < count) {
        Point p = GetRandomCoordinate();
        bool exists = false;
        for (const auto& existing : activePoints) {
            if (existing == p) { exists = true; break; }
        }
        if (!exists) activePoints.push_back(p);
    }

}

bool PointSystem::EliminateSpecificPointByCoord(int targetX, int targetY)
{
    Point target = { targetX, targetY };

    // 1. 查找目标点是否存在
    auto it = std::find(activePoints.begin(), activePoints.end(), target);

    if (it == activePoints.end()) {
        return false;
    }

	std::swap(*it, activePoints.back());

    while (true) {
        Point candidate = GetRandomCoordinate();

        auto collisionIt = std::find(activePoints.begin(), activePoints.end(), candidate);
        if (collisionIt == activePoints.end()) {
            activePoints.pop_back();
            activePoints.push_back(candidate);
            std::cout << "-> 新敌人刷新在: (" << candidate.x << "," << candidate.y << ")" << std::endl;
            break;
        }
    }

	m_HitCount++;
    return true;
}

bool PointSystem::EliminateSpecificPoint(const Point& point)
{
	return EliminateSpecificPointByCoord(point.x, point.y);
}

CollisionResult PointSystem::CheckCollision(const Ray& playerRay) const
{
    Sphere unitSphere;
	unitSphere.radius = 0.5f;
    for (const auto& point : activePoints) {
        unitSphere.center = { static_cast<float>(point.x), static_cast<float>(point.y) ,0.0f};
        if (Collision_isHitRayOnSphere(playerRay, unitSphere)) {
			return { true, point };
        }
    }
	return { false, {-1, -1} };
}

void PointSystem::DrawPoints() const
{
    if (!m_pModel) return;
    for (const auto& point : activePoints) {
	    DirectX::XMMATRIX world = DirectX::XMMatrixTranslation(static_cast<float>(point.x), static_cast<float>(point.y), 0.0f);
        ModelDraw(m_pModel, world);
	}
}
