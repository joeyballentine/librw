// The cel look, in pixel shader constants.
//
// c27 to c29, above perPixelConstants.h's c8..c26 and well above the c1..c7
// that other passes scribble on. ps_2_0 allows 32.
//
// **Both the key direction and the room colour arrive already worked out.**
// The GL3 version finds them by looping over the light array in the shader,
// which ps_2_0 cannot do -- it has neither loops nor branches, so eight lights
// means eight unrolled comparisons in every pixel. They are per-draw
// quantities and not per-pixel ones, so d3drender.cpp computes them once on
// the way to the uniform and what is left here is a dot, a lookup and two
// multiplies. That is why the toon path is CHEAPER than the lighting it
// replaces rather than an addition to it.

float4 toonParams : register(c27);
// Where the light travels, already resolved: the character's own front, the
// camera, or the room's brightest light, whichever the setting asked for.
float4 toonLightDir : register(c28);
// What colour it is in here, with no direction in it.
float4 toonRoom : register(c29);

#define toonEnabled (toonParams.x)
#define toonSaturation (toonParams.z)
#define toonStrength (toonParams.w)

// The colour strip the light term looks up. Band count, widths and colours are
// properties of the texture, so a character can be retuned without a rebuild.
sampler2D tex3 : register(s3);

float3 ToonRamp(float l)
{
	// Half a texel in, so the two ends sample their own colour rather than
	// blending with the clamp.
	return tex2D(tex3, float2(clamp(l, 0.02, 0.98), 0.5)).rgb;
}

// Push colour away from grey. Mixing AWAY from luminance -- a factor above one
// -- leaves greys alone and pulls everything else outward.
float3 ToonSaturate(float3 c)
{
	float l = dot(c, float3(0.299, 0.587, 0.114));

	return saturate(lerp(l.xxx, c, toonSaturation));
}
