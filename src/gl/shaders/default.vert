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
// World space, and NOT normalized: interpolating two unit normals across a
// triangle does not give a unit normal, which is why simple.frag normalizes it
// again. skin.vert declares the same output for the same fragment shader.
VSOUT vec3 v_normal;
#else
// How squarely this vertex faces the light, for the shadow test. Only where
// there is no normal going across anyway -- the per-pixel build works the same
// number out from v_normal, and more accurately.
VSOUT float v_shadowNdl;
#endif

void
main(void)
{
	vec4 Vertex = u_world * vec4(in_pos, 1.0);
	vec3 Normal = mat3(u_normal) * in_normal;

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
	v_outline = in_pos.y < u_outlineColor2.a ? u_outlineColor2 : u_outlineColor;
#endif

	gl_Position = u_proj * u_view * Vertex;

#ifdef UVXFORM
	vec4 uv = vec4(in_tex0, 1.0, 1.0);
	v_tex0 = vec2(dot(u_uvXform[0], uv), dot(u_uvXform[1], uv));
#else
	v_tex0 = in_tex0;
#endif

	v_color = in_color;
#ifdef PERPIXEL
	// Everything from the ambient term down happens in the fragment shader
	// instead, the clamp and the material colour with it -- they come after the
	// lighting and cannot be split from it. The prelight goes across untouched.
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
