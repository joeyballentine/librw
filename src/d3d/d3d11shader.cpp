#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define WITH_D3D
#include "../rwbase.h"
#include "../rwplg.h"
#include "../rwerror.h"
#include "../rwrender.h"
#include "../rwengine.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "rwd3d.h"
#include "rwd3d9.h"
#include "rwd3d11.h"
#include "rwd3dimpl.h"

#define PLUGIN_ID 0

namespace rw {
namespace d3d {

#ifdef RW_D3D11

// Shader constants, kept as D3D9's constant register file.
//
// Every VSLOC_/PSLOC_ in this driver is a D3D9 c-register number and every
// shader is written against those numbers, so the register file is what the
// D3D11 backend keeps too: one array of float4 per stage, uploaded as a
// constant buffer when it has changed. The shaders declare their constants with
// packoffset at the same register, which is how HLSL spells a c-register inside
// a cbuffer.
// Up to c238: skin_matfx_env_VS.hlsl puts 64 bone matrices at c41 and the
// environment map constants after them.
#define NUMVSCONST 256
#define NUMPSCONST 32
#define NUMVSINT 4

static float vsConstants[NUMVSCONST*4];
static float psConstants[NUMPSCONST*4];
static int32 vsIntConstants[NUMVSINT*4];

static bool32 vsConstantsDirty = 1;
static bool32 psConstantsDirty = 1;
static bool32 vsIntConstantsDirty = 1;

static ID3D11Buffer *vsConstantBuffer;
static ID3D11Buffer *psConstantBuffer;
static ID3D11Buffer *vsIntConstantBuffer;

// The pixel stage's alpha test, which D3D11 has no render state for. Kept at
// the top of the pixel register file, out of the way of the transient
// scratch registers c1..c3 that the port's own passes use.
#define PSLOC_alphaTest 7

static ID3D11Buffer*
createConstantBuffer(uint32 bytes)
{
	D3D11_BUFFER_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.ByteWidth = bytes;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	ID3D11Buffer *buf = nil;
	d3d11device->CreateBuffer(&desc, nil, &buf);
	return buf;
}

void
openShaderConstants(void)
{
	vsConstantBuffer = createConstantBuffer(sizeof(vsConstants));
	psConstantBuffer = createConstantBuffer(sizeof(psConstants));
	vsIntConstantBuffer = createConstantBuffer(sizeof(vsIntConstants));
	vsConstantsDirty = psConstantsDirty = vsIntConstantsDirty = 1;
}

void
closeShaderConstants(void)
{
	if(vsConstantBuffer){ vsConstantBuffer->Release(); vsConstantBuffer = nil; }
	if(psConstantBuffer){ psConstantBuffer->Release(); psConstantBuffer = nil; }
	if(vsIntConstantBuffer){ vsIntConstantBuffer->Release(); vsIntConstantBuffer = nil; }
}

static void
upload(ID3D11Buffer *buf, const void *data, uint32 bytes)
{
	if(buf == nil)
		return;
	D3D11_MAPPED_SUBRESOURCE map;
	if(SUCCEEDED(d3d11context->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))){
		memcpy(map.pData, data, bytes);
		d3d11context->Unmap(buf, 0);
	}
}

void
uploadShaderConstants(void)
{
	if(vsConstantsDirty){
		vsConstantsDirty = 0;
		upload(vsConstantBuffer, vsConstants, sizeof(vsConstants));
	}
	if(vsIntConstantsDirty){
		vsIntConstantsDirty = 0;
		upload(vsIntConstantBuffer, vsIntConstants, sizeof(vsIntConstants));
	}
	if(psConstantsDirty){
		psConstantsDirty = 0;
		upload(psConstantBuffer, psConstants, sizeof(psConstants));
	}
	ID3D11Buffer *vsbufs[2] = { vsConstantBuffer, vsIntConstantBuffer };
	d3d11context->VSSetConstantBuffers(0, 2, vsbufs);
	d3d11context->PSSetConstantBuffers(0, 1, &psConstantBuffer);
}

void
setVertexShaderConstantF(uint32 reg, const float32 *data, int32 numRegs)
{
	if(reg + numRegs > NUMVSCONST)
		return;
	memcpy(vsConstants + reg*4, data, numRegs*4*sizeof(float));
	vsConstantsDirty = 1;
}

void
setVertexShaderConstantI(uint32 reg, const int32 *data, int32 numRegs)
{
	if(reg + numRegs > NUMVSINT)
		return;
	memcpy(vsIntConstants + reg*4, data, numRegs*4*sizeof(int32));
	vsIntConstantsDirty = 1;
}

void
setPixelShaderConstantF(uint32 reg, const float32 *data, int32 numRegs)
{
	if(reg + numRegs > NUMPSCONST)
		return;
	memcpy(psConstants + reg*4, data, numRegs*4*sizeof(float));
	psConstantsDirty = 1;
}

// The alpha test, as the pixel shader sees it: the comparison as a number and
// the reference in 0..1. ALPHAALWAYS means no clip at all.
void
setAlphaTestConstants(uint32 func, uint32 ref)
{
	float c[4];
	c[0] = (float)func;
	c[1] = ref/255.0f;
	c[2] = 0.0f;
	c[3] = 0.0f;
	setPixelShaderConstantF(PSLOC_alphaTest, c, 1);
}

// --- shaders ----------------------------------------------------------------

// A vertex shader keeps its bytecode: D3D11 validates an input layout against
// the shader signature it will be drawn with, so the blob has to outlive
// creation.
struct VertexShader
{
	ID3D11VertexShader *shader;
	uint8 *code;
	uint32 codeSize;
};

// The compiled blobs the make_*.cmd scripts emit are DWORD arrays with no
// length beside them. A DXBC container carries its own total size at offset 24,
// which is where the length comes from.
static uint32
blobSize(const void *code)
{
	return ((const uint32*)code)[6];
}

void*
createVertexShader(void *csosrc)
{
	uint32 size = blobSize(csosrc);
	VertexShader *vs = rwNewT(VertexShader, 1, MEMDUR_EVENT | ID_DRIVER);
	memset(vs, 0, sizeof(*vs));
	if(FAILED(d3d11device->CreateVertexShader(csosrc, size, nil, &vs->shader))){
		rwFree(vs);
		return nil;
	}
	vs->code = rwNewT(uint8, size, MEMDUR_EVENT | ID_DRIVER);
	memcpy(vs->code, csosrc, size);
	vs->codeSize = size;
	d3d11Globals.numVertexShaders++;
	return vs;
}

void*
createPixelShader(void *csosrc)
{
	ID3D11PixelShader *ps = nil;
	if(FAILED(d3d11device->CreatePixelShader(csosrc, blobSize(csosrc), nil, &ps)))
		return nil;
	d3d11Globals.numPixelShaders++;
	return ps;
}

void
destroyVertexShader(void *shader)
{
	VertexShader *vs = (VertexShader*)shader;
	if(vs == nil)
		return;
	if(vs->shader)
		vs->shader->Release();
	rwFree(vs->code);
	rwFree(vs);
	d3d11Globals.numVertexShaders--;
}

void
destroyPixelShader(void *shader)
{
	if(shader){
		((ID3D11PixelShader*)shader)->Release();
		d3d11Globals.numPixelShaders--;
	}
}

// --- input layouts ----------------------------------------------------------

// A D3D9 vertex declaration is a description on its own; a D3D11 input layout
// is a description validated against one shader's input signature. So the
// declaration stays what the pipelines build and pass around -- d3d9.cpp's
// non-D3D9 arm already keeps it as a plain array -- and the layout is made when
// a declaration and a shader are first drawn together.
struct InputLayout
{
	void *declaration;
	void *vertexShader;
	ID3D11InputLayout *layout;
};

#define MAXLAYOUTS 128
static InputLayout layouts[MAXLAYOUTS];
static int32 numLayouts;

static const char*
declUsageName(uint32 usage)
{
	switch(usage){
	case D3DDECLUSAGE_POSITION:	return "POSITION";
	case D3DDECLUSAGE_BLENDWEIGHT:	return "BLENDWEIGHT";
	case D3DDECLUSAGE_BLENDINDICES:	return "BLENDINDICES";
	case D3DDECLUSAGE_NORMAL:	return "NORMAL";
	case D3DDECLUSAGE_PSIZE:	return "PSIZE";
	case D3DDECLUSAGE_TEXCOORD:	return "TEXCOORD";
	case D3DDECLUSAGE_TANGENT:	return "TANGENT";
	case D3DDECLUSAGE_BINORMAL:	return "BINORMAL";
	case D3DDECLUSAGE_COLOR:	return "COLOR";
	}
	return "TEXCOORD";
}

static DXGI_FORMAT
declTypeFormat(uint32 type)
{
	switch(type){
	case D3DDECLTYPE_FLOAT1:	return DXGI_FORMAT_R32_FLOAT;
	case D3DDECLTYPE_FLOAT2:	return DXGI_FORMAT_R32G32_FLOAT;
	case D3DDECLTYPE_FLOAT3:	return DXGI_FORMAT_R32G32B32_FLOAT;
	case D3DDECLTYPE_FLOAT4:	return DXGI_FORMAT_R32G32B32A32_FLOAT;
	// D3DCOLOR is BGRA bytes; D3D11 has the swizzle in the format.
	case D3DDECLTYPE_D3DCOLOR:	return DXGI_FORMAT_B8G8R8A8_UNORM;
	case D3DDECLTYPE_UBYTE4:	return DXGI_FORMAT_R8G8B8A8_UINT;
	case D3DDECLTYPE_UBYTE4N:	return DXGI_FORMAT_R8G8B8A8_UNORM;
	case D3DDECLTYPE_SHORT2:	return DXGI_FORMAT_R16G16_SINT;
	case D3DDECLTYPE_SHORT4:	return DXGI_FORMAT_R16G16B16A16_SINT;
	case D3DDECLTYPE_SHORT2N:	return DXGI_FORMAT_R16G16_SNORM;
	case D3DDECLTYPE_SHORT4N:	return DXGI_FORMAT_R16G16B16A16_SNORM;
	case D3DDECLTYPE_USHORT2N:	return DXGI_FORMAT_R16G16_UNORM;
	case D3DDECLTYPE_USHORT4N:	return DXGI_FORMAT_R16G16B16A16_UNORM;
	case D3DDECLTYPE_FLOAT16_2:	return DXGI_FORMAT_R16G16_FLOAT;
	case D3DDECLTYPE_FLOAT16_4:	return DXGI_FORMAT_R16G16B16A16_FLOAT;
	}
	return DXGI_FORMAT_UNKNOWN;
}

ID3D11InputLayout*
inputLayoutFor(void *declaration, void *vertexShader)
{
	if(declaration == nil || vertexShader == nil)
		return nil;

	for(int32 i = 0; i < numLayouts; i++)
		if(layouts[i].declaration == declaration && layouts[i].vertexShader == vertexShader)
			return layouts[i].layout;

	d3d9::VertexElement *elements = (d3d9::VertexElement*)declaration;
	D3D11_INPUT_ELEMENT_DESC desc[16];
	int32 n = 0;
	for(; elements[n].stream != 0xFF && n < 16; n++){
		desc[n].SemanticName = declUsageName(elements[n].usage);
		desc[n].SemanticIndex = elements[n].usageIndex;
		desc[n].Format = declTypeFormat(elements[n].type);
		desc[n].InputSlot = elements[n].stream;
		desc[n].AlignedByteOffset = elements[n].offset;
		desc[n].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		desc[n].InstanceDataStepRate = 0;
	}

	VertexShader *vs = (VertexShader*)vertexShader;
	ID3D11InputLayout *layout = nil;
	if(FAILED(d3d11device->CreateInputLayout(desc, n, vs->code, vs->codeSize, &layout)))
		return nil;

	if(numLayouts < MAXLAYOUTS){
		layouts[numLayouts].declaration = declaration;
		layouts[numLayouts].vertexShader = vertexShader;
		layouts[numLayouts].layout = layout;
		numLayouts++;
	}
	return layout;
}

// A declaration is freed when the geometry that owns it is; anything made
// against it has to go with it.
void
forgetInputLayouts(void *declaration)
{
	for(int32 i = 0; i < numLayouts; ){
		if(layouts[i].declaration == declaration){
			layouts[i].layout->Release();
			layouts[i] = layouts[--numLayouts];
		}else
			i++;
	}
}

void
releaseInputLayouts(void)
{
	for(int32 i = 0; i < numLayouts; i++)
		layouts[i].layout->Release();
	numLayouts = 0;
}

ID3D11VertexShader*
vertexShaderResource(void *shader)
{
	return shader ? ((VertexShader*)shader)->shader : nil;
}

#endif
}
}
