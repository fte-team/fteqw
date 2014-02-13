#include "quakedef.h"

#ifdef AVAIL_OGGVORBIS
#define OV_EXCLUDE_STATIC_CALLBACKS

#ifdef __MORPHOS__
	#include <exec/exec.h>
	#include <libraries/vorbisfile.h>

	#include <proto/exec.h>
	#include <proto/vorbisfile.h>
#else
	#include <vorbis/vorbisfile.h>
#endif


#ifdef LIBVORBISFILE_STATIC
	#define p_ov_open_callbacks ov_open_callbacks
	#define p_ov_clear ov_clear
	#define p_ov_info ov_info
	#define p_ov_comment ov_comment
	#define p_ov_pcm_total ov_pcm_total
	#define p_ov_read ov_read
	#define p_ov_pcm_seek ov_pcm_seek
#else
	#if defined(__MORPHOS__)

		#define oggvorbislibrary VorbisFileBase
		struct Library *VorbisFileBase;

	#else
		dllhandle_t *oggvorbislibrary;
	#endif

	#ifdef __MORPHOS__
		#define p_ov_open_callbacks(a, b, c, d, e) ov_open_callbacks(a, b, c, d, &e)
		#define p_ov_clear ov_clear
		#define p_ov_info ov_info
		#define p_ov_comment ov_comment
		#define p_ov_pcm_total ov_pcm_total
		#define p_ov_read ov_read
		#define p_ov_pcm_seek ov_pcm_seek
	#else
		int (VARGS *p_ov_open_callbacks) (void *datasource, OggVorbis_File *vf, char *initial, long ibytes, ov_callbacks callbacks);
		int (VARGS *p_ov_clear)(OggVorbis_File *vf);
		vorbis_info *(VARGS *p_ov_info)(OggVorbis_File *vf,int link);
		vorbis_comment *(VARGS *p_ov_comment) (OggVorbis_File *vf,int link);
		ogg_int64_t (VARGS *p_ov_pcm_total)(OggVorbis_File *vf,int i);
		long (VARGS *p_ov_read)(OggVorbis_File *vf,char *buffer,int length,
					int bigendianp,int word,int sgned,int *bitstream);
		int (VARGS *p_ov_pcm_seek)(OggVorbis_File *vf,ogg_int64_t pos);
	#endif
#endif


typedef struct {
	unsigned char *start;	//file positions
	unsigned long length;
	unsigned long pos;
	int srcspeed;
	int srcchannels;

	qboolean failed;

	char *tempbuffer;
	int tempbufferbytes;

	char *decodedbuffer;
	int decodedbufferbytes;
	int decodedbytestart;
	int decodedbytecount;

	OggVorbis_File vf;

	sfx_t *s;
} ovdecoderbuffer_t;

sfxcache_t *OV_DecodeSome(struct sfx_s *sfx, struct sfxcache_s *buf, int start, int length);
void OV_CancelDecoder(sfx_t *s);
qboolean OV_StartDecode(unsigned char *start, unsigned long length, ovdecoderbuffer_t *buffer);

qboolean S_LoadOVSound (sfx_t *s, qbyte *data, int datalen, int sndspeed)
{
	ovdecoderbuffer_t *buffer;

	if (datalen < 4 || strncmp(data, "OggS", 4))
		return false;

	buffer = Z_Malloc(sizeof(ovdecoderbuffer_t));

	buffer->decodedbytestart = 0;
	buffer->decodedbytecount = 0;
	buffer->s = s;
	s->decoder.buf = buffer;
	s->decoder.decodedata = OV_DecodeSome;
	s->decoder.abort = OV_CancelDecoder;

	if (!OV_StartDecode(data, datalen, buffer))
	{
		if (buffer->decodedbuffer)
		{
			BZ_Free(buffer->decodedbuffer);
			buffer->decodedbuffer = NULL;
		}
		buffer->decodedbufferbytes = 0;
		buffer->decodedbytestart = 0;
		buffer->decodedbytecount = 0;
		Z_Free(s->decoder.buf);
		s->decoder.buf = NULL;
		s->failedload = true;
		return false;
	}

	s->decoder.decodedata(s, NULL, 0, 100);

	return true;
}

