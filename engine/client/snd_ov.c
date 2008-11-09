#include "quakedef.h"

#ifdef AVAIL_OGGVORBIS

#ifdef __MORPHOS__
	#include <exec/exec.h>
	#include <libraries/vorbisfile.h>

	#include <proto/exec.h>
	#include <proto/vorbisfile.h>
#else
	#include <vorbis/vorbisfile.h>
#endif


#if defined(__MORPHOS__)

	#define oggvorbislibrary VorbisFileBase
	struct Library *VorbisFileBase;

#elif defined(_WIN32)
	#define WINDOWSDYNAMICLINK

	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>

	HINSTANCE oggvorbislibrary;
#else
	#include <dlfcn.h>
	void *oggvorbislibrary;
#endif

#ifdef __MORPHOS__
	#define p_ov_open_callbacks(a, b, c, d, e) ov_open_callbacks(a, b, c, d, &e)
	#define p_ov_clear ov_clear
	#define p_ov_info ov_info
	#define p_ov_comment ov_comment
	#define p_ov_pcm_total ov_pcm_total
	#define p_ov_read ov_read
#else
	int (VARGS *p_ov_open_callbacks) (void *datasource, OggVorbis_File *vf, char *initial, long ibytes, ov_callbacks callbacks);
	int (VARGS *p_ov_clear)(OggVorbis_File *vf);
	vorbis_info *(VARGS *p_ov_info)(OggVorbis_File *vf,int link);
	vorbis_comment *(VARGS *p_ov_comment) (OggVorbis_File *vf,int link);
	ogg_int64_t (VARGS *p_ov_pcm_total)(OggVorbis_File *vf,int i);
	long (VARGS *p_ov_read)(OggVorbis_File *vf,char *buffer,int length,
				int bigendianp,int word,int sgned,int *bitstream);
#endif


typedef struct {
	unsigned char *start;	//file positions
	unsigned long length;
	unsigned long pos;
	int srcspeed;

	qboolean failed;

	sfxcache_t	mediasc;
	char *mediaaswavdata;	
	int mediaaswavpos;
	int mediaaswavbuflen;
	char *mediatemprecode;
	int mediatemprecodelen;

	OggVorbis_File vf;

	sfx_t *s;
} ovdecoderbuffer_t;

int OV_DecodeSome(sfx_t *s, int minlength);
void OV_CancelDecoder(sfx_t *s);
qboolean OV_StartDecode(unsigned char *start, unsigned long length, ovdecoderbuffer_t *buffer);

qbyte *COM_LoadFile (char *path, int usehunk);

sfxcache_t *S_LoadOVSound (sfx_t *s, qbyte *data, int datalen, int sndspeed)
{
	//char	namebuffer[MAX_OSPATH]; //unreferenced
	char	*name;
	ovdecoderbuffer_t *buffer;
	//FILE *f; //unreferenced

	//qboolean telluser; //unreferenced
	//int len; //unreferenced

	if (datalen < 4 || strncmp(data, "OggS", 4))
		return NULL;

	name = s->name;

	if (!s->decoder)
		s->decoder = Z_Malloc(sizeof(ovdecoderbuffer_t) + sizeof(sfxdecode_t));
	buffer = (ovdecoderbuffer_t*)(s->decoder+1);

	buffer->mediaaswavpos=0;
	buffer->mediasc.length=0;
	buffer->s = s;
	s->decoder->buf = buffer;
	s->decoder->decodemore = OV_DecodeSome;
	s->decoder->abort = OV_CancelDecoder;

	if (!OV_StartDecode(data, datalen, buffer))
	{
		if (buffer->mediaaswavdata)
		{
			BZ_Free(buffer->mediaaswavdata);
			buffer->mediaaswavdata = NULL;
		}
		if (buffer->mediatemprecode)
		{
			BZ_Free(buffer->mediatemprecode);
			buffer->mediatemprecode = NULL;
		}
		Z_Free(s->decoder);
		s->decoder=NULL;
		s->failedload = true;
		return NULL;
	}

	buffer->mediaaswavpos = sizeof(sfxcache_t);

	s->decoder->decodemore(s, 100);

	s->cache.fake=true;
	return buffer->s->cache.data;
}

