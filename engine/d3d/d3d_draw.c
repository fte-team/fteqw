#include "quakedef.h"
#ifdef D3DQUAKE
#include "d3dquake.h"

#define MAX_WIDTH 512
#define MAX_HEIGHT 512

void *d3dexplosiontexture;
void *d3dballtexture;

LPDIRECTDRAWSURFACE7 draw_chars_tex;
mpic_t *conback_tex;

typedef struct d3dcachepic_s
{
	char		name[MAX_QPATH];
	mpic_t		pic;
} d3dcachepic_t;
#define	MAX_CACHED_PICS		512	//a temporary solution
d3dcachepic_t	d3dmenu_cachepics[MAX_CACHED_PICS];
int			d3dmenu_numcachepics;



typedef struct {
	float x, y, z;
	int colour;
	float s, t;
} d3dquadvert_t;
d3dquadvert_t d3dquadvert[4];
index_t d3dquadindexes[6] = {
	0, 1, 2,
	0, 2, 3
};
//pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX2, d3dstate.vertbuf, d3dstate.numverts, d3dstate.indexbuf, d3dstate.numindicies, 0);


static void Upload_Texture_32(LPDIRECTDRAWSURFACE7 surf, unsigned int *data, int width, int height)
{
	int x, y;
	unsigned int *dest;
	unsigned char swapbuf[4];
	unsigned char swapbuf2[4];
	DDSURFACEDESC2 desc;

	memset(&desc, 0, sizeof(desc));

	surf->lpVtbl->Lock(surf, NULL, &desc, DDLOCK_NOSYSLOCK|DDLOCK_WAIT|DDLOCK_WRITEONLY|DDLOCK_DISCARDCONTENTS, NULL);

	if (width == desc.dwWidth && height == desc.dwHeight)
	{
//		if (desc.lPitch == twidth*4)
//		{
//			memcpy(desc.lpSurface, data, width*height*4);
//		}
//		else
		{
			for (y = 0; y < desc.dwHeight; y++)
			{
				dest = (unsigned int *)((char *)desc.lpSurface + desc.lPitch*y);
				for (x = 0; x < desc.dwWidth; x++)
				{
					*(unsigned int*)swapbuf2 = *(unsigned int*)swapbuf = data[x];
					swapbuf[0] = swapbuf2[2];
					swapbuf[2] = swapbuf2[0];
					dest[x] = *(unsigned int*)swapbuf;
				}
				data += width;
			}
		}
	}
	else
	{
		int x, y;
		int iny;
		unsigned int *row, *inrow;

		for (y = 0; y < desc.dwHeight; y++)
		{
			row = (unsigned int*)((char *)desc.lpSurface + desc.lPitch*y);
			iny = (y * height) / desc.dwHeight;
			inrow = data + width*iny;
			for (x = 0; x < desc.dwWidth; x++)
			{
				*(unsigned int*)swapbuf2 = *(unsigned int*)swapbuf =  inrow[(x * width)/desc.dwWidth];
				swapbuf[0] = swapbuf2[2];
				swapbuf[2] = swapbuf2[0];
				row[x] = *(unsigned int*)swapbuf;
			}
		}



		//mimic opengl and draw it white
//		memset(desc.lpSurface, 255, twidth*theight*4);
	}

	surf->lpVtbl->Unlock(surf, NULL);
}

void D3D7_MipMap (qbyte *out, qbyte *in, int width, int height)
{
	int		i, j;

	width <<=2;
	height >>= 1;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=8, out+=4, in+=8)
		{
			out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
			out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
			out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
			out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
		}
	}
}

