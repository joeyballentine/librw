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

D3d11Globals d3d11Globals;

ID3D11Device *d3d11device;
ID3D11DeviceContext *d3d11context;

// The fixed-size screen, as in the D3D9 backend. Declared here so that the
// application's calls resolve; nothing renders into it yet.
int32 virtualScreenWidth;
int32 virtualScreenHeight;
static int32 virtualScreenSamples = 1;

static void forgetBindings(void);

// What OMSetRenderTargets was last given. Kept so a clear lands on the camera's
// own surfaces rather than on the window's.
static ID3D11RenderTargetView *currentTarget;
static ID3D11DepthStencilView *currentDepth;

void
getScreenExtent(int32 *width, int32 *height)
{
	if(virtualScreenWidth && virtualScreenHeight){
		*width = virtualScreenWidth;
		*height = virtualScreenHeight;
		return;
	}
	RECT rect;
	GetClientRect(d3d11Globals.window, &rect);
	*width = rect.right;
	*height = rect.bottom;
}

void
setVirtualScreen(int32 width, int32 height)
{
	virtualScreenWidth = width;
	virtualScreenHeight = height;
}

void
getVirtualScreen(int32 *width, int32 *height)
{
	*width = virtualScreenWidth;
	*height = virtualScreenHeight;
}

void setVirtualScreenSamples(int32 samples) { virtualScreenSamples = samples < 1 ? 1 : samples; }
int32 getVirtualScreenSamples(void) { return 1; }

// --- the swap chain ---------------------------------------------------------

static void
releaseBackBuffer(void)
{
	if(d3d11Globals.depthBufferView){
		d3d11Globals.depthBufferView->Release();
		d3d11Globals.depthBufferView = nil;
	}
	if(d3d11Globals.depthBuffer){
		d3d11Globals.depthBuffer->Release();
		d3d11Globals.depthBuffer = nil;
	}
	if(d3d11Globals.backBufferTarget){
		d3d11Globals.backBufferTarget->Release();
		d3d11Globals.backBufferTarget = nil;
	}
}

// Make the views over whatever the swap chain's buffers currently are. Called
// after creating the chain and again after every resize.
static bool32
acquireBackBuffer(void)
{
	ID3D11Texture2D *backBuffer = nil;
	if(FAILED(d3d11Globals.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer)))
		return 0;

	D3D11_TEXTURE2D_DESC desc;
	backBuffer->GetDesc(&desc);
	d3d11Globals.backBufferWidth = desc.Width;
	d3d11Globals.backBufferHeight = desc.Height;

	HRESULT hr = d3d11device->CreateRenderTargetView(backBuffer, nil, &d3d11Globals.backBufferTarget);
	backBuffer->Release();
	if(FAILED(hr))
		return 0;

	D3D11_TEXTURE2D_DESC dd;
	memset(&dd, 0, sizeof(dd));
	dd.Width = desc.Width;
	dd.Height = desc.Height;
	dd.MipLevels = 1;
	dd.ArraySize = 1;
	dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dd.SampleDesc = desc.SampleDesc;
	dd.Usage = D3D11_USAGE_DEFAULT;
	dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	if(FAILED(d3d11device->CreateTexture2D(&dd, nil, &d3d11Globals.depthBuffer)))
		return 0;
	if(FAILED(d3d11device->CreateDepthStencilView(d3d11Globals.depthBuffer, nil, &d3d11Globals.depthBufferView)))
		return 0;
	return 1;
}

// Follow the window. The swap chain does not resize itself, and a chain whose
// buffers are a different size than the window is stretched by the presenter
// rather than redrawn -- which reads as a blurry frame, not as an error.
static void
resizeToWindow(void)
{
	RECT rect;
	GetClientRect(d3d11Globals.window, &rect);
	if(rect.right == 0 || rect.bottom == 0)
		return;
	if(rect.right == d3d11Globals.backBufferWidth &&
	   rect.bottom == d3d11Globals.backBufferHeight)
		return;

	d3d11context->OMSetRenderTargets(0, nil, nil);
	currentTarget = nil;
	currentDepth = nil;
	releaseBackBuffer();
	d3d11Globals.swapChain->ResizeBuffers(0, rect.right, rect.bottom, DXGI_FORMAT_UNKNOWN, 0);
	acquireBackBuffer();
}

// --- video modes ------------------------------------------------------------

