#ifdef USE_UBOS
layout(std140) uniform State
{
	vec2 u_alphaRef;
	vec4  u_fogData;
	vec4  u_fogColor;
};
#else
uniform vec4 u_alphaRef;

uniform vec4  u_fogData;
uniform vec4  u_fogColor;
#endif

#define u_fogStart (u_fogData.x)
#define u_fogEnd (u_fogData.y)
#define u_fogRange (u_fogData.z)
#define u_fogDisable (u_fogData.w)

#ifndef GL2
out vec4 fragColor;
#endif

// The shadow map, and its knobs.
//
// u_shadowParams is (on, bias, strength, slope bias) and u_shadowParams2.x is
// one texel of the map in texture coordinates. The two bias terms and the texel
// are all handed over already converted into the map's own 0..1 depth units, so
// nothing here has to know how deep the light volume is. iShadowMap.cpp does
// that conversion, because it is the only thing that knows.
//
// tex2 and not a name of its own, because Shader::create binds tex0..tex3 to
// texture units 0..3 by name. Unit 0 is the material's texture and unit 1 is
// the environment map -- matfx_env.frag declares tex1 itself, so putting the
// shadow map there both redeclared the sampler and fought for the unit.
uniform sampler2D tex2;
uniform vec4 u_shadowParams;
uniform vec4 u_shadowParams2;
// Where the light travels, from it towards what it lights.
uniform vec4 u_shadowLightDir;

#define shadowEnabled (u_shadowParams.x)
#define shadowBias (u_shadowParams.y)
#define shadowStrength (u_shadowParams.z)
#define shadowSlopeBias (u_shadowParams.w)
#define shadowTexel (u_shadowParams2.x)

// Undo depth.frag's packing. The dot is the encode read backwards: each channel
// carries the fraction the ones before it could not.
float
UnpackDepth(vec4 c)
{
	return dot(c.rgb, vec3(1.0, 1.0/255.0, 1.0/65025.0));
}

// One comparison against one texel: 1.0 if the light reaches here.
float
ShadowTap(vec2 uv, float depth)
{
	return depth > UnpackDepth(texture(tex2, uv)) ? 0.0 : 1.0;
}

// 1.0 in light, shadowStrength in shadow, and the values between where the
// filter straddles an edge.
//
// Nine taps in a square, averaged. The map cannot be filtered by the hardware
// -- it holds depth packed across three bytes, and a linear filter would
// average the bytes of two unrelated depths into a number that is neither -- so
// the comparison happens first and the RESULTS are what get averaged. That is
// the whole of PCF, and it is why the taps are written out rather than done
// with a wider filter mode.
//
// The bounds test is not an optimisation. The map covers a slab of the world
// and CLAMP addressing means everything outside it samples the edge texel, so
// without this every surface beyond the volume takes whatever the border
// happens to hold -- the same trap xShadow.cpp's border comment describes, in
// the other direction.
float
ShadowLookup(vec4 shadowPos, float bias)
{
	if(shadowEnabled == 0.0)
		return 1.0;

	vec3 t = shadowPos.xyz*0.5 + 0.5;

	if(t.x < 0.0 || t.x > 1.0 || t.y < 0.0 || t.y > 1.0 || t.z > 1.0)
		return 1.0;

	float d = t.z - bias;
	float o = shadowTexel;

	float lit = ShadowTap(t.xy + vec2(-o, -o), d) +
	            ShadowTap(t.xy + vec2(0.0, -o), d) +
	            ShadowTap(t.xy + vec2( o, -o), d) +
	            ShadowTap(t.xy + vec2(-o, 0.0), d) +
	            ShadowTap(t.xy, d) +
	            ShadowTap(t.xy + vec2( o, 0.0), d) +
	            ShadowTap(t.xy + vec2(-o,  o), d) +
	            ShadowTap(t.xy + vec2(0.0,  o), d) +
	            ShadowTap(t.xy + vec2( o,  o), d);

	return mix(shadowStrength, 1.0, lit*(1.0/9.0));
}

// The test, given how squarely the surface faces the light. Two things come of
// that number, and both matter.
//
// A surface facing away from the light needs no map: it cannot see the light,
// and the lighting has already darkened it. Asking anyway is worse than
// pointless, because those are exactly the surfaces whose depth IS the map --
// the caster pass stores back faces -- so each one compares against its own
// record and breaks into stripes on the rounding.
//
// And a surface nearly edge-on to the light needs a larger bias than one facing
// it, because one texel of the map covers more depth the more the surface
// slopes away. tan of the angle to the light is exactly that ratio, which is
// the sqrt over ndl. Capped, or a surface at ninety degrees asks for infinity.
float
ShadowFactorV(vec4 shadowPos, float ndl)
{
	if(ndl <= 0.0)
		return 1.0;

	float slope = min(sqrt(max(1.0 - ndl*ndl, 0.0))/ndl, 8.0);

	return ShadowLookup(shadowPos, shadowBias + shadowSlopeBias*slope);
}

