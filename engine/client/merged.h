#ifdef VKQUAKE
#if defined(__LP64__) || defined(_WIN64)
#define VulkanWasDesignedByARetard void*
#elif defined(_MSC_VER) && _MSC_VER < 1300
#define VulkanWasDesignedByARetard __int64
#else
#define VulkanWasDesignedByARetard long long
#endif
#define VkRetardedDescriptorSet VulkanWasDesignedByARetard
#define VkRetardedShaderModule VulkanWasDesignedByARetard
#define VkRetardedPipelineLayout VulkanWasDesignedByARetard
#define VkRetardedDescriptorSetLayout VulkanWasDesignedByARetard
#define VkRetardedBuffer VulkanWasDesignedByARetard
#define VkRetardedDeviceMemory VulkanWasDesignedByARetard
#endif

//These are defined later in the source tree. This file should probably be moved to a later spot.
struct pubprogfuncs_s;
struct globalvars_s;
struct texture_s;
struct texnums_s;
struct vbo_s;
struct mesh_s;
struct batch_s;
struct entity_s;
struct dlight_s;
struct galiasbone_s;
struct dlight_s;
struct font_s;

typedef enum
{
	SKEL_RELATIVE,	//relative to parent.
	SKEL_ABSOLUTE,	//relative to model. doesn't blend very well.
	SKEL_INVERSE_RELATIVE,	//pre-inverted. faster than regular relative but has weirdness with skeletal objects. blends okay.
	SKEL_INVERSE_ABSOLUTE,	//final renderable type.
	SKEL_IDENTITY,	//PANIC
	SKEL_QUATS		//quat+org, 7 floats rather than 12. better lerping.
} skeltype_t;

#ifdef HALFLIFEMODELS
	#define MAX_BONE_CONTROLLERS 5
#endif

#define FRAME_BLENDS 4
#define FST_BASE 0	//base frames
#define FS_REG 1	//regular frames
#define FS_COUNT 2	//regular frames
typedef struct {
	struct framestateregion_s {
		int frame[FRAME_BLENDS];
		float frametime[FRAME_BLENDS];
		float lerpweight[FRAME_BLENDS];

#ifdef HALFLIFEMODELS
		float subblendfrac;	//hl models are weird
#endif

		int endbone;
	} g[FS_COUNT];

#ifdef SKELETALOBJECTS
	float *bonestate;
	int bonecount;
	skeltype_t skeltype;
#endif

#ifdef HALFLIFEMODELS
	float bonecontrols[MAX_BONE_CONTROLLERS];	//hl special bone controllers
#endif
} framestate_t;
#define NULLFRAMESTATE (framestate_t*)NULL




//function prototypes

#if defined(SERVERONLY)
#define qrenderer QR_NONE
#define FNC(n) (n)			//FNC is defined as 'pointer if client build, direct if dedicated server'

#else
#define FNC(n) (*n)
extern r_qrenderer_t qrenderer;
extern char *q_renderername;

apic_t *R2D_LoadAtlasedPic(const char *name);
void R2D_ImageAtlas(float x, float y, float w, float h, float s1, float t1, float s2, float t2, apic_t *pic);
#define R2D_ScalePicAtlas(x,y,w,h,p) R2D_ImageAtlas(x,y,w,h,0,0,1,1,p)

mpic_t *R2D_SafeCachePic (const char *path);
mpic_t *R2D_SafePicFromWad (const char *name);
void R2D_DrawCrosshair (void);
void R2D_ScalePic (float x, float y, float width, float height, mpic_t *pic);
void R2D_SubPic(float x, float y, float width, float height, mpic_t *pic, float srcx, float srcy, float srcwidth, float srcheight);
void R2D_TransPicTranslate (float x, float y, int width, int height, qbyte *pic, unsigned int *palette);
void R2D_TileClear (float x, float y, float w, float h);
void R2D_FadeScreen (void);

void R2D_Font_Changed(void);
void R2D_ConsoleBackground (int firstline, int lastline, qboolean forceopaque);
void R2D_EditorBackground (void);

void R2D_Image(float x, float y, float w, float h, float s1, float t1, float s2, float t2, mpic_t *pic);
void R2D_Image2dQuad(vec2_t points[], vec2_t texcoords[], mpic_t *pic);

void R2D_ImageColours(float r, float g, float b, float a);
void R2D_ImagePaletteColour(unsigned int i, float a);
void R2D_FillBlock(float x, float y, float w, float h);
void R2D_Line(float x1, float y1, float x2, float y2, mpic_t *pic);
extern void (*R2D_Flush)(void);