//create a basic shader from a 32bit image
void *D3D7_LoadTexture_32(char *name, unsigned int *data, int width, int height, int flags)
{
	static unsigned char mipdata[(MAX_WIDTH/2)*(MAX_HEIGHT/2)][4];

	DWORD tflags;
	DWORD twidth = width;
	DWORD theight = height;
	D3DX_SURFACEFORMAT tformat = D3DX_SF_A8R8G8B8;
	LPDIRECTDRAWSURFACE7 newsurf;
	DWORD nummips;

/*	if (!(flags & TF_MANDATORY))
	{
		Con_Printf("Texture upload missing flags\n");
		return NULL;
	}
*/
	if (flags & TF_MIPMAP)
		tflags = 0;
	else
		tflags = D3DX_TEXTURE_NOMIPMAP;

    D3DXCreateTexture(pD3DDev, &tflags, &twidth, &theight, &tformat, NULL, &newsurf, &nummips);
	if (!newsurf)
		return NULL;

	if (tformat != D3DX_SF_A8R8G8B8)
		Sys_Error("Couldn't create a d3d texture with a usable texture format");


	Upload_Texture_32(newsurf, data, width, height);

	if (flags & TF_MIPMAP)
	{
		LPDIRECTDRAWSURFACE7 miptex;
		LPDIRECTDRAWSURFACE7 lasttex;
		DDSCAPS2 caps;
		memset(&caps, 0, sizeof(caps));
		caps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP; 
		newsurf->lpVtbl->GetAttachedSurface(newsurf, &caps, &miptex);
		while (miptex)
		{
			D3D7_MipMap(mipdata, data, width, height);
			data = mipdata;
			width/=2;
			height/=2;

			Upload_Texture_32(miptex, mipdata, width, height);

			lasttex = miptex;
			miptex->lpVtbl->GetAttachedSurface(miptex, &caps, &miptex);
			lasttex->lpVtbl->Release(lasttex);
		}
	}

	return newsurf;
}

//create a basic shader from an 8bit image with 24bit palette
void *D3D7_LoadTexture_8_Pal24(char *name, unsigned char *data, int width, int height, int flags, unsigned char *palette, int transparentpix)
{
	//just expands it to 32bpp and passes it on
	static unsigned char out[MAX_WIDTH*MAX_HEIGHT][4];
	int i;

	if (!(flags & TF_ALPHA))
		transparentpix = 256;

	if (width*height > MAX_WIDTH*MAX_HEIGHT)
		Sys_Error("GL_Upload8_Pal24: too big");

	for (i = width*height; i >= 0 ; i--)
	{
		out[i][0] = palette[data[i]*3+0];
		out[i][1] = palette[data[i]*3+1];
		out[i][2] = palette[data[i]*3+2];
		out[i][3] = 255*(data[i] != transparentpix);
	}


	return D3D7_LoadTexture_32(name, (unsigned int*)out, width, height, flags);
}

void *D3D7_LoadTexture_8_Pal32(char *name, unsigned char *data, int width, int height, int flags, unsigned char *palette)
{
	//just expands it to 32bpp and passes it on
	static unsigned char out[MAX_WIDTH*MAX_HEIGHT][4];
	int i;

	if (width*height > MAX_WIDTH*MAX_HEIGHT)
		Sys_Error("GL_Upload8_Pal24: too big");

	for (i = width*height; i >= 0 ; i--)
	{
		out[i][0] = palette[data[i]*4+0];
		out[i][1] = palette[data[i]*4+1];
		out[i][2] = palette[data[i]*4+2];
		out[i][3] = palette[data[i]*4+3];
	}


	return D3D7_LoadTexture_32(name, (unsigned int*)out, width, height, flags);
}


void D3D7_UnloadTexture(LPDIRECTDRAWSURFACE7 tex)
{
	tex->lpVtbl->Release(tex);
}








