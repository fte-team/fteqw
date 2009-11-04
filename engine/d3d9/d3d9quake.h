//#include    "ddraw.h"

#ifndef D3D9QUAKE_H
#define D3D9QUAKE_H


#include "d3d9.h"
#include "com_mesh.h"
#include "glquake.h"

//
// d3d9_draw.c
//
void					D3D9_Draw_Alt_String (int x, int y, const qbyte *str);
void					D3D9_Draw_BeginDisc (void);
mpic_t*					D3D9_Draw_CachePic (char *path);
void					D3D9_Draw_Character (int x, int y, unsigned int num);
void					D3D9_Draw_ColouredCharacter (int x, int y, unsigned int num);
void					D3D9_Draw_ConsoleBackground	(int firstline, int lastline, qboolean forceopaque);
void					D3D9_Draw_Crosshair (void);
void					D3D9_Draw_DebugChar (qbyte num);
void					D3D9_Draw_EditorBackground (int lines);
void					D3D9_Draw_EndDisc (void);
void					D3D9_Draw_FadeScreen (void);
void					D3D9_Draw_Fill (int x, int y, int w, int h, unsigned int c);
void					D3D9_Draw_Fill_Colours (int x, int y, int w, int h);
void					D3D9_Draw_Fill_I (int x, int y, int w, int h, unsigned int imgcolour);
void					D3D9_Draw_FillRGB (int x, int y, int w, int h, float r, float g, float b);
void					D3D9_Draw_Image (float x, float y, float w, float h, float s1, float t1, float s2, float t2, mpic_t *pic);
void					D3D9_Draw_ImageColours (float r, float g, float b, float a);
void					D3D9_Draw_Init (void);
void					D3D9_Draw_Pic (int x, int y, mpic_t *pic);
void					D3D9_Draw_ReInit (void);
mpic_t*					D3D9_Draw_SafeCachePic (char *path);
mpic_t*					D3D9_Draw_SafePicFromWad (char *name);
void					D3D9_Draw_ScalePic (int x, int y, int width, int height, mpic_t *pic);
void					D3D9_Draw_String (int x, int y, const qbyte *str);
void					D3D9_Draw_SubPic (int x, int y, int width, int height, mpic_t *pic, int srcx, int srcy, int srcwidth, int srcheight);
void					D3D9_Draw_TileClear (int x, int y, int w, int h);
void					D3D9_Draw_TransPic (int x, int y, mpic_t *pic);
void					D3D9_Draw_TransPicTranslate (int x, int y, int w, int h, qbyte *pic, qbyte *translation);
void					D3D9_InitParticleTexture (void);
LPDIRECT3DBASETEXTURE9	D3D9_LoadTexture_32 (char *name, unsigned int *data, int width, int height, int flags);
LPDIRECT3DBASETEXTURE9	D3D9_LoadTexture_8_Pal24 (char *name, unsigned char *data, int width, int height, int flags, unsigned char *palette, int transparentpix);
LPDIRECT3DBASETEXTURE9	D3D9_LoadTexture_8_Pal32 (char *name, unsigned char *data, int width, int height, int flags, unsigned char *palette);
void					D3D9_Media_ShowFrame8bit (qbyte *framedata, int inwidth, int inheight, qbyte *palette);
void					D3D9_Media_ShowFrameBGR_24_Flip	(qbyte *framedata, int inwidth, int inheight);
void					D3D9_Media_ShowFrameRGBA_32 (qbyte *framedata, int inwidth, int inheight);
void					D3D9_MipMap (qbyte *out, qbyte *in, int width, int height);
void					D3D9_RoundDimensions (int *scaled_width, int *scaled_height, qboolean mipmap);
void					D3D9_UnloadTexture (LPDIRECT3DBASETEXTURE9 tex);
static void				Upload_Texture_32(LPDIRECT3DTEXTURE9 surf, unsigned int *data, int width, int height);

