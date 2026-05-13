#pragma once

void InitAudio();
void UninitAudio();

int  LoadAudio(const char* fileName);
void UnloadAudio(int index);
void PlayAudio(int index, bool loop = false);
void SetVolume(int index, float volume);