static qbyte	exptexture[16][16] =
{
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{0,0,0,0,1,0,0,0,1,0,0,1,0,0,0,0},
	{0,0,0,1,1,1,1,1,3,1,1,2,1,0,0,0},
	{0,0,0,1,1,1,1,4,4,4,5,4,2,1,1,0},
	{0,0,1,1,6,5,5,8,6,8,3,6,3,2,1,0},
	{0,0,1,5,6,7,5,6,8,8,8,3,3,1,0,0},
	{0,0,0,1,6,8,9,9,9,9,4,6,3,1,0,0},
	{0,0,2,1,7,7,9,9,9,9,5,3,1,0,0,0},
	{0,0,2,4,6,8,9,9,9,9,8,6,1,0,0,0},
	{0,0,2,2,3,5,6,8,9,8,8,4,4,1,0,0},
	{0,0,1,2,4,1,8,7,8,8,6,5,4,1,0,0},
	{0,1,1,1,7,8,1,6,7,5,4,7,1,0,0,0},
	{0,1,2,1,1,5,1,3,4,3,1,1,0,0,0,0},
	{0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};
void D3D7_InitParticleTexture (void)
{
#define PARTICLETEXTURESIZE 64
	int		x,y;
	float dx, dy, d;
	qbyte	data[PARTICLETEXTURESIZE*PARTICLETEXTURESIZE][4];

//Explosion texture


	for (x=0 ; x<16 ; x++)
	{
		for (y=0 ; y<16 ; y++)
		{
			data[y*16+x][0] = 255;
			data[y*16+x][1] = 255;
			data[y*16+x][2] = 255;
			data[y*16+x][3] = exptexture[x][y]*255/9.0;
		}
	}
	d3dexplosiontexture = D3D7_LoadTexture_32("", (unsigned int*)data, 16, 16, TF_ALPHA|TF_NOTBUMPMAP|TF_NOMIPMAP);
		
	memset(data, 255, sizeof(data));
	for (y = 0;y < PARTICLETEXTURESIZE;y++)
	{
		dy = (y - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
		for (x = 0;x < PARTICLETEXTURESIZE;x++)
		{
			dx = (x - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
			d = 256 * (1 - (dx*dx+dy*dy));
			d = bound(0, d, 255);
			data[y*PARTICLETEXTURESIZE+x][3] = (qbyte) d;
		}
	}

	d3dballtexture = D3D7_LoadTexture_32("", (unsigned int*)data, PARTICLETEXTURESIZE, PARTICLETEXTURESIZE, TF_ALPHA|TF_NOTBUMPMAP|TF_NOMIPMAP);
}









mpic_t	*(D3D7_Draw_SafePicFromWad)			(char *name)
{
	LPDIRECTDRAWSURFACE7 *p;
	d3dcachepic_t *pic;
	qpic_t *qpic;
	int i;

	for (i = 0; i < d3dmenu_numcachepics; i++)
	{
		if (!strcmp(d3dmenu_cachepics[i].name, name))
			return &d3dmenu_cachepics[i].pic;
	}

	qpic = (qpic_t *)W_SafeGetLumpName (name);
	if (!qpic)
	{
		return NULL;
	}

	SwapPic (qpic);

	pic = &d3dmenu_cachepics[d3dmenu_numcachepics++];

	Q_strncpyz (pic->name, name, sizeof(pic->name));

	pic->pic.width = qpic->width;
	pic->pic.height = qpic->height;
	p = (LPDIRECTDRAWSURFACE7*)&pic->pic.data;
	if (!strcmp(name, "conchars"))
		*p = draw_chars_tex;
	else
	{
		*p = (LPDIRECTDRAWSURFACE7)Mod_LoadReplacementTexture(pic->name, "wad", false, true, true);
		if (!*p)
			*p = D3D7_LoadTexture_8_Pal24(name, (unsigned char*)(qpic+1), qpic->width, qpic->height, TF_NOMIPMAP|TF_ALPHA|TF_NOTBUMPMAP, host_basepal, 255);
	}

//	Con_Printf("Fixme: D3D_Draw_SafePicFromWad\n");
	return &pic->pic;
}
mpic_t	*(D3D7_Draw_SafeCachePic)		(char *path)
{
	LPDIRECTDRAWSURFACE7 *p;
	d3dcachepic_t *pic;
	qpic_t *qpic;
	int i;


	for (i = 0; i < d3dmenu_numcachepics; i++)
	{
		if (!strcmp(d3dmenu_cachepics[i].name, path))
			return &d3dmenu_cachepics[i].pic;
	}

	qpic = (qpic_t *)COM_LoadTempFile (path);
	if (!qpic)
	{
		return NULL;
	}

	SwapPic (qpic);

	pic = &d3dmenu_cachepics[d3dmenu_numcachepics++];

	Q_strncpyz (pic->name, path, sizeof(pic->name));

	pic->pic.width = qpic->width;
	pic->pic.height = qpic->height;
	p = (LPDIRECTDRAWSURFACE7*)&pic->pic.data;
	*p = (LPDIRECTDRAWSURFACE7)Mod_LoadReplacementTexture(pic->name, "gfx", false, true, true);
	if (!*p)
		*p = D3D7_LoadTexture_8_Pal24(path, (unsigned char*)(qpic+1), qpic->width, qpic->height, TF_NOMIPMAP|TF_ALPHA|TF_NOTBUMPMAP, host_basepal, 255);

//	Con_Printf("Fixme: D3D_Draw_SafeCachePic\n");
	return &pic->pic;
}
mpic_t	*(D3D7_Draw_CachePic)			(char *path)
{
	mpic_t	*pic;
	pic = D3D7_Draw_SafeCachePic(path);
	if (!pic)
		Sys_Error("Couldn't load picture %s", path);
	return pic;
}
void	(D3D7_Draw_ReInit)				(void)
{
	d3dmenu_numcachepics = 0;

	draw_chars = W_SafeGetLumpName ("conchars");

	draw_chars_tex = (LPDIRECTDRAWSURFACE7)Mod_LoadReplacementTexture("conchars", "gfx", false, true, true);
	if (!draw_chars_tex)
	{			
		if (draw_chars)
			draw_chars_tex = D3D7_LoadTexture_8_Pal24("conchars", draw_chars, 128, 128, TF_NOMIPMAP|TF_ALPHA|TF_NOTBUMPMAP, host_basepal, 0);
		if (!draw_chars_tex)
			draw_chars_tex = (LPDIRECTDRAWSURFACE7)Mod_LoadHiResTexture("gfx/2d/bigchars.tga", NULL, false, true, false);	//try q3 path
		if (!draw_chars_tex)
			draw_chars_tex = (LPDIRECTDRAWSURFACE7)Mod_LoadHiResTexture("pics/conchars.pcx", NULL, false, true, false);	//try low res q2 path
		if (!draw_chars_tex)
		{
			extern qbyte default_conchar[11356];
			int width, height;
			int i;
			qbyte *image;

			image = ReadTargaFile(default_conchar, sizeof(default_conchar), &width, &height, false);
			for (i = 0; i < width*height; i++)
			{
				image[i*4+3] = image[i*4];
				image[i*4+0] = 255;
				image[i*4+1] = 255;
				image[i*4+2] = 255;
			}
			draw_chars_tex = D3D7_LoadTexture_32("charset", (void*)image, width, height, TF_NOMIPMAP|TF_ALPHA|TF_NOTBUMPMAP);
		}
	}


	//now emit the conchars picture as if from a wad.
	strcpy(d3dmenu_cachepics[d3dmenu_numcachepics].name, "conchars");
	d3dmenu_cachepics[d3dmenu_numcachepics].pic.width = 128;
	d3dmenu_cachepics[d3dmenu_numcachepics].pic.height = 128;
	*(int *)&d3dmenu_cachepics[d3dmenu_numcachepics].pic.data = (int)draw_chars_tex;
	d3dmenu_numcachepics++;


	conback_tex = D3D7_Draw_SafeCachePic("gfx/conback.lmp");


	Plug_DrawReloadImages();
	D3D7_InitParticleTexture();
}
void	(D3D7_Draw_Init)				(void)
{
	D3D7_Draw_ReInit();
}
void	(D3D7_Draw_Character)			(int x, int y, unsigned int num)
{
	int row;
	int col;
	float frow, fcol, size;

#define char_instep 0
	num &= 255;
	if (num == ' ')
		return;

	row = num>>4;
	col = num&15;

	frow = row*0.0625+char_instep;
	fcol = col*0.0625+char_instep;
	size = 0.0625-char_instep*2;

	d3dquadvert[0].x = x;
	d3dquadvert[0].y = y;
	d3dquadvert[0].z = 0;
	d3dquadvert[0].colour = 0xffffffff;
	d3dquadvert[0].s = fcol;
	d3dquadvert[0].t = frow;

	d3dquadvert[1].x = x+8;
	d3dquadvert[1].y = y;
	d3dquadvert[1].z = 0;
	d3dquadvert[1].colour = 0xffffffff;
	d3dquadvert[1].s = fcol+size;
	d3dquadvert[1].t = frow;

	d3dquadvert[2].x = x+8;
	d3dquadvert[2].y = y+8;
	d3dquadvert[2].z = 0;
	d3dquadvert[2].colour = 0xffffffff;
	d3dquadvert[2].s = fcol+size;
	d3dquadvert[2].t = frow+size;

	d3dquadvert[3].x = x;
	d3dquadvert[3].y = y+8;
	d3dquadvert[3].z = 0;
	d3dquadvert[3].colour = 0xffffffff;
	d3dquadvert[3].s = fcol;
	d3dquadvert[3].t = frow+size;

	pD3DDev->lpVtbl->SetTexture(pD3DDev, 0, draw_chars_tex);

	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_TEXCOORDINDEX, 0);

	pD3DDev->lpVtbl->DrawIndexedPrimitive(pD3DDev, D3DPT_TRIANGLELIST, D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1, d3dquadvert, 4, d3dquadindexes, 6, 0);
}

void	(D3D7_Draw_ColouredCharacter)	(int x, int y, unsigned int num)
{
	int row;
	int col;
	float frow, fcol, size;
	unsigned int imgcolour;
	unsigned char *c = (unsigned char *)&imgcolour;

#define char_instep 0

	if (num&0xffff == ' ')
		return;

	col = (num & CON_FGMASK) >> CON_FGSHIFT;
	c[0] = consolecolours[col].fb*255;
	c[1] = consolecolours[col].fg*255;
	c[2] = consolecolours[col].fr*255;
	c[3] = (num & CON_HALFALPHA)?128:255;


	num &= 0xff;
	row = num>>4;
	col = num&15;

	frow = row*0.0625+char_instep;
	fcol = col*0.0625+char_instep;
	size = 0.0625-char_instep*2;

	d3dquadvert[0].x = x;
	d3dquadvert[0].y = y;
	d3dquadvert[0].z = 0;
	d3dquadvert[0].colour = imgcolour;
	d3dquadvert[0].s = fcol;
	d3dquadvert[0].t = frow;

	d3dquadvert[1].x = x+8;
	d3dquadvert[1].y = y;
	d3dquadvert[1].z = 0;
	d3dquadvert[1].colour = imgcolour;
	d3dquadvert[1].s = fcol+size;
	d3dquadvert[1].t = frow;

	d3dquadvert[2].x = x+8;
	d3dquadvert[2].y = y+8;
	d3dquadvert[2].z = 0;
	d3dquadvert[2].colour = imgcolour;
	d3dquadvert[2].s = fcol+size;
	d3dquadvert[2].t = frow+size;

	d3dquadvert[3].x = x;
	d3dquadvert[3].y = y+8;
	d3dquadvert[3].z = 0;
	d3dquadvert[3].colour = imgcolour;
	d3dquadvert[3].s = fcol;
	d3dquadvert[3].t = frow+size;

	pD3DDev->lpVtbl->SetTexture(pD3DDev, 0, draw_chars_tex);

	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_TEXCOORDINDEX, 0);

	pD3DDev->lpVtbl->DrawIndexedPrimitive(pD3DDev, D3DPT_TRIANGLELIST, D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1, d3dquadvert, 4, d3dquadindexes, 6, 0);
}
void	(D3D7_Draw_String)				(int x, int y, const qbyte *str)
{
	while(*str)
	{
		D3D7_Draw_Character(x, y, *str++);
		x+=8;
	}
}
void	(D3D7_Draw_Alt_String)			(int x, int y, const qbyte *str)
{
	while(*str)
	{
		D3D7_Draw_Character(x, y, *str++ | 128);
		x+=8;
	}
}
void	(D3D7_Draw_Crosshair)			(void)
{
	D3D7_Draw_Character(vid.width/2 - 4, vid.height/2 - 4, '+');
}
void	(D3D7_Draw_DebugChar)			(qbyte num)
{
	Sys_Error("D3D function not implemented\n");
}
void	(D3D7_Draw_TransPicTranslate)	(int x, int y, int w, int h, qbyte *pic, qbyte *translation)
{
//	Sys_Error("D3D function not implemented\n");
}
void	(D3D7_Draw_TileClear)			(int x, int y, int w, int h)
{
//	Sys_Error("D3D function not implemented\n");
}
void	(D3D7_Draw_Fill_I)				(int x, int y, int w, int h, unsigned int imgcolour)
{
	d3dquadvert[0].x = x;
	d3dquadvert[0].y = y;
	d3dquadvert[0].z = 0;
	d3dquadvert[0].colour = imgcolour;
	d3dquadvert[0].s = 0;
	d3dquadvert[0].t = 0;

	d3dquadvert[1].x = x+w;
	d3dquadvert[1].y = y;
	d3dquadvert[1].z = 0;
	d3dquadvert[1].colour = imgcolour;
	d3dquadvert[1].s = 0;
	d3dquadvert[1].t = 0;

	d3dquadvert[2].x = x+w;
	d3dquadvert[2].y = y+h;
	d3dquadvert[2].z = 0;
	d3dquadvert[2].colour = imgcolour;
	d3dquadvert[2].s = 0;
	d3dquadvert[2].t = 0;

	d3dquadvert[3].x = x;
	d3dquadvert[3].y = y+h;
	d3dquadvert[3].z = 0;
	d3dquadvert[3].colour = imgcolour;
	d3dquadvert[3].s = 0;
	d3dquadvert[3].t = 0;

	pD3DDev->lpVtbl->SetTexture(pD3DDev, 0, NULL);

	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);

	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, FALSE );

	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHABLENDENABLE, TRUE );
	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_SRCBLEND,  D3DBLEND_SRCALPHA);
	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_DESTBLEND,  D3DBLEND_INVSRCALPHA);

	pD3DDev->lpVtbl->DrawIndexedPrimitive(pD3DDev, D3DPT_TRIANGLELIST, D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1, d3dquadvert, 4, d3dquadindexes, 6, 0);

	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHABLENDENABLE, FALSE );
	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, TRUE );

	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
}