//
// d3d9_mesh.c
//
static galiastexnum_t*	D3D9_ChooseSkin (galiasinfo_t *inf, char *modelname, int surfnum, entity_t *e);
void					D3D9_DrawAliasModel (void);
void					D3D9_DrawMesh (mesh_t *mesh);
void					d3d9_GAliasFlushSkinCache (void);
static void				LotsOfLightDirectionHacks (entity_t *e, model_t *m, vec3_t lightaxis[3]);

//
// d3d9_rmain.c
//
void					D3D9_BaseBModelTextures (entity_t *e);
/*
void					D3D9_DrawParticleBeam (beamseg_t *b, part_type_t *type);
void					D3D9_DrawParticleBeamUT (beamseg_t *b, part_type_t *type);
void					D3D9_DrawParticleBlob (particle_t *p, part_type_t *type);
void					D3D9_DrawParticleSpark (particle_t *p, part_type_t *type);
*/
void					D3D9_DrawParticles (float ptime);
static void				D3D9_DrawSpriteModel (entity_t *e);
void					D3D9_DrawTextureChains (void);
void					D3D9_DrawWorld (void);
void					D3D9_R_DeInit (void);
void					D3D9_R_DrawEntitiesOnList (void);
void					D3D9_R_Init (void);
void					D3D9_R_ReInit (void);
void					D3D9_R_RenderScene (void);
void					D3D9_R_RenderView (void);
static void				D3D9_RecursiveQ2WorldNode (mnode_t *node);
static void				D3D9_RecursiveWorldNode (mnode_t *node);
void					D3D9_SetupFrame (void);
void					D3D9_SetupViewPort (void);
qboolean				D3D9_ShouldDraw (void);
void					D3D9R_DrawSprite(int count, void **e, void *parm);
void					IDirect3DDevice9_DrawIndexedPrimitive7 (LPDIRECT3DDEVICE9 pD3DDev9, int mode, int fvf, void *verts, int numverts, index_t *indicies, int numindicies, int wasted);

//
// d3d9_rsurf.c
//
int						D3D9_AllocBlock (int w, int h, int *x, int *y);
void					D3D9_BuildLightmaps (void);
void					D3D9_BuildSurfaceDisplayList (msurface_t *fa);
void					D3D9_CreateSurfaceLightmap (msurface_t *surf, int shift);
void					D3D9_DrawSkyMesh(int pass, int texture, void *verts, int numverts, void *indicies, int numelements);
int						D3D9_FillBlock (int texnum, int w, int h, int x, int y);
LPDIRECT3DTEXTURE9		D3D9_NewLightmap (void);
//void					D3D9R_BuildLightMap (msurface_t *surf, qbyte *dest, qbyte *deluxdest, stmap *stainsrc, int shift);
void					D3D9R_RenderDynamicLightmaps (msurface_t *fa, int shift);

//
// vid_d3d9.c
//
void					D3D9_BrightenScreen (void);
void					D3D9_D_BeginDirectRect (int x, int y, qbyte *pbitmap, int width, int height);
void					D3D9_D_EndDirectRect (int x, int y, int width, int height);
void					D3D9_GetBufferSize(int *width, int *height);
void					D3D9_Mod_ClearAll (void);
void*					D3D9_Mod_Extradata (struct model_s *mod);
struct model_s*			D3D9_Mod_FindName (char *name);
struct model_s*			D3D9_Mod_ForName (char *name, qboolean crash);
void					D3D9_Mod_Init (void);
void					D3D9_Mod_NowLoadExternal (void);
int						D3D9_Mod_SkinForName (struct model_s *model, char *name);
void					D3D9_Mod_Think (void);
void					D3D9_Mod_TouchModel (char *name);
void					D3D9_R_AddStain (vec3_t org, float red, float green, float blue, float radius);
qboolean				D3D9_R_CheckSky (void);
void					D3D9_R_LessenStains (void);
int						D3D9_R_LightPoint (vec3_t point);
void					D3D9_R_NewMap (void);
void					D3D9_R_PreNewMap (void);
void					D3D9_R_PushDlights (void);
void					D3D9_R_SetSky (char *name, float rotate, vec3_t axis);
void					D3D9_SCR_UpdateScreen (void);
void					D3D9_Set2D (void);
void					D3D9_VID_DeInit (void);
void					D3D9_VID_ForceLockState (int lk);
int						D3D9_VID_ForceUnlockedAndReturnState (void);
void					D3D9_VID_GenPaletteTables (unsigned char *palette);
char*					D3D9_VID_GetRGBInfo (int prepad, int *truevidwidth, int *truevidheight);
qboolean				D3D9_VID_Init (rendererstate_t *info, unsigned char *palette);
void					D3D9_VID_LockBuffer (void);
void					D3D9_VID_SetPalette (unsigned char *palette);
void					D3D9_VID_SetWindowCaption (char *msg);
void					D3D9_VID_ShiftPalette (unsigned char *palette);
void					D3D9_VID_UnlockBuffer (void);
static LRESULT WINAPI	D3D9_WindowProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
qboolean				D3D9AppActivate (BOOL fActive, BOOL minimize);
int						d3d9error (int i);
void					initD3D9 (HWND hWnd, rendererstate_t *info);
void					resetD3D9 (void);


