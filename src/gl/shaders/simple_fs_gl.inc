const char *simple_frag_src =
"uniform sampler2D tex0;\n"

"FSIN vec4 v_color;\n"
"FSIN vec2 v_tex0;\n"
"FSIN float v_fog;\n"
"#ifdef PERPIXEL\n"
"FSIN vec3 v_normal;\n"
"#endif\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 color = v_color;\n"

"#ifdef PERPIXEL\n"
"	// The vertex shader handed over the prelight and a normal and did nothing\n"
"	// else. What follows is default.vert's lighting, in the same order and with\n"
"	// the same clamp, evaluated here instead. lighting.frag declares the\n"
"	// uniforms it reads.\n"
"	vec3 N = normalize(v_normal);\n"
"	color.rgb += u_ambLight.rgb*surfAmbient;\n"
"	color.rgb += DoDynamicLightPP(N)*surfDiffuse;\n"
"	color = clamp(color, 0.0, 1.0);\n"
"	color *= u_matColor;\n"
"#endif\n"

"	color *= texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));\n"
"	color.rgb = mix(u_fogColor.rgb, color.rgb, v_fog);\n"
"	DoAlphaTest(color.a);\n"
"	FRAGCOLOR(color);\n"
"}\n"
;
