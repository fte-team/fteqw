#include "quakedef.h"
#ifdef RGLQUAKE
#include "glquake.h"
#endif

cvar_t r_dodgytgafiles = {"r_dodgytgafiles", "0"};	//Certain tgas are upside down.
													//This is due to a bug in tenebrae.
													//(normally) the textures are actually the right way around.
													//but some people have gone and 'fixed' those broken ones by flipping.
													//these images appear upside down in any editor but correct in tenebrae
													//set this to 1 to emulate tenebrae's bug.

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
	int				columns, rows, numPixels;
	int				row, column;
	qbyte			*pixbuf, *pal;
	qboolean		flipped;

	qbyte *pixels = BZ_Malloc(tgahead->width * tgahead->height * (asgrey?1:4));

	if (tgahead->version!=1 
		&& tgahead->version!=3) 
		Sys_Error ("LoadGrayTGA: Only type 1 and 3 greyscale targa images are understood.\n");

    if (tgahead->version==1 && tgahead->bpp != 8 && 
		tgahead->cm_size != 24 && tgahead->cm_len != 256)
		Sys_Error ("LoadGrayTGA: Strange palette type\n");

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
	//the eye doesn't see different colours in the same proportion.
	//must add to slightly less than 1
#define NTSC_RED 0.299
#define NTSC_GREEN 0.587
#define NTSC_BLUE 0.114

	char *data;

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
	else if (tgaheader.version == 10)
	{		
#undef getc
#define getc(x) *data++
		unsigned row, rows=tgaheader.height, column, columns=tgaheader.width, packetHeader, packetSize, j;
		qbyte *pixbuf, *targa_rgba=BZ_Malloc(rows*columns*(asgrey?1:4)), *inrow;

		qbyte blue, red, green, alphabyte;

		if (tgaheader.bpp == 8) return NULL;

		for(row=rows-1; row>=0; row--)
		{
			if (flipped)
				pixbuf = targa_rgba + row*columns*(asgrey?1:4);
			else
				pixbuf = targa_rgba + ((rows-1)-row)*columns*(asgrey?1:4);
			for(column=0; column<columns; )
			{
				packetHeader=getc(fin);
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80)
				{        // run-length packet
					switch (tgaheader.bpp)
					{
						case 16:
								inrow = data;
								data+=2;
								red = ((inrow[1] & 0x7c)>>2) *8;					//red
								green =	(((inrow[1] & 0x03)<<3) + ((inrow[0] & 0xe0)>>5))*8;	//green
								blue = (inrow[0] & 0x1f)*8;					//blue
								alphabyte = (int)(inrow[1]&0x80)*2-1;			//alpha?					
								break;
						case 24:
								blue = getc(fin);
								green = getc(fin);
								red = getc(fin);
								alphabyte = 255;
								break;
						case 32:
								blue = getc(fin);
								green = getc(fin);
								red = getc(fin);
								alphabyte = getc(fin);
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
								pixbuf = targa_rgba + row*columns*4;
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
								pixbuf = targa_rgba + row*columns;
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
										blue = getc(fin);
										green = getc(fin);
										red = getc(fin);
										*pixbuf++ = red;
										*pixbuf++ = green;
										*pixbuf++ = blue;
										*pixbuf++ = 255;
										break;
								case 32:
										blue = getc(fin);
										green = getc(fin);
										red = getc(fin);
										alphabyte = getc(fin);
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
								pixbuf = targa_rgba + row*columns*4;
							}						
						}
					}
					else	//convert to grey
					{
						for(j=0;j<packetSize;j++)
						{
							switch (tgaheader.bpp)
							{
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
										blue = getc(fin);
										green = getc(fin);
										red = getc(fin);
										*pixbuf++ = red*NTSC_RED + green*NTSC_GREEN + blue*NTSC_BLUE;
										break;
								case 32:
										blue = getc(fin);
										green = getc(fin);
										red = getc(fin);
										alphabyte = getc(fin);
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
								pixbuf = targa_rgba + row*columns;
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
#error no pngs with mingw
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
qbyte *ReadPNGFile(qbyte *buf, int length, int *width, int *height)
{
	qbyte header[8], **rowpointers = NULL, *data = NULL;
	png_structp png;
	png_infop pnginfo;
	int y, bitdepth, colortype, interlace, compression, filter, bytesperpixel;
	unsigned long rowbytes;
	pngreadinfo_t ri;

	memcpy(header, buf, 8);

	if (png_sig_cmp(header, 0, 8))
	{
		return (png_rgba = NULL);
	}

	if (!(png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)))
	{
		return (png_rgba = NULL);
	}

	if (!(pnginfo = png_create_info_struct(png))) {
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
	png_get_IHDR(png, pnginfo, (png_uint_32 *) width, (png_uint_32 *) height, &bitdepth, &colortype, &interlace, &compression, &filter);

	if (colortype == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png);
		png_set_filler(png, 255, PNG_FILLER_AFTER);			
	}

	if (colortype == PNG_COLOR_TYPE_GRAY && bitdepth < 8) 
		png_set_gray_1_2_4_to_8(png);
	
	if (png_get_valid( png, pnginfo, PNG_INFO_tRNS ))		
		png_set_tRNS_to_alpha(png); 

	if (bitdepth == 8 && colortype == PNG_COLOR_TYPE_RGB)	
		png_set_filler(png, 255, PNG_FILLER_AFTER);
	
	if (colortype == PNG_COLOR_TYPE_GRAY || colortype == PNG_COLOR_TYPE_GRAY_ALPHA) {
		png_set_gray_to_rgb( png );
		png_set_filler(png, 255, PNG_FILLER_AFTER);			
	}

	if (bitdepth < 8)
		png_set_expand (png);
	else if (bitdepth == 16)
		png_set_strip_16(png);

	
	png_read_update_info( png, pnginfo );
	rowbytes = png_get_rowbytes( png, pnginfo );
	bytesperpixel = png_get_channels( png, pnginfo );
	bitdepth = png_get_bit_depth(png, pnginfo);

	if (bitdepth != 8 || bytesperpixel != 4) {	
		Con_Printf ("Bad PNG color depth and/or bpp\n");		
		png_destroy_read_struct(&png, &pnginfo, NULL);
		return (png_rgba = NULL);
	}

	data = BZF_Malloc(*height * rowbytes );
	rowpointers = BZF_Malloc(*height * 4);

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




int Image_WritePNG (char *filename, int compression, qbyte *pixels, int width, int height) {
	char name[MAX_OSPATH];
	int i;
	FILE *fp;
	png_structp png_ptr;
	png_infop info_ptr;
	png_byte **row_pointers;
	_snprintf (name, sizeof(name)-1, "%s/%s", com_gamedir, filename);
	
	if (!(fp = fopen (name, "wb"))) {
		COM_CreatePath (name);
		if (!(fp = fopen (name, "wb")))
			return false;
	}

    if (!(png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
		fclose(fp);
			return false;
	}

    if (!(info_ptr = png_create_info_struct(png_ptr))) {
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		fclose(fp);
		return false;
    }

	if (setjmp(png_ptr->jmpbuf)) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
		return false;
    }

	png_init_io(png_ptr, fp);
	png_set_compression_level(png_ptr, (compression*Z_BEST_COMPRESSION)/100);

	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);	
	png_write_info(png_ptr, info_ptr);

	row_pointers = BZ_Malloc (4 * height);
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
#if defined(MINGW)
#error no jpegs with mingw
#elif defined(_WIN32)

#define JPEG_API VARGS
#include "jpeglib.h"
#include "jerror.h"
#pragma comment(lib, "../libs/jpeg.lib")

#else

//#include <jinclude.h>
#include <jpeglib.h>
#include <jerror.h>
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
  boolean start_of_file;	/* have we gotten any data yet? */
} my_source_mgr;

typedef my_source_mgr * my_src_ptr;

#define INPUT_BUF_SIZE  4096	/* choose an efficiently fread'able size */


METHODDEF(void)
init_source (j_decompress_ptr cinfo)
{
  my_src_ptr src = (my_src_ptr) cinfo->src;

  src->start_of_file = TRUE;
}

METHODDEF(boolean)
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

    jpeg_destroy_decompress(&cinfo);    

	if (mem)
		BZ_Free(mem);
    return 0;
  }  
  jpeg_create_decompress(&cinfo);

  jpeg_mem_src(&cinfo, infile, length);

  (void) jpeg_read_header(&cinfo, TRUE);

  (void) jpeg_start_decompress(&cinfo);


  if (cinfo.output_components!=3)
	  Sys_Error("Bad number of componants in jpeg");
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



typedef struct  {	
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
} jpeg_error_mgr_wrapper;

METHODDEF(void) jpeg_error_exit (j_common_ptr cinfo) {	
  longjmp(((jpeg_error_mgr_wrapper *) cinfo->err)->setjmp_buffer, 1);
}
extern char	com_basedir[];
void screenshotJPEG(char *filename, qbyte *screendata, int screenwidth, int screenheight)	//input is rgb NOT rgba
{
	char	name[MAX_OSPATH];
	qbyte	*buffer;
	FILE	*outfile;
	jpeg_error_mgr_wrapper jerr;
	struct jpeg_compress_struct cinfo;
	JSAMPROW row_pointer[1];

	sprintf (name, "%s/%s", com_gamedir, filename);	
	if (!(outfile = fopen (name, "wb"))) {
		COM_CreatePath (name);
		if (!(outfile = fopen (name, "wb")))
			Sys_Error ("Error opening %s", filename);
	}

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jpeg_error_exit;
	if (setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_compress(&cinfo);
		fclose(outfile);
		unlink(name);
		Con_Printf("Failed to create jpeg\n");
		return;
	}
	jpeg_create_compress(&cinfo);

	buffer = screendata;
	
	jpeg_stdio_dest(&cinfo, outfile);
	cinfo.image_width = screenwidth; 	
	cinfo.image_height = screenheight;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality (&cinfo, 75/*bound(0, (int) gl_image_jpeg_quality_level.value, 100)*/, true);
	jpeg_start_compress(&cinfo, true);

	while (cinfo.next_scanline < cinfo.image_height) {
	    *row_pointer = &buffer[(cinfo.image_height - cinfo.next_scanline - 1) * cinfo.image_width * 3];
	    jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	fclose(outfile);
	jpeg_destroy_compress(&cinfo);

	Con_Printf ("Wrote %s\n", filename);
}

#endif


/* 
============== 
WritePCXfile 
============== 
*/ 
void WritePCXfile (char *filename, qbyte *data, int width, int height,
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

//
// parse the PCX file
//	

	pcx = (pcx_t *)buf;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| pcx->xmax >= 1024
		|| pcx->ymax >= 1024)
	{		
		return NULL;
	}

	*width = pcx->xmax-pcx->xmin+1;
	*height = pcx->ymax-pcx->ymin+1;

	palette = buf + length-768;

	data = (char *)(pcx+1);

	count = (pcx->xmax+1) * (pcx->ymax+1);
	pcx_rgb = BZ_Malloc( count * 4);

	for (y=0 ; y<=pcx->ymax ; y++)
	{
		pix = pcx_rgb + 4*y*(pcx->xmax+1);
		for (x=0 ; x<=pcx->xmax ; )
		{
			dataByte = *data;
			data++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
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
		|| pcx->xmax >= 1024
		|| pcx->ymax >= 1024)
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

#if defined(RGLQUAKE)

//returns r8g8b8a8
qbyte *Read32BitImageFile(qbyte *buf, int len, int *width, int *height)
{
	qbyte *data;
	if ((data = ReadTargaFile(buf, len, width, height, false)))
		return data;
	
#ifdef AVAIL_PNGLIB
	if ((buf[0] == -119 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G') && (data = ReadPNGFile(buf, com_filesize, width, height)))
		return data;
#endif
#ifdef AVAIL_JPEGLIB
	//jpeg jfif only.
	if ((buf[0] == 0xff && buf[1] == 0xd8 && buf[2] == 0xff && buf[3] == 0xe0) && (data = ReadJPEGFile(buf, com_filesize, width, height)))
		return data;
#endif
	if ((data = ReadPCXFile(buf, com_filesize, width, height)))		
		return data;

	if ((buf[0] == 'B' && buf[1] == 'M') && (data = ReadBMPFile(buf, com_filesize, width, height)))
		return data;

	return NULL;
}

int GL_LoadTexture8Bump (char *identifier, int width, int height, unsigned char *data, qboolean mipmap);

int image_width, image_height;
qbyte *COM_LoadFile (char *path, int usehunk);
int Mod_LoadHiResTexture(char *name, qboolean mipmap, qboolean alpha, qboolean colouradjust)
{
	qboolean alphaed;
	char *buf, *data;
	int len;
//	int h;
	char fname[MAX_QPATH], nicename[MAX_QPATH];	

	static char *extensions[] = {//reverse order of preference - (match commas with optional file types)
		".pcx",	//pcxes are the origional gamedata of q2. So we don't want them to override pngs.
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
		"%s%s",
		"textures/%s/%s%s",	//this is special... It's special name is Mr Ben Ian Graham Hacksworth.
		"textures/%s%s",
		"override/%s%s",
		"pics/%s%s",	//quake2 sort of path.
		"progs/%s%s"
	};

	int i, e;

	COM_StripExtension(name, nicename);

	while((data = strchr(nicename, '*')))
	{
		*data = '#';
	}

	if ((len = GL_FindTexture(name))!=-1)	//don't bother if it already exists.
		return len;

	if ((len = GL_LoadCompressed(name)))
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
					COM_FileBase(cl.model_name[1], map);
				_snprintf(fname, sizeof(fname)-1, path[i], map, nicename, extensions[e]);
			}
			else
				_snprintf(fname, sizeof(fname)-1, path[i], nicename, extensions[e]);
			if ((buf = COM_LoadFile (fname, 5)))
			{
				if ((data = Read32BitImageFile(buf, com_filesize, &image_width, &image_height)))
				{
					BoostGamma(data, image_width, image_height);
					len = GL_LoadTexture32 (name, image_width, image_height, (unsigned*)data, mipmap, alpha);
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
		return GL_LoadTexture32 (name, image_width, image_height, (unsigned*)data, mipmap, alphaed);
	return 0;	
}
int Mod_LoadReplacementTexture(char *name, qboolean mipmap, qboolean alpha)
{
	if (!gl_load24bit.value)
		return 0;
	return Mod_LoadHiResTexture(name, mipmap, alpha, true);
}

int Mod_LoadBumpmapTexture(char *name)
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
		"override/%s%s",
		"pics/%s%s",	//quake2 sort of path.
		"progs/%s%s"
	};

	int i, e;

	COM_StripExtension(name, nicename);

	if ((len = GL_FindTexture(name))!=-1)	//don't bother if it already exists.
		return len;

	if ((len = GL_LoadCompressed(name)))
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
					COM_FileBase(cl.model_name[1], map);
				_snprintf(fname, sizeof(fname)-1, path[i], map, nicename, extensions[e]);
			}
			else
				_snprintf(fname, sizeof(fname)-1, path[i], nicename, extensions[e]);
			if ((buf = COM_LoadFile (fname, 5)))
			{
				if ((data = ReadTargaFile(buf, com_filesize, &image_width, &image_height, 2)))	//Only load a greyscale image.
				{
					len = GL_LoadTexture8Bump(name, image_width, image_height, data, true);
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
