VSIN(ATTRIB_POS)	vec3 in_pos;

#ifdef UVXFORM
// The texture coordinate transform, as the two rows of a 2x4 matrix that
// multiplies (u, v, 1, 1). rwrender.h says what the two constant columns mean
// and why there are two of them.
uniform vec4 u_uvXform[2];
#endif

VSOUT vec4 v_color;
VSOUT vec2 v_tex0;
VSOUT float v_fog;

void
main(void)
{
	vec4 Vertex = u_world * vec4(in_pos, 1.0);
	gl_Position = u_proj * u_view * Vertex;
	vec3 Normal = mat3(u_normal) * in_normal;

#ifdef UVXFORM
	vec4 uv = vec4(in_tex0, 1.0, 1.0);
	v_tex0 = vec2(dot(u_uvXform[0], uv), dot(u_uvXform[1], uv));
#else
	v_tex0 = in_tex0;
#endif

	v_color = in_color;
	v_color.rgb += u_ambLight.rgb*surfAmbient;
	v_color.rgb += DoDynamicLight(Vertex.xyz, Normal)*surfDiffuse;
	v_color = clamp(v_color, 0.0, 1.0);
	v_color *= u_matColor;

	v_fog = DoFog(gl_Position.w);
}