void	(D3D7_Draw_FillRGB)				(int x, int y, int w, int h, float r, float g, float b)
{
	char colours[4];
	colours[0] = b*255;
	colours[1] = g*255;
	colours[2] = r*255;
	colours[3] = 255;

	D3D7_Draw_Fill_I(x, y, w, h, *(unsigned int*)colours);
}

void	(D3D7_Draw_Fill)				(int x, int y, int w, int h, unsigned int c)
{
	D3D7_Draw_FillRGB(x, y, w, h, host_basepal[c*3+0]/255.0f, host_basepal[c*3+1]/255.0f, host_basepal[c*3+2]/255.0f);
}
void	(D3D7_Draw_FadeScreen)			(void)
{
//	Sys_Error("D3D function not implemented\n");
}
void	(D3D7_Draw_BeginDisc)			(void)
{
//	Sys_Error("D3D function not implemented\n");
}
void	(D3D7_Draw_EndDisc)				(void)
{
//	Sys_Error("D3D function not implemented\n");
}

static int imgcolour;

void	(D3D7_Draw_Fill_Colours)				(int x, int y, int w, int h)
{
	D3D7_Draw_Fill_I(x, y, w, h, imgcolour);
}

void	(D3D7_Draw_Image)				(float x, float y, float w, float h, float s1, float t1, float s2, float t2, mpic_t *pic)
{
	LPDIRECTDRAWSURFACE7 *p;
	if (!conback_tex)
		return;
	if (!pic)
		return;

	d3dquadvert[0].x = x;
	d3dquadvert[0].y = y;
	d3dquadvert[0].z = 0;
	d3dquadvert[0].colour = imgcolour;
	d3dquadvert[0].s = s1;// - 3.0/pic->width;
	d3dquadvert[0].t = t1;

	d3dquadvert[1].x = x+w;
	d3dquadvert[1].y = y;
	d3dquadvert[1].z = 0;
	d3dquadvert[1].colour = imgcolour;
	d3dquadvert[1].s = s2;// - 3.0/pic->width;
	d3dquadvert[1].t = t1;

	d3dquadvert[2].x = x+w;
	d3dquadvert[2].y = y+h;
	d3dquadvert[2].z = 0;
	d3dquadvert[2].colour = imgcolour;
	d3dquadvert[2].s = s2;// - 3.0/pic->width;
	d3dquadvert[2].t = t2;

	d3dquadvert[3].x = x;
	d3dquadvert[3].y = y+h;
	d3dquadvert[3].z = 0;
	d3dquadvert[3].colour = imgcolour;
	d3dquadvert[3].s = s1;// - 3.0/pic->width;
	d3dquadvert[3].t = t2;

	p = (LPDIRECTDRAWSURFACE7*)&pic->data;
	pD3DDev->lpVtbl->SetTexture(pD3DDev, 0, *p);

	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_TEXCOORDINDEX, 0);

	pD3DDev->lpVtbl->DrawIndexedPrimitive(pD3DDev, D3DPT_TRIANGLELIST, D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1, d3dquadvert, 4, d3dquadindexes, 6, 0);
}

