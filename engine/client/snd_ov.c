#include "quakedef.h"

#ifdef AVAIL_OGGVORBIS

#include <vorbis/vorbisfile.h>



#if defined(_WIN32)
#define WINDOWSDYNAMICLINK
#include <windows.h>
HINSTANCE oggvorbislibrary;
#else
#include <dlfcn.h>
void *oggvorbislibrary;
#endif

int (*p_ov_open_callbacks) (void *datasource, OggVorbis_File *vf, char *initial, long ibytes, ov_callbacks callbacks);
int (*p_ov_clear)(OggVorbis_File *vf);
vorbis_info *(*p_ov_info)(OggVorbis_File *vf,int link);
vorbis_comment *(*p_ov_comment) (OggVorbis_File *vf,int link);
ogg_int64_t (*p_ov_pcm_total)(OggVorbis_File *vf,int i);
long (*p_ov_read)(OggVorbis_File *vf,char *buffer,int length,
		    int bigendianp,int word,int sgned,int *bitstream);


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

	OggVorbis_File vf;

	sfx_t *s;
} ovdecoderbuffer_t;

int OV_DecodeSome(sfx_t *s, int minlength);
void OV_CancelDecoder(sfx_t *s);
qboolean OV_StartDecode(unsigned char *start, unsigned long length, ovdecoderbuffer_t *buffer);

qbyte *COM_LoadFile (char *path, int usehunk);

