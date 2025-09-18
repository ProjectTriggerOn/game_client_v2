#pragma once
#ifndef SAMPLER_H
#define SAMPLER_H
#include <d3d11.h>

void Sampler_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Sampler_Finalize();
void Sampler_SetFilterPoint();
void Sampler_SetFilterLinear();
void Sampler_SetFilterAnisotropic();




#endif
