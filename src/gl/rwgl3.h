#ifdef RW_GL3
#include "glad/glad.h"
#ifdef LIBRW_SDL2
#include <SDL.h>
#elif defined(LIBRW_SDL3)
#include <SDL3/SDL.h>
#elif defined(LIBRW_GLFW)
#include <GLFW/glfw3.h>
#else
// sane fallback
#define LIBRW_GLFW
#include <GLFW/glfw3.h>
#endif
#endif

namespace rw {

#ifdef RW_GL3
struct EngineOpenParams
{
#ifdef LIBRW_SDL2
	SDL_Window **window;
	bool32 fullscreen;
#elif defined(LIBRW_SDL3)
	SDL_Window **window;
	bool32 fullscreen;
#elif defined(LIBRW_GLFW)
	GLFWwindow **window;
#else
    not implemented
#endif
	int width, height;
	const char *windowtitle;
};
#endif

namespace gl3 {

void registerPlatformPlugins(void);

extern Device renderdevice;

// arguments to glVertexAttribPointer basically
struct AttribDesc
{
	uint32 index;
	int32  type;
	bool32 normalized;
	int32  size;
	uint32 stride;
	uint32 offset;
};

enum AttribIndices
{
	ATTRIB_POS = 0,
	ATTRIB_NORMAL,
	ATTRIB_COLOR,
	ATTRIB_WEIGHTS,
	ATTRIB_INDICES,
	ATTRIB_TEXCOORDS0,
	ATTRIB_TEXCOORDS1,
	ATTRIB_TEXCOORDS2,
	ATTRIB_TEXCOORDS3,
	ATTRIB_TEXCOORDS4,
	ATTRIB_TEXCOORDS5,
	ATTRIB_TEXCOORDS6,
	ATTRIB_TEXCOORDS7,
};

// default uniform indices
// The colour the vertices of a non-PRELIT geometry are given, for a platform
// that has no per-vertex colour to read. Zero -- black -- is the default and
// means lighting is the only thing that can brighten such a model. An
// application whose art expects an unlit vertex to start white rather than
// black sets this before the engine is started.
extern bool32 constantVertexColorWhite;

extern int32 u_matColor;
extern int32 u_surfProps;

struct InstanceData
{
	uint32    numIndex;
	uint32    minVert;	// not used for rendering
	int32     numVertices;	//
	Material *material;
	bool32    vertexAlpha;
	uint32    program;
	uint32    offset;
};

struct InstanceDataHeader : rw::InstanceDataHeader
{
	uint32      serialNumber;
	uint32      numMeshes;
	uint16     *indexBuffer;
	uint32      primType;
	uint8      *vertexBuffer;
	int32       numAttribs;
	AttribDesc *attribDesc;
	uint32      totalNumIndex;
	uint32      totalNumVertex;

	uint32      ibo;
	uint32      vbo;		// or 2?
#ifdef RW_GL_USE_VAOS
	uint32      vao;
#endif

	InstanceData *inst;
};

#ifdef RW_GL3

struct Shader;

extern Shader *defaultShader, *defaultShader_noAT;
extern Shader *defaultShader_fullLight, *defaultShader_fullLight_noAT;
extern Shader *uvXformShader, *uvXformShader_noAT;
extern Shader *uvXformShader_fullLight, *uvXformShader_fullLight_noAT;
// The per-pixel lighting path. One pair each and not two, because these do no
// lighting in the vertex shader and so have nothing for DIRECTIONALS to switch
// on. Their fragment shader is not simple.frag as the others' is -- it is
// simple.frag with PERPIXEL, which needs lighting.frag ahead of it.
extern Shader *defaultShader_pp, *defaultShader_pp_noAT;
extern Shader *uvXformShader_pp, *uvXformShader_pp_noAT;

// Evaluate lighting per fragment rather than per vertex, in the default,
// uvxform and skin pipelines. Directional lights only: an atomic reached by a
// point or spot light keeps the per-vertex path for that draw, and one lit by
// ambient alone has nothing to gain. Safe to call before the device exists.
void setPerPixelLightingEnabled(bool32 enable);
bool32 getPerPixelLighting(void);
extern int32 u_uvXform;

struct Im3DVertex
{
	V3d     position;
	V3d     normal;		// librw extension
	uint8   r, g, b, a;
	float32 u, v;

	void setX(float32 x) { this->position.x = x; }
	void setY(float32 y) { this->position.y = y; }
	void setZ(float32 z) { this->position.z = z; }
	void setNormalX(float32 x) { this->normal.x = x; }
	void setNormalY(float32 y) { this->normal.y = y; }
	void setNormalZ(float32 z) { this->normal.z = z; }
	void setColor(uint8 r, uint8 g, uint8 b, uint8 a) {
		this->r = r; this->g = g; this->b = b; this->a = a; }
	void setU(float32 u) { this->u = u; }
	void setV(float32 v) { this->v = v; }

