#pragma once
#include <d3d11.h>

namespace UI {

// Copies Ultralight's BGRA pixels into a D3D11 dynamic texture, then draws a
// full-screen quad. Draw() saves/restores every pipeline state it touches, so
// callers never see side effects.
class D3D11BitmapRenderer {
public:
    bool Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx, int w, int h);
    void Finalize();

    // Recreate only the size-dependent dynamic texture + SRV at the new size
    // (shaders/sampler/blend/depth/raster are size-independent and preserved).
    bool Resize(int w, int h);

    // Upload pixels from the Ultralight BitmapSurface into the dynamic texture (DISCARD map)
    void UpdateFromBitmap(const void* bgra, int w, int h, int row_bytes);

    // Full-screen quad + premultiplied alpha blend; saves/restores D3D11 state around it
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
