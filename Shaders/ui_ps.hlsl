// Sample BGRA texture (Ultralight BitmapSurface output).
// Texture is already premultiplied alpha — blend state handles compositing.

Texture2D    tex : register(t0);
SamplerState smp : register(s0);

struct PSIn {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 main(PSIn i) : SV_TARGET {
    return tex.Sample(smp, i.uv);
}
