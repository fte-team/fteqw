#include "quakedef.h"
#ifdef RGLQUAKE
#include "glquake.h"
#endif

#ifdef D3DQUAKE
#include "d3dquake.h"
#endif

cvar_t r_dodgytgafiles = SCVAR("r_dodgytgafiles", "0");	//Certain tgas are upside down.
													//This is due to a bug in tenebrae.
													//(normally) the textures are actually the right way around.
													//but some people have gone and 'fixed' those broken ones by flipping.
													//these images appear upside down in any editor but correct in tenebrae
													//set this to 1 to emulate tenebrae's bug.
cvar_t r_dodgypcxfiles = SCVAR("r_dodgypcxfiles", "0");	//Quake 2's PCX loading isn't complete,
													//and some Q2 mods include PCX files
													//that only work with this assumption

#ifndef _WIN32
#include <unistd.h>
#endif

//the eye doesn't see different colours in the same proportion.
//must add to slightly less than 1
#define NTSC_RED 0.299
#define NTSC_GREEN 0.587
#define NTSC_BLUE 0.114
#define NTSC_SUM (NTSC_RED + NTSC_GREEN + NTSC_BLUE)

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
	int				columns, rows, numPixels;
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
	numPixels = columns * rows;

	flipped = !((tgahead->attribs & 0x20) >> 5);
	if (r_dodgytgafiles.value)
		flipped = true;


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