void	(D3D7_Draw_ScalePic)			(int x, int y, int width, int height, mpic_t *pic)
{
	D3D7_Draw_Image(x, y, width, height, 0, 0, 1, 1, pic);
}
void	(D3D7_Draw_SubPic)				(int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height)
{
	float s, t;
	float sw, tw;
	if (!pic)
		return;

	s = (float)srcx/pic->width;
	t = (float)srcy/pic->height;
	sw = (float)width/pic->width;
	tw = (float)height/pic->height;
	D3D7_Draw_Image(x, y, width, height, s, t, s+sw, t+tw, pic);
}
void	(D3D7_Draw_TransPic)			(int x, int y, mpic_t *pic)
{
	if (!pic)
		return;
	D3D7_Draw_Image(x, y, pic->width, pic->height, 0, 0, 1, 1, pic);
}
void	(D3D7_Draw_Pic)					(int x, int y, mpic_t *pic)
{
	if (!pic)
		return;
	D3D7_Draw_Image(x, y, pic->width, pic->height, 0, 0, 1, 1, pic);
}
void	(D3D7_Draw_ImageColours)		(float r, float g, float b, float a)
{
	unsigned char *c = (unsigned char *)&imgcolour;
	
	c[0] = b*255;
	c[1] = g*255;
	c[2] = r*255;
	c[3] = a*255;
}

