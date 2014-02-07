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



#ifdef HALFLIFEMODELS
	#define MAX_BONE_CONTROLLERS 5
#endif

#define FST_BASE 0	//base frames
#define FS_REG 1	//regular frames
#define FS_COUNT 2	//regular frames
typedef struct {
	struct {
		int frame[2];
		float frametime[2];
		float lerpfrac;

#ifdef HALFLIFEMODELS
		float subblendfrac;	//hl models are weird
#endif

		int endbone;
	} g[FS_COUNT];

	float *bonestate;
	int bonecount;
	qboolean boneabs;

#ifdef HALFLIFEMODELS
	float bonecontrols[MAX_BONE_CONTROLLERS];	//hl special bone controllers
#endif
} framestate_t;




//function prototypes

#if defined(SERVERONLY)
#define qrenderer QR_NONE
#define FNC(n) (n)			//FNC is defined as 'pointer if client build, direct if dedicated server'

#else
#define FNC(n) (*n)
extern r_qrenderer_t qrenderer;
extern char *q_renderername;

mpic_t *R2D_SafeCachePic (char *path);
mpic_t *R2D_SafePicFromWad (char *name);
void R2D_DrawCrosshair (void);
void R2D_ScalePic (float x, float y, float width, float height, mpic_t *pic);
void R2D_SubPic(float x, float y, float width, float height, mpic_t *pic, float srcx, float srcy, float srcwidth, float srcheight);
void R2D_TransPicTranslate (float x, float y, int width, int height, qbyte *pic, qbyte *translation);
void R2D_TileClear (float x, float y, float w, float h);
void R2D_FadeScreen (void);

void R2D_Font_Changed(void);
void R2D_ConsoleBackground (int firstline, int lastline, qboolean forceopaque);
void R2D_EditorBackground (void);

void R2D_Image(float x, float y, float w, float h, float s1, float t1, float s2, float t2, mpic_t *pic);

void R2D_ImageColours(float r, float g, float b, float a);
void R2D_ImagePaletteColour(unsigned int i, float a);
void R2D_FillBlock(float x, float y, float w, float h);

extern void	(*Draw_Init)							(void);

extern void	(*R_Init)								(void);
extern void	(*R_DeInit)								(void);
extern void	(*R_RenderView)							(void);		// must set r_refdef first

extern void	(*R_NewMap)								(void);
extern void	(*R_PreNewMap)							(void);

extern qboolean	(*VID_Init)							(rendererstate_t *info, unsigned char *palette);
extern void	(*VID_DeInit)							(void);
extern char *(*VID_GetRGBInfo)						(int prepad, int *truevidwidth, int *truevidheight);
extern void	(*VID_SetWindowCaption)					(char *msg);

extern void SCR_Init								(void);
extern void SCR_DeInit								(void);
extern void (*SCR_UpdateScreen)						(void);
extern void SCR_BeginLoadingPlaque					(void);
extern void SCR_EndLoadingPlaque					(void);
extern void SCR_DrawConsole							(qboolean noback);
extern void SCR_SetUpToDrawConsole					(void);
extern void SCR_EraseCenterString					(void);
extern void SCR_CenterPrint							(int pnum, char *str, qboolean skipgamecode);

void R_DrawTextField(int x, int y, int w, int h, char *text, unsigned int defaultmask, unsigned int fieldflags);
#define CPRINT_BALIGN		(1<<0)	//B
#define CPRINT_TALIGN		(1<<1)	//T
#define CPRINT_LALIGN		(1<<2)	//L
#define CPRINT_RALIGN		(1<<3)	//R
#define CPRINT_BACKGROUND	(1<<4)	//P

#define CPRINT_OBITUARTY	(1<<16)	//O (show at 2/3rds from top)
#define CPRINT_PERSIST		(1<<17)	//P (doesn't time out)
#define CPRINT_TYPEWRITER	(1<<18)	//  (char at a time)

#endif

//mod_purge flags
enum mod_purge_e
{
	MP_MAPCHANGED,	//new map. old stuff no longer needed
	MP_FLUSH,		//user flush command. anything flushable goes.
	MP_RESET		//*everything* is destroyed. renderer is going down.
};

extern void	Mod_ClearAll						(void);
extern void Mod_Purge							(enum mod_purge_e type);
extern struct model_s *Mod_ForName				(char *name, qboolean crash);
extern struct model_s *Mod_FindName				(char *name);
extern void	*Mod_Extradata						(struct model_s *mod);	// handles caching
extern void	Mod_TouchModel						(char *name);

extern void	Mod_NowLoadExternal					(void);

extern void	Mod_Think							(void);
extern int Mod_SkinNumForName					(struct model_s *model, char *name);
extern int Mod_FrameNumForName					(struct model_s *model, char *name);
extern float Mod_GetFrameDuration				(struct model_s *model, int framenum);

#undef FNC

extern qboolean	Mod_GetTag						(struct model_s *model, int tagnum, framestate_t *framestate, float *transforms);
extern int Mod_TagNumForName					(struct model_s *model, char *name);