static void
makeVideoModeList(void)
{
	IDXGIAdapter1 *adapter = nil;
	IDXGIOutput *output = nil;
	UINT num = 0;

	rwFree(d3d11Globals.modes);
	d3d11Globals.modes = nil;
	d3d11Globals.numModes = 0;

	if(FAILED(d3d11Globals.factory->EnumAdapters1(d3d11Globals.adapter, &adapter)))
		return;
	if(FAILED(adapter->EnumOutputs(0, &output))){
		adapter->Release();
		return;
	}

	// The windowed mode goes in first, as entry zero, so that an application
	// that never picks one gets a window rather than a display mode change.
	output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, 0, &num, nil);
	d3d11Globals.modes = rwNewT(DisplayMode, num+1, MEMDUR_EVENT | ID_DRIVER);
	memset(d3d11Globals.modes, 0, sizeof(DisplayMode)*(num+1));

	RECT rect;
	GetClientRect(d3d11Globals.window, &rect);
	d3d11Globals.modes[0].mode.Width = rect.right;
	d3d11Globals.modes[0].mode.Height = rect.bottom;
	d3d11Globals.modes[0].mode.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	d3d11Globals.modes[0].flags = 0;

	if(num){
		DXGI_MODE_DESC *descs = rwNewT(DXGI_MODE_DESC, num, MEMDUR_FUNCTION | ID_DRIVER);
		output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, 0, &num, descs);
		for(UINT i = 0; i < num; i++){
			d3d11Globals.modes[i+1].mode = descs[i];
			d3d11Globals.modes[i+1].flags = VIDEOMODEEXCLUSIVE;
		}
		rwFree(descs);
	}
	d3d11Globals.numModes = num+1;
	d3d11Globals.currentMode = 0;
	d3d11Globals.startMode = d3d11Globals.modes[0];

	output->Release();
	adapter->Release();
}

// --- open, start, stop, close -----------------------------------------------

static int
openD3D11(EngineOpenParams *params)
{
	d3d11Globals.window = params->window;
	d3d11Globals.adapter = 0;

	if(FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&d3d11Globals.factory))){
		RWERROR((ERR_GENERAL, "CreateDXGIFactory1 failed"));
		return 0;
	}

	IDXGIAdapter1 *adapter = nil;
	d3d11Globals.numAdapters = 0;
	while(d3d11Globals.factory->EnumAdapters1(d3d11Globals.numAdapters, &adapter) != DXGI_ERROR_NOT_FOUND){
		adapter->Release();
		d3d11Globals.numAdapters++;
	}
	makeVideoModeList();
	return 1;
}

static int
closeD3D11(void)
{
	rwFree(d3d11Globals.modes);
	d3d11Globals.modes = nil;
	d3d11Globals.numModes = 0;
	if(d3d11Globals.factory){
		d3d11Globals.factory->Release();
		d3d11Globals.factory = nil;
	}
	return 1;
}

static int
startD3D11(void)
{
	DXGI_SWAP_CHAIN_DESC scd;
	DisplayMode *mode = &d3d11Globals.modes[d3d11Globals.currentMode];

	memset(&scd, 0, sizeof(scd));
	scd.BufferCount = 2;
	scd.BufferDesc = mode->mode;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.OutputWindow = d3d11Globals.window;
	scd.SampleDesc.Count = 1;
	scd.SampleDesc.Quality = 0;
	scd.Windowed = !(mode->flags & VIDEOMODEEXCLUSIVE);
	scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT flags = 0;
#ifdef DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	static const D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};

	HRESULT hr = D3D11CreateDeviceAndSwapChain(nil, D3D_DRIVER_TYPE_HARDWARE, nil, flags,
		levels, sizeof(levels)/sizeof(levels[0]), D3D11_SDK_VERSION,
		&scd, &d3d11Globals.swapChain, &d3d11device, &d3d11Globals.featureLevel, &d3d11context);
	if(FAILED(hr)){
		RWERROR((ERR_GENERAL, "D3D11CreateDeviceAndSwapChain failed"));
		return 0;
	}

	// The presenter's own alt-enter would swap the chain to a display mode
	// behind the engine's back. Video modes are the engine's to set.
	d3d11Globals.factory->MakeWindowAssociation(d3d11Globals.window, DXGI_MWA_NO_ALT_ENTER);

	if(!acquireBackBuffer()){
		RWERROR((ERR_GENERAL, "could not view the back buffer"));
		return 0;
	}
	return 1;
}

static int
stopD3D11(void)
{
	releaseBackBuffer();
	currentTarget = nil;
	currentDepth = nil;
	if(d3d11Globals.swapChain){
		d3d11Globals.swapChain->SetFullscreenState(FALSE, nil);
		d3d11Globals.swapChain->Release();
		d3d11Globals.swapChain = nil;
	}
	if(d3d11context){
		d3d11context->ClearState();
		d3d11context->Release();
		d3d11context = nil;
	}
	if(d3d11device){
		d3d11device->Release();
		d3d11device = nil;
	}
	return 1;
}