void	(D3D7_Draw_ConsoleBackground)	(int lines)
{
	D3D7_Draw_ImageColours(1,1,1,1);
	D3D7_Draw_Image(0, 0, vid.width, lines, 0, 1 - (float)lines/vid.height, 1, 1, conback_tex);
}
void	(D3D7_Draw_EditorBackground)	(int lines)
{
	D3D7_Draw_ConsoleBackground(lines);
}




void (D3D7_Media_ShowFrameBGR_24_Flip)	(qbyte *framedata, int inwidth, int inheight)
{
	mpic_t pic;
	LPDIRECTDRAWSURFACE7 *p;
	p = (LPDIRECTDRAWSURFACE7*)&pic.data;
	*p = D3D7_LoadTexture_32("", (unsigned int*)framedata, inwidth, inheight, TF_NOMIPMAP|TF_NOALPHA|TF_NOTBUMPMAP);

	D3D7_Set2D ();
	D3D7_Draw_ImageColours(1,1,1,1);
	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, FALSE );
	D3D7_Draw_Image(0, 0, vid.width, vid.height, 0, 1, 1, 0, &pic);
	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, TRUE );

	D3D7_UnloadTexture(*p);
}	//input is bottom up...
void (D3D7_Media_ShowFrameRGBA_32)		(qbyte *framedata, int inwidth, int inheight)
{
	mpic_t pic;
	LPDIRECTDRAWSURFACE7 *p;

	pic.width = inwidth;
	pic.height = inheight;
	pic.flags = 0;
	p = (LPDIRECTDRAWSURFACE7*)&pic.data;
	*p = D3D7_LoadTexture_32("", (unsigned int*)framedata, inwidth, inheight, TF_NOMIPMAP|TF_NOALPHA|TF_NOTBUMPMAP);

	D3D7_Set2D ();
	D3D7_Draw_ImageColours(1,1,1,1);
	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, FALSE );
	D3D7_Draw_Image(0, 0, vid.width, vid.height, 0, 0, 1, 1, &pic);
	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, TRUE );

	D3D7_UnloadTexture(*p);
}	//top down
void (D3D7_Media_ShowFrame8bit)			(qbyte *framedata, int inwidth, int inheight, qbyte *palette)
{
	mpic_t pic;
	LPDIRECTDRAWSURFACE7 *p;
	p = (LPDIRECTDRAWSURFACE7*)&pic.data;
	*p = D3D7_LoadTexture_8_Pal24("", (unsigned char*)framedata, inwidth, inheight, TF_NOMIPMAP|TF_NOALPHA|TF_NOTBUMPMAP, palette, 256);

	D3D7_Set2D ();
	D3D7_Draw_ImageColours(1,1,1,1);
	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, FALSE );
	D3D7_Draw_Image(0, 0, vid.width, vid.height, 0, 1, 1, 0, &pic);
	pD3DDev->lpVtbl->SetRenderState(pD3DDev, D3DRENDERSTATE_ALPHATESTENABLE, TRUE );

	D3D7_UnloadTexture(*p);
}	//paletted topdown (framedata is 8bit indexes into palette)
#endif
