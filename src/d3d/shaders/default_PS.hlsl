// TOON takes perPixelConstants too: it wants the material colour and the fog
// colour, and it needs the same normal the per-pixel path does.
#if defined(PERPIXEL) || defined(TOON)
#include "perPixelConstants.h"
#include "lighting.h"
#else
float4 fogColor : register(c0);
#endif

#ifdef TOON
#include "toonConstants.h"
#endif

struct VS_out {
	float4 Position		: POSITION;
	float3 TexCoord0	: TEXCOORD0;
	float4 Color		: COLOR0;
#if defined(PERPIXEL) || defined(TOON)
	float3 Normal		: TEXCOORD1;
#endif
};

sampler2D tex0 : register(s0);

float4 main(VS_out input) : COLOR
{
	float4 color = input.Color;

#ifdef TOON
	// **The lighting is replaced, not shaded on top of.**
	//
	// The room contributes its colour and nothing else, flat across the model,
	// so a cave darkens the shadow band and Rock Bottom turns it blue without
	// putting back the smooth falloff the bands exist to remove. See
	// simple.frag on the GL3 side; this is the same arithmetic.
	float3 N = normalize(input.Normal);
	float3 cel = ToonRamp(saturate(dot(N, -toonLightDir.xyz)));

	color.rgb = toonRoom.rgb * lerp(float3(1.0, 1.0, 1.0), cel, toonStrength);
	color.a *= ppMatCol.a;
#elif defined(PERPIXEL)
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
#ifdef TOON
	color.rgb = ToonSaturate(color.rgb);
#endif

	color.rgb = lerp(fogColor.rgb, color.rgb, input.TexCoord0.z);
	return color;
}
