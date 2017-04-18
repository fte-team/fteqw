#include "quakedef.h"
#include "shader.h"

//#define PURGEIMAGES	//somewhat experimental still. we're still flushing more than we should.

//FIXME
texid_t GL_FindTextureFallback (const char *identifier, unsigned int flags, void *fallback, int fallbackwidth, int fallbackheight, uploadfmt_t fallbackfmt);
//FIXME

#ifdef NPFTE
//#define Con_Printf(f, ...)
//hope you're on a littleendian machine
#define LittleShort(s) s
#define LittleLong(s) s
#else
cvar_t r_dodgytgafiles = CVARD("r_dodgytgafiles", "0", "Many old glquake engines had a buggy tga loader that ignored bottom-up flags. Naturally people worked around this and the world was plagued with buggy images. Most engines have now fixed the bug, but you can reenable it if you have bugged tga files.");
cvar_t r_dodgypcxfiles = CVARD("r_dodgypcxfiles", "0", "When enabled, this will ignore the palette stored within pcx files, for compatibility with quake2.");
cvar_t r_dodgymiptex = CVARD("r_dodgymiptex", "1", "When enabled, this will force regeneration of mipmaps, discarding mips1-4 like glquake did. This may eg solve fullbright issues with some maps, but may reduce distant detail levels.");

char *r_defaultimageextensions =
#ifdef IMAGEFMT_DDS
	"dds "	//compressed or something
#endif
	"tga"	//fairly fast to load
#if defined(AVAIL_PNGLIB) || defined(FTE_TARGET_WEB)
	" png"	//pngs, fairly common, but slow
#endif
	//" bmp"	//wtf? at least not lossy
#if defined(AVAIL_JPEGLIB) || defined(FTE_TARGET_WEB)
	" jpg"	//q3 uses some jpegs, for some reason
#endif
#ifndef NOLEGACY
	" pcx"	//pcxes are the original gamedata of q2. So we don't want them to override pngs.
#endif
	;
static void QDECL R_ImageExtensions_Callback(struct cvar_s *var, char *oldvalue);
cvar_t r_imageexensions				= CVARCD("r_imageexensions", NULL, R_ImageExtensions_Callback, "The list of image file extensions which fte should attempt to load.");
cvar_t r_image_downloadsizelimit	= CVARFD("r_image_downloadsizelimit", "64000", CVAR_NOTFROMSERVER, "The maximum allowed file size of images loaded from a web-based url. 0 disables completely, while empty imposes no limit.");
extern cvar_t gl_lerpimages;
extern cvar_t gl_picmip2d;
extern cvar_t gl_picmip;
extern cvar_t r_shadow_bumpscale_basetexture;
extern cvar_t r_shadow_bumpscale_bumpmap;
extern cvar_t r_shadow_heightscale_basetexture;
extern cvar_t r_shadow_heightscale_bumpmap;


static bucket_t *imagetablebuckets[256];
static hashtable_t imagetable;
static image_t *imagelist;
#endif

#ifndef _WIN32
#include <unistd.h>
#endif

typedef struct {	//cm = colourmap
	char	id_len;		//0
	char	cm_type;	//1
	char	version;	//2
	short	cm_idx;		//3
	short	cm_len;		//5
	char	cm_size;	//7
	short	originx;	//8 (ignored)
	short	originy;	//10 (ignored)
	short	width;		//12-13
	short	height;		//14-15
	qbyte	bpp;		//16
	qbyte	attribs;	//17
} tgaheader_t;

char *ReadGreyTargaFile (qbyte *data, int flen, tgaheader_t *tgahead, int asgrey)	//preswapped header
{
	int				columns, rows;
	int				row, column;
	qbyte			*pixbuf, *pal;
	qboolean		flipped;

	qbyte *pixels = BZ_Malloc(tgahead->width * tgahead->height * (asgrey?1:4));

	if (tgahead->version!=1
		&& tgahead->version!=3)
	{
		Con_Printf("LoadGrayTGA: Only type 1 and 3 greyscale targa images are understood.\n");
		BZ_Free(pixels);
		return NULL;
	}

	if (tgahead->version==1 && tgahead->bpp != 8 &&
		tgahead->cm_size != 24 && tgahead->cm_len != 256)
	{
		Con_Printf("LoadGrayTGA: Strange palette type\n");
		BZ_Free(pixels);
		return NULL;
	}

	columns = tgahead->width;
	rows = tgahead->height;

	flipped = !((tgahead->attribs & 0x20) >> 5);
#ifndef NPFTE
	if (r_dodgytgafiles.value)
		flipped = true;
#endif

	if (tgahead->version == 1)
	{
		pal = data;
		data += tgahead->cm_len*3;
		if (asgrey)
		{
			for(row=rows-1; row>=0; row--)
			{
				if (flipped)
					pixbuf = pixels + row*columns;
				else
					pixbuf = pixels + ((rows-1)-row)*columns;

				for(column=0; column<columns; column++)
					*pixbuf++= *data++;
			}
		}
		else
		{
			for(row=rows-1; row>=0; row--)
			{
				if (flipped)
					pixbuf = pixels + row*columns*4;
				else
					pixbuf = pixels + ((rows-1)-row)*columns*4;

				for(column=0; column<columns; column++)
				{
					*pixbuf++= pal[*data*3+2];
					*pixbuf++= pal[*data*3+1];
					*pixbuf++= pal[*data*3+0];
					*pixbuf++= 255;
					data++;
				}
			}
		}
		return pixels;
	}
	//version 3 now

	if (asgrey)
	{
		for(row=rows-1; row>=0; row--)
		{
			if (flipped)
				pixbuf = pixels + row*columns;
			else
				pixbuf = pixels + ((rows-1)-row)*columns;

			pixbuf = pixels + row*columns;
			for(column=0; column<columns; column++)
				*pixbuf++= *data++;
		}
	}
	else
	{
		for(row=rows-1; row>=0; row--)
		{
			if (flipped)
				pixbuf = pixels + row*columns*4;
			else
				pixbuf = pixels + ((rows-1)-row)*columns*4;

			for(column=0; column<columns; column++)
			{
				*pixbuf++= *data;
				*pixbuf++= *data;
				*pixbuf++= *data;
				*pixbuf++= 255;
				data++;
			}
		}
	}

	return pixels;
}

#define MISSHORT(ptr) (*(ptr) | (*(ptr+1) << 8))
//remember to free it
qbyte *ReadTargaFile(qbyte *buf, int length, int *width, int *height, qboolean *hasalpha, int asgrey)
{
	//tga files sadly lack a true magic header thing.
	unsigned char *data;

	qboolean flipped;

	tgaheader_t tgaheader;	//things are misaligned, so no pointer.

	if (length < 18 || buf[1] > 1 || (buf[16] != 8 && buf[16] != 16 && buf[16] != 24 && buf[16] != 32))
		return NULL;	//BUMMER!

	tgaheader.id_len = buf[0];
	tgaheader.cm_type = buf[1];
	tgaheader.version = buf[2];
	tgaheader.cm_idx = MISSHORT(buf+3);
	tgaheader.cm_len = MISSHORT(buf+5);
	tgaheader.cm_size = buf[7];
	tgaheader.originx = LittleShort(*(short *)&buf[8]);
	tgaheader.originy = LittleShort(*(short *)&buf[10]);
	tgaheader.width = LittleShort(*(short *)&buf[12]);
	tgaheader.height = LittleShort(*(short *)&buf[14]);
	tgaheader.bpp = buf[16];
	tgaheader.attribs = buf[17];

	switch(tgaheader.version)
	{
	case 0:	//No image data included.
		return NULL;	//not really valid for us. reject it after all
	case 1:	//Uncompressed, color-mapped images.
	case 2:	//Uncompressed, RGB images.
	case 3:	//Uncompressed, black and white images.
	case 9:	//Runlength encoded color-mapped images.
	case 10:	//Runlength encoded RGB images.
	case 11:	//Compressed, black and white images.
	case 32:	//Compressed color-mapped data, using Huffman, Delta, and runlength encoding.
	case 33:	//Compressed color-mapped data, using Huffman, Delta, and runlength encoding.  4-pass quadtree-type process.
		break;
	default:
		return NULL;
	}
	//validate the size to some sanity limit.
	if ((unsigned short)tgaheader.width > 8192 || (unsigned short)tgaheader.height > 8192)
		return NULL;


	flipped = !((tgaheader.attribs & 0x20) >> 5);
#ifndef NPFTE
	if (r_dodgytgafiles.value)
		flipped = true;
#endif

	data=buf+18;
	data += tgaheader.id_len;

	*width = tgaheader.width;
	*height = tgaheader.height;

	if (asgrey == 2)	//grey only, load as 8 bit..
	{
		if (!(tgaheader.version == 1) && !(tgaheader.version == 3))
			return NULL;
	}
	if (tgaheader.version == 1 || tgaheader.version == 3)
	{
		return ReadGreyTargaFile(data, length, &tgaheader, asgrey);
	}
	else if (tgaheader.version == 10 || tgaheader.version == 9 || tgaheader.version == 11)
	{
		//9:paletted
		//10:bgr(a)
		//11:greyscale
#undef getc
#define getc(x) *data++
		unsigned int row, rows=tgaheader.height, column, columns=tgaheader.width, packetHeader, packetSize, j;
		qbyte *pixbuf, *targa_rgba=BZ_Malloc(rows*columns*(asgrey?1:4)), *inrow;

		qbyte blue, red, green, alphabyte;

		byte_vec4_t palette[256];

		if (tgaheader.version == 9)
		{
			for (row = 0; row < 256; row++)
			{
				palette[row][0] = row;
				palette[row][1] = row;
				palette[row][2] = row;
				palette[row][3] = 255;
			}
			if (tgaheader.bpp != 8)
				return NULL;
		}
		if (tgaheader.version == 10)
		{
			if (tgaheader.bpp == 8)
				return NULL;

			*hasalpha = (tgaheader.bpp==32);
		}
		if (tgaheader.version == 11)
		{
			for (row = 0; row < 256; row++)
			{
				palette[row][0] = row;
				palette[row][1] = row;
				palette[row][2] = row;
				palette[row][3] = 255;
			}
			if (tgaheader.bpp != 8)
				return NULL;
		}

		if (tgaheader.cm_type)
		{
			switch(tgaheader.cm_size)
			{
			case 24:
				for (row = 0; row < tgaheader.cm_len; row++)
				{
					palette[row][0] = *data++;
					palette[row][1] = *data++;
					palette[row][2] = *data++;
					palette[row][3] = 255;
				}
				break;
			case 32:
				for (row = 0; row < tgaheader.cm_len; row++)
				{
					palette[row][0] = *data++;
					palette[row][1] = *data++;
					palette[row][2] = *data++;
					palette[row][3] = *data++;
				}
				*hasalpha = true;
				break;
			}
		}

		for(row=rows; row-->0; )
		{
			if (flipped)
				pixbuf = targa_rgba + row*columns*(asgrey?1:4);
			else
				pixbuf = targa_rgba + ((rows-1)-row)*columns*(asgrey?1:4);
			for(column=0; column<columns; )
			{
				packetHeader=*data++;
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80)
				{	// run-length packet
					switch (tgaheader.bpp)
					{
						case 8:	//we made sure this was version 11
								blue = palette[*data][0];
								green = palette[*data][1];
								red = palette[*data][2];
								alphabyte = palette[*data][3];
								data++;
								break;

						case 16:
								inrow = data;
								data+=2;
								red = ((inrow[1] & 0x7c)>>2) *8;					//red
								green =	(((inrow[1] & 0x03)<<3) + ((inrow[0] & 0xe0)>>5))*8;	//green
								blue = (inrow[0] & 0x1f)*8;					//blue
								alphabyte = (int)(inrow[1]&0x80)*2-1;			//alpha?
								break;
						case 24:
								blue = *data++;
								green = *data++;
								red = *data++;
								alphabyte = 255;
								break;
						case 32:
								blue = *data++;
								green = *data++;
								red = *data++;
								alphabyte = *data++;
								break;
						default:
								blue = 127;
								green = 127;
								red = 127;
								alphabyte = 127;
								break;
					}

					if (!asgrey)	//keep colours
					{
						for(j=0;j<packetSize;j++)
						{
							*pixbuf++=red;
							*pixbuf++=green;
							*pixbuf++=blue;
							*pixbuf++=alphabyte;
							column++;
							if (column==columns)
							{ // run spans across rows
								column=0;
								if (row>0)
									row--;
								else
									goto breakOut;
								if (flipped)
									pixbuf = targa_rgba + row*columns*4;
								else
									pixbuf = targa_rgba + ((rows-1)-row)*columns*4;
							}
						}
					}
					else	//convert to greyscale
					{
						for(j=0;j<packetSize;j++)
						{
							*pixbuf++ = red*NTSC_RED + green*NTSC_GREEN + blue*NTSC_BLUE;
							column++;
							if (column==columns)
							{ // run spans across rows
								column=0;
								if (row>0)
									row--;
								else
									goto breakOut;
								if (flipped)
									pixbuf = targa_rgba + row*columns*1;
								else
									pixbuf = targa_rgba + ((rows-1)-row)*columns*1;
							}
						}
					}
				}
				else
				{                            // non run-length packet
					if (!asgrey)	//keep colours
					{
						for(j=0;j<packetSize;j++)
						{
							switch (tgaheader.bpp)
							{
								case 8:
										blue = palette[*data][0];
										green = palette[*data][1];
										red = palette[*data][2];
										*pixbuf++ = red;
										*pixbuf++ = green;
										*pixbuf++ = blue;
										*pixbuf++ = palette[*data][3];
										data++;
										break;
								case 16:
										inrow = data;
										data+=2;
										red = ((inrow[1] & 0x7c)>>2) *8;					//red
										green =	(((inrow[1] & 0x03)<<3) + ((inrow[0] & 0xe0)>>5))*8;	//green
										blue = (inrow[0] & 0x1f)*8;					//blue
										alphabyte = (int)(inrow[1]&0x80)*2-1;			//alpha?

										*pixbuf++ = red;
										*pixbuf++ = green;
										*pixbuf++ = blue;
										*pixbuf++ = alphabyte;
										break;
								case 24:
										blue = *data++;
										green = *data++;
										red = *data++;
										*pixbuf++ = red;
										*pixbuf++ = green;
										*pixbuf++ = blue;
										*pixbuf++ = 255;
										break;
								case 32:
										blue = *data++;
										green = *data++;
										red = *data++;
										alphabyte = *data++;
										*pixbuf++ = red;
										*pixbuf++ = green;
										*pixbuf++ = blue;
										*pixbuf++ = alphabyte;
										break;
								default:
										blue = 127;
										green = 127;
										red = 127;
										alphabyte = 127;
										break;
							}
							column++;
							if (column==columns)
							{ // pixel packet run spans across rows
								column=0;
								if (row>0)
									row--;
								else
									goto breakOut;
								if (flipped)
									pixbuf = targa_rgba + row*columns*4;
								else
									pixbuf = targa_rgba + ((rows-1)-row)*columns*4;
							}
						}
					}
					else	//convert to grey
					{
						for(j=0;j<packetSize;j++)
						{
							switch (tgaheader.bpp)
							{
								case 8:
										blue = palette[*data][0];
										green = palette[*data][1];
										red = palette[*data][2];
										*pixbuf++ = (blue + green + red)/3;
										data++;
										break;
								case 16:
										inrow = data;
										data+=2;
										red = ((inrow[1] & 0x7c)>>2) *8;					//red
										green =	(((inrow[1] & 0x03)<<3) + ((inrow[0] & 0xe0)>>5))*8;	//green
										blue = (inrow[0] & 0x1f)*8;					//blue
										alphabyte = (int)(inrow[1]&0x80)*2-1;			//alpha?

										*pixbuf++ = red*NTSC_RED + green*NTSC_GREEN + blue*NTSC_BLUE;
										break;
								case 24:
										blue = *data++;
										green = *data++;
										red = *data++;
										*pixbuf++ = red*NTSC_RED + green*NTSC_GREEN + blue*NTSC_BLUE;
										break;
								case 32:
										blue = *data++;
										green = *data++;
										red = *data++;
										alphabyte = *data++;
										*pixbuf++ = red*NTSC_RED + green*NTSC_GREEN + blue*NTSC_BLUE;
										break;
								default:
										blue = 127;
										green = 127;
										red = 127;
										alphabyte = 127;
										break;
							}
							column++;
							if (column==columns)
							{ // pixel packet run spans across rows
								column=0;
								if (row>0)
									row--;
								else
									goto breakOut;
								if (flipped)
									pixbuf = targa_rgba + row*columns*1;
								else
									pixbuf = targa_rgba + ((rows-1)-row)*columns*1;
							}
						}
					}
				}
			}
		}
		breakOut:;

		return targa_rgba;
	}
	else if (tgaheader.version == 2)
	{
		qbyte *initbuf=BZ_Malloc(tgaheader.height*tgaheader.width* (asgrey?1:4));
		qbyte *inrow, *outrow;
		int x, y, mul;
		qbyte blue, red, green;

		if (tgaheader.bpp == 8)
			return NULL;

		mul = tgaheader.bpp/8;
		*hasalpha = mul==4;
//flip +convert to 32 bit
		if (asgrey)
			outrow = &initbuf[(int)(0)*tgaheader.width];
		else
			outrow = &initbuf[(int)(0)*tgaheader.width*mul];
		for (y = 0; y < tgaheader.height; y+=1)
		{
			if (flipped)
				inrow = &data[(int)(tgaheader.height-y-1)*tgaheader.width*mul];
			else
				inrow = &data[(int)(y)*tgaheader.width*mul];

			if (!asgrey)
			{
				switch(mul)
				{
				case 2:
					for (x = 0; x < tgaheader.width; x+=1)
					{
						*outrow++ = ((inrow[1] & 0x7c)>>2) *8;					//red
						*outrow++ = (((inrow[1] & 0x03)<<3) + ((inrow[0] & 0xe0)>>5))*8;	//green
						*outrow++ = (inrow[0] & 0x1f)*8;					//blue
						*outrow++ = (int)(inrow[1]&0x80)*2-1;			//alpha?
						inrow+=2;
					}
					break;
				case 3:
					for (x = 0; x < tgaheader.width; x+=1)
					{
						*outrow++ = inrow[2];
						*outrow++ = inrow[1];
						*outrow++ = inrow[0];
						*outrow++ = 255;
						inrow+=3;
					}
					break;
				case 4:
					for (x = 0; x < tgaheader.width; x+=1)
					{
						*outrow++ = inrow[2];
						*outrow++ = inrow[1];
						*outrow++ = inrow[0];
						*outrow++ = inrow[3];
						inrow+=4;
					}
					break;
				}
			}
			else
			{
				switch(mul)
				{
				case 2:
					for (x = 0; x < tgaheader.width; x+=1)
					{
						red = ((inrow[1] & 0x7c)>>2) *8;					//red
						green = (((inrow[1] & 0x03)<<3) + ((inrow[0] & 0xe0)>>5))*8;	//green
						blue = (inrow[0] & 0x1f)*8;					//blue
//						alphabyte = (int)(inrow[1]&0x80)*2-1;			//alpha?

						*outrow++ = red*NTSC_RED + green*NTSC_GREEN + blue*NTSC_BLUE;
						inrow+=2;
					}
					break;
				case 3:
					for (x = 0; x < tgaheader.width; x+=1)
					{
						red = inrow[2];
						green = inrow[1];
						blue = inrow[0];
						*outrow++ = red*NTSC_RED + green*NTSC_GREEN + blue*NTSC_BLUE;
						inrow+=3;
					}
					break;
				case 4:
					for (x = 0; x < tgaheader.width; x+=1)
					{
						red = inrow[2];
						green = inrow[1];
						blue = inrow[0];
						*outrow++ = red*NTSC_RED + green*NTSC_GREEN + blue*NTSC_BLUE;
						inrow+=4;
					}
					break;
				}
			}
		}

		return initbuf;
	}
	else
		Con_Printf("TGA: Unsupported version\n");
	return NULL;
}

#ifdef AVAIL_PNGLIB
	#ifndef AVAIL_ZLIB
		#error PNGLIB requires ZLIB
	#endif

	#undef channels

	#ifndef PNG_SUCKS_WITH_SETJMP
		#if defined(MINGW)
			#include "./mingw-libs/png.h"
		#elif defined(_WIN32)
			#include "png.h"
		#else
			#include <png.h>
		#endif
	#endif

	#ifdef DYNAMIC_LIBPNG
		#define PSTATIC(n)
		static dllhandle_t *libpng_handle;
		#define LIBPNG_LOADED() (libpng_handle != NULL)
	#else
		#define LIBPNG_LOADED() 1
		#define PSTATIC(n) = &n
		#ifdef _MSC_VER
			#ifdef _WIN64
				#pragma comment(lib, MSVCLIBSPATH "libpng64.lib")
			#else
				#pragma comment(lib, MSVCLIBSPATH "libpng.lib")
			#endif
		#endif
	#endif

#ifndef PNG_NORETURN
#define PNG_NORETURN
#endif
#ifndef PNG_ALLOCATED
#define PNG_ALLOCATED
#endif

#if PNG_LIBPNG_VER < 10500
	#define png_const_infop png_infop
	#define png_const_structp png_structp
	#define png_const_bytep png_bytep
	#define png_const_unknown_chunkp png_unknown_chunkp
#endif
#if PNG_LIBPNG_VER < 10600
	#define png_inforp png_infop
	#define png_const_inforp png_const_infop
	#define png_structrp png_structp
	#define png_const_structrp png_const_structp
#endif

