uniform sampler2D tex0;

FSIN vec4 v_color;
FSIN vec2 v_tex0;
FSIN float v_fog;
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
	color.rgb = mix(u_fogColor.rgb, color.rgb, v_fog);
	DoAlphaTest(color.a);
	FRAGCOLOR(color);
}
