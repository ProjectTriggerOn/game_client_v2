#pragma once
#include "title_UI.h"

class ResultUI
{
	public:
	ResultUI() = default;
	~ResultUI() = default;
	void Initialize();
	void Update(double elapsedTime);
	void UpdateData(int fireCount, int hitCount);
	void Draw();

private:
	int m_Points = 0;
	int m_ButtonBGTexId = -1;
	float m_Accuracy = 0.0f;
	Button m_BackToTitle;
	Button m_ExitButton;
};