void (PNGAPI *qpng_error) PNGARG((png_const_structrp png_ptr, png_const_charp error_message)) PSTATIC(png_error);
void (PNGAPI *qpng_read_end) PNGARG((png_structp png_ptr, png_infop info_ptr)) PSTATIC(png_read_end);
void (PNGAPI *qpng_read_image) PNGARG((png_structp png_ptr, png_bytepp image)) PSTATIC(png_read_image);
png_byte (PNGAPI *qpng_get_bit_depth) PNGARG((png_const_structp png_ptr, png_const_inforp info_ptr)) PSTATIC(png_get_bit_depth);
png_byte (PNGAPI *qpng_get_channels) PNGARG((png_const_structp png_ptr, png_const_inforp info_ptr)) PSTATIC(png_get_channels);
png_size_t (PNGAPI *qpng_get_rowbytes) PNGARG((png_const_structp png_ptr, png_const_inforp info_ptr)) PSTATIC(png_get_rowbytes);
void (PNGAPI *qpng_read_update_info) PNGARG((png_structp png_ptr, png_infop info_ptr)) PSTATIC(png_read_update_info);
void (PNGAPI *qpng_set_strip_16) PNGARG((png_structp png_ptr)) PSTATIC(png_set_strip_16);
void (PNGAPI *qpng_set_expand) PNGARG((png_structp png_ptr)) PSTATIC(png_set_expand);
void (PNGAPI *qpng_set_gray_to_rgb) PNGARG((png_structp png_ptr)) PSTATIC(png_set_gray_to_rgb);
void (PNGAPI *qpng_set_tRNS_to_alpha) PNGARG((png_structp png_ptr)) PSTATIC(png_set_tRNS_to_alpha);
png_uint_32 (PNGAPI *qpng_get_valid) PNGARG((png_const_structp png_ptr, png_const_infop info_ptr, png_uint_32 flag)) PSTATIC(png_get_valid);
#if PNG_LIBPNG_VER > 10400
void (PNGAPI *qpng_set_expand_gray_1_2_4_to_8) PNGARG((png_structp png_ptr)) PSTATIC(png_set_expand_gray_1_2_4_to_8);
#else
void (PNGAPI *qpng_set_gray_1_2_4_to_8) PNGARG((png_structp png_ptr)) PSTATIC(png_set_gray_1_2_4_to_8);
#endif
void (PNGAPI *qpng_set_bgr) PNGARG((png_structp png_ptr)) PSTATIC(png_set_bgr);
void (PNGAPI *qpng_set_filler) PNGARG((png_structp png_ptr, png_uint_32 filler, int flags)) PSTATIC(png_set_filler);
void (PNGAPI *qpng_set_palette_to_rgb) PNGARG((png_structp png_ptr)) PSTATIC(png_set_palette_to_rgb);
png_uint_32 (PNGAPI *qpng_get_IHDR) PNGARG((png_const_structrp png_ptr, png_const_inforp info_ptr, png_uint_32 *width, png_uint_32 *height,
			int *bit_depth, int *color_type, int *interlace_method, int *compression_method, int *filter_method)) PSTATIC(png_get_IHDR);
void (PNGAPI *qpng_read_info) PNGARG((png_structp png_ptr, png_infop info_ptr)) PSTATIC(png_read_info);
void (PNGAPI *qpng_set_sig_bytes) PNGARG((png_structp png_ptr, int num_bytes)) PSTATIC(png_set_sig_bytes);
void (PNGAPI *qpng_set_read_fn) PNGARG((png_structp png_ptr, png_voidp io_ptr, png_rw_ptr read_data_fn)) PSTATIC(png_set_read_fn);
void (PNGAPI *qpng_destroy_read_struct) PNGARG((png_structpp png_ptr_ptr, png_infopp info_ptr_ptr, png_infopp end_info_ptr_ptr)) PSTATIC(png_destroy_read_struct);
png_infop (PNGAPI *qpng_create_info_struct) PNGARG((png_const_structrp png_ptr)) PSTATIC(png_create_info_struct);
png_structp (PNGAPI *qpng_create_read_struct) PNGARG((png_const_charp user_png_ver, png_voidp error_ptr, png_error_ptr error_fn, png_error_ptr warn_fn)) PSTATIC(png_create_read_struct);
int (PNGAPI *qpng_sig_cmp) PNGARG((png_const_bytep sig, png_size_t start, png_size_t num_to_check)) PSTATIC(png_sig_cmp);

void (PNGAPI *qpng_write_end) PNGARG((png_structrp png_ptr, png_inforp info_ptr)) PSTATIC(png_write_end);
void (PNGAPI *qpng_write_image) PNGARG((png_structrp png_ptr, png_bytepp image)) PSTATIC(png_write_image);
void (PNGAPI *qpng_write_info) PNGARG((png_structrp png_ptr, png_const_inforp info_ptr)) PSTATIC(png_write_info);
void (PNGAPI *qpng_set_IHDR) PNGARG((png_const_structrp png_ptr, png_infop info_ptr, png_uint_32 width, png_uint_32 height,
			int bit_depth, int color_type, int interlace_method, int compression_method, int filter_method)) PSTATIC(png_set_IHDR);
void (PNGAPI *qpng_set_compression_level) PNGARG((png_structrp png_ptr, int level)) PSTATIC(png_set_compression_level);
void (PNGAPI *qpng_init_io) PNGARG((png_structp png_ptr, png_FILE_p fp)) PSTATIC(png_init_io);
png_voidp (PNGAPI *qpng_get_io_ptr) PNGARG((png_const_structrp png_ptr)) PSTATIC(png_get_io_ptr);
void (PNGAPI *qpng_destroy_write_struct) PNGARG((png_structpp png_ptr_ptr, png_infopp info_ptr_ptr)) PSTATIC(png_destroy_write_struct);
png_structp (PNGAPI *qpng_create_write_struct) PNGARG((png_const_charp user_png_ver, png_voidp error_ptr, png_error_ptr error_fn, png_error_ptr warn_fn)) PSTATIC(png_create_write_struct);
void (PNGAPI *qpng_set_unknown_chunks) PNGARG((png_const_structrp png_ptr, png_inforp info_ptr, png_const_unknown_chunkp unknowns, int num_unknowns)) PSTATIC(png_set_unknown_chunks);

png_voidp (PNGAPI *qpng_get_error_ptr) PNGARG((png_const_structrp png_ptr)) PSTATIC(png_get_error_ptr);

qboolean LibPNG_Init(void)
{
#ifdef DYNAMIC_LIBPNG
	static dllfunction_t pngfuncs[] =
	{
		{(void **) &qpng_error,							"png_error"},
		{(void **) &qpng_read_end,						"png_read_end"},
		{(void **) &qpng_read_image,					"png_read_image"},
		{(void **) &qpng_get_bit_depth,					"png_get_bit_depth"},
		{(void **) &qpng_get_channels,					"png_get_channels"},
		{(void **) &qpng_get_rowbytes,					"png_get_rowbytes"},
		{(void **) &qpng_read_update_info,				"png_read_update_info"},
		{(void **) &qpng_set_strip_16,					"png_set_strip_16"},
		{(void **) &qpng_set_expand,					"png_set_expand"},
		{(void **) &qpng_set_gray_to_rgb,				"png_set_gray_to_rgb"},
		{(void **) &qpng_set_tRNS_to_alpha,				"png_set_tRNS_to_alpha"},
		{(void **) &qpng_get_valid,						"png_get_valid"},
#if PNG_LIBPNG_VER > 10400
		{(void **) &qpng_set_expand_gray_1_2_4_to_8,	"png_set_expand_gray_1_2_4_to_8"},
#else
		{(void **) &qpng_set_gray_1_2_4_to_8,	"png_set_gray_1_2_4_to_8"},
#endif
		{(void **) &qpng_set_bgr,						"png_set_bgr"},
		{(void **) &qpng_set_filler,					"png_set_filler"},
		{(void **) &qpng_set_palette_to_rgb,			"png_set_palette_to_rgb"},
		{(void **) &qpng_get_IHDR,						"png_get_IHDR"},
		{(void **) &qpng_read_info,						"png_read_info"},
		{(void **) &qpng_set_sig_bytes,					"png_set_sig_bytes"},
		{(void **) &qpng_set_read_fn,					"png_set_read_fn"},
		{(void **) &qpng_destroy_read_struct,			"png_destroy_read_struct"},
		{(void **) &qpng_create_info_struct,			"png_create_info_struct"},
		{(void **) &qpng_create_read_struct,			"png_create_read_struct"},
		{(void **) &qpng_sig_cmp,						"png_sig_cmp"},

		{(void **) &qpng_write_end,						"png_write_end"},
		{(void **) &qpng_write_image,					"png_write_image"},
		{(void **) &qpng_write_info,					"png_write_info"},
		{(void **) &qpng_set_IHDR,						"png_set_IHDR"},
		{(void **) &qpng_set_compression_level,			"png_set_compression_level"},
		{(void **) &qpng_init_io,						"png_init_io"},
		{(void **) &qpng_get_io_ptr,					"png_get_io_ptr"},
		{(void **) &qpng_destroy_write_struct,			"png_destroy_write_struct"},
		{(void **) &qpng_create_write_struct,			"png_create_write_struct"},
		{(void **) &qpng_set_unknown_chunks,			"png_set_unknown_chunks"},

		{(void **) &qpng_get_error_ptr,					"png_get_error_ptr"},
		{NULL, NULL}
	};
	static qboolean tried;
	if (!tried)
	{
		tried = true;

		if (!LIBPNG_LOADED())
		{
			char *libnames[] =
			{
			#ifdef _WIN32
				va("libpng%i", PNG_LIBPNG_VER_DLLNUM);
			#else
				//linux...
				//lsb uses 'libpng12.so' specifically, so make sure that works.
				"libpng" STRINGIFY(PNG_LIBPNG_VER_MAJOR) STRINGIFY(PNG_LIBPNG_VER_MINOR) ".so." STRINGIFY(PNG_LIBPNG_VER_SONUM),
				"libpng" STRINGIFY(PNG_LIBPNG_VER_MAJOR) STRINGIFY(PNG_LIBPNG_VER_MINOR) ".so",
				"libpng.so." STRINGIFY(PNG_LIBPNG_VER_SONUM)
				"libpng.so",
			#endif
			};
			size_t i;
			for (i = 0; i < countof(libnames); i++)
			{
				if (libnames[i])
				{
					libpng_handle = Sys_LoadLibrary(libnames[i], pngfuncs);
					if (libpng_handle)
						break;
				}
			}
			if (!libpng_handle)
				Con_Printf("Unable to load %s\n", libnames[0]);
		}

//		if (!LIBPNG_LOADED())
//			libpng_handle = Sys_LoadLibrary("libpng", pngfuncs);
	}
#endif
	return LIBPNG_LOADED();
}

typedef struct {
	char *data;
	int readposition;
	int filelen;
} pngreadinfo_t;

static void VARGS readpngdata(png_structp png_ptr,png_bytep data,png_size_t len)
{
	pngreadinfo_t *ri = (pngreadinfo_t*)qpng_get_io_ptr(png_ptr);
	if (ri->readposition+len > ri->filelen)
	{
		qpng_error(png_ptr, "unexpected eof");
		return;
	}
	memcpy(data, &ri->data[ri->readposition], len);
	ri->readposition+=len;
}

struct pngerr
{
	const char *fname;
	jmp_buf jbuf;
};
static void VARGS png_onerror(png_structp png_ptr, png_const_charp error_msg)
{
	struct pngerr *err = qpng_get_error_ptr(png_ptr);
	Con_Printf("libpng %s: %s\n", err->fname, error_msg);
	longjmp(err->jbuf, 1);
	abort();
}

static void VARGS png_onwarning(png_structp png_ptr, png_const_charp warning_msg)
{
	struct pngerr *err = qpng_get_error_ptr(png_ptr);
#ifndef NPFTE
	Con_DPrintf("libpng %s: %s\n", err->fname, warning_msg);
#endif
}

qbyte *ReadPNGFile(qbyte *buf, int length, int *width, int *height, const char *fname)
{
	qbyte header[8], **rowpointers = NULL, *data = NULL;
	png_structp png;
	png_infop pnginfo;
	int y, bitdepth, colortype, interlace, compression, filter, bytesperpixel;
	unsigned long rowbytes;
	pngreadinfo_t ri;
	png_uint_32 pngwidth, pngheight;
	struct pngerr errctx;

	if (!LibPNG_Init())
		return NULL;

	memcpy(header, buf, 8);

	errctx.fname = fname;
	if (setjmp(errctx.jbuf))
	{
error:
		if (data)
			BZ_Free(data);
		if (rowpointers)
			BZ_Free(rowpointers);
		qpng_destroy_read_struct(&png, &pnginfo, NULL);
		return NULL;
	}

	if (qpng_sig_cmp(header, 0, 8))
	{
		return NULL;
	}

	if (!(png = qpng_create_read_struct(PNG_LIBPNG_VER_STRING, &errctx, png_onerror, png_onwarning)))
	{
		return NULL;
	}

	if (!(pnginfo = qpng_create_info_struct(png)))
	{
		qpng_destroy_read_struct(&png, &pnginfo, NULL);
		return NULL;
	}

	ri.data=buf;
	ri.readposition=8;
	ri.filelen=length;
	qpng_set_read_fn(png, &ri, readpngdata);

	qpng_set_sig_bytes(png, 8);
	qpng_read_info(png, pnginfo);
	qpng_get_IHDR(png, pnginfo, &pngwidth, &pngheight, &bitdepth, &colortype, &interlace, &compression, &filter);

	*width = pngwidth;
	*height = pngheight;

	if (colortype == PNG_COLOR_TYPE_PALETTE)
	{
		qpng_set_palette_to_rgb(png);
		qpng_set_filler(png, 255, PNG_FILLER_AFTER);
	}

	if (colortype == PNG_COLOR_TYPE_GRAY && bitdepth < 8)
	{
		#if PNG_LIBPNG_VER > 10400
			qpng_set_expand_gray_1_2_4_to_8(png);
		#else
			qpng_set_gray_1_2_4_to_8(png);
		#endif
	}

	if (qpng_get_valid( png, pnginfo, PNG_INFO_tRNS))
		qpng_set_tRNS_to_alpha(png);

	if (bitdepth >= 8 && colortype == PNG_COLOR_TYPE_RGB)
		qpng_set_filler(png, 255, PNG_FILLER_AFTER);

	if (colortype == PNG_COLOR_TYPE_GRAY || colortype == PNG_COLOR_TYPE_GRAY_ALPHA)
	{
		qpng_set_gray_to_rgb( png );
		qpng_set_filler(png, 255, PNG_FILLER_AFTER);
	}

	if (bitdepth < 8)
		qpng_set_expand (png);
	else if (bitdepth == 16)
		qpng_set_strip_16(png);


	qpng_read_update_info(png, pnginfo);
	rowbytes = qpng_get_rowbytes(png, pnginfo);
	bytesperpixel = qpng_get_channels(png, pnginfo);
	bitdepth = qpng_get_bit_depth(png, pnginfo);

	if (bitdepth != 8 || bytesperpixel != 4)
	{
		Con_Printf ("Bad PNG color depth and/or bpp (%s)\n", fname);
		qpng_destroy_read_struct(&png, &pnginfo, NULL);
		return NULL;
	}

	data = BZF_Malloc(*height * rowbytes);
	rowpointers = BZF_Malloc(*height * sizeof(*rowpointers));

	if (!data || !rowpointers)
		goto error;

	for (y = 0; y < *height; y++)
		rowpointers[y] = data + y * rowbytes;

	qpng_read_image(png, rowpointers);
	qpng_read_end(png, NULL);

	qpng_destroy_read_struct(&png, &pnginfo, NULL);
	BZ_Free(rowpointers);
	return data;
}



#ifndef NPFTE
int Image_WritePNG (char *filename, enum fs_relative fsroot, int compression, void **buffers, int numbuffers, int width, int height, enum uploadfmt fmt)
{
	char name[MAX_OSPATH];
	int i;
	FILE *fp;
	png_structp png_ptr;
	png_infop info_ptr;
	png_byte **row_pointers;
	struct pngerr errctx;
	int pxsize;
	int stride = width;

	qbyte stereochunk = 0;	//cross-eyed
	png_unknown_chunk unknowns = {"sTER", &stereochunk, sizeof(stereochunk), PNG_HAVE_PLTE};

	if (!FS_NativePath(filename, fsroot, name, sizeof(name)))
		return false;

	if (numbuffers == 2)
	{
		stride = width;
		if (stride & 7)	//standard stereo images must be padded to 8 pixels width padding between
			stride += 8-(stride & 7);
		stride += width;
	}
	else	//arrange them all horizontally
		stride = width * numbuffers;

	if (!LibPNG_Init())
		return false;

	if (!(fp = fopen (name, "wb")))
	{
		FS_CreatePath (filename, FS_GAMEONLY);
		if (!(fp = fopen (name, "wb")))
			return false;
	}

	errctx.fname = filename;
	if (setjmp(errctx.jbuf))
	{
err:
		qpng_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fp);
		return false;
	}

	if (!(png_ptr = qpng_create_write_struct(PNG_LIBPNG_VER_STRING, &errctx, png_onerror, png_onwarning)))
	{
		fclose(fp);
		return false;
	}

	if (!(info_ptr = qpng_create_info_struct(png_ptr)))
	{
		qpng_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		fclose(fp);
		return false;
	}

	qpng_init_io(png_ptr, fp);
	compression = bound(0, compression, 100);

// had to add these when I migrated from libpng 1.4.x to 1.5.x
#ifndef Z_NO_COMPRESSION
#define Z_NO_COMPRESSION			0
#endif
#ifndef Z_BEST_COMPRESSION
#define Z_BEST_COMPRESSION			9
#endif
	qpng_set_compression_level(png_ptr, Z_NO_COMPRESSION + (compression*(Z_BEST_COMPRESSION-Z_NO_COMPRESSION))/100);

	if (fmt == TF_BGR24 || fmt == TF_BGRA32)
		qpng_set_bgr(png_ptr);
	if (fmt == TF_RGBA32 || fmt == TF_BGRA32)
	{
		pxsize = 4;
		qpng_set_IHDR(png_ptr, info_ptr, stride, height, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	}
	else
	{
		pxsize = 3;
		qpng_set_IHDR(png_ptr, info_ptr, stride, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	}

	if (numbuffers == 2)	//flag it as a standard stereographic image
		qpng_set_unknown_chunks(png_ptr, info_ptr, &unknowns, 1);

	qpng_write_info(png_ptr, info_ptr);

	if (numbuffers == 2)
	{	//standard stereographic png image.
		qbyte *pixels, *left, *right;
		//we need to pack the data into a single image for libpng to use
		row_pointers = Z_Malloc (sizeof(png_byte *) * height + stride*height*pxsize);	//must be zeroed, because I'm too lazy to specially deal with padding.
		if (!row_pointers)
			goto err;
		pixels = (qbyte*)row_pointers + height;
		//png requires right then left, which is a bit weird.
		right = pixels;
		left = right + (stride-width)*pxsize;

		for (i = 0; i < height; i++)
		{
			if ((qbyte*)buffers[1])
				memcpy(right + i*stride*pxsize, (qbyte*)buffers[1] + i*pxsize*width, pxsize * width);
			if ((qbyte*)buffers[0])
				memcpy(left + i*stride*pxsize, (qbyte*)buffers[0] + i*pxsize*width, pxsize * width);
			row_pointers[height - i - 1] = pixels + i * stride * pxsize;
		}
	}
	else if (numbuffers == 1)
	{
		row_pointers = BZ_Malloc (sizeof(png_byte *) * height);
		if (!row_pointers)
			goto err;
		for (i = 0; i < height; i++)
			row_pointers[height - i - 1] = (qbyte*)buffers[0] + i * width * pxsize;
	}
	else
	{	//pack all images horizontally, because preventing people from doing the whole cross-eyed thing is cool, or something.
		qbyte *pixels;
		int j;
		//we need to pack the data into a single image for libpng to use
		row_pointers = BZ_Malloc (sizeof(png_byte *) * height + stride*height*pxsize);
		if (!row_pointers)
			goto err;
		pixels = (qbyte*)row_pointers + height;
		for (i = 0; i < height; i++)
		{
			for (j = 0; j < numbuffers; j++)
			{
				if (buffers[j])
					memcpy(pixels+(width*j + i*stride)*pxsize, (qbyte*)buffers[j] + i*pxsize*width, pxsize * width);
			}
			row_pointers[height - i - 1] = pixels + i * stride * pxsize;
		}
	}
	qpng_write_image(png_ptr, row_pointers);
	qpng_write_end(png_ptr, info_ptr);
	BZ_Free(row_pointers);
	qpng_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);
	return true;
}
#endif


#endif

#ifdef AVAIL_JPEGLIB
#define XMD_H	//fix for mingw

#if defined(MINGW)
	#define JPEG_API VARGS
	#include "./mingw-libs/jpeglib.h"
	#include "./mingw-libs/jerror.h"
#elif defined(_WIN32)
	#define JPEG_API VARGS
	#include "jpeglib.h"
	#include "jerror.h"
#else
//	#include <jinclude.h>
	#include <jpeglib.h>
	#include <jerror.h>
#endif

#ifdef DYNAMIC_LIBJPEG
	#define JSTATIC(n)
	static dllhandle_t *libjpeg_handle;
	#define LIBJPEG_LOADED() (libjpeg_handle != NULL)
#else
	#ifdef _MSC_VER
		#ifdef _WIN64
			#pragma comment(lib, MSVCLIBSPATH "libjpeg64.lib")
		#else
			#pragma comment(lib, MSVCLIBSPATH "jpeg.lib")
		#endif
	#endif
	#define JSTATIC(n) = &n
	#define LIBJPEG_LOADED() (1)
#endif

#ifndef JPEG_FALSE
#define JPEG_boolean boolean
#endif

#define qjpeg_create_compress(cinfo) \
    qjpeg_CreateCompress((cinfo), JPEG_LIB_VERSION, \
			(size_t) sizeof(struct jpeg_compress_struct))
#define qjpeg_create_decompress(cinfo) \
    qjpeg_CreateDecompress((cinfo), JPEG_LIB_VERSION, \
			  (size_t) sizeof(struct jpeg_decompress_struct))

#ifdef DYNAMIC_LIBJPEG
boolean (VARGS *qjpeg_resync_to_restart) JPP((j_decompress_ptr cinfo, int desired))										JSTATIC(jpeg_resync_to_restart);
boolean (VARGS *qjpeg_finish_decompress) JPP((j_decompress_ptr cinfo))												JSTATIC(jpeg_finish_decompress);
JDIMENSION (VARGS *qjpeg_read_scanlines) JPP((j_decompress_ptr cinfo, JSAMPARRAY scanlines, JDIMENSION max_lines))	JSTATIC(jpeg_read_scanlines);
boolean (VARGS *qjpeg_start_decompress) JPP((j_decompress_ptr cinfo))													JSTATIC(jpeg_start_decompress);
int (VARGS *qjpeg_read_header) JPP((j_decompress_ptr cinfo, boolean require_image))									JSTATIC(jpeg_read_header);
void (VARGS *qjpeg_CreateDecompress) JPP((j_decompress_ptr cinfo, int version, size_t structsize))					JSTATIC(jpeg_CreateDecompress);
void (VARGS *qjpeg_destroy_decompress) JPP((j_decompress_ptr cinfo))													JSTATIC(jpeg_destroy_decompress);

struct jpeg_error_mgr * (VARGS *qjpeg_std_error) JPP((struct jpeg_error_mgr * err))									JSTATIC(jpeg_std_error);

