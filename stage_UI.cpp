#include "stage_UI.h"

#include <string>

#include "direct3d.h"
#include "font.h"
#include "game.h"
#include "sprite.h"
#include "texture.h"

void StageUI::Initialize()
{
	m_Accuracy = 0.0f;
	m_HitCounter = 0;
	m_isTimerActive = false;
	m_isCountdownActive = false;
	m_Points = 0;
	m_Timer = 30.0f;
	m_CountdownTimer = 3.0f;
	m_frameTexId = Texture_LoadFromFile(L"resource/texture/frame.png");
}

void StageUI::Update(double elapsed_time)
{
	if (m_isTimerActive)
	{
		m_Timer -= elapsed_time;
		if (m_Timer < 0.0)
		{
			m_Timer = 0.0;
			m_isTimerActive = false;
			Game_SetState(RESULT);
		}
	}

	if(m_isCountdownActive)
	{
		m_CountdownTimer -= elapsed_time;
		if (m_CountdownTimer < 0.0)
		{
			m_CountdownTimer = 0.0;
			m_isCountdownActive = false;
			Game_SetState(PLAY);
		}
	}
}

void StageUI::Draw()
{


	std::wstring timerText = L"Time: \n" + std::to_wstring(static_cast<int>(m_Timer));
	std::wstring pointsText = L"Points: \n" + std::to_wstring(m_Points);
	std::wstring accuracyText = L"Accuracy: \n" + std::to_wstring(static_cast<int>(m_Accuracy * 100.0f)) + L"%";
	std::wstring countdownText = std::to_wstring(static_cast<int>(m_CountdownTimer + 1));

	Sprite_Draw(m_frameTexId, 300, 30, 350.0f, 150.0f, { 1.0f,1.0f,1.0f,1.0f });
	Sprite_Draw(m_frameTexId, 660, 30, 350.0f, 150.0f, { 1.0f,1.0f,1.0f,1.0f });
	Sprite_Draw(m_frameTexId, 1120, 30, 350.0f, 150.0f, { 1.0f,1.0f,1.0f,1.0f });

	Font_Draw(timerText.c_str(), 320, 50, { 1.0f,1.0f,1.0f,1.0f });
	Font_Draw(pointsText.c_str(), 680, 50, { 1.0f,1.0f,1.0f,1.0f });
	Font_Draw(accuracyText.c_str(), 1140, 50, { 1.0f,1.0f,1.0f,1.0f });

	if (m_isCountdownActive) {
		Font_Draw(countdownText.c_str(), 960, 400, { 1.0f, 0.0f, 0.0f, 1.0f },3.0);
	}

}

void StageUI::StartTimer()
{
	m_isTimerActive = true;
}

void StageUI::StopTimer()
{
	m_isTimerActive = false;
}

void StageUI::ResetTimer()
{
	m_Timer = 30.0f;
}

void StageUI::StartCountdownTimer()
{
	m_isCountdownActive = true;
}

void StageUI::StopCountdownTimer()
{
	m_isCountdownActive = false;
}

void StageUI::ResetCountdownTimer()
{
	m_CountdownTimer = 3.0f;
}

void StageUI::UpdateAccuracy(int fireCount, int hitCount)
{
	m_Points = hitCount * 10;
	m_HitCounter = hitCount;
	if (fireCount > 0)
	{
		m_Accuracy = static_cast<float>(hitCount) / static_cast<float>(fireCount);
	}
	else
	{
		m_Accuracy = 0.0f;
	}
}