	float getX(void) { return this->position.x; }
	float getY(void) { return this->position.y; }
	float getZ(void) { return this->position.z; }
	float getNormalX(void) { return this->normal.x; }
	float getNormalY(void) { return this->normal.y; }
	float getNormalZ(void) { return this->normal.z; }
	RGBA getColor(void) { return makeRGBA(this->r, this->g, this->b, this->a); }
	float getU(void) { return this->u; }
	float getV(void) { return this->v; }
};
extern RGBA im3dMaterialColor;
extern SurfaceProperties im3dSurfaceProps;

struct Im2DVertex
{
	float32 x, y, z, w;
	uint8   r, g, b, a;
	float32 u, v;

	void setScreenX(float32 x) { this->x = x; }
	void setScreenY(float32 y) { this->y = y; }
	void setScreenZ(float32 z) { this->z = z; }
	// This is a bit unefficient but we have to counteract GL's divide, so multiply
	void setCameraZ(float32 z) { this->w = z; }
	void setRecipCameraZ(float32 recipz) { this->w = 1.0f/recipz; }
	void setColor(uint8 r, uint8 g, uint8 b, uint8 a) {
		this->r = r; this->g = g; this->b = b; this->a = a; }
	void setU(float32 u, float recipz) { this->u = u; }
	void setV(float32 v, float recipz) { this->v = v; }

	float getScreenX(void) { return this->x; }
	float getScreenY(void) { return this->y; }
	float getScreenZ(void) { return this->z; }
	float getCameraZ(void) { return this->w; }
	float getRecipCameraZ(void) { return 1.0f/this->w; }
	RGBA getColor(void) { return makeRGBA(this->r, this->g, this->b, this->a); }
	float getU(void) { return this->u; }
	float getV(void) { return this->v; }
};

void setAttribPointers(AttribDesc *attribDescs, int32 numAttribs);
void disableAttribPointers(AttribDesc *attribDescs, int32 numAttribs);
void setupVertexInput(InstanceDataHeader *header);
void teardownVertexInput(InstanceDataHeader *header);

// Render state

// Vertex shader bits
enum
{
	// These should be low so they could be used as indices
	VSLIGHT_DIRECT	= 1,
	VSLIGHT_POINT	= 2,
	VSLIGHT_SPOT	= 4,
	VSLIGHT_MASK	= 7,	// all the above
	// less critical
	VSLIGHT_AMBIENT = 8,
};

// A fixed-size screen, scaled onto the window at present time.
//
// Without one, a Raster::CAMERA IS the default framebuffer: the viewport
// follows the window and the camera's projection does not, so resizing the
// window stretches the picture. With one, the camera raster gets an FBO of its
// own at its own size, everything draws into that, and showRaster blits it into
// the window as the largest rectangle of its shape that fits -- centred, with
// the rest black. Same contract as the D3D9 device's setVirtualScreen.
//
// Call it before the game creates its camera raster, and never with a size that
// disagrees with the raster: the raster's own width and height are what the FBO
// is built at, so a mismatch would silently render at the wrong size rather
// than fail.
//
// Zero, the default, means no virtual screen and the behaviour above it.
void setVirtualScreen(int32 width, int32 height);
extern int32 virtualScreenWidth, virtualScreenHeight;

// The ONE framebuffer every Raster::CAMERA draws into when there is a virtual
// screen, and the texture behind it. Created on first use, because
// setVirtualScreen is called before there is a GL context to create it in.
// Zero when there is no virtual screen.
//
// Device-wide and shared, exactly as the D3D9 virtual screen is one surface
// that every camera raster points at rather than one per raster. An
// application makes several cameras -- a movie player, an offscreen instancer,
// a dummy for bucket sorting -- and they all draw onto the same picture.
// Copy the frame as it stands into a Raster::CAMERATEXTURE, which is how an
// effect gets the picture it is about to distort, blur or hold on screen. The
// source is the virtual screen, NOT whatever framebuffer happens to be bound:
// the last thing drawn before a present is often a camera texture -- a shadow
// buffer, say -- and reading that would capture the shadow buffer.
//
// False when there is no virtual screen to read, when the destination is not a
// camera texture, or when the two disagree about size. A caller that gets
// false has no picture and must fall back, not draw whatever was there before.
bool32 copyVirtualScreen(Raster *dst);

uint32 virtualScreenFramebuffer(void);
uint32 virtualScreenTexture(void);
// Where to read the frame, as against virtualScreenFramebuffer, which is where
// to draw it. They differ only when the scene is drawn multisampled.
uint32 virtualScreenResolvedFramebuffer(void);
void destroyVirtualScreen(void);

// How many samples the scene is drawn with. Set before the engine opens; the
// count the driver granted is what getVirtualScreenSamples answers, and it is
// 1 when there is no multisampling.
void setVirtualScreenSamples(int32 samples);
int32 getVirtualScreenSamples(void);
// Fold the samples into the single-sample picture every reader of the frame
// sees. Called for you by showRaster and by copyVirtualScreen.
void resolveVirtualScreen(void);

// Bracket a 2D primitive, which has no place in the depth buffer and whose
// edges are placed rather than found. See setIm2DActive's own comment.
void setIm2DActive(bool32 active);

extern const char *shaderDecl;	// #version stuff
extern const char *header_vert_src;
extern const char *header_frag_src;

extern Shader *im2dOverrideShader;

// per Scene
void setProjectionMatrix(float32*);
void setViewMatrix(float32*);

// per Object
void setWorldMatrix(Matrix*);
int32 setLights(WorldLights *lightData);

// per Mesh
void setTexture(int32 n, Texture *tex);
// Object pipelines announce what the instanced geometry needs here; it is
// combined with the application's own VERTEXALPHA request, never replaces it.
void setPipelineVertexAlpha(bool32 enable);
void setMaterial(const RGBA &color, const SurfaceProperties &surfaceprops, float extraSurfProp = 0.0f);
inline void setMaterial(uint32 flags, const RGBA &color, const SurfaceProperties &surfaceprops, float extraSurfProp = 0.0f)
{
	static RGBA white = { 255, 255, 255, 255 };
	if(flags & Geometry::MODULATE)
		setMaterial(color, surfaceprops, extraSurfProp);
	else
		setMaterial(white, surfaceprops, extraSurfProp);
}

void setAlphaBlend(bool32 enable);
bool32 getAlphaBlend(void);

bool32 getAlphaTest(void);

void bindFramebuffer(uint32 fbo);
uint32 bindTexture(uint32 texid);

void flushCache(void);

#endif

class ObjPipeline : public rw::ObjPipeline
{
public:
	void init(void);
	static ObjPipeline *create(void);