int OV_DecodeSome(sfx_t *s, int minlength)
{	
	extern int snd_speed;
	extern cvar_t snd_linearresample_stream;
	int bigendianp = bigendian;
	int current_section = 0;
	sfxcache_t *sc;

	ovdecoderbuffer_t *dec = s->decoder->buf;
	int bytesread;

//	Con_Printf("Minlength = %03i   ", minlength);

	for (;;)
	{
		if (dec->mediaaswavbuflen < dec->mediaaswavpos+minlength+11050)	//expand if needed.
		{
	//		Con_Printf("Expand buffer\n");
			dec->mediaaswavbuflen += minlength+22100;
			dec->mediaaswavdata = BZ_Realloc(dec->mediaaswavdata, dec->mediaaswavbuflen);
			s->cache.data = dec->mediaaswavdata;
			s->cache.fake = true;

			sc = s->cache.data;
			sc->numchannels = dec->mediasc.numchannels;
			sc->loopstart = -1;
		}
		else
			sc = s->cache.data;

		if (minlength < sc->length)
		{
	//		Con_Printf("No need for decode\n");
			//done enough for now, don't whizz through the lot
			return 0;
		}

		if (snd_speed == dec->srcspeed)
		{
			bytesread = p_ov_read(&dec->vf, dec->mediaaswavdata+dec->mediaaswavpos, dec->mediaaswavbuflen-dec->mediaaswavpos, bigendianp, 2, 1, &current_section);
			if (bytesread <= 0)
			{
				if (bytesread != 0)	//0==eof
				{
					Con_Printf("ogg decoding failed\n");
					return 1;
				}
				return 0;
			}
		}
		else
		{
			double scale = dec->srcspeed / (double)snd_speed;
			int decodesize = ceil((dec->mediaaswavbuflen-dec->mediaaswavpos) * scale);
			if (decodesize > dec->mediatemprecodelen)
			{
				dec->mediatemprecode = BZ_Realloc(dec->mediatemprecode, decodesize);
				dec->mediatemprecodelen = decodesize;
			}

			bytesread = p_ov_read(&dec->vf, dec->mediatemprecode, decodesize, bigendianp, 2, 1, &current_section);

			if (bytesread <= 0)
			{
				if (bytesread != 0)	//0==eof
				{
					Con_Printf("ogg decoding failed\n");
					return 1;
				}
				return 0;
			}

			SND_ResampleStream(dec->mediatemprecode, 
				dec->srcspeed, 
				2, 
				dec->mediasc.numchannels, 
				bytesread / (2 * dec->mediasc.numchannels),
				dec->mediaaswavdata+dec->mediaaswavpos,
				snd_speed,
				2,
				dec->mediasc.numchannels,
				(int)snd_linearresample_stream.value);

			bytesread = (int)floor(bytesread / scale) & ~0x1;
		}

		dec->mediaaswavpos += bytesread;

		sc->length = (dec->mediaaswavpos-sizeof(sfxcache_t))/(2*(dec->mediasc.numchannels));
		dec->mediasc.length = sc->length;

		if (minlength<=sc->length)
		{
//			Con_Printf("Reached length\n");
			return 1;
		}
	}
}
void OV_CancelDecoder(sfx_t *s)
{
	sfxcache_t *src, *dest;
	ovdecoderbuffer_t *dec;

	dec = s->decoder->buf;
	p_ov_clear (&dec->vf);	//close the decoder

	//copy to new buffer

	src = s->cache.data;
	s->cache.fake = false;
	s->cache.data = NULL;
	dest = Cache_Alloc(&s->cache, dec->mediaaswavpos, s->name);
	memcpy(dest, src, dec->mediaaswavpos);
	BZ_Free(src);

	if (dec->mediatemprecode)
	{
		BZ_Free(dec->mediatemprecode);
		dec->mediatemprecode = NULL;
	}

	Z_Free(s->decoder);
	s->decoder = NULL;

	//and it's now indistinguisable from a wav
}

static size_t read_func (void *ptr, size_t size, size_t nmemb, void *datasource)
{	
	ovdecoderbuffer_t *buffer = datasource;
	int spare = buffer->length - buffer->pos;

	if (size*nmemb > spare)
		nmemb = spare / size;
	memcpy(ptr, &buffer->start[buffer->pos], size*nmemb);
	buffer->pos += size*nmemb;
	return nmemb;
}

static int seek_func (void *datasource, ogg_int64_t offset, int whence)
{	
	ovdecoderbuffer_t *buffer = datasource;
	switch(whence)
	{
	case SEEK_SET:
		buffer->pos = offset;
		break;
	case SEEK_END:
		buffer->pos = buffer->length+offset;
		break;
	case SEEK_CUR:
		buffer->pos+=offset;
		break;
	}	
	return 0;
}

