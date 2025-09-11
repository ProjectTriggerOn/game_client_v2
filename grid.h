#pragma once
#ifndef GRID_H	
#define GRID_H
#include <d3d11.h>

void Grid_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Grid_Finalize(void);
void Grid_Draw(void);

#endif