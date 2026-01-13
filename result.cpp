#include "result.h"

#include "ms_logger.h"
#include "sprite.h"
#include "texture.h"
#include "font.h"
#include "scene.h"

void ResultUI::Initialize()
{

	m_ButtonBGTexId = Texture_LoadFromFile(L"resource/texture/frame.png");
	m_ExitButton.height = 50;
	m_ExitButton.width = 200;
	m_ExitButton.x = 1500;
	m_ExitButton.y = 550;
	m_ExitButton.text = L"Exit";
	m_BackToTitle.height = 50;
	m_BackToTitle.width = 200;
	m_BackToTitle.x = 1500;
	m_BackToTitle.y = 400;
	m_BackToTitle.text = L"Title";

}

void ResultUI::Update(double elapsedTime)
{
	int mouse_x = MSLogger_GetXUI();
	int mouse_y = MSLogger_GetYUI();
	if (MSLogger_IsUIMode()) {
		// Back to Title Button
		if (mouse_x >= m_BackToTitle.x && mouse_x <= m_BackToTitle.x + m_BackToTitle.width &&
			mouse_y >= m_BackToTitle.y && mouse_y <= m_BackToTitle.y + m_BackToTitle.height) {
			m_BackToTitle.isActive = true;
			if (MSLogger_IsTriggerUI(MBT_LEFT)) {
				Restart_Game();
			}
		}
		else {
			m_BackToTitle.isActive = false;
		}
		// Exit Button
		if (mouse_x >= m_ExitButton.x && mouse_x <= m_ExitButton.x + m_ExitButton.width &&
			mouse_y >= m_ExitButton.y && mouse_y <= m_ExitButton.y + m_ExitButton.height) {
			m_ExitButton.isActive = true;
			if (MSLogger_IsTriggerUI(MBT_LEFT)) {
				// Exit Game
				// PostQuitMessage(0);
			}
		}
		else {
			m_ExitButton.isActive = false;
		}
	
	}
}

void ResultUI::UpdateData(int fireCount, int hitCount)
{
	m_Points = hitCount * 10;
	if (fireCount > 0)
	{
		m_Accuracy = static_cast<float>(hitCount) / static_cast<float>(fireCount);
	}
	else
	{
		m_Accuracy = 0.0f;
	}
}


void ResultUI::Draw()
{
	Sprite_Draw(m_ButtonBGTexId, (float)m_BackToTitle.x , (float)m_BackToTitle.y , (float)m_BackToTitle.width , (float)m_BackToTitle.height );
	Sprite_Draw(m_ButtonBGTexId, (float)m_ExitButton.x, (float)m_ExitButton.y, (float)m_ExitButton.width , (float)m_ExitButton.height);

	DirectX::XMFLOAT4 color = { 0.1f, 0.1f, 0.1f, 1.0f };
	DirectX::XMFLOAT4 activeColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	Font_Draw(
		m_BackToTitle.text.c_str(),
		m_BackToTitle.x + 20,
		m_BackToTitle.y + 10,
		m_BackToTitle.isActive ? activeColor : color
	);
	Font_Draw(
		m_ExitButton.text.c_str(),
		m_ExitButton.x + 60,
		m_ExitButton.y + 10,
		m_ExitButton.isActive ? activeColor : color
	);

	std::wstring pointText = L"FINAL SCORE: \n" + std::to_wstring(m_Points);
	std::wstring accuText = L"Accuracy: \n" + std::to_wstring(static_cast<int>(m_Accuracy * 100.0f)) + L"%";

	Font_Draw(pointText.c_str(), 300.0f, 300.0f, { 1.0f,1.0f,1.0f,1.0f });
	Font_Draw(accuText.c_str(), 300.0f, 500.0f, { 1.0f,1.0f,1.0f,1.0f });

}