	void (*instanceCB)(Geometry *geo, InstanceDataHeader *header, bool32 reinstance);
	void (*uninstanceCB)(Geometry *geo, InstanceDataHeader *header);
	void (*renderCB)(Atomic *atomic, InstanceDataHeader *header);
};

void defaultInstanceCB(Geometry *geo, InstanceDataHeader *header, bool32 reinstance);
void defaultUninstanceCB(Geometry *geo, InstanceDataHeader *header);
void defaultRenderCB(Atomic *atomic, InstanceDataHeader *header);
void uvTransformRenderCB(Atomic *atomic, InstanceDataHeader *header);
int32 lightingCB(Atomic *atomic);
int32 lightingCB(void);

void drawInst_simple(InstanceDataHeader *header, InstanceData *inst);
// Emulate PS2 GS alpha test FB_ONLY case: failed alpha writes to frame- but not to depth buffer
void drawInst_GSemu(InstanceDataHeader *header, InstanceData *inst);
// This one switches between the above two depending on render state;
void drawInst(InstanceDataHeader *header, InstanceData *inst);


void *destroyNativeData(void *object, int32, int32);

ObjPipeline *makeDefaultPipeline(void);
ObjPipeline *makeUVTransformPipeline(void);

// Native Texture and Raster

struct Gl3Raster
{
	// arguments to glTexImage2D
	int32 internalFormat;
	int32 type;
	int32 format;
	int32 bpp;	// bytes per pixel
	// texture object
	uint32 texid;

	bool isCompressed;
	bool hasAlpha;
	// One of rw::AlphaKind: what the texels actually hold, as opposed to what
	// the pixel format has room for. ALPHAGRADED is the safe default -- it is
	// how every raster with an alpha channel behaved before this existed.
	uint8 alphaKind;
	bool autogenMipmap;
	int8 numLevels;
	// cached filtermode and addressing
	uint8 filterMode;
	uint8 addressU;
	uint8 addressV;
	int32 maxAnisotropy;

	uint32 fbo;		// used for camera texture only!
	Raster *fboMate;	// color or zbuffer raster mate of this one
	RasterLevels *backingStore;	// if we can't read back GPU memory but have to
};

struct Gl3Caps
{
	int gles;
	int glversion;
	bool dxtSupported;
	bool astcSupported;	// not used yet
	float maxAnisotropy;
};
extern Gl3Caps gl3Caps;
// GLES can't read back textures very nicely.
// In most cases that's not an issue, but when it is,
// this has to be set before the texture is filled:
extern bool32 needToReadBackTextures;

void allocateDXT(Raster *raster, int32 dxt, int32 numLevels, bool32 hasAlpha);
void setRasterAlphaKind(Raster *raster, int32 kind);

Texture *readNativeTexture(Stream *stream);
void writeNativeTexture(Texture *tex, Stream *stream);
uint32 getSizeNativeTexture(Texture *tex);

extern int32 nativeRasterOffset;
void registerNativeRaster(void);
#define GETGL3RASTEREXT(raster) PLUGINOFFSET(Gl3Raster, raster, rw::gl3::nativeRasterOffset)

}
}