static int
initD3D11(void)
{
	// D3D11 has no paletted texture format at all, so librw expands a palette
	// into true colour before a raster is created rather than after.
	isP8supported = 0;

	forgetBindings();
	resetRenderState();
	openShaderConstants();
	openIm2D();
	openIm3D();
	return 1;
}
static int
termD3D11(void)
{
	closeIm3D();
	closeIm2D();
	closeShaderConstants();
	releaseInputLayouts();
	releaseStateObjects();
	forgetBindings();
	return stopD3D11();
}
static int finalizeD3D11(void) { return 1; }

// --- the camera -------------------------------------------------------------

static void
setRenderSurfaces(Camera *cam)
{
	ID3D11RenderTargetView *rtv = d3d11Globals.backBufferTarget;
	ID3D11DepthStencilView *dsv = d3d11Globals.depthBufferView;
	int32 width = d3d11Globals.backBufferWidth;
	int32 height = d3d11Globals.backBufferHeight;

	if(cam->frameBuffer){
		D3dRaster *natras = GETD3DRASTEREXT(cam->frameBuffer);
		if(natras->rtv){
			rtv = (ID3D11RenderTargetView*)natras->rtv;
			width = cam->frameBuffer->width;
			height = cam->frameBuffer->height;
		}
	}
	if(cam->zBuffer){
		D3dRaster *natras = GETD3DRASTEREXT(cam->zBuffer);
		if(natras->dsv)
			dsv = (ID3D11DepthStencilView*)natras->dsv;
	}

	currentTarget = rtv;
	currentDepth = dsv;
	d3d11context->OMSetRenderTargets(1, &rtv, dsv);

	D3D11_VIEWPORT vp;
	vp.TopLeftX = 0.0f;
	vp.TopLeftY = 0.0f;
	vp.Width = (float)width;
	vp.Height = (float)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	d3d11context->RSSetViewports(1, &vp);
}

// The same view and projection as the D3D9 backend builds. D3D11 keeps D3D9's
// clip space -- left handed, z in [0,1] -- so the matrices are unchanged.
static void
beginUpdate(Camera *cam)
{
	float view[16], proj[16];

	Matrix inv;
	Matrix::invert(&inv, cam->getFrame()->getLTM());
	view[0]  = -inv.right.x;
	view[1]  =  inv.right.y;
	view[2]  =  inv.right.z;
	view[3]  =  0.0f;
	view[4]  = -inv.up.x;
	view[5]  =  inv.up.y;
	view[6]  =  inv.up.z;
	view[7]  =  0.0f;
	view[8]  = -inv.at.x;
	view[9]  =  inv.at.y;
	view[10] =  inv.at.z;
	view[11] =  0.0f;
	view[12] = -inv.pos.x;
	view[13] =  inv.pos.y;
	view[14] =  inv.pos.z;
	view[15] =  1.0f;
	memcpy(&cam->devView, view, sizeof(RawMatrix));

	float32 invwx = 1.0f/cam->viewWindow.x;
	float32 invwy = 1.0f/cam->viewWindow.y;
	float32 invz = 1.0f/(cam->farPlane-cam->nearPlane);

	proj[0] = invwx;
	proj[1] = 0.0f;
	proj[2] = 0.0f;
	proj[3] = 0.0f;

	proj[4] = 0.0f;
	proj[5] = invwy;
	proj[6] = 0.0f;
	proj[7] = 0.0f;

	proj[8] = cam->viewOffset.x*invwx;
	proj[9] = cam->viewOffset.y*invwy;
	proj[12] = -proj[8];
	proj[13] = -proj[9];
	if(cam->projection == Camera::PERSPECTIVE){
		proj[10] = cam->farPlane*invz;
		proj[11] = 1.0f;
		proj[15] = 0.0f;
	}else{
		proj[10] = invz;
		proj[11] = 0.0f;
		proj[15] = 1.0f;
	}
	proj[14] = -cam->nearPlane*proj[10];
	memcpy(&cam->devProj, proj, sizeof(RawMatrix));

	setRenderSurfaces(cam);
}

static void
endUpdate(Camera *cam)
{
}

