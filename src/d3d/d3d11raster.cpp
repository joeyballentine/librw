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
#include "rwd3d11.h"
#include "rwd3dimpl.h"

#define PLUGIN_ID 0

namespace rw {
namespace d3d {

#ifdef RW_D3D11

// D3DFMT_ is what the whole d3d driver speaks -- the native texture reader
// writes it into D3dRaster::format and the xbox converter hands it out -- so it
// stays the currency here and is translated at the one point a D3D11 resource
// is made.
DXGI_FORMAT
formatToDXGI(uint32 format)
{
	switch(format){
	case D3DFMT_A8R8G8B8:	return DXGI_FORMAT_B8G8R8A8_UNORM;
	case D3DFMT_X8R8G8B8:	return DXGI_FORMAT_B8G8R8X8_UNORM;
	case D3DFMT_A8B8G8R8:	return DXGI_FORMAT_R8G8B8A8_UNORM;
	case D3DFMT_R5G6B5:	return DXGI_FORMAT_B5G6R5_UNORM;
	// D3D11 has no X1R5G5B5. The alpha bit is present either way and the
	// pixel shader is what decides whether to read it, which is the same
	// bargain the D3D9 backend makes for a 555 raster.
	case D3DFMT_X1R5G5B5:	return DXGI_FORMAT_B5G5R5A1_UNORM;
	case D3DFMT_A1R5G5B5:	return DXGI_FORMAT_B5G5R5A1_UNORM;
	case D3DFMT_A4R4G4B4:	return DXGI_FORMAT_B4G4R4A4_UNORM;
	case D3DFMT_A8:		return DXGI_FORMAT_A8_UNORM;
	case D3DFMT_L8:		return DXGI_FORMAT_R8_UNORM;
	case D3DFMT_DXT1:	return DXGI_FORMAT_BC1_UNORM;
	case D3DFMT_DXT2:
	case D3DFMT_DXT3:	return DXGI_FORMAT_BC2_UNORM;
	case D3DFMT_DXT4:
	case D3DFMT_DXT5:	return DXGI_FORMAT_BC3_UNORM;
	case D3DFMT_D16:	return DXGI_FORMAT_D16_UNORM;
	case D3DFMT_D24S8:
	case D3DFMT_D24X8:	return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case D3DFMT_D32:	return DXGI_FORMAT_D32_FLOAT;
	}
	return DXGI_FORMAT_UNKNOWN;
}

static bool
isBlockCompressed(uint32 format)
{
	return format == D3DFMT_DXT1 || format == D3DFMT_DXT2 || format == D3DFMT_DXT3 ||
	       format == D3DFMT_DXT4 || format == D3DFMT_DXT5;
}

// Bytes from the start of one row of the source image to the start of the next.
// Taken from the level's own size rather than recomputed from the format: for a
// block format a "row" is a row of 4x4 blocks, and createTexture already did
// that arithmetic when it laid the level out.
static uint32
levelRowPitch(uint32 format, RasterLevels::Level *level)
{
	int32 rows = isBlockCompressed(format) ? (level->height + 3)/4 : level->height;
	if(rows <= 0)
		rows = 1;
	return (uint32)(level->size / rows);
}

// --- textures ---------------------------------------------------------------

// Make the GPU texture and its view from the system copy. Deferred to the first
// upload rather than done in rasterCreate, because a raster's texels arrive
// after it exists and a texture with no data in it is nothing to make early.
static bool32
createTextureResource(Raster *raster, D3dRaster *natras)
{
	RasterLevels *levels = (RasterLevels*)natras->texture;
	DXGI_FORMAT fmt = formatToDXGI(natras->format);
	if(fmt == DXGI_FORMAT_UNKNOWN || levels == nil)
		return 0;

	D3D11_TEXTURE2D_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.Width = levels->levels[0].width;
	desc.Height = levels->levels[0].height;
	desc.MipLevels = levels->numlevels;
	desc.ArraySize = 1;
	desc.Format = fmt;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	ID3D11Texture2D *tex = nil;
	if(FAILED(d3d11device->CreateTexture2D(&desc, nil, &tex)))
		return 0;
	ID3D11ShaderResourceView *srv = nil;
	if(FAILED(d3d11device->CreateShaderResourceView(tex, nil, &srv))){
		tex->Release();
		return 0;
	}
	natras->tex11 = tex;
	natras->srv = srv;
	d3d11Globals.numTextures++;
	return 1;
}

// Push the system copy at the GPU. Cheap when nothing has changed, which is the
// common case: the game writes a raster's texels once at load.
void
rasterUpload(Raster *raster)
{
	D3dRaster *natras = GETD3DRASTEREXT(raster);
	if(!natras->dirty)
		return;
	natras->dirty = 0;

	if(natras->tex11 == nil && !createTextureResource(raster, natras))
		return;

	RasterLevels *levels = (RasterLevels*)natras->texture;
	for(int32 i = 0; i < levels->numlevels; i++)
		d3d11context->UpdateSubresource((ID3D11Texture2D*)natras->tex11, i, nil,
			levels->levels[i].data,
			levelRowPitch(natras->format, &levels->levels[i]), 0);
}

void*
rasterShaderResource(Raster *raster)
{
	D3dRaster *natras = GETD3DRASTEREXT(raster);
	rasterUpload(raster);
	return natras->srv;
}

// --- camera targets and z buffers -------------------------------------------

Raster*
rasterCreateCameraTexture(Raster *raster)
{
	D3dRaster *natras = GETD3DRASTEREXT(raster);
	DXGI_FORMAT fmt = formatToDXGI(natras->format);
	if(fmt == DXGI_FORMAT_UNKNOWN){
		RWERROR((ERR_NOTEXTURE));
		return nil;
	}

	D3D11_TEXTURE2D_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.Width = raster->width;
	desc.Height = raster->height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = fmt;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	ID3D11Texture2D *tex = nil;
	if(FAILED(d3d11device->CreateTexture2D(&desc, nil, &tex))){
		RWERROR((ERR_NOTEXTURE));
		return nil;
	}
	if(FAILED(d3d11device->CreateRenderTargetView(tex, nil, (ID3D11RenderTargetView**)&natras->rtv)) ||
	   FAILED(d3d11device->CreateShaderResourceView(tex, nil, (ID3D11ShaderResourceView**)&natras->srv))){
		tex->Release();
		RWERROR((ERR_NOTEXTURE));
		return nil;
	}
	natras->tex11 = tex;
	// No system copy: a camera texture is written by the GPU, and reading it
	// back goes through a staging texture rather than through this.
	natras->texture = nil;
	d3d11Globals.numTextures++;
	return raster;
}

Raster*
rasterCreateCamera(Raster *raster)
{
	D3dRaster *natras = GETD3DRASTEREXT(raster);
	natras->autogenMipmap = 0;
	natras->format = D3DFMT_A8R8G8B8;
	raster->depth = 32;
	// nil means the back buffer, the same convention the D3D9 backend uses.
	natras->texture = nil;
	return raster;
}

Raster*
rasterCreateZbuffer(Raster *raster)
{
	D3dRaster *natras = GETD3DRASTEREXT(raster);
	natras->autogenMipmap = 0;
	natras->format = D3DFMT_D24S8;
	raster->depth = 32;

	D3D11_TEXTURE2D_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.Width = raster->width;
	desc.Height = raster->height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	ID3D11Texture2D *tex = nil;
	if(FAILED(d3d11device->CreateTexture2D(&desc, nil, &tex))){
		RWERROR((ERR_NOTEXTURE));
		return nil;
	}
	if(FAILED(d3d11device->CreateDepthStencilView(tex, nil, (ID3D11DepthStencilView**)&natras->dsv))){
		tex->Release();
		RWERROR((ERR_NOTEXTURE));
		return nil;
	}
	natras->tex11 = tex;
	natras->texture = nil;
	return raster;
}

// --- reading a target back --------------------------------------------------

// The frame buffer and camera textures live only on the GPU, so a lock is a
// copy to a staging texture and a read out of that. The result is handed back
// in a system buffer the raster keeps, which is what raster->pixels points at
// until unlock.
uint8*
rasterLockTarget(Raster *raster, int32 level, int32 lockMode)
{
	D3dRaster *natras = GETD3DRASTEREXT(raster);

	if(lockMode & Raster::LOCKWRITE){
		assert(0 && "can't lock a render target for writing");
		return nil;
	}

	ID3D11Texture2D *src;
	if(raster->type == Raster::CAMERA){
		// The virtual screen is what the scene was drawn into; the back buffer
		// holds the previous frame's letterboxed blit, in another channel
		// order. Only with no virtual screen is the back buffer the picture.
		src = virtualScreenTexture();
		if(src)
			src->AddRef();
		else if(FAILED(d3d11Globals.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&src)))
			return nil;
	}else{
		src = (ID3D11Texture2D*)natras->tex11;
		if(src == nil)
			return nil;
		src->AddRef();
	}

