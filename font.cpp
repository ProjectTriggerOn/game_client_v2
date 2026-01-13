#include "font.h"
#include "texture.h"
#include "Sprite.h"
#include <windows.h>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

#include "direct3d.h"

// --- 1. 将结构体定义放在文件顶部 ---
// 用于存储单个字符的渲染信息
struct CharInfo
{
	int srcX, srcY;
	int srcW, srcH;
	int xoffset, yoffset;
	int xadvance;
};

// --- 2. 定义全局变量 ---
static int g_FontTextureID = -1;
static std::map<wchar_t, CharInfo> g_CharMap;

// --- 3. 将辅助函数放在它们被调用之前 ---

// 辅助函数：解析一行BMFont的 "char" 数据
static void ParseCharLine(const std::string& line)
{
	std::stringstream ss(line);
	std::string token;
	int charId = 0;
	CharInfo info = {};

	while (ss >> token)
	{
		size_t pos = token.find('=');
		if (pos == std::string::npos) continue;

		std::string key = token.substr(0, pos);
		int value = std::stoi(token.substr(pos + 1));

		if (key == "id") charId = value;
		else if (key == "x") info.srcX = value;
		else if (key == "y") info.srcY = value;
		else if (key == "width") info.srcW = value;
		else if (key == "height") info.srcH = value;
		else if (key == "xoffset") info.xoffset = value;
		else if (key == "yoffset") info.yoffset = value;
		else if (key == "xadvance") info.xadvance = value;
	}

	if (charId != 0 && info.srcW > 0 && info.srcH > 0)
	{
		g_CharMap[static_cast<wchar_t>(charId)] = info;
	}
}

// 辅助函数：获取单个字符的渲染宽度
static int GetCharWidth(wchar_t c)
{
	auto it = g_CharMap.find(c);
	if (it != g_CharMap.end()) {
		return it->second.xadvance;
	}
	return 16; // 返回一个默认的空格宽度
}


// --- 4. 实现模块的公共函数 ---

void Font_Initialize()
{
	g_FontTextureID = Texture_LoadFromFile(L"resource/texture/rfont_0.png");
	g_CharMap.clear();

	std::ifstream file("resource/texture/rfont.fnt");

	std::string line;
	while (std::getline(file, line))
	{
		if (line.rfind("char ", 0) == 0)
		{
			ParseCharLine(line);
		}
	}
	file.close();
}

void Font_Finalize()
{
	g_CharMap.clear();
}

void Font_Draw(const wchar_t* text, float dx, float dy, const DirectX::XMFLOAT4& color)
{
	if (g_FontTextureID < 0 || g_CharMap.empty()) return;

	float cursorX = dx;
	float cursorY = dy;

	for (int i = 0; text[i] != L'\0'; ++i)
	{
		wchar_t c = text[i];
		if (c == L'\n') {
			cursorX = dx;
			cursorY += 48.0f;
			continue;
		}

		auto it = g_CharMap.find(c);
		if (it == g_CharMap.end()) {
			cursorX += 16;
			continue;
		}

		const CharInfo& info = it->second;
		float finalX = cursorX + info.xoffset;
		float finalY = cursorY + info.yoffset;

		Sprite_Draw(g_FontTextureID, finalX, finalY, (float)info.srcW, (float)info.srcH,
			info.srcX, info.srcY, info.srcW, info.srcH, color);

		cursorX += info.xadvance;
	}
}

void Font_DrawWrapped(const wchar_t* text, float dx, float dy, float maxWidth, const DirectX::XMFLOAT4& color)
{
	if (g_FontTextureID < 0 || g_CharMap.empty()) return;

	float cursorY = dy;
	const float lineHeight = 48.0f;

	std::wstring remainingText(text);
	while (!remainingText.empty())
	{
		size_t breakPos = std::wstring::npos;
		float currentLineWidth = 0;

		// 找到这一行能容纳的最后一个字符
		for (size_t i = 0; i < remainingText.length(); ++i) {
			currentLineWidth += GetCharWidth(remainingText[i]);
			if (currentLineWidth > maxWidth) {
				breakPos = i;
				break;
			}
		}

		// 如果一行都放不下，就在 breakPos 处断开
		if (breakPos != std::wstring::npos) {
			// 尝试在断点前回溯，找到最后一个空格或标点，以实现更自然的单词换行
			size_t wordBreakPos = remainingText.find_last_of(L" \t,.", breakPos);
			if (wordBreakPos != std::wstring::npos && wordBreakPos > 0) {
				breakPos = wordBreakPos;
			}

			std::wstring lineToDraw = remainingText.substr(0, breakPos);
			Font_Draw(lineToDraw.c_str(), dx, cursorY, color);
			remainingText = remainingText.substr(breakPos);

			// 去掉下一行开头的空格
			while (!remainingText.empty() && (remainingText[0] == L' ' || remainingText[0] == L'\t')) {
				remainingText.erase(0, 1);
			}

		}
		else { // 如果剩余的文本能在一行内放完
			Font_Draw(remainingText.c_str(), dx, cursorY, color);
			remainingText.clear();
		}

		cursorY += lineHeight; // 换行
	}
}