int Mod_GetNumBones(struct model_s *model, qboolean allowtags);
int Mod_GetBoneRelations(struct model_s *model, int firstbone, int lastbone, framestate_t *fstate, float *result);
int Mod_GetBoneParent(struct model_s *model, int bonenum);
struct galiasbone_s *Mod_GetBoneInfo(struct model_s *model, int *numbones);
char *Mod_GetBoneName(struct model_s *model, int bonenum);

void Draw_FunString(float x, float y, const void *str);
void Draw_AltFunString(float x, float y, const void *str);
void Draw_FunStringWidth(float x, float y, const void *str, int width, qboolean rightalign, qboolean highlight);

extern int r_regsequence;

#ifdef SERVERONLY
#define Mod_Q1LeafPVS Mod_LeafPVS
// qbyte *Mod_LeafPVS (struct mleaf_s *leaf, struct model_s *model, qbyte *buffer);
#endif

typedef struct
{
	int regsequence;
	int width;
	int height;
} texcom_t;
struct texid_s
{
	union
	{
		unsigned int num;
#if defined(D3DQUAKE) || defined(SWQUAKE)
		void *ptr;
#endif
	};
	texcom_t *ref;
};
#if 1
typedef struct texid_s texid_t;
#define texid_tf texid_t
#define TEXASSIGN(d,s) d=s
#define TEXASSIGNF(d,s) d=s
#define TEXVALID(t) ((t).ref!=NULL)
#else
typedef struct texid_s texid_t[1];
typedef struct texid_s texid_tf;
#define TEXASSIGN(d,s) memcpy(&d,&s,sizeof(d))
#define TEXASSIGNF(d,s) memcpy(&d,&s,sizeof(d))
#define TEXVALID(t) 1
#endif

//small context for easy vbo creation.
typedef struct
{
	size_t maxsize;
	size_t pos;
	int vboid[2];
	void *fallback;
} vbobctx_t;

typedef struct vboarray_s
{
	union
	{
		void *dummy;
#ifdef GLQUAKE
		struct
		{
			int vbo;
			void *addr;
		} gl;
#endif
#if defined(D3D9QUAKE) || defined(D3D11QUAKE)
		struct
		{
			void *buff;
			unsigned int offs;
		} d3d;
#endif
	};
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
	texid_t base;
	texid_t bump;
	texid_t specular;
	texid_t upperoverlay;
	texid_t loweroverlay;
	texid_t fullbright;
} texnums_t;
typedef enum uploadfmt
{
        TF_INVALID,
        TF_RGBA32,              /*rgba byte order*/
        TF_BGRA32,              /*bgra byte order*/
        TF_RGBX32,              /*rgb byte order, with extra wasted byte after blue*/
		TF_BGRX32,              /*rgb byte order, with extra wasted byte after blue*/
        TF_RGB24,               /*rgb byte order, no alpha channel nor pad, and regular top down*/
		TF_BGR24,               /*bgr byte order, no alpha channel nor pad, and regular top down*/
        TF_BGR24_FLIP,  /*bgr byte order, no alpha channel nor pad, and bottom up*/
		TF_LUM8,		/*8bit greyscale image*/
        TF_SOLID8,      /*8bit quake-palette image*/
        TF_TRANS8,      /*8bit quake-palette image, index 255=transparent*/
        TF_TRANS8_FULLBRIGHT,   /*fullbright 8 - fullbright texels have alpha 255, everything else 0*/
        TF_HEIGHT8,     /*image data is greyscale, convert to a normalmap and load that, uploaded alpha contains the original heights*/
        TF_HEIGHT8PAL, /*source data is palette values rather than actual heights, generate a fallback heightmap*/
        TF_H2_T7G1, /*8bit data, odd indexes give greyscale transparence*/
        TF_H2_TRANS8_0, /*8bit data, 0 is transparent, not 255*/
        TF_H2_T4A4,     /*8bit data, weird packing*/

        /*this block requires a palette*/
        TF_PALETTES,
        TF_8PAL24,
        TF_8PAL32,

		/*for render targets*/
		TF_DEPTH16,
		TF_DEPTH24,
		TF_DEPTH32,
		TF_RGBA16F,
		TF_RGBA32F
} uploadfmt_t;

//not all modes accept meshes - STENCIL(intentional) and DEPTHONLY(not implemented)
typedef enum backendmode_e
{
        BEM_STANDARD,			//regular mode to draw surfaces akin to q3 (aka: legacy mode). lightmaps+delux+ambient
        BEM_DEPTHONLY,			//just a quick depth pass. textures used only for alpha test (shadowmaps).
		BEM_WIREFRAME,			//for debugging or something
        BEM_STENCIL,			//used for drawing shadow volumes to the stencil buffer.
        BEM_DEPTHDARK,			//a quick depth pass. textures used only for alpha test. additive textures still shown as normal.
		BEM_CREPUSCULAR,		//sky is special, everything else completely black
		BEM_DEPTHNORM,			//all opaque stuff drawn using 'depthnorm' shader
		BEM_FOG,				//drawing a fog volume
        BEM_LIGHT,				//we have a valid light
} backendmode_t;

