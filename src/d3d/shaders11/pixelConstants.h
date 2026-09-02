// The pixel stage's constant register file, and the alpha test.
//
// The register numbers are the PSLOC_ ones in rwd3d.h and perPixelConstants.h.
// c1 to c3 are scratch for whoever is drawing -- the matfx shader's shininess
// is c1, the port's glow pass takes c1 to c3 -- which is why the alpha test
// sits at c7 and the per-pixel block starts at c8.
//
// One cbuffer for every pixel shader, because a D3D9 pixel shader constant was
// device state shared by all of them and the driver still uploads it that way.

cbuffer PSConstants : register(b0)
{
	float4	fogColor	: packoffset(c0);
	float4	fxparams	: packoffset(c1);
	float4	alphaTest	: packoffset(c7);
	float4	ppMatCol	: packoffset(c8);
	float4	ppSurfProps	: packoffset(c9);
	float4	ppAmbient	: packoffset(c10);
	// Colour and direction as two arrays rather than one array of the Light
	// struct: the per-pixel path only ever does directional lights, so a
	// light's position register would be uploaded and never read.
	float4	ppLightColor[8]	: packoffset(c11);
	float4	ppLightDir[8]	: packoffset(c19);
};

#define shininess (fxparams.x)
#define disableFBA (fxparams.y)

#define ppSurfAmbient (ppSurfProps.x)
#define ppSurfDiffuse (ppSurfProps.z)

// The alpha test, which D3D11 has no render state for.
//
// D3D9 compared a fragment's alpha against a reference in the output merger and
// the driver set D3DRS_ALPHAFUNC to say how. There is no such stage here, so
// every pixel shader clips instead, against the same two numbers the render
// states carry. setAlphaTestConstants uploads them.

// rw::AlphaTestFunc.
#define ALPHAALWAYS 0
#define ALPHAGREATEREQUAL 1
#define ALPHALESS 2

void DoAlphaTest(float alpha)
{
	int func = (int)alphaTest.x;
	if(func == ALPHAGREATEREQUAL)
		clip(alpha - alphaTest.y);
	else if(func == ALPHALESS)
		clip(alphaTest.y - alpha - 1.0/512.0);
}