//remember to free it
qbyte *ReadTargaFile(qbyte *buf, int length, int *width, int *height, int asgrey)
{
	unsigned char *data;

	qboolean flipped;

	tgaheader_t tgaheader;

	if (buf[16] != 8 && buf[16] != 16 && buf[16] != 24 && buf[16] != 32)
		return NULL;	//BUMMER!

	tgaheader.id_len = buf[0];
	tgaheader.cm_type = buf[1];
	tgaheader.version = buf[2];
	tgaheader.cm_idx = LittleShort(*(short *)&buf[3]);
	tgaheader.cm_len = LittleShort(*(short *)&buf[5]);
	tgaheader.cm_size = buf[7];
	tgaheader.originx = LittleShort(*(short *)&buf[8]);
	tgaheader.originy = LittleShort(*(short *)&buf[10]);
	tgaheader.width = LittleShort(*(short *)&buf[12]);
	tgaheader.height = LittleShort(*(short *)&buf[14]);
	tgaheader.bpp = buf[16];
	tgaheader.attribs = buf[17];

	flipped = !((tgaheader.attribs & 0x20) >> 5);
	if (r_dodgytgafiles.value)
		flipped = true;

	data=buf+18;
	data += tgaheader.id_len;

	*width = tgaheader.width;
	*height = tgaheader.height;

	if (asgrey == 2)	//grey only, load as 8 bit..
	{
		if (!tgaheader.version == 1 && !tgaheader.version == 3)
			return NULL;
	}
	if (tgaheader.version == 1 || tgaheader.version == 3)
	{
		return ReadGreyTargaFile(data, length, &tgaheader, asgrey);
	}
	else if (tgaheader.version == 10 || tgaheader.version == 11)
	{
#undef getc
#define getc(x) *data++
		unsigned row, rows=tgaheader.height, column, columns=tgaheader.width, packetHeader, packetSize, j;
		qbyte *pixbuf, *targa_rgba=BZ_Malloc(rows*columns*(asgrey?1:4)), *inrow;

		qbyte blue, red, green, alphabyte;

		if (tgaheader.version == 10 && tgaheader.bpp == 8) return NULL;
		if (tgaheader.version == 11 && tgaheader.bpp != 8) return NULL;

		for(row=rows-1; row>=0; row--)
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
				{        // run-length packet
					switch (tgaheader.bpp)
					{
						case 8:	//we made sure this was version 11
								blue = green = red = *data++;
								alphabyte = 255;
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
										blue = green = red = *data++;
										*pixbuf++ = red;
										*pixbuf++ = green;
										*pixbuf++ = blue;
										*pixbuf++ = 255;
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
										*pixbuf++ = *data++;
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
			#pragma comment(lib, "../libs/libpng.a")
		#elif defined(_WIN32)
			#include "png.h"
			#pragma comment(lib, "../libs/libpng.lib")
		#else
			#include <png.h>
		#endif
	#endif


#if defined(MINGW)	//hehehe... add annother symbol so the statically linked cygwin libpng can link
#undef setjmp
int setjmp (jmp_buf jb)
{
	return _setjmp(jb);
}
#endif

typedef struct {
	char *data;
	int readposition;
	int filelen;
} pngreadinfo_t;

void PNGAPI png_default_read_data(png_structp png_ptr, png_bytep data, png_size_t length);

void VARGS readpngdata(png_structp png_ptr,png_bytep data,png_size_t len)
{
	pngreadinfo_t *ri = (pngreadinfo_t*)png_ptr->io_ptr;
	if (ri->readposition+len > ri->filelen)
	{
		png_error(png_ptr, "unexpected eof");
		return;
	}
	memcpy(data, &ri->data[ri->readposition], len);
	ri->readposition+=len;
}

qbyte *png_rgba;
qbyte *ReadPNGFile(qbyte *buf, int length, int *width, int *height, const char *fname)
{
	qbyte header[8], **rowpointers = NULL, *data = NULL;
	png_structp png;
	png_infop pnginfo;
	int y, bitdepth, colortype, interlace, compression, filter, bytesperpixel;
	unsigned long rowbytes;
	pngreadinfo_t ri;
	png_uint_32 pngwidth, pngheight;

	memcpy(header, buf, 8);

	if (png_sig_cmp(header, 0, 8))
	{
		return (png_rgba = NULL);
	}

	if (!(png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)))
	{
		return (png_rgba = NULL);
	}

	if (!(pnginfo = png_create_info_struct(png)))
	{
		png_destroy_read_struct(&png, &pnginfo, NULL);
		return (png_rgba = NULL);
	}

	if (setjmp(png->jmpbuf))
	{
error:
		if (data)
			BZ_Free(data);
		if (rowpointers)
			BZ_Free(rowpointers);
        png_destroy_read_struct(&png, &pnginfo, NULL);
        return (png_rgba = NULL);
    }

	ri.data=buf;
	ri.readposition=8;
	ri.filelen=length;
	png_set_read_fn(png, &ri, readpngdata);

	png_set_sig_bytes(png, 8);
	png_read_info(png, pnginfo);
	png_get_IHDR(png, pnginfo, &pngwidth, &pngheight, &bitdepth, &colortype, &interlace, &compression, &filter);

	*width = pngwidth;
	*height = pngheight;

	if (colortype == PNG_COLOR_TYPE_PALETTE)
	{
		png_set_palette_to_rgb(png);
		png_set_filler(png, 255, PNG_FILLER_AFTER);
	}

	if (colortype == PNG_COLOR_TYPE_GRAY && bitdepth < 8)
		png_set_gray_1_2_4_to_8(png);

	if (png_get_valid( png, pnginfo, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);

	if (bitdepth >= 8 && colortype == PNG_COLOR_TYPE_RGB)
		png_set_filler(png, 255, PNG_FILLER_AFTER);

	if (colortype == PNG_COLOR_TYPE_GRAY || colortype == PNG_COLOR_TYPE_GRAY_ALPHA)
	{
		png_set_gray_to_rgb( png );
		png_set_filler(png, 255, PNG_FILLER_AFTER);
	}

	if (bitdepth < 8)
		png_set_expand (png);
	else if (bitdepth == 16)
		png_set_strip_16(png);


	png_read_update_info(png, pnginfo);
	rowbytes = png_get_rowbytes(png, pnginfo);
	bytesperpixel = png_get_channels(png, pnginfo);
	bitdepth = png_get_bit_depth(png, pnginfo);

	if (bitdepth != 8 || bytesperpixel != 4)
	{
		Con_Printf ("Bad PNG color depth and/or bpp (%s)\n", fname);
		png_destroy_read_struct(&png, &pnginfo, NULL);
		return (png_rgba = NULL);
	}

	data = BZF_Malloc(*height * rowbytes);
	rowpointers = BZF_Malloc(*height * sizeof(*rowpointers));

	if (!data || !rowpointers)
		goto error;

	for (y = 0; y < *height; y++)
		rowpointers[y] = data + y * rowbytes;

	png_read_image(png, rowpointers);
	png_read_end(png, NULL);

	png_destroy_read_struct(&png, &pnginfo, NULL);
	BZ_Free(rowpointers);
	return (png_rgba = data);
}




int Image_WritePNG (char *filename, int compression, qbyte *pixels, int width, int height)
{
	char name[MAX_OSPATH];
	int i;
	FILE *fp;
	png_structp png_ptr;
	png_infop info_ptr;
	png_byte **row_pointers;

	if (!FS_NativePath(filename, FS_GAMEONLY, name, sizeof(name)))
		return false;

	if (!(fp = fopen (name, "wb")))
	{
		COM_CreatePath (name);
		if (!(fp = fopen (name, "wb")))
			return false;
	}

    if (!(png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)))
	{
		fclose(fp);
			return false;
	}

    if (!(info_ptr = png_create_info_struct(png_ptr)))
	{
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		fclose(fp);
		return false;
    }

	if (setjmp(png_ptr->jmpbuf))
	{
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
		return false;
    }

	png_init_io(png_ptr, fp);
	compression = bound(0, compression, 100);
	png_set_compression_level(png_ptr, Z_NO_COMPRESSION + (compression*(Z_BEST_COMPRESSION-Z_NO_COMPRESSION))/100);

	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png_ptr, info_ptr);

	row_pointers = BZ_Malloc (sizeof(png_byte *) * height);
	for (i = 0; i < height; i++)
		row_pointers[height - i - 1] = pixels + i * width * 3;
	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);
	BZ_Free(row_pointers);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);
	return true;
}


