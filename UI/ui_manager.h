#pragma once

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace UI {

// Slice A: 仅最小骨架，硬编码 HTML 显示红底 Hello。
// 后续切片会加 ProcessInput / Update / OnGameStateChanged / Push* 等。
void Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx, int backBufW, int backBufH);
void Finalize();
void Render();

}  // namespace UI
