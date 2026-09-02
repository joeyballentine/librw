#include "pixelConstants.h"
#ifdef PERPIXEL
#include "lighting.h"
#endif

struct VS_out {
	float4 Position		: SV_POSITION;
	float3 TexCoord0	: TEXCOORD0;
	float4 Color		: COLOR0;
#ifdef PERPIXEL
	float3 Normal		: TEXCOORD1;
#endif
};

Texture2D tex0 : register(t0);
SamplerState tex0Sampler : register(s0);

float4 main(VS_out input) : SV_TARGET
{
	float4 color = input.Color;

#ifdef PERPIXEL
	// The vertex shader handed over the prelight and a normal and did nothing
	// else. What follows is default_VS.hlsl's lighting, in the same order and
	// with the same clamp, evaluated here instead.
	float3 N = normalize(input.Normal);

	color.rgb += ppAmbient.rgb * ppSurfAmbient;

	// Eight slots, always, as in the ps_2_0 shader this was ported from. A slot
	// that holds no light is one whose colour d3drender.cpp uploaded as zero,
	// so summing them all costs a few instructions and needs no count.
	float3 lit = float3(0.0, 0.0, 0.0);
	for(int i = 0; i < 8; i++)
		lit += DoDirLightPP(ppLightColor[i].rgb, ppLightDir[i].xyz, N);

	// One multiply by the diffuse coefficient rather than one per light. The
	// vertex path scales each light as it sums them; the coefficient is scalar,
	// so the two are the same sum.
	color.rgb += lit * ppSurfDiffuse;

	// PS2 clamps before material color
	color = clamp(color, 0.0, 1.0);
	color *= ppMatCol;
#endif

#ifdef TEX
	color *= tex0.Sample(tex0Sampler, input.TexCoord0.xy);
#endif
	DoAlphaTest(color.a);
	color.rgb = lerp(fogColor.rgb, color.rgb, input.TexCoord0.z);
	return color;
}
