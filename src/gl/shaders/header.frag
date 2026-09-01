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

// The shadow map, and its knobs: x is on or off, y the depth bias, z what a
// shadowed pixel is multiplied by.
//
// tex2 and not a name of its own, because Shader::create binds tex0..tex3 to
// texture units 0..3 by name. Unit 0 is the material's texture and unit 1 is
// the environment map -- matfx_env.frag declares tex1 itself, so putting the
// shadow map there both redeclared the sampler and fought for the unit.
uniform sampler2D tex2;
uniform vec4 u_shadowParams;

#define shadowEnabled (u_shadowParams.x)
#define shadowBias (u_shadowParams.y)
#define shadowStrength (u_shadowParams.z)

// Undo depth.frag's packing. The dot is the encode read backwards: each channel
// carries the fraction the ones before it could not.
float
UnpackDepth(vec4 c)
{
	return dot(c.rgb, vec3(1.0, 1.0/255.0, 1.0/65025.0));
}

// 1.0 in light, shadowStrength in shadow.
//
// The bounds test is not an optimisation. The map covers a slab of the world
// and CLAMP addressing means everything outside it samples the edge texel, so
// without this every surface beyond the volume takes whatever the border
// happens to hold -- the same trap xShadow.cpp's border comment describes, in
// the other direction.
float
ShadowFactor(vec4 shadowPos)
{
	if(shadowEnabled == 0.0)
		return 1.0;

	vec3 t = shadowPos.xyz*0.5 + 0.5;

	if(t.x < 0.0 || t.x > 1.0 || t.y < 0.0 || t.y > 1.0 || t.z > 1.0)
		return 1.0;

	float casterDepth = UnpackDepth(texture(tex2, t.xy));

	return t.z - shadowBias > casterDepth ? shadowStrength : 1.0;
}

void DoAlphaTest(float a)
{
#ifndef NO_ALPHATEST
	if(a < u_alphaRef.x || a >= u_alphaRef.y)
		discard;
#endif
}
