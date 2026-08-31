#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwengine.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwanim.h"
#include "../rwplugins.h"

#include "rwgl3.h"
#include "rwgl3shader.h"
#include "rwgl3plg.h"

#include "rwgl3impl.h"

// The pipeline RenderWare selects with rpSKINTYPEMATFX: bone skinning AND a
// material effect on the same atomic. Neither of the two pipelines it is made
// of can stand in for it -- the skin pipeline draws the deformed model with no
// effect, and the matfx pipeline draws the effect on an undeformed one -- and
// every environment-mapped character in the game needs both. The env map in
// particular is meaningless without the first: its texture coordinate comes
// off the vertex normal, so a normal still in its bind pose reflects whatever
// the model was authored facing rather than what it is facing now.
//
// It is deliberately assembled out of the other two rather than rewritten:
// instancing is skinInstanceCB unchanged, the env state is gl3matfx.cpp's
// uploadEnvMapState, the fragment shader is matfx's, and the fallback for a
// mesh with no effect on it is gl3skin.cpp's own programs. Only the env-mapped
// vertex shader is new, because only it has to do both jobs at once.

namespace rw {
namespace gl3 {

#ifdef RW_OPENGL

static Shader *skinEnvShader, *skinEnvShader_noAT;
static Shader *skinEnvShader_fullLight, *skinEnvShader_fullLight_noAT;

static void
skinMatfxRender_Default(InstanceDataHeader *header, InstanceData *inst, int32 vsBits, uint32 flags)
{
	Material *m = inst->material;

	// Exactly what skinRenderCB draws. A skinned atomic stays on this pipeline
	// once MatFX::enableEffects has put it here, so most of the meshes it sees
	// have no effect on them at all -- including every mesh of every frame in
	// which the game has turned the effects off again.
	setMaterial(flags, m->color, m->surfaceProps);

	setTexture(0, m->texture);

	setPipelineVertexAlpha(inst->vertexAlpha || m->color.alpha != 0xFF);

	if((vsBits & VSLIGHT_MASK) == 0){
		if(getAlphaTest())
			skinShader->use();
		else
			skinShader_noAT->use();
	}else{
		if(getAlphaTest())
			skinShader_fullLight->use();
		else
			skinShader_fullLight_noAT->use();
	}

	drawInst(header, inst);
}

static void
skinMatfxRender_EnvMap(InstanceDataHeader *header, InstanceData *inst, int32 vsBits, uint32 flags, MatFX::Env *env)
{
	Material *m = inst->material;

	if(env->tex == nil || env->coefficient == 0.0f){
		skinMatfxRender_Default(header, inst, vsBits, flags);
		return;
	}

	setTexture(0, m->texture);
	uploadEnvMapState(m, env);

	setMaterial(flags, m->color, m->surfaceProps);

	setPipelineVertexAlpha(1);
	rw::SetRenderState(SRCBLEND, BLENDONE);

	if((vsBits & VSLIGHT_MASK) == 0){
		if(getAlphaTest())
			skinEnvShader->use();
		else
			skinEnvShader_noAT->use();
	}else{
		if(getAlphaTest())
			skinEnvShader_fullLight->use();
		else
			skinEnvShader_fullLight_noAT->use();
	}

	drawInst(header, inst);

	rw::SetRenderState(SRCBLEND, BLENDSRCALPHA);
}

void
skinMatfxRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	uint32 flags = atomic->geometry->flags;
	setWorldMatrix(atomic->getFrame()->getLTM());
	int32 vsBits = lightingCB(atomic);

	setupVertexInput(header);

	uploadSkinMatrices(atomic);

	// Without normals there is nothing to reflect, so the env map cannot be
	// generated at all and the mesh falls back to plain skinning.
	bool normals = !!(flags & Geometry::NORMALS);

	InstanceData *inst = header->inst;
	int32 n = header->numMeshes;

	while(n--){
		MatFX *matfx = MatFX::get(inst->material);

		if(matfx && matfx->type == MatFX::ENVMAP && normals)
			skinMatfxRender_EnvMap(header, inst, vsBits, flags, &matfx->fx[0].env);
		else
			// Every other effect is unsupported here for the same reason it is
			// unsupported in the unskinned matfx pipeline: no shader for it.
			skinMatfxRender_Default(header, inst, vsBits, flags);

		inst++;
	}
	teardownVertexInput(header);
}

void
createSkinMatFXShaders(void)
{
#include "shaders/matfx_gl.inc"
#include "shaders/skinmatfx_gl.inc"
	const char *vs[] = { shaderDecl, header_vert_src, skin_matfx_env_vert_src, nil };
	const char *vs_fullLight[] = { shaderDecl, "#define DIRECTIONALS\n#define POINTLIGHTS\n#define SPOTLIGHTS\n", header_vert_src, skin_matfx_env_vert_src, nil };
	const char *fs[] = { shaderDecl, header_frag_src, matfx_env_frag_src, nil };
	const char *fs_noAT[] = { shaderDecl, "#define NO_ALPHATEST\n", header_frag_src, matfx_env_frag_src, nil };

	skinEnvShader = Shader::create(vs, fs);
	assert(skinEnvShader);
	skinEnvShader_noAT = Shader::create(vs, fs_noAT);
	assert(skinEnvShader_noAT);

	skinEnvShader_fullLight = Shader::create(vs_fullLight, fs);
	assert(skinEnvShader_fullLight);
	skinEnvShader_fullLight_noAT = Shader::create(vs_fullLight, fs_noAT);
	assert(skinEnvShader_fullLight_noAT);
}

void
destroySkinMatFXShaders(void)
{
	skinEnvShader->destroy();
	skinEnvShader = nil;
	skinEnvShader_noAT->destroy();
	skinEnvShader_noAT = nil;
	skinEnvShader_fullLight->destroy();
	skinEnvShader_fullLight = nil;
	skinEnvShader_fullLight_noAT->destroy();
	skinEnvShader_fullLight_noAT = nil;
}

ObjPipeline*
makeSkinMatFXPipeline(void)
{
	ObjPipeline *pipe = ObjPipeline::create();
	// The instancing callback is the skin one, and that is not an economy: the
	// instanced vertex buffer is cached on the GEOMETRY and is never rebuilt
	// because the atomic changed pipeline, so a skinned atomic that can be on
	// either skinning pipeline has to lay its vertices out the same way on
	// both or the one that did not instance them reads bones that are not there.
	pipe->instanceCB = skinInstanceCB;
	pipe->uninstanceCB = nil;
	pipe->renderCB = skinMatfxRenderCB;
	// The plugin ID the pipeline is streamed out under. It is the skin plugin's
	// because this is a skinning pipeline; pluginData is the same 1 the plain
	// skinning pipeline writes, so a DFF written with an atomic on this reads
	// back as a skinned atomic on any RenderWare, which is what it is.
	pipe->pluginID = ID_SKIN;
	pipe->pluginData = 1;
	return pipe;
}

#endif

}
}
