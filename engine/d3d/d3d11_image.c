#include "quakedef.h"
#ifdef D3D11QUAKE
#include "winquake.h"
#define COBJMACROS
#include <d3d11.h>
extern ID3D11Device *pD3DDev11;
extern ID3D11DeviceContext *d3ddevctx;

//FIXME: need support for cubemaps.

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

ID3D11ShaderResourceView *D3D11_Image_View(const texid_t *id)
{
	d3d11texture_t *info = (d3d11texture_t*)id->ref;
	if (!info)
		return NULL;
	if (!info->view)
		ID3D11Device_CreateShaderResourceView(pD3DDev11, (ID3D11Resource *)info->tex2d, NULL, &info->view);
	return info->view;
}

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

	if (!t)
		return;

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
	tid.ptr = NULL;//(void*)0xdeadbeef;
	return tid;
}

static void D3D_MipMap (qbyte *out, int outwidth, int outheight, const qbyte *in, int inwidth, int inheight)
{
    int             i, j;
    const qbyte   *inrow;

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

static void *D3D11_AllocNewTextureData(void *datargba, int width, int height, unsigned int flags)
{
	HRESULT hr;
	ID3D11Texture2D *tx = NULL;
	D3D11_TEXTURE2D_DESC tdesc = {0};
	D3D11_SUBRESOURCE_DATA subresdesc[64] = {0};
	int i;
	int owidth, oheight;

	tdesc.Width = width;
	tdesc.Height = height;
	tdesc.MipLevels = (!datargba || (flags & IF_NOMIPMAP))?1:0;	//0 generates mipmaps automagically
	tdesc.ArraySize = 1;
	tdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	tdesc.SampleDesc.Count = 1;
	tdesc.SampleDesc.Quality = 0;
	tdesc.Usage = datargba?D3D11_USAGE_IMMUTABLE:D3D11_USAGE_DYNAMIC;
	tdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	tdesc.CPUAccessFlags = (datargba)?0:D3D11_CPU_ACCESS_WRITE;
	tdesc.MiscFlags = 0;

	//first mip level
	subresdesc[0].SysMemPitch = width*4;
	subresdesc[0].SysMemSlicePitch = width*height*4;

	if (!datargba)
	{
		subresdesc[0].pSysMem = NULL;
		//one mip, but no data
		i = 1;
	}
	else
	{
		subresdesc[0].pSysMem = datargba;
		for (i = 1; i < 64 && width > 1 && height > 1; i++)
		{
			owidth = width;
			oheight = height;
			width /= 2;
			height /= 2;

			subresdesc[i].pSysMem = malloc(width*height*4);
			subresdesc[i].SysMemPitch = width*4;
			subresdesc[i].SysMemSlicePitch = width*height*4;

			D3D_MipMap((void*)subresdesc[i].pSysMem, width, height, subresdesc[i-1].pSysMem, owidth, oheight);
		}
	}

	tdesc.MipLevels = i;	//0 generates mipmaps automagically

	hr = ID3D11Device_CreateTexture2D(pD3DDev11, &tdesc, (subresdesc[0].pSysMem?subresdesc:NULL), &tx);
	if (FAILED(hr))
	{
		Con_Printf("Failed to create texture\n");
		tx = NULL;
	}

	for (i = 1; i < tdesc.MipLevels; i++)
	{
		free((void*)subresdesc[i].pSysMem);
	}
	return tx;
}
texid_t D3D11_AllocNewTexture(char *ident, int width, int height, unsigned int flags)
{
	d3d11texture_t *t = d3d_lookup_texture("");
	texid_t id;
	if (t->tex2d)
		return ToTexID(t);
	t->tex2d = D3D11_AllocNewTextureData(NULL, width, height, flags);
	t->com.width = width;
	t->com.height = height;

	id = ToTexID(t);
	if (!t->tex2d)
	{
		D3D11_DestroyTexture(id);
		return r_nulltex;
	}
	
	return id;
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

static void Upload_Texture_32(ID3D11Texture2D *tex, unsigned int *data, int width, int height, unsigned int flags)
{
	int x, y;
	unsigned int *dest;
//	unsigned char swapbuf[4];
//	unsigned char swapbuf2[4];
	D3D11_MAPPED_SUBRESOURCE lock;

	D3D11_TEXTURE2D_DESC desc;
	if (!tex)
		return;

	desc.Width = 0;
	desc.Height = 0;
	ID3D11Texture2D_GetDesc(tex, &desc);
#if 0
	if (width == desc.Width && height == desc.Height)
	{
		ID3D11DeviceContext_UpdateSubresource(d3ddevctx, (ID3D11Resource*)tex, 0, NULL, data, width*4, width*height*4);
		return;
	}

	Con_Printf("Wrong size!\n");
	return;
#else
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
			//	*(unsigned int*)swapbuf2 = *(unsigned int*)swapbuf = data[x];
			//	swapbuf[0] = swapbuf2[2];
			//	swapbuf[2] = swapbuf2[0];
				dest[x] = data[x];//*(unsigned int*)swapbuf;
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
				//*(unsigned int*)swapbuf2 = *(unsigned int*)swapbuf =  inrow[(x * width)/desc.Width];
				//swapbuf[0] = swapbuf2[2];
				//swapbuf[2] = swapbuf2[0];
				row[x] = inrow[(x * width)/desc.Width];//*(unsigned int*)swapbuf;
			}
		}
	}

	ID3D11DeviceContext_Unmap(d3ddevctx, (ID3D11Resource*)tex, 0);
