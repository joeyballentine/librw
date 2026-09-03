// The D3D9 constant register file, as a constant buffer.
//
// Every VSLOC_ in the driver is a c-register number and the D3D11 backend keeps
// the register file for that reason -- see d3d11shader.cpp. HLSL spells a
// c-register inside a cbuffer as packoffset, so these declarations sit at
// exactly the registers rwd3d.h names and the upload path stays a memcpy.
//
// c41 upwards is each pipeline's own: the uv transform, the skin pipeline's
// bone matrices and the matfx pipeline's texture matrix all live there, because
// each uploads what it needs immediately before it draws. A cbuffer has to be
// declared in one piece, so a shader that wants those registers defines VSTAIL
// before including this and they land inside the same block.

#include "lighting.h"

#ifndef VSTAIL
#define VSTAIL
#endif

cbuffer VSConstants : register(b0)
{
	float4x4	combinedMat	: packoffset(c0);
	float4x4	worldMat	: packoffset(c4);
	float4x4	normalMat4	: packoffset(c8);
	float4		matCol		: packoffset(c12);
	float4		surfProps	: packoffset(c13);
	float4		fogData		: packoffset(c14);
	float4		ambientLight	: packoffset(c15);
	float4		firstLight	: packoffset(c16);
	Light		lights[8]	: packoffset(c17);
	VSTAIL
};

// The integer registers, which D3D9 kept in a file of their own. One count per
// register, because that is how setNumLights uploads them.
cbuffer VSIntConstants : register(b1)
{
	int4		dirLightCount	: packoffset(c0);
	int4		pointLightCount	: packoffset(c1);
	int4		spotLightCount	: packoffset(c2);
};

#define numDirLights (dirLightCount.x)
#define numPointLights (pointLightCount.x)
#define numSpotLights (spotLightCount.x)

// normalMat is a 3x3 in the shaders that use it and four registers on the wire,
// which is what the driver uploads.
#define normalMat ((float3x3)normalMat4)

#define surfAmbient (surfProps.x)
#define surfSpecular (surfProps.y)
#define surfDiffuse (surfProps.z)

#define fogStart (fogData.x)
#define fogEnd (fogData.y)
#define fogRange (fogData.z)
#define fogDisable (fogData.w)

#define firstDirLight ((int)firstLight.x)
#define firstPointLight ((int)firstLight.y)
#define firstSpotLight ((int)firstLight.z)