#define D3D9_LoadTexture8Pal32(skinname,width,height,data,palette,usemips,alpha) (int)D3D9_LoadTexture_8_Pal32(skinname, data, width, height, (usemips?TF_MIPMAP:TF_NOMIPMAP) | (alpha?TF_ALPHA:TF_NOALPHA) | TF_NOTBUMPMAP, host_basepal)
#define D3D9_LoadTexture(skinname,width,height,data,usemips,alpha) (int)D3D9_LoadTexture_8_Pal24(skinname, data, width, height, (usemips?TF_MIPMAP:TF_NOMIPMAP) | (alpha?TF_ALPHA:TF_NOALPHA) | TF_NOTBUMPMAP, host_basepal, 255)
#define D3D9_LoadTexture32(skinname,width,height,data,usemips,alpha) (int)D3D9_LoadTexture_32(skinname, data, width, height, (usemips?TF_MIPMAP:TF_NOMIPMAP) | (alpha?TF_ALPHA:TF_NOALPHA) | TF_NOTBUMPMAP)
#define D3D9_LoadTextureFB(skinname,width,height,data,usemips,alpha) 0
#define D3D9_LoadTexture8Bump(skinname,width,height,data,usemips,alpha) 0

#define D3D9_FindTexture(name) -1 
#define D3D9_LoadCompressed(name) 0



extern LPDIRECT3DDEVICE9 pD3DDev9;

extern int		d_lightstylevalue[256];	// 8.8 fraction of base light value

#define lightmap_bytes 4


extern int numlightmaps;

extern mvertex_t *r_pcurrentvertbase;

#ifndef LMBLOCK_WIDTH
#define LMBLOCK_WIDTH 128
#define LMBLOCK_HEIGHT LMBLOCK_WIDTH
typedef struct glRect_s {
	unsigned char l,t,w,h;
} glRect_t;
typedef unsigned char stmap;

typedef struct {
	qboolean	modified;
	qboolean	deluxmodified;
	glRect_t	rectchange;
	glRect_t	deluxrectchange;
	int allocated[LMBLOCK_WIDTH];
	qbyte		lightmaps[4*LMBLOCK_WIDTH*LMBLOCK_HEIGHT];
	qbyte		deluxmaps[4*LMBLOCK_WIDTH*LMBLOCK_HEIGHT];	//fixme: make seperate structure for easy disabling with less memory usage.
	stmap		stainmaps[3*LMBLOCK_WIDTH*LMBLOCK_HEIGHT];	//rgb no a. added to lightmap for added (hopefully) speed.
} lightmapinfo_t;
#endif

extern LPDIRECT3DTEXTURE9 *lightmap_d3d9textures;
extern LPDIRECT3DTEXTURE9 *deluxmap_d3d9textures;
extern lightmapinfo_t **lightmap;

extern void *d3dexplosiontexture;
extern void *d3dballtexture;

extern index_t dummyindex;
#if sizeof_index_t == 2
	#define D3DFMT_QINDEX D3DFMT_INDEX16
#else
	#define D3DFMT_QINDEX D3DFMT_INDEX32
#endif

#endif
