#ifdef PERPIXEL
#include "perPixelConstants.h"
#include "lighting.h"
#else
float4 fogColor : register(c0);
#endif

struct VS_out {
	float4 Position		: POSITION;
	float3 TexCoord0	: TEXCOORD0;
	float4 Color		: COLOR0;
#ifdef PERPIXEL
	float3 Normal		: TEXCOORD1;
#endif
};

sampler2D tex0 : register(s0);

float4 main(VS_out input) : COLOR
{
	float4 color = input.Color;

#ifdef PERPIXEL
	// The vertex shader handed over the prelight and a normal and did nothing
	// else. What follows is default_VS.hlsl's lighting, in the same order and
	// with the same clamp, evaluated here instead.
	float3 N = normalize(input.Normal);

	color.rgb += ppAmbient.rgb * ppSurfAmbient;

	// Eight lights, always. ps_2_0 has no loops and no branches, so the count
	// cannot be a constant the shader reads -- every slot is evaluated and a
	// slot that holds no light is one whose colour d3drender.cpp uploaded as
	// zero. That is about twenty-four instructions of the sixty-four available.
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
	color *= tex2D(tex0, input.TexCoord0.xy);
#endif
	color.rgb = lerp(fogColor.rgb, color.rgb, input.TexCoord0.z);
	return color;
}