extern void	(*Draw_Init)							(void);

extern void	(*R_Init)								(void);
extern void	(*R_DeInit)								(void);
extern void	(*R_RenderView)							(void);		// must set r_refdef first

extern qboolean	(*VID_Init)							(rendererstate_t *info, unsigned char *palette);
extern void	(*VID_DeInit)							(void);
extern char *(*VID_GetRGBInfo)						(int *stride, int *truevidwidth, int *truevidheight, enum uploadfmt *fmt); //if stride is negative, then the return value points to the last line intead of the first. this allows it to be freed normally.
extern void	(*VID_SetWindowCaption)					(const char *msg);

extern void SCR_Init								(void);
extern void SCR_DeInit								(void);
extern qboolean (*SCR_UpdateScreen)					(void);
extern void SCR_BeginLoadingPlaque					(void);
extern void SCR_EndLoadingPlaque					(void);
extern void SCR_DrawConsole							(qboolean noback);
extern void SCR_SetUpToDrawConsole					(void);
extern void SCR_CenterPrint							(int pnum, char *str, qboolean skipgamecode);

void R_DrawTextField(int x, int y, int w, int h, const char *text, unsigned int defaultmask, unsigned int fieldflags, struct font_s *font, vec2_t fontscale);
#define CPRINT_LALIGN		(1<<0)	//L
#define CPRINT_TALIGN		(1<<1)	//T
#define CPRINT_RALIGN		(1<<2)	//R
#define CPRINT_BALIGN		(1<<3)	//B
#define CPRINT_BACKGROUND	(1<<4)	//P

#define CPRINT_OBITUARTY	(1<<16)	//O (show at 2/3rds from top)
#define CPRINT_PERSIST		(1<<17)	//P (doesn't time out)
#define CPRINT_TYPEWRITER	(1<<18)	//  (char at a time)

#endif

//mod_purge flags
enum mod_purge_e
{
	MP_MAPCHANGED,	//new map. old stuff no longer needed, can skip stuff if it'll be expensive.
	MP_FLUSH,		//user flush command. anything flushable goes.
	MP_RESET		//*everything* is destroyed. renderer is going down, or at least nothing depends upon it.
};
enum mlverbosity_e
{
	MLV_SILENT,
	MLV_WARN,
	MLV_WARNSYNC,
	MLV_ERROR
};

const char *Mod_GetEntitiesString(struct model_s *mod);
void Mod_SetEntitiesStringLen(struct model_s *mod, const char *str, size_t strsize);
void Mod_SetEntitiesString(struct model_s *mod, const char *str, qboolean docopy);
void Mod_ParseEntities(struct model_s *mod);
extern void	Mod_ClearAll						(void);
extern void Mod_Purge							(enum mod_purge_e type);
extern qboolean Mod_PurgeModel					(struct model_s	*mod, enum mod_purge_e ptype);
extern struct model_s *Mod_FindName				(const char *name);	//find without loading. needload should be set.
extern struct model_s *Mod_ForName				(const char *name, enum mlverbosity_e verbosity);	//finds+loads
extern struct model_s *Mod_LoadModel			(struct model_s *mod, enum mlverbosity_e verbose);	//makes sure a model is loaded
extern void	*Mod_Extradata						(struct model_s *mod);	// handles caching
extern void	Mod_TouchModel						(const char *name);
extern const char *Mod_FixName					(const char *modname, const char *worldname);	//remaps the name appropriately
const char *Mod_ParseWorldspawnKey				(struct model_s *mod, const char *key, char *buffer, size_t sizeofbuffer);

extern long relitsurface;
extern struct model_s *lightmodel;
extern void	Mod_Think							(void);
extern qboolean Mod_GetModelEvent				(struct model_s *model, int animation, int eventidx, float *timestamp, int *eventcode, char **eventdata);
extern int Mod_SkinNumForName					(struct model_s *model, int surfaceidx, const char *name);
extern int Mod_FrameNumForName					(struct model_s *model, int surfaceidx, const char *name);
extern float Mod_GetFrameDuration				(struct model_s *model, int surfaceidx, int framenum);
extern int Mod_GetFrameCount					(struct model_s *model);

#undef FNC

extern qboolean	Mod_GetTag						(struct model_s *model, int tagnum, framestate_t *framestate, float *transforms);
extern int Mod_TagNumForName					(struct model_s *model, const char *name);

