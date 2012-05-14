#include "quakedef.h"
#ifdef SWQUAKE
#include "sw.h"



texid_tf SW_AllocNewTexture(char *identifier, int w, int h)
{
	texid_t n;
	swimage_t *img;
	if (w & 3)
		return r_nulltex;
	img = BZ_Malloc(sizeof(*img) - sizeof(img->data) + (w * h * 4));

	Q_strncpy(img->name, identifier, sizeof(img->name));
	img->width = w;
	img->height = h;
	img->pitch = w;

	n.ptr = img;
	n.ref = &img->com;
	return n;
}
texid_tf SW_FindTexture(char *identifier)
{
	return r_nulltex;
}

void SW_RGBToBGR(swimage_t *img)
{
	int x, y;
	unsigned int *d = img->data;
	for (y = 0; y < img->height; y++)
	{
		for (x = 0; x < img->width; x++)
		{
			d[x] = (d[x]&0xff00ff00) | ((d[x]&0xff)<<16) | ((d[x]&0xff0000)>>16);
		}
		d += img->pitch;
	}
}
void SW_Upload32(swimage_t *img, int w, int h, unsigned int *data)
{
	int x, y;
	unsigned int *out = img->data;
	for (y = 0; y < h; y++)
	{
		for (x = 0; x < w; x++)
		{
			out[x] = *data++;
		}
		out += img->pitch;
	}
	SW_RGBToBGR(img);
}
void SW_Upload8(swimage_t *img, int w, int h, unsigned char *data)
{
	int x, y;
	unsigned int *out = img->data;
	for (y = 0; y < h; y++)
	{
		for (x = 0; x < w; x++)
		{
			out[x] = d_8to24rgbtable[*data++];
		}
		out += img->pitch;
	}
	SW_RGBToBGR(img);
}

texid_tf SW_LoadTexture(char *identifier, int width, int height, uploadfmt_t fmt, void *data, unsigned int flags)
{
	texid_t img = SW_FindTexture(identifier);
	if (!img.ptr)
		img = SW_AllocNewTexture(identifier, width, height);
	switch(fmt)
	{
	case TF_SOLID8:
		SW_Upload8(img.ptr, width, height, data);
		break;
	case TF_TRANS8:
		SW_Upload8(img.ptr, width, height, data);
		break;
	case TF_TRANS8_FULLBRIGHT:
		SW_Upload8(img.ptr, width, height, data);
		break;
	case TF_RGBA32:
		SW_Upload32(img.ptr, width, height, data);
		break;
	default:
		//shouldn't happen, so I'm gonna leak
		Con_Printf("SW_LoadTexture: unsupported format %i\n", fmt);
		return r_nulltex;
	}
	return img;
}
texid_tf SW_LoadTexture8Pal24(char *identifier, int width, int height, qbyte *data, qbyte *palette24, unsigned int flags)
{
	return r_nulltex;
}
texid_tf SW_LoadTexture8Pal32(char *identifier, int width, int height, qbyte *data, qbyte *palette32, unsigned int flags)
{
	return r_nulltex;
}
texid_tf SW_LoadCompressed(char *name)
{
	return r_nulltex;
}
void SW_Upload(texid_t tex, char *name, uploadfmt_t fmt, void *data, void *palette, int width, int height, unsigned int flags)
{
}
void SW_DestroyTexture(texid_t tex)
{
	swimage_t *img = tex.ptr;

	/*make sure its not in use by the renderer*/
	SWRast_Sync();

	/*okay, it can be killed*/
	BZ_Free(img);
}
#endif