typedef struct rendererinfo_s {
	char *description;
	char *name[4];
	r_qrenderer_t rtype;

	void	(*Draw_Init)				(void);
	void	(*Draw_Shutdown)			(void);

	texid_tf (*IMG_LoadTexture)			(char *identifier, int width, int height, uploadfmt_t fmt, void *data, unsigned int flags);
	texid_tf (*IMG_LoadTexture8Pal24)	(char *identifier, int width, int height, qbyte *data, qbyte *palette24, unsigned int flags);
	texid_tf (*IMG_LoadTexture8Pal32)	(char *identifier, int width, int height, qbyte *data, qbyte *palette32, unsigned int flags);
	texid_tf (*IMG_LoadCompressed)		(char *name);
	texid_tf (*IMG_FindTexture)			(char *identifier, unsigned int flags);
	texid_tf (*IMG_AllocNewTexture)		(char *identifier, int w, int h, unsigned int flags);
	void    (*IMG_Upload)				(texid_t tex, char *name, uploadfmt_t fmt, void *data, void *palette, int width, int height, unsigned int flags);
	void    (*IMG_DestroyTexture)		(texid_t tex);

	void	(*R_Init)					(void); //FIXME - merge implementations
	void	(*R_DeInit)					(void);	//FIXME - merge implementations
	void	(*R_RenderView)				(void);	// must set r_refdef first

	void	(*R_NewMap)					(void);	//FIXME - merge implementations
	void	(*R_PreNewMap)				(void); //FIXME - merge implementations

	qboolean (*VID_Init)				(rendererstate_t *info, unsigned char *palette);
	void	 (*VID_DeInit)				(void);
	qboolean (*VID_ApplyGammaRamps)		(unsigned short *ramps);
	char	*(*VID_GetRGBInfo)			(int prepad, int *truevidwidth, int *truevidheight);
	void	(*VID_SetWindowCaption)		(char *msg);

	void	(*SCR_UpdateScreen)			(void);

	
	//Select the current render mode and modifier flags
	void	(*BE_SelectMode)(backendmode_t mode);
	/*Draws an entire mesh list from a VBO. vbo can be null, in which case the chain may be drawn without batching.
	  Rules for using a list: Every mesh must be part of the same VBO, shader, lightmap, and must have the same pointers set*/
	void	(*BE_DrawMesh_List)(shader_t *shader, int nummeshes, struct mesh_s **mesh, struct vbo_s *vbo, struct texnums_s *texnums, unsigned int be_flags);
	void	(*BE_DrawMesh_Single)(shader_t *shader, struct mesh_s *meshchain, struct vbo_s *vbo, struct texnums_s *texnums, unsigned int be_flags);
	void	(*BE_SubmitBatch)(struct batch_s *batch);
	struct batch_s *(*BE_GetTempBatch)(void);
	//Asks the backend to invoke DrawMeshChain for each surface, and to upload lightmaps as required
	void	(*BE_DrawWorld) (qboolean drawworld, qbyte *vis);
	//called at init, force the display to the right defaults etc
	void	(*BE_Init)(void);
	//Generates an optimised VBO, one for each texture on the map
	void (*BE_GenBrushModelVBO)(struct model_s *mod);
	//Destroys the given vbo
	void (*BE_ClearVBO)(struct vbo_s *vbo);
	//Uploads all modified lightmaps
	void (*BE_UploadAllLightmaps)(void);
	void (*BE_SelectEntity)(struct entity_s *ent);
	qboolean (*BE_SelectDLight)(struct dlight_s *dl, vec3_t colour, unsigned int lmode);
	void (*BE_Scissor)(srect_t *rect);
	/*check to see if an ent should be drawn for the selected light*/
	qboolean (*BE_LightCullModel)(vec3_t org, struct model_s *model);
	void (*BE_VBO_Begin)(vbobctx_t *ctx, unsigned int maxsize);
	void (*BE_VBO_Data)(vbobctx_t *ctx, void *data, unsigned int size, vboarray_t *varray);
	void (*BE_VBO_Finish)(vbobctx_t *ctx, void *edata, unsigned int esize, vboarray_t *earray);
	void (*BE_VBO_Destroy)(vboarray_t *vearray);
	void (*BE_RenderToTextureUpdate2d)(qboolean destchanged);
	char *alignment;
} rendererinfo_t;

#define rf currentrendererstate.renderer

#define R_LoadTexture		rf->IMG_LoadTexture
#define R_LoadTexture8Pal24	rf->IMG_LoadTexture8Pal24
#define R_LoadTexture8Pal32	rf->IMG_LoadTexture8Pal32
#define R_LoadCompressed	rf->IMG_LoadCompressed
#define R_FindTexture		rf->IMG_FindTexture
#define R_AllocNewTexture	rf->IMG_AllocNewTexture
#define R_Upload			rf->IMG_Upload
#define R_DestroyTexture	rf->IMG_DestroyTexture

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

texid_t R2D_RT_Configure(unsigned int id, int width, int height, uploadfmt_t rtfmt);
texid_t R2D_RT_GetTexture(unsigned int id, unsigned int *width, unsigned int *height);

