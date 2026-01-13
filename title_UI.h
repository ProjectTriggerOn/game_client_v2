#pragma once
#include <string>


struct Button {
	std::wstring text;
	int x;
	int y;
	int width;
	int height;
	bool isActive;
};

class TitleUI
{
	public:
	TitleUI() = default;
	~TitleUI() = default;
	void Initialize();
	void Update(double elapsedTime);
	void Draw();

private:
	Button m_Buttons[3];
	int m_ButtonBGTexId = -1;
};

