// The D3D9 constant register file, as a constant buffer.
//
// Every VSLOC_/PSLOC_ in the driver is a c-register number and the D3D11
// backend keeps the register file for that reason -- see d3d11shader.cpp. HLSL
// spells a c-register inside a cbuffer as packoffset, so these declarations sit
// at exactly the registers rwd3d.h names and the upload path is a memcpy.

cbuffer VSConstants : register(b0)
{
	float4x4	combinedMat	: packoffset(c0);
	float4x4	worldMat	: packoffset(c4);
	float4x4	normalMat4	: packoffset(c8);
	float4		matCol		: packoffset(c12);
	float4		surfProps	: packoffset(c13);
	float4		fogData		: packoffset(c14);
	float4		ambientLight	: packoffset(c15);
	int4		firstLight	: packoffset(c16);
	float4		lightData[24]	: packoffset(c17);
	float4		xform		: packoffset(c41);
};

cbuffer VSIntConstants : register(b1)
{
	int4		numLights	: packoffset(c0);
};

#define numDirLights (numLights.x)
#define numPointLights (numLights.y)
#define numSpotLights (numLights.z)

#define normalMat ((float3x3)normalMat4)

#define surfAmbient (surfProps.x)
#define surfSpecular (surfProps.y)
#define surfDiffuse (surfProps.z)

#define fogStart (fogData.x)
#define fogEnd (fogData.y)
#define fogRange (fogData.z)
#define fogDisable (fogData.w)

#define firstDirLight (firstLight.x)
#define firstPointLight (firstLight.y)
#define firstSpotLight (firstLight.z)

// One light is three registers, laid out by LightVS in d3drender.cpp.
#define lightColor(i) (lightData[(i)*3+0])
#define lightPosition(i) (lightData[(i)*3+1])
#define lightDirection(i) (lightData[(i)*3+2])
