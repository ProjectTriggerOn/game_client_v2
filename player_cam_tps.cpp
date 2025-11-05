#include "player_cam_tps.h"

#include "direct3d.h"
#include "player.h"
#include "shader_3d.h"
#include "shader_field.h"
#include "mouse.h"
using namespace DirectX;
namespace 
{
	//XMMATRIX mtxView;

	XMFLOAT3 eyePosition{0.0f,0.0f,0.0f};
	XMFLOAT3 eyeDirection{};
	XMFLOAT3 upDirection{};

	constexpr float MOVE_SPEED = 1.0f;
	constexpr float CAMERA_ROT_SPEED = XMConvertToRadians(30.0f);

	XMFLOAT3 g_CameraFront{0.0f,0.0f,1.0f};
	XMFLOAT3 g_CameraRight{};
	XMFLOAT3 g_CameraUp{};

	XMFLOAT4X4 g_ViewMatrix{};
	XMFLOAT4X4 g_PerspectiveMatrix{};

	float g_fov;

	float g_cameraYaw = 0.0f;     // 环绕水平角度（弧度）
	float g_cameraPitch = 0.2f;   // 上下仰角（弧度）
	float g_cameraDistance = 5.0f; // 距离，默认5米
	bool g_lastRightButton = false;    // 上一帧右键
	int  g_lastMouseX = 0, g_lastMouseY = 0; // 上一帧鼠标位置
}
void PlayerCamTps_Initialize()
{

}

void PlayerCamTps_Initialize(
	const DirectX::XMFLOAT3& position, 
	const DirectX::XMFLOAT3& front, 
	const DirectX::XMFLOAT3& right, 
	const DirectX::XMFLOAT3& up)
{
}

void PlayerCamTps_Finalize(void)
{
}

void PlayerCamTps_Update(double elapsed_time)
{
	XMVECTOR position = XMLoadFloat3(&Player_GetPosition());

	position = XMVectorMultiply(position, { 1.0f,0.0f,1.0f });

	XMVECTOR target = position;

	position = XMVectorAdd(position, { 0.0f,3.0f,-5.0f });


	XMVECTOR front = XMVector3Normalize(target - position);

	XMStoreFloat3(&g_CameraFront, front);
	XMStoreFloat3(&eyePosition, position);

	XMMATRIX mtxView = XMMatrixLookAtLH(
		position, // 視点座標
		target, // 注視点座標
		{0.0f,1.0f,0.0f} // 上方向ベクトル
	);

	Shader_3D_SetViewMatrix(mtxView);
	Shader_Field_SetViewMatrix(mtxView);

	float aspectRatio = static_cast<float>(Direct3D_GetBackBufferWidth()) / static_cast<float>(Direct3D_GetBackBufferHeight());
	float nearZ = 0.1f;
	float farZ = 1000.0f;

	XMMATRIX mtxPerspective = XMMatrixPerspectiveFovLH(
		1.0f,
		aspectRatio,
		nearZ,
		farZ
	);

	Shader_3D_SetProjectMatrix(mtxPerspective);
	Shader_Field_SetProjectMatrix(mtxPerspective);
}



void PlayerCamTps_Update_Mouse(double elapsed_time)
{
    // 读取鼠标
    Mouse_State ms;
    Mouse_GetState(&ms);

    // 仅按下右键时旋转摄像机
    if (ms.rightButton) {
        // 只在"按住"时更新
        static bool dragging = false;
        if (!g_lastRightButton) { // 新按下，初始化
            dragging = true;
            g_lastMouseX = ms.x;
            g_lastMouseY = ms.y;
        }
        if (dragging) {
            int dx = ms.x - g_lastMouseX;
            int dy = ms.y - g_lastMouseY;
            g_cameraYaw -= dx * 0.01f; // 鼠标X控制水平角
            g_cameraPitch -= dy * 0.01f; // 鼠标Y控制俯仰角

            // Pitch限制，避免翻转
            constexpr float PITCH_MAX = DirectX::XM_PIDIV2 - 0.05f;
            constexpr float PITCH_MIN = -DirectX::XM_PIDIV4; // 视角朝上不要超过45°
            //if (g_cameraPitch > PITCH_MAX) g_cameraPitch = PITCH_MAX;
			g_cameraPitch = std::min(g_cameraPitch, PITCH_MAX);
            //if (g_cameraPitch < PITCH_MIN) g_cameraPitch = PITCH_MIN;
			g_cameraPitch = std::max(g_cameraPitch, PITCH_MIN);

            g_lastMouseX = ms.x;
            g_lastMouseY = ms.y;
        }
    }
    else {
        g_lastRightButton = false;
    }
    g_lastRightButton = ms.rightButton;

    // 滚轮控制距离
    if (ms.scrollWheelValue != 0) {
        g_cameraDistance -= ms.scrollWheelValue * 0.001f;  // 一格缩放0.12左右
        if (g_cameraDistance < 2.0f) g_cameraDistance = 2.0f;
        if (g_cameraDistance > 20.0f) g_cameraDistance = 20.0f;

        Mouse_ResetScrollWheelValue(); // 把值清掉
    }

    // 球坐标计算摄像机
    DirectX::XMFLOAT3 target = Player_GetPosition();
    float x = cosf(g_cameraPitch) * cosf(g_cameraYaw);
    float z = cosf(g_cameraPitch) * sinf(g_cameraYaw);
    float y = sinf(g_cameraPitch);

    XMFLOAT3 camPos = {
        target.x + x * g_cameraDistance,
        target.y + 2.0f + y * g_cameraDistance, // 2.0f为高度补偿，让镜头瞄准玩家头顶
        target.z + z * g_cameraDistance
    };

    DirectX::XMVECTOR position = XMLoadFloat3(&camPos);
    DirectX::XMVECTOR focus = XMLoadFloat3(&target);

    XMVECTOR up = { 0, 1, 0 };

    // 生成视图矩阵
    XMMATRIX mtxView = XMMatrixLookAtLH(position, focus, up);
    Shader_3D_SetViewMatrix(mtxView);
    Shader_Field_SetViewMatrix(mtxView);

    // 投影矩阵
    float aspectRatio = static_cast<float>(Direct3D_GetBackBufferWidth()) / static_cast<float>(Direct3D_GetBackBufferHeight());
    float fov = 1.0f;
    float nearZ = 0.1f;
    float farZ = 1000.0f;
    XMMATRIX mtxPerspective = XMMatrixPerspectiveFovLH(fov, aspectRatio, nearZ, farZ);
    Shader_3D_SetProjectMatrix(mtxPerspective);
    Shader_Field_SetProjectMatrix(mtxPerspective);

    // 存储当前摄像机位置、前方向
    XMStoreFloat3(&eyePosition, {camPos.x,camPos.y,camPos.z});
    XMVECTOR camFront = XMVector3Normalize(focus - position);
    XMStoreFloat3(&g_CameraFront, camFront);
}



const DirectX::XMFLOAT3& PlayerCamTps_GetFront()
{
	return g_CameraFront;
}

const DirectX::XMFLOAT3& PlayerCamTps_GetPosition()
{
	return eyePosition;
}


