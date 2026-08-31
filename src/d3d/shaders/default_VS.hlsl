#include "standardConstants.h"

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

	output.Position = mul(combinedMat, input.Position);
	float3 Vertex = mul(worldMat, input.Position).xyz;
	float3 Normal = mul(normalMat, input.Normal);

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
