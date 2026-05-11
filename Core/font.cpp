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

// Render info for a single BMFont glyph.
struct CharInfo
{
	int srcX, srcY;
	int srcW, srcH;
	int xoffset, yoffset;
	int xadvance;
};

static int g_FontTextureID = -1;
static std::map<wchar_t, CharInfo> g_CharMap;

// Parses a single "char ..." line from a BMFont .fnt file.
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

// Render width of a single glyph (fallback to a default space width).
static int GetCharWidth(wchar_t c)
{
	auto it = g_CharMap.find(c);
	if (it != g_CharMap.end()) {
		return it->second.xadvance;
	}
	return 16;
}


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

void Font_Draw(const wchar_t* text, float dx, float dy, const DirectX::XMFLOAT4& color, float scale)
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

		Sprite_Draw(g_FontTextureID, finalX, finalY, (float)info.srcW * scale, (float)info.srcH * scale,
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

		// Find the last character that fits on this line.
		for (size_t i = 0; i < remainingText.length(); ++i) {
			currentLineWidth += GetCharWidth(remainingText[i]);
			if (currentLineWidth > maxWidth) {
				breakPos = i;
				break;
			}
		}

		if (breakPos != std::wstring::npos) {
			// Back up to the last space/punctuation for a more natural word wrap.
			size_t wordBreakPos = remainingText.find_last_of(L" \t,.", breakPos);
			if (wordBreakPos != std::wstring::npos && wordBreakPos > 0) {
				breakPos = wordBreakPos;
			}

			std::wstring lineToDraw = remainingText.substr(0, breakPos);
			Font_Draw(lineToDraw.c_str(), dx, cursorY, color);
			remainingText = remainingText.substr(breakPos);

			// Trim leading whitespace from the next line.
			while (!remainingText.empty() && (remainingText[0] == L' ' || remainingText[0] == L'\t')) {
				remainingText.erase(0, 1);
			}
		}
		else {
			// Remaining text fits in a single line.
			Font_Draw(remainingText.c_str(), dx, cursorY, color);
			remainingText.clear();
		}

		cursorY += lineHeight;
	}
}

float Font_MeasureWidth(const wchar_t* text)
{
	float width = 0.0f;
	for (int i = 0; text[i] != L'\0'; ++i)
	{
		auto it = g_CharMap.find(text[i]);
		width += (it != g_CharMap.end()) ? static_cast<float>(it->second.xadvance) : 16.0f;
	}
	return width;
}

float Font_LineHeight()
{
	return 32.0f; // rfont.fnt: lineHeight=32
}