	D3D11_TEXTURE2D_DESC desc;
	src->GetDesc(&desc);
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.MiscFlags = 0;

	ID3D11Texture2D *staging = nil;
	if(FAILED(d3d11device->CreateTexture2D(&desc, nil, &staging))){
		src->Release();
		return nil;
	}
	d3d11context->CopyResource(staging, src);
	src->Release();

	D3D11_MAPPED_SUBRESOURCE map;
	if(FAILED(d3d11context->Map(staging, 0, D3D11_MAP_READ, 0, &map))){
		staging->Release();
		return nil;
	}

	uint32 bpp = natras->bpp;
	uint8 *pixels = (uint8*)rwNew(desc.Width*desc.Height*bpp, MEMDUR_EVENT | ID_DRIVER);
	for(UINT y = 0; y < desc.Height; y++)
		memcpy(pixels + y*desc.Width*bpp, (uint8*)map.pData + y*map.RowPitch, desc.Width*bpp);
	d3d11context->Unmap(staging, 0);
	staging->Release();

	// Kept so unlock can free it; nothing else reads lockedSurf on this
	// backend.
	natras->lockedSurf = pixels;
	raster->pixels = pixels;
	raster->width = desc.Width;
	raster->height = desc.Height;
	raster->stride = desc.Width*bpp;
	return pixels;
}

void
rasterUnlockTarget(Raster *raster)
{
	D3dRaster *natras = GETD3DRASTEREXT(raster);
	rwFree(natras->lockedSurf);
	natras->lockedSurf = nil;
}

// --- teardown ---------------------------------------------------------------

void
rasterDestroy(Raster *raster, D3dRaster *natras)
{
	forgetRaster(raster);
	if(natras->srv){
		((ID3D11ShaderResourceView*)natras->srv)->Release();
		natras->srv = nil;
	}
	if(natras->rtv){
		((ID3D11RenderTargetView*)natras->rtv)->Release();
		natras->rtv = nil;
	}
	if(natras->dsv){
		((ID3D11DepthStencilView*)natras->dsv)->Release();
		natras->dsv = nil;
	}
	if(natras->tex11){
		((ID3D11Texture2D*)natras->tex11)->Release();
		natras->tex11 = nil;
		d3d11Globals.numTextures--;
	}
}

#endif
}
}
