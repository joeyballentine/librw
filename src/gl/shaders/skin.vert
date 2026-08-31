uniform mat4 u_boneMatrices[64];

VSIN(ATTRIB_POS)	vec3 in_pos;

VSOUT vec4 v_color;
VSOUT vec2 v_tex0;
VSOUT float v_fog;
#ifdef PERPIXEL
// The skinned normal, world space and not normalized. Same output as
// default.vert's; both feed simple.frag's PERPIXEL build.
VSOUT vec3 v_normal;
#endif

void
main(void)
{
	vec3 SkinVertex = vec3(0.0, 0.0, 0.0);
	vec3 SkinNormal = vec3(0.0, 0.0, 0.0);
	for(int i = 0; i < 4; i++){
		SkinVertex += (u_boneMatrices[int(in_indices[i])] * vec4(in_pos, 1.0)).xyz * in_weights[i];
		SkinNormal += (mat3(u_boneMatrices[int(in_indices[i])]) * in_normal) * in_weights[i];
	}

	vec4 Vertex = u_world * vec4(SkinVertex, 1.0);
	gl_Position = u_proj * u_view * Vertex;
	vec3 Normal = mat3(u_normal) * SkinNormal;

	v_tex0 = in_tex0;

	v_color = in_color;
#ifdef PERPIXEL
	// As in default.vert: the lighting moves to the fragment shader whole.
	v_normal = Normal;
#else
	v_color.rgb += u_ambLight.rgb*surfAmbient;
	v_color.rgb += DoDynamicLight(Vertex.xyz, Normal)*surfDiffuse;
	v_color = clamp(v_color, 0.0, 1.0);
	v_color *= u_matColor;
#endif

	v_fog = DoFog(gl_Position.w);
}