#endif

#ifdef AVAIL_JPEGLIB
#define XMD_H	//fix for mingw

#if defined(MINGW)
	#define JPEG_API VARGS
	#include "./mingw-libs/jpeglib.h"
	#include "./mingw-libs/jerror.h"
	#pragma comment(lib, "../libs/jpeg.a")
#elif defined(_WIN32)
	#define JPEG_API VARGS
	#include "jpeglib.h"
	#include "jerror.h"
	#pragma comment(lib, "../libs/jpeg.lib")
#else
//	#include <jinclude.h>
	#include <jpeglib.h>
	#include <jerror.h>
#endif

#ifndef JPEG_FALSE
#define JPEG_BOOL boolean
#endif





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
  JPEG_BOOL start_of_file;	/* have we gotten any data yet? */
} my_source_mgr;

typedef my_source_mgr * my_src_ptr;

#define INPUT_BUF_SIZE  4096	/* choose an efficiently fread'able size */


METHODDEF(void)
init_source (j_decompress_ptr cinfo)
{
  my_src_ptr src = (my_src_ptr) cinfo->src;

  src->start_of_file = TRUE;
}

METHODDEF(JPEG_BOOL)
fill_input_buffer (j_decompress_ptr cinfo)
{
	my_source_mgr *src = (my_source_mgr*) cinfo->src;
	size_t nbytes;

	nbytes = src->maxlen - src->currentpos;
	if (nbytes > INPUT_BUF_SIZE)
		nbytes = INPUT_BUF_SIZE;
	memcpy(src->buffer, &src->infile[src->currentpos], nbytes);
	src->currentpos+=nbytes;

	if (nbytes <= 0) {
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
	src->start_of_file = FALSE;

	return TRUE;
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
jpeg_mem_src (j_decompress_ptr cinfo, qbyte * infile, int maxlen)
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
  src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
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


  /* Step 1: allocate and initialize JPEG decompression object */

  /* We set up the normal JPEG error routines, then override error_exit. */
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    // If we get here, the JPEG code has signaled an error.

badjpeg:

    jpeg_destroy_decompress(&cinfo);

	if (mem)
		BZ_Free(mem);
    return 0;
  }
  jpeg_create_decompress(&cinfo);

  jpeg_mem_src(&cinfo, infile, length);

  (void) jpeg_read_header(&cinfo, TRUE);

  (void) jpeg_start_decompress(&cinfo);


  if (cinfo.output_components == 0)
  {
  		#ifdef _DEBUG
  		Con_Printf("No JPEG Components, not a JPEG.\n");
  		#endif
  		goto badjpeg;
  }
  if (cinfo.output_components!=3)
  {
		#ifdef _DEBUG
		Con_Printf("Bad number of components in JPEG: '%d', should be '3'.\n",cinfo.output_components);
		#endif
		goto badjpeg;
  }
  size_stride = cinfo.output_width * cinfo.output_components;
  /* Make a one-row-high sample array that will go away when done with image */
   buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, size_stride, 1);

   out=mem=BZ_Malloc(cinfo.output_height*cinfo.output_width*4);
   memset(out, 0, cinfo.output_height*cinfo.output_width*4);

  while (cinfo.output_scanline < cinfo.output_height) {
    (void) jpeg_read_scanlines(&cinfo, buffer, 1);

	in = buffer[0];
	for (i = 0; i < cinfo.output_width; i++)
	{//rgb to rgba
		*out++ = *in++;
		*out++ = *in++;
		*out++ = *in++;
		*out++ = 255;
	}
  }

  (void) jpeg_finish_decompress(&cinfo);

  jpeg_destroy_decompress(&cinfo);

  *width = cinfo.output_width;
  *height = cinfo.output_height;

  return mem;

}


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
METHODDEF(JPEG_BOOL) empty_output_buffer (j_compress_ptr cinfo)
{
	my_destination_mgr *dest = (my_destination_mgr*) cinfo->dest;

	VFS_WRITE(dest->vfs, dest->buffer, OUTPUT_BUF_SIZE);
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

	return TRUE;
}
METHODDEF(void) term_destination (j_compress_ptr cinfo)
{
	my_destination_mgr *dest = (my_destination_mgr*) cinfo->dest;

	VFS_WRITE(dest->vfs, dest->buffer, OUTPUT_BUF_SIZE - dest->pub.free_in_buffer);
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

void jpeg_mem_dest (j_compress_ptr cinfo, vfsfile_t *vfs)
{
  my_destination_mgr *dest;

  if (cinfo->dest == NULL) {	/* first time for this JPEG object? */
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



METHODDEF(void) jpeg_error_exit (j_common_ptr cinfo) {
  longjmp(((jpeg_error_mgr_wrapper *) cinfo->err)->setjmp_buffer, 1);
}
void screenshotJPEG(char *filename, int compression, qbyte *screendata, int screenwidth, int screenheight)	//input is rgb NOT rgba
{
	qbyte	*buffer;
	vfsfile_t	*outfile;
	jpeg_error_mgr_wrapper jerr;
	struct jpeg_compress_struct cinfo;
	JSAMPROW row_pointer[1];

	if (!(outfile = FS_OpenVFS(filename, "wb", FS_GAMEONLY)))
	{
		FS_CreatePath (filename, FS_GAME);
		if (!(outfile = FS_OpenVFS(filename, "wb", FS_GAMEONLY)))
		{
			Con_Printf("Error opening %s\n", filename);
			return;
		}
	}

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jpeg_error_exit;
	if (setjmp(jerr.setjmp_buffer))
	{
		jpeg_destroy_compress(&cinfo);
		VFS_CLOSE(outfile);
		FS_Remove(filename, FS_GAME);
		Con_Printf("Failed to create jpeg\n");
		return;
	}
	jpeg_create_compress(&cinfo);

	buffer = screendata;

	jpeg_mem_dest(&cinfo, outfile);
	cinfo.image_width = screenwidth;
	cinfo.image_height = screenheight;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality (&cinfo, bound(0, compression, 100), true);
	jpeg_start_compress(&cinfo, true);

	while (cinfo.next_scanline < cinfo.image_height)
	{
	    *row_pointer = &buffer[(cinfo.image_height - cinfo.next_scanline - 1) * cinfo.image_width * 3];
	    jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	VFS_CLOSE(outfile);
	jpeg_destroy_compress(&cinfo);
}

#endif


/*
==============
WritePCXfile
==============
*/
void WritePCXfile (const char *filename, qbyte *data, int width, int height,
	int rowbytes, qbyte *palette, qboolean upload) //data is 8bit.
{
	int		i, j, length;
	pcx_t	*pcx;
	qbyte		*pack;

	pcx = Hunk_TempAlloc (width*height*2+1000);
	if (pcx == NULL)
	{
		Con_Printf("SCR_ScreenShot_f: not enough memory\n");
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
		COM_WriteFile (filename, pcx, length);
}



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

	if (r_dodgypcxfiles.value)
		palette = host_basepal;
	else
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
					pix[3] = 0;
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
	qbyte	*palette;
	qbyte	*pix;
	int		x, y;
	int		dataByte, runLength;
	int		count;
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


	palette = buf + length-768;

	data = (char *)(pcx+1);

	count = (swidth) * (sheight);

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
	unsigned short	Type;
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
	bmpheader_t h, *in;
	qbyte *data;

	in = (bmpheader_t *)buf;
	h.Type = LittleShort(in->Type);
	if (h.Type != 'B' + ('M'<<8))
		return NULL;
	h.Size = LittleLong(in->Size);
	h.Reserved1 = LittleShort(in->Reserved1);
	h.Reserved2 = LittleShort(in->Reserved2);
	h.OffsetofBMPBits = LittleLong(in->OffsetofBMPBits);
	h.SizeofBITMAPINFOHEADER = LittleLong(in->SizeofBITMAPINFOHEADER);
	h.Width = LittleLong(in->Width);
	h.Height = LittleLong(in->Height);
	h.Planes = LittleShort(in->Planes);
	h.BitCount = LittleShort(in->BitCount);
	h.Compression = LittleLong(in->Compression);
	h.ImageSize = LittleLong(in->ImageSize);
	h.TargetDeviceXRes = LittleLong(in->TargetDeviceXRes);
	h.TargetDeviceYRes = LittleLong(in->TargetDeviceYRes);
	h.NumofColorIndices = LittleLong(in->NumofColorIndices);
	h.NumofImportantColorIndices = LittleLong(in->NumofImportantColorIndices);

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
			pal[i] = data[i*4+0] + (data[i*4+1]<<8) + (data[i*4+2]<<16) + (255/*data[i*4+3]*/<<16);
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
#if defined(RGLQUAKE)
	int i;
	extern qbyte gammatable[256];

	if (qrenderer != QR_OPENGL)
		return;//don't brighten in SW.

	for (i=0 ; i<width*height*4 ; i+=4)
	{
		rgba[i+0] = gammatable[rgba[i+0]];
		rgba[i+1] = gammatable[rgba[i+1]];
		rgba[i+2] = gammatable[rgba[i+2]];
		//and not alpha
	}
#endif
}







#if defined(RGLQUAKE) || defined(D3DQUAKE)

#ifdef DDS
#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT                   0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT                  0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT                  0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT                  0x83F3
#endif

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


int GL_LoadTextureDDS(unsigned char *buffer, int filesize)
{
	extern int		gl_filter_min;
	extern int		gl_filter_max;
	int texnum;
	int nummips;
	int mipnum;
	int datasize;
	int intfmt;
	int pad;

	ddsheader fmtheader;
	if (*(int*)buffer != *(int*)"DDS ")
		return 0;
	buffer+=4;

	memcpy(&fmtheader, buffer, sizeof(fmtheader));
	if (fmtheader.dwSize != sizeof(fmtheader))
		return 0;	//corrupt/different version

	buffer += fmtheader.dwSize;

	nummips = fmtheader.dwMipMapCount;
	if (nummips < 1)
		nummips = 1;

	if (*(int*)&fmtheader.ddpfPixelFormat.dwFourCC == *(int*)"DXT1")
	{
		intfmt = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;	//alpha or not? Assume yes, and let the drivers decide.
		pad = 8;
	}
	else if (*(int*)&fmtheader.ddpfPixelFormat.dwFourCC == *(int*)"DXT3")
	{
		intfmt = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		pad = 8;
	}
	else if (*(int*)&fmtheader.ddpfPixelFormat.dwFourCC == *(int*)"DXT5")
	{
		intfmt = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		pad = 8;
	}
	else
		return 0;

	if (!qglCompressedTexImage2DARB)
		return 0;

	texnum = GL_AllocNewTexture();
	GL_Bind(texnum);

	datasize = fmtheader.dwPitchOrLinearSize;
	for (mipnum = 0; mipnum < nummips; mipnum++)
	{
//	(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid* data);
		if (datasize < pad)
			datasize = pad;
		qglCompressedTexImage2DARB(GL_TEXTURE_2D, mipnum, intfmt, fmtheader.dwWidth>>mipnum, fmtheader.dwHeight>>mipnum, 0, datasize, buffer);
		if (qglGetError())
			Con_Printf("Incompatible dds file (mip %i)\n", mipnum);
		buffer += datasize;
		datasize/=4;
	}
	if (qglGetError())
		Con_Printf("Incompatible dds file\n");


	if (nummips>1)
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}

	return texnum;
}
#endif

//returns r8g8b8a8
qbyte *Read32BitImageFile(qbyte *buf, int len, int *width, int *height, char *fname)
{
	qbyte *data;
	if ((data = ReadTargaFile(buf, len, width, height, false)))
	{
		TRACE(("dbg: Read32BitImageFile: tga\n"));
		return data;
	}

#ifdef AVAIL_PNGLIB
	if ((buf[0] == 137 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G') && (data = ReadPNGFile(buf, com_filesize, width, height, fname)))
	{
		TRACE(("dbg: Read32BitImageFile: png\n"));
		return data;
	}
#endif
#ifdef AVAIL_JPEGLIB
	//jpeg jfif only.
	if ((buf[0] == 0xff && buf[1] == 0xd8 && buf[2] == 0xff && buf[3] == 0xe0) && (data = ReadJPEGFile(buf, com_filesize, width, height)))
	{
		TRACE(("dbg: Read32BitImageFile: jpeg\n"));
		return data;
	}
#endif
	if ((data = ReadPCXFile(buf, com_filesize, width, height)))
	{
		TRACE(("dbg: Read32BitImageFile: pcx\n"));
		return data;
	}

	if ((buf[0] == 'B' && buf[1] == 'M') && (data = ReadBMPFile(buf, com_filesize, width, height)))
	{
		TRACE(("dbg: Read32BitImageFile: bitmap\n"));
		return data;
	}

	TRACE(("dbg: Read32BitImageFile: life sucks\n"));

	return NULL;
}

int image_width, image_height;
qbyte *COM_LoadFile (char *path, int usehunk);
//fixme: should probably get rid of the 'Mod' prefix, and use something more suitable.
int Mod_LoadHiResTexture(char *name, char *subpath, qboolean mipmap, qboolean alpha, qboolean colouradjust)
{
	qboolean alphaed;
	char *buf, *data;
	int len;
//	int h;
	char fname[MAX_QPATH], nicename[MAX_QPATH];

	static char *extensions[] = {//reverse order of preference - (match commas with optional file types)
		".pcx",	//pcxes are the original gamedata of q2. So we don't want them to override pngs.
#ifdef AVAIL_JPEGLIB
		".jpg",
#endif
		".bmp",
#ifdef AVAIL_PNGLIB
		".png",
#endif
		".tga",
		""
	};

	static char *path[] ={
		"2%s%s",
		"3textures/%s/%s%s",	//this is special... It uses the subpath parameter. Note references to (i == 1)
		"3%s/%s%s",
		"2textures/%s%s",
		"2override/%s%s"
	};

	int i, e;

	COM_StripAllExtensions(name, nicename, sizeof(nicename));

	while((data = strchr(nicename, '*')))
	{
		*data = '#';
	}

	if ((len = R_FindTexture(name))!=-1)	//don't bother if it already exists.
		return len;
	if (subpath && *subpath)
	{
		snprintf(fname, sizeof(fname)-1, "%s/%s", subpath, name);
		if ((len = R_FindTexture(fname))!=-1)	//don't bother if it already exists.
			return len;
	}

	if ((len = R_LoadCompressed(name)))
		return len;

	if (strchr(name, '/'))	//never look in a root dir for the pic
		i = 0;
	else
		i = 1;

	//should write this nicer.
	for (; i < sizeof(path)/sizeof(char *); i++)
	{
#ifdef DDS
		if (path[i][0] >= '3')
		{
			if (!subpath)
					continue;
			snprintf(fname, sizeof(fname)-1, path[i]+1, subpath, /*COM_SkipPath*/(nicename), ".dds");
		}
		else
			snprintf(fname, sizeof(fname)-1, path[i]+1, nicename, ".dds");
		if ((buf = COM_LoadFile (fname, 5)))
		{
			len = GL_LoadTextureDDS(buf, com_filesize);
			BZ_Free(buf);
			if (len)
				return len;
		}
#endif

		for (e = sizeof(extensions)/sizeof(char *)-1; e >=0 ; e--)
		{
			if (path[i][0] >= '3')
			{
				if (!subpath)
					continue;
				snprintf(fname, sizeof(fname)-1, path[i]+1, subpath, /*COM_SkipPath*/(nicename), extensions[e]);
			}
			else
				snprintf(fname, sizeof(fname)-1, path[i]+1, nicename, extensions[e]);
			TRACE(("dbg: Mod_LoadHiResTexture: trying %s\n", fname));
			if ((buf = COM_LoadFile (fname, 5)))
			{
				if ((data = Read32BitImageFile(buf, com_filesize, &image_width, &image_height, fname)))
				{
					extern cvar_t vid_hardwaregamma;
					if (colouradjust && !vid_hardwaregamma.value)
						BoostGamma(data, image_width, image_height);
					TRACE(("dbg: Mod_LoadHiResTexture: %s loaded\n", name));
					if (i == 1)
					{	//if it came from a special subpath (eg: map specific), upload it using the subpath prefix
						snprintf(fname, sizeof(fname)-1, "%s/%s", subpath, name);
						len = R_LoadTexture32 (fname, image_width, image_height, (unsigned*)data, mipmap, alpha);
					}
					else
						len = R_LoadTexture32 (name, image_width, image_height, (unsigned*)data, mipmap, alpha);

					BZ_Free(data);

					BZ_Free(buf);

					return len;
				}
				else
				{
					BZ_Free(buf);
					continue;
				}
			}
		}
	}

	//now look in wad files. (halflife compatability)
	data = W_GetTexture(name, &image_width, &image_height, &alphaed);
	if (data)
		return R_LoadTexture32 (name, image_width, image_height, (unsigned*)data, mipmap, alphaed);
	return 0;
}
int Mod_LoadReplacementTexture(char *name, char *subpath, qboolean mipmap, qboolean alpha, qboolean gammaadjust)
{
	if (!gl_load24bit.value)
		return 0;
	return Mod_LoadHiResTexture(name, subpath, mipmap, alpha, gammaadjust);
}

extern cvar_t r_shadow_bumpscale_bumpmap;
int Mod_LoadBumpmapTexture(char *name, char *subpath)
{
	char *buf, *data;
	int len;
//	int h;
	char fname[MAX_QPATH], nicename[MAX_QPATH];

	static char *extensions[] = {//reverse order of preference - (match commas with optional file types)
		".tga",
		""
	};

	static char *path[] ={
		"%s%s",
		"textures/%s/%s%s",	//this is special... It's special name is Mr Ben Ian Graham Hacksworth.
		"textures/%s%s",
		"override/%s%s"
	};

	int i, e;

	TRACE(("dbg: Mod_LoadBumpmapTexture: texture %s\n", name));

	COM_StripExtension(name, nicename, sizeof(nicename));

	if ((len = R_FindTexture(name))!=-1)	//don't bother if it already exists.
		return len;

	if ((len = R_LoadCompressed(name)))
		return len;

	if (strchr(name, '/'))	//never look in a root dir for the pic
		i = 0;
	else
		i = 1;

	//should write this nicer.
	for (; i < sizeof(path)/sizeof(char *); i++)
	{
		for (e = sizeof(extensions)/sizeof(char *)-1; e >=0 ; e--)
		{
			if (i == 1)
			{
				char map [MAX_QPATH*2];
#ifndef CLIENTONLY
				if (*sv.name)	//server loads before the client knows what's happening. I suppose we could have some sort of param...
					Q_strncpyz(map, sv.name, sizeof(map));
				else
#endif
					COM_FileBase(cl.model_name[1], map, sizeof(map));
				snprintf(fname, sizeof(fname)-1, path[i], map, nicename, extensions[e]);
			}
			else
				snprintf(fname, sizeof(fname)-1, path[i], nicename, extensions[e]);

			TRACE(("dbg: Mod_LoadBumpmapTexture: opening %s\n", fname));

			if ((buf = COM_LoadFile (fname, 5)))
			{
				if ((data = ReadTargaFile(buf, com_filesize, &image_width, &image_height, 2)))	//Only load a greyscale image.
				{
					TRACE(("dbg: Mod_LoadBumpmapTexture: tga %s loaded\n", name));
					len = R_LoadTexture8Bump(name, image_width, image_height, data, true, r_shadow_bumpscale_bumpmap.value);
					BZ_Free(data);
				}
				else
				{
					BZ_Free(buf);
					continue;
				}

				BZ_Free(buf);

				return len;
			}
		}
	}
	return 0;
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