// The same, worked out here from a normal the fragment stage already has.
// Sharper than the interpolated number on a low-polygon model, which is where
// every character in this game sits.
//
// Takes the normal UNNORMALIZED, and that is the point. Geometry with no
// normals hands over a zero, normalize of which is a NaN, and a NaN reaching
// the comparisons below makes the result a driver's opinion rather than an
// answer -- header.vert's DoShadowNdl says more. A surface with no normal still
// needs its shadow, so it gets the test with the fixed slope allowance and no
// facing check.
float
ShadowFactorN(vec4 shadowPos, vec3 N)
{
	float len2 = dot(N, N);

	if(len2 < 1e-12)
		return ShadowLookup(shadowPos, shadowBias + shadowSlopeBias);

	return ShadowFactorV(shadowPos, dot(N, -u_shadowLightDir.xyz)*inversesqrt(len2));
}

// The outline's colour, with its thickness in world units in alpha.
uniform vec4 u_outlineColor;
uniform vec4 u_outlineColor2;
uniform vec4 u_outlineFlags;

// A light locked to the model rather than to the world, in xyz, with w saying
// whether to use it.
//
// **This is how the show lights a character and it is not how a renderer does.**
// An animator draws SpongeBob's front flat yellow and his side a solid darker
// green, and that stays true however he turns or wherever the sun is -- the
// shading describes the SHAPE, not the lighting. A world-space light cannot do
// that: turn the character and the dark side swings round with the room.
//
// So for a character the direction is taken from his own matrix instead, and
// travels straight back through him from the front. His face is then always in
// the top band and his sides always in the bottom one, whichever way he faces.
uniform vec4 u_toonLightDir;
// The colour of the room a character is standing in, with w saying whether to
// use it in place of his own lighting.
//
// A level lights its world and its objects with different rigs, and Rock Bottom
// is the case that shows why it matters: the room gets a blue kit summing to
// 0.83 1.52 1.77 and the characters get a grey one summing to 1.50 1.50 1.50.
// That is the game's own authoring and it is right for a renderer that draws
// characters as objects -- but a cartoon paints everything in a scene from the
// same palette, so a character standing in a blue room is drawn blue.
uniform vec4 u_toonRoomTint;

// Two more of the look, both about flattening rather than lighting.
//
// x is how many shades a character's colours are cut down to, 0 to leave them
// alone. The rest is spare.
uniform vec4 u_toonExtra;

#define toonColors (u_toonExtra.x)


// The stylised look, off unless the application asks for it.
//
// x is on or off, y how many steps the light is cut into, z how far colour is
// pushed away from grey, w how strongly a surface facing away from the camera
// is lifted.
uniform vec4 u_toonParams;

#define toonEnabled (u_toonParams.x)
#define toonBands (u_toonParams.y)
#define toonSaturation (u_toonParams.z)
#define toonStrength (u_toonParams.w)

// The colour ramp: what the light term looks up instead of being multiplied in
// directly.
//
// **This is the difference between a cel shade and a dimmer switch.** Banding
// the light arithmetically gives every step the same hue and only varies how
// much of it there is, so the shadow side of a character is the lit side turned
// down. A drawing does not do that -- its shadows shift towards blue while the
// lit side stays warm -- and no amount of arithmetic on a scalar can express
// that, because the colour has to come from somewhere.
//
// So the term indexes a strip of authored colour instead. Band count, band
// widths and band colours are all properties of the texture rather than of this
// shader, which is what lets a character be retuned without a rebuild.
//
// tex3, above the material texture, the environment map and the shadow map.
uniform sampler2D tex3;

vec3 ToonRamp(float l)
{
	// Half a texel in, so the two ends of the strip sample their own colour
	// rather than blending with the clamp.
	return texture(tex3, vec2(clamp(l, 0.02, 0.98), 0.5)).rgb;
}


// Push colour away from grey.
//
// The show's palette is far more saturated than anything a light rig produces,
// and the levels were painted to match it before the bake flattened them out.
// Mixing AWAY from luminance -- a factor above one -- is the cheap way back:
// it leaves greys alone and pulls everything else outward.
//
// Applied after the texture, unlike the banding, because it is the artwork's
// colour that wants pushing and not the light's.
// Cut a colour down to a handful of shades, keeping the colour.
//
// **Rounding each channel on its own was the obvious way and it is wrong.** It
// moves the three channels by different amounts, so the HUE shifts as a colour
// lands: a face rounds towards pink in one band and towards orange in the
// next, which is banding that draws attention to itself rather than flatness
// that does not.
//
// Scaling instead. The brightest channel is rounded to a level and the whole
// colour is scaled by that ratio, so all three move together: hue and
// saturation come through untouched and only brightness is stepped. That is
// the axis the artwork actually varies along -- these textures were painted
// from a small palette and then shaded, and it is the shading that wants
// removing.
vec3 ToonQuantize(vec3 c)
{
	if(toonColors < 2.0)
		return c;

	float v = max(c.r, max(c.g, c.b));

	// Black has no hue to keep and no brightness to round.
	if(v < 1.0/255.0)
		return c;

	return c*(floor(v*toonColors + 0.5)/toonColors)/v;
}

vec3 ToonSaturate(vec3 c)
{
	if(toonEnabled == 0.0)
		return c;

	float l = dot(c, vec3(0.299, 0.587, 0.114));

	return clamp(mix(vec3(l), c, toonSaturation), 0.0, 1.0);
}

void DoAlphaTest(float a)
{
#ifndef NO_ALPHATEST
	if(a < u_alphaRef.x || a >= u_alphaRef.y)
		discard;
#endif
}
