#include "player_cam_tps.h"

#include "direct3d.h"
#include "key_logger.h"
#include "mouse.h"
#include "ms_logger.h"
#include "shader_3d.h"
#include "shader_field.h"
#include "shader_infinite.h"

#include <algorithm>

namespace
{
    using namespace DirectX;

    XMFLOAT3 g_EyePosition{ 0.0f, 0.0f, 0.0f };
    XMFLOAT3 g_CameraFront{ 0.0f, 0.0f, 1.0f };

    XMFLOAT4X4 g_ViewMatrix{};
    XMFLOAT4X4 g_PerspectiveMatrix{};

    float g_CameraYaw      = 0.0f;
    float g_CameraPitch    = 0.2f;
    float g_CameraDistance = 5.0f;
}

void PlayerCamTps_Initialize()
{
}

void PlayerCamTps_Finalize()
{
}

// Maya-style orbit camera (debug only). Orbits around the supplied target.
void PlayerCamTps_Update_Maya(double /*elapsed_time*/, const DirectX::XMFLOAT3& target)
{
    using namespace DirectX;

    const bool altDown = KeyLogger_IsPressed(KK_LEFTALT);
    static int  lastX = 0, lastY = 0;
    static bool dragging   = false;
    static int  dragButton = 0; // 1:Left, 2:Middle, 3:Right

    if (altDown && (isButtonDown(MBT_LEFT) || isButtonDown(MBT_MIDDLE) || isButtonDown(MBT_RIGHT)))
    {
        if (!dragging)
        {
            dragging = true;
            lastX = MSLogger_GetX();
            lastY = MSLogger_GetY();
            if      (isButtonDown(MBT_LEFT))   dragButton = 1;
            else if (isButtonDown(MBT_MIDDLE)) dragButton = 2;
            else if (isButtonDown(MBT_RIGHT))  dragButton = 3;
        }
        int dx = MSLogger_GetX() - lastX;
        int dy = MSLogger_GetY() - lastY;
        lastX = MSLogger_GetX();
        lastY = MSLogger_GetY();

        if (dragButton == 1)
        {
            // Alt + Left : orbit
            g_CameraYaw   -= dx * 0.01f;
            g_CameraPitch -= dy * 0.01f;
            constexpr float PITCH_MAX = XM_PIDIV2 - 0.05f;
            constexpr float PITCH_MIN = -XM_PIDIV4;
            g_CameraPitch = std::min(g_CameraPitch, PITCH_MAX);
            g_CameraPitch = std::max(g_CameraPitch, PITCH_MIN);
        }
        else if (dragButton == 3)
        {
            // Alt + Right : dolly (push/pull)
            g_CameraDistance += dy * 0.05f;
            g_CameraDistance = std::max(2.0f,  g_CameraDistance);
            g_CameraDistance = std::min(20.0f, g_CameraDistance);
        }
    }
    else
    {
        dragging   = false;
        dragButton = 0;
    }

    // Scroll wheel zoom
    if (MSLogger_GetScrollWheelValue() != 0)
    {
        g_CameraDistance -= MSLogger_GetScrollWheelValue() * 0.001f;
        g_CameraDistance = std::max(2.0f,  g_CameraDistance);
        g_CameraDistance = std::min(20.0f, g_CameraDistance);
        Mouse_ResetScrollWheelValue();
    }

    // Spherical -> Cartesian camera position
    const float cx = cosf(g_CameraPitch) * cosf(g_CameraYaw);
    const float cz = cosf(g_CameraPitch) * sinf(g_CameraYaw);
    const float cy = sinf(g_CameraPitch);

    const XMFLOAT3 camPos = {
        target.x + cx * g_CameraDistance,
        target.y + 2.0f + cy * g_CameraDistance,
        target.z + cz * g_CameraDistance,
    };

    const XMVECTOR position = XMLoadFloat3(&camPos);
    const XMVECTOR focus    = XMLoadFloat3(&target);
    const XMVECTOR up       = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    const XMMATRIX mtxView = XMMatrixLookAtLH(position, focus, up);
    Shader_InfiniteGrid_SetViewMatrix(mtxView);
    XMStoreFloat4x4(&g_ViewMatrix, mtxView);

    const float aspectRatio = static_cast<float>(Direct3D_GetBackBufferWidth())
                            / static_cast<float>(Direct3D_GetBackBufferHeight());
    const float fov   = 1.0f;
    const float nearZ = 0.1f;
    const float farZ  = 1000.0f;
    const XMMATRIX mtxPerspective = XMMatrixPerspectiveFovLH(fov, aspectRatio, nearZ, farZ);

    Shader_InfiniteGrid_SetProjectMatrix(mtxPerspective);
    XMStoreFloat4x4(&g_PerspectiveMatrix, mtxPerspective);

    g_EyePosition = camPos;
    const XMVECTOR camFront = XMVector3Normalize(focus - position);
    XMStoreFloat3(&g_CameraFront, camFront);
}

const DirectX::XMFLOAT3& PlayerCamTps_GetPosition()
{
    return g_EyePosition;
}

const DirectX::XMFLOAT4X4& PlayerCamTps_GetPerspectiveMatrix()
{
    return g_PerspectiveMatrix;
}

const DirectX::XMFLOAT4X4& PlayerCamTps_GetViewMatrix()
{
    return g_ViewMatrix;
}