void (VARGS *qjpeg_finish_compress) JPP((j_compress_ptr cinfo))														JSTATIC(jpeg_finish_compress);
JDIMENSION (VARGS *qjpeg_write_scanlines) JPP((j_compress_ptr cinfo, JSAMPARRAY scanlines, JDIMENSION num_lines))		JSTATIC(jpeg_write_scanlines);
void (VARGS *qjpeg_start_compress) JPP((j_compress_ptr cinfo, boolean write_all_tables))								JSTATIC(jpeg_start_compress);
void (VARGS *qjpeg_set_quality) JPP((j_compress_ptr cinfo, int quality, boolean force_baseline))						JSTATIC(jpeg_set_quality);
void (VARGS *qjpeg_set_defaults) JPP((j_compress_ptr cinfo))															JSTATIC(jpeg_set_defaults);
void (VARGS *qjpeg_CreateCompress) JPP((j_compress_ptr cinfo, int version, size_t structsize))						JSTATIC(jpeg_CreateCompress);
void (VARGS *qjpeg_destroy_compress) JPP((j_compress_ptr cinfo))														JSTATIC(jpeg_destroy_compress);
#endif

qboolean LibJPEG_Init(void)
{
	#ifdef DYNAMIC_LIBJPEG
	static dllfunction_t jpegfuncs[] =
	{
		{(void **) &qjpeg_resync_to_restart,		"jpeg_resync_to_restart"},
		{(void **) &qjpeg_finish_decompress,		"jpeg_finish_decompress"},
		{(void **) &qjpeg_read_scanlines,			"jpeg_read_scanlines"},
		{(void **) &qjpeg_start_decompress,			"jpeg_start_decompress"},
		{(void **) &qjpeg_read_header,				"jpeg_read_header"},
		{(void **) &qjpeg_CreateDecompress,			"jpeg_CreateDecompress"},
		{(void **) &qjpeg_destroy_decompress,		"jpeg_destroy_decompress"},

		{(void **) &qjpeg_std_error,				"jpeg_std_error"},

		{(void **) &qjpeg_finish_compress,			"jpeg_finish_compress"},
		{(void **) &qjpeg_write_scanlines,			"jpeg_write_scanlines"},
		{(void **) &qjpeg_start_compress,			"jpeg_start_compress"},
		{(void **) &qjpeg_set_quality,				"jpeg_set_quality"},
		{(void **) &qjpeg_set_defaults,				"jpeg_set_defaults"},
		{(void **) &qjpeg_CreateCompress,			"jpeg_CreateCompress"},
		{(void **) &qjpeg_destroy_compress,			"jpeg_destroy_compress"},

		{NULL, NULL}
	};

	if (!LIBJPEG_LOADED())
		libjpeg_handle = Sys_LoadLibrary("libjpeg", jpegfuncs);
#ifndef _WIN32
	if (!LIBJPEG_LOADED())
		libjpeg_handle = Sys_LoadLibrary("libjpeg"ARCH_DL_POSTFIX".8", jpegfuncs);
	if (!LIBJPEG_LOADED())
		libjpeg_handle = Sys_LoadLibrary("libjpeg"ARCH_DL_POSTFIX".62", jpegfuncs);
#endif
	#endif

	if (!LIBJPEG_LOADED())
		Con_Printf("Unable to init libjpeg\n");
	return LIBJPEG_LOADED();
}

/*begin jpeg read*/

struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */

  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

/*
 * Here's the routine that will replace the standard error_exit method:
 */

METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}


/*
 * Sample routine for JPEG decompression.  We assume that the source file name
 * is passed in.  We want to return 1 on success, 0 on error.
 */




/* Expanded data source object for stdio input */

typedef struct {
  struct jpeg_source_mgr pub;	/* public fields */

  qbyte * infile;		/* source stream */
  int currentpos;
  int maxlen;
  JOCTET * buffer;		/* start of buffer */
  JPEG_boolean start_of_file;	/* have we gotten any data yet? */
} my_source_mgr;

typedef my_source_mgr * my_src_ptr;

#define INPUT_BUF_SIZE  4096	/* choose an efficiently fread'able size */


METHODDEF(void)
init_source (j_decompress_ptr cinfo)
{
  my_src_ptr src = (my_src_ptr) cinfo->src;

  src->start_of_file = true;
}

METHODDEF(JPEG_boolean)
fill_input_buffer (j_decompress_ptr cinfo)
{
	my_source_mgr *src = (my_source_mgr*) cinfo->src;
	size_t nbytes;

	nbytes = src->maxlen - src->currentpos;
	if (nbytes > INPUT_BUF_SIZE)
		nbytes = INPUT_BUF_SIZE;
	memcpy(src->buffer, &src->infile[src->currentpos], nbytes);
	src->currentpos+=nbytes;

	if (nbytes <= 0)
	{
		if (src->start_of_file)	/* Treat empty input file as fatal error */
			ERREXIT(cinfo, JERR_INPUT_EMPTY);
		WARNMS(cinfo, JWRN_JPEG_EOF);
		/* Insert a fake EOI marker */
		src->buffer[0] = (JOCTET) 0xFF;
		src->buffer[1] = (JOCTET) JPEG_EOI;
		nbytes = 2;
	}

	src->pub.next_input_byte = src->buffer;
	src->pub.bytes_in_buffer = nbytes;
	src->start_of_file = false;

	return true;
}


METHODDEF(void)
skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
  my_source_mgr *src = (my_source_mgr*) cinfo->src;

  if (num_bytes > 0) {
    while (num_bytes > (long) src->pub.bytes_in_buffer) {
      num_bytes -= (long) src->pub.bytes_in_buffer;
      (void) fill_input_buffer(cinfo);
    }
    src->pub.next_input_byte += (size_t) num_bytes;
    src->pub.bytes_in_buffer -= (size_t) num_bytes;
  }
}



METHODDEF(void)
term_source (j_decompress_ptr cinfo)
{
}


#undef GLOBAL
#define GLOBAL(x) x

GLOBAL(void)
ftejpeg_mem_src (j_decompress_ptr cinfo, qbyte * infile, int maxlen)
{
  my_source_mgr *src;

  if (cinfo->src == NULL) {	/* first time for this JPEG object? */
    cinfo->src = (struct jpeg_source_mgr *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				  sizeof(my_source_mgr));
    src = (my_source_mgr*) cinfo->src;
    src->buffer = (JOCTET *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				  INPUT_BUF_SIZE * sizeof(JOCTET));
  }

  src = (my_source_mgr*) cinfo->src;
  src->pub.init_source = init_source;
  src->pub.fill_input_buffer = fill_input_buffer;
  src->pub.skip_input_data = skip_input_data;
  #ifdef DYNAMIC_LIBJPEG
  	src->pub.resync_to_restart = qjpeg_resync_to_restart; /* use default method */
  #else
  	src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
  #endif
  src->pub.term_source = term_source;
  src->infile = infile;
  src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
  src->pub.next_input_byte = NULL; /* until buffer loaded */

  src->currentpos = 0;
  src->maxlen = maxlen;
}

qbyte *ReadJPEGFile(qbyte *infile, int length, int *width, int *height)
{
	qbyte *mem=NULL, *in, *out;
	int i;

	/* This struct contains the JPEG decompression parameters and pointers to
	* working space (which is allocated as needed by the JPEG library).
	*/
	struct jpeg_decompress_struct cinfo;
	/* We use our private extension JPEG error handler.
	* Note that this struct must live as long as the main JPEG parameter
	* struct, to avoid dangling-pointer problems.
	*/
	struct my_error_mgr jerr;
	/* More stuff */
	JSAMPARRAY buffer;		/* Output row buffer */
	int size_stride;		/* physical row width in output buffer */

	memset(&cinfo, 0, sizeof(cinfo));

	if (!LIBJPEG_LOADED())
	{
		Con_DPrintf("libjpeg not available.\n");
		return NULL;
	}

	/* Step 1: allocate and initialize JPEG decompression object */

	/* We set up the normal JPEG error routines, then override error_exit. */
	#ifdef DYNAMIC_LIBJPEG
		cinfo.err = qjpeg_std_error(&jerr.pub);
	#else
		cinfo.err = jpeg_std_error(&jerr.pub);
	#endif
	jerr.pub.error_exit = my_error_exit;
	/* Establish the setjmp return context for my_error_exit to use. */
	if (setjmp(jerr.setjmp_buffer))
	{
		// If we get here, the JPEG code has signaled an error.
		Con_DPrintf("libjpeg failed to decode a file.\n");
badjpeg:
		#ifdef DYNAMIC_LIBJPEG
			qjpeg_destroy_decompress(&cinfo);
		#else
			jpeg_destroy_decompress(&cinfo);
		#endif

		if (mem)
			BZ_Free(mem);
		return NULL;
	}
	#ifdef DYNAMIC_LIBJPEG
		qjpeg_create_decompress(&cinfo);
	#else
		jpeg_create_decompress(&cinfo);
	#endif

	ftejpeg_mem_src(&cinfo, infile, length);

	#ifdef DYNAMIC_LIBJPEG
		(void) qjpeg_read_header(&cinfo, true);
	#else
		(void) jpeg_read_header(&cinfo, true);
	#endif

	#ifdef DYNAMIC_LIBJPEG
		(void) qjpeg_start_decompress(&cinfo);
	#else
		(void) jpeg_start_decompress(&cinfo);
	#endif


	if (cinfo.output_components == 0)
	{
		Con_DPrintf("No JPEG Components, not a JPEG.\n");
		goto badjpeg;
	}
	if (cinfo.output_components!=3 && cinfo.output_components != 1)
	{
		Con_DPrintf("Bad number of components in JPEG: '%d', should be '3'.\n",cinfo.output_components);
		goto badjpeg;
	}
	size_stride = cinfo.output_width * cinfo.output_components;
	/* Make a one-row-high sample array that will go away when done with image */
	buffer = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE, size_stride, 1);

	out=mem=BZ_Malloc(cinfo.output_height*cinfo.output_width*4);
	memset(out, 0, cinfo.output_height*cinfo.output_width*4);

	if (cinfo.output_components == 1)
	{
		while (cinfo.output_scanline < cinfo.output_height)
		{
			#ifdef DYNAMIC_LIBJPEG
				(void) qjpeg_read_scanlines(&cinfo, buffer, 1);
			#else
				(void) jpeg_read_scanlines(&cinfo, buffer, 1);
			#endif

			in = buffer[0];
			for (i = 0; i < cinfo.output_width; i++)
			{//rgb to rgba
				*out++ = *in;
				*out++ = *in;
				*out++ = *in;
				*out++ = 255;
				in++;
			}
		}
	}
	else
	{
		while (cinfo.output_scanline < cinfo.output_height)
		{
			#ifdef DYNAMIC_LIBJPEG
				(void) qjpeg_read_scanlines(&cinfo, buffer, 1);
			#else
				(void) jpeg_read_scanlines(&cinfo, buffer, 1);
			#endif

			in = buffer[0];
			for (i = 0; i < cinfo.output_width; i++)
			{//rgb to rgba
				*out++ = *in++;
				*out++ = *in++;
				*out++ = *in++;
				*out++ = 255;
			}
		}
	}

	#ifdef DYNAMIC_LIBJPEG
		(void) qjpeg_finish_decompress(&cinfo);
	#else
		(void) jpeg_finish_decompress(&cinfo);
	#endif

	#ifdef DYNAMIC_LIBJPEG
		qjpeg_destroy_decompress(&cinfo);
	#else
		jpeg_destroy_decompress(&cinfo);
	#endif

	*width = cinfo.output_width;
	*height = cinfo.output_height;

	return mem;

}
/*end read*/
#ifndef NPFTE
/*begin write*/


#ifndef DYNAMIC_LIBJPEG
#define qjpeg_std_error			jpeg_std_error
#define qjpeg_destroy_compress	jpeg_destroy_compress
#define qjpeg_CreateCompress	jpeg_CreateCompress
#define qjpeg_set_defaults		jpeg_set_defaults
#define qjpeg_set_quality		jpeg_set_quality
#define qjpeg_start_compress	jpeg_start_compress
#define qjpeg_write_scanlines	jpeg_write_scanlines
#define qjpeg_finish_compress	jpeg_finish_compress
#define qjpeg_destroy_compress	jpeg_destroy_compress
#endif
#define OUTPUT_BUF_SIZE 4096
typedef struct  {
	struct jpeg_error_mgr pub;

	jmp_buf setjmp_buffer;
} jpeg_error_mgr_wrapper;

typedef struct {
	struct jpeg_destination_mgr pub;

	vfsfile_t *vfs;


	JOCTET  buffer[OUTPUT_BUF_SIZE];		/* start of buffer */
} my_destination_mgr;

METHODDEF(void) init_destination (j_compress_ptr cinfo)
{
	my_destination_mgr *dest = (my_destination_mgr*) cinfo->dest;

	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}
METHODDEF(JPEG_boolean) empty_output_buffer (j_compress_ptr cinfo)
{
	my_destination_mgr *dest = (my_destination_mgr*) cinfo->dest;

	VFS_WRITE(dest->vfs, dest->buffer, OUTPUT_BUF_SIZE);
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

	return true;
}
METHODDEF(void) term_destination (j_compress_ptr cinfo)
{
	my_destination_mgr *dest = (my_destination_mgr*) cinfo->dest;

	VFS_WRITE(dest->vfs, dest->buffer, OUTPUT_BUF_SIZE - dest->pub.free_in_buffer);
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

void ftejpeg_mem_dest (j_compress_ptr cinfo, vfsfile_t *vfs)
{
	my_destination_mgr *dest;

	if (cinfo->dest == NULL)
	{	/* first time for this JPEG object? */
		cinfo->dest = (struct jpeg_destination_mgr *)
						(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
						sizeof(my_destination_mgr));
		dest = (my_destination_mgr*) cinfo->dest;
		//    dest->buffer = (JOCTET *)
		//      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
		//				  OUTPUT_BUF_SIZE * sizeof(JOCTET));
	}

	dest = (my_destination_mgr*) cinfo->dest;
	dest->pub.init_destination = init_destination;
	dest->pub.empty_output_buffer = empty_output_buffer;
	dest->pub.term_destination = term_destination;
	dest->pub.free_in_buffer = 0; /* forces fill_input_buffer on first read */
	dest->pub.next_output_byte = NULL; /* until buffer loaded */
	dest->vfs = vfs;
}



METHODDEF(void) jpeg_error_exit (j_common_ptr cinfo)
{
  longjmp(((jpeg_error_mgr_wrapper *) cinfo->err)->setjmp_buffer, 1);
}
qboolean screenshotJPEG(char *filename, enum fs_relative fsroot, int compression, qbyte *screendata, int screenwidth, int screenheight, enum uploadfmt fmt)
{
	qbyte	*buffer;
	vfsfile_t	*outfile;
	jpeg_error_mgr_wrapper jerr;
	struct jpeg_compress_struct cinfo;
	JSAMPROW row_pointer[1];

	//convert in-place if needed.
	//bgr->rgb may require copying out entirely for the first pixel to work properly.
	if (fmt == TF_BGRA32)
	{
		qbyte *in=screendata, *out=screendata;
		size_t sz = screenwidth*screenheight;
		while(sz --> 0)
		{
			int r = in[2];
			int g = in[1];
			int b = in[0];
			out[0] = r;
			out[1] = g;
			out[2] = b;
			in+=4;
			out+=3;
		}
		fmt = TF_RGB24;
	}
	else if (fmt == TF_RGBA32)
	{
		qbyte *in=screendata, *out=screendata;
		size_t sz = screenwidth*screenheight;
		while(sz --> 0)
		{
			int r = in[0];
			int g = in[1];
			int b = in[2];
			out[0] = r;
			out[1] = g;
			out[2] = b;
			in+=4;
			out+=3;
		}
		fmt = TF_RGB24;
	}
	else if (fmt == TF_BGR24)
	{
		qbyte *in=screendata, *out=screendata;
		size_t sz = screenwidth*screenheight;
		while(sz --> 0)
		{
			int r = in[0];
			int g = in[1];
			int b = in[2];
			out[0] = r;
			out[1] = g;
			out[2] = b;
			in+=3;
			out+=3;
		}
		fmt = TF_RGB24;
	}
	else if (fmt != TF_RGB24)
	{
		Con_Printf("screenshotJPEG: image format not supported\n");
		return false;
	}

	if (!LIBJPEG_LOADED())
		return false;

	if (!(outfile = FS_OpenVFS(filename, "wb", fsroot)))
	{
		FS_CreatePath (filename, fsroot);
		if (!(outfile = FS_OpenVFS(filename, "wb", fsroot)))
		{
			Con_Printf("Error opening %s\n", filename);
			return false;
		}
	}

	cinfo.err = qjpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jpeg_error_exit;
	if (setjmp(jerr.setjmp_buffer))
	{
		qjpeg_destroy_compress(&cinfo);
		VFS_CLOSE(outfile);
		FS_Remove(filename, FS_GAME);
		Con_Printf("Failed to create jpeg\n");
		return false;
	}
	qjpeg_create_compress(&cinfo);

	buffer = screendata;

	ftejpeg_mem_dest(&cinfo, outfile);
	cinfo.image_width = screenwidth;
	cinfo.image_height = screenheight;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	qjpeg_set_defaults(&cinfo);
	qjpeg_set_quality (&cinfo, bound(0, compression, 100), true);
	qjpeg_start_compress(&cinfo, true);

	while (cinfo.next_scanline < cinfo.image_height)
	{
		*row_pointer = &buffer[(cinfo.image_height - cinfo.next_scanline - 1) * cinfo.image_width * 3];
		qjpeg_write_scanlines(&cinfo, row_pointer, 1);
	}
	qjpeg_finish_compress(&cinfo);
	VFS_CLOSE(outfile);
	qjpeg_destroy_compress(&cinfo);
	return true;
}
#endif
#endif

#ifndef NPFTE
/*
==============
WritePCXfile
==============
*/
void WritePCXfile (const char *filename, enum fs_relative fsroot, qbyte *data, int width, int height,
	int rowbytes, qbyte *palette, qboolean upload) //data is 8bit.
{
	int		i, j, length;
	pcx_t	*pcx;
	qbyte		*pack;

	pcx = Hunk_TempAlloc (width*height*2+1000);
	if (pcx == NULL)
	{
		Con_Printf("WritePCXfile: not enough memory\n");
		return;
	}

	pcx->manufacturer = 0x0a;	// PCX id
	pcx->version = 5;			// 256 color
 	pcx->encoding = 1;		// uncompressed
	pcx->bits_per_pixel = 8;		// 256 color
	pcx->xmin = 0;
	pcx->ymin = 0;
	pcx->xmax = LittleShort((short)(width-1));
	pcx->ymax = LittleShort((short)(height-1));
	pcx->hres = LittleShort((short)width);
	pcx->vres = LittleShort((short)height);
	Q_memset (pcx->palette,0,sizeof(pcx->palette));
	pcx->color_planes = 1;		// chunky image
	pcx->bytes_per_line = LittleShort((short)width);
	pcx->palette_type = LittleShort(2);		// not a grey scale
	Q_memset (pcx->filler,0,sizeof(pcx->filler));

// pack the image
	pack = (qbyte *)(pcx+1);

	data += rowbytes * (height - 1);

	for (i=0 ; i<height ; i++)
	{
		for (j=0 ; j<width ; j++)
		{
			if ( (*data & 0xc0) != 0xc0)
				*pack++ = *data++;
			else
			{
				*pack++ = 0xc1;
				*pack++ = *data++;
			}
		}

		data += rowbytes - width;
		data -= rowbytes * 2;
	}

// write the palette
	*pack++ = 0x0c;	// palette ID qbyte
	for (i=0 ; i<768 ; i++)
		*pack++ = *palette++;

// write output file
	length = pack - (qbyte *)pcx;

	if (upload)
		CL_StartUpload((void *)pcx, length);
	else
		COM_WriteFile (filename, fsroot, pcx, length);
}
#endif


/*
============
LoadPCX
============
*/
qbyte *ReadPCXFile(qbyte *buf, int length, int *width, int *height)
{
	pcx_t	*pcx;
//	pcx_t pcxbuf;
	qbyte	*palette;
	qbyte	*pix;
	int		x, y;
	int		dataByte, runLength;
	int		count;
	qbyte *data;

	qbyte	*pcx_rgb;

	unsigned short xmin, ymin, swidth, sheight;

//
// parse the PCX file
//

	if (length < sizeof(*pcx))
		return NULL;

	pcx = (pcx_t *)buf;

	xmin = LittleShort(pcx->xmin);
	ymin = LittleShort(pcx->ymin);
	swidth = LittleShort(pcx->xmax)-xmin+1;
	sheight = LittleShort(pcx->ymax)-ymin+1;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| swidth >= 1024
		|| sheight >= 1024)
	{
		return NULL;
	}

	*width = swidth;
	*height = sheight;

#ifndef NPFTE
	if (r_dodgypcxfiles.value)
		palette = host_basepal;
	else
#endif
		palette = buf + length-768;

	data = (char *)(pcx+1);

	count = (swidth) * (sheight);
	pcx_rgb = BZ_Malloc( count * 4);

	for (y=0 ; y<sheight ; y++)
	{
		pix = pcx_rgb + 4*y*(swidth);
		for (x=0 ; x<swidth ; )
		{
			dataByte = *data;
			data++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				if (x+runLength>swidth)
				{
					Con_Printf("corrupt pcx\n");
					BZ_Free(pcx_rgb);
					return NULL;
				}
				dataByte = *data;
				data++;
			}
			else
				runLength = 1;

			while(runLength-- > 0)
			{
				pix[0] = palette[dataByte*3];
				pix[1] = palette[dataByte*3+1];
				pix[2] = palette[dataByte*3+2];
				pix[3] = 255;
				if (dataByte == 255)
				{
					pix[0] = 0;	//linear filtering can mean transparent pixel colours are visible. black is a more neutral colour.
					pix[1] = 0;
					pix[2] = 0;
					pix[3] = 0;
				}
				pix += 4;
				x++;
			}
		}
	}

	return pcx_rgb;
}

qbyte *ReadPCXData(qbyte *buf, int length, int width, int height, qbyte *result)
{
	pcx_t	*pcx;
//	pcx_t pcxbuf;
//	qbyte	*palette;
	qbyte	*pix;
	int		x, y;
	int		dataByte, runLength;
//	int		count;
	qbyte *data;

	unsigned short xmin, ymin, swidth, sheight;

//
// parse the PCX file
//

	pcx = (pcx_t *)buf;

	xmin = LittleShort(pcx->xmin);
	ymin = LittleShort(pcx->ymin);
	swidth = LittleShort(pcx->xmax)-xmin+1;
	sheight = LittleShort(pcx->ymax)-ymin+1;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8)
	{
		return NULL;
	}

	if (width != swidth ||
		height > sheight)
	{
		Con_Printf("unsupported pcx size\n");
		return NULL;	//we can't feed the requester with enough info
	}

	data = (char *)(pcx+1);

	for (y=0 ; y<height ; y++)
	{
		pix = result + y*swidth;
		for (x=0 ; x<swidth ; )
		{
			dataByte = *data;
			data++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				if (x+runLength>swidth)
				{
					Con_Printf("corrupt pcx\n");
					return NULL;
				}
				dataByte = *data;
				data++;
			}
			else
				runLength = 1;

			while(runLength-- > 0)
			{
				*pix++ = dataByte;
				x++;
			}
		}
	}

	return result;
}

