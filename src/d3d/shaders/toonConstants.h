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
// The rest of the look, in two registers because there are seven things and a
// register is four floats.
//
// **toonIsCharacter gates everything that is not lighting.** The world and the
// characters take the same shader, and most of what follows is wrong on a
// background: a painted backdrop does not want its colours rounded, does not
// want a rim, and has no baked occlusion worth reading.
float4 toonExtra : register(c30);
float4 toonExtra2 : register(c31);

#define toonEnabled (toonParams.x)
#define toonSaturation (toonParams.z)
#define toonStrength (toonParams.w)

#define toonColors (toonExtra.x)
#define toonRampRow (toonExtra.y)
#define toonIsCharacter (toonExtra.z)
#define toonWrap (toonExtra.w)

#define toonRim (toonExtra2.x)
#define toonRimEdge (toonExtra2.y)
#define toonOcclusion (toonExtra2.z)
#define toonHardness (toonExtra2.w)

// The colour strip the light term looks up. Band count, widths and colours are
// properties of the texture, so a character can be retuned without a rebuild.
sampler2D tex3 : register(s3);

// How many ramps are stacked in the strip. iToon.cpp builds them and the game
// names a row per draw -- skin does not band like sheet metal.
#define TOON_RAMP_ROWS 4.0

float3 ToonRampAt(float l)
{
	// Half a texel in on both axes, so the two ends of a row sample their own
	// colour rather than blending with the clamp, and a row samples itself
	// rather than the one above it.
	return tex2D(tex3, float2(clamp(l, 0.02, 0.98),
	                          (toonRampRow + 0.5)/TOON_RAMP_ROWS)).rgb;
}

// The ramp, sampled across one pixel of the light term rather than at a point.
//
// **A band edge is a step function and a step function aliases.** The strip is
// point sampled on purpose -- filtering it would turn every band back into the
// gradient the bands exist to replace -- so the edge is genuinely hard, and at
// one sample per pixel it crawls along a character as he walks.
//
// ddx and ddy say how much the light term changes between this pixel and the
// next, so four taps spread across that are four samples of what the pixel
// actually covers: flat, they land in one band and nothing changes; across an
// edge, they straddle it and the average is the coverage.
//
// **This is why the toon shaders are ps_3_0 and the rest are ps_2_0.** ps_2_0
// has no derivative instructions at all. See make_default.cmd.
float3 ToonRamp(float l)
{
	float w = min(abs(ddx(l)) + abs(ddy(l)), 0.08);

	return 0.25*(ToonRampAt(l - 0.375*w) + ToonRampAt(l - 0.125*w) +
	             ToonRampAt(l + 0.125*w) + ToonRampAt(l + 0.375*w));
}

// What the ramp is indexed by: the facing, wrapped and occluded.
//
// D3D9 has no shadow map yet, so unlike the GL3 side there is nothing else to
// fold in here. When there is, it multiplies in alongside the occlusion and for
// the same reason -- see ToonLight in header.frag.
float ToonLight(float3 N, float3 L, float occlusion)
{
	float ndl = dot(N, -L);

	// Wrapped, if asked. A plain max collapses the whole far hemisphere to one
	// value; the half-lambert remap spreads it over the dark end of the strip
	// and gives it somewhere to put a second tone.
	float l = lerp(max(0.0, ndl), 0.5 + 0.5*ndl, toonWrap);

	return saturate(l*occlusion);
}

// How much light the artists said reaches this vertex, as a scale on the term.
//
// The models carry a baked colour per vertex, and under a chin or inside a fold
// it is dark -- occlusion somebody drew, which no light rig recovers. Geometry
// with no baked colour reads as black, and black here would black the model
// out, so nothing is what an absent value means.
float ToonOcclusion(float3 prelit)
{
	float v = max(prelit.r, max(prelit.g, prelit.b));

	if(toonOcclusion <= 0.0 || toonIsCharacter == 0.0 || v < 1.0/255.0)
		return 1.0;

	return lerp(1.0, v, toonOcclusion);
}

// A hard edge of light along the silhouette, in the colour of the room. Cheap,
// and what keeps a dark character legible against a dark background.
//
// Characters only: a rim on the world draws a bright line along every wall the
// camera happens to see edge-on.
float3 ToonRimLight(float3 N, float3 V, float3 room)
{
	if(toonRim <= 0.0 || toonIsCharacter == 0.0)
		return float3(0.0, 0.0, 0.0);

	float f = 1.0 - saturate(dot(N, normalize(V)));
	float w = max(abs(ddx(f)) + abs(ddy(f)), 1.0/255.0);

	return room*(toonRim*smoothstep(toonRimEdge - w, toonRimEdge + w, f));
}

// The face's own normal, from how the surface moves across the triangle.
//
// **Welding took the hard edges out of the shading and this puts them back.**
// The hull needs one normal per position or the inflated copy cracks open at
// every corner, so iToon.cpp averages them -- and that same average is what
// lights the surface, rounding off exactly the corners a drawing wants square.
//
// V is the vector to the eye, so its derivatives are the surface's negated, and
// a cross product of two negated vectors is unchanged. The sign is settled
// against the vertex normal either way, because a derivative follows the
// winding and a normal does not.
float3 ToonHardNormal(float3 N, float3 V)
{
	if(toonHardness <= 0.0 || toonIsCharacter == 0.0)
		return N;

	float3 Ng = cross(ddx(V), ddy(V));
	float len2 = dot(Ng, Ng);

	if(len2 < 1e-20)
		return N;

	Ng *= rsqrt(len2);

	if(dot(Ng, N) < 0.0)
		Ng = -Ng;

	return normalize(lerp(N, Ng, toonHardness));
}

// Cut a colour down to a handful of shades, keeping the colour.
//
// The brightest channel is rounded to a level and the whole colour scaled by
// that ratio, so all three move together and only brightness is stepped.
// Rounding each channel on its own shifts the hue as a colour lands, which is
// banding that draws attention to itself -- see header.frag on the GL3 side.
float3 ToonQuantize(float3 c)
{
	if(toonColors < 2.0)
		return c;

	float v = max(c.r, max(c.g, c.b));

	if(v < 1.0/255.0)
		return c;

	return c*(floor(v*toonColors + 0.5)/toonColors)/v;
}

// Push colour away from grey. Mixing AWAY from luminance -- a factor above one
// -- leaves greys alone and pulls everything else outward.
float3 ToonSaturate(float3 c)
{
	float l = dot(c, float3(0.299, 0.587, 0.114));
	float3 s = max(lerp(l.xxx, c, toonSaturation), 0.0);

	// **Scaled down to fit, not clipped per channel** -- the same rule as
	// ToonQuantize above and as the room colour in d3drender.cpp. Pushing away
	// from grey is what drives a channel past one, so clamping each on its own
	// shifts the hue of exactly the colours the setting was turned up for.
	float m = max(s.r, max(s.g, s.b));

	if(m > 1.0)
		s /= m;

	return s;
}
