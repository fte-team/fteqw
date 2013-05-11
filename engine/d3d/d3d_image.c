#include "quakedef.h"
#include "winquake.h"
#ifdef D3D9QUAKE
#if !defined(HMONITOR_DECLARED) && (WINVER < 0x0500)
    #define HMONITOR_DECLARED
    DECLARE_HANDLE(HMONITOR);
#endif
#include <d3d9.h>
extern LPDIRECT3DDEVICE9 pD3DDev9;

typedef struct d3dtexture_s
{
	texcom_t com;
	struct d3dtexture_s *next;
	texid_t tex;
	char name[1];
} d3dtexture_t;
static d3dtexture_t *d3dtextures;

void D3D9_Image_Shutdown(void)
{
	LPDIRECT3DTEXTURE9 tx;
	while(d3dtextures)
	{
		d3dtexture_t *t = d3dtextures;
		d3dtextures = t->next;

		tx = t->tex.ptr;
		if (tx)
			IDirect3DTexture9_Release(tx);

		free(t);
	}
}

static d3dtexture_t *d3d_lookup_texture(char *ident)
{
	d3dtexture_t *tex;

	if (*ident)
	{
		for (tex = d3dtextures; tex; tex = tex->next)
			if (!strcmp(tex->name, ident))
				return tex;
	}

	tex = calloc(1, sizeof(*tex)+strlen(ident));
	strcpy(tex->name, ident);
	tex->tex.ptr = NULL;
	tex->tex.ref = &tex->com;
	tex->next = d3dtextures;
	d3dtextures = tex;

	return tex;
}

extern cvar_t gl_picmip;
extern cvar_t gl_picmip2d;

texid_t D3D9_AllocNewTexture(char *ident, int width, int height, unsigned int flags)
{
	IDirect3DTexture9 *tx;
	texid_t ret = r_nulltex;
	if (!FAILED(IDirect3DDevice9_CreateTexture(pD3DDev9, width, height, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tx, NULL)))
		ret.ptr = tx;
	return ret;
}

void    D3D9_DestroyTexture (texid_t tex)
{
	IDirect3DTexture9 *tx = tex.ptr;
	if (tx)
		IDirect3DTexture9_Release(tx);
}

static void D3D9_RoundDimensions(int *scaled_width, int *scaled_height, qboolean mipmap)
{
//	if (gl_config.arb_texture_non_power_of_two)	//NPOT is a simple extension that relaxes errors.
//	{
//		TRACE(("dbg: GL_RoundDimensions: GL_ARB_texture_non_power_of_two\n"));
//	}
//	else
	{
		int width = *scaled_width;
		int height = *scaled_height;
		for (*scaled_width = 1 ; *scaled_width < width ; *scaled_width<<=1)
			;
		for (*scaled_height = 1 ; *scaled_height < height ; *scaled_height<<=1)
			;
	}

	if (mipmap)
	{
		TRACE(("dbg: GL_RoundDimensions: %i\n", gl_picmip.ival));
		*scaled_width >>= gl_picmip.ival;
		*scaled_height >>= gl_picmip.ival;
	}
	else
	{
		*scaled_width >>= gl_picmip2d.ival;
		*scaled_height >>= gl_picmip2d.ival;
	}

	TRACE(("dbg: GL_RoundDimensions: %i\n", gl_max_size.ival));
	if (gl_max_size.ival)
	{
		if (*scaled_width > gl_max_size.ival)
			*scaled_width = gl_max_size.ival;
		if (*scaled_height > gl_max_size.ival)
			*scaled_height = gl_max_size.ival;
	}

	if (*scaled_width < 1)
		*scaled_width = 1;
	if (*scaled_height < 1)
		*scaled_height = 1;
}

#if 0
static void D3D_MipMap (qbyte *out, int outwidth, int outheight, qbyte *in, int inwidth, int inheight)
{
        int             i, j;
        qbyte   *inrow;

        //with npot
        int rowwidth = inwidth*4; //rowwidth is the byte width of the input
        inrow = in;

        for (i=0 ; i<outheight ; i++, inrow+=rowwidth*2)
        {
                for (in = inrow, j=0 ; j<outwidth ; j++, out+=4, in+=8)
                {
                        out[0] = (in[0] + in[4] + in[rowwidth+0] + in[rowwidth+4])>>2;
                        out[1] = (in[1] + in[5] + in[rowwidth+1] + in[rowwidth+5])>>2;
                        out[2] = (in[2] + in[6] + in[rowwidth+2] + in[rowwidth+6])>>2;
                        out[3] = (in[3] + in[7] + in[rowwidth+3] + in[rowwidth+7])>>2;
                }
        }
}
#endif