qbyte *ReadPCXPalette(qbyte *buf, int len, qbyte *out)
{
	pcx_t	*pcx;


//
// parse the PCX file
//

	pcx = (pcx_t *)buf;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| LittleShort(pcx->xmax) >= 1024
		|| LittleShort(pcx->ymax) >= 1024)
	{
		return NULL;
	}

	memcpy(out, (qbyte *)pcx + len - 768, 768);

	return out;
}


typedef struct bmpheader_s
{
/*	unsigned short	Type;*/
	unsigned long	Size;
	unsigned short	Reserved1;
	unsigned short	Reserved2;
	unsigned long	OffsetofBMPBits;
	unsigned long	SizeofBITMAPINFOHEADER;
	signed long		Width;
	signed long		Height;
	unsigned short	Planes;
	unsigned short	BitCount;
	unsigned long	Compression;
	unsigned long	ImageSize;
	signed long		TargetDeviceXRes;
	signed long		TargetDeviceYRes;
	unsigned long	NumofColorIndices;
	unsigned long	NumofImportantColorIndices;
} bmpheader_t;

qbyte *ReadBMPFile(qbyte *buf, int length, int *width, int *height)
{
	unsigned int i;
	bmpheader_t h;
	qbyte *data;

	if (buf[0] != 'B' || buf[1] != 'M')
		return NULL;

	memcpy(&h, (bmpheader_t *)(buf+2), sizeof(h));	
	h.Size = LittleLong(h.Size);
	h.Reserved1 = LittleShort(h.Reserved1);
	h.Reserved2 = LittleShort(h.Reserved2);
	h.OffsetofBMPBits = LittleLong(h.OffsetofBMPBits);
	h.SizeofBITMAPINFOHEADER = LittleLong(h.SizeofBITMAPINFOHEADER);
	h.Width = LittleLong(h.Width);
	h.Height = LittleLong(h.Height);
	h.Planes = LittleShort(h.Planes);
	h.BitCount = LittleShort(h.BitCount);
	h.Compression = LittleLong(h.Compression);
	h.ImageSize = LittleLong(h.ImageSize);
	h.TargetDeviceXRes = LittleLong(h.TargetDeviceXRes);
	h.TargetDeviceYRes = LittleLong(h.TargetDeviceYRes);
	h.NumofColorIndices = LittleLong(h.NumofColorIndices);
	h.NumofImportantColorIndices = LittleLong(h.NumofImportantColorIndices);

	if (h.Compression)	//probably RLE?
		return NULL;

	*width = h.Width;
	*height = h.Height;

	if (h.NumofColorIndices != 0 || h.BitCount == 8)	//8 bit
	{
		int x, y;
		unsigned int *data32;
		unsigned int	pal[256];
		if (!h.NumofColorIndices)
			h.NumofColorIndices = (int)pow(2, h.BitCount);
		if (h.NumofColorIndices>256)
			return NULL;

		data = buf+2;
		data += sizeof(h);

		for (i = 0; i < h.NumofColorIndices; i++)
		{
			pal[i] = data[i*4+2] + (data[i*4+1]<<8) + (data[i*4+0]<<16) + (255/*data[i*4+3]*/<<24);
		}

		buf += h.OffsetofBMPBits;
		data32 = BZ_Malloc(h.Width * h.Height*4);
		for (y = 0; y < h.Height; y++)
		{
			i = (h.Height-1-y) * (h.Width);
			for (x = 0; x < h.Width; x++)
			{
				data32[i] = pal[buf[x]];
				i++;
			}
			buf += h.Width;
		}

		return (qbyte *)data32;
	}
	else if (h.BitCount == 4)	//4 bit
	{
		int x, y;
		unsigned int *data32;
		unsigned int	pal[16];
		if (!h.NumofColorIndices)
			h.NumofColorIndices = (int)pow(2, h.BitCount);
		if (h.NumofColorIndices>16)
			return NULL;
		if (h.Width&1)
			return NULL;

		data = buf+2;
		data += sizeof(h);

		for (i = 0; i < h.NumofColorIndices; i++)
		{
			pal[i] = data[i*4+0] + (data[i*4+1]<<8) + (data[i*4+2]<<16) + (255/*data[i*4+3]*/<<16);
		}

		buf += h.OffsetofBMPBits;
		data32 = BZ_Malloc(h.Width * h.Height*4);
		for (y = 0; y < h.Height; y++)
		{
			i = (h.Height-1-y) * (h.Width);
			for (x = 0; x < h.Width/2; x++)
			{
				data32[i++] = pal[buf[x]>>4];
				data32[i++] = pal[buf[x]&15];
			}
			buf += h.Width>>1;
		}

		return (qbyte *)data32;
	}
	else if (h.BitCount == 24)	//24 bit... no 16?
	{
		int x, y;
		buf += h.OffsetofBMPBits;
		data = BZ_Malloc(h.Width * h.Height*4);
		for (y = 0; y < h.Height; y++)
		{
			i = (h.Height-1-y) * (h.Width);
			for (x = 0; x < h.Width; x++)
			{
				data[i*4+0] = buf[x*3+2];
				data[i*4+1] = buf[x*3+1];
				data[i*4+2] = buf[x*3+0];
				data[i*4+3] = 255;
				i++;
			}
			buf += h.Width*3;
		}

		return data;
	}
	else
		return NULL;

	return NULL;
}

/*void WriteBMPFile(char *filename, qbyte *in, int width, int height)
{
	unsigned int i;
	bmpheader_t *h;
	qbyte *data;
	qbyte *out;

	out = BZ_Malloc(sizeof(bmpheader_t)+width*3*height);



	*(short*)((qbyte *)h-2) = *(short*)"BM";
	h->Size = LittleLong(in->Size);
	h->Reserved1 = LittleShort(in->Reserved1);
	h->Reserved2 = LittleShort(in->Reserved2);
	h->OffsetofBMPBits = LittleLong(in->OffsetofBMPBits);
	h->SizeofBITMAPINFOHEADER = LittleLong(in->SizeofBITMAPINFOHEADER);
	h->Width = LittleLong(in->Width);
	h->Height = LittleLong(in->Height);
	h->Planes = LittleShort(in->Planes);
	h->BitCount = LittleShort(in->BitCount);
	h->Compression = LittleLong(in->Compression);
	h->ImageSize = LittleLong(in->ImageSize);
	h->TargetDeviceXRes = LittleLong(in->TargetDeviceXRes);
	h->TargetDeviceYRes = LittleLong(in->TargetDeviceYRes);
	h->NumofColorIndices = LittleLong(in->NumofColorIndices);
	h->NumofImportantColorIndices = LittleLong(in->NumofImportantColorIndices);

	if (h.Compression)	//probably RLE?
		return NULL;

	*width = h.Width;
	*height = h.Height;

	if (h.NumofColorIndices != 0 || h.BitCount == 8)	//8 bit
	{
		int x, y;
		unsigned int *data32;
		unsigned int	pal[256];
		if (!h.NumofColorIndices)
			h.NumofColorIndices = (int)pow(2, h.BitCount);
		if (h.NumofColorIndices>256)
			return NULL;

		data = buf+2;
		data += sizeof(h);

		for (i = 0; i < h.NumofColorIndices; i++)
		{
			pal[i] = data[i*4+0] + (data[i*4+1]<<8) + (data[i*4+2]<<16) + (255/<<16);
		}

		buf += h.OffsetofBMPBits;
		data32 = BZ_Malloc(h.Width * h.Height*4);
		for (y = 0; y < h.Height; y++)
		{
			i = (h.Height-1-y) * (h.Width);
			for (x = 0; x < h.Width; x++)
			{
				data32[i] = pal[buf[x]];
				i++;
			}
			buf += h.Width;
		}

		return (qbyte *)data32;
	}
	else if (h.BitCount == 4)	//4 bit
	{
		int x, y;
		unsigned int *data32;
		unsigned int	pal[16];
		if (!h.NumofColorIndices)
			h.NumofColorIndices = (int)pow(2, h.BitCount);
		if (h.NumofColorIndices>16)
			return NULL;
		if (h.Width&1)
			return NULL;

		data = buf+2;
		data += sizeof(h);

		for (i = 0; i < h.NumofColorIndices; i++)
		{
			pal[i] = data[i*4+0] + (data[i*4+1]<<8) + (data[i*4+2]<<16) + (255<<16);
		}

		buf += h.OffsetofBMPBits;
		data32 = BZ_Malloc(h.Width * h.Height*4);
		for (y = 0; y < h.Height; y++)
		{
			i = (h.Height-1-y) * (h.Width);
			for (x = 0; x < h.Width/2; x++)
			{
				data32[i++] = pal[buf[x]>>4];
				data32[i++] = pal[buf[x]&15];
			}
			buf += h.Width>>1;
		}

		return (qbyte *)data32;
	}
	else if (h.BitCount == 24)	//24 bit... no 16?
	{
		int x, y;
		buf += h.OffsetofBMPBits;
		data = BZ_Malloc(h.Width * h.Height*4);
		for (y = 0; y < h.Height; y++)
		{
			i = (h.Height-1-y) * (h.Width);
			for (x = 0; x < h.Width; x++)
			{
				data[i*4+0] = buf[x*3+2];
				data[i*4+1] = buf[x*3+1];
				data[i*4+2] = buf[x*3+0];
				data[i*4+3] = 255;
				i++;
			}
			buf += h.Width*3;
		}

		return data;
	}
	else
		return NULL;

	return NULL;
}*/


#ifndef NPFTE


// saturate function, stolen from jitspoe
void SaturateR8G8B8(qbyte *data, int size, float sat)
{
	int i;
	float r, g, b, v;

	if (sat > 1)
	{
		for(i=0; i < size; i+=3)
		{
			r = data[i];
			g = data[i+1];
			b = data[i+2];

			v = r * NTSC_RED + g * NTSC_GREEN + b * NTSC_BLUE;
			r = v + (r - v) * sat;
			g = v + (g - v) * sat;
			b = v + (b - v) * sat;

			// bounds check
			if (r < 0)
				r = 0;
			else if (r > 255)
				r = 255;

			if (g < 0)
				g = 0;
			else if (g > 255)
				g = 255;

			if (b < 0)
				b = 0;
			else if (b > 255)
				b = 255;

			// scale down to avoid overbright lightmaps
			v = v / (r * NTSC_RED + g * NTSC_GREEN + b * NTSC_BLUE);
			if (v > NTSC_SUM)
				v = NTSC_SUM;
			else
				v *= v;

			data[i]   = r*v;
			data[i+1] = g*v;
			data[i+2] = b*v;
		}
	}
	else // avoid bounds check for desaturation
	{
		if (sat < 0)
			sat = 0;

		for(i=0; i < size; i+=3)
		{
			r = data[i];
			g = data[i+1];
			b = data[i+2];

			v = r * NTSC_RED + g * NTSC_GREEN + b * NTSC_BLUE;

			data[i]   = v + (r - v) * sat;
			data[i+1] = v + (g - v) * sat;
			data[i+2] = v + (b - v) * sat;
		}
	}
}

void BoostGamma(qbyte *rgba, int width, int height)
{
	//note: should not be used where hardware gamma is supported.
	int i;
	extern qbyte gammatable[256];

	for (i=0 ; i<width*height*4 ; i+=4)
	{
		rgba[i+0] = gammatable[rgba[i+0]];
		rgba[i+1] = gammatable[rgba[i+1]];
		rgba[i+2] = gammatable[rgba[i+2]];
		//and not alpha
	}
}


static void Image_LoadTexture_Failed(void *ctx, void *data, size_t a, size_t b)
{
	texid_t tex = ctx;
	tex->status = TEX_FAILED;
}
static void Image_LoadTextureMips(void *ctx, void *data, size_t a, size_t b)
{
	int i;
	texid_t tex = ctx;
	struct pendingtextureinfo *mips = data;

	//setting the dimensions here can break npot textures, so lets not do that.
//	tex->width = mips->mip[0].width;
//	tex->height = mips->mip[0].height;

	if (rf->IMG_LoadTextureMips(tex, mips))
		tex->status = TEX_LOADED;
	else
		tex->status = TEX_FAILED;

	for (i = 0; i < mips->mipcount; i++)
		if (mips->mip[i].needfree)
			BZ_Free(mips->mip[i].data);
	if (mips->extrafree)
		BZ_Free(mips->extrafree);
	BZ_Free(mips);

	//ezhud breaks without this. I assume other things will too. this is why you shouldn't depend upon querying an image's size.
	if (!strncmp(tex->ident, "gfx/", 4))
	{
		size_t lumpsize;
		qpic_t *pic = W_SafeGetLumpName(tex->ident+4, &lumpsize);
		if (pic && lumpsize == 8 + pic->width*pic->height)
		{
			tex->width = pic->width;
			tex->height = pic->height;
		}
	}
	//FIXME: check loaded wad files too.
}

#ifdef IMAGEFMT_DDS
typedef struct {
	unsigned int dwSize;
	unsigned int dwFlags;
	unsigned int dwFourCC;

	unsigned int unk[5];
} ddspixelformat;

typedef struct {
	unsigned int dwSize;
	unsigned int dwFlags;
	unsigned int dwHeight;
	unsigned int dwWidth;
	unsigned int dwPitchOrLinearSize;
	unsigned int dwDepth;
	unsigned int dwMipMapCount;
	unsigned int dwReserved1[11];
	ddspixelformat ddpfPixelFormat;
	unsigned int ddsCaps[4];
	unsigned int dwReserved2;
} ddsheader;


static qboolean Image_ReadDDSFile(texid_t tex, unsigned int flags, char *fname, qbyte *filedata, size_t filesize)
{
	int nummips;
	int mipnum;
	int datasize;
	int pad;
	unsigned int w, h;
	int divsize, blocksize;
	struct pendingtextureinfo *mips;
	int encoding;

	ddsheader fmtheader;
	if (*(int*)filedata != (('D'<<0)|('D'<<8)|('S'<<16)|(' '<<24)))
		return false;

	memcpy(&fmtheader, filedata+4, sizeof(fmtheader));
	if (fmtheader.dwSize != sizeof(fmtheader))
		return false;	//corrupt/different version

	nummips = fmtheader.dwMipMapCount;
	if (nummips < 1)
		nummips = 1;

	if (*(int*)&fmtheader.ddpfPixelFormat.dwFourCC == (('D'<<0)|('X'<<8)|('T'<<16)|('1'<<24)))
	{
		encoding = PTI_S3RGBA1;	//alpha or not? Assume yes, and let the drivers decide.
		pad = 8;
		divsize = 4;
		blocksize = 8;
	}
	else if (*(int*)&fmtheader.ddpfPixelFormat.dwFourCC == (('D'<<0)|('X'<<8)|('T'<<16)|('2'<<24)))	//dx3 with premultiplied alpha
	{
//		if (!(tex->flags & IF_PREMULTIPLYALPHA))
			return false;
		encoding = PTI_S3RGBA3;
		pad = 8;
		divsize = 4;
		blocksize = 16;
	}
	else if (*(int*)&fmtheader.ddpfPixelFormat.dwFourCC == (('D'<<0)|('X'<<8)|('T'<<16)|('3'<<24)))
	{
		if (tex->flags & IF_PREMULTIPLYALPHA)
			return false;
		encoding = PTI_S3RGBA3;
		pad = 8;
		divsize = 4;
		blocksize = 16;
	}
	else if (*(int*)&fmtheader.ddpfPixelFormat.dwFourCC == (('D'<<0)|('X'<<8)|('T'<<16)|('4'<<24)))	//dx5 with premultiplied alpha
	{
//		if (!(tex->flags & IF_PREMULTIPLYALPHA))
			return false;
		encoding = PTI_S3RGBA5;
		pad = 8;
		divsize = 4;
		blocksize = 16;
	}
	else if (*(int*)&fmtheader.ddpfPixelFormat.dwFourCC == (('D'<<0)|('X'<<8)|('T'<<16)|('5'<<24)))
	{
		if (tex->flags & IF_PREMULTIPLYALPHA)
			return false;

		encoding = PTI_S3RGBA5;
		pad = 8;
		divsize = 4;
		blocksize = 16;
	}
	else
	{
		Con_Printf("Unsupported dds fourcc in %s\n", fname);
		return false;
	}

	mips = Z_Malloc(sizeof(*mips));
	mips->mipcount = 0;
	mips->type = PTI_2D;
	mips->extrafree = filedata;
	mips->encoding = encoding;

	filedata += 4+fmtheader.dwSize;

	datasize = fmtheader.dwPitchOrLinearSize;
	w = fmtheader.dwWidth;
	h = fmtheader.dwHeight;
	for (mipnum = 0; mipnum < nummips; mipnum++)
	{
		if (nummips >= countof(mips->mip))
			break;

		if (datasize < pad)
			datasize = pad;
		datasize = ((w+divsize-1)/divsize) * ((h+divsize-1)/divsize) * blocksize;

		mips->mip[mipnum].data = filedata;
		mips->mip[mipnum].datasize = datasize;
		mips->mip[mipnum].width = w;
		mips->mip[mipnum].height = h;
		filedata += datasize;
		w = (w+1)>>1;
		h = (h+1)>>1;
	}

	mips->mipcount = mipnum;

	if (flags & IF_NOWORKER)
		Image_LoadTextureMips(tex, mips, 0, 0);
	else
		COM_AddWork(WG_MAIN, Image_LoadTextureMips, tex, mips, 0, 0);
	return true;
}
#endif

#ifdef IMAGEFMT_BLP
static qboolean Image_ReadBLPFile(texid_t tex, unsigned int flags, char *fname, qbyte *filedata, size_t filesize)
{
	//FIXME: cba with endian.
	int miplevel;
	int w, h, i;
	struct blp_s
	{
		char blp2[4];
		int type;
		qbyte encoding;
		qbyte alphadepth;
		qbyte alphaencoding;
		qbyte hasmips;
		unsigned int xres;
		unsigned int yres;
		unsigned int mipoffset[16];
		unsigned int mipsize[16];
		unsigned int palette[256];
	} *blp;
	unsigned int *tmpmem = NULL;
	unsigned char *in;
	unsigned int inlen;

	struct pendingtextureinfo *mips;

	blp = (void*)filedata;

	if (memcmp(blp->blp2, "BLP2", 4) || blp->type != 1)
		return false;

	mips = Z_Malloc(sizeof(*mips));
	mips->mipcount = 0;
	mips->type = PTI_2D;

	w = LittleLong(blp->xres);
	h = LittleLong(blp->yres);

	if (blp->encoding == 2)
	{
		//s3tc/dxt
		switch(blp->alphaencoding)
		{
		default:
		case 0: //dxt1
			if (blp->alphadepth)
				mips->encoding = PTI_S3RGBA1;
			else
				mips->encoding = PTI_S3RGB1;
			break;
		case 1: //dxt2/3
			mips->encoding = PTI_S3RGBA3;
			break;
		case 7: //dxt4/5
			mips->encoding = PTI_S3RGBA5;
			break;
		}
		for (miplevel = 0; miplevel < 16; )
		{
			if (!w && !h)	//shrunk to no size
				break;
			if (!w)
				w = 1;
			if (!h)
				h = 1;
			if (!blp->mipoffset[miplevel] || !blp->mipsize[miplevel] || blp->mipoffset[miplevel]+blp->mipsize[miplevel] > filesize)	//no data
				break;
			mips->mip[miplevel].width = w;
			mips->mip[miplevel].height = h;
			mips->mip[miplevel].data = filedata + LittleLong(blp->mipoffset[miplevel]);
			mips->mip[miplevel].datasize = LittleLong(blp->mipsize[miplevel]);

			miplevel++;
			if (!blp->hasmips || (flags & IF_NOMIPMAP))
				break;
			w >>= 1;
			h >>= 1;
		}
		mips->mipcount = miplevel;
		mips->extrafree = filedata;
	}
	else
	{
		mips->encoding = PTI_BGRA8;
		for (miplevel = 0; miplevel < 16; )
		{
			if (!w && !h)
				break;
			if (!w)
				w = 1;
			if (!h)
				h = 1;
			//if we ran out of mips to load, give up.
			if (!blp->mipoffset[miplevel] || !blp->mipsize[miplevel] || blp->mipoffset[miplevel]+blp->mipsize[miplevel] > filesize)
			{
				//if we got at least one mip, cap the mips. might help save some ram? naaah...
				//if this is the first mip, well, its completely fucked.
				break;
			}

			in = filedata + LittleLong(blp->mipoffset[miplevel]);
			inlen = LittleLong(blp->mipsize[miplevel]);

			if (inlen != w*h+((w*h*blp->alphadepth+7)>>3))
			{
				Con_Printf("%s: mip level %i does not contain the correct amount of data\n", fname, miplevel);
				break;
			}

			mips->mip[miplevel].width = w;
			mips->mip[miplevel].height = h;
			mips->mip[miplevel].datasize = 4*w*h;
			mips->mip[miplevel].data = tmpmem = BZ_Malloc(4*w*h);
			mips->mip[miplevel].needfree = true;

			//load the rgb data first (8-bit paletted)
			for (i = 0; i < w*h; i++)
				tmpmem[i] = blp->palette[*in++] | 0xff000000;

			//and then change the alpha bits accordingly.
			switch(blp->alphadepth)
			{
			case 0:
				//BGRX palette, 8bit
				break;
			case 1:
				//BGRX palette, 8bit
				//1bit trailing alpha
				for (i = 0; i < w*h; i+=8, in++)
				{
					tmpmem[i+0] = (tmpmem[i+0] & 0xffffff) | ((*in&0x01)?0xff000000:0);
					tmpmem[i+1] = (tmpmem[i+1] & 0xffffff) | ((*in&0x02)?0xff000000:0);
					tmpmem[i+2] = (tmpmem[i+2] & 0xffffff) | ((*in&0x04)?0xff000000:0);
					tmpmem[i+3] = (tmpmem[i+3] & 0xffffff) | ((*in&0x08)?0xff000000:0);
					tmpmem[i+4] = (tmpmem[i+4] & 0xffffff) | ((*in&0x10)?0xff000000:0);
					tmpmem[i+5] = (tmpmem[i+5] & 0xffffff) | ((*in&0x20)?0xff000000:0);
					tmpmem[i+6] = (tmpmem[i+6] & 0xffffff) | ((*in&0x40)?0xff000000:0);
					tmpmem[i+7] = (tmpmem[i+7] & 0xffffff) | ((*in&0x80)?0xff000000:0);
				}
				break;
			case 4:
				//BGRX palette, 8bit
				//4bit trailing alpha
				for (i = 0; i < w*h; i++)
					tmpmem[i] = (tmpmem[i] & 0xffffff) | (*in++*0x11000000);
				break;
			case 8:
				//BGRX palette, 8bit
				//8bit trailing alpha
				for (i = 0; i < w*h; i++)
					tmpmem[i] = (tmpmem[i] & 0xffffff) | (*in++<<24);
				break;
			}

			miplevel++;
			if (!blp->hasmips || (flags & IF_NOMIPMAP))
				break;
			w = w>>1;
			h = h>>1;
		}
		BZ_Free(filedata);
		mips->mipcount = miplevel;
	}

	COM_AddWork(WG_MAIN, Image_LoadTextureMips, tex, mips, 0, 0);

	return true;
}
#endif

