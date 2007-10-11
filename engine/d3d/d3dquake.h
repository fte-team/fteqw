#ifndef __D3DQUAKE_H__
#define __D3DQUAKE_H__

#ifdef __GNUC__
#define _inline static inline
#endif

#include    "ddraw.h"
#include    "d3d.h"
#include    "d3dx.h"
#include    "glquake.h"

void *D3D_LoadTexture_32(char *name, unsigned int *data, int width, int height, int flags);
void *D3D_LoadTexture_8_Pal24(char *name, unsigned char *data, int width, int height, int flags, unsigned char *palette, int transparentpix);
void *D3D_LoadTexture_8_Pal32(char *name, unsigned char *data, int width, int height, int flags, unsigned char *palette);
/*
#define D3D9_LoadTexture8Pal32(skinname,width,height,data,palette,usemips,alpha) (int)D3D9_LoadTexture_8_Pal32(skinname, data, width, height, (usemips?TF_MIPMAP:TF_NOMIPMAP) | (alpha?TF_ALPHA:TF_NOALPHA) | TF_NOTBUMPMAP, host_basepal)
#define D3D9_LoadTexture(skinname,width,height,data,usemips,alpha) (int)D3D9_LoadTexture_8_Pal24(skinname, data, width, height, (usemips?TF_MIPMAP:TF_NOMIPMAP) | (alpha?TF_ALPHA:TF_NOALPHA) | TF_NOTBUMPMAP, host_basepal, 255)
#define D3D9_LoadTexture32(skinname,width,height,data,usemips,alpha) (int)D3D9_LoadTexture_32(skinname, data, width, height, (usemips?TF_MIPMAP:TF_NOMIPMAP) | (alpha?TF_ALPHA:TF_NOALPHA) | TF_NOTBUMPMAP)
#define D3D9_LoadTextureFB(skinname,width,height,data,usemips,alpha) 0
#define D3D9_LoadTexture8Bump(skinname,width,height,data,usemips,alpha) 0

#define D3D9_FindTexture(name) -1 
#define D3D9_LoadCompressed(name) 0
*/


void *D3D9_LoadTexture_32(char *name, unsigned int *data, int width, int height, int flags);
void *D3D9_LoadTexture_8_Pal24(char *name, unsigned char *data, int width, int height, int flags, unsigned char *palette, int transparentpix);
void *D3D9_LoadTexture_8_Pal32(char *name, unsigned char *data, int width, int height, int flags, unsigned char *palette);


#define D3D_LoadTexture8Pal32(skinname,width,height,data,palette,usemips,alpha) (int)(pD3DDev?D3D_LoadTexture_8_Pal32:D3D9_LoadTexture_8_Pal32)(skinname, data, width, height, (usemips?TF_MIPMAP:TF_NOMIPMAP) | (alpha?TF_ALPHA:TF_NOALPHA) | TF_NOTBUMPMAP, palette)
#define D3D_LoadTexture8Pal24(skinname,width,height,data,palette,usemips,alpha) (int)(pD3DDev?D3D_LoadTexture_8_Pal24:D3D9_LoadTexture_8_Pal24)(skinname, data, width, height, (usemips?TF_MIPMAP:TF_NOMIPMAP) | (alpha?TF_ALPHA:TF_NOALPHA) | TF_NOTBUMPMAP, palette, 255)
#define D3D_LoadTexture(skinname,width,height,data,usemips,alpha) (int)(pD3DDev?D3D_LoadTexture_8_Pal24:D3D9_LoadTexture_8_Pal24)(skinname, data, width, height, (usemips?TF_MIPMAP:TF_NOMIPMAP) | (alpha?TF_ALPHA:TF_NOALPHA) | TF_NOTBUMPMAP, host_basepal, 255)
#define D3D_LoadTexture32(skinname,width,height,data,usemips,alpha) (int)(pD3DDev?D3D_LoadTexture_32:D3D9_LoadTexture_32)(skinname, data, width, height, (usemips?TF_MIPMAP:TF_NOMIPMAP) | (alpha?TF_ALPHA:TF_NOALPHA) | TF_NOTBUMPMAP)
#define D3D_LoadTextureFB(skinname,width,height,data,usemips,alpha) 0
#define D3D_LoadTexture8Bump(skinname,width,height,data,usemips,alpha) 0

#define D3D_FindTexture(name) -1 
#define D3D_LoadCompressed(name) 0



extern LPDIRECT3DDEVICE7 pD3DDev;

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

extern LPDIRECTDRAWSURFACE7 *lightmap_d3dtextures;
extern LPDIRECTDRAWSURFACE7 *deluxmap_d3dtextures;
extern lightmapinfo_t **lightmap;

extern void *d3dexplosiontexture;
extern void *d3dballtexture;

#endif

