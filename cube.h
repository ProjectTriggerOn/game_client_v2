#pragma once
#ifndef CUBE_H
#define CUBE_H

#include <d3d11.h>

void Cube_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Cube_Finalize(void);
void Cube_Draw(void);


#endif