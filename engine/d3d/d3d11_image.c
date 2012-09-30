#include "quakedef.h"
#ifdef D3D11QUAKE
#include "winquake.h"
#define COBJMACROS
#include <d3d11.h>
extern ID3D11Device *pD3DDev11;
extern ID3D11DeviceContext *d3ddevctx;

typedef struct d3dtexture_s
{
	texcom_t com;
	ID3D11ShaderResourceView *view;
	struct d3dtexture_s *prev;
	struct d3dtexture_s *next;
	ID3D11Texture2D *tex2d;
	char name[1];
} d3d11texture_t;
static d3d11texture_t *d3d11textures;

void D3D11_Image_Shutdown(void)
{
	//destroy all named textures
	while(d3d11textures)
	{
		d3d11texture_t *t = d3d11textures;
		d3d11textures = t->next;

		if (t->view)
			ID3D11ShaderResourceView_Release(t->view);
		if (t->tex2d)
			ID3D11Texture2D_Release(t->tex2d);

		free(t);
	}
}

void    D3D11_DestroyTexture (texid_t tex)
{
	d3d11texture_t *t = (d3d11texture_t*)tex.ref;

	ID3D11Texture2D *tx = tex.ptr;
	if (t->view)
		ID3D11ShaderResourceView_Release(t->view);
	if (t->tex2d)
		ID3D11Texture2D_Release(t->tex2d);
	t->view = NULL;
	t->tex2d = NULL;

	if (t->prev)
		t->prev->next = t->next;
	else
		d3d11textures = t->next;
	if (t->next)
		t->next->prev = t->prev;
	t->prev = NULL;
	t->next = NULL;
	free(t);
}

static d3d11texture_t *d3d_lookup_texture(char *ident)
{
	d3d11texture_t *tex;

	if (*ident)
	{
		for (tex = d3d11textures; tex; tex = tex->next)
			if (!strcmp(tex->name, ident))
				return tex;
	}

	tex = calloc(1, sizeof(*tex)+strlen(ident));
	strcpy(tex->name, ident);
	tex->view = NULL;
	tex->tex2d = NULL;
	tex->next = d3d11textures;
	tex->prev = NULL;
	d3d11textures = tex;
	if (tex->next)
		tex->next->prev = tex;

	return tex;
}

extern cvar_t gl_picmip;
extern cvar_t gl_picmip2d;

static texid_t ToTexID(d3d11texture_t *tex)
{
	texid_t tid;
	tid.ref = &tex->com;
	if (!tex->view)
		ID3D11Device_CreateShaderResourceView(pD3DDev11, (ID3D11Resource *)tex->tex2d, NULL, &tex->view);
	tid.ptr = tex->view;
	return tid;
}

static void *D3D11_AllocNewTextureData(void *datargba, int width, int height, unsigned int flags)
{
	HRESULT hr;
	ID3D11Texture2D *tx = NULL;
	D3D11_TEXTURE2D_DESC tdesc = {0};
	D3D11_SUBRESOURCE_DATA subresdesc = {0};

	subresdesc.pSysMem = datargba;
	subresdesc.SysMemPitch = width*4;
	subresdesc.SysMemSlicePitch = width*height*4;

	tdesc.Width = width;
	tdesc.Height = height;
	tdesc.MipLevels = 1;
	tdesc.ArraySize = 1;
	tdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	tdesc.SampleDesc.Count = 1;
	tdesc.SampleDesc.Quality = 0;
	tdesc.Usage = datargba?D3D11_USAGE_DEFAULT:D3D11_USAGE_DYNAMIC;
	tdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	tdesc.CPUAccessFlags = 0;
	tdesc.MiscFlags = 0;
	hr = ID3D11Device_CreateTexture2D(pD3DDev11, &tdesc, (datargba?&subresdesc:NULL), &tx);
	if (FAILED(hr))
	{
		tx = NULL;
	}
	return tx;
}
texid_t D3D11_AllocNewTexture(char *ident, int width, int height, unsigned int flags)
{
	void *img = D3D11_AllocNewTextureData(NULL, width, height, flags);
	d3d11texture_t *t = d3d_lookup_texture("");
	t->tex2d = img;
	
	return ToTexID(t);
}

static void D3D11_RoundDimensions(int *scaled_width, int *scaled_height, qboolean mipmap)
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