//returns r8g8b8a8
qbyte *Read32BitImageFile(qbyte *buf, int len, int *width, int *height, qboolean *hasalpha, const char *fname)
{
	qbyte *data;
	if ((data = ReadTargaFile(buf, len, width, height, hasalpha, false)))
	{
		TRACE(("dbg: Read32BitImageFile: tga\n"));
		return data;
	}

#ifdef AVAIL_PNGLIB
	if (len > 4 && (buf[0] == 137 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G') && (data = ReadPNGFile(buf, len, width, height, fname)))
	{
		TRACE(("dbg: Read32BitImageFile: png\n"));
		return data;
	}
#endif
#ifdef AVAIL_JPEGLIB
	//jpeg jfif only.
	if (len > 4 && (buf[0] == 0xff && buf[1] == 0xd8 && buf[2] == 0xff /*&& buf[3] == 0xe0*/) && (data = ReadJPEGFile(buf, len, width, height)))
	{
		TRACE(("dbg: Read32BitImageFile: jpeg\n"));
		return data;
	}
#endif
	if ((data = ReadPCXFile(buf, len, width, height)))
	{
		TRACE(("dbg: Read32BitImageFile: pcx\n"));
		return data;
	}

	if (len > 2 && (buf[0] == 'B' && buf[1] == 'M') && (data = ReadBMPFile(buf, len, width, height)))
	{
		TRACE(("dbg: Read32BitImageFile: bitmap\n"));
		return data;
	}

	if (len >= 8)	//.lmp has no magic id. guess at it.
	{
		int w = LittleLong(((int*)buf)[0]);
		int h = LittleLong(((int*)buf)[1]);
		int i;
		if (w >= 3 && h >= 4 && w*h+sizeof(int)*2 == len)
		{
			qboolean foundalpha = false;
			qbyte *in = (qbyte*)((int*)buf+2);
			data = BZ_Malloc(w * h * sizeof(int));
			for (i = 0; i < w * h; i++)
			{
				if (in[i] == 255)
					foundalpha = true;
				((unsigned int*)data)[i] = d_8to24rgbtable[in[i]];
			}
			*width = w;
			*height = h;
			*hasalpha = foundalpha;
			return data;
		}
	}

	TRACE(("dbg: Read32BitImageFile: life sucks\n"));

	return NULL;
}

static void *R_FlipImage32(void *in, int *inoutwidth, int *inoutheight, qboolean flipx, qboolean flipy, qboolean flipd)
{
	int x, y;
	unsigned int *in32, *inr, *out32;
	void *out;
	int inwidth = *inoutwidth;
	int inheight = *inoutheight;
	int rowstride = inwidth;
	int colstride = 1;

	//simply return if no operation
	if (!flipx && !flipy && !flipd)
		return in;

	inr = in;
	out32 = out = BZ_Malloc(inwidth*inheight*4);

	if (flipy)
	{
		inr += inwidth*inheight-inwidth;//start on the bottom row
		rowstride *= -1;	//and we need to move up instead
	}
	if (flipx)
	{
		colstride *= -1;	//move backwards
		inr += inwidth-1;	//start at the end of the row
	}
	if (flipd)
	{ 
		//switch the dimensions
		int tmp = inwidth;
		inwidth = inheight;
		inheight = tmp;
		//make sure the caller gets the new dimensions
		*inoutwidth = inwidth;
		*inoutheight = inheight;
		//switch the strides
		tmp = colstride;
		colstride = rowstride;
		rowstride = tmp;
	}

	//rows->rows, columns->columns
	for (y = 0; y < inheight; y++)
	{
		in32 = inr;	//reset the input after each row, so we have truely independant row+column strides
		inr += rowstride;
		for (x = 0; x < inheight; x++)
		{
			*out32++ = *in32;
			in32 += colstride;
		}
	}
	BZ_Free(in);
	return out;
}

int tex_extensions_count;
#define tex_extensions_max 15
static struct
{
	char name[6];
} tex_extensions[tex_extensions_max];
static void QDECL R_ImageExtensions_Callback(struct cvar_s *var, char *oldvalue)
{
	char *v = var->string;
	tex_extensions_count = 0;

	while (tex_extensions_count < tex_extensions_max)
	{
		v = COM_Parse(v);
		if (!v)
			break;
		Q_snprintfz(tex_extensions[tex_extensions_count].name, sizeof(tex_extensions[tex_extensions_count].name), ".%s", com_token); 
		tex_extensions_count++;
	}

	if (tex_extensions_count < tex_extensions_max)
	{
		Q_snprintfz(tex_extensions[tex_extensions_count].name, sizeof(tex_extensions[tex_extensions_count].name), ""); 
		tex_extensions_count++;
	}
}

static struct
{
	int args;
	char *path;

	int enabled;
} tex_path[] =
{
	/*if three args, first is the subpath*/
	/*the last two args are texturename then extension*/
	{2, "%s%s", 1},				/*directly named texture*/
	{3, "textures/%s/%s%s", 1},	/*fuhquake compatibility*/
	{3, "%s/%s%s", 1},			/*fuhquake compatibility*/
	{2, "textures/%s%s", 1},	/*directly named texture with textures/ prefix*/
#ifndef NOLEGACY
	{2, "override/%s%s", 1}		/*tenebrae compatibility*/
#endif
};

static void Image_MipMap8888 (qbyte *in, int inwidth, int inheight, qbyte *out, int outwidth, int outheight)
{
	int		i, j;
	qbyte	*inrow;

	int rowwidth = inwidth*4;	//rowwidth is the byte width of the input
	inrow = in;

	//mips round down, except for when the input is 1. which bugs out.
	if (inwidth <= 1 && inheight <= 1)
	{
		out[0] = in[0];
		out[1] = in[1];
		out[2] = in[2];
		out[3] = in[3];
	}
	else if (inheight <= 1)
	{
		//single row, don't peek at the next
		for (in = inrow, j=0 ; j<outwidth ; j++, out+=4, in+=8)
		{
			out[0] = (in[0] + in[4])>>1;
			out[1] = (in[1] + in[5])>>1;
			out[2] = (in[2] + in[6])>>1;
			out[3] = (in[3] + in[7])>>1;
		}
	}
	else if (inwidth <= 1)
	{
		//single colum, peek only at this pixel
		for (i=0 ; i<outheight ; i++, inrow+=rowwidth*2)
		{
			for (in = inrow, j=0 ; j<outwidth ; j++, out+=4, in+=8)
			{
				out[0] = (in[0] + in[rowwidth+0])>>1;
				out[1] = (in[1] + in[rowwidth+1])>>1;
				out[2] = (in[2] + in[rowwidth+2])>>1;
				out[3] = (in[3] + in[rowwidth+3])>>1;
			}
		}
	}
	else
	{
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
}

static qbyte Image_BlendPalette_2(qbyte a, qbyte b)
{
	return a;
}
static qbyte Image_BlendPalette_4(qbyte a, qbyte b, qbyte c, qbyte d)
{
	return a;
}
//this is expected to be slow, thanks to those two expensive helpers.
static void Image_MipMap8Pal (qbyte *in, int inwidth, int inheight, qbyte *out, int outwidth, int outheight)
{
	int		i, j;
	qbyte	*inrow;

	int rowwidth = inwidth;	//rowwidth is the byte width of the input
	inrow = in;

	//mips round down, except for when the input is 1. which bugs out.
	if (inwidth <= 1 && inheight <= 1)
		out[0] = in[0];
	else if (inheight <= 1)
	{
		//single row, don't peek at the next
		for (in = inrow, j=0 ; j<outwidth ; j++, out+=1, in+=2)
			out[0] = Image_BlendPalette_2(in[0], in[1]);
	}
	else if (inwidth <= 1)
	{
		//single colum, peek only at this pixel
		for (i=0 ; i<outheight ; i++, inrow+=rowwidth*2)
			for (in = inrow, j=0 ; j<outwidth ; j++, out+=1, in+=2)
				out[0] = Image_BlendPalette_2(in[0], in[rowwidth]);
	}
	else
	{
		for (i=0 ; i<outheight ; i++, inrow+=rowwidth*2)
			for (in = inrow, j=0 ; j<outwidth ; j++, out+=1, in+=2)
				out[0] = Image_BlendPalette_4(in[0], in[1], in[rowwidth+0], in[rowwidth+1]);
	}
}

static void Image_GenerateMips(struct pendingtextureinfo *mips, unsigned int flags)
{
	int mip;

	if (mips->type != PTI_2D)
		return;	//blurgh

	if (flags & IF_NOMIPMAP)
		return;

	switch(mips->encoding)
	{
	case PTI_R8:
		if (sh_config.can_mipcap)
			return;	//if we can cap mips, do that. it'll save lots of expensive lookups and uglyness.
		for (mip = mips->mipcount; mip < 32; mip++)
		{
			mips->mip[mip].width = mips->mip[mip-1].width >> 1;
			mips->mip[mip].height = mips->mip[mip-1].height >> 1;
			if (mips->mip[mip].width < 1 && mips->mip[mip].height < 1)
				break;
			if (mips->mip[mip].width < 1)
				mips->mip[mip].width = 1;
			if (mips->mip[mip].height < 1)
				mips->mip[mip].height = 1;
			mips->mip[mip].datasize = ((mips->mip[mip].width+3)&~3) * mips->mip[mip].height*4;
			mips->mip[mip].data = BZ_Malloc(mips->mip[mip].datasize);
			mips->mip[mip].needfree = true;

			Image_MipMap8Pal(mips->mip[mip-1].data, mips->mip[mip-1].width, mips->mip[mip-1].height, mips->mip[mip].data, mips->mip[mip].width, mips->mip[mip].height);
			mips->mipcount = mip+1;
		}
		return;
	case PTI_RGBA8:
	case PTI_RGBX8:
	case PTI_BGRA8:
	case PTI_BGRX8:
		for (mip = mips->mipcount; mip < 32; mip++)
		{
			mips->mip[mip].width = mips->mip[mip-1].width >> 1;
			mips->mip[mip].height = mips->mip[mip-1].height >> 1;
			if (mips->mip[mip].width < 1 && mips->mip[mip].height < 1)
				break;
			if (mips->mip[mip].width < 1)
				mips->mip[mip].width = 1;
			if (mips->mip[mip].height < 1)
				mips->mip[mip].height = 1;
			mips->mip[mip].datasize = ((mips->mip[mip].width+3)&~3) * mips->mip[mip].height*4;
			mips->mip[mip].data = BZ_Malloc(mips->mip[mip].datasize);
			mips->mip[mip].needfree = true;

			Image_MipMap8888(mips->mip[mip-1].data, mips->mip[mip-1].width, mips->mip[mip-1].height, mips->mip[mip].data, mips->mip[mip].width, mips->mip[mip].height);
			mips->mipcount = mip+1;
		}
		break;
	case PTI_RGBA4444:
	case PTI_RGB565:
	case PTI_RGBA5551:
		return;	//convert to 16bit afterwards. always mipmap at 8 bit, to try to preserve what little precision there is.
	default:
		return;	//not supported.
	}
}

//stolen from DP
static void Image_Resample32LerpLine (const qbyte *in, qbyte *out, int inwidth, int outwidth)
{
	int		j, xi, oldx = 0, f, fstep, endx, lerp;
	fstep = (int) (inwidth*65536.0f/outwidth);
	endx = (inwidth-1);
	for (j = 0,f = 0;j < outwidth;j++, f += fstep)
	{
		xi = f >> 16;
		if (xi != oldx)
		{
			in += (xi - oldx) * 4;
			oldx = xi;
		}
		if (xi < endx)
		{
			lerp = f & 0xFFFF;
			*out++ = (qbyte) ((((in[4] - in[0]) * lerp) >> 16) + in[0]);
			*out++ = (qbyte) ((((in[5] - in[1]) * lerp) >> 16) + in[1]);
			*out++ = (qbyte) ((((in[6] - in[2]) * lerp) >> 16) + in[2]);
			*out++ = (qbyte) ((((in[7] - in[3]) * lerp) >> 16) + in[3]);
		}
		else // last pixel of the line has no pixel to lerp to
		{
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
			*out++ = in[3];
		}
	}
}

//yes, this is lordhavok's code too.
//superblur away!
#define LERPBYTE(i) r = row1[i];out[i] = (qbyte) ((((row2[i] - r) * lerp) >> 16) + r)
static void Image_Resample32Lerp(const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight)
{
	int i, j, r, yi, oldy, f, fstep, lerp, endy = (inheight-1), inwidth4 = inwidth*4, outwidth4 = outwidth*4;
	qbyte *out;
	const qbyte *inrow;
	qbyte *tmem, *row1, *row2;

	tmem = row1 = BZ_Malloc(2*(outwidth*4));
	row2 = row1 + (outwidth * 4);

	out = outdata;
	fstep = (int) (inheight*65536.0f/outheight);

	inrow = indata;
	oldy = 0;
	Image_Resample32LerpLine (inrow, row1, inwidth, outwidth);
	Image_Resample32LerpLine (inrow + inwidth4, row2, inwidth, outwidth);
	for (i = 0, f = 0;i < outheight;i++,f += fstep)
	{
		yi = f >> 16;
		if (yi < endy)
		{
			lerp = f & 0xFFFF;
			if (yi != oldy)
			{
				inrow = (qbyte *)indata + inwidth4*yi;
				if (yi == oldy+1)
					memcpy(row1, row2, outwidth4);
				else
					Image_Resample32LerpLine (inrow, row1, inwidth, outwidth);
				Image_Resample32LerpLine (inrow + inwidth4, row2, inwidth, outwidth);
				oldy = yi;
			}
			j = outwidth - 4;
			while(j >= 0)
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				LERPBYTE( 6);
				LERPBYTE( 7);
				LERPBYTE( 8);
				LERPBYTE( 9);
				LERPBYTE(10);
				LERPBYTE(11);
				LERPBYTE(12);
				LERPBYTE(13);
				LERPBYTE(14);
				LERPBYTE(15);
				out += 16;
				row1 += 16;
				row2 += 16;
				j -= 4;
			}
			if (j & 2)
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				LERPBYTE( 6);
				LERPBYTE( 7);
				out += 8;
				row1 += 8;
				row2 += 8;
			}
			if (j & 1)
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				out += 4;
				row1 += 4;
				row2 += 4;
			}
			row1 -= outwidth4;
			row2 -= outwidth4;
		}
		else
		{
			if (yi != oldy)
			{
				inrow = (qbyte *)indata + inwidth4*yi;
				if (yi == oldy+1)
					memcpy(row1, row2, outwidth4);
				else
					Image_Resample32LerpLine (inrow, row1, inwidth, outwidth);
				oldy = yi;
			}
			memcpy(out, row1, outwidth4);
		}
	}
	BZ_Free(tmem);
}


/*
================
GL_ResampleTexture
================
*/
void Image_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight)
{
	int		i, j;
	unsigned	*inrow;
	unsigned	frac, fracstep;

	if (gl_lerpimages.ival)
	{
		Image_Resample32Lerp(in, inwidth, inheight, out, outwidth, outheight);
		return;
	}

	fracstep = inwidth*0x10000/outwidth;
	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(i*inheight/outheight);
		frac = outwidth*fracstep;
		j=outwidth;
		while ((j)&3)
		{
			j--;
			frac -= fracstep;
			out[j] = inrow[frac>>16];
		}
		for ( ; j>=4 ;)
		{
			j-=4;
			frac -= fracstep;
			out[j+3] = inrow[frac>>16];
			frac -= fracstep;
			out[j+2] = inrow[frac>>16];
			frac -= fracstep;
			out[j+1] = inrow[frac>>16];
			frac -= fracstep;
			out[j+0] = inrow[frac>>16];
		}
	}
}

//ripped from tenebrae
static unsigned int * Image_GenerateNormalMap(qbyte *pixels, unsigned int *nmap, int w, int h, float scale, float offsetscale)
{
	int i, j, wr, hr;
	unsigned char r, g, b, height;
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
			height = bound(0, (pixels[i*wr + j]*offsetscale)+(255*(1-offsetscale)), 255);
			nmap[i*w+j] = LittleLong((height << 24)|(b << 16)|(g << 8)|(r));	// <AWE> Added support for big endian.
		}
	}

	return &nmap[0];
}

static void Image_RoundDimensions(int *scaled_width, int *scaled_height, unsigned int flags)
{
	if (sh_config.texture_non_power_of_two)	//NPOT is a simple extension that relaxes errors.
	{
		//lax form
		TRACE(("dbg: GL_RoundDimensions: GL_ARB_texture_non_power_of_two\n"));
	}
	else if ((flags & IF_CLAMP) && (flags & IF_NOMIPMAP) && sh_config.texture_non_power_of_two_pic)
	{
		//more strict form
		TRACE(("dbg: GL_RoundDimensions: GL_OES_texture_npot\n"));
	}
	else
	{
		int width = *scaled_width;
		int height = *scaled_height;
		for (*scaled_width = 1 ; *scaled_width < width ; *scaled_width<<=1)
			;
		for (*scaled_height = 1 ; *scaled_height < height ; *scaled_height<<=1)
			;

		/*round npot textures down if we're running on an embedded system*/
		if (sh_config.npot_rounddown)
		{
			if (*scaled_width != width)
				*scaled_width >>= 1;
			if (*scaled_height != height)
				*scaled_height >>= 1;
		}
	}

	if (!(flags & IF_NOPICMIP))
	{
		if (flags & IF_NOMIPMAP)
		{
			if (gl_picmip2d.ival > 0)
			{
				*scaled_width >>= gl_picmip2d.ival;
				*scaled_height >>= gl_picmip2d.ival;
			}
		}
		else
		{
			if (gl_picmip.ival > 0)
			{
				TRACE(("dbg: GL_RoundDimensions: %f\n", gl_picmip.value));
				*scaled_width >>= gl_picmip.ival;
				*scaled_height >>= gl_picmip.ival;
			}
		}
	}

	TRACE(("dbg: GL_RoundDimensions: %f\n", gl_max_size.value));

	if (sh_config.texture_maxsize)
	{
		if (*scaled_width > sh_config.texture_maxsize)
			*scaled_width = sh_config.texture_maxsize;
		if (*scaled_height > sh_config.texture_maxsize)
			*scaled_height = sh_config.texture_maxsize;
	}
	if (!(flags & (IF_UIPIC|IF_RENDERTARGET)))
	{
		if (gl_max_size.value)
		{
			if (*scaled_width > gl_max_size.value)
				*scaled_width = gl_max_size.value;
			if (*scaled_height > gl_max_size.value)
				*scaled_height = gl_max_size.value;
		}
	}

	if (*scaled_width < 1)
		*scaled_width = 1;
	if (*scaled_height < 1)
		*scaled_height = 1;
}

//may operate in place
static void Image_8888to565(qbyte *in, unsigned short *out, unsigned int w, unsigned int h, qboolean bgra)
{
	unsigned int p = w*h;
	unsigned short tmp;

	if (bgra)
	{
		while(p-->0)
		{
			tmp  = ((*in++>>3) << 0);//b
			tmp |= ((*in++>>2) << 5);//g
			tmp |= ((*in++>>3) << 11);//r
			in++;
			*out++ = tmp;
		}
	}
	else
	{
		while(p-->0)
		{
			tmp  = ((*in++>>3) << 11);//r
			tmp |= ((*in++>>2) << 5);//g
			tmp |= ((*in++>>3) << 0);//b
			in++;
			*out++ = tmp;
		}
	}
}
//may operate in place
static void Image_8888to1555(qbyte *in, unsigned short *out, unsigned int w, unsigned int h, qboolean bgra)
{
	unsigned int p = w*h;
	unsigned short tmp;
	if (bgra)
	{
		while(p-->0)
		{
			tmp  = ((*in++>>3) << 0);//b
			tmp |= ((*in++>>3) << 5);//g
			tmp |= ((*in++>>3) << 10);//r
			tmp |= ((*in++>>7) << 15);//a
			*out++ = tmp;
		}
	}
	else
	{
		while(p-->0)
		{
			tmp  = ((*in++>>3) << 10);//r
			tmp |= ((*in++>>3) << 5);//g
			tmp |= ((*in++>>3) << 0);//b
			tmp |= ((*in++>>7) << 15);//a
			*out++ = tmp;
		}
	}
}
//may operate in place
static void Image_8888to5551(qbyte *in, unsigned short *out, unsigned int w, unsigned int h, qboolean bgra)
{
	unsigned int p = w*h;
	unsigned short tmp;
	if (bgra)
	{
		while(p-->0)
		{
			tmp  = ((*in++>>3) << 1);//b
			tmp |= ((*in++>>3) << 6);//g
			tmp |= ((*in++>>3) << 11);//r
			tmp |= ((*in++>>7) << 0);//a
			*out++ = tmp;
		}
	}
	else
	{
		while(p-->0)
		{
			tmp  = ((*in++>>3) << 11);//r
			tmp |= ((*in++>>3) << 6);//g
			tmp |= ((*in++>>3) << 1);//b
			tmp |= ((*in++>>7) << 0);//a
			*out++ = tmp;
		}
	}
}
//may operate in place
static void Image_8888to4444(qbyte *in, unsigned short *out, unsigned int w, unsigned int h, qboolean bgra)
{
	unsigned int p = w*h;
	unsigned short tmp;

	if (bgra)
	{
		while(p-->0)
		{
			tmp  = ((*in++>>4) << 4);//b
			tmp |= ((*in++>>4) << 8);//g
			tmp |= ((*in++>>4) << 12);//r
			tmp |= ((*in++>>4) << 0);//a
			*out++ = tmp;
		}
	}
	else
	{
		while(p-->0)
		{
			tmp  = ((*in++>>4) << 12);//r
			tmp |= ((*in++>>4) << 8);//g
			tmp |= ((*in++>>4) << 4);//b
			tmp |= ((*in++>>4) << 0);//a
			*out++ = tmp;
		}
	}
}
//may operate in place
static void Image_8888toARGB4444(qbyte *in, unsigned short *out, unsigned int w, unsigned int h, qboolean bgra)
{
	unsigned int p = w*h;
	unsigned short tmp;

	if (bgra)
	{
		while(p-->0)
		{
			tmp  = ((*in++>>4) << 0);//b
			tmp |= ((*in++>>4) << 4);//g
			tmp |= ((*in++>>4) << 8);//r
			tmp |= ((*in++>>4) << 12);//a
			*out++ = tmp;
		}
	}
	else
	{
		while(p-->0)
		{
			tmp  = ((*in++>>4) << 8);//r
			tmp |= ((*in++>>4) << 4);//g
			tmp |= ((*in++>>4) << 0);//b
			tmp |= ((*in++>>4) << 12);//a
			*out++ = tmp;
		}
	}
}

