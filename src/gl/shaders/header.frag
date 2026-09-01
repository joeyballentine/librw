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

void DoAlphaTest(float a)
{
#ifndef NO_ALPHATEST
	if(a < u_alphaRef.x || a >= u_alphaRef.y)
		discard;
#endif
}
