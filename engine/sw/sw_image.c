#include "quakedef.h"
#ifdef SWQUAKE
#include "sw.h"

void SW_RoundDimensions(int width, int height, int *scaled_width, int *scaled_height, qboolean mipmap)
{
	for (*scaled_width = 1 ; *scaled_width < width ; *scaled_width<<=1)
		;
	for (*scaled_height = 1 ; *scaled_height < height ; *scaled_height<<=1)
		;

	if (*scaled_width != width)
		*scaled_width >>= 1;
	if (*scaled_height != height)
		*scaled_height >>= 1;

	if (*scaled_width > 256)
		*scaled_width = 256;
	if (*scaled_height > 256)
		*scaled_height = 256;
}

texid_tf SW_AllocNewTexture(char *identifier, int w, int h, unsigned int flags)
{
	int nw, nh;
	texid_t n;
	swimage_t *img;
	if (w & 3)
		return r_nulltex;

	SW_RoundDimensions(w, h, &nw, &nh, false);
	img = BZ_Malloc(sizeof(*img) - sizeof(img->data) + (nw * nh * 4));

	Q_strncpy(img->name, identifier, sizeof(img->name));
	img->com.width = w;
	img->com.height = h;
	img->pwidth = nw;
	img->pheight = nh;
	img->pitch = nw;

	img->pwidthmask = nw-1;
	img->pheightmask = nh-1;

	n.ptr = img;
	n.ref = &img->com;
	return n;
}
texid_tf SW_FindTexture(char *identifier, unsigned int flags)
{
	return r_nulltex;
}

void SW_RGBToBGR(swimage_t *img)
{
	int x, y;
	unsigned int *d = img->data;
	for (y = 0; y < img->pheight; y++)
	{
		for (x = 0; x < img->pwidth; x++)
		{
			d[x] = (d[x]&0xff00ff00) | ((d[x]&0xff)<<16) | ((d[x]&0xff0000)>>16);
		}
		d += img->pitch;
	}
}
void SW_Upload32(swimage_t *img, int iw, int ih, unsigned int *data)
{
	//rescale the input to the output.
	//just use nearest-sample, cos we're lazy.
	int x, y;
	unsigned int *out = img->data;
	unsigned int *in;
	int sx, sy, stx, sty;
	int ow = img->pwidth, oh = img->pheight;
	stx = (iw<<16) / ow;
	sty = (ih<<16) / oh;

	for (y = 0, sy = 0; y < oh; y++, sy += sty)
	{
		in = data + iw*(sy>>16);
		for (x = 0, sx = 0; x < ow; x++, sx += stx)
		{
			out[x] = in[sx>>16];
		}
		out += img->pitch;
	}
	SW_RGBToBGR(img);
}
void SW_Upload8(swimage_t *img, int iw, int ih, unsigned char *data)
{
	//rescale the input to the output.
	//just use nearest-sample, cos we're lazy.
	int x, y;
	unsigned int *out = img->data;
	unsigned char *in;
	int sx, sy, stx, sty;
	int ow = img->pwidth, oh = img->pheight;
	stx = (iw<<16) / ow;
	sty = (ih<<16) / oh;

	for (y = 0, sy = 0; y < oh; y++, sy += sty)
	{
		in = data + iw*(sy>>16);
		for (x = 0, sx = 0; x < ow; x++, sx += stx)
		{
			out[x] = d_8to24rgbtable[in[sx>>16]];
		}
		out += img->pitch;
	}

	SW_RGBToBGR(img);
}

texid_tf SW_LoadTexture(char *identifier, int width, int height, uploadfmt_t fmt, void *data, unsigned int flags)
{
	texid_t img = SW_FindTexture(identifier, flags);
	if (!img.ptr)
		img = SW_AllocNewTexture(identifier, width, height, flags);
	if (!img.ptr)
		return r_nulltex;
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
	SWRast_Sync(&commandqueue);

	/*okay, it can be killed*/
	BZ_Free(img);
}
#endif