//may operate in place
static void Image_8_BGR_RGB_Swap(qbyte *data, unsigned int w, unsigned int h)
{
	unsigned int p = w*h;
	qbyte tmp;
	while(p-->0)
	{
		tmp = data[0];
		data[0] = data[2];
		data[2] = tmp;
		data += 4;
	}
}

static void Image_ChangeFormat(struct pendingtextureinfo *mips, unsigned int flags, uploadfmt_t origfmt)
{
	int mip;

	if (mips->type != PTI_2D)
		return;	//blurgh

	if (flags & IF_PALETTIZE)
	{
		if (mips->encoding == PTI_RGBX8)
		{
			mips->encoding = PTI_R8;
			for (mip = 0; mip < mips->mipcount; mip++)
			{
				unsigned int i;
				unsigned char *out;
				unsigned char *in;
				void *needfree = NULL;
				
				in = mips->mip[mip].data;
				if (mips->mip[mip].needfree)
					out = in;
				else
				{
					needfree = in;
					out = BZ_Malloc(mips->mip[mip].width*mips->mip[mip].height*sizeof(*out));
					mips->mip[mip].data = out;
				}
				mips->mip[mip].datasize = mips->mip[mip].width*mips->mip[mip].height;
				mips->mip[mip].needfree = true;

				for (i = 0; i < mips->mip[mip].width*mips->mip[mip].height; i++, in+=4)
					out[i] = GetPaletteIndex(in[0], in[1], in[2]);

				if (needfree)
					BZ_Free(needfree);
			}
		}
	}

	//if that format isn't supported/desired, try converting it.
	if (sh_config.texfmt[mips->encoding])
		return;

	if (mips->encoding == PTI_R8)
	{
		if (sh_config.texfmt[PTI_BGRX8])	//bgra8 is typically faster when supported.
			mips->encoding = PTI_BGRX8;
		else if (sh_config.texfmt[PTI_BGRX8])
			mips->encoding = PTI_RGBX8;
		else if (sh_config.texfmt[PTI_BGRA8])
			mips->encoding = PTI_BGRA8;
		else
			mips->encoding = PTI_RGBA8;
		for (mip = 0; mip < mips->mipcount; mip++)
		{
			unsigned int i;
			unsigned int *out;
			unsigned char *in;
			in = mips->mip[mip].data;
			out = BZ_Malloc(mips->mip[mip].width*mips->mip[mip].height*sizeof(*out));
			for (i = 0; i < mips->mip[mip].width*mips->mip[mip].height; i++)
				out[i] = in[i] * 0x01010101;
			if (mips->mip[mip].needfree)
				BZ_Free(in);
			mips->mip[mip].data = out;
			mips->mip[mip].needfree = true;
			mips->mip[mip].datasize = mips->mip[mip].width*mips->mip[mip].height*sizeof(*out);
		}

		if (sh_config.texfmt[mips->encoding])
			return;
	}

	if ((mips->encoding == PTI_RGBX8 && sh_config.texfmt[PTI_BGRX8]) ||
		(mips->encoding == PTI_BGRX8 && sh_config.texfmt[PTI_RGBX8]) ||
		(mips->encoding == PTI_RGBA8 && sh_config.texfmt[PTI_BGRA8]) ||
		(mips->encoding == PTI_BGRA8 && sh_config.texfmt[PTI_RGBA8]))
	{
		for (mip = 0; mip < mips->mipcount; mip++)
			Image_8_BGR_RGB_Swap(mips->mip[mip].data, mips->mip[mip].width, mips->mip[mip].height);
		if (mips->encoding == PTI_RGBA8)
			mips->encoding = PTI_BGRA8;
		else if (mips->encoding == PTI_BGRA8)
			mips->encoding = PTI_RGBA8;
		else if (mips->encoding == PTI_RGBX8)
			mips->encoding = PTI_BGRX8;
		else// if (mips->encoding == PTI_BGRX8)
			mips->encoding = PTI_RGBX8;
	}
	//should we just use 5551 always?
	else if (mips->encoding == PTI_RGBX8 || mips->encoding == PTI_BGRX8)
	{
		/*if (0)
		{	//prevent discolouration.
			if (sh_config.texfmt[PTI_RGBA5551])
			{
				for (mip = 0; mip < mips->mipcount; mip++)
					Image_8888to5551(mips->mip[mip].data, mips->mip[mip].data, mips->mip[mip].width, mips->mip[mip].height, mips->encoding == PTI_BGRX8);
				mips->encoding = PTI_RGBA5551;
			}
			else
			{
				for (mip = 0; mip < mips->mipcount; mip++)
					Image_8888to1555(mips->mip[mip].data, mips->mip[mip].data, mips->mip[mip].width, mips->mip[mip].height, mips->encoding == PTI_BGRX8);
				mips->encoding = PTI_ARGB1555;
			}
		}
		else*/
		if (sh_config.texfmt[PTI_RGB565])
		{
			for (mip = 0; mip < mips->mipcount; mip++)
				Image_8888to565(mips->mip[mip].data, mips->mip[mip].data, mips->mip[mip].width, mips->mip[mip].height, mips->encoding == PTI_BGRX8);
			mips->encoding = PTI_RGB565;
		}
	}
	else if (mips->encoding == PTI_RGBA8 || mips->encoding == PTI_BGRA8)
	{
		if (origfmt == TF_TRANS8 || origfmt == TF_TRANS8_FULLBRIGHT || origfmt == TF_H2_TRANS8_0 || !(sh_config.texfmt[PTI_RGBA4444] || sh_config.texfmt[PTI_ARGB4444]))
		{	//1-bit alpha is okay for these textures.
			if (sh_config.texfmt[PTI_RGBA5551])
			{
				for (mip = 0; mip < mips->mipcount; mip++)
					Image_8888to5551(mips->mip[mip].data, mips->mip[mip].data, mips->mip[mip].width, mips->mip[mip].height, mips->encoding == PTI_BGRA8);
				mips->encoding = PTI_RGBA5551;
			}
			else
			{
				for (mip = 0; mip < mips->mipcount; mip++)
					Image_8888to1555(mips->mip[mip].data, mips->mip[mip].data, mips->mip[mip].width, mips->mip[mip].height, mips->encoding == PTI_BGRA8);
				mips->encoding = PTI_ARGB1555;
			}
		}
		else
		{
			if (sh_config.texfmt[PTI_RGBA4444])
			{
				for (mip = 0; mip < mips->mipcount; mip++)
					Image_8888to4444(mips->mip[mip].data, mips->mip[mip].data, mips->mip[mip].width, mips->mip[mip].height, mips->encoding == PTI_BGRA8);
				mips->encoding = PTI_RGBA4444;
			}
			else
			{
				for (mip = 0; mip < mips->mipcount; mip++)
					Image_8888toARGB4444(mips->mip[mip].data, mips->mip[mip].data, mips->mip[mip].width, mips->mip[mip].height, mips->encoding == PTI_BGRA8);
				mips->encoding = PTI_ARGB4444;
			}
		}
	}
}

//resamples and depalettes as required
static qboolean Image_GenMip0(struct pendingtextureinfo *mips, unsigned int flags, void *rawdata, void *palettedata, int imgwidth, int imgheight, uploadfmt_t fmt, qboolean freedata)
{
	unsigned int *rgbadata = rawdata;
	int i;
	qboolean valid;

	mips->mip[0].width = imgwidth;
	mips->mip[0].height = imgheight;
	mips->mipcount = 1;

	switch(fmt)
	{
	case TF_DEPTH16:
		mips->encoding = PTI_DEPTH16;
		break;
	case TF_DEPTH24:
		mips->encoding = PTI_DEPTH24;
		break;
	case TF_DEPTH32:
		mips->encoding = PTI_DEPTH32;
		break;
	case TF_RGBA16F:
	case TF_RGBA32F:
		if (rawdata)
		{
			Con_Printf("R_LoadRawTexture: bad format\n");
			if (freedata)
				BZ_Free(rawdata);
			return false;
		}
		mips->encoding = (fmt==TF_RGBA16F)?PTI_RGBA16F:TF_RGBA32F;
		break;

	default:
	case TF_INVALID:
		Con_Printf("R_LoadRawTexture: bad format\n");
		if (freedata)
			BZ_Free(rawdata);
		return false;
	case TF_RGBX32:
		mips->encoding = PTI_RGBX8;
		break;
	case TF_RGBA32:
		mips->encoding = PTI_RGBA8;
		break;
	case TF_BGRX32:
		mips->encoding = PTI_BGRX8;
		break;
	case TF_BGRA32:
		mips->encoding = PTI_BGRA8;
		break;
	case TF_MIP4_LUM8:
		//8bit indexed data.
		Image_RoundDimensions(&mips->mip[0].width, &mips->mip[0].height, flags);
		flags |= IF_NOPICMIP;
		if (/*!r_dodgymiptex.ival &&*/ mips->mip[0].width == imgwidth && mips->mip[0].height == imgheight)
		{
			unsigned int pixels =
				(imgwidth>>0) * (imgheight>>0) + 
				(imgwidth>>1) * (imgheight>>1) +
				(imgwidth>>2) * (imgheight>>2) +
				(imgwidth>>3) * (imgheight>>3);

			mips->encoding = PTI_R8;
			rgbadata = BZ_Malloc(pixels);
			memcpy(rgbadata, rawdata, pixels);

			for (i = 0; i < 4; i++)
			{
				mips->mip[i].width = imgwidth>>i;
				mips->mip[i].height = imgheight>>i;
				mips->mip[i].datasize = mips->mip[i].width * mips->mip[i].height;
				mips->mip[i].needfree = false;
			}
			mips->mipcount = i;
			mips->mip[0].data = rgbadata;
			mips->mip[1].data = (qbyte*)mips->mip[0].data + mips->mip[0].datasize;
			mips->mip[2].data = (qbyte*)mips->mip[1].data + mips->mip[1].datasize;
			mips->mip[3].data = (qbyte*)mips->mip[2].data + mips->mip[2].datasize;

			mips->extrafree = rgbadata;
			if (freedata)
				BZ_Free(rawdata);
			return true;
		}
		//fall through
	case TF_LUM8:
		mips->encoding = PTI_R8;
		break;
	case TF_MIP4_SOLID8:
		//8bit opaque data
		Image_RoundDimensions(&mips->mip[0].width, &mips->mip[0].height, flags);
		flags |= IF_NOPICMIP;
		if (!r_dodgymiptex.ival && mips->mip[0].width == imgwidth && mips->mip[0].height == imgheight)
		{	//special hack required to preserve the hand-drawn lower mips.
			unsigned int pixels =
				(imgwidth>>0) * (imgheight>>0) + 
				(imgwidth>>1) * (imgheight>>1) +
				(imgwidth>>2) * (imgheight>>2) +
				(imgwidth>>3) * (imgheight>>3);

			mips->encoding = PTI_RGBX8;
			rgbadata = BZ_Malloc(pixels*4);
			for (i = 0; i < pixels; i++)
				rgbadata[i] = d_8to24rgbtable[((qbyte*)rawdata)[i]];

			for (i = 0; i < 4; i++)
			{
				mips->mip[i].width = imgwidth>>i;
				mips->mip[i].height = imgheight>>i;
				mips->mip[i].datasize = mips->mip[i].width * mips->mip[i].height * 4;
				mips->mip[i].needfree = false;
			}
			mips->mipcount = i;
			mips->mip[0].data = rgbadata;
			mips->mip[1].data = (qbyte*)mips->mip[0].data + mips->mip[0].datasize;
			mips->mip[2].data = (qbyte*)mips->mip[1].data + mips->mip[1].datasize;
			mips->mip[3].data = (qbyte*)mips->mip[2].data + mips->mip[2].datasize;

			mips->extrafree = rgbadata;
			if (freedata)
				BZ_Free(rawdata);
			return true;
		}
		//fall through
	case TF_SOLID8:
		rgbadata = BZ_Malloc(imgwidth * imgheight*4);
		if (sh_config.texfmt[PTI_BGRX8])
		{	//bgra8 is typically faster when supported.
			mips->encoding = PTI_BGRX8;
			for (i = 0; i < imgwidth * imgheight; i++)
				rgbadata[i] = d_8to24bgrtable[((qbyte*)rawdata)[i]];
		}
		else
		{
			mips->encoding = PTI_RGBX8;
			for (i = 0; i < imgwidth * imgheight; i++)
				rgbadata[i] = d_8to24rgbtable[((qbyte*)rawdata)[i]];
		}
		if (freedata)
			BZ_Free(rawdata);
		freedata = true;
		break;
	case TF_TRANS8:
		{
			mips->encoding = PTI_RGBX8;
			rgbadata = BZ_Malloc(imgwidth * imgheight*4);
			for (i = 0; i < imgwidth * imgheight; i++)
			{
				if (((qbyte*)rawdata)[i] == 0xff)
				{//fixme: blend non-0xff neighbours. no, just use premultiplied alpha instead, where it matters.
					rgbadata[i] = 0;
					mips->encoding = PTI_RGBA8;
				}
				else
					rgbadata[i] = d_8to24rgbtable[((qbyte*)rawdata)[i]];
			}
			if (freedata)
				BZ_Free(rawdata);
			freedata = true;
		}
		break;
	case TF_H2_TRANS8_0:
		{
			mips->encoding = PTI_RGBX8;
			rgbadata = BZ_Malloc(imgwidth * imgheight*4);
			for (i = 0; i < imgwidth * imgheight; i++)
			{
				if (((qbyte*)rawdata)[i] == 0xff || ((qbyte*)rawdata)[i] == 0)
				{//fixme: blend non-0xff neighbours. no, just use premultiplied alpha instead, where it matters.
					rgbadata[i] = 0;
					mips->encoding = PTI_RGBA8;
				}
				else
					rgbadata[i] = d_8to24rgbtable[((qbyte*)rawdata)[i]];
			}
			if (freedata)
				BZ_Free(rawdata);
			freedata = true;
		}
		break;
	case TF_TRANS8_FULLBRIGHT:
		mips->encoding = PTI_RGBA8;
		rgbadata = BZ_Malloc(imgwidth * imgheight*4);
		for (i = 0, valid = false; i < imgwidth * imgheight; i++)
		{
			if (((qbyte*)rawdata)[i] == 255 || ((qbyte*)rawdata)[i] < 256-vid.fullbright)
				rgbadata[i] = 0;
			else
			{
				rgbadata[i] = d_8to24rgbtable[((qbyte*)rawdata)[i]];
				valid = true;
			}
		}
		if (freedata)
			BZ_Free(rawdata);
		freedata = true;
		if (!valid)
		{
			BZ_Free(rgbadata);
			return false;
		}
		break;

	case TF_HEIGHT8PAL:
		mips->encoding = PTI_RGBA8;
		rgbadata = BZ_Malloc(imgwidth * imgheight*5);
		{
			qbyte *heights = (qbyte*)(rgbadata + (imgwidth*imgheight));
			for (i = 0; i < imgwidth * imgheight; i++)
			{
				unsigned int rgb = d_8to24rgbtable[((qbyte*)rawdata)[i]];
				heights[i] = (((rgb>>16)&0xff) + ((rgb>>8)&0xff) + ((rgb>>0)&0xff))/3;
			}
			Image_GenerateNormalMap(heights, rgbadata, imgwidth, imgheight, r_shadow_bumpscale_basetexture.value?r_shadow_bumpscale_basetexture.value:4, r_shadow_heightscale_basetexture.value);
		}
		if (freedata)
			BZ_Free(rawdata);
		freedata = true;
		break;
	case TF_HEIGHT8:
		mips->encoding = PTI_RGBA8;
		rgbadata = BZ_Malloc(imgwidth * imgheight*4);
		Image_GenerateNormalMap(rawdata, rgbadata, imgwidth, imgheight, r_shadow_bumpscale_bumpmap.value, r_shadow_heightscale_bumpmap.value);
		if (freedata)
			BZ_Free(rawdata);
		freedata = true;
		break;

	case TF_BGR24_FLIP:
		mips->encoding = PTI_RGBX8;
		rgbadata = BZ_Malloc(imgwidth * imgheight*4);
		for (i = 0; i < imgheight; i++)
		{
			int x;
			qbyte *in = (qbyte*)rawdata + (imgheight-i-1) * imgwidth * 3;
			qbyte *out = (qbyte*)rgbadata + i * imgwidth * 4;
			for (x = 0; x < imgwidth; x++, in+=3, out+=4)
			{
				out[0] = in[2];
				out[1] = in[1];
				out[2] = in[0];
				out[3] = 0xff;
			}
		}
		if (freedata)
			BZ_Free(rawdata);
		freedata = true;
		break;

	case TF_MIP4_8PAL24_T255:
	case TF_MIP4_8PAL24:
		//8bit opaque data
		{
			unsigned int pixels =
					(imgwidth>>0) * (imgheight>>0) + 
					(imgwidth>>1) * (imgheight>>1) +
					(imgwidth>>2) * (imgheight>>2) +
					(imgwidth>>3) * (imgheight>>3);
			palettedata = (qbyte*)rawdata + pixels;
			Image_RoundDimensions(&mips->mip[0].width, &mips->mip[0].height, flags);
			flags |= IF_NOPICMIP;
			if (!r_dodgymiptex.ival && mips->mip[0].width == imgwidth && mips->mip[0].height == imgheight)
			{
				unsigned int pixels =
					(imgwidth>>0) * (imgheight>>0) + 
					(imgwidth>>1) * (imgheight>>1) +
					(imgwidth>>2) * (imgheight>>2) +
					(imgwidth>>3) * (imgheight>>3);

				rgbadata = BZ_Malloc(pixels*4);
				if (fmt == TF_MIP4_8PAL24_T255)
				{
					mips->encoding = PTI_RGBA8;
					for (i = 0; i < pixels; i++)
					{
						qbyte idx = ((qbyte*)rawdata)[i];
						if (idx == 255)
							rgbadata[i] = 0;
						else
						{
							qbyte *p = ((qbyte*)palettedata) + idx*3;
							rgbadata[i] = 0xff000000 | (p[0]<<0) | (p[1]<<8) | (p[2]<<16);	//FIXME: endian
						}
					}
				}
				else
				{
					mips->encoding = PTI_RGBX8;
					for (i = 0; i < pixels; i++)
					{
						qbyte *p = ((qbyte*)palettedata) + ((qbyte*)rawdata)[i]*3;
						//FIXME: endian
						rgbadata[i] = 0xff000000 | (p[0]<<0) | (p[1]<<8) | (p[2]<<16);
					}
				}

				for (i = 0; i < 4; i++)
				{
					mips->mip[i].width = imgwidth>>i;
					mips->mip[i].height = imgheight>>i;
					mips->mip[i].datasize = mips->mip[i].width * mips->mip[i].height * 4;
					mips->mip[i].needfree = false;
				}
				mips->mipcount = i;
				mips->mip[0].data = rgbadata;
				mips->mip[1].data = (qbyte*)mips->mip[0].data + mips->mip[0].datasize;
				mips->mip[2].data = (qbyte*)mips->mip[1].data + mips->mip[1].datasize;
				mips->mip[3].data = (qbyte*)mips->mip[2].data + mips->mip[2].datasize;

				mips->extrafree = rgbadata;
				if (freedata)
					BZ_Free(rawdata);
				return true;
			}
		}
		//fall through
	case TF_8PAL24:
		if (!palettedata)
		{
			Con_Printf("TF_8PAL24: no palette");
			if (freedata)
				BZ_Free(rawdata);
			return false;
		}
		rgbadata = BZ_Malloc(imgwidth * imgheight*4);
		if (fmt == TF_MIP4_8PAL24_T255)
		{
			mips->encoding = PTI_RGBA8;
			for (i = 0; i < imgwidth * imgheight; i++)
			{
				qbyte idx = ((qbyte*)rawdata)[i];
				if (idx == 255)
					rgbadata[i] = 0;
				else
				{
					qbyte *p = ((qbyte*)palettedata) + idx*3;
					rgbadata[i] = 0xff000000 | (p[0]<<0) | (p[1]<<8) | (p[2]<<16);	//FIXME: endian
				}
			}
		}
		else
		{
			mips->encoding = PTI_RGBX8;
			for (i = 0; i < imgwidth * imgheight; i++)
			{
				qbyte *p = ((qbyte*)palettedata) + ((qbyte*)rawdata)[i]*3;
				//FIXME: endian
				rgbadata[i] = 0xff000000 | (p[0]<<0) | (p[1]<<8) | (p[2]<<16);
			}
		}
		if (freedata)
			BZ_Free(rawdata);
		freedata = true;
		break;
	case TF_8PAL32:
		if (!palettedata)
		{
			Con_Printf("TF_8PAL32: no palette");
			if (freedata)
				BZ_Free(rawdata);
			return false;
		}
		mips->encoding = PTI_RGBA8;
		rgbadata = BZ_Malloc(imgwidth * imgheight*4);
		for (i = 0; i < imgwidth * imgheight; i++)
			rgbadata[i] = ((unsigned int*)palettedata)[((qbyte*)rawdata)[i]];
		if (freedata)
			BZ_Free(rawdata);
		freedata = true;
		break;

#ifdef HEXEN2
	case TF_H2_T7G1: /*8bit data, odd indexes give greyscale transparence*/
		mips->encoding = PTI_RGBA8;
		rgbadata = BZ_Malloc(imgwidth * imgheight*4);
		for (i = 0; i < imgwidth * imgheight; i++)
		{
			qbyte p = ((qbyte*)rawdata)[i];
			rgbadata[i] = d_8to24rgbtable[p] & 0x00ffffff;
			if (p == 0)
				;
			else if (p&1)
				rgbadata[i] |= 0x80000000;
			else
				rgbadata[i] |= 0xff000000;
		}
		if (freedata)
			BZ_Free(rawdata);
		freedata = true;
		break;
	case TF_H2_T4A4:     /*8bit data, weird packing*/
		mips->encoding = PTI_RGBA8;
		rgbadata = BZ_Malloc(imgwidth * imgheight*4);
		for (i = 0; i < imgwidth * imgheight; i++)
		{
			static const int ColorIndex[16] = {0, 31, 47, 63, 79, 95, 111, 127, 143, 159, 175, 191, 199, 207, 223, 231};
			static const unsigned ColorPercent[16] = {25, 51, 76, 102, 114, 127, 140, 153, 165, 178, 191, 204, 216, 229, 237, 247};
			qbyte p = ((qbyte*)rawdata)[i];
			rgbadata[i] = d_8to24rgbtable[ColorIndex[p>>4]] & 0x00ffffff;
			rgbadata[i] |= ( int )ColorPercent[p&15] << 24;
		}
		if (freedata)
			BZ_Free(rawdata);
		freedata = true;
		break;
#endif
	}

	if (flags & IF_NOALPHA)
	{
		switch(mips->encoding)
		{
		case PTI_RGBA8:
			mips->encoding = PTI_RGBX8;
			break;
		case PTI_BGRA8:
			mips->encoding = PTI_BGRX8;
			break;
		case PTI_RGBA16F:
		case PTI_RGBA32F:
		case PTI_ARGB4444:
		case PTI_ARGB1555:
		case PTI_RGBA4444:
		case PTI_RGBA5551:
			break;	//erk
		case PTI_S3RGBA1:	//mostly compatible, but I don't want to push it.
		case PTI_S3RGBA3:
		case PTI_S3RGBA5:
		case PTI_WHOLEFILE:
			//erk. meh.
			break;
		case PTI_R8:
		case PTI_RG8:
		case PTI_RGB565:
		case PTI_RGBX8:
		case PTI_BGRX8:
		case PTI_S3RGB1:
			break;	//already no alpha in these formats
		case PTI_DEPTH16:
		case PTI_DEPTH24:
		case PTI_DEPTH32:
		case PTI_DEPTH24_8:
			break;
		case PTI_MAX: break;	//stfu
		}
		//FIXME: fill alpha channel with 255?
	}


	Image_RoundDimensions(&mips->mip[0].width, &mips->mip[0].height, flags);
	if (rgbadata)
	{
		if (mips->mip[0].width == imgwidth && mips->mip[0].height == imgheight)
			mips->mip[0].data = rgbadata;
		else
		{
			mips->mip[0].data = BZ_Malloc(((mips->mip[0].width+3)&~3)*mips->mip[0].height*4);
	//		memset(mips->mip[0].data, 0, mips->mip[0].width*mips->mip[0].height*4);
			Image_ResampleTexture(rgbadata, imgwidth, imgheight, mips->mip[0].data, mips->mip[0].width, mips->mip[0].height);
			if (freedata)
				BZ_Free(rgbadata);
			freedata = true;
		}
	}
	else
		mips->mip[0].data = NULL;
	mips->mip[0].datasize = mips->mip[0].width*mips->mip[0].height*4;

	if (mips->type == PTI_3D)
	{
		qbyte *data2d = mips->mip[0].data, *data3d;
		mips->mip[0].data = NULL;
		/*our 2d input image is interlaced as y0z0,y0z1,y1z0,y1z1
		  however, hardware uses the more logical y0z0,y1z0,y0z1,y1z1 ordering (xis ordered properly already)*/
		if (mips->mip[0].height*mips->mip[0].height == mips->mip[0].width && (mips->encoding == PTI_RGBA8 || mips->encoding == PTI_RGBX8 || mips->encoding == PTI_BGRA8 || mips->encoding == PTI_BGRX8))
		{
			int d, r;
			int size = mips->mip[0].height;
			mips->mip[0].data = data3d = BZ_Malloc(size*size*size);
			for (d = 0; d < size; d++)
				for (r = 0; r < size; r++)
					memcpy(data3d + (r + d*size) * size, data2d + (r*size + d) * size, size*4);
			mips->mip[0].datasize = size*size*size*4;
		}
		if (freedata)
			BZ_Free(data2d);
		if (!mips->mip[0].data)
			return false;
	}

	if (flags & IF_PREMULTIPLYALPHA)
	{
		//works for rgba or bgra
		int i;
		unsigned char *fte_restrict premul = (unsigned char*)mips->mip[0].data;
		for (i = 0; i < mips->mip[0].width*mips->mip[0].height; i++, premul+=4)
		{
			premul[0] = (premul[0] * premul[3])>>8;
			premul[1] = (premul[1] * premul[3])>>8;
			premul[2] = (premul[2] * premul[3])>>8;
		}
	}

	mips->mip[0].needfree = freedata;
	return true;
}
//loads from a single mip. takes ownership of the data.
static qboolean Image_LoadRawTexture(texid_t tex, unsigned int flags, void *rawdata, void *palettedata, int imgwidth, int imgheight, uploadfmt_t fmt)
{
	struct pendingtextureinfo *mips;
	mips = Z_Malloc(sizeof(*mips));
	mips->type = (flags & IF_3DMAP)?PTI_3D:PTI_2D;

	if (!Image_GenMip0(mips, flags, rawdata, palettedata, imgwidth, imgheight, fmt, true))
	{
		Z_Free(mips);
		if (flags & IF_NOWORKER)
			Image_LoadTexture_Failed(tex, NULL, 0, 0);
		else
			COM_AddWork(WG_MAIN, Image_LoadTexture_Failed, tex, NULL, 0, 0);
		return false;
	}
	Image_GenerateMips(mips, flags);
	Image_ChangeFormat(mips, flags, fmt);

	tex->width = imgwidth;
	tex->height = imgheight;

	if (flags & IF_NOWORKER)
		Image_LoadTextureMips(tex, mips, 0, 0);
	else
		COM_AddWork(WG_MAIN, Image_LoadTextureMips, tex, mips, 0, 0);
	return true;
}

