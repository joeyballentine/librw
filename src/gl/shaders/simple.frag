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
	// lighting.frag declares the uniforms both arms below read.
	vec3 N = normalize(v_normal);

	if(toonEnabled != 0.0){
		// **The lighting is replaced, not shaded on top of.**
		//
		// Mixing the room's per-pixel lighting back in was the obvious way to
		// keep a character tied to where he is standing, and it reads wrong:
		// what comes back with it is the smooth falloff the bands exist to
		// remove, so every band has a gradient inside it and the whole thing
		// looks like banding laid over lighting rather than like a drawing.
		//
		// So the room contributes its COLOUR and nothing else. ToonRoomLight
		// is flat across the model -- how bright and what colour it is in here,
		// with no direction in it -- and it tints the shadow band alone. The
		// lit band stays the artwork's own colour whatever the room is doing,
		// which is what a cel is: one flat tone for the light side, one for the
		// dark, and the dark one painted to match the background.
		vec3 L = u_toonLightDir.w != 0.0 ? u_toonLightDir.xyz : ToonKeyDir();
		vec3 cel = ToonRamp(max(0.0, dot(N, -L)));

		float dark = 1.0 - max(cel.r, max(cel.g, cel.b));
		vec3 band = cel*mix(vec3(1.0), ToonRoomLight(), dark);

		// How deep the shadow band goes. 0 leaves a character flat and fully
		// lit, 1 is the ramp at its full depth.
		color.rgb = mix(vec3(1.0), band, toonStrength);
	}else{
		color.rgb = v_color.rgb;
		color.rgb += u_ambLight.rgb*surfAmbient;
		color.rgb += DoDynamicLightPP(N)*surfDiffuse;
		color.rgb = clamp(color.rgb, 0.0, 1.0);
	}

	color.a = clamp(color.a, 0.0, 1.0);
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

	color.rgb = ToonSaturate(color.rgb);

	color.rgb = mix(u_fogColor.rgb, color.rgb, v_fog);
	DoAlphaTest(color.a);
	FRAGCOLOR(color);
}
