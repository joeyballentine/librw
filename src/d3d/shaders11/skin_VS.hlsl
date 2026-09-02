// The bone matrices, 64 of them at three registers each, from c41 to c232.
#define VSTAIL float4x3 boneMatrices[64] : packoffset(c41);

#include "standardConstants.h"

struct VS_in
{
	float4 Position		: POSITION;
	float3 Normal		: NORMAL;
	float2 TexCoord		: TEXCOORD0;
	float4 Prelight		: COLOR0;
	float4 Weights		: BLENDWEIGHT;
	uint4 Indices		: BLENDINDICES;
};

struct VS_out {
	float4 Position		: SV_POSITION;
	float3 TexCoord0	: TEXCOORD0;	// also fog
	float4 Color		: COLOR0;
#ifdef PERPIXEL
	// The skinned normal, in world space and not normalized. See the same
	// member in default_VS.hlsl; both feed default_PS.hlsl's PERPIXEL build,
	// so the three structs have to agree.
	float3 Normal		: TEXCOORD1;
#endif
};


VS_out main(in VS_in input)
{
	VS_out output;

	int j;
	float3 SkinVertex = float3(0.0, 0.0, 0.0);
	float3 SkinNormal = float3(0.0, 0.0, 0.0);
	for(j = 0; j < 4; j++){
		SkinVertex += mul(input.Position, boneMatrices[input.Indices[j]]).xyz * input.Weights[j];
		SkinNormal += mul(input.Normal, (float3x3)boneMatrices[input.Indices[j]]).xyz * input.Weights[j];
	}

	output.Position = mul(combinedMat, float4(SkinVertex, 1.0));
	float3 Vertex = mul(worldMat, float4(SkinVertex, 1.0)).xyz;
	float3 Normal = mul(normalMat, SkinNormal);

	output.TexCoord0.xy = input.TexCoord;

	output.Color = input.Prelight;

#ifdef PERPIXEL
	// As in default_VS.hlsl: the lighting moves to the pixel shader whole,
	// clamp and material colour with it.
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