sfxcache_t *S_LoadOVSound (sfx_t *s)
{
	char	namebuffer[MAX_OSPATH];
	char	*name;
	ovdecoderbuffer_t *buffer;
	FILE *f;

	char *data;
	qboolean telluser;
	int len;

	name = s->name;

	if (name[0] == '#')
		strcpy(namebuffer, &name[1]);
	else
		sprintf (namebuffer, "sound/%s", name);

	len = strlen(namebuffer);
	telluser = strcmp(namebuffer+len-4, ".wav");
	if (!telluser)
		strcpy(namebuffer+len-4, ".ogg");

	//try opening from a quake path
	data = COM_LoadMallocFile(namebuffer);
	if (!data)
	{//if that didn't work, try opening direct from exe - this is media after all.		

		if (!telluser)
			return NULL;	//never go out of the quake path for a wav replacement.
#ifndef _WIN32
		char unixname[128];
		if (name[1] == ':' && name[2] == '\\')	//convert from windows to a suitable alternative.
		{			
			sprintf(unixname, "/mnt/%c/%s", name[0]-'A'+'a', name+3);
			name = unixname;
			while (*name)
			{
				if (*name == '\\')
					*name = '/';
				name++;
			}			
			name = unixname;			
		}
#endif
		if ((f = fopen(name, "rb")))
		{
			com_filesize = COM_filelength(f);
			data = BZ_Malloc(com_filesize);
			fread(data, 1, com_filesize, f);
			fseek(f, 0, SEEK_SET);
			fclose(f);
			f = NULL;
		}
		else
		{
			Con_SafePrintf ("Couldn't load %s\n", namebuffer);
			return NULL;
		}
	}

	if (!s->decoder)
		s->decoder = Z_Malloc(sizeof(ovdecoderbuffer_t) + sizeof(sfxdecode_t));
	buffer = (ovdecoderbuffer_t*)(s->decoder+1);

	buffer->mediaaswavpos=0;
	buffer->mediasc.length=0;
	buffer->s = s;
	s->decoder->buf = buffer;
	s->decoder->decodemore = OV_DecodeSome;
	s->decoder->abort = OV_CancelDecoder;

	if (!OV_StartDecode(data, com_filesize, buffer))
	{
		if (buffer->mediaaswavdata)
		{
			BZ_Free(buffer->mediaaswavdata);

			buffer->mediaaswavdata=NULL;
		}
		Z_Free(s->decoder);
		s->decoder=NULL;
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
	int i;
	int bigendianp = 0;
	int current_section = 0;
	sfxcache_t *sc;

	ovdecoderbuffer_t *dec = s->decoder->buf;
	int bytesread;

	Con_Printf("Minlength = %03i   ", minlength);

	if (dec->mediaaswavbuflen < dec->mediaaswavpos+minlength+11050)	//expand if needed.
	{
		Con_Printf("Expand buffer\n");
		dec->mediaaswavbuflen += minlength+22100;
		dec->mediaaswavdata = BZ_Realloc(dec->mediaaswavdata, dec->mediaaswavbuflen);
		s->cache.data = dec->mediaaswavdata;
		s->cache.fake = true;

		sc = s->cache.data;
		sc->stereo = dec->mediasc.stereo;
		sc->loopstart = -1;
	}
	else
		sc = s->cache.data;

	if (minlength < sc->length)
	{
		Con_Printf("No need for decode\n");
		//done enough for now, don't whizz through the lot
		return 0;
	}

	for (;;)
	{
		bytesread = p_ov_read(&dec->vf, dec->mediaaswavdata+dec->mediaaswavpos, dec->mediaaswavbuflen-dec->mediaaswavpos, bigendianp, 2, 1, &current_section);
		if (bytesread <= 0)
		{
			Con_Printf("ogg decoding failed\n");
			return 0;
		}

		if (snd_speed != dec->srcspeed)
		{	//resample
			if (dec->mediasc.stereo)
			{
				int *data = (int*)(dec->mediaaswavdata+dec->mediaaswavpos);
				float frac = (float)dec->srcspeed/snd_speed;
				bytesread = (int)(bytesread/frac);
				for(i = 0; i < bytesread/4; i++)
					data[i] = data[(int)(i*frac)];
			}
			else
			{
			}
		}

		dec->mediaaswavpos += bytesread;
		sc->length = (dec->mediaaswavpos-sizeof(sfxcache_t))/(2*(dec->mediasc.stereo+1));
		dec->mediasc.length = sc->length;

		if (minlength<=sc->length)
		{
			Con_Printf("Reached length\n");
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
#ifdef WINDOWSDYNAMICLINK
	{
		tried = true;
		oggvorbislibrary = LoadLibrary("vorbisfile.dll");
		if (!oggvorbislibrary)
		{
			Con_Printf("Couldn't load DLL: \"vorbisfile.dll\".\n");
			return false;
		}
		p_ov_open_callbacks	= (void *)GetProcAddress(oggvorbislibrary, "ov_open_callbacks");
		p_ov_comment		= (void *)GetProcAddress(oggvorbislibrary, "ov_comment");
		p_ov_pcm_total		= (void *)GetProcAddress(oggvorbislibrary, "ov_pcm_total");
		p_ov_clear			= (void *)GetProcAddress(oggvorbislibrary, "ov_clear");
		p_ov_info			= (void *)GetProcAddress(oggvorbislibrary, "ov_info");
		p_ov_read			= (void *)GetProcAddress(oggvorbislibrary, "ov_read");
	}
#else
	{
		tried = true;
		oggvorbislibrary = dlopen("libvorbisfile.so", RTLD_LOCAL | RTLD_LAZY);
		if (!oggvorbislibrary)
		{
			Con_Printf("Couldn't load DLL: \"vorbisfile.dll\".\n");
			return false;
		}
		p_ov_open_callbacks	= (void *)dlsym(oggvorbislibrary, "ov_open_callbacks");
		p_ov_comment		= (void *)dlsym(oggvorbislibrary, "ov_comment");
		p_ov_pcm_total		= (void *)dlsym(oggvorbislibrary, "ov_pcm_total");
		p_ov_clear		= (void *)dlsym(oggvorbislibrary, "ov_clear");
		p_ov_info		= (void *)dlsym(oggvorbislibrary, "ov_info");
		p_ov_read		= (void *)dlsym(oggvorbislibrary, "ov_read");
	}
#endif

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

	buffer->mediasc.stereo = vi->channels-1;
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
	return true;
}
#endif

