//#include    "ddraw.h"


#include    "d3d9.h"

LPDIRECT3DBASETEXTURE9 D3D9_LoadTexture_32(char *name, unsigned int *data, int width, int height, int flags);
LPDIRECT3DBASETEXTURE9 D3D9_LoadTexture_8_Pal24(char *name, unsigned char *data, int width, int height, int flags, unsigned char *palette, int transparentpix);



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
