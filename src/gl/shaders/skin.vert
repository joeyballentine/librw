uniform mat4 u_boneMatrices[64];

VSIN(ATTRIB_POS)	vec3 in_pos;

VSOUT vec4 v_color;
VSOUT vec2 v_tex0;
VSOUT float v_fog;
// World position in the shadow map's space. Interpolated, then biased into
// texture coordinates by the fragment shader.
VSOUT vec4 v_shadowPos;
#ifdef OUTLINE
// Which ink this vertex is drawn in. Flat across the triangle would be truer to
// a pen, but the regions are split by height and the boundary runs through the
// middle of triangles, so an interpolated colour blends the two over a band of
// a few pixels instead of stepping mid-face.
VSOUT vec4 v_outline;
#endif
#ifdef PERPIXEL
// The skinned normal, world space and not normalized. Same output as
// default.vert's; both feed simple.frag's PERPIXEL build.
VSOUT vec3 v_normal;
#else
// How squarely this vertex faces the light, for the shadow test. As in
// default.vert, and for the same reason.
VSOUT float v_shadowNdl;
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
	vec3 Normal = mat3(u_normal) * SkinNormal;

#ifdef OUTLINE
	// Push the surface out along its own normal before projecting. The normal
	// has to be in hand first, which is why it is computed above the
	// projection here and below it in a stock librw.
	//
	// In world units, so the band is thicker up close and thinner far away --
	// which is what a drawn line does NOT do, but scaling by depth instead
	// makes distant characters look inked in marker.
	Vertex.xyz += normalize(Normal)*u_outlineColor.a;

	// Which of the two inks this vertex belongs to, decided here rather than
	// in a second pass over the whole model: a vertex shader can branch, and
	// the earlier GameCube version could not.
	v_outline = SkinVertex.y < u_outlineColor2.a ? u_outlineColor2 : u_outlineColor;
#endif

	gl_Position = u_proj * u_view * Vertex;

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
	v_shadowNdl = DoShadowNdl(Normal);
#endif

	v_shadowPos = u_shadowMatrix * Vertex;

	v_fog = DoFog(gl_Position.w);
}