void Mod_AddSingleSurface(struct entity_s *ent, int surfaceidx, shader_t *shader);
int Mod_GetNumBones(struct model_s *model, qboolean allowtags);
int Mod_GetBoneRelations(struct model_s *model, int firstbone, int lastbone, framestate_t *fstate, float *result);
int Mod_GetBoneParent(struct model_s *model, int bonenum);
struct galiasbone_s *Mod_GetBoneInfo(struct model_s *model, int *numbones);
const char *Mod_GetBoneName(struct model_s *model, int bonenum);

void Draw_FunString(float x, float y, const void *str);
void Draw_AltFunString(float x, float y, const void *str);
void Draw_FunStringWidth(float x, float y, const void *str, int width, int rightalign, qboolean highlight);

extern int r_regsequence;

#ifdef SERVERONLY
#define Mod_Q1LeafPVS Mod_LeafPVS
// qbyte *Mod_LeafPVS (struct mleaf_s *leaf, struct model_s *model, qbyte *buffer);
#endif

enum
{
	TEX_NOTLOADED,
	TEX_LOADING,
	TEX_LOADED,
	TEX_FAILED
};
typedef struct image_s
{
#ifdef _DEBUG
	char dbgident[32];
#endif
	char *ident;	//allocated on end
	char *subpath;	//allocated on end
	int regsequence;
	int width;	//this is the logical size. the physical size is not considered important (except for render targets, which should not be loaded from disk).
	int height;
	int status;	//TEX_
	unsigned int flags;
	struct image_s *next;
	struct image_s *prev;
	struct image_s *aliasof;
	union
	{
		int nosyntaxerror;
#if defined(D3DQUAKE) || defined(SWQUAKE)
		struct
		{
			void *ptr;	//texture
			void *ptr2;	//view
		};
#endif
#ifdef GLQUAKE
		int num;
#endif
#ifdef VKQUAKE
		struct
		{
			VkRetardedDescriptorSet vkdescriptor;
			struct vk_image_s *vkimage;
		};
#endif
	};

	void *fallbackdata;
	int fallbackwidth;
	int fallbackheight;
	uploadfmt_t fallbackfmt;
} image_t;

#if 1
typedef image_t *texid_t;
#define texid_tf texid_t
#define TEXASSIGN(d,s) d=s
#define TEXASSIGNF(d,s) d=s
#define TEXVALID(t) ((t))
#define TEXLOADED(tex) ((tex) && (tex)->status == TEX_LOADED)
#define TEXDOWAIT(tex)	do{if ((tex) && (tex)->status == TEX_LOADING)	COM_WorkerPartialSync((tex), &(tex)->status, TEX_LOADING);}while(0)
#else
typedef struct texid_s texid_t[1];
typedef struct texid_s texid_tf;
#define TEXASSIGN(d,s) memcpy(&d,&s,sizeof(d))
#define TEXASSIGNF(d,s) memcpy(&d,&s,sizeof(d))
#define TEXVALID(t) 1
#endif

#define Image_LinearFloatFromsRGBFloat(c) (((c) <= 0.04045f) ? (c) * (1.0f / 12.92f) : (float)pow(((c) + 0.055f)*(1.0f/1.055f), 2.4f))
#define Image_sRGBFloatFromLinearFloat(c) (((c) < 0.0031308f) ? (c) * 12.92f : 1.055f * (float)pow((c), 1.0f/2.4f) - 0.055f)

struct pendingtextureinfo
{
	enum
	{
		PTI_2D,
		PTI_3D,
		PTI_CUBEMAP	//mips are packed (to make d3d11 happy)
	} type;

	enum
	{
		//these formats are specified as direct byte access
		PTI_RGBA8,	//rgba byte ordering
		PTI_RGBX8,	//rgb pad byte ordering
		PTI_BGRA8,	//alpha channel
		PTI_BGRX8,	//no alpha channel
		PTI_RGBA8_SRGB,	//rgba byte ordering
		PTI_RGBX8_SRGB,	//rgb pad byte ordering
		PTI_BGRA8_SRGB,	//alpha channel
		PTI_BGRX8_SRGB,	//no alpha channel
		//these formats are specified in native endian order
		PTI_RGB565,		//16bit alphaless format.
		PTI_RGBA4444,	//16bit format (gl)
		PTI_ARGB4444,	//16bit format (d3d)
		PTI_RGBA5551,	//16bit alpha format (gl).
		PTI_ARGB1555,	//16bit alpha format (d3d).
		//floating point formats
		PTI_RGBA16F,
		PTI_RGBA32F,
		//small formats.
		PTI_R8,
		PTI_RG8,
		//compressed formats
		PTI_S3RGB1,
		PTI_S3RGBA1,
		PTI_S3RGBA3,
		PTI_S3RGBA5,
		//weird specialcase mess to take advantage of webgl so we don't need redundant bloat where we're already strugging with potential heap limits
		PTI_WHOLEFILE,
		//depth formats
		PTI_DEPTH16,
		PTI_DEPTH24,
		PTI_DEPTH32,
		PTI_DEPTH24_8,
		PTI_MAX,
	} encoding;	//0
	int mipcount;
	struct
	{
		void *data;
		size_t datasize;
		int width;
		int height;
		qboolean needfree;
	} mip[32];
	void *extrafree;
};