sfxcache_t *OV_DecodeSome(struct sfx_s *sfx, struct sfxcache_s *buf, int start, int length)
{
	extern int snd_speed;
	extern cvar_t snd_linearresample_stream;
	int bigendianp = bigendian;
	int current_section = 0;

	ovdecoderbuffer_t *dec = sfx->decoder.buf;
	int bytesread;

	int outspeed = snd_speed;

//	Con_Printf("Minlength = %03i   ", minlength);

	start *= 2*dec->srcchannels;
	length *= 2*dec->srcchannels;

	if (start < dec->decodedbytestart)
	{
		/*something rewound, purge clear the buffer*/
		dec->decodedbytecount = 0;
		dec->decodedbytestart = start;

		//check pos
		p_ov_pcm_seek(&dec->vf, dec->decodedbytestart);
	}

	if (dec->decodedbytecount > snd_speed*8)
	{
		/*everything is okay, but our buffer is getting needlessly large.
		keep anything after the 'new' position, but discard all before that
		trim shouldn't be able to go negative
		*/
		int trim = start - dec->decodedbytestart;
		if (trim < 0)
		{
			dec->decodedbytecount = 0;
			dec->decodedbytestart = start;
		}
		else
		{
			//FIXME: retain an extra half-second for dual+ sound devices running slightly out of sync
			memmove(dec->decodedbuffer, dec->decodedbuffer + trim, dec->decodedbytecount - trim);
			dec->decodedbytecount -= trim;
			dec->decodedbytestart += trim;
		}
	}

	for (;;)
	{
		if (start+length <= dec->decodedbytestart + dec->decodedbytecount)
			break;

		if (dec->decodedbufferbytes < start+length - dec->decodedbytestart + 128)	//expand if needed.
		{
	//		Con_Printf("Expand buffer\n");
			dec->decodedbufferbytes = (start+length - dec->decodedbytestart) + snd_speed;
			dec->decodedbuffer = BZ_Realloc(dec->decodedbuffer, dec->decodedbufferbytes);
		}

		if (outspeed == dec->srcspeed)
		{
			bytesread = p_ov_read(&dec->vf, dec->decodedbuffer+dec->decodedbytecount, (start+length) - (dec->decodedbytestart+dec->decodedbytecount), bigendianp, 2, 1, &current_section);
			if (bytesread <= 0)
			{
				if (bytesread != 0)	//0==eof
				{
					Con_Printf("ogg decoding failed\n");
					return NULL;
				}
				break;
			}
		}
		else
		{
			double scale = dec->srcspeed / (double)outspeed;
			int decodesize = ceil((dec->decodedbufferbytes-dec->decodedbytecount) * scale);
			/*round down...*/
			decodesize &= ~(2 * dec->srcchannels - 1);
			if (decodesize > dec->tempbufferbytes)
			{
				dec->tempbuffer = BZ_Realloc(dec->tempbuffer, decodesize);
				dec->tempbufferbytes = decodesize;
			}

			bytesread = p_ov_read(&dec->vf, dec->tempbuffer, decodesize, bigendianp, 2, 1, &current_section);

			if (bytesread <= 0)
			{
				if (bytesread != 0)	//0==eof
				{
					Con_Printf("ogg decoding failed\n");
					return NULL;
				}
				return NULL;
			}

			SND_ResampleStream(dec->tempbuffer,
				dec->srcspeed,
				2,
				dec->srcchannels,
				bytesread / (2 * dec->srcchannels),
				dec->decodedbuffer+dec->decodedbytecount,
				outspeed,
				2,
				dec->srcchannels,
				snd_linearresample_stream.ival);

			bytesread = (int)floor(bytesread / scale) & ~0x1;
		}

		dec->decodedbytecount += bytesread;
	}

	if (buf)
	{
		buf->data = dec->decodedbuffer;
		buf->soundoffset = dec->decodedbytestart / (2 * dec->srcchannels);
		buf->length = dec->decodedbytecount;
		buf->loopstart = -1;
		buf->numchannels = dec->srcchannels;
		buf->speed = snd_speed;
		buf->width = 2;
	}
	return buf;
}
void OV_CancelDecoder(sfx_t *s)
{
	ovdecoderbuffer_t *dec;

	dec = s->decoder.buf;
	s->decoder.buf = NULL;
	s->decoder.abort = NULL;
	s->decoder.decodedata = NULL;
	p_ov_clear (&dec->vf);	//close the decoder

	if (dec->tempbuffer)
	{
		BZ_Free(dec->tempbuffer);
		dec->tempbufferbytes = 0;
	}

	BZ_Free(dec->decodedbuffer);
	dec->decodedbuffer = NULL;

	BZ_Free(dec);
}

