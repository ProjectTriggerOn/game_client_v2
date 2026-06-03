#pragma once
#include <d3d11.h>

namespace UI {

// 把 Ultralight 的 BGRA 像素拷到 D3D11 动态纹理，再画全屏 quad。
// Draw() 内部 save/restore 所有受影响的 pipeline state，调用者无需关心副作用。
class D3D11BitmapRenderer {
public:
    bool Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx, int w, int h);
    void Finalize();

    // 从 Ultralight BitmapSurface 上传像素到 dynamic texture（DISCARD map）
    void UpdateFromBitmap(const void* bgra, int w, int h, int row_bytes);

    // 全屏 quad + premul alpha blend；前后 save/restore D3D11 state
    void Draw();

private:
    ID3D11Device*             m_device   = nullptr;
    ID3D11DeviceContext*      m_context  = nullptr;
    ID3D11Texture2D*          m_texture  = nullptr;
    ID3D11ShaderResourceView* m_srv      = nullptr;
    ID3D11VertexShader*       m_vs       = nullptr;
    ID3D11PixelShader*        m_ps       = nullptr;
    ID3D11SamplerState*       m_sampler  = nullptr;
    ID3D11BlendState*         m_blend    = nullptr;
    ID3D11DepthStencilState*  m_depth    = nullptr;
    ID3D11RasterizerState*    m_raster   = nullptr;
    int m_width  = 0;
    int m_height = 0;
};

}  // namespace UI