//small context for easy vbo creation.
typedef struct
{
	size_t maxsize;
	size_t pos;
	int vboid[2];
	void *vboptr[2];
	void *fallback;
} vbobctx_t;

typedef union vboarray_s
{
	void *sysptr;
#ifdef GLQUAKE
	struct
	{
		int vbo;
		void *addr;
	} gl;
#endif
#if defined(D3D8QUAKE) || defined(D3D9QUAKE) || defined(D3D11QUAKE)
	struct
	{
		void *buff;
		unsigned int offs;
	} d3d;
#endif
#ifdef VKQUAKE
	struct
	{
		VkRetardedBuffer buff;
		unsigned int offs;
	} vk;
#endif
} vboarray_t;

//scissor rects
typedef struct
{
	float x;
	float y;
	float width;
	float height;
	double dmin;
	double dmax;
} srect_t;

typedef struct texnums_s {
	char	mapname[MAX_QPATH];	//the 'official' name of the diffusemap. used to generate filenames for other textures.
	texid_t base;			//regular diffuse texture. may have alpha if surface is transparent
	texid_t bump;			//normalmap. height values packed in alpha.
	texid_t specular;		//specular lighting values.
	texid_t upperoverlay;	//diffuse texture for the upper body(shirt colour). no alpha channel. added to base.rgb
	texid_t loweroverlay;	//diffuse texture for the lower body(trouser colour). no alpha channel. added to base.rgb
	texid_t paletted;		//8bit paletted data, just because.
	texid_t fullbright;
	texid_t reflectcube;
	texid_t reflectmask;
} texnums_t;

//not all modes accept meshes - STENCIL(intentional) and DEPTHONLY(not implemented)
typedef enum backendmode_e
{
	BEM_STANDARD,			//regular mode to draw surfaces akin to q3 (aka: legacy mode). lightmaps+delux+ambient
	BEM_DEPTHONLY,			//just a quick depth pass. textures used only for alpha test (shadowmaps).
	BEM_WIREFRAME,			//for debugging or something
	BEM_STENCIL,			//used for drawing shadow volumes to the stencil buffer.
	BEM_DEPTHDARK,			//a quick depth pass. textures used only for alpha test. additive textures still shown as normal.
	BEM_CREPUSCULAR,		//sky is special, everything else completely black
	BEM_GBUFFER,			//
	BEM_FOG,				//drawing a fog volume
	BEM_LIGHT,				//we have a valid light
} backendmode_t;

