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
float4 outlineFlags : register(c235);   // x upper flat, y lower flat, z min width
#endif

// Where the camera is, in world space. The pixel shader wants the vector from
// the surface to the eye and there is no view matrix in here to recover it from
// -- combinedMat has already swallowed the projection.
float4 toonCamPos : register(c236);


float4x3 boneMatrices[64] : register(c41);

struct VS_in
{
	float4 Position		: POSITION;
	float3 Normal		: NORMAL;
	float2 TexCoord		: TEXCOORD0;
	float4 Prelight		: COLOR0;
	float4 Weights		: BLENDWEIGHT;
	int4 Indices		: BLENDINDICES;
};

struct VS_out {
	float4 Position		: POSITION;
	float3 TexCoord0	: TEXCOORD0;	// also fog
	float4 Color		: COLOR0;
#ifdef PERPIXEL
	// The skinned normal, in world space and not normalized. See the same
	// member in default_VS.hlsl; both feed default_PS.hlsl's PERPIXEL build,
	// so the three structs have to agree.
	float3 Normal		: TEXCOORD1;
	// And the vector to the eye, for the toon pixel shader. As in
	// default_VS.hlsl -- the toon path runs with this vertex shader.
	float3 ViewDir		: TEXCOORD2;
#endif
#ifdef OUTLINE
	// Which ink this vertex is drawn in, and how to read it. TEXCOORD1 as
	// well: the hull never carries a normal, so the two never coexist.
	float4 Outline		: TEXCOORD1;
#endif
};


VS_out main(in VS_in input)
{
	VS_out output;

	int j;
	float4 Local = input.Position;

#ifdef OUTLINE
	// The region is a fact about the model, so it is decided on the bind pose
	// -- testing the posed height moves the boundary every time he lifts a
	// leg.
	output.Outline = input.Position.y < outlineColor2.a
	               ? float4(outlineColor2.rgb, outlineFlags.y)
	               : float4(outlineColor.rgb, outlineFlags.x);
#endif

	float3 SkinVertex = float3(0.0, 0.0, 0.0);
	float3 SkinNormal = float3(0.0, 0.0, 0.0);
	for(j = 0; j < 4; j++){
		SkinVertex += mul(Local, boneMatrices[input.Indices[j]]).xyz * input.Weights[j];
		SkinNormal += mul(input.Normal, (float3x3)boneMatrices[input.Indices[j]]).xyz * input.Weights[j];
	}

#ifdef OUTLINE
	// **Inflated after the skinning, and it is the same answer as before.**
	//
	// Skinning is linear, so blending the bones over (P + t*N) gives exactly
	// blend(P) + t*blend(N) -- which is SkinVertex and SkinNormal, both
	// already in hand. The hull still follows every animation; nothing drifts.
	//
	// It has to happen here now because the width has a floor in screen units
	// and that floor needs the depth of the POSED vertex, which does not exist
	// until the bones have been applied. outlineFlags.z is the floor already
	// divided through by the camera and the render height -- see
	// default_VS.hlsl, which says the whole of it.
	//
	// A fixed world width falls below a pixel somewhere down the level and the
	// character stops being inked, which is the one thing an animated drawing
	// never does.
	float outlineW = mul(combinedMat, float4(SkinVertex, 1.0)).w;
	float thickness = max(outlineColor.a,
	                      outlineFlags.z*max(outlineW, 1e-4));

	SkinVertex += SkinNormal*thickness;
#endif

	output.Position = mul(combinedMat, float4(SkinVertex, 1.0));
	float3 Vertex = mul(worldMat, float4(SkinVertex, 1.0)).xyz;
	float3 Normal = mul(normalMat, SkinNormal);

	output.TexCoord0.xy = input.TexCoord;

	output.Color = input.Prelight;

#ifdef PERPIXEL
	// As in default_VS.hlsl: the lighting moves to the pixel shader whole,
	// clamp and material colour with it.
	output.Normal = Normal;
	output.ViewDir = toonCamPos.xyz - Vertex;
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