static size_t VARGS read_func (void *ptr, size_t size, size_t nmemb, void *datasource)
{
	ovdecoderbuffer_t *buffer = datasource;
	int spare = buffer->length - buffer->pos;

	if (size*nmemb > spare)
		nmemb = spare / size;
	memcpy(ptr, &buffer->start[buffer->pos], size*nmemb);
	buffer->pos += size*nmemb;
	return nmemb;
}

static int VARGS seek_func (void *datasource, ogg_int64_t offset, int whence)
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

static int VARGS close_func (void *datasource)
{
	ovdecoderbuffer_t *buffer = datasource;
	BZ_Free(buffer->start);
	buffer->start=0;
	return 0;
}

static long VARGS tell_func (void *datasource)
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
#ifndef LIBVORBISFILE_STATIC
	static qboolean tried;
#ifndef __MORPHOS__
	static dllfunction_t funcs[] =
	{
		{(void*)&p_ov_open_callbacks, "ov_open_callbacks"},
		{(void*)&p_ov_comment, "ov_comment"},
		{(void*)&p_ov_pcm_total, "ov_pcm_total"},
		{(void*)&p_ov_clear, "ov_clear"},
		{(void*)&p_ov_info, "ov_info"},
		{(void*)&p_ov_read, "ov_read"},
		{(void*)&p_ov_pcm_seek, "ov_pcm_seek"},
		{NULL}
	};
#endif

	if (!oggvorbislibrary && !tried)
#if defined(__MORPHOS__)
	{
		VorbisFileBase = OpenLibrary("vorbisfile.library", 2);
		if (!VorbisFileBase)
		{
			Con_Printf("Unable to open vorbisfile.library version 2\n");
		}
	}
#elif defined(_WIN32)
	{
		oggvorbislibrary = Sys_LoadLibrary("vorbisfile", funcs);
		if (!oggvorbislibrary)
			oggvorbislibrary = Sys_LoadLibrary("libvorbisfile", funcs);

		if (!oggvorbislibrary)
		{
			oggvorbislibrary = Sys_LoadLibrary("libvorbisfile-3", funcs);
			if (!oggvorbislibrary)
				oggvorbislibrary = Sys_LoadLibrary("libvorbisfile", funcs);
		}

		if (!oggvorbislibrary)
			Con_Printf("Couldn't load DLL: \"vorbisfile.dll\" or \"libvorbisfile-3\".\n");
	}
#else
	{
		oggvorbislibrary = Sys_LoadLibrary("libvorbisfile", funcs);
		if (!oggvorbislibrary)
			Con_Printf("Couldn't load library: \"libvorbisfile\".\n");
	}
#endif

	tried = true;

	if (!oggvorbislibrary)
	{
		Con_Printf("ogg vorbis library is not loaded.\n");
		return false;
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

	buffer->srcchannels = vi->channels;
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
	buffer->tempbuffer = NULL;
	buffer->tempbufferbytes = 0;

	buffer->start = BZ_Malloc(length);
	memcpy(buffer->start, start, length);

	return true;
}
#endif

