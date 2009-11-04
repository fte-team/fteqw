#include "quakedef.h"

#include <d3d9.h>
LPDIRECT3DDEVICE9 pD3DDev9;



extern cvar_t gl_picmip;
extern cvar_t gl_picmip2d;

texid_t D3D_AllocNewTexture(int width, int height)
{
	return r_nulltex;
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

static void Upload_Texture_32(LPDIRECT3DTEXTURE9 tex, unsigned int *data, int width, int height)
{
	int x, y;
	unsigned int *dest;
	unsigned char swapbuf[4];
	unsigned char swapbuf2[4];
	D3DLOCKED_RECT lock;

	D3DSURFACE_DESC desc;
	IDirect3DTexture9_GetLevelDesc(tex, 0, &desc);

	IDirect3DTexture9_LockRect(tex, 0, &lock, NULL, DDLOCK_NOSYSLOCK|D3DLOCK_READONLY);

	if (width == desc.Width && height == desc.Height)
	{
//		if (desc.lPitch == twidth*4)
//		{
//			memcpy(desc.lpSurface, data, width*height*4);
//		}
//		else
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



		//mimic opengl and draw it white
//		memset(desc.lpSurface, 255, twidth*theight*4);
	}

	IDirect3DTexture9_UnlockRect(tex, 0);
}

//create a basic shader from a 32bit image
static LPDIRECT3DBASETEXTURE9 D3D9_LoadTexture_32(char *name, unsigned int *data, int width, int height, int flags)
{
	int nwidth, nheight;

	LPDIRECT3DTEXTURE9 newsurf;
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

	IDirect3DDevice9_CreateTexture(pD3DDev9, nwidth, nheight, 0, ((flags & IF_NOMIPMAP)?0:D3DUSAGE_AUTOGENMIPMAP), D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &newsurf, NULL);

	if (!newsurf)
		return NULL;

	Upload_Texture_32(newsurf, data, width, height);

	if (!(flags & IF_NOMIPMAP))
		IDirect3DBaseTexture9_GenerateMipSubLevels(newsurf);

	return (LPDIRECT3DBASETEXTURE9)newsurf;
}

static LPDIRECT3DBASETEXTURE9 D3D9_LoadTexture_8(char *name, unsigned char *data, int width, int height, int flags, enum uploadfmt fmt)
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
	if ((fmt!=TF_SOLID8) && !(flags & IF_NOALPHA))
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
				trans[i] = d_8to24rgbtable[p];
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
			trans[i] = d_8to24rgbtable[data[i]];
			trans[i+1] = d_8to24rgbtable[data[i+1]];
			trans[i+2] = d_8to24rgbtable[data[i+2]];
			trans[i+3] = d_8to24rgbtable[data[i+3]];
		}
		for (i=s&~3 ; i<s ; i++)	//wow, funky
		{
			trans[i] = d_8to24rgbtable[data[i]];
		}
	}
	return D3D9_LoadTexture_32(name, trans, width, height, flags);
}

void D3D_UploadFmt(texid_t tex, char *name, enum uploadfmt fmt, void *data, int width, int height, unsigned int flags)
{
	switch (fmt)
	{
	case TF_RGBA32:
	default:
		break;
	}
}

texid_t D3D_LoadTextureFmt (char *identifier, int width, int height, enum uploadfmt fmt, void *data, unsigned int flags)
{
	texid_t tid;
	switch (fmt)
	{
	case TF_SOLID8:
	case TF_TRANS8:
	case TF_H2_T7G1:
	case TF_H2_TRANS8_0:
	case TF_H2_T4A4:
		tid.ptr = D3D9_LoadTexture_8(identifier, data, width, height, flags, fmt);
		return tid;
	case TF_RGBA32:
		tid.ptr = D3D9_LoadTexture_32(identifier, data, width, height, flags);
		return tid;
	default:
		return r_nulltex;
	}
}

texid_t D3D_LoadCompressed(char *name)
{
	return r_nulltex;
}

texid_t D3D_FindTexture (char *identifier)
{
	return r_nulltex;
}
