#include "title_UI.h"

#include "font.h"
#include "ms_logger.h"
#include "sprite.h"
#include "texture.h"


void TitleUI::Initialize()
{
	m_ButtonBGTexId = Texture_LoadFromFile(L"resource/texture/frame.png");

	m_Buttons[0].text = L"START";
	m_Buttons[0].x = 350;
	m_Buttons[0].y = 200;
	m_Buttons[0].width = 200;
	m_Buttons[0].height = 50;
	m_Buttons[0].isActive = false;

	m_Buttons[1].text = L"EXIT";
	m_Buttons[1].x = 350;
	m_Buttons[1].y = 300;
	m_Buttons[1].width = 200;
	m_Buttons[1].height = 50;
	m_Buttons[1].isActive = false;

	m_Buttons[2].text = L"SETTING";
	m_Buttons[2].x = 350;
	m_Buttons[2].y = 400;
	m_Buttons[2].width = 200;
	m_Buttons[2].height = 50;
	m_Buttons[2].isActive = false;
}

void TitleUI::Update(double elapsedTime)
{
	int mouse_x = MSLogger_GetXUI();
	int mouse_y = MSLogger_GetYUI();

	if (MSLogger_IsUIMode()) {
		for (Button& btn : m_Buttons) {
			// 检测鼠标是否在按钮范围内
			if (mouse_x >= btn.x && mouse_x <= btn.x + btn.width &&
				mouse_y >= btn.y && mouse_y <= btn.y + btn.height) {
					btn.isActive = true;
				}
		}

		for (Button& btn : m_Buttons) {
			if (btn.isActive) {
				if (MSLogger_IsPressed(MBT_LEFT)) {
					// 按钮被点击，执行相应操作
					if (btn.text == L"START") {
						// 开始游戏的逻辑
					}
					else if (btn.text == L"EXIT") {
						// 退出游戏的逻辑
					}
					else if (btn.text == L"SETTING") {
						// 设置的逻辑
					}
				}
			}
		}
	}
}

void TitleUI::Draw()
{


	Sprite_Draw(m_ButtonBGTexId, 1200, 300, 200, 100, { 1.0f,1.0f,1.0f,1.0f });
	DirectX::XMFLOAT4 color = { 0.7f, 0.7f, 0.8f, 1.0f };
	if (m_Buttons[0].isActive) {
		
		color = { 1.0f, 1.0f, 1.0f, 1.0f };
	}

	Font_Draw(m_Buttons[0].text.c_str(), 1200, 300,color);
}