static void Upload_Texture_32(LPDIRECT3DTEXTURE9 tex, unsigned int *data, int width, int height, unsigned int flags)
{
	int x, y;
	unsigned int *dest;
	unsigned char swapbuf[4];
	unsigned char swapbuf2[4];
	D3DLOCKED_RECT lock;

	D3DSURFACE_DESC desc;
	IDirect3DTexture9_GetLevelDesc(tex, 0, &desc);

	IDirect3DTexture9_LockRect(tex, 0, &lock, NULL, D3DLOCK_NOSYSLOCK);

	if (width == desc.Width && height == desc.Height)
	{
		for (y = 0; y < height; y++)
		{
			dest = (unsigned int *)((char *)lock.pBits + lock.Pitch*y);
			for (x = 0; x < width; x++)
			{
				*(unsigned int*)swapbuf2 = *(unsigned int*)swapbuf = data[x];
				swapbuf[0] = swapbuf2[2];
				swapbuf[2] = swapbuf2[0];
				dest[x] = *(unsigned int*)swapbuf;
			}
			data += width;
		}
	}
	else
	{
		int x, y;
		int iny;
		unsigned int *row, *inrow;

		for (y = 0; y < desc.Height; y++)
		{
			row = (unsigned int*)((char *)lock.pBits + lock.Pitch*y);
			iny = (y * height) / desc.Height;
			inrow = data + width*iny;
			for (x = 0; x < desc.Width; x++)
			{
				*(unsigned int*)swapbuf2 = *(unsigned int*)swapbuf =  inrow[(x * width)/desc.Width];
				swapbuf[0] = swapbuf2[2];
				swapbuf[2] = swapbuf2[0];
				row[x] = *(unsigned int*)swapbuf;
			}
		}
	}

#if 0 //D3DUSAGE_AUTOGENMIPMAP so this isn't needed
	if (!(flags & IF_NOMIPMAP))
	{
		int max = IDirect3DTexture9_GetLevelCount(tex);
		for (i = 1; i < max; i++)
		{
			width = desc.Width;
			height = desc.Height;
			data = lock.pBits;
			IDirect3DTexture9_LockRect(tex, i, &lock, NULL, D3DLOCK_NOSYSLOCK|D3DLOCK_DISCARD);
		        IDirect3DTexture9_GetLevelDesc(tex, i, &desc);
			D3D_MipMap(lock.pBits, desc.Width, desc.Height, data, width, height);
			IDirect3DTexture9_UnlockRect(tex, i-1);
		}
		IDirect3DTexture9_UnlockRect(tex, i-1);
	}
	else
#endif
		IDirect3DTexture9_UnlockRect(tex, 0);
}

//create a basic shader from a 32bit image
static void D3D9_LoadTexture_32(d3dtexture_t *tex, unsigned int *data, int width, int height, int flags)
{
	int nwidth, nheight;

/*
	if (!(flags & TF_MANDATORY))
	{
		Con_Printf("Texture upload missing flags\n");
		return NULL;
	}
*/

	nwidth = width;
	nheight = height;
	D3D9_RoundDimensions(&nwidth, &nheight, !(flags & IF_NOMIPMAP));

	if (!tex->tex.ptr)
	{
		LPDIRECT3DTEXTURE9 newsurf;
		IDirect3DDevice9_CreateTexture(pD3DDev9, nwidth, nheight, (flags & IF_NOMIPMAP)?1:0, ((flags & IF_NOMIPMAP)?0:D3DUSAGE_AUTOGENMIPMAP), D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &newsurf, NULL);
		if (!newsurf)
			return;
		tex->tex.ptr = newsurf;
	}

	tex->com.width = width;
	tex->com.height = height;
	Upload_Texture_32(tex->tex.ptr, data, width, height, flags);
}

static void D3D9_LoadTexture_8(d3dtexture_t *tex, unsigned char *data, unsigned int *pal32, int width, int height, int flags, enum uploadfmt fmt)
{
	static unsigned	trans[1024*1024];
	int			i, s;
	qboolean	noalpha;
	int			p;

	if (width*height > 1024*1024)
		Sys_Error("GL_Upload8: image too big (%i*%i)", width, height);

	s = width*height;
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (fmt == TF_TRANS8_FULLBRIGHT)
	{
		for (i=0 ; i<s ; i++)
		{
			p = data[i];
			noalpha = true;
			if (p > 255-vid.fullbright)
				trans[i] = pal32[p];
			else
			{
				noalpha = false;
				trans[i] = 0;
			}
		}
	}
	else if ((fmt!=TF_SOLID8) && !(flags & IF_NOALPHA))
	{
		noalpha = true;
		for (i=0 ; i<s ; i++)
		{
			p = data[i];
			if (p == 255)
			{
				noalpha = false;
				trans[i] = 0;
			}
			else
				trans[i] = pal32[p];
		}

		switch(fmt)
		{
		default:
			if (noalpha)
				fmt = TF_SOLID8;
			break;
		case TF_H2_T7G1:
			fmt = TF_TRANS8;
			for (i=0 ; i<s ; i++)
			{
				p = data[i];
				if (p == 0)
					trans[i] &= 0x00ffffff;
				else if( p & 1 )
				{
					trans[i] &= 0x00ffffff;
					trans[i] |= ( ( int )( 255 * 0.5 ) ) << 24;
				}
				else
				{
					trans[i] |= 0xff000000;
				}
			}
			break;
		case TF_H2_TRANS8_0:
			fmt = TF_TRANS8;
			for (i=0 ; i<s ; i++)
			{
				p = data[i];
				if (p == 0)
					trans[i] &= 0x00ffffff;
			}
			break;
/*		case TF_H2_T4A4:
			fmt = TF_TRANS8;
			for (i=0 ; i<s ; i++)
			{
				p = data[i];
				trans[i] = d_8to24rgbtable[ColorIndex[p>>4]] & 0x00ffffff;
				trans[i] |= ( int )ColorPercent[p&15] << 24;
				//trans[i] = 0x7fff0000;
			}
			break;
*/
		}
	}
	else
	{
		for (i=(s&~3)-4 ; i>=0 ; i-=4)
		{
			trans[i] = pal32[data[i]];
			trans[i+1] = pal32[data[i+1]];
			trans[i+2] = pal32[data[i+2]];
			trans[i+3] = pal32[data[i+3]];
		}
		for (i=s&~3 ; i<s ; i++)	//wow, funky
		{
			trans[i] = pal32[data[i]];
		}
	}
	D3D9_LoadTexture_32(tex, trans, width, height, flags);
}

