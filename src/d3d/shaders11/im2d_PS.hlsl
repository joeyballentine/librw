#include "alphaTest.h"

struct VS_out {
	float4 Position		: SV_POSITION;
	float3 TexCoord0	: TEXCOORD0;
	float4 Color		: COLOR0;
};

Texture2D tex0 : register(t0);
SamplerState tex0Sampler : register(s0);

float4 main(VS_out input) : SV_TARGET
{
	float4 color = input.Color;
#ifdef TEX
	color *= tex0.Sample(tex0Sampler, input.TexCoord0.xy);
#endif
	DoAlphaTest(color.a);
	color.rgb = lerp(fogColor.rgb, color.rgb, input.TexCoord0.z);
	return color;
}
