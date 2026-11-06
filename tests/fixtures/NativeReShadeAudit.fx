#include "ReShade.fxh"
texture Color : COLOR;
sampler ColorSampler { Texture = Color; };
texture Depth : DEPTH;
sampler DepthSampler { Texture = Depth; };
uniform float Exposure = 0;
uniform int Mode = 0;
void VS(uint id : SV_VertexID, out float4 position : SV_Position, out float2 uv : TEXCOORD) {
    uv = float2((id << 1) & 2, id & 2);
    position = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}
float4 PS(float4 position : SV_Position, float2 uv : TEXCOORD) : SV_Target {
    if (Mode == 1) return float4(tex2D(DepthSampler, uv).rrr, 1);
    if (Mode == 2) return float4((ReShade::GetLinearizedDepth(uv) * 1000).rrr, 1);
    return float4(tex2D(ColorSampler, uv).rgb * exp2(Exposure), 1);
}
technique Audit { pass { VertexShader = VS; PixelShader = PS; } }