typedef struct rendererinfo_s {
	char *description;
	char *name[4];
	r_qrenderer_t rtype;
	//FIXME: all but the vid stuff really should be filled in by the video code, simplifying system-specific stuff.

	void	(*Draw_Init)				(void);
	void	(*Draw_Shutdown)			(void);

	void	 (*IMG_UpdateFiltering)		(image_t *imagelist, int filtermip[3], int filterpic[3], int mipcap[2], float anis);
	qboolean (*IMG_LoadTextureMips)		(texid_t tex, struct pendingtextureinfo *mips);
	void	 (*IMG_DestroyTexture)		(texid_t tex);

	void	 (*R_Init)					(void); //FIXME - merge implementations
	void	 (*R_DeInit)					(void);	//FIXME - merge implementations
	void	 (*R_RenderView)				(void);	// must set r_refdef first

	qboolean (*VID_Init)				(rendererstate_t *info, unsigned char *palette);
	void	 (*VID_DeInit)				(void);
	void	 (*VID_SwapBuffers)			(void);	//force a buffer swap, regardless of what's displayed.
	qboolean (*VID_ApplyGammaRamps)		(unsigned int size, unsigned short *ramps);

	void	*(*VID_CreateCursor)			(const char *filename, float hotx, float hoty, float scale);	//may be null, stub returns null
	qboolean (*VID_SetCursor)			(void *cursor);	//may be null
	void	 (*VID_DestroyCursor)			(void *cursor);	//may be null

	void	 (*VID_SetWindowCaption)		(const char *msg);
	char	*(*VID_GetRGBInfo)			(int *bytestride, int *truevidwidth, int *truevidheight, enum uploadfmt *fmt);

	qboolean (*SCR_UpdateScreen)			(void);

	
	//Select the current render mode and modifier flags
	void	(*BE_SelectMode)(backendmode_t mode);
	/*Draws an entire mesh list from a VBO. vbo can be null, in which case the chain may be drawn without batching.
	  Rules for using a list: Every mesh must be part of the same VBO, shader, lightmap, and must have the same pointers set*/
	void	(*BE_DrawMesh_List)(shader_t *shader, int nummeshes, struct mesh_s **mesh, struct vbo_s *vbo, struct texnums_s *texnums, unsigned int be_flags);
	void	(*BE_DrawMesh_Single)(shader_t *shader, struct mesh_s *meshchain, struct vbo_s *vbo, unsigned int be_flags);
	void	(*BE_SubmitBatch)(struct batch_s *batch);
	struct batch_s *(*BE_GetTempBatch)(void);
	//Asks the backend to invoke DrawMeshChain for each surface, and to upload lightmaps as required
	void	(*BE_DrawWorld) (struct batch_s **worldbatches);
	//called at init, force the display to the right defaults etc
	void	(*BE_Init)(void);
	//Generates an optimised VBO, one for each texture on the map
	void	(*BE_GenBrushModelVBO)(struct model_s *mod);
	//Destroys the given vbo
	void	(*BE_ClearVBO)(struct vbo_s *vbo);
	//Uploads all modified lightmaps
	void	(*BE_UploadAllLightmaps)(void);
	void	(*BE_SelectEntity)(struct entity_s *ent);
	qboolean (*BE_SelectDLight)(struct dlight_s *dl, vec3_t colour, vec3_t axis[3], unsigned int lmode);
	void	(*BE_Scissor)(srect_t *rect);
	/*check to see if an ent should be drawn for the selected light*/
	qboolean (*BE_LightCullModel)(vec3_t org, struct model_s *model);
	void	(*BE_VBO_Begin)(vbobctx_t *ctx, size_t maxsize);
	void	(*BE_VBO_Data)(vbobctx_t *ctx, void *data, size_t size, vboarray_t *varray);
	void	(*BE_VBO_Finish)(vbobctx_t *ctx, void *edata, size_t esize, vboarray_t *earray, void **vbomem, void **ebomem);
	void	(*BE_VBO_Destroy)(vboarray_t *vearray, void *mem);
	void	(*BE_RenderToTextureUpdate2d)(qboolean destchanged);

	char *alignment;	//just to make sure that added functions cause compile warnings.
} rendererinfo_t;

#define rf currentrendererstate.renderer

#define VID_SwapBuffers		rf->VID_SwapBuffers

#define BE_Init					rf->BE_Init
#define BE_SelectMode			rf->BE_SelectMode
#define BE_GenBrushModelVBO		rf->BE_GenBrushModelVBO
#define BE_ClearVBO				rf->BE_ClearVBO
#define BE_UploadAllLightmaps	rf->BE_UploadAllLightmaps
#define BE_LightCullModel		rf->BE_LightCullModel
#define BE_SelectEntity			rf->BE_SelectEntity
#define BE_SelectDLight			rf->BE_SelectDLight
#define BE_GetTempBatch			rf->BE_GetTempBatch
#define BE_SubmitBatch			rf->BE_SubmitBatch
#define BE_DrawMesh_List		rf->BE_DrawMesh_List
#define BE_DrawMesh_Single		rf->BE_DrawMesh_Single
#define BE_SubmitMeshes			rf->BE_SubmitMeshes
#define BE_DrawWorld			rf->BE_DrawWorld
#define BE_VBO_Begin 			rf->BE_VBO_Begin
#define BE_VBO_Data				rf->BE_VBO_Data
#define BE_VBO_Finish			rf->BE_VBO_Finish
#define BE_VBO_Destroy			rf->BE_VBO_Destroy
#define BE_Scissor				rf->BE_Scissor

#define BE_RenderToTextureUpdate2d rf->BE_RenderToTextureUpdate2d

#define RT_IMAGEFLAGS (IF_NOMIPMAP|IF_CLAMP|IF_LINEAR|IF_RENDERTARGET)
texid_t R2D_RT_Configure(const char *id, int width, int height, uploadfmt_t rtfmt, unsigned int imageflags);
texid_t R2D_RT_GetTexture(const char *id, unsigned int *width, unsigned int *height);


