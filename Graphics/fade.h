#pragma once
#ifndef FADE_H
#define FADE_H

#include <DirectXMath.h>
void Fade_Initialize();
void Fade_Finalize();
void Fade_Update(double elapsed_time);
void Fade_Draw();

// Clear any active/finished fade (state -> NONE) WITHOUT reloading the texture
// (unlike Fade_Initialize). Used to drop a lingering death/respawn overlay so it
// can't bleed across a scene transition.
void Fade_Reset();

void Fade_Start(double time, bool isFadeout, DirectX::XMFLOAT3 color = { 0.0f,0.0f,0.0f });

enum FadeState : int
{
	FADE_STATE_NONE,
	FADE_STATE_FINISHED_IN,
	FADE_STATE_FINISHED_OUT,
	FADE_STATE_FADEIN,
	FADE_STATE_FADEOUT,
	FADE_STATE_MAX
};

FadeState Fade_GetState();
#endif // !FADE_H
