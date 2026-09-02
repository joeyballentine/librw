namespace rw {
namespace d3d {

#ifdef RW_D3D11

#ifdef __d3d11_h__

extern ID3D11Device *d3d11device;
extern ID3D11DeviceContext *d3d11context;

struct DisplayMode
{
	DXGI_MODE_DESC mode;
	uint32 flags;
};

struct D3d11Globals
{
	HWND window;

	IDXGIFactory1 *factory;
	int numAdapters;
	int adapter;
	D3D_FEATURE_LEVEL featureLevel;

	DisplayMode *modes;
	int numModes;
	int currentMode;
	DisplayMode startMode;

	uint32 msLevel;

	IDXGISwapChain *swapChain;
	// The back buffer's view, and the depth buffer created alongside it. Both
	// are remade when the window changes size.
	ID3D11RenderTargetView *backBufferTarget;
	ID3D11Texture2D *depthBuffer;
	ID3D11DepthStencilView *depthBufferView;
	int32 backBufferWidth;
	int32 backBufferHeight;

	int numTextures;
	int numVertexShaders;
	int numPixelShaders;
	int numVertexBuffers;
	int numIndexBuffers;
	int numVertexDeclarations;
};

extern D3d11Globals d3d11Globals;

DXGI_FORMAT formatToDXGI(uint32 format);

#endif

// The raster layer, in d3d11raster.cpp. The system-memory copy and the lock
// against it are d3d.cpp's non-D3D9 arms, unchanged; these are the GPU side.
Raster *rasterCreateCameraTexture(Raster *raster);
Raster *rasterCreateCamera(Raster *raster);
Raster *rasterCreateZbuffer(Raster *raster);
// Send the system copy to the GPU if it has been written since last time.
void rasterUpload(Raster *raster);
// The view to bind, uploading first if needed. nil when there is nothing to
// sample -- an unallocated raster, or a format with no D3D11 equivalent.
void *rasterShaderResource(Raster *raster);
uint8 *rasterLockTarget(Raster *raster, int32 level, int32 lockMode);
void rasterUnlockTarget(Raster *raster);
void rasterDestroy(Raster *raster, D3dRaster *natras);

#endif

}
}
