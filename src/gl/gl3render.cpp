#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwengine.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#ifdef RW_OPENGL
#include "rwgl3.h"
#include "rwgl3shader.h"

#include "rwgl3impl.h"

namespace rw {
namespace gl3 {

#define MAX_LIGHTS 

void
drawInst_simple(InstanceDataHeader *header, InstanceData *inst)
{
	flushCache();
	glDrawElements(header->primType, inst->numIndex,
	               GL_UNSIGNED_SHORT, (void*)(uintptr)inst->offset);
}

// Emulate PS2 GS alpha test FB_ONLY case: failed alpha writes to frame- but not to depth buffer
void
drawInst_GSemu(InstanceDataHeader *header, InstanceData *inst)
{
	uint32 hasAlpha;
	int alphafunc, alpharef, gsalpharef;
	int zwrite;
	hasAlpha = getAlphaBlend();
	if(hasAlpha){
		zwrite = rw::GetRenderState(rw::ZWRITEENABLE);
		alphafunc = rw::GetRenderState(rw::ALPHATESTFUNC);
		if(zwrite){
			alpharef = rw::GetRenderState(rw::ALPHATESTREF);
			gsalpharef = rw::GetRenderState(rw::GSALPHATESTREF);

			SetRenderState(rw::ALPHATESTFUNC, rw::ALPHAGREATEREQUAL);
			SetRenderState(rw::ALPHATESTREF, gsalpharef);
			drawInst_simple(header, inst);
			SetRenderState(rw::ALPHATESTFUNC, rw::ALPHALESS);
			SetRenderState(rw::ZWRITEENABLE, 0);
			drawInst_simple(header, inst);
			SetRenderState(rw::ZWRITEENABLE, 1);
			SetRenderState(rw::ALPHATESTFUNC, alphafunc);
			SetRenderState(rw::ALPHATESTREF, alpharef);
		}else{
			SetRenderState(rw::ALPHATESTFUNC, rw::ALPHAALWAYS);
			drawInst_simple(header, inst);
			SetRenderState(rw::ALPHATESTFUNC, alphafunc);
		}
	}else
		drawInst_simple(header, inst);
}

void
drawInst(InstanceDataHeader *header, InstanceData *inst)
{
	if(rw::GetRenderState(rw::GSALPHATEST))
		drawInst_GSemu(header, inst);
	else
		drawInst_simple(header, inst);
}


void
setAttribPointers(AttribDesc *attribDescs, int32 numAttribs)
{
	AttribDesc *a;
	for(a = attribDescs; a != &attribDescs[numAttribs]; a++){
		glEnableVertexAttribArray(a->index);
		glVertexAttribPointer(a->index, a->size, a->type, a->normalized,
		                      a->stride, (void*)(uint64)a->offset);
	}
}

void
disableAttribPointers(AttribDesc *attribDescs, int32 numAttribs)
{
	AttribDesc *a;
	for(a = attribDescs; a != &attribDescs[numAttribs]; a++)
		glDisableVertexAttribArray(a->index);
}

void
setupVertexInput(InstanceDataHeader *header)
{
#ifdef RW_GL_USE_VAOS
	glBindVertexArray(header->vao);
#else
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, header->ibo);
	glBindBuffer(GL_ARRAY_BUFFER, header->vbo);
	setAttribPointers(header->attribDesc, header->numAttribs);
#endif
}

void
teardownVertexInput(InstanceDataHeader *header)
{
#ifndef RW_GL_USE_VAOS
	disableAttribPointers(header->attribDesc, header->numAttribs);
#endif
}

int32
lightingCB(Atomic *atomic)
{
	WorldLights lightData;
	Light *directionals[8];
	Light *locals[8];
	lightData.directionals = directionals;
	lightData.numDirectionals = 8;
	lightData.locals = locals;
	lightData.numLocals = 8;

	if(atomic->geometry->flags & rw::Geometry::LIGHT)
		((World*)engine->currentWorld)->enumerateLights(atomic, &lightData);
	else
		memset(&lightData, 0, sizeof(lightData));
	return setLights(&lightData);
}

int32
lightingCB(void)
{
	WorldLights lightData;
	Light *directionals[8];
	Light *locals[8];
	lightData.directionals = directionals;
	lightData.numDirectionals = 8;
	lightData.locals = locals;
	lightData.numLocals = 8;

	((World*)engine->currentWorld)->enumerateLights(&lightData);
	return setLights(&lightData);
}


// The default pipeline's render, and the UV-transforming one's. They differ by
// four shader programs and one uniform upload, so they are one function rather
// than a copy that will drift.
static void
renderCB(Atomic *atomic, InstanceDataHeader *header, bool32 uvXform)
{
	Material *m;

	uint32 flags = atomic->geometry->flags;
	setWorldMatrix(atomic->getFrame()->getLTM());
	int32 vsBits = lightingCB(atomic);

	setupVertexInput(header);

	// Uploaded per atomic and not cached, because the transform is state the
	// application changes between draws -- that is what makes a surface
	// animate -- so there is nothing here that stays the same long enough to
	// be worth comparing against.
	if(uvXform)
		setUniform(u_uvXform, uvTransform);

	InstanceData *inst = header->inst;
	int32 n = header->numMeshes;

	while(n--){
		m = inst->material;

		setMaterial(flags, m->color, m->surfaceProps);

		setTexture(0, m->texture);

		setPipelineVertexAlpha(inst->vertexAlpha || m->color.alpha != 0xFF);

		// Per-pixel lighting replaces exactly one of the light cases:
		// directional and nothing else. Ambient alone is the same colour at
		// every fragment and has nothing to gain, and the per-pixel fragment
		// shader does not do point or spot lights, so anything reached by one
		// keeps the per-vertex path.
		if((vsBits & VSLIGHT_MASK) == 0){
			if(getAlphaTest())
				(uvXform ? uvXformShader : defaultShader)->use();
			else
				(uvXform ? uvXformShader_noAT : defaultShader_noAT)->use();
		}else if(getPerPixelLighting() && (vsBits & VSLIGHT_MASK) == VSLIGHT_DIRECT){
			if(getAlphaTest())
				(uvXform ? uvXformShader_pp : defaultShader_pp)->use();
			else
				(uvXform ? uvXformShader_pp_noAT : defaultShader_pp_noAT)->use();
		}else{
			if(getAlphaTest())
				(uvXform ? uvXformShader_fullLight : defaultShader_fullLight)->use();
			else
				(uvXform ? uvXformShader_fullLight_noAT : defaultShader_fullLight_noAT)->use();
		}

		drawInst(header, inst);
		inst++;
	}
	teardownVertexInput(header);
}

void
defaultRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	renderCB(atomic, header, 0);
}

void
uvTransformRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	renderCB(atomic, header, 1);
}


}
}

#endif