void    D3D9_Upload (texid_t tex, char *name, enum uploadfmt fmt, void *data, void *palette, int width, int height, unsigned int flags)
{
	switch (fmt)
	{
	case TF_RGBX32:
		flags |= IF_NOALPHA;
		//fall through
	case TF_RGBA32:
		tex.ref->width = width;
		tex.ref->height = height;
		Upload_Texture_32(tex.ptr, data, width, height, flags);
		break;
	default:
		OutputDebugString(va("D3D9_LoadTextureFmt doesn't support fmt %i (%s)", fmt, name));
		break;
	}
}

texid_t D3D9_LoadTexture (char *identifier, int width, int height, enum uploadfmt fmt, void *data, unsigned int flags)
{
	d3dtexture_t *tex;
	switch (fmt)
	{
	case TF_TRANS8_FULLBRIGHT:
		{
			qbyte *d = data;
			unsigned int c = width * height;
			while (c)
			{
				if (d[--c] > 255 - vid.fullbright)
					break;
			}
			/*reject it if there's no fullbrights*/
			if (!c)
				return r_nulltex;
		}
		break;
	case TF_INVALID:
	case TF_RGBA32:
	case TF_BGRA32:
	case TF_RGBX32:
	case TF_RGB24:
	case TF_BGR24_FLIP:
	case TF_SOLID8:
	case TF_TRANS8:
	case TF_HEIGHT8:
	case TF_HEIGHT8PAL:
	case TF_H2_T7G1:
	case TF_H2_TRANS8_0:
	case TF_H2_T4A4:
	case TF_PALETTES:
	case TF_8PAL24:
	case TF_8PAL32:
		break;

	}
	tex = d3d_lookup_texture(identifier);

	switch (fmt)
	{
	case TF_SOLID8:
	case TF_TRANS8:
	case TF_H2_T7G1:
	case TF_H2_TRANS8_0:
	case TF_H2_T4A4:
	case TF_TRANS8_FULLBRIGHT:
		D3D9_LoadTexture_8(tex, data, d_8to24rgbtable, width, height, flags, fmt);
		return tex->tex;
	case TF_RGBX32:
		flags |= IF_NOALPHA;
	case TF_RGBA32:
		D3D9_LoadTexture_32(tex, data, width, height, flags);
		return tex->tex;
	default:
		OutputDebugString(va("D3D9_LoadTexture doesn't support fmt %i", fmt));
		return r_nulltex;
	}
}

texid_t D3D9_LoadCompressed (char *name)
{
	return r_nulltex;
}

texid_t D3D9_FindTexture (char *identifier, unsigned int flags)
{
	d3dtexture_t *tex = d3d_lookup_texture(identifier);
	if (tex->tex.ptr)
		return tex->tex;
	return r_nulltex;
}

texid_t D3D9_LoadTexture8Pal32 (char *identifier, int width, int height, qbyte *data, qbyte *palette32, unsigned int flags)
{
	d3dtexture_t *tex = d3d_lookup_texture(identifier);
	D3D9_LoadTexture_8(tex, data, (unsigned int *)palette32, width, height, flags, TF_SOLID8);
	return tex->tex;
}
texid_t D3D9_LoadTexture8Pal24 (char *identifier, int width, int height, qbyte *data, qbyte *palette24, unsigned int flags)
{
	unsigned int pal32[256];
	int i;
	for (i = 0; i < 256; i++)
	{
		pal32[i] = 0x00000000 |
				(palette24[i*3+2]<<24) |
				(palette24[i*3+1]<<8) |
				(palette24[i*3+0]<<0);
	}
	return D3D9_LoadTexture8Pal32(identifier, width, height, data, (qbyte*)pal32, flags);
}

#endif