static void Upload_Texture_32(ID3D11Texture2D *tex, unsigned int *data, int width, int height, unsigned int flags)
{
//	int x, y;
//	unsigned int *dest;
//	unsigned char swapbuf[4];
//	unsigned char swapbuf2[4];
//	D3D11_MAPPED_SUBRESOURCE lock;

	D3D11_TEXTURE2D_DESC desc;

	ID3D11Texture2D_GetDesc(tex, &desc);
	if (width == desc.Width && height == desc.Height)
	{
		ID3D11DeviceContext_UpdateSubresource(d3ddevctx, (ID3D11Resource*)tex, 0, NULL, data, width*4, width*height*4);
		return;
	}

	Con_Printf("Wrong size!\n");
	return;
#if 0
	if (FAILED(ID3D11DeviceContext_Map(d3ddevctx, (ID3D11Resource*)tex, 0, D3D11_MAP_WRITE_DISCARD, 0, &lock)))
	{
		Con_Printf("Dynamic texture update failed\n");
		return;
	}

	if (width == desc.Width && height == desc.Height)
	{
		for (y = 0; y < height; y++)
		{
			dest = (unsigned int *)((char *)lock.pData + lock.RowPitch*y);
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
			row = (unsigned int*)((char *)lock.pData + lock.RowPitch*y);
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

	ID3D11DeviceContext_Unmap(d3ddevctx, (ID3D11Resource*)tex, 0);
#endif
}

//create a basic shader from a 32bit image
static void D3D11_LoadTexture_32(d3d11texture_t *tex, unsigned int *data, int width, int height, int flags)
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
	D3D11_RoundDimensions(&nwidth, &nheight, !(flags & IF_NOMIPMAP));

	if (!tex->tex2d)
	{
		tex->tex2d = D3D11_AllocNewTextureData(data, width, height, flags);
		return;
	}

	Upload_Texture_32(tex->tex2d, data, width, height, flags);
}

static void D3D11_LoadTexture_8(d3d11texture_t *tex, unsigned char *data, unsigned int *pal32, int width, int height, int flags, enum uploadfmt fmt)
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
	D3D11_LoadTexture_32(tex, trans, width, height, flags);
}

void    D3D11_Upload (texid_t tex, char *name, enum uploadfmt fmt, void *data, void *palette, int width, int height, unsigned int flags)
{
	switch (fmt)
	{
	case TF_RGBX32:
		flags |= IF_NOALPHA;
		//fall through
	case TF_RGBA32:
		Upload_Texture_32(tex.ptr, data, width, height, flags);
		break;
	case TF_TRANS8:
		OutputDebugString(va("D3D11_LoadTextureFmt doesn't support fmt TF_TRANS8 (%s)\n", fmt, name));
		break;
	default:
		OutputDebugString(va("D3D11_LoadTextureFmt doesn't support fmt %i (%s)\n", fmt, name));
		break;
	}
}

void D3D11_UploadLightmap(lightmapinfo_t *lm)
{
	d3d11texture_t *tex;
	lm->modified = false;
	if (!TEXVALID(lm->lightmap_texture))
	{
		lm->lightmap_texture = R_AllocNewTexture("***lightmap***", LMBLOCK_WIDTH, LMBLOCK_HEIGHT, 0);
		if (!lm->lightmap_texture.ref)
			return;
	}
	tex = (d3d11texture_t*)lm->lightmap_texture.ref;

	if (!tex->tex2d)
		tex->tex2d = D3D11_AllocNewTextureData(lm->lightmaps, lm->width, lm->height, 0);
	else
	{
		if (tex->view)
		{
			ID3D11ShaderResourceView_Release(tex->view);
			tex->view = NULL;
		}
		Upload_Texture_32(tex->tex2d, (void*)lm->lightmaps, lm->width, lm->height, 0);
	}

	lm->lightmap_texture = ToTexID(tex);
}

texid_t D3D11_LoadTexture (char *identifier, int width, int height, enum uploadfmt fmt, void *data, unsigned int flags)
{
	d3d11texture_t *tex;
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
		D3D11_LoadTexture_8(tex, data, d_8to24rgbtable, width, height, flags, fmt);
		return ToTexID(tex);
	case TF_RGBX32:
		flags |= IF_NOALPHA;
	case TF_RGBA32:
		D3D11_LoadTexture_32(tex, data, width, height, flags);
		return ToTexID(tex);
	case TF_HEIGHT8PAL:
		OutputDebugString(va("D3D11_LoadTexture doesn't support fmt TF_HEIGHT8PAL (%s)\n", identifier));
		return r_nulltex;
	default:
		OutputDebugString(va("D3D11_LoadTexture doesn't support fmt %i (%s)\n", fmt, identifier));
		return r_nulltex;
	}
}

texid_t D3D11_LoadCompressed (char *name)
{
	return r_nulltex;
}

texid_t D3D11_FindTexture (char *identifier, unsigned int flags)
{
	d3d11texture_t *tex = d3d_lookup_texture(identifier);
	if (tex->tex2d)
		return ToTexID(tex);
	return r_nulltex;
}

texid_t D3D11_LoadTexture8Pal32 (char *identifier, int width, int height, qbyte *data, qbyte *palette32, unsigned int flags)
{
	d3d11texture_t *tex = d3d_lookup_texture(identifier);
	D3D11_LoadTexture_8(tex, data, (unsigned int *)palette32, width, height, flags, TF_SOLID8);
	return ToTexID(tex);
}
texid_t D3D11_LoadTexture8Pal24 (char *identifier, int width, int height, qbyte *data, qbyte *palette24, unsigned int flags)
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
	return D3D11_LoadTexture8Pal32(identifier, width, height, data, (qbyte*)pal32, flags);
}

#endif
