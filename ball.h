#pragma once
#include "collision.h"
#include <DirectXMath.h>

#include "model.h"
#include "texture.h"

using namespace DirectX;

class Ball
{
private:
    XMFLOAT3 m_position{};
    float    m_Radius{ 0.5f };
    MODEL* m_pModel = nullptr;
    int      m_Hp{ 1 };

public:
    Ball(const XMFLOAT3& position, float radius)
        : m_position(position), m_Radius(radius)
    {
    }

    Sphere GetCollisionSphere() const;
    void Update(double elapsed_time);
    bool isDestroyed() const;
    void Draw() const;

    void Damage(int damage) { m_Hp -= damage; }

    void SetPosition(const XMFLOAT3& pos) { m_position = pos; }
    void ResetHp(int hp = 1) { m_Hp = hp; }


    void SetModel(MODEL* pModel) { m_pModel = pModel; }
};

void   Ball_Initialize();
void   Ball_Finalize();
Sphere Ball_GetSphere(int index);
Ball* Ball_GetBall(int index);
void   Ball_UpdateAll(double elapsed_time);
void   Ball_DrawAll();