static void
clearCamera(Camera *cam, RGBA *col, uint32 mode)
{
	// Not only for beginUpdate's sake: a clear before the scene opens is how
	// the game wipes a camera texture it is about to render into.
	setRenderSurfaces(cam);
	if(currentTarget == nil)
		return;

	if(mode & Camera::CLEARIMAGE){
		float c[4];
		c[0] = col->red/255.0f;
		c[1] = col->green/255.0f;
		c[2] = col->blue/255.0f;
		c[3] = col->alpha/255.0f;
		d3d11context->ClearRenderTargetView(currentTarget, c);
	}
	uint32 depthFlags = 0;
	if(mode & Camera::CLEARZ)
		depthFlags |= D3D11_CLEAR_DEPTH;
	if(mode & Camera::CLEARSTENCIL)
		depthFlags |= D3D11_CLEAR_STENCIL;
	if(depthFlags && currentDepth)
		d3d11context->ClearDepthStencilView(currentDepth, depthFlags, 1.0f, 0);
}

// One place where D3D11 is simply better: the presentation interval is an
// argument to Present, so asking for vsync between one frame and the next costs
// nothing. Under D3D9 it lives in the present parameters and changing it resets
// the device.
static void
showRaster(Raster *raster, uint32 flags)
{
	d3d11Globals.swapChain->Present(flags & Raster::FLIPWAITVSYNCH ? 1 : 0, 0);
}

static bool32
rasterRenderFast(Raster *raster, int32 x, int32 y)
{
	return 0;
}

// --- what is bound to draw with ---------------------------------------------

// The input assembler and the shader stages. An input layout depends on both
// the declaration and the vertex shader, so the two are remembered and the
// layout is settled at draw time rather than when either is set.
static struct {
	void *vertexShader;
	void *pixelShader;
	void *declaration;
	void *indexBuffer;
	struct {
		void *buffer;
		uint32 offset;
		uint32 stride;
	} streams[2];
	ID3D11InputLayout *layout;
	uint32 primType;
} bound;

void
setVertexShader(void *vs)
{
	if(bound.vertexShader != vs){
		bound.vertexShader = vs;
		bound.layout = nil;
		d3d11context->VSSetShader(vertexShaderResource(vs), nil, 0);
	}
}

void
setPixelShader(void *ps)
{
	if(bound.pixelShader != ps){
		bound.pixelShader = ps;
		d3d11context->PSSetShader((ID3D11PixelShader*)ps, nil, 0);
	}
}

void
setVertexDeclaration(void *declaration)
{
	if(bound.declaration != declaration){
		bound.declaration = declaration;
		bound.layout = nil;
	}
}

void
setIndices(void *indexBuffer)
{
	if(bound.indexBuffer != indexBuffer){
		bound.indexBuffer = indexBuffer;
		d3d11context->IASetIndexBuffer(bufferResource(indexBuffer), DXGI_FORMAT_R16_UINT, 0);
	}
}

void
setStreamSource(int n, void *buffer, uint32 offset, uint32 stride)
{
	if(n < 0 || n > 1)
		return;
	if(bound.streams[n].buffer == buffer &&
	   bound.streams[n].offset == offset &&
	   bound.streams[n].stride == stride)
		return;
	bound.streams[n].buffer = buffer;
	bound.streams[n].offset = offset;
	bound.streams[n].stride = stride;

	ID3D11Buffer *buf = bufferResource(buffer);
	UINT strides = stride, offsets = offset;
	d3d11context->IASetVertexBuffers(n, 1, &buf, &strides, &offsets);
}

// D3D9 counts primitives, D3D11 counts vertices, and the two disagree by the
// primitive's shape rather than by a factor.
static uint32
indexCount(uint32 primType, uint32 numPrimitives)
{
	switch(primType){
	case D3DPT_LINELIST:		return numPrimitives*2;
	case D3DPT_LINESTRIP:		return numPrimitives+1;
	case D3DPT_TRIANGLELIST:	return numPrimitives*3;
	case D3DPT_TRIANGLESTRIP:	return numPrimitives+2;
	case D3DPT_POINTLIST:		return numPrimitives;
	}
	return 0;
}