//always frees filedata, even on failure.
//also frees the textures fallback data, but only on success
qboolean Image_LoadTextureFromMemory(texid_t tex, int flags, const char *iname, char *fname, qbyte *filedata, int filesize)
{
	qboolean hasalpha;
	qbyte *rgbadata;

	int imgwidth, imgheight;

	//these formats have special handling, because they cannot be implemented via Read32BitImageFile - they don't result in rgba images.
#ifdef IMAGEFMT_DDS
	if (Image_ReadDDSFile(tex, flags, fname, filedata, filesize))
		return true;
#endif
#ifdef IMAGEFMT_BLP
	if (filedata[0] == 'B' && filedata[1] == 'L' && filedata[2] == 'P' && filedata[3] == '2') 
	{
		if (Image_ReadBLPFile(tex, flags, fname, filedata, filesize))
			return true;
	}
#endif

	hasalpha = false;
	if ((rgbadata = Read32BitImageFile(filedata, filesize, &imgwidth, &imgheight, &hasalpha, fname)))
	{
		extern cvar_t vid_hardwaregamma;
		if (!(flags&IF_NOGAMMA) && !vid_hardwaregamma.value)
			BoostGamma(rgbadata, imgwidth, imgheight);

		if (hasalpha) 
			flags &= ~IF_NOALPHA;
		else if (!(flags & IF_NOALPHA))
		{
			unsigned int alpha_width, alpha_height, p;
			char aname[MAX_QPATH];
			unsigned char *alphadata;
			char *alph;
			size_t alphsize;
			char ext[8];
			COM_StripExtension(fname, aname, sizeof(aname));
			COM_FileExtension(fname, ext, sizeof(ext));
			Q_strncatz(aname, "_alpha.", sizeof(aname));
			Q_strncatz(aname, ext, sizeof(aname));
			if (!strchr(aname, ':') && (alph = COM_LoadFile (aname, 5, &alphsize)))
			{
				if ((alphadata = Read32BitImageFile(alph, alphsize, &alpha_width, &alpha_height, &hasalpha, aname)))
				{
					if (alpha_width == imgwidth && alpha_height == imgheight)
					{
						for (p = 0; p < alpha_width*alpha_height; p++)
						{
							rgbadata[(p<<2) + 3] = (alphadata[(p<<2) + 0] + alphadata[(p<<2) + 1] + alphadata[(p<<2) + 2])/3;
						}
					}
					BZ_Free(alphadata);
				}
				BZ_Free(alph);
			}
		}

		if (Image_LoadRawTexture(tex, flags, rgbadata, NULL, imgwidth, imgheight, TF_RGBA32))
		{
			BZ_Free(filedata);

			//and kill the fallback now that its loaded, as it won't be needed any more.
			BZ_Free(tex->fallbackdata);
			tex->fallbackdata = NULL;
			return true;
		}
		BZ_Free(rgbadata);
	}
#ifdef FTE_TARGET_WEB
	else if (1)
	{
		struct pendingtextureinfo *mips;
		mips = Z_Malloc(sizeof(*mips));
		mips->type = (flags & IF_3DMAP)?PTI_3D:PTI_2D;
		mips->mipcount = 1;
		mips->encoding = PTI_WHOLEFILE;
		mips->extrafree = NULL;
		mips->mip[0].width = 1;
		mips->mip[0].height = 1;
		mips->mip[0].data = filedata;
		mips->mip[0].datasize = filesize;
		mips->mip[0].needfree = true;
		//width+height are not yet known. bah.
		if (flags & IF_NOWORKER)
			Image_LoadTextureMips(tex, mips, 0, 0);
		else
			COM_AddWork(WG_MAIN, Image_LoadTextureMips, tex, mips, 0, 0);
		return true;
	}
#endif
	else
		Con_Printf("Unable to read file %s (format unsupported)\n", fname);

	BZ_Free(filedata);
	return false;
}

//loads from a single mip. takes ownership of the data.
static qboolean Image_LoadCubemapTexture(texid_t tex, char *nicename)
{
	static struct
	{
		char *suffix;
		qboolean flipx, flipy, flipd;
	} cmscheme[] =
	{
		{"rt", true,  false, true},
		{"lf", false, true,  true},
		{"ft", true,  true,  false},
		{"bk", false, false, false},
		{"up", true,  false, true},
		{"dn", true,  false, true},

		{"px", false, false, false},
		{"nx", false, false, false},
		{"py", false, false, false},
		{"ny", false, false, false},
		{"pz", false, false, false},
		{"nz", false, false, false},

		{"posx", false, false, false},
		{"negx", false, false, false},
		{"posy", false, false, false},
		{"negy", false, false, false},
		{"posz", false, false, false},
		{"negz", false, false, false}
	};
	int i, j, e;
	struct pendingtextureinfo *mips;
	char fname[MAX_QPATH];
	size_t filesize;
	int width, height;
	qboolean hasalpha;
	mips = Z_Malloc(sizeof(*mips));
	mips->type = PTI_CUBEMAP;
	mips->mipcount = 6;
	mips->encoding = PTI_RGBA8;
	mips->extrafree = NULL;

	for (i = 0; i < 6; i++)
	{
		for (e = (tex->flags & IF_EXACTEXTENSION)?tex_extensions_count-1:0; e < tex_extensions_count; e++)
		{
			//try and open one
			qbyte *buf = NULL, *data;
			filesize = 0;
			for (j = 0; j < sizeof(cmscheme)/sizeof(cmscheme[0])/6; j++)
			{
				Q_snprintfz(fname, sizeof(fname), "%s%s%s", nicename, cmscheme[i + 6*j].suffix, tex_extensions[e].name);
				buf = COM_LoadFile(fname, 5, &filesize);
				if (buf)
					break;
			}

			//now read it
			if (buf)
			{
				if ((data = Read32BitImageFile(buf, filesize, &width, &height, &hasalpha, fname)))
				{
					extern cvar_t vid_hardwaregamma;
					if (!(tex->flags&IF_NOGAMMA) && !vid_hardwaregamma.value)
						BoostGamma(data, width, height);
					mips->mip[i].data = R_FlipImage32(data, &width, &height, cmscheme[i + 6*j].flipx, cmscheme[i + 6*j].flipy, cmscheme[i + 6*j].flipd);
					mips->mip[i].datasize = width*height*4;
					mips->mip[i].width = width;
					mips->mip[i].height = height;
					mips->mip[i].needfree = true;

					if (i == 0)
					{
						tex->width = width;
						tex->height = height;
					}
					BZ_Free(buf);
					break;
				}
				BZ_Free(buf);
			}
		}
	}

	if (tex->flags & IF_NOWORKER)
		Image_LoadTextureMips(tex, mips, 0, 0);
	else
		COM_AddWork(WG_MAIN, Image_LoadTextureMips, tex, mips, 0, 0);
	return true;
}

static qboolean Image_LocateHighResTexture(image_t *tex, flocation_t *bestloc, char *bestname, size_t bestnamesize, unsigned int *bestflags)
{
	char fname[MAX_QPATH], nicename[MAX_QPATH];
	int i, e;
	char *altname;
	char *nextalt;
	qboolean exactext = !!(tex->flags & IF_EXACTEXTENSION);

	int locflags = FSLF_DEPTH_INEXPLICIT|FSLF_DEEPONFAILURE;
	int bestdepth = 0x7fffffff, depth;
	int firstex = (tex->flags & IF_EXACTEXTENSION)?tex_extensions_count-1:0;

	flocation_t loc;

	for(altname = tex->ident;altname;altname = nextalt)
	{
		nextalt = strchr(altname, ':');
		if (nextalt)
		{
			nextalt++;
			if (nextalt-altname >= sizeof(fname))
				continue;	//too long...
			memcpy(fname, altname, nextalt-altname-1);
			fname[nextalt-altname-1] = 0;
			altname = fname;
		}

		//see if we recognise the extension, and only strip it if we do.
		if (exactext)
			e = tex_extensions_count;
		else
		{
			COM_FileExtension(altname, nicename, sizeof(nicename));
			e = 0;
			if (strcmp(nicename, "lmp") && strcmp(nicename, "wal"))
				for (; e < tex_extensions_count; e++)
				{
					if (!strcmp(nicename, (*tex_extensions[e].name=='.')?tex_extensions[e].name+1:tex_extensions[e].name))
						break;
				}
		}

		//strip it and try replacements if we do, otherwise assume that we're meant to be loading progs/foo.mdl_0.tga or whatever
		if (e == tex_extensions_count || exactext)
		{
			exactext = true;
			Q_strncpyz(nicename, altname, sizeof(nicename));
		}
		else
			COM_StripExtension(altname, nicename, sizeof(nicename));

		if (!tex->fallbackdata || (gl_load24bit.ival && !(tex->flags & IF_NOREPLACE)))
		{
			Q_snprintfz(fname, sizeof(fname), "dds/%s.dds", nicename);
			depth = FS_FLocateFile(fname, locflags, &loc);
			if (depth < bestdepth)
			{
				Q_strncpyz(bestname, fname, bestnamesize);
				bestdepth = depth;
				*bestloc = loc;
				bestflags = 0;
			}

			if (strchr(nicename, '/') || strchr(nicename, '\\'))	//never look in a root dir for the pic
				i = 0;
			else
				i = 1;

			for (; i < sizeof(tex_path)/sizeof(tex_path[0]); i++)
			{
				if (!tex_path[i].enabled)
					continue;
				if (tex_path[i].args >= 3)
				{	//this is a path that needs subpaths
					char subpath[MAX_QPATH];
					char basename[MAX_QPATH];
					char *s, *n;
					if (!tex->subpath || !*nicename)
						continue;

					s = COM_SkipPath(nicename);
					n = basename;
					while (*s && (*s != '.'||exactext) && n < basename+sizeof(basename)-5)
						*n++ = *s++;
					s = strchr(s, '_');
					if (s)
					{
						while (*s && n < basename+sizeof(basename)-5)
							*n++ = *s++;
					}
					*n = 0;

					for(s = tex->subpath; s; s = n)
					{
						//subpath a:b:c tries multiple possible sub paths, for compatibility
						n = strchr(s, ':');
						if (n)
						{
							if (n-s >= sizeof(subpath))
								*subpath = 0;
							else
							{
								memcpy(subpath, s, n-s);
								subpath[n-s] = 0;
							}
							n++;
						}
						else
							Q_strncpyz(subpath, s, sizeof(subpath));
						for (e = firstex; e < tex_extensions_count; e++)
						{
							if (tex->flags & IF_NOPCX)
								if (!strcmp(tex_extensions[e].name, ".pcx"))
									continue;
							Q_snprintfz(fname, sizeof(fname), tex_path[i].path, subpath, basename, tex_extensions[e].name);
							depth = FS_FLocateFile(fname, locflags, &loc);
							if (depth < bestdepth)
							{
								Q_strncpyz(bestname, fname, bestnamesize);
								bestdepth = depth;
								*bestloc = loc;
								bestflags = 0;
							}
						}
					}
				}
				else
				{
					for (e = firstex; e < tex_extensions_count; e++)
					{
						if (tex->flags & IF_NOPCX)
							if (!strcmp(tex_extensions[e].name, ".pcx"))
								continue;
						Q_snprintfz(fname, sizeof(fname), tex_path[i].path, nicename, tex_extensions[e].name);
						depth = FS_FLocateFile(fname, locflags, &loc);
						if (depth < bestdepth)
						{
							Q_strncpyz(bestname, fname, bestnamesize);
							bestdepth = depth;
							*bestloc = loc;
							bestflags = 0;
						}
					}
				}

				//support expansion of _bump textures to _norm textures.
				if (tex->flags & IF_TRYBUMP)
				{
					if (tex_path[i].args >= 3)
					{
						/*no legacy compat needed, hopefully*/
					}
					else
					{
						char bumpname[MAX_QPATH], *b;
						const char *n;
						b = bumpname;
						n = nicename;
						while(*n)
						{
							if (*n == '_' && !strcmp(n, "_norm"))
							{
								strcpy(b, "_bump");
								b += 5;
								n += 5;
								break;
							}
							*b++ = *n++;
						}
						if (*n)	//no _norm, give up with that
							continue;
						*b = 0;
						for (e = firstex; e < tex_extensions_count; e++)
						{
							if (!strcmp(tex_extensions[e].name, ".tga"))
							{
								Q_snprintfz(fname, sizeof(fname), tex_path[i].path, bumpname, tex_extensions[e].name);

								Q_snprintfz(fname, sizeof(fname), tex_path[i].path, nicename, tex_extensions[e].name);
								depth = FS_FLocateFile(fname, locflags, &loc);
								if (depth < bestdepth)
								{
									Q_strncpyz(bestname, fname, bestnamesize);
									bestdepth = depth;
									*bestloc = loc;
									*bestflags = IF_TRYBUMP;
								}
							}
						}
					}
				}
			}


			/*still failed? attempt to load quake lmp files, which have no real format id (hence why they're not above)*/
			Q_strncpyz(fname, nicename, sizeof(fname));
			COM_DefaultExtension(fname, ".lmp", sizeof(fname));
			if (!(tex->flags & IF_NOPCX))
			{
				depth = FS_FLocateFile(fname, locflags, &loc);
				if (depth < bestdepth)
				{
					Q_strncpyz(bestname, fname, bestnamesize);
					bestdepth = depth;
					*bestloc = loc;
					bestflags = 0;
				}
			}
		}
	}

	return bestdepth != 0x7fffffff;
}

static void Image_LoadHiResTextureWorker(void *ctx, void *data, size_t a, size_t b)
{
	image_t *tex = ctx;
	char fname[MAX_QPATH];
	char *altname;
	char *nextalt;

	flocation_t loc;
	unsigned int locflags = 0;

	vfsfile_t *f;
	size_t fsize;
	char *buf;

	int imgwidth;
	int imgheight;
	qboolean alphaed;

//	Sys_Sleep(0.3);

	if ((tex->flags & IF_TEXTYPE) == IF_CUBEMAP)
	{	//cubemaps require special handling because they are (normally) 6 files instead of 1.
		//the exception is single-file dds cubemaps, but we don't support those.
		for(altname = tex->ident;altname;altname = nextalt)
		{
			nextalt = strchr(altname, ':');
			if (nextalt)
			{
				nextalt++;
				if (nextalt-altname >= sizeof(fname))
					continue;	//too long...
				memcpy(fname, altname, nextalt-altname-1);
				fname[nextalt-altname-1] = 0;
				altname = fname;
			}

			if (!Image_LoadCubemapTexture(tex, altname))
			{
				if (tex->flags & IF_NOWORKER)
					Image_LoadTexture_Failed(tex, NULL, 0, 0);
				else
					COM_AddWork(WG_MAIN, Image_LoadTexture_Failed, tex, NULL, 0, 0);
			}
		}
		return;
	}

	

	if (Image_LocateHighResTexture(tex, &loc, fname, sizeof(fname), &locflags))
	{
		f = FS_OpenReadLocation(&loc);
		if (f)
		{
			fsize = VFS_GETLEN(f);
			buf = BZ_Malloc(fsize);
			if (buf)
			{
				VFS_READ(f, buf, fsize);
				VFS_CLOSE(f);

				if (locflags & IF_TRYBUMP)
				{	//it was supposed to be a heightmap image (that we need to convert to normalmap)
					qbyte *d;
					int w, h;
					qboolean a;
					if ((d = ReadTargaFile(buf, fsize, &w, &h, &a, 2)))	//Only load a greyscale image.
					{
						BZ_Free(buf);
						if (Image_LoadRawTexture(tex, tex->flags, d, NULL, w, h, TF_HEIGHT8))
						{
							BZ_Free(tex->fallbackdata);
							tex->fallbackdata = NULL;	
							return;
						}
					}
					//guess not, fall back to normalmaps
				}

				if (Image_LoadTextureFromMemory(tex, tex->flags, tex->ident, fname, buf, fsize))
				{
					BZ_Free(tex->fallbackdata);
					tex->fallbackdata = NULL;	
					return;
				}
			}
			else
				VFS_CLOSE(f);
		}
	}

	//now look in wad files and swap over the fallback. (halflife compatability)
	COM_StripExtension(tex->ident, fname, sizeof(fname));
	buf = W_GetTexture(fname, &imgwidth, &imgheight, &alphaed);
	if (buf)
	{
		BZ_Free(tex->fallbackdata);
		tex->fallbackdata = buf;
		tex->fallbackfmt = TF_RGBA32;
		tex->fallbackwidth = imgwidth;
		tex->fallbackheight = imgheight;
	}

	if (tex->fallbackdata)
	{
		if (Image_LoadRawTexture(tex, tex->flags, tex->fallbackdata, (char*)tex->fallbackdata+(tex->fallbackwidth*tex->fallbackheight), tex->fallbackwidth, tex->fallbackheight, tex->fallbackfmt))
		{
			tex->fallbackdata = NULL;
			return;
		}
		tex->fallbackdata = NULL;	
	}

//	Sys_Printf("Texture %s failed\n", nicename);
	//signal the main thread to set the final status instead of just setting it to avoid deadlock (it might already be waiting for it).
	if (tex->flags & IF_NOWORKER)
		Image_LoadTexture_Failed(tex, NULL, 0, 0);
	else
		COM_AddWork(WG_MAIN, Image_LoadTexture_Failed, tex, NULL, 0, 0);
}