#endif
}

//create a basic shader from a 32bit image
static void D3D11_LoadTexture_32(d3d11texture_t *tex, unsigned int *data, int width, int height, int flags)
{
//	int nwidth, nheight;

/*
	if (!(flags & TF_MANDATORY))
	{
		Con_Printf("Texture upload missing flags\n");
		return NULL;
	}
*/

//	nwidth = width;
//	nheight = height;
//	D3D11_RoundDimensions(&nwidth, &nheight, !(flags & IF_NOMIPMAP));

	tex->com.width = width;
	tex->com.height = height;
	if (!tex->tex2d)
	{
		tex->tex2d = D3D11_AllocNewTextureData(data, width, height, flags);
		return;
	}
	else
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
	if (fmt == TF_8PAL24)
	{
		unsigned char *pal24 = (void*)pal32;
		//strictly bgr little endian.
		for (i=0 ; i<s ; i++)
		{
			p = data[i];
			trans[i] = (pal24[p*3+0] << 0) | (pal24[p*3+1] << 8) | (pal24[p*3+2] << 16) | (255<<24);
		}
	}
	else if (fmt == TF_TRANS8_FULLBRIGHT)
	{
		noalpha = true;
		for (i=0 ; i<s ; i++)
		{
			p = data[i];
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

void    D3D11_Upload (texid_t id, char *name, enum uploadfmt fmt, void *data, void *palette, int width, int height, unsigned int flags)
{
	d3d11texture_t *tex = (d3d11texture_t *)id.ref;
	switch (fmt)
	{
	case TF_RGBX32:
		flags |= IF_NOALPHA;
		//fall through
	case TF_RGBA32:
		Upload_Texture_32(tex->tex2d, data, width, height, flags);
		if (tex->view)
		{
			tex->view->lpVtbl->Release(tex->view);
			tex->view = NULL;
		}
		ToTexID(tex);
		break;
	case TF_8PAL24:
		D3D11_LoadTexture_8(tex, data, palette, width, height, flags, fmt);
		if (tex->view)
		{
			tex->view->lpVtbl->Release(tex->view);
			tex->view = NULL;
		}
		ToTexID(tex);
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
	tex->com.width = lm->width;
	tex->com.height = lm->height;

	lm->lightmap_texture = ToTexID(tex);
}

static void genNormalMap(unsigned int *nmap, qbyte *pixels, int w, int h, float scale)
{
	int i, j, wr, hr;
	unsigned char r, g, b;
	float sqlen, reciplen, nx, ny, nz;

	const float oneOver255 = 1.0f/255.0f;

	float c, cx, cy, dcx, dcy;

	wr = w;
	hr = h;

	for (i=0; i<h; i++)
	{
		for (j=0; j<w; j++)
		{
			/* Expand [0,255] texel values to the [0,1] range. */
			c = pixels[i*wr + j] * oneOver255;
			/* Expand the texel to its right. */
			cx = pixels[i*wr + (j+1)%wr] * oneOver255;
			/* Expand the texel one up. */
			cy = pixels[((i+1)%hr)*wr + j] * oneOver255;
			dcx = scale * (c - cx);
			dcy = scale * (c - cy);

			/* Normalize the vector. */
			sqlen = dcx*dcx + dcy*dcy + 1;
			reciplen = 1.0f/(float)sqrt(sqlen);
			nx = dcx*reciplen;
			ny = -dcy*reciplen;
			nz = reciplen;

			/* Repack the normalized vector into an RGB unsigned qbyte
			   vector in the normal map image. */
			r = (qbyte) (128 + 127*nx);
			g = (qbyte) (128 + 127*ny);
			b = (qbyte) (128 + 127*nz);

			/* The highest resolution mipmap level always has a
			   unit length magnitude. */
			nmap[i*w+j] = LittleLong ((pixels[i*wr + j] << 24)|(b << 16)|(g << 8)|(r));	// <AWE> Added support for big endian.
		}
	}
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
	if (tex->tex2d)	//already loaded
		return ToTexID(tex);

	switch (fmt)
	{
	case TF_HEIGHT8PAL:
		{
			extern cvar_t r_shadow_bumpscale_basetexture;
			unsigned int *norm = malloc(width*height*4);
			genNormalMap(norm, data, width, height, r_shadow_bumpscale_basetexture.value);
			D3D11_LoadTexture_32(tex, norm, width, height, flags);
			free(norm);
			return ToTexID(tex);
		}
	case TF_HEIGHT8:
		{
			extern cvar_t r_shadow_bumpscale_basetexture;
			unsigned int *norm = malloc(width*height*4);
			genNormalMap(norm, data, width, height, r_shadow_bumpscale_basetexture.value);
			D3D11_LoadTexture_32(tex, norm, width, height, flags);
			free(norm);
			return ToTexID(tex);
		}
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
		pal32[i] =	(255<<24) |
					(palette24[i*3+2]<<16) |
					(palette24[i*3+1]<<8) |
					(palette24[i*3+0]<<0);
	}
	return D3D11_LoadTexture8Pal32(identifier, width, height, data, (qbyte*)pal32, flags);
}

#ifdef RTLIGHTS
static const int shadowfmt = 1;
static const int shadowfmts[][3] =
{
	//sampler,				creation,			render
	{DXGI_FORMAT_R24_UNORM_X8_TYPELESS, DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT},
	{DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_D16_UNORM},
	{DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM}
};
d3d11texture_t shadowmap_texture[2];
ID3D11DepthStencilView *shadowmap_dsview[2];
ID3D11RenderTargetView *shadowmap_rtview[2];
texid_t D3D11_GetShadowMap(int id)
{
	d3d11texture_t *tex = &shadowmap_texture[id];
	texid_t tid;
	tid.ref = &tex->com;
	if (!tex->view)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC desc;
		desc.Format = shadowfmts[shadowfmt][0];
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MostDetailedMip = 0;
		desc.Texture2D.MipLevels = -1;
		ID3D11Device_CreateShaderResourceView(pD3DDev11, (ID3D11Resource *)tex->tex2d, &desc, &tex->view);
	}
	tid.ptr = NULL;
	return tid;
}
void D3D11_TerminateShadowMap(void)
{
	int i;
	for (i = 0; i < sizeof(shadowmap_texture)/sizeof(shadowmap_texture[0]); i++)
	{
		if (shadowmap_dsview[i])
			ID3D11DepthStencilView_Release(shadowmap_dsview[i]);
		shadowmap_dsview[i] = NULL;
		if (shadowmap_texture[i].tex2d)
			ID3D11DepthStencilView_Release(shadowmap_texture[i].tex2d);
		shadowmap_texture[i].tex2d = NULL;
	}
}
void D3D11_BeginShadowMap(int id, int w, int h)
{
	D3D11_TEXTURE2D_DESC texdesc;
	HRESULT hr;

	if (!shadowmap_dsview[id] && !shadowmap_rtview[id])
	{
		memset(&texdesc, 0, sizeof(texdesc));

		texdesc.Width = w;
		texdesc.Height = h;
		texdesc.MipLevels = 1;
		texdesc.ArraySize = 1;
		texdesc.Format = shadowfmts[shadowfmt][1];
		texdesc.SampleDesc.Count = 1;
		texdesc.SampleDesc.Quality = 0;
		texdesc.Usage = D3D11_USAGE_DEFAULT;
		texdesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		texdesc.CPUAccessFlags = 0;
		texdesc.MiscFlags = 0;

		if (shadowfmt == 2)
			texdesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		// Create the texture
		hr = ID3D11Device_CreateTexture2D(pD3DDev11, &texdesc, NULL, &shadowmap_texture[id].tex2d);
		if (FAILED(hr))
			Sys_Error("Failed to create depth texture\n");


		if (shadowfmt == 2)
		{
			hr = ID3D11Device_CreateRenderTargetView(pD3DDev11, (ID3D11Resource *)shadowmap_texture[id].tex2d, NULL, &shadowmap_rtview[id]);
		}
		else
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC rtdesc;
			rtdesc.Format = shadowfmts[shadowfmt][2];
			rtdesc.Flags = 0;
			rtdesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			rtdesc.Texture2D.MipSlice = 0;
			hr = ID3D11Device_CreateDepthStencilView(pD3DDev11, (ID3D11Resource *)shadowmap_texture[id].tex2d, &rtdesc, &shadowmap_dsview[id]);
		}
		if (FAILED(hr))
			Sys_Error("ID3D11Device_CreateDepthStencilView failed\n");
	}
	if (shadowfmt == 2)
	{
		float colours[4] = {0, 1, 0, 0};
		colours[0] = frandom();
		colours[1] = frandom();
		colours[2] = frandom();
		ID3D11DeviceContext_OMSetRenderTargets(d3ddevctx, 1, &shadowmap_rtview[id], shadowmap_dsview[id]);
		ID3D11DeviceContext_ClearRenderTargetView(d3ddevctx, shadowmap_rtview[id], colours);
	}
	else
	{
		ID3D11DeviceContext_OMSetRenderTargets(d3ddevctx, 0, NULL, shadowmap_dsview[id]);
		ID3D11DeviceContext_ClearDepthStencilView(d3ddevctx, shadowmap_dsview[id], D3D11_CLEAR_DEPTH, 1.0f, 0);
	}
}
void D3D11_EndShadowMap(void)
{
	extern ID3D11RenderTargetView *fb_backbuffer;
	extern ID3D11DepthStencilView *fb_backdepthstencil;
	ID3D11DeviceContext_OMSetRenderTargets(d3ddevctx, 1, &fb_backbuffer, fb_backdepthstencil);
}
#endif

#endif
