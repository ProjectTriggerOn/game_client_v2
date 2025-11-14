#pragma once

#ifndef INFINITE_GRID_H
#define INFINITE_GRID_H
#include "debug_text.h"

bool InfiniteGrid_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);

void InfiniteGrid_Finalize();

void InfiniteGrid_Draw();


#endif

