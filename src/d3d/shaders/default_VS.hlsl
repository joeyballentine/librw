#include "standardConstants.h"

#ifdef OUTLINE
// The two inks, and where they meet.
//
// c233 and up. Every pipeline puts its own constants at c41 -- the uvXform
// pair, the matfx texture matrix -- and the skin pipeline's 64 bone matrices
// run from c41 all the way to c232, so this is the first register no pipeline
// has already claimed.
float4 outlineColor : register(c233);   // rgb ink or scale, a thickness
float4 outlineColor2 : register(c234);  // rgb ink or scale, a split height
float4 outlineFlags : register(c235);   // x upper flat, y lower flat
#endif


#ifdef UVXFORM
// The texture coordinate transform, as the two rows of a 2x4 matrix that
// multiplies (u, v, 1, 1). rwrender.h says what the two constant columns mean
// and why there are two of them.
//
// c41 is where every pipeline puts its own constants -- the skin pipeline's
// bone matrices and the matfx pipeline's texture matrix are both there too --
// because each one uploads what it needs immediately before it draws. Sharing
// the register is safe for exactly that reason and would not be otherwise.
float4		uvXform0	: register(c41);
float4		uvXform1	: register(c42);
#endif

struct VS_in
{
	float4 Position		: POSITION;
	float3 Normal		: NORMAL;
	float2 TexCoord		: TEXCOORD0;
	float4 Prelight		: COLOR0;
};

struct VS_out {
	float4 Position		: POSITION;
	float3 TexCoord0	: TEXCOORD0;	// also fog
	float4 Color		: COLOR0;
#ifdef OUTLINE
	// Which ink this vertex is drawn in, and how to read it. Interpolated
	// rather than flat: the regions are split by height and the boundary runs
	// through the middle of triangles, so a blend over a few pixels beats a
	// step mid-face.
	float4 Outline		: TEXCOORD1;
#endif
#ifdef PERPIXEL
	// World space, and NOT normalized: interpolating two unit normals across a
	// triangle does not give a unit normal, which is the whole reason the pixel
	// shader normalizes it again. Must match default_PS.hlsl's VS_out.
	float3 Normal		: TEXCOORD1;
#endif
};


VS_out main(in VS_in input)
{
	VS_out output;

	float4 Local = input.Position;
	float3 Normal = mul(normalMat, input.Normal);

#ifdef OUTLINE
	// Push the surface out along its own normal before projecting: what is
	// left of an inflated copy once its front faces are culled is a band
	// around the silhouette. Object space, so the bones below still move it.
	Local.xyz += normalize(input.Normal)*outlineColor.a;

	output.Outline = input.Position.y < outlineColor2.a
	               ? float4(outlineColor2.rgb, outlineFlags.y)
	               : float4(outlineColor.rgb, outlineFlags.x);
#endif

	output.Position = mul(combinedMat, Local);
	float3 Vertex = mul(worldMat, Local).xyz;

#ifdef UVXFORM
	float4 uv = float4(input.TexCoord, 1.0, 1.0);
	output.TexCoord0.xy = float2(dot(uvXform0, uv), dot(uvXform1, uv));
#else
	output.TexCoord0.xy = input.TexCoord;
#endif

	output.Color = input.Prelight;

#ifdef PERPIXEL
	// Everything from the ambient term down happens in the pixel shader
	// instead, including the clamp and the material colour -- they come after
	// the lighting and so cannot be split from it. The prelight goes across
	// untouched.
	output.Normal = Normal;
#else
	output.Color.rgb += ambientLight.rgb * surfAmbient;

	int i;
#ifdef DIRECTIONALS
	for(i = 0; i < numDirLights; i++)
		output.Color.xyz += DoDirLight(lights[i+firstDirLight], Normal)*surfDiffuse;
#endif
#ifdef POINTLIGHTS
	for(i = 0; i < numPointLights; i++)
		output.Color.xyz += DoPointLight(lights[i+firstPointLight], Vertex.xyz, Normal)*surfDiffuse;
#endif
#ifdef SPOTLIGHTS
	for(i = 0; i < numSpotLights; i++)
		output.Color.xyz += DoSpotLight(lights[i+firstSpotLight], Vertex.xyz, Normal)*surfDiffuse;
#endif
	// PS2 clamps before material color
	output.Color = clamp(output.Color, 0.0, 1.0);
	output.Color *= matCol;
#endif

	output.TexCoord0.z = clamp((output.Position.w - fogEnd)*fogRange, fogDisable, 1.0);

	return output;
}
