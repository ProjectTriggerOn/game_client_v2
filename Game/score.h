#pragma once
#ifndef SCORE_H
#define SCORE_H
void Score_Initialize(float x,float y,int digit);
void Score_Finalize();
void Score_Draw();
void Score_Update();
unsigned int Score_GetScore();
void Score_AddScore(int score);
void Score_Reset();

#endif