static D3D11_PRIMITIVE_TOPOLOGY
topology(uint32 primType)
{
	switch(primType){
	case D3DPT_POINTLIST:		return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	case D3DPT_LINELIST:		return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	case D3DPT_LINESTRIP:		return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case D3DPT_TRIANGLELIST:	return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case D3DPT_TRIANGLESTRIP:	return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	}
	// D3D11 dropped the triangle fan. Nothing in this driver emits one --
	// im2DRenderPrimitive is the only caller that could, and the game's 2D
	// passes are lists and strips.
	return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

// Settle what depends on more than one binding: the input layout on the
// declaration and the vertex shader, the topology on the draw.
static bool32
readyToDraw(uint32 primType)
{
	D3D11_PRIMITIVE_TOPOLOGY topo = topology(primType);
	if(topo == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
		return 0;
	if(bound.primType != primType){
		bound.primType = primType;
		d3d11context->IASetPrimitiveTopology(topo);
	}
	if(bound.layout == nil){
		bound.layout = inputLayoutFor(bound.declaration, bound.vertexShader);
		if(bound.layout == nil)
			return 0;
		d3d11context->IASetInputLayout(bound.layout);
	}
	return 1;
}

void
drawPrimitive(uint32 primType, uint32 startVertex, uint32 numPrimitives)
{
	if(numPrimitives == 0 || !readyToDraw(primType))
		return;
	d3d11context->Draw(indexCount(primType, numPrimitives), startVertex);
}

void
drawIndexedPrimitive(uint32 primType, int32 baseVertex, uint32 minVertex,
	uint32 numVertices, uint32 startIndex, uint32 numPrimitives)
{
	(void)minVertex;
	(void)numVertices;
	if(numPrimitives == 0 || !readyToDraw(primType))
		return;
	d3d11context->DrawIndexed(indexCount(primType, numPrimitives), startIndex, baseVertex);
}

// Nothing the context holds survives a device teardown, and the shadow above
// would otherwise claim it did.
static void
forgetBindings(void)
{
	memset(&bound, 0, sizeof(bound));
	bound.primType = 0xFFFFFFFF;
}

// --- the device interface ---------------------------------------------------

static int
deviceSystem(DeviceReq req, void *arg, int32 n)
{
	VideoMode *rwmode;
	IDXGIAdapter1 *adapter;
	DXGI_ADAPTER_DESC1 desc;

	switch(req){
	case DEVICEOPEN:
		return openD3D11((EngineOpenParams*)arg);
	case DEVICECLOSE:
		return closeD3D11();

	case DEVICEINIT:
		return startD3D11() && initD3D11();
	case DEVICETERM:
		return termD3D11();

	case DEVICEFINALIZE:
		return finalizeD3D11();

	case DEVICEGETNUMSUBSYSTEMS:
		return d3d11Globals.numAdapters;

	case DEVICEGETCURRENTSUBSYSTEM:
		return d3d11Globals.adapter;

	case DEVICESETSUBSYSTEM:
		if(n >= d3d11Globals.numAdapters)
			return 0;
		d3d11Globals.adapter = n;
		makeVideoModeList();
		return 1;

	case DEVICEGETSUBSSYSTEMINFO:
		if(n >= d3d11Globals.numAdapters)
			return 0;
		if(FAILED(d3d11Globals.factory->EnumAdapters1(n, &adapter)))
			return 0;
		adapter->GetDesc1(&desc);
		adapter->Release();
		WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1,
			((SubSystemInfo*)arg)->name, sizeof(SubSystemInfo::name), nil, nil);
		return 1;

	case DEVICEGETNUMVIDEOMODES:
		return d3d11Globals.numModes;

	case DEVICEGETCURRENTVIDEOMODE:
		return d3d11Globals.currentMode;

	case DEVICESETVIDEOMODE:
		if(n >= d3d11Globals.numModes)
			return 0;
		d3d11Globals.currentMode = n;
		return 1;

	case DEVICEGETVIDEOMODEINFO:
		rwmode = (VideoMode*)arg;
		rwmode->width = d3d11Globals.modes[n].mode.Width;
		rwmode->height = d3d11Globals.modes[n].mode.Height;
		rwmode->depth = 32;
		rwmode->flags = d3d11Globals.modes[n].flags;
		return 1;

	case DEVICEGETMAXMULTISAMPLINGLEVELS:
		return 1;
	case DEVICEGETMULTISAMPLINGLEVELS:
		return d3d11Globals.msLevel == 0 ? 1 : d3d11Globals.msLevel;
	case DEVICESETMULTISAMPLINGLEVELS:
		d3d11Globals.msLevel = (uint32)n;
		return 1;
	}
	return 1;
}

Device renderdevice = {
	0.0f, 1.0f,
	d3d::beginUpdate,
	d3d::endUpdate,
	d3d::clearCamera,
	d3d::showRaster,
	d3d::rasterRenderFast,
	d3d::setRwRenderState,
	d3d::getRwRenderState,
	d3d::im2DRenderLine,
	d3d::im2DRenderTriangle,
	d3d::im2DRenderPrimitive,
	d3d::im2DRenderIndexedPrimitive,
	d3d::im3DTransform,
	d3d::im3DRenderPrimitive,
	d3d::im3DRenderIndexedPrimitive,
	d3d::im3DEnd,
	d3d::deviceSystem,
};

#endif
}
}
