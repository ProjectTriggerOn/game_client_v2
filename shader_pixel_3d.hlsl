
struct PS_IN
{
    float4 posH : SV_POSITION;
    float4 color : COLOR0;
};


float4 main(PS_IN ps_in) : SV_TARGET
{
    return ps_in.color;
}