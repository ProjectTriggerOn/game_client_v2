// Full-screen triangle via SV_VertexID — no vertex/index buffers needed.
// Vertex 0 = (-1,-1), Vertex 1 = (-1, 3), Vertex 2 = ( 3,-1).
// The oversized triangle covers the entire viewport; rasterizer clips to [-1,1].

struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOut main(uint id : SV_VertexID) {
    VSOut o;
    float2 p = float2((id == 2) ? 3.0 : -1.0, (id == 1) ? 3.0 : -1.0);
    o.pos = float4(p, 0, 1);
    o.uv  = float2((p.x + 1) * 0.5, 1.0 - (p.y + 1) * 0.5);
    return o;
}
