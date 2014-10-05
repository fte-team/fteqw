#include "quakedef.h"
#include "winquake.h"
#ifdef D3D9QUAKE
#if !defined(HMONITOR_DECLARED) && (WINVER < 0x0500)
    #define HMONITOR_DECLARED
    DECLARE_HANDLE(HMONITOR);
#endif
#include <d3d9.h>
extern LPDIRECT3DDEVICE9 pD3DDev9;

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



void D3D9_DestroyTexture (texid_t tex)
{
	if (!tex)
		return;

	if (tex->ptr)
		IDirect3DTexture9_Release((IDirect3DTexture9*)tex->ptr);
	tex->ptr = NULL;
}

qboolean D3D9_LoadTextureMips(image_t *tex, struct pendingtextureinfo *mips)
{
	qbyte *fte_restrict out, *fte_restrict in;
	int x, y, i;
	D3DLOCKED_RECT lock;
	D3DFORMAT fmt;
	D3DSURFACE_DESC desc;
	IDirect3DTexture9 *dt;
	qboolean swap = false;

	switch(mips->encoding)
	{
	case PTI_RGBA8:
		fmt = D3DFMT_A8R8G8B8;
		swap = true;
		break;
	case PTI_RGBX8:
		fmt = D3DFMT_X8R8G8B8;
		swap = true;
		break;
	case PTI_BGRA8:
		fmt = D3DFMT_A8R8G8B8;
		break;
	case PTI_BGRX8:
		fmt = D3DFMT_X8R8G8B8;
		break;

	//too lazy to support these for now
	case PTI_S3RGB1:
	case PTI_S3RGBA1:
	case PTI_S3RGBA3:
	case PTI_S3RGBA5:
		return false;

	default:	//no idea
		return false;
	}

	if (FAILED(IDirect3DDevice9_CreateTexture(pD3DDev9, mips->mip[0].width, mips->mip[0].height, mips->mipcount, 0, fmt, D3DPOOL_MANAGED, &dt, NULL)))
		return false;

	for (i = 0; i < mips->mipcount; i++)
	{
		IDirect3DTexture9_GetLevelDesc(dt, i, &desc);

		if (mips->mip[i].height != desc.Height || mips->mip[i].width != desc.Width)
		{
			IDirect3DTexture9_Release(dt);
			return false;
		}

		IDirect3DTexture9_LockRect(dt, i, &lock, NULL, D3DLOCK_NOSYSLOCK|D3DLOCK_DISCARD);
		//can't do it in one go. pitch might contain padding or be upside down.
		if (swap)
		{
			for (y = 0, out = lock.pBits, in = mips->mip[i].data; y < mips->mip[i].height; y++, out += lock.Pitch, in += mips->mip[i].width*4)
			{
				for (x = 0; x < mips->mip[i].width*4; x+=4)
				{
					out[x+0] = in[x+2];
					out[x+1] = in[x+1];
					out[x+2] = in[x+0];
					out[x+3] = in[x+3];
				}
			}
		}
		else
		{
			for (y = 0, out = lock.pBits, in = mips->mip[i].data; y < mips->mip[i].height; y++, out += lock.Pitch, in += mips->mip[i].width*4)
				memcpy(out, in, mips->mip[i].width*4);
		}
		IDirect3DTexture9_UnlockRect(dt, i);
	}

	D3D9_DestroyTexture(tex);
	tex->ptr = dt;

	return true;
}
void D3D9_UploadLightmap(lightmapinfo_t *lm)
{
}

#endif
