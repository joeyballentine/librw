// c0, as every pixel shader here has it.
float4 fogColor : register(c0);

#include "toonConstants.h"

// The ink, and how to read it: rgb is either a colour outright or a scale on
// the surface, and w says which. The vertex shader picks the region.
struct VS_out {
	float4 Position		: POSITION;
	float3 TexCoord0	: TEXCOORD0;
	float4 Color		: COLOR0;
	float4 Outline		: TEXCOORD1;
};

sampler2D tex0 : register(s0);

float4 main(VS_out input) : COLOR
{
	// Two ways to read the ink, chosen per region.
	//
	// Scaled: a darkened copy of whatever the surface is painted, which gives
	// every character an ink on its own hue for nothing. Flat: a colour named
	// outright, for a region whose ink is not a darker version of itself --
	// SpongeBob's trousers are inked black whatever they are painted.
	float4 tex = tex2D(tex0, input.TexCoord0.xy);
	float3 ink = lerp(tex.rgb*input.Outline.rgb, input.Outline.rgb, input.Outline.a);

	// Lit like everything else. A line that stayed the same colour while the
	// surface it surrounds went blue reads as something laid over the scene
	// rather than part of it.
	float4 color = float4(ink*toonRoom.rgb, 1.0);

	color.rgb = lerp(fogColor.rgb, color.rgb, input.TexCoord0.z);
	return color;
}