//find an existing texture
image_t *Image_FindTexture(const char *identifier, const char *subdir, unsigned int flags)
{
	image_t *tex;
	if (!subdir)
		subdir = "";
	tex = Hash_Get(&imagetable, identifier);
	while(tex)
	{
		if (!((tex->flags ^ flags) & (IF_CLAMP|IF_PALETTIZE)))
		{
#ifdef PURGEIMAGES
			if (!strcmp(subdir, tex->subpath?tex->subpath:""))
#endif
			{
				tex->regsequence = r_regsequence;
				return tex;
			}
		}
		tex = Hash_GetNext(&imagetable, identifier, tex);
	}
	return NULL;
}
//create a texture, with dupes. you'll need to load something into it too.
static image_t *Image_CreateTexture_Internal (const char *identifier, const char *subdir, unsigned int flags)
{
	image_t *tex;
	bucket_t *buck;

	tex = Z_Malloc(sizeof(*tex) + sizeof(bucket_t) + strlen(identifier)+1 + (subdir?strlen(subdir)+1:0));
	buck = (bucket_t*)(tex+1);
	tex->ident = (char*)(buck+1);
	strcpy(tex->ident, identifier);
#ifdef _DEBUG
	Q_strncpyz(tex->dbgident, identifier, sizeof(tex->dbgident));
#endif
	if (subdir && *subdir)
	{
		tex->subpath = tex->ident + strlen(identifier)+1;
		strcpy(tex->subpath, subdir);
	}

	tex->next = imagelist;
	imagelist = tex;

	tex->flags = flags;
	tex->width = 0;
	tex->height = 0;
	tex->regsequence = r_regsequence;
	tex->status = TEX_NOTLOADED;
	tex->fallbackdata = NULL;
	tex->fallbackwidth = 0;
	tex->fallbackheight = 0;
	tex->fallbackfmt = TF_INVALID;
	if (*tex->ident)
		Hash_Add(&imagetable, tex->ident, tex, buck);
	return tex;
}

image_t *Image_CreateTexture (const char *identifier, const char *subdir, unsigned int flags)
{
	image_t *image;
#ifdef LOADERTHREAD
	Sys_LockMutex(com_resourcemutex);
#endif
	image = Image_CreateTexture_Internal(identifier, subdir, flags);
#ifdef LOADERTHREAD
	Sys_UnlockMutex(com_resourcemutex);
#endif
	return image;
}

#ifdef WEBCLIENT
//called on main thread. oh well.
static void Image_Downloaded(struct dl_download *dl)
{
	qboolean success = false;
	image_t *tex = dl->user_ctx;
	image_t *p;

	//make sure the renderer wasn't restarted mid-download
	for (p = imagelist; p; p = p->next)
		if (p == tex)
			break;
	if (p)
	{
		if (dl->status == DL_FINISHED)
		{
			size_t fsize = VFS_GETLEN(dl->file);
			char *buf = BZ_Malloc(fsize);
			if (VFS_READ(dl->file, buf, fsize) == fsize)
				if (Image_LoadTextureFromMemory(tex, tex->flags, tex->ident, dl->url, buf, fsize))
					success = true;
		}
		if (!success)
			Image_LoadTexture_Failed(tex, NULL, 0, 0);
	}
}
#endif

//find a texture. will try to load it from disk, using the fallback if it would fail.
image_t *Image_GetTexture(const char *identifier, const char *subpath, unsigned int flags, void *fallbackdata, void *fallbackpalette, int fallbackwidth, int fallbackheight, uploadfmt_t fallbackfmt)
{
	image_t *tex;

	qboolean dontposttoworker = (flags & (IF_NOWORKER | IF_LOADNOW));
	qboolean lowpri = (flags & IF_LOWPRIORITY);
//	qboolean highpri = (flags & IF_HIGHPRIORITY);
	flags &= ~(IF_LOADNOW | IF_LOWPRIORITY | IF_HIGHPRIORITY);

#ifdef LOADERTHREAD
	Sys_LockMutex(com_resourcemutex);
#endif
	tex = Image_FindTexture(identifier, subpath, flags);
	if (tex)
	{
		//FIXME: race condition is possible here
		//if a non-replaced texture is given a fallback while a non-fallback version is still loading, it can still fail.
		if (tex->status == TEX_FAILED && fallbackdata)
			tex->status = TEX_LOADING;
		else if (tex->status != TEX_NOTLOADED)
		{
#ifdef LOADERTHREAD
			Sys_UnlockMutex(com_resourcemutex);
#endif
			return tex;	//already exists
		}
		tex->flags = flags;
	}
	else
		tex = Image_CreateTexture_Internal(identifier, subpath, flags);

	tex->status = TEX_LOADING;
	if (fallbackdata)
	{
		int b = fallbackwidth*fallbackheight, pb = 0;
		switch(fallbackfmt)
		{
		case TF_8PAL24:
			pb = 3*256;
			b *= 1;
			break;
		case TF_8PAL32:
			pb = 4*256;
			b *= 1;
			break;
		case TF_LUM8:
		case TF_SOLID8:
		case TF_TRANS8:
		case TF_TRANS8_FULLBRIGHT:
		case TF_H2_T7G1:
		case TF_H2_TRANS8_0:
		case TF_H2_T4A4:
		case TF_HEIGHT8:
		case TF_HEIGHT8PAL:	//we don't care about the actual palette.
			b *= 1;
			break;
		case TF_RGBX32:
		case TF_RGBA32:
		case TF_BGRX32:
		case TF_BGRA32:
			b *= 4;
			break;
		case TF_MIP4_8PAL24:
		case TF_MIP4_8PAL24_T255:
			pb = 3*256;
		case TF_MIP4_LUM8:
		case TF_MIP4_SOLID8:
			b = (fallbackwidth>>0)*(fallbackheight>>0) +
				(fallbackwidth>>1)*(fallbackheight>>1) +
				(fallbackwidth>>2)*(fallbackheight>>2) +
				(fallbackwidth>>3)*(fallbackheight>>3);
			break;
		default:
			Sys_Error("Image_GetTexture: bad format");
		}
		tex->fallbackdata = BZ_Malloc(b + pb);
		memcpy(tex->fallbackdata, fallbackdata, b);
		if (pb)
			memcpy((qbyte*)tex->fallbackdata + b, fallbackpalette, pb);
		tex->fallbackwidth = fallbackwidth;
		tex->fallbackheight = fallbackheight;
		tex->fallbackfmt = fallbackfmt;
	}
	else
	{
		tex->fallbackdata = NULL;
		tex->fallbackwidth = 0;
		tex->fallbackheight = 0;
		tex->fallbackfmt = TF_INVALID;
	}
#ifdef LOADERTHREAD
	Sys_UnlockMutex(com_resourcemutex);
#endif
	//FIXME: pass fallback through this way instead?

	if (dontposttoworker)
		Image_LoadHiResTextureWorker(tex, NULL, 0, 0);
	else
	{
#ifdef WEBCLIENT
		if (!strncmp(tex->ident, "http://", 7) || !strncmp(tex->ident, "https://", 8))
		{
			struct dl_download *dl;
			size_t sizelimit = max(0,r_image_downloadsizelimit.ival);
			if (sizelimit>0 || !*r_image_downloadsizelimit.string)
				dl = HTTP_CL_Get(tex->ident, NULL, Image_Downloaded);
			else
			{
				Con_Printf("r_image_downloadsizelimit: image downloading is blocked\n");
				dl = NULL;
			}
			if (dl)
			{
				if (sizelimit)
					dl->sizelimit = sizelimit;
				dl->user_ctx = tex;
				dl->file = VFSPIPE_Open();
				dl->isquery = true;
			}
#ifdef MULTITHREAD
			DL_CreateThread(dl, NULL, NULL);
#else
			tex->status = TEX_FAILED;	//HACK: so nothing waits for it.
#endif
		}
		else
#endif
			if (lowpri)
			COM_AddWork(WG_LOADER, Image_LoadHiResTextureWorker, tex, NULL, 0, 0);
		else
			COM_AddWork(WG_LOADER, Image_LoadHiResTextureWorker, tex, NULL, 0, 0);
	}
	return tex;
}
void Image_Upload			(texid_t tex, uploadfmt_t fmt, void *data, void *palette, int width, int height, unsigned int flags)
{
	struct pendingtextureinfo mips;
	size_t i;

	mips.extrafree = NULL;
	mips.type = (flags & IF_3DMAP)?PTI_3D:PTI_2D;
	if (!Image_GenMip0(&mips, flags, data, palette, width, height, fmt, false))
		return;
	Image_GenerateMips(&mips, flags);
	Image_ChangeFormat(&mips, flags, fmt);
	rf->IMG_LoadTextureMips(tex, &mips);
	tex->width = width;
	tex->height = height;
	tex->status = TEX_LOADED;

	for (i = 0; i < mips.mipcount; i++)
		if (mips.mip[i].needfree)
			BZ_Free(mips.mip[i].data);
	if (mips.extrafree)
		BZ_Free(mips.extrafree);
}

typedef struct
{
	char *name;
	char *legacyname;
	int	maximize, minmip, minimize;
} texmode_t;
static texmode_t texmodes[] = {
	{"n",	"GL_NEAREST",					0,	-1,	0},
	{"l",	"GL_LINEAR",					1,	-1,	1},
	{"nn",	"GL_NEAREST_MIPMAP_NEAREST",	0,	0,	0},
	{"ln",	"GL_LINEAR_MIPMAP_NEAREST",		1,	0,	1},
	{"nl",	"GL_NEAREST_MIPMAP_LINEAR",		0,	1,	0},
	{"ll",	"GL_LINEAR_MIPMAP_LINEAR",		1,	1,	1},

	//more explicit names
	{"n.n",	NULL,							0,	-1,	0},
	{"l.l",	NULL,							1,	-1,	1},
	{"nnn",	NULL,							0,	0,	0},
	{"lnl",	NULL,							1,	0,	1},
	{"nln",	NULL,							0,	1,	0},
	{"lll",	NULL,							1,	1,	1},

	//inverted mag filters
	{"n.l",	NULL,							0,	-1,	1},
	{"l.n",	NULL,							1,	-1,	0},
	{"nnl",	NULL,							0,	0,	1},
	{"lnn",	NULL,							1,	0,	0},
	{"nll",	NULL,							0,	1,	1},
	{"lln",	NULL,							1,	1,	0}
};
static void Image_ParseTextureMode(char *cvarname, char *modename, int modes[3])
{
	int i;
	modes[0] = 1;
	modes[1] = 0;
	modes[2] = 1;
	for (i = 0; i < sizeof(texmodes) / sizeof(texmodes[0]); i++)
	{
		if (!Q_strcasecmp(modename, texmodes[i].name) || (texmodes[i].legacyname && !Q_strcasecmp(modename, texmodes[i].legacyname)))
		{
			modes[0] = texmodes[i].minimize;
			modes[1] = texmodes[i].minmip;
			modes[2] = texmodes[i].maximize;
			return;
		}
	}
	Con_Printf("%s: mode %s was not recognised\n", cvarname, modename);
}
void QDECL Image_TextureMode_Callback (struct cvar_s *var, char *oldvalue)
{
	int mip[3]={1,0,1}, pic[3]={1,-1,1}, mipcap[2] = {0, 1000};
	float anis = 1;
	char *s;
	extern cvar_t gl_texturemode, gl_texturemode2d, gl_texture_anisotropic_filtering, gl_mipcap;

	Image_ParseTextureMode(gl_texturemode.name, gl_texturemode.string, mip);
	Image_ParseTextureMode(gl_texturemode2d.name, gl_texturemode2d.string, pic);
	anis = gl_texture_anisotropic_filtering.value;
	//parse d_mipcap (two values, nearest furthest)
	s = COM_Parse(gl_mipcap.string);
	mipcap[0] = *com_token?atoi(com_token):0;
//	if (mipcap[0] > 3)	/*cap it to 3, so no 16*16 textures get bugged*/
//		mipcap[0] = 3;
	s = COM_Parse(s);
	mipcap[1] = *com_token?atoi(com_token):1000;
	if (mipcap[1] < mipcap[0])
		mipcap[1] = mipcap[0];

	if (rf && rf->IMG_UpdateFiltering)
		rf->IMG_UpdateFiltering(imagelist, mip, pic, mipcap, anis);
}
qboolean Image_UnloadTexture(image_t *tex)
{
	if (tex->status == TEX_LOADED)
	{
		rf->IMG_DestroyTexture(tex);
		tex->status = TEX_NOTLOADED;
		return true;
	}
	return false;
}

//nukes an existing texture, destroying all traces. any lingering references will cause problems, so be careful about how you access these.
void Image_DestroyTexture(image_t *tex)
{
	image_t **link;
	if (!tex)
		return;
	TEXDOWAIT(tex);	//just in case.
#ifdef LOADERTHREAD
	Sys_LockMutex(com_resourcemutex);
#endif
	Image_UnloadTexture(tex);

	for (link = &imagelist; *link; link = &(*link)->next)
	{
		if (*link == tex)
		{
			*link = tex->next;
			break;
		}
	}
#ifdef LOADERTHREAD
	Sys_UnlockMutex(com_resourcemutex);
#endif
	if (*tex->ident)
		Hash_RemoveData(&imagetable, tex->ident, tex);
	Z_Free(tex);
}


void Shader_TouchTextures(void);
void Image_Purge(void)
{
#ifdef PURGEIMAGES
	image_t *tex, *a;
	int loaded = 0, total = 0;
	size_t mem = 0;
	Shader_TouchTextures();
	for (tex = imagelist; tex; tex = tex->next)
	{
		if (tex->flags & IF_NOPURGE)
			continue;
		if (tex->regsequence != r_regsequence)
			Image_UnloadTexture(tex);
	}
#endif
}


void Image_List_f(void)
{
	flocation_t loc;
	image_t *tex, *a;
	int loaded = 0, total = 0;
	size_t mem = 0;
	unsigned int loadflags;
	char fname[MAX_QPATH];
	const char *filter = Cmd_Argv(1);
	for (tex = imagelist; tex; tex = tex->next)
	{
		total++;
		if (*filter && !strstr(tex->ident, filter))
			continue;
		if (tex->subpath)
			Con_Printf("^h(%s)^h", tex->subpath);
		
		if (Image_LocateHighResTexture(tex, &loc, fname, sizeof(fname), &loadflags))
		{
			char defuck[MAX_OSPATH], *bullshit;
			Q_strncpyz(defuck, loc.search->logicalpath, sizeof(defuck));
			while((bullshit=strchr(defuck, '\\')))
				*bullshit = '/';
			Con_Printf("^[%s\\desc\\%s/%s^]: ", tex->ident, defuck, fname);
		}
		else
			Con_Printf("%s: ", tex->ident);

		for (a = tex->aliasof; a; a = a->aliasof)
		{
			if (a->subpath)
				Con_Printf("^3^h(%s)^h%s: ", a->subpath, a->ident);
			else
				Con_Printf("^3%s: ", a->ident);
		}

		if (tex->status == TEX_LOADED)
		{
			Con_Printf("^2loaded\n");
			if (!tex->aliasof)
			{
				mem += tex->width * tex->height * 4;
				loaded++;
			}
		}
		else if (tex->status == TEX_FAILED)
			Con_Printf("^1failed\n");
		else if (tex->status == TEX_NOTLOADED)
			Con_Printf("^5not loaded\n");
		else
			Con_Printf("^bloading\n");
	}

	Con_Printf("%i images loaded (%i known)\n", loaded, total);
}

//may not create any images yet.
void Image_Init(void)
{
	wadmutex = Sys_CreateMutex();
	memset(imagetablebuckets, 0, sizeof(imagetablebuckets));
	Hash_InitTable(&imagetable, sizeof(imagetablebuckets)/sizeof(imagetablebuckets[0]), imagetablebuckets);

	Cmd_AddCommandD("image_list", Image_List_f, "Prints out a list of the currently-known textures.");
}
//destroys all textures
void Image_Shutdown(void)
{
	image_t *tex;
	int i = 0, j = 0;
	Cmd_RemoveCommand("image_list");
	while (imagelist)
	{
		tex = imagelist;
		if (*tex->ident)
			Hash_RemoveData(&imagetable, tex->ident, tex);
		imagelist = tex->next;
		if (tex->status == TEX_LOADED)
			j++;
		rf->IMG_DestroyTexture(tex);
		Z_Free(tex);
		i++;
	}
	if (i)
		Con_DPrintf("Destroyed %i/%i images\n", j, i);

	if (wadmutex)
		Sys_DestroyMutex(wadmutex);
	wadmutex = NULL;
}

//load the named file, without failing.
texid_t R_LoadHiResTexture(const char *name, const char *subpath, unsigned int flags)
{
	char nicename[MAX_QPATH], *data;
	if (!*name)
		return r_nulltex;
	Q_strncpyz(nicename, name, sizeof(nicename));
	while((data = strchr(nicename, '*')))
		*data = '#';
	return Image_GetTexture(nicename, subpath, flags, NULL, NULL, 0, 0, TF_INVALID);	//queues the texture creation.
}

//attempt to load the named texture
//will not load external textures if gl_load24bit is set (failing instantly if its just going to fail later on anyway)
//the specified data will be used if the high-res image is blocked/not found.
texid_t R_LoadReplacementTexture(const char *name, const char *subpath, unsigned int flags, void *lowres, int lowreswidth, int lowresheight, uploadfmt_t format)
{
	char nicename[MAX_QPATH], *data;
	if (!*name)
		return r_nulltex;
	if (!gl_load24bit.ival && !lowres)
		return r_nulltex;
	Q_strncpyz(nicename, name, sizeof(nicename));
	while((data = strchr(nicename, '*')))
		*data = '#';
	return Image_GetTexture(nicename, subpath, flags, lowres, NULL, lowreswidth, lowresheight, format);	//queues the texture creation.
}
#ifdef RTLIGHTS
void R_LoadNumberedLightTexture(dlight_t *dl, int cubetexnum)
{
	Q_snprintfz(dl->cubemapname, sizeof(dl->cubemapname), "cubemaps/%i", cubetexnum);
	if (!gl_load24bit.ival)
		dl->cubetexture = r_nulltex;
	else
		dl->cubetexture = Image_GetTexture(dl->cubemapname, NULL, IF_CUBEMAP, NULL, NULL, 0, 0, TF_INVALID);
}
#endif

#if 0
extern cvar_t r_shadow_bumpscale_bumpmap;
texid_t R_LoadBumpmapTexture(const char *name, const char *subpath)
{
	char *buf, *data;
	texid_t tex;
//	int h;
	char fname[MAX_QPATH], nicename[MAX_QPATH];
	qboolean hasalpha;

	static char *extensions[] =
	{//reverse order of preference - (match commas with optional file types)
		".tga",
		""
	};

	int i, e;

	TRACE(("dbg: Mod_LoadBumpmapTexture: texture %s\n", name));

	COM_StripExtension(name, nicename, sizeof(nicename));

	tex = R_FindTexture(name, 0);
	if (TEXVALID(tex))	//don't bother if it already exists.
	{
		image_width = tex->width;
		image_height = tex->height;
		return tex;
	}

	tex = R_LoadCompressed(name);
	if (TEXVALID(tex))
		return tex;

	if (strchr(name, '/'))	//never look in a root dir for the pic
		i = 0;
	else
		i = 1;

	//should write this nicer.
	for (; i < sizeof(tex_path)/sizeof(tex_path[0]); i++)
	{
		if (!tex_path[i].enabled)
			continue;
		for (e = sizeof(extensions)/sizeof(char *)-1; e >=0 ; e--)
		{
			size_t fsize;
			if (tex_path[i].args >= 3)
			{
				if (!subpath)
					continue;
				snprintf(fname, sizeof(fname)-1, tex_path[i].path, subpath, nicename, extensions[e]);
			}
			else
				snprintf(fname, sizeof(fname)-1, tex_path[i].path, nicename, extensions[e]);

			TRACE(("dbg: Mod_LoadBumpmapTexture: opening %s\n", fname));

			if ((buf = COM_LoadFile (fname, 5, &fsize)))
			{
				if ((data = ReadTargaFile(buf, fsize, &image_width, &image_height, &hasalpha, 2)))	//Only load a greyscale image.
				{
					TRACE(("dbg: Mod_LoadBumpmapTexture: tga %s loaded\n", name));
					TEXASSIGNF(tex, R_LoadTexture8Bump(name, image_width, image_height, data, IF_NOALPHA|IF_NOGAMMA));
					BZ_Free(data);
				}
				else
				{
					BZ_Free(buf);
					continue;
				}

				BZ_Free(buf);

				return tex;
			}
		}
	}
	return r_nulltex;
}
#endif

// ocrana led functions
static int ledcolors[8][3] =
{
	// green
	{ 0, 255, 0 },
	{ 0, 127, 0 },
	// red
	{ 255, 0, 0 },
	{ 127, 0, 0 },
	// yellow
	{ 255, 255, 0 },
	{ 127, 127, 0 },
	// blue
	{ 0, 0, 255 },
	{ 0, 0, 127 }
};

void AddOcranaLEDsIndexed (qbyte *image, int h, int w)
{
	int tridx[8]; // transition indexes
	qbyte *point;
	int i, idx, x, y, rs;
	int r, g, b, rd, gd, bd;

	// calc row size, character size
	rs = w;
	h /= 16;
	w /= 16;

	// generate palettes
	for (i = 0; i < 4; i++)
	{
		// get palette
		r = ledcolors[i*2][0];
		g = ledcolors[i*2][1];
		b = ledcolors[i*2][2];
		rd = (r - ledcolors[i*2+1][0]) / 8;
		gd = (g - ledcolors[i*2+1][1]) / 8;
		bd = (b - ledcolors[i*2+1][2]) / 8;
		for (idx = 0; idx < 8; idx++)
		{
			tridx[idx] = GetPaletteIndex(r, g, b);
			r -= rd;
			g -= gd;
			b -= bd;
		}

		// generate LED into image
		b = (w * w + h * h) / 16;
		if (b < 1)
			b = 1;
		rd = w + 1;
		gd = h + 1;

		point = image + (8 * rs * h) + ((6 + i) * w);
		for (y = 1; y <= h; y++)
		{
			for (x = 1; x <= w; x++)
			{
				r = rd - (x*2); r *= r;
				g = gd - (y*2); g *= g;
				idx = (r + g) / b;

				if (idx > 7)
					*point++ = 0;
				else
					*point++ = tridx[idx];
			}
			point += rs - w;
		}
	}
}
#endif
