#pragma once

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace UI {

void Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx, int backBufW, int backBufH);
void Finalize();
void Render();

// Slice B 验证用：调 EvaluateScript("Router.show('<name>')")。
// 后续 Slice D 会被正经 JS Bridge 替代。
void ShowPage(const char* name);

}  // namespace UI