static int close_func (void *datasource)
{
	ovdecoderbuffer_t *buffer = datasource;
	BZ_Free(buffer->start);
	buffer->start=0;
	return 0;
}

static long tell_func (void *datasource)
{
	ovdecoderbuffer_t *buffer = datasource;
	return buffer->pos;
}
static ov_callbacks callbacks = {
	read_func,
	seek_func,
	close_func,
	tell_func,
};
qboolean OV_StartDecode(unsigned char *start, unsigned long length, ovdecoderbuffer_t *buffer)
{
	static qboolean tried;
	if (!oggvorbislibrary && !tried)
#if defined(__MORPHOS__)
	{
		VorbisFileBase = OpenLibrary("vorbisfile.library", 2);
		if (!VorbisFileBase)
		{
			Con_Printf("Unable to open vorbisfile.library version 2\n");
		}
	}
#elif defined(WINDOWSDYNAMICLINK)
	{
		oggvorbislibrary = LoadLibrary("vorbisfile.dll");
		if (!oggvorbislibrary)
			oggvorbislibrary = LoadLibrary("libvorbisfile.dll");
		if (!oggvorbislibrary)
		{
			Con_Printf("Couldn't load DLL: \"vorbisfile.dll\".\n");
		}
		else
		{
			p_ov_open_callbacks	= (void *)GetProcAddress(oggvorbislibrary, "ov_open_callbacks");
			p_ov_comment		= (void *)GetProcAddress(oggvorbislibrary, "ov_comment");
			p_ov_pcm_total		= (void *)GetProcAddress(oggvorbislibrary, "ov_pcm_total");
			p_ov_clear			= (void *)GetProcAddress(oggvorbislibrary, "ov_clear");
			p_ov_info			= (void *)GetProcAddress(oggvorbislibrary, "ov_info");
			p_ov_read			= (void *)GetProcAddress(oggvorbislibrary, "ov_read");
		}
	}
#else
	{
		oggvorbislibrary = dlopen("libvorbisfile.so", RTLD_LOCAL | RTLD_LAZY);
		if (!oggvorbislibrary)
		{
			Con_Printf("Couldn't load SO: \"libvorbisfile.so\".\n");
		}
		else
		{
			p_ov_open_callbacks	= (void *)dlsym(oggvorbislibrary, "ov_open_callbacks");
			p_ov_comment		= (void *)dlsym(oggvorbislibrary, "ov_comment");
			p_ov_pcm_total		= (void *)dlsym(oggvorbislibrary, "ov_pcm_total");
			p_ov_clear		= (void *)dlsym(oggvorbislibrary, "ov_clear");
			p_ov_info		= (void *)dlsym(oggvorbislibrary, "ov_info");
			p_ov_read		= (void *)dlsym(oggvorbislibrary, "ov_read");
		}
	}
#endif

	tried = true;

	if (!oggvorbislibrary)
	{
		Con_Printf("ogg vorbis library is not loaded.\n");
		return false;
	}

	buffer->start = start;
	buffer->length = length;
	buffer->pos = 0;
	if (p_ov_open_callbacks(buffer, &buffer->vf, NULL, 0, callbacks))
	{
		Con_Printf("Input does not appear to be an Ogg bitstream.\n");
		return false;
	}

  /* Print the comments plus a few lines about the bitstream we're
     decoding */
  {
//    char **ptr=p_ov_comment(&buffer->vf,-1)->user_comments;
    vorbis_info *vi=p_ov_info(&buffer->vf,-1);

	if (vi->channels < 1 || vi->channels > 2)
	{
		p_ov_clear (&buffer->vf);
		return false;
	}

	buffer->mediasc.numchannels = vi->channels;
	buffer->mediasc.loopstart = -1;
	buffer->srcspeed = vi->rate;
/*
    while(*ptr){
      Con_Printf("%s\n",*ptr);
      ptr++;
    }
    Con_Printf("\nBitstream is %d channel, %ldHz\n",vi->channels,vi->rate);
    Con_Printf("\nDecoded length: %ld samples\n",
	    (long)p_ov_pcm_total(&buffer->vf,-1));
    Con_Printf("Encoded by: %s\n\n",p_ov_comment(&buffer->vf,-1)->vendor);
*/  }
	buffer->mediatemprecode = NULL;
	buffer->mediatemprecodelen = 0;

	buffer->start = BZ_Malloc(length);
	memcpy(buffer->start, start, length);

	return true;
}
#endif

