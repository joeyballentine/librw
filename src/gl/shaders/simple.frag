uniform sampler2D tex0;

FSIN vec4 v_color;
FSIN vec2 v_tex0;
FSIN float v_fog;
#ifdef SHADOWRECEIVER
// Only where a vertex shader writes it. im2d.vert and im3d.vert pair with this
// same fragment shader and have no world position to transform, and reading a
// varying the vertex stage never wrote is a link error rather than a warning.
FSIN vec4 v_shadowPos;
#ifndef PERPIXEL
// How squarely the surface faces the light, interpolated. Where PERPIXEL is on
// the normal is here already and this is worked out from that instead.
FSIN float v_shadowNdl;
#endif
#endif
#ifdef PERPIXEL
FSIN vec3 v_normal;
#endif

void
main(void)
{
	vec4 color = v_color;

#ifdef PERPIXEL
	// The vertex shader handed over the prelight and a normal and did nothing
	// else. What follows is default.vert's lighting, in the same order and with
	// the same clamp, evaluated here instead. lighting.frag declares the
	// uniforms it reads.
	vec3 N = normalize(v_normal);
	color.rgb += u_ambLight.rgb*surfAmbient;
	color.rgb += DoDynamicLightPP(N)*surfDiffuse;
	color = clamp(color, 0.0, 1.0);
	color *= u_matColor;
#endif

	color *= texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));

	// After the material and before the fog. Before the fog because a shadow is
	// a property of the surface and fog is a property of the air in front of it
	// -- darkening a fogged colour would tint the fog itself.
	//
	// Applied to the whole colour here, which is right while the world's
	// lighting is baked into that colour. If the world is ever lit at run time
	// this has to move to multiply the direct term only and never the ambient,
	// or every shadowed surface goes black. docs/SHADOWS.md says so too; this
	// is the one line it is talking about.
#ifdef SHADOWRECEIVER
#ifdef PERPIXEL
	// The normal is already here for the lighting, so the shadow test can use
	// it to skip surfaces that face away from the light. Those are the ones the
	// caster pass recorded, so they would otherwise compare against themselves.
	//
	// v_normal and not N: ShadowFactorN wants it unnormalized so it can tell a
	// missing normal from a real one, and N is already a NaN where there is no
	// normal to normalize.
	color.rgb *= ShadowFactorN(v_shadowPos, v_normal);
#else
	color.rgb *= ShadowFactorV(v_shadowPos, v_shadowNdl);
#endif
#endif

	color.rgb = mix(u_fogColor.rgb, color.rgb, v_fog);
	DoAlphaTest(color.a);
	FRAGCOLOR(color);
}
