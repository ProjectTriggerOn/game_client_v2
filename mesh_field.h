#ifndef MESH_FIELD_H
#define MESH_FIELD_H
#include "debug_text.h"

void MeshField_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);

void MeshField_Finalize();

void MeshField_Draw(const DirectX::XMMATRIX& mtxW);



#endif