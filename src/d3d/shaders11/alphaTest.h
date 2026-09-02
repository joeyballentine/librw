// The pixel stage's constant registers, and the alpha test.
//
// D3D9 compared a fragment's alpha against a reference in the output merger,
// and the driver set D3DRS_ALPHAFUNC to say how. D3D11 has no such stage, so
// every pixel shader clips instead, against the same two numbers the render
// states carry. setAlphaTestConstants uploads them.
//
// The register numbers are the PSLOC_ ones in rwd3d.h; c1 to c3 are scratch for
// whoever is drawing, which is why the alpha test sits at c7.

cbuffer PSConstants : register(b0)
{
	float4	fogColor	: packoffset(c0);
	float4	fxparams	: packoffset(c1);
	float4	alphaTest	: packoffset(c7);
	float4	ppMatColor	: packoffset(c8);
	float4	ppSurfProps	: packoffset(c9);
	float4	ppAmbient	: packoffset(c10);
	float4	ppLightColor[8]	: packoffset(c11);
	float4	ppLightDir[8]	: packoffset(c19);
};

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
