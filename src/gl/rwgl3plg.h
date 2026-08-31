namespace rw {
namespace gl3 {

struct Shader;

void initMatFX(void);
ObjPipeline *makeMatFXPipeline(void);
void matfxRenderCB(Atomic *atomic, InstanceDataHeader *header);
void registerEnvUniforms(void);
void uploadEnvMapState(Texture *envTex, MatFXEnvState *es);

void initSkin(void);
ObjPipeline *makeSkinPipeline(void);
void skinInstanceCB(Geometry *geo, InstanceDataHeader *header, bool32 reinstance);
void skinRenderCB(Atomic *atomic, InstanceDataHeader *header);
void uploadSkinMatrices(Atomic *atomic);
extern Shader *skinShader, *skinShader_noAT;
extern Shader *skinShader_fullLight, *skinShader_fullLight_noAT;

ObjPipeline *makeSkinMatFXPipeline(void);
void skinMatfxRenderCB(Atomic *atomic, InstanceDataHeader *header);
void createSkinMatFXShaders(void);
void destroySkinMatFXShaders(void);


}
}
