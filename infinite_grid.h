#pragma once

#ifndef INFINITE_GRID_H
#define INFINITE_GRID_H
#include "debug_text.h"

void InfiniteGrid_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);

void InfiniteGrid_Finalize();

void InfiniteGrid_Draw(const DirectX::XMMATRIX& mtxWorld);


#endif

