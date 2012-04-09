//mp3 menu and track selector.
//was origonally an mp3 track selector, now handles lots of media specific stuff - like q3 films!
//should rename to m_media.c
#include "quakedef.h"
#ifdef GLQUAKE
#include "glquake.h"//fixme
#endif
#include "shader.h"

#if !defined(NOMEDIA)


#include "winquake.h"
#ifdef _WIN32
#define WINAMP
#endif

#ifdef WINAMP

#include "winamp.h"
HWND hwnd_winamp;

#endif

qboolean Media_EvaluateNextTrack(void);

typedef struct mediatrack_s{
	char filename[128];
	char nicename[128];
	int length;
	struct mediatrack_s *next;
} mediatrack_t;

static mediatrack_t currenttrack;
int lasttrackplayed;

int media_playing=true;//try to continue from the standard playlist
cvar_t media_shuffle = SCVAR("media_shuffle", "1");
cvar_t media_repeat = SCVAR("media_repeat", "1");
#ifdef WINAMP
cvar_t media_hijackwinamp = SCVAR("media_hijackwinamp", "0");
#endif

int selectedoption=-1;
int numtracks;
int nexttrack=-1;
mediatrack_t *tracks;

char media_iofilename[MAX_OSPATH]="";

int loadedtracknames;

#ifdef WINAMP
qboolean WinAmp_GetHandle (void)
{
	if ((hwnd_winamp = FindWindow("Winamp", NULL)))
		return true;
	if ((hwnd_winamp = FindWindow("Winamp v1.x", NULL)))
		return true;

	*currenttrack.nicename = '\0';

	return false;
}

qboolean WinAmp_StartTune(char *name)
{
	int trys;
	int pos;
	COPYDATASTRUCT cds;
	if (!WinAmp_GetHandle())
		return false;

	//FIXME: extract from fs if it's in a pack.
	//FIXME: always give absolute path
	cds.dwData = IPC_PLAYFILE;
	cds.lpData = (void *) name;
	cds.cbData = strlen((char *) cds.lpData)+1; // include space for null char
	SendMessage(hwnd_winamp,WM_WA_IPC,0,IPC_DELETE);
	SendMessage(hwnd_winamp,WM_COPYDATA,(WPARAM)NULL,(LPARAM)&cds);
	SendMessage(hwnd_winamp,WM_WA_IPC,(WPARAM)0,IPC_STARTPLAY );

	for (trys = 1000; trys; trys--)
	{
		pos = SendMessage(hwnd_winamp,WM_WA_IPC,0,IPC_GETOUTPUTTIME);
		if (pos>100 && SendMessage(hwnd_winamp,WM_WA_IPC,0,IPC_GETOUTPUTTIME)>=0)	//tune has started
			break;

		Sleep(10);	//give it a chance.
		if (!WinAmp_GetHandle())
			break;
	}

	return true;
}

void WinAmp_Think(void)
{
	int pos;
	int len;

	if (!WinAmp_GetHandle())
		return;

	pos = bgmvolume.value*255;
	if (pos > 255) pos = 255;
	if (pos < 0) pos = 0;
	PostMessage(hwnd_winamp, WM_WA_IPC,pos,IPC_SETVOLUME);

//optimise this to reduce calls?
	pos = SendMessage(hwnd_winamp,WM_WA_IPC,0,IPC_GETOUTPUTTIME);
	len = SendMessage(hwnd_winamp,WM_WA_IPC,1,IPC_GETOUTPUTTIME)*1000;

	if ((pos > len || pos <= 100) && len != -1)
	if (Media_EvaluateNextTrack())
		WinAmp_StartTune(currenttrack.filename);
}
#endif
void Media_Seek (float time)
{
#ifdef WINAMP
	if (media_hijackwinamp.value)
	{
		int pos;
		if (WinAmp_GetHandle())
		{
			pos = SendMessage(hwnd_winamp,WM_WA_IPC,0,IPC_GETOUTPUTTIME);
			pos += time*1000;
			PostMessage(hwnd_winamp,WM_WA_IPC,pos,IPC_JUMPTOTIME);

			WinAmp_Think();
		}
	}
#endif
	S_Music_Seek(time);
}

void Media_FForward_f(void)
{
	float time = atoi(Cmd_Argv(1));
	if (!time)
		time = 15;
	Media_Seek(time);
}
void Media_Rewind_f (void)
{
	float time = atoi(Cmd_Argv(1));
	if (!time)
		time = 15;
	Media_Seek(-time);
}

qboolean Media_EvaluateNextTrack(void)
{
	mediatrack_t *track;
	int trnum;
	if (!tracks)
		return false;
	if (nexttrack>=0)
	{
		trnum = nexttrack;
		for (track = tracks; track; track=track->next)
		{
			if (!trnum)
			{
				memcpy(&currenttrack, track->filename, sizeof(mediatrack_t));
				lasttrackplayed = nexttrack;
				break;
			}
			trnum--;
		}
		nexttrack = -1;
	}
	else
	{
		if (media_shuffle.value)
			nexttrack=((float)(rand()&0x7fff)/0x7fff)*numtracks;
		else
		{
			nexttrack = lasttrackplayed+1;
			if (nexttrack >= numtracks)
			{
				if (media_repeat.value)
					nexttrack = 0;
				else
				{
					*currenttrack.filename='\0';
					*currenttrack.nicename='\0';
					nexttrack = -1;
					media_playing = false;
					return false;
				}
			}
		}
		trnum = nexttrack;
		for (track = tracks; track; track=track->next)
		{
			if (!trnum)
			{
				memcpy(&currenttrack, track->filename, sizeof(mediatrack_t));
				lasttrackplayed = nexttrack;
				break;
			}
			trnum--;
		}
		nexttrack = -1;
	}

	return true;
}

//flushes music channel on all soundcards, and the tracks that arn't decoded yet.
void Media_Clear (void)
{
	S_Music_Clear(NULL);
}

qboolean fakecdactive;
qboolean Media_FakeTrack(int i, qboolean loop)
{
	char trackname[512];
	qboolean found;

	if (i > 0 && i <= 999)
	{
		found = false;
		if (!found)
		{
			sprintf(trackname, "sound/cdtracks/track%03i.ogg", i);
			found = COM_FCheckExists(trackname);
		}
		if (!found)
		{
			sprintf(trackname, "sound/cdtracks/track%03i.mp3", i);
			found = COM_FCheckExists(trackname);
		}
		if (!found)
		{
			sprintf(trackname, "sound/cdtracks/track%03i.wav", i);
			found = COM_FCheckExists(trackname);
		}
		if (found)
		{
			Media_Clear();
			strcpy(currenttrack.filename, trackname+6);

			fakecdactive = true;
			media_playing = true;
			return true;
		}
	}

	fakecdactive = false;
	return false;
}

//actually, this func just flushes and states that it should be playing. the ambientsound func actually changes the track.
void Media_Next_f (void)
{
	Media_Clear();
	media_playing=true;

#ifdef WINAMP
	if (media_hijackwinamp.value)
	{
		if (WinAmp_GetHandle())
		if (Media_EvaluateNextTrack())
			WinAmp_StartTune(currenttrack.filename);
	}
#endif
}








void M_Menu_Media_f (void) {
	key_dest = key_menu;
	m_state = m_media;
}

void Media_LoadTrackNames (char *listname);

#define MEDIA_MIN	-8
#define MEDIA_VOLUME -8
#define MEDIA_REWIND -7
#define MEDIA_FASTFORWARD -6
#define MEDIA_CLEARLIST -5
#define MEDIA_ADDTRACK -4
#define MEDIA_ADDLIST -3
#define MEDIA_SHUFFLE -2
#define MEDIA_REPEAT -1

void M_Media_Draw (void)
{
	mpic_t	*p;
	mediatrack_t *track;
	int y;
	int op, i;

#define MP_Hightlight(x,y,text,hl) (hl?M_PrintWhite(x, y, text):M_Print(x, y, text))

	p = R2D_SafeCachePic ("gfx/p_option.lmp");
	if (p)
		M_DrawScalePic ( (320-p->width)/2, 4, 144, 24, p);
	if (!bgmvolume.value)
		M_Print (12, 32, "Not playing - no volume");
	else if (!*currenttrack.nicename)
	{
		if (!tracks)
			M_Print (12, 32, "Not playing - no track to play");
		else
		{
#ifdef WINAMP
			if (!WinAmp_GetHandle())
				M_Print (12, 32, "Please start WinAmp 2");
			else
#endif
				M_Print (12, 32, "Not playing - switched off");
		}
	}
	else
	{
		M_Print (12, 32, "Currently playing:");
		M_Print (12, 40, currenttrack.nicename);
	}

	op = selectedoption - (vid.height-52)/16;
	if (op + (vid.height-52)/8>numtracks)
		op = numtracks - (vid.height-52)/8;
	if (op < MEDIA_MIN)
		op = MEDIA_MIN;
	y=52;
	while(op < 0)
	{
		switch(op)
		{
		case MEDIA_VOLUME:
			MP_Hightlight (12, y, "Volume", op == selectedoption);
			y+=8;
			break;
		case MEDIA_CLEARLIST:
			MP_Hightlight (12, y, "Clear all", op == selectedoption);
			y+=8;
			break;
		case MEDIA_FASTFORWARD:
			MP_Hightlight (12, y, ">> Fast Forward", op == selectedoption);
			y+=8;
			break;
		case MEDIA_REWIND:
			MP_Hightlight (12, y, "<< Rewind", op == selectedoption);
			y+=8;
			break;
		case MEDIA_ADDTRACK:
			MP_Hightlight (12, y, "Add Track", op == selectedoption);
			if (op == selectedoption)
				M_PrintWhite (12+9*8, y, media_iofilename);
			y+=8;
			break;
		case MEDIA_ADDLIST:
			MP_Hightlight (12, y, "Add List", op == selectedoption);
			if (op == selectedoption)
				M_PrintWhite (12+9*8, y, media_iofilename);
			y+=8;
			break;
		case MEDIA_SHUFFLE:
			if (media_shuffle.value)
				MP_Hightlight (12, y, "Shuffle on", op == selectedoption);
			else
				MP_Hightlight (12, y, "Shuffle off", op == selectedoption);
			y+=8;
			break;
		case MEDIA_REPEAT:
			if (media_shuffle.value)
			{
				if (media_repeat.value)
					MP_Hightlight (12, y, "Repeat on", op == selectedoption);
				else
					MP_Hightlight (12, y, "Repeat off", op == selectedoption);
			}
			else
			{
				if (media_repeat.value)
					MP_Hightlight (12, y, "(Repeat on)", op == selectedoption);
				else
					MP_Hightlight (12, y, "(Repeat off)", op == selectedoption);
			}
			y+=8;
			break;
		}
		op++;
	}

	for (track = tracks, i=0; track && i<op; track=track->next, i++);
	for (; track; track=track->next, y+=8, op++)
	{
		if (op == selectedoption)
			M_PrintWhite (12, y, track->nicename);
		else
			M_Print (12, y, track->nicename);
	}
}

char compleatenamepath[MAX_OSPATH];
char compleatenamename[MAX_OSPATH];
qboolean compleatenamemultiple;
int Com_CompleatenameCallback(const char *name, int size, void *data)
{
	if (*compleatenamename)
		compleatenamemultiple = true;
	Q_strncpyz(compleatenamename, name, sizeof(compleatenamename));

	return true;
}
void Com_CompleateOSFileName(char *name)
{
	char *ending;
	compleatenamemultiple = false;

	strcpy(compleatenamepath, name);
	ending = COM_SkipPath(compleatenamepath);
	if (compleatenamepath!=ending)
		ending[-1] = '\0';	//strip a slash
	*compleatenamename='\0';

	Sys_EnumerateFiles(NULL, va("%s*", name), Com_CompleatenameCallback, NULL);
	Sys_EnumerateFiles(NULL, va("%s*.*", name), Com_CompleatenameCallback, NULL);

	if (*compleatenamename)
		strcpy(name, compleatenamename);
}

void M_Media_Key (int key)
{
	int dir;
	if (key == K_ESCAPE)
		M_Menu_Main_f();
	else if (key == K_RIGHTARROW || key == K_LEFTARROW)
	{
		if (key == K_RIGHTARROW)
			dir = 1;
		else dir = -1;
		switch(selectedoption)
		{
		case MEDIA_VOLUME:
			bgmvolume.value += dir * 0.1;
			if (bgmvolume.value < 0)
				bgmvolume.value = 0;
			if (bgmvolume.value > 1)
				bgmvolume.value = 1;
			Cvar_SetValue (&bgmvolume, bgmvolume.value);
			break;
		default:
			if (selectedoption >= 0)
				Media_Next_f();
			break;
		}
	}
	else if (key == K_DOWNARROW)
	{
		selectedoption++;
		if (selectedoption>=numtracks)
			selectedoption = numtracks-1;
	}
	else if (key == K_PGDN)
	{
		selectedoption+=10;
		if (selectedoption>=numtracks)
			selectedoption = numtracks-1;
	}
	else if (key == K_UPARROW)
	{
		selectedoption--;
		if (selectedoption < MEDIA_MIN)
			selectedoption = MEDIA_MIN;
	}
	else if (key == K_PGUP)
	{
		selectedoption-=10;
		if (selectedoption < MEDIA_MIN)
			selectedoption = MEDIA_MIN;
	}
	else if (key == K_DEL)
	{
		if (selectedoption>=0)
		{
			mediatrack_t *prevtrack=NULL, *tr;
			int num=0;
			tr=tracks;
			while(tr)
			{
				if (num == selectedoption)
				{
					if (prevtrack)
						prevtrack->next = tr->next;
					else
						tracks = tr->next;
					Z_Free(tr);
					numtracks--;
					break;
				}

				prevtrack = tr;
				tr=tr->next;
				num++;
			}
		}
	}
	else if (key == K_ENTER)
	{
		switch(selectedoption)
		{
		case MEDIA_FASTFORWARD:
			Media_Seek(15);
			break;
		case MEDIA_REWIND:
			Media_Seek(-15);
			break;
		case MEDIA_CLEARLIST:
			{
				mediatrack_t *prevtrack;
				while(tracks)
				{
					prevtrack = tracks;
					tracks=tracks->next;
					Z_Free(prevtrack);
					numtracks--;
				}
				if (numtracks!=0)
				{
					numtracks=0;
					Con_SafePrintf("numtracks should be 0\n");
				}
			}
			break;
		case MEDIA_ADDTRACK:
			if (*media_iofilename)
			{
				mediatrack_t *newtrack;
				newtrack = Z_Malloc(sizeof(mediatrack_t));
				Q_strncpyz(newtrack->filename, media_iofilename, sizeof(newtrack->filename));
				Q_strncpyz(newtrack->nicename, COM_SkipPath(media_iofilename), sizeof(newtrack->filename));
				newtrack->length = 0;
				newtrack->next = tracks;
				tracks = newtrack;
				numtracks++;
			}
			break;
		case MEDIA_ADDLIST:
			if (*media_iofilename)
				Media_LoadTrackNames(media_iofilename);
			break;
		case MEDIA_SHUFFLE:
			Cvar_Set(&media_shuffle, media_shuffle.value?"0":"1");
			break;
		case MEDIA_REPEAT:
			Cvar_Set(&media_repeat, media_repeat.value?"0":"1");
			break;
		default:
			if (selectedoption>=0)
			{
				media_playing = true;
				nexttrack = selectedoption;
				Media_Next_f();
			}
			break;
		}
	}
	else
	{
		if (selectedoption == MEDIA_ADDLIST || selectedoption == MEDIA_ADDTRACK)
		{
			if (key == K_TAB)
				Com_CompleateOSFileName(media_iofilename);
			else if (key == K_BACKSPACE)
			{
				dir = strlen(media_iofilename);
				if (dir)
					media_iofilename[dir-1] = '\0';
			}
			else if ((key >= 'a' && key <= 'z') || (key >= '0' && key <= '9') || key == '/' || key == '_' || key == '.' || key == ':')
			{
				dir = strlen(media_iofilename);
				media_iofilename[dir] = key;
				media_iofilename[dir+1] = '\0';
			}
		}
		else if (selectedoption>=0)
		{
			mediatrack_t *prevtrack, *tr;
			int num=0;
			tr=tracks;
			while(tr)
			{
				if (num == selectedoption)
					break;

				prevtrack = tr;
				tr=tr->next;
				num++;
			}
			if (!tr)
				return;

			if (key == K_BACKSPACE)
			{
				dir = strlen(tr->nicename);
				if (dir)
					tr->nicename[dir-1] = '\0';
			}
			else if ((key >= 'a' && key <= 'z') || (key >= '0' && key <= '9') || key == '/' || key == '_' || key == '.' || key == ':'  || key == '&' || key == '|' || key == '#' || key == '\'' || key == '\"' || key == '\\' || key == '*' || key == '@' || key == '!' || key == '(' || key == ')' || key == '%' || key == '^' || key == '?' || key == '[' || key == ']' || key == ';' || key == ':' || key == '+' || key == '-' || key == '=')
			{
				dir = strlen(tr->nicename);
				tr->nicename[dir] = key;
				tr->nicename[dir+1] = '\0';
			}
		}
	}
}





//safeprints only.
void Media_LoadTrackNames (char *listname)
{
	char *lineend;
	char *len;
	char *filename;
	char *trackname;
	mediatrack_t *newtrack;
	char *data = COM_LoadTempFile(listname);

	loadedtracknames=true;

	if (!data)
		return;

	if (!Q_strncasecmp(data, "#extm3u", 7))
	{
		data = strchr(data, '\n')+1;
		for(;;)
		{
			lineend = strchr(data, '\n');

			if (Q_strncasecmp(data, "#extinf:", 8))
			{
				if (!lineend)
					return;
				Con_SafePrintf("Bad m3u file\n");
				return;
			}
			len = data+8;
			trackname = strchr(data, ',')+1;
			if (!trackname)
				return;

			lineend[-1]='\0';

			filename = data = lineend+1;

			lineend = strchr(data, '\n');

			if (lineend)
			{
				lineend[-1]='\0';
				data = lineend+1;
			}

			newtrack = Z_Malloc(sizeof(mediatrack_t));
#ifndef _WIN32	//crossplatform - lcean up any dos names
			if (filename[1] == ':')
			{
				snprintf(newtrack->filename, sizeof(newtrack->filename)-1, "/mnt/%c/%s", filename[0]-'A'+'a', filename+3);
				while((filename = strchr(newtrack->filename, '\\')))
					*filename = '/';

			}
			else
#endif
				Q_strncpyz(newtrack->filename, filename, sizeof(newtrack->filename));
			Q_strncpyz(newtrack->nicename, trackname, sizeof(newtrack->filename));
			newtrack->length = atoi(len);
			newtrack->next = tracks;
			tracks = newtrack;
			numtracks++;

			if (!lineend)
				return;
		}
	}
	else
	{
		for(;;)
		{
			trackname = filename = data;
			lineend = strchr(data, '\n');

			if (!lineend && !*data)
				break;
			lineend[-1]='\0';
			data = lineend+1;

			newtrack = Z_Malloc(sizeof(mediatrack_t));
			Q_strncpyz(newtrack->filename, filename, sizeof(newtrack->filename));
			Q_strncpyz(newtrack->nicename, COM_SkipPath(trackname), sizeof(newtrack->filename));
			newtrack->length = 0;
			newtrack->next = tracks;
			tracks = newtrack;
			numtracks++;

			if (!lineend)
				break;
		}
	}
}

//safeprints only.
char *Media_NextTrack(int musicchannelnum)
{
#ifdef WINAMP
	if (media_hijackwinamp.value)
	{
		WinAmp_Think();
		return NULL;
	}
#endif
	if (bgmvolume.value <= 0 || !media_playing)
		return NULL;

	if (!loadedtracknames)
		Media_LoadTrackNames("sound/media.m3u");
	if (!tracks && !fakecdactive)
	{
		*currenttrack.filename='\0';
		*currenttrack.nicename='\0';
		lasttrackplayed=-1;
		media_playing = false;
		return NULL;
	}

//	if (cursndcard == sndcardinfo)	//figure out the next track (primary sound card, we could actually get different tracks on different cards (and unfortunatly do))
//	{
		Media_EvaluateNextTrack();
//	}
	return currenttrack.filename;
}







//Avi files are specific to windows. Bit of a bummer really.
#if defined(_WIN32)
#define WINAVI
#endif









#undef	dwFlags
#undef	lpFormat
#undef	lpData
#undef	cbData
#undef	lTime


#ifdef OFFSCREENGECKO
#include "offscreengecko/embedding.h"
#include "offscreengecko/browser.h"
#endif


///temporary residence for media handling
#include "roq.h"


#ifdef WINAVI
#undef CDECL	//windows is stupid at times.
#define CDECL __cdecl

#if defined(_MSC_VER) && (_MSC_VER < 1300)
#define DWORD_PTR DWORD
#endif

#if 0
#include <msacm.h>
#else
DECLARE_HANDLE(HACMSTREAM);
typedef HACMSTREAM *LPHACMSTREAM;
DECLARE_HANDLE(HACMDRIVER);
typedef struct {
	DWORD     cbStruct;
	DWORD     fdwStatus;
	DWORD_PTR dwUser;
	LPBYTE    pbSrc;
	DWORD     cbSrcLength;
	DWORD     cbSrcLengthUsed;
	DWORD_PTR dwSrcUser;
	LPBYTE    pbDst;
	DWORD     cbDstLength;
	DWORD     cbDstLengthUsed;
	DWORD_PTR dwDstUser;
	DWORD     dwReservedDriver[10];
} ACMSTREAMHEADER, *LPACMSTREAMHEADER;
#define ACM_STREAMCONVERTF_BLOCKALIGN   0x00000004
#endif

//mingw workarounds
#define LPWAVEFILTER void *
#include <objbase.h>

MMRESULT (WINAPI *qacmStreamUnprepareHeader) (HACMSTREAM has, LPACMSTREAMHEADER pash, DWORD fdwUnprepare);
MMRESULT (WINAPI *qacmStreamConvert) (HACMSTREAM has, LPACMSTREAMHEADER pash, DWORD fdwConvert);
MMRESULT (WINAPI *qacmStreamPrepareHeader) (HACMSTREAM has, LPACMSTREAMHEADER pash, DWORD fdwPrepare);
MMRESULT (WINAPI *qacmStreamOpen) (LPHACMSTREAM phas, HACMDRIVER had, LPWAVEFORMATEX pwfxSrc, LPWAVEFORMATEX pwfxDst, LPWAVEFILTER pwfltr, DWORD_PTR dwCallback, DWORD_PTR dwInstance, DWORD fdwOpen);

static qboolean qacmStartup(void)
{
	static int inited;
	static dllhandle_t *module;
	if (!inited)
	{
		dllfunction_t funcs[] =
		{
			{(void*)&qacmStreamUnprepareHeader,	"acmStreamUnprepareHeader"},
			{(void*)&qacmStreamConvert,			"acmStreamConvert"},
			{(void*)&qacmStreamPrepareHeader,	"acmStreamPrepareHeader"},
			{(void*)&qacmStreamOpen,			"acmStreamOpen"},
			{NULL,NULL}
		};
		inited = true;
		module = Sys_LoadLibrary("msacm32.dll", funcs);
	}

	return module?true:false;
}

#if 0
#include <vfw.h>
#else
typedef struct 
{
	DWORD fccType;
	DWORD fccHandler;
	DWORD dwFlags;
	DWORD dwCaps;
	WORD  wPriority;
	WORD  wLanguage;
	DWORD dwScale;
	DWORD dwRate;
	DWORD dwStart;
	DWORD dwLength;
	DWORD dwInitialFrames;
	DWORD dwSuggestedBufferSize;
	DWORD dwQuality;
	DWORD dwSampleSize;
	RECT  rcFrame;
	DWORD dwEditCount;
	DWORD dwFormatChangeCount;
	TCHAR szName[64];
} AVISTREAMINFOA, *LPAVISTREAMINFOA;
typedef struct AVISTREAM *PAVISTREAM;
typedef struct AVIFILE *PAVIFILE;
typedef struct GETFRAME *PGETFRAME;
typedef struct	 
{
	DWORD  fccType;
	DWORD  fccHandler;
	DWORD  dwKeyFrameEvery;
	DWORD  dwQuality;
	DWORD  dwBytesPerSecond;
	DWORD  dwFlags;
	LPVOID lpFormat;
	DWORD  cbFormat;
	LPVOID lpParms;
	DWORD  cbParms;
	DWORD  dwInterleaveEvery;
} AVICOMPRESSOPTIONS;
#define streamtypeVIDEO         mmioFOURCC('v', 'i', 'd', 's')
#define streamtypeAUDIO         mmioFOURCC('a', 'u', 'd', 's')
#define AVISTREAMREAD_CONVENIENT	(-1L)
#define AVIIF_KEYFRAME	0x00000010L
#endif

ULONG	(WINAPI *qAVIStreamRelease)			(PAVISTREAM pavi);
HRESULT	(WINAPI *qAVIStreamEndStreaming)	(PAVISTREAM pavi);
HRESULT	(WINAPI *qAVIStreamGetFrameClose)	(PGETFRAME pg);
HRESULT	(WINAPI *qAVIStreamRead)			(PAVISTREAM pavi, LONG lStart, LONG lSamples, LPVOID lpBuffer, LONG cbBuffer, LONG FAR * plBytes, LONG FAR * plSamples);
LPVOID	(WINAPI *qAVIStreamGetFrame)		(PGETFRAME pg, LONG lPos);
HRESULT	(WINAPI *qAVIStreamReadFormat)		(PAVISTREAM pavi, LONG lPos,LPVOID lpFormat,LONG FAR *lpcbFormat);
LONG	(WINAPI *qAVIStreamStart)			(PAVISTREAM pavi);
PGETFRAME(WINAPI*qAVIStreamGetFrameOpen)	(PAVISTREAM pavi, LPBITMAPINFOHEADER lpbiWanted);
HRESULT	(WINAPI *qAVIStreamBeginStreaming)	(PAVISTREAM pavi, LONG lStart, LONG lEnd, LONG lRate);
LONG	(WINAPI *qAVIStreamSampleToTime)	(PAVISTREAM pavi, LONG lSample);
LONG	(WINAPI *qAVIStreamLength)			(PAVISTREAM pavi);
HRESULT	(WINAPI *qAVIStreamInfoA)			(PAVISTREAM pavi, LPAVISTREAMINFOA psi, LONG lSize);
ULONG	(WINAPI *qAVIFileRelease)			(PAVIFILE pfile);
HRESULT	(WINAPI *qAVIFileGetStream)			(PAVIFILE pfile, PAVISTREAM FAR * ppavi, DWORD fccType, LONG lParam);
HRESULT	(WINAPI *qAVIFileOpenA)				(PAVIFILE FAR *ppfile, LPCSTR szFile, UINT uMode, LPCLSID lpHandler);
void	(WINAPI *qAVIFileInit)				(void);
HRESULT	(WINAPI *qAVIStreamWrite)			(PAVISTREAM pavi, LONG lStart, LONG lSamples, LPVOID lpBuffer, LONG cbBuffer, DWORD dwFlags, LONG FAR *plSampWritten, LONG FAR *plBytesWritten);
HRESULT	(WINAPI *qAVIStreamSetFormat)		(PAVISTREAM pavi, LONG lPos,LPVOID lpFormat,LONG cbFormat);
HRESULT	(WINAPI *qAVIMakeCompressedStream)	(PAVISTREAM FAR * ppsCompressed, PAVISTREAM ppsSource, AVICOMPRESSOPTIONS FAR * lpOptions, CLSID FAR *pclsidHandler);
HRESULT	(WINAPI *qAVIFileCreateStreamA)		(PAVIFILE pfile, PAVISTREAM FAR *ppavi, AVISTREAMINFOA FAR * psi);

static qboolean qAVIStartup(void)
{
	static int aviinited;
	static dllhandle_t *avimodule;
	if (!aviinited)
	{
		dllfunction_t funcs[] =
		{
			{(void*)&qAVIFileInit,				"AVIFileInit"},
			{(void*)&qAVIStreamRelease,			"AVIStreamRelease"},
			{(void*)&qAVIStreamEndStreaming,	"AVIStreamEndStreaming"},
			{(void*)&qAVIStreamGetFrameClose,	"AVIStreamGetFrameClose"},
			{(void*)&qAVIStreamRead,			"AVIStreamRead"},
			{(void*)&qAVIStreamGetFrame,		"AVIStreamGetFrame"},
			{(void*)&qAVIStreamReadFormat,		"AVIStreamReadFormat"},
			{(void*)&qAVIStreamStart,			"AVIStreamStart"},
			{(void*)&qAVIStreamGetFrameOpen,	"AVIStreamGetFrameOpen"},
			{(void*)&qAVIStreamBeginStreaming,	"AVIStreamBeginStreaming"},
			{(void*)&qAVIStreamSampleToTime,	"AVIStreamSampleToTime"},
			{(void*)&qAVIStreamLength,			"AVIStreamLength"},
			{(void*)&qAVIStreamInfoA,			"AVIStreamInfoA"},
			{(void*)&qAVIFileRelease,			"AVIFileRelease"},
			{(void*)&qAVIFileGetStream,			"AVIFileGetStream"},
			{(void*)&qAVIFileOpenA,				"AVIFileOpenA"},
			{(void*)&qAVIStreamWrite,			"AVIStreamWrite"},
			{(void*)&qAVIStreamSetFormat,		"AVIStreamSetFormat"},
			{(void*)&qAVIMakeCompressedStream,	"AVIMakeCompressedStream"},
			{(void*)&qAVIFileCreateStreamA,		"AVIFileCreateStreamA"},
			{NULL,NULL}
		};
		aviinited = true;
		avimodule = Sys_LoadLibrary("avifil32.dll", funcs);

		if (avimodule)
			qAVIFileInit();
	}

	return avimodule?true:false;
}
#endif

#define MFT_CAPTURE 5 //fixme

typedef enum {
	MFT_NONE,
	MFT_STATIC,	//non-moving, PCX, no sound
	MFT_ROQ,
	MFT_AVI,
	MFT_CIN,
	MFT_OFSGECKO
} media_filmtype_t;

struct cin_s {

	//these are the outputs (not always power of two!)
	enum uploadfmt outtype;
	int outwidth;
	int outheight;
	qbyte *outdata;
	qbyte *outpalette;
	int outunchanged;
	qboolean ended;

	texid_t texture;

	qboolean (*decodeframe)(cin_t *cin, qboolean nosound);
	void (*doneframe)(cin_t *cin);
	void (*shutdown)(cin_t *cin);	//warning: doesn't free cin_t
	void (*rewind)(cin_t *cin);
	//these are any interactivity functions you might want...
	void (*cursormove) (struct cin_s *cin, float posx, float posy);	//pos is 0-1
	void (*key) (struct cin_s *cin, int code, int unicode, int event);
	qboolean (*setsize) (struct cin_s *cin, int width, int height);
	void (*getsize) (struct cin_s *cin, int *width, int *height);
	void (*changestream) (struct cin_s *cin, char *streamname);


	//

	media_filmtype_t filmtype;

#ifdef WINAVI
	struct {
		qboolean resettimer;
		AVISTREAMINFOA		vidinfo;
		PAVISTREAM			pavivideo;
		AVISTREAMINFOA		audinfo;
		PAVISTREAM			pavisound;
		PAVIFILE			pavi;
		PGETFRAME			pgf;

		HACMSTREAM			audiodecoder;

		LPWAVEFORMATEX pWaveFormat;

		//sound stuff
		int soundpos;

		//source info
		float filmfps;
		int num_frames;
	} avi;
#endif

#ifdef OFFSCREENGECKO
	struct {
		OSGK_Browser *gbrowser;
		int bwidth;
		int bheight;
	} gecko;
#endif

	struct {
		qbyte *filmimage;	//rgba
		int imagewidth;
		int imageheight;
	} image;

	struct {
		roq_info *roqfilm;
	} roq;

	struct {
		struct cinematics_s *cin;
	} q2cin;

	float filmstarttime;
	float nextframetime;
	float filmlasttime;

	int currentframe;	//last frame in buffer
	qbyte *framedata;	//Z_Malloced buffer
};

shader_t *videoshader;

//////////////////////////////////////////////////////////////////////////////////
//AVI Support (windows)
#ifdef WINAVI
void Media_WINAVI_Shutdown(struct cin_s *cin)
{
	qAVIStreamGetFrameClose(cin->avi.pgf);
	qAVIStreamEndStreaming(cin->avi.pavivideo);
	qAVIStreamRelease(cin->avi.pavivideo);
	//we don't need to free the file (we freed it immediatly after getting the stream handles)
}
qboolean Media_WinAvi_DecodeFrame(cin_t *cin, qboolean nosound)
{
	LPBITMAPINFOHEADER lpbi;									// Holds The Bitmap Header Information
	float newframe;
	int newframei;
	int wantsoundtime;
	extern cvar_t _snd_mixahead;

	float curtime = Sys_DoubleTime();

	if (cin->avi.resettimer)
	{
		cin->filmstarttime = curtime;
		cin->avi.resettimer = 0;
		newframe = 0;
		newframei = newframe;
	}
	else
	{
		newframe = (((curtime - cin->filmstarttime) * cin->avi.vidinfo.dwRate) / cin->avi.vidinfo.dwScale) + cin->avi.vidinfo.dwInitialFrames;
		newframei = newframe;

		if (newframei>=cin->avi.num_frames)
			cin->ended = true;

		if (newframei == cin->currentframe)
		{
			cin->outunchanged = true;
			return true;
		}
	}
	cin->outunchanged = false;

	if (cin->currentframe < newframei-1)
		Con_DPrintf("Dropped %i frame(s)\n", (newframei - cin->currentframe)-1);

	cin->currentframe = newframei;

	if (newframei>=cin->avi.num_frames)
	{
		cin->filmstarttime = curtime;
		cin->currentframe = newframei = 0;
		cin->avi.soundpos = 0;
	}

	lpbi = (LPBITMAPINFOHEADER)qAVIStreamGetFrame(cin->avi.pgf, cin->currentframe);	// Grab Data From The AVI Stream
	if (!lpbi || lpbi->biBitCount != 24)//oops
	{
		cin->avi.resettimer = true;
		cin->ended = true;
		return false;
	}
	else
	{
		cin->outtype = TF_BGR24_FLIP;
		cin->outwidth = lpbi->biWidth;
		cin->outheight = lpbi->biHeight;
		cin->outdata = (char*)lpbi+lpbi->biSize;
	}

	if(nosound)
		wantsoundtime = 0;
	else
		wantsoundtime = ((((curtime - cin->filmstarttime) + _snd_mixahead.value + 0.02) * cin->avi.audinfo.dwRate) / cin->avi.audinfo.dwScale) + cin->avi.audinfo.dwInitialFrames;

	while (cin->avi.pavisound && cin->avi.soundpos < wantsoundtime)
	{
		LONG lSize;
		LPBYTE pBuffer;
		LONG samples;

		/*if the audio skipped more than a second, drop it all and start at a sane time, so our raw audio playing code doesn't buffer too much*/
		if (cin->avi.soundpos + (1*cin->avi.audinfo.dwRate / cin->avi.audinfo.dwScale) < wantsoundtime)
		{
			cin->avi.soundpos = wantsoundtime;
			break;
		}

		qAVIStreamRead(cin->avi.pavisound, cin->avi.soundpos, AVISTREAMREAD_CONVENIENT, NULL, 0, &lSize, &samples);
		pBuffer = cin->framedata;
		qAVIStreamRead(cin->avi.pavisound, cin->avi.soundpos, AVISTREAMREAD_CONVENIENT, pBuffer, lSize, NULL, &samples);

		cin->avi.soundpos+=samples;

		/*if no progress, stop!*/
		if (!samples)
			break;

		if (cin->avi.audiodecoder)
		{
			ACMSTREAMHEADER strhdr;
			char buffer[1024*256];

			memset(&strhdr, 0, sizeof(strhdr));
			strhdr.cbStruct = sizeof(strhdr);
			strhdr.pbSrc = pBuffer;
			strhdr.cbSrcLength = lSize;
			strhdr.pbDst = buffer;
			strhdr.cbDstLength = sizeof(buffer);

			qacmStreamPrepareHeader(cin->avi.audiodecoder, &strhdr, 0);
			qacmStreamConvert(cin->avi.audiodecoder, &strhdr, ACM_STREAMCONVERTF_BLOCKALIGN);
			qacmStreamUnprepareHeader(cin->avi.audiodecoder, &strhdr, 0);

			S_RawAudio(-1, strhdr.pbDst, cin->avi.pWaveFormat->nSamplesPerSec, strhdr.cbDstLengthUsed/4, cin->avi.pWaveFormat->nChannels, 2);
		}
		else
			S_RawAudio(-1, pBuffer, cin->avi.pWaveFormat->nSamplesPerSec, samples, cin->avi.pWaveFormat->nChannels, 2);
	}
	return true;
}

cin_t *Media_WinAvi_TryLoad(char *name)
{
	cin_t *cin;
	PAVIFILE			pavi;
	flocation_t loc;

	if (!qAVIStartup())
		return NULL;


	FS_FLocateFile(name, FSLFRT_DEPTH_OSONLY, &loc);

	if (!loc.offset && !qAVIFileOpenA(&pavi, loc.rawname, OF_READ, NULL))//!AVIStreamOpenFromFile(&pavi, name, streamtypeVIDEO, 0, OF_READ, NULL))
	{
		int filmwidth;
		int filmheight;

		cin = Z_Malloc(sizeof(cin_t));
		cin->filmtype = MFT_AVI;
		cin->avi.pavi = pavi;

		if (qAVIFileGetStream(cin->avi.pavi, &cin->avi.pavivideo, streamtypeVIDEO, 0))	//retrieve video stream
		{
			qAVIFileRelease(pavi);
			Con_Printf("%s contains no video stream\n", name);
			return NULL;
		}
		if (qAVIFileGetStream(cin->avi.pavi, &cin->avi.pavisound, streamtypeAUDIO, 0))	//retrieve audio stream
		{
			Con_DPrintf("%s contains no audio stream\n", name);
			cin->avi.pavisound=NULL;
		}
		qAVIFileRelease(cin->avi.pavi);

//play with video
		qAVIStreamInfoA(cin->avi.pavivideo, &cin->avi.vidinfo, sizeof(cin->avi.vidinfo));
		filmwidth=cin->avi.vidinfo.rcFrame.right-cin->avi.vidinfo.rcFrame.left;					// Width Is Right Side Of Frame Minus Left
		filmheight=cin->avi.vidinfo.rcFrame.bottom-cin->avi.vidinfo.rcFrame.top;					// Height Is Bottom Of Frame Minus Top
		cin->framedata = BZ_Malloc(filmwidth*filmheight*4);

		cin->avi.num_frames=qAVIStreamLength(cin->avi.pavivideo);							// The Last Frame Of The Stream
		cin->avi.filmfps=1000.0f*(float)cin->avi.num_frames/(float)qAVIStreamSampleToTime(cin->avi.pavivideo,cin->avi.num_frames);		// Calculate Rough Milliseconds Per Frame

		qAVIStreamBeginStreaming(cin->avi.pavivideo, 0, cin->avi.num_frames, 100);

		cin->avi.pgf=qAVIStreamGetFrameOpen(cin->avi.pavivideo, NULL);

		if (!cin->avi.pgf)
		{
			Con_Printf("AVIStreamGetFrameOpen failed. Please install a vfw codec for '%c%c%c%c'. Try ffdshow.\n", 
				((unsigned char*)&cin->avi.vidinfo.fccHandler)[0],
				((unsigned char*)&cin->avi.vidinfo.fccHandler)[1],
				((unsigned char*)&cin->avi.vidinfo.fccHandler)[2],
				((unsigned char*)&cin->avi.vidinfo.fccHandler)[3]
				);
		}

		cin->currentframe=0;
		cin->filmstarttime = Sys_DoubleTime();

		cin->avi.soundpos=0;
		cin->avi.resettimer = true;


//play with sound
		if (cin->avi.pavisound)
		{
			LONG lSize;
			LPBYTE pChunk;
			qAVIStreamInfoA(cin->avi.pavisound, &cin->avi.audinfo, sizeof(cin->avi.audinfo));

			qAVIStreamRead(cin->avi.pavisound, 0, AVISTREAMREAD_CONVENIENT, NULL, 0, &lSize, NULL);

			if (!lSize)
				cin->avi.pWaveFormat = NULL;
			else
			{
				pChunk = BZ_Malloc(sizeof(qbyte)*lSize);

				if(qAVIStreamReadFormat(cin->avi.pavisound, qAVIStreamStart(cin->avi.pavisound), pChunk, &lSize))
				{
				   // error
					Con_Printf("Failiure reading sound info\n");
				}
				cin->avi.pWaveFormat = (LPWAVEFORMATEX)pChunk;
			}

			if (!cin->avi.pWaveFormat)
			{
				Con_Printf("VFW is broken\n");
				qAVIStreamRelease(cin->avi.pavisound);
				cin->avi.pavisound=NULL;
			}
			else if (cin->avi.pWaveFormat->wFormatTag != 1)
			{
				WAVEFORMATEX pcm_format;
				HACMDRIVER drv = NULL;

				memset (&pcm_format, 0, sizeof(pcm_format));
				pcm_format.wFormatTag = WAVE_FORMAT_PCM;
				pcm_format.nChannels = cin->avi.pWaveFormat->nChannels;
				pcm_format.nSamplesPerSec = cin->avi.pWaveFormat->nSamplesPerSec;
				pcm_format.nBlockAlign = 4;
				pcm_format.nAvgBytesPerSec = pcm_format.nSamplesPerSec*4;
				pcm_format.wBitsPerSample = 16;
				pcm_format.cbSize = 0;

				if (!qacmStartup() || 0!=qacmStreamOpen(&cin->avi.audiodecoder, drv, cin->avi.pWaveFormat, &pcm_format, NULL, 0, 0, 0))
				{
					Con_Printf("Failed to open audio decoder\n");	//FIXME: so that it no longer is...
					qAVIStreamRelease(cin->avi.pavisound);
					cin->avi.pavisound=NULL;
				}
			}
		}

		cin->filmtype = MFT_AVI;
		cin->decodeframe = Media_WinAvi_DecodeFrame;
		cin->shutdown = Media_WINAVI_Shutdown;
		return cin;
	}
	return NULL;
}

#else
cin_t *Media_WinAvi_TryLoad(char *name)
{
	return NULL;
}
#endif

//AVI Support (windows)
//////////////////////////////////////////////////////////////////////////////////
//Quake3 RoQ Support

#ifdef Q3CLIENT
void Media_Roq_Shutdown(struct cin_s *cin)
{
	roq_close(cin->roq.roqfilm);
	cin->roq.roqfilm=NULL;
}

qboolean Media_Roq_DecodeFrame (cin_t *cin, qboolean nosound)
{
	float curtime = Sys_DoubleTime();

	if ((int)(cin->filmlasttime*30) == (int)((float)realtime*30) && cin->outtype != TF_INVALID)
	{
		cin->outunchanged = !!cin->outtype;
		return true;
	}
	else if (curtime<cin->nextframetime || roq_read_frame(cin->roq.roqfilm)==1)	 //0 if end, -1 if error, 1 if success
	{
	//#define LIMIT(x) ((x)<0xFFFF)?(x)>>16:0xFF;
#define LIMIT(x) ((((x) > 0xffffff) ? 0xff0000 : (((x) <= 0xffff) ? 0 : (x) & 0xff0000)) >> 16)
		unsigned char *pa=cin->roq.roqfilm->y[0];
		unsigned char *pb=cin->roq.roqfilm->u[0];
		unsigned char *pc=cin->roq.roqfilm->v[0];
		int pix=0;
		int num_columns=(cin->roq.roqfilm->width)>>1;
		int num_rows=cin->roq.roqfilm->height;
		int y;
		int x;

		qbyte *framedata;

		cin->filmlasttime = (float)realtime;

		if (!(curtime<cin->nextframetime))	//roq file was read properly
		{
			cin->nextframetime += 1/30.0;	//add a little bit of extra speed so we cover up a little bit of glitchy sound... :o)

			if (cin->nextframetime < curtime)
				cin->nextframetime = curtime;

			framedata = cin->framedata;

			for(y = 0; y < num_rows; ++y)	//roq playing doesn't give nice data. It's still fairly raw.
			{										//convert it properly.
				for(x = 0; x < num_columns; ++x)
				{

					int r, g, b, y1, y2, u, v, t;
					y1 = *(pa++); y2 = *(pa++);
					u = pb[x] - 128;
					v = pc[x] - 128;

					y1 <<= 16;
					y2 <<= 16;
					r = 91881 * v;
					g = -22554 * u + -46802 * v;
					b = 116130 * u;

					t=r+y1;
					framedata[pix] =(unsigned char) LIMIT(t);
					t=g+y1;
					framedata[pix+1] =(unsigned char) LIMIT(t);
					t=b+y1;
					framedata[pix+2] =(unsigned char) LIMIT(t);

					t=r+y2;
					framedata[pix+4] =(unsigned char) LIMIT(t);
					t=g+y2;
					framedata[pix+5] =(unsigned char) LIMIT(t);
					t=b+y2;
					framedata[pix+6] =(unsigned char) LIMIT(t);
					pix+=8;

				}
				if(y & 0x01) { pb += num_columns; pc += num_columns; }
			}
		}

		cin->outunchanged = false;
		cin->outtype = TF_RGBA32;
		cin->outwidth = cin->roq.roqfilm->width;
		cin->outheight = cin->roq.roqfilm->height;
		cin->outdata = cin->framedata;

		if (!nosound)
		if (cin->roq.roqfilm->audio_channels && S_HaveOutput() && cin->roq.roqfilm->aud_pos < cin->roq.roqfilm->vid_pos)
		if (roq_read_audio(cin->roq.roqfilm)>0)
		{
/*				FILE *f;
			char wav[] = "\x52\x49\x46\x46\xea\x5f\x04\x00\x57\x41\x56\x45\x66\x6d\x74\x20\x12\x00\x00\x00\x01\x00\x02\x00\x22\x56\x00\x00\x88\x58\x01\x00\x04\x00\x10\x00\x00\x00\x66\x61\x63\x74\x04\x00\x00\x00\xee\x17\x01\x00\x64\x61\x74\x61\xb8\x5f\x04\x00";
			int size;

			f = fopen("d:/quake/id1/sound/raw.wav", "r+b");
			if (!f)
				f = fopen("d:/quake/id1/sound/raw.wav", "w+b");
			fseek(f, 0, SEEK_SET);
			fwrite(&wav, sizeof(wav), 1, f);
			fseek(f, 0, SEEK_END);
			fwrite(roqfilm->audio, roqfilm->audio_size, 2, f);
			size = ftell(f) - sizeof(wav);
			fseek(f, 54, SEEK_SET);
			fwrite(&size, sizeof(size), 1, f);
			fclose(f);
*/
			S_RawAudio(-1, cin->roq.roqfilm->audio, 22050, cin->roq.roqfilm->audio_size/cin->roq.roqfilm->audio_channels, cin->roq.roqfilm->audio_channels, 2);
		}

		return true;
	}
	else
	{
		cin->ended = true;
		cin->roq.roqfilm->frame_num = 0;
		cin->roq.roqfilm->aud_pos = cin->roq.roqfilm->roq_start;
		cin->roq.roqfilm->vid_pos = cin->roq.roqfilm->roq_start;
	}

	return false;
}

cin_t *Media_RoQ_TryLoad(char *name)
{
	cin_t *cin;
	roq_info *roqfilm;
	if ((roqfilm = roq_open(name)))
	{
		cin = Z_Malloc(sizeof(cin_t));
		cin->filmtype = MFT_ROQ;
		cin->decodeframe = Media_Roq_DecodeFrame;
		cin->shutdown = Media_Roq_Shutdown;

		cin->roq.roqfilm = roqfilm;
		cin->nextframetime = Sys_DoubleTime();

		cin->framedata = BZ_Malloc(roqfilm->width*roqfilm->height*4);
		return cin;
	}
	return NULL;
}
#endif

//Quake3 RoQ Support
//////////////////////////////////////////////////////////////////////////////////
//Static Image Support

#ifndef MINIMAL
void Media_Static_Shutdown(struct cin_s *cin)
{
	BZ_Free(cin->image.filmimage);
	cin->image.filmimage = NULL;
}

qboolean Media_Static_DecodeFrame(cin_t *cin, qboolean nosound)
{
	cin->outunchanged = cin->outtype==TF_RGBA32?true:false;//handy
	cin->outtype = TF_RGBA32;
	cin->outwidth = cin->image.imagewidth;
	cin->outheight = cin->image.imageheight;
	cin->outdata = cin->image.filmimage;
	return true;
}

cin_t *Media_Static_TryLoad(char *name)
{
	cin_t *cin;
	char *dot = strrchr(name, '.');

	if (dot && (!strcmp(dot, ".pcx") || !strcmp(dot, ".tga") || !strcmp(dot, ".png") || !strcmp(dot, ".jpg")))
	{
		qbyte *staticfilmimage;
		int imagewidth;
		int imageheight;
		qboolean hasalpha;

		int fsize;
		char fullname[MAX_QPATH];
		qbyte *file;

		Q_snprintfz(fullname, sizeof(fullname), "%s", name);
		fsize = FS_LoadFile(fullname, (void **)&file);
		if (!file)
		{
			Q_snprintfz(fullname, sizeof(fullname), "pics/%s", name);
			fsize = FS_LoadFile(fullname, (void **)&file);
			if (!file)
				return NULL;
		}

		if ((staticfilmimage = ReadPCXFile(file, fsize, &imagewidth, &imageheight)) ||	//convert to 32 rgba if not corrupt
			(staticfilmimage = ReadTargaFile(file, fsize, &imagewidth, &imageheight, &hasalpha, false)) ||
#ifdef AVAIL_JPEGLIB
			(staticfilmimage = ReadJPEGFile(file, fsize, &imagewidth, &imageheight)) ||
#endif
#ifdef AVAIL_PNGLIB
			(staticfilmimage = ReadPNGFile(file, fsize, &imagewidth, &imageheight, fullname)) ||
#endif
			0)
		{
			FS_FreeFile(file);	//got image data
		}
		else
		{
			FS_FreeFile(file);	//got image data
			Con_Printf("Static cinematic format not supported.\n");	//not supported format
			return NULL;
		}

		cin = Z_Malloc(sizeof(cin_t));
		cin->filmtype = MFT_STATIC;
		cin->decodeframe = Media_Static_DecodeFrame;
		cin->shutdown = Media_Static_Shutdown;

		cin->image.filmimage = staticfilmimage;
		cin->image.imagewidth = imagewidth;
		cin->image.imageheight = imageheight;

		return cin;
	}
	return NULL;
}
#endif

//Static Image Support
//////////////////////////////////////////////////////////////////////////////////
//Quake2 CIN Support

#ifdef Q2CLIENT
void Media_Cin_Shutdown(struct cin_s *cin)
{
	CIN_StopCinematic(cin->q2cin.cin);
}

qboolean Media_Cin_DecodeFrame(cin_t *cin, qboolean nosound)
{
	cin->outunchanged = cin->outdata!=NULL;
	switch (CIN_RunCinematic(cin->q2cin.cin, &cin->outdata, &cin->outwidth, &cin->outheight, &cin->outpalette))
	{
	default:
	case 0:
		cin->ended = true;
		return cin->outdata!=NULL;
	case 1:
		cin->outunchanged = false;
		return cin->outdata!=NULL;
	case 2:
		return cin->outdata!=NULL;
	}
}

cin_t *Media_Cin_TryLoad(char *name)
{
	struct cinematics_s *q2cin;
	cin_t *cin;
	char *dot = strrchr(name, '.');

	if (dot && (!strcmp(dot, ".cin")))
	{
		q2cin = CIN_PlayCinematic(name);
		if (q2cin)
		{
			cin = Z_Malloc(sizeof(cin_t));
			cin->q2cin.cin = q2cin;
			cin->filmtype = MFT_CIN;
			cin->decodeframe = Media_Cin_DecodeFrame;
			cin->shutdown = Media_Cin_Shutdown;

			cin->outtype = TF_8PAL24;
			return cin;
		}
	}

	return NULL;
}
#endif

//Quake2 CIN Support
//////////////////////////////////////////////////////////////////////////////////
//Gecko Support

#ifdef OFFSCREENGECKO

int (VARGS *posgk_release) (OSGK_BaseObject* obj);

OSGK_Browser* (VARGS *posgk_browser_create) (OSGK_Embedding* embedding, int width, int height);
void (VARGS *posgk_browser_resize) (OSGK_Browser* browser, int width, int height);
void (VARGS *posgk_browser_navigate) (OSGK_Browser* browser, const char* uri);
const unsigned char* (VARGS *posgk_browser_lock_data) (OSGK_Browser* browser, int* isDirty);
void (VARGS *posgk_browser_unlock_data) (OSGK_Browser* browser, const unsigned char* data);

void (VARGS *posgk_browser_event_mouse_move) (OSGK_Browser* browser, int x, int y);
void (VARGS *posgk_browser_event_mouse_button) (OSGK_Browser* browser, OSGK_MouseButton button, OSGK_MouseButtonEventType eventType);
int (VARGS *posgk_browser_event_key) (OSGK_Browser* browser, unsigned int key, OSGK_KeyboardEventType eventType);

OSGK_EmbeddingOptions* (VARGS *posgk_embedding_options_create) (void);
OSGK_Embedding* (VARGS *posgk_embedding_create2) (unsigned int apiVer, OSGK_EmbeddingOptions* options, OSGK_GeckoResult* geckoResult);
void (VARGS *posgk_embedding_options_set_profile_dir) (OSGK_EmbeddingOptions* options, const char* profileDir, const char* localProfileDir);
void (VARGS *posgk_embedding_options_add_search_path) (OSGK_EmbeddingOptions* options, const char* path);

dllhandle_t geckodll;
dllfunction_t gecko_functions[] =
{
	{(void**)&posgk_release, "osgk_release"},

	{(void**)&posgk_browser_create, "osgk_browser_create"},
	{(void**)&posgk_browser_resize, "osgk_browser_resize"},
	{(void**)&posgk_browser_navigate, "osgk_browser_navigate"},
	{(void**)&posgk_browser_lock_data, "osgk_browser_lock_data"},
	{(void**)&posgk_browser_unlock_data, "osgk_browser_unlock_data"},

	{(void**)&posgk_browser_event_mouse_move, "osgk_browser_event_mouse_move"},
	{(void**)&posgk_browser_event_mouse_move, "osgk_browser_event_mouse_move"},
	{(void**)&posgk_browser_event_mouse_button, "osgk_browser_event_mouse_button"},
	{(void**)&posgk_browser_event_key, "osgk_browser_event_key"},

	{(void**)&posgk_embedding_options_create, "osgk_embedding_options_create"},
	{(void**)&posgk_embedding_create2, "osgk_embedding_create2"},
	{(void**)&posgk_embedding_options_set_profile_dir, "osgk_embedding_options_set_profile_dir"},
	{(void**)&posgk_embedding_options_add_search_path, "osgk_embedding_options_add_search_path"},
	{NULL}
};
OSGK_Embedding *gecko_embedding;

void Media_Gecko_Shutdown(struct cin_s *cin)
{
	posgk_release(&cin->gecko.gbrowser->baseobj);
}

qboolean Media_Gecko_DecodeFrame(cin_t *cin, qboolean nosound)
{
	cin->outdata = (char*)posgk_browser_lock_data(cin->gecko.gbrowser, &cin->outunchanged);
	cin->outwidth = cin->gecko.bwidth;
	cin->outheight = cin->gecko.bheight;
	cin->outtype = TF_BGRA32;
	return !!cin->gecko.gbrowser;
}

void Media_Gecko_DoneFrame(cin_t *cin)
{
	posgk_browser_unlock_data(cin->gecko.gbrowser, cin->outdata);
	cin->outdata = NULL;
}

void Media_Gecko_MoveCursor (struct cin_s *cin, float posx, float posy)
{
	posgk_browser_event_mouse_move(cin->gecko.gbrowser, posx*cin->gecko.bwidth, posy*cin->gecko.bheight);
}

void Media_Gecko_KeyPress (struct cin_s *cin, int code, int unicode, int event)
{
	if (code >= K_MOUSE1 && code < K_MOUSE10)
	{
		posgk_browser_event_mouse_button(cin->gecko.gbrowser, code - K_MOUSE1, (event==3)?2:event);
	}
	else
	{
		switch(code)
		{
		case K_BACKSPACE:
			code = OSGKKey_Backspace;
			break;
		case K_TAB:
			code = OSGKKey_Tab;
			break;
		case K_ENTER:
			code = OSGKKey_Return;
			break;
		case K_SHIFT:
			code = OSGKKey_Shift;
			break;
		case K_CTRL:
			code = OSGKKey_Control;
			break;
		case K_ALT:
			code = OSGKKey_Alt;
			break;
		case K_CAPSLOCK:
			code = OSGKKey_CapsLock;
			break;
		case K_ESCAPE:
			code = OSGKKey_Escape;
			break;
		case K_SPACE:
			code = OSGKKey_Space;
			break;
		case K_PGUP:
			code = OSGKKey_PageUp;
			break;
		case K_PGDN:
			code = OSGKKey_PageDown;
			break;
		case K_END:
			code = OSGKKey_End;
			break;
		case K_HOME:
			code = OSGKKey_Home;
			break;
		case K_LEFTARROW:
			code = OSGKKey_Left;
			break;
		case K_UPARROW:
			code = OSGKKey_Up;
			break;
		case K_RIGHTARROW:
			code = OSGKKey_Right;
			break;
		case K_DOWNARROW:
			code = OSGKKey_Down;
			break;
		case K_INS:
			code = OSGKKey_Insert;
			break;
		case K_DEL:
			code = OSGKKey_Delete;
			break;
		case K_F1:
			code = OSGKKey_F1;
			break;
		case K_F2:
			code = OSGKKey_F2;
			break;
		case K_F3:
			code = OSGKKey_F3;
			break;
		case K_F4:
			code = OSGKKey_F4;
			break;
		case K_F5:
			code = OSGKKey_F5;
			break;
		case K_F6:
			code = OSGKKey_F6;
			break;
		case K_F7:
			code = OSGKKey_F7;
			break;
		case K_F8:
			code = OSGKKey_F8;
			break;
		case K_F9:
			code = OSGKKey_F9;
			break;
		case K_F10:
			code = OSGKKey_F10;
			break;
		case K_F11:
			code = OSGKKey_F11;
			break;
		case K_F12:
			code = OSGKKey_F12;
			break;
		case K_KP_NUMLOCK:
			code = OSGKKey_NumLock;
			break;
		case K_SCRLCK:
			code = OSGKKey_ScrollLock;
			break;
		case K_LWIN:
			code = OSGKKey_Meta;
			break;
		default:
			code = unicode;
			break;
		}
		posgk_browser_event_key(cin->gecko.gbrowser, code, kePress);
		//posgk_browser_event_key(cin->gecko.gbrowser, code, event);
	}
}

qboolean Media_Gecko_SetSize (struct cin_s *cin, int width, int height)
{
	if (width < 4 || height < 4)
		return false;

	posgk_browser_resize(cin->gecko.gbrowser, width, height);
	cin->gecko.bwidth = width;
	cin->gecko.bheight = height;
	return true;
}

void Media_Gecko_GetSize (struct cin_s *cin, int *width, int *height)
{
	*width = cin->gecko.bwidth;
	*height = cin->gecko.bheight;
}

void Media_Gecko_ChangeStream (struct cin_s *cin, char *streamname)
{
	posgk_browser_navigate(cin->gecko.gbrowser, streamname);
}

cin_t *Media_Gecko_TryLoad(char *name)
{
	char xulprofiledir[MAX_OSPATH];
	cin_t *cin;

	if (!strncmp(name, "http://", 7))
	{
		OSGK_GeckoResult result;

		OSGK_EmbeddingOptions *opts;

		if (!gecko_embedding)
		{
			geckodll = Sys_LoadLibrary("OffscreenGecko", gecko_functions);
			if (!geckodll)
			{
				Con_Printf("OffscreenGecko not installed\n");
				return NULL;
			}

			opts = posgk_embedding_options_create();
			if (!opts)
				return NULL;

			posgk_embedding_options_add_search_path(opts, "./xulrunner/");
			if (FS_NativePath("xulrunner_profile/", FS_ROOT, xulprofiledir, sizeof(xulprofiledir)))
				posgk_embedding_options_set_profile_dir(opts, xulprofiledir, 0);

			gecko_embedding = posgk_embedding_create2(OSGK_API_VERSION, opts, &result);
			posgk_release(&opts->baseobj);
			if (!gecko_embedding)
				return NULL;
		}

		cin = Z_Malloc(sizeof(cin_t));
		cin->filmtype = MFT_OFSGECKO;
		cin->decodeframe = Media_Gecko_DecodeFrame;
		cin->doneframe = Media_Gecko_DoneFrame;
		cin->shutdown = Media_Gecko_Shutdown;

		cin->cursormove = Media_Gecko_MoveCursor;
		cin->key = Media_Gecko_KeyPress;
		cin->setsize = Media_Gecko_SetSize;
		cin->getsize = Media_Gecko_GetSize;
		cin->changestream = Media_Gecko_ChangeStream;

		cin->gecko.bwidth = 1024;
		cin->gecko.bheight = 1024;

		cin->gecko.gbrowser = posgk_browser_create(gecko_embedding, cin->gecko.bwidth, cin->gecko.bheight);
		if (!cin->gecko.gbrowser)
		{
			Con_Printf("osgk_browser_create failed, your version of xulrunner is likely unsupported\n");
			Z_Free(cin);
			return NULL;
		}
		posgk_browser_navigate(cin->gecko.gbrowser, name);
		return cin;
	}
	return NULL;
}
#else
cin_t *Media_Gecko_TryLoad(char *name)
{
	return NULL;
}
#endif

//Gecko Support
//////////////////////////////////////////////////////////////////////////////////

qboolean Media_PlayingFullScreen(void)
{
	return videoshader!=NULL;
}

void Media_ShutdownCin(cin_t *cin)
{
	if (!cin)
		return;

	if (cin->shutdown)
		cin->shutdown(cin);

	if (TEXVALID(cin->texture))
		R_DestroyTexture(cin->texture);

	if (cin->framedata)
	{
		BZ_Free(cin->framedata);
		cin->framedata = NULL;
	}

	Z_Free(cin);
}

cin_t *Media_StartCin(char *name)
{
	cin_t *cin = NULL;

	if (!name || !*name)	//clear only.
		return NULL;

#ifdef OFFSCREENGECKO
	if (!cin)
		cin = Media_Gecko_TryLoad(name);
#endif

	if (!cin)
		cin = Media_Static_TryLoad(name);

#ifdef Q2CLIENT
	if (!cin)
		cin = Media_Cin_TryLoad(name);
#endif
#ifdef Q3CLIENT
	if (!cin)
		cin = Media_RoQ_TryLoad(name);
#endif
#ifdef WINAVI
	if (!cin)
		cin = Media_WinAvi_TryLoad(name);
#endif

	return cin;
}

qboolean Media_Playing(void)
{
	if (videoshader)
		return true;
	return false;
}

qboolean Media_PlayFilm(char *name)
{
	cin_t *cin;
	static char sname[MAX_QPATH];

	if (!qrenderer)
		return false;

	if (videoshader)
	{
		R_UnloadShader(videoshader);
		videoshader = NULL;
	}

	if (!*name)
	{
		if (cls.state == ca_active)
		{
			CL_SendClientCommand(true, "nextserver %i", cl.servercount);
		}
		S_RawAudio(0, NULL, 0, 0, 0, 0);
		videoshader = NULL;
	}
	else
	{
		snprintf(sname, sizeof(sname), "cinematic/%s", name);
		videoshader = R_RegisterCustom(sname, Shader_DefaultCinematic, sname+10);

		cin = R_ShaderGetCinematic(videoshader);
		if (cin)
		{
			cin->ended = false;
			if (cin->rewind)
				cin->rewind(cin);
		}
		else
		{
			R_UnloadShader(videoshader);
			videoshader = NULL;
		}
	}

//	Media_ShutdownCin(fullscreenvid);
//	fullscreenvid = Media_StartCin(name);

	if (videoshader)
	{
		CDAudio_Stop();
		SCR_EndLoadingPlaque();

		if (key_dest == key_menu)
		{
			key_dest = key_game;
			m_state = m_none;
		}
		if (key_dest != key_console)
			scr_con_current=0;
		return true;
	}
	else
		return false;
}
qboolean Media_ShowFilm(void)
{
	if (videoshader)
	{
		cin_t *cin = R_ShaderGetCinematic(videoshader);
		if (cin && cin->ended)
			Media_PlayFilm("");
		else
		{
			R2D_ImageColours(1, 1, 1, 1);
			R2D_ScalePic(0, 0, vid.width, vid.height, videoshader);

			SCR_SetUpToDrawConsole();
			if  (scr_con_current)
				SCR_DrawConsole (false);
			return true;
		}
	}
	return false;
}

#if defined(GLQUAKE) || defined(D3DQUAKE)
texid_tf Media_UpdateForShader(cin_t *cin)
{
	if (!cin)
		return r_nulltex;
	if (!cin->decodeframe(cin, false))
	{
		return r_nulltex;
	}

	if (!cin->outunchanged)
	{
		if (!TEXVALID(cin->texture))
			TEXASSIGN(cin->texture, R_AllocNewTexture("***cin***", cin->outwidth, cin->outheight));
		R_Upload(cin->texture, "cin", cin->outtype, cin->outdata, cin->outpalette, cin->outwidth, cin->outheight, IF_NOMIPMAP|IF_NOALPHA|IF_NOGAMMA);
	}

	if (cin->doneframe)
		cin->doneframe(cin);


	return cin->texture;
}
#endif

void Media_Send_Command(cin_t *cin, char *command)
{
	if (!cin)
		cin = R_ShaderGetCinematic(videoshader);
	if (!cin || !cin->changestream)
		return;
	cin->changestream(cin, command);
}
void Media_Send_KeyEvent(cin_t *cin, int button, int unicode, int event)
{
	if (!cin)
		cin = R_ShaderGetCinematic(videoshader);
	if (!cin || !cin->key)
		return;
	cin->key(cin, button, unicode, event);
}
void Media_Send_MouseMove(cin_t *cin, float x, float y)
{
	if (!cin)
		cin = R_ShaderGetCinematic(videoshader);
	if (!cin || !cin->cursormove)
		return;
	cin->cursormove(cin, x, y);
}
void Media_Send_Resize(cin_t *cin, int x, int y)
{
	if (!cin)
		cin = R_ShaderGetCinematic(videoshader);
	if (!cin || !cin->setsize)
		return;
	cin->setsize(cin, x, y);
}
void Media_Send_GetSize(cin_t *cin, int *x, int *y)
{
	if (!cin)
		cin = R_ShaderGetCinematic(videoshader);
	if (!cin || !cin->getsize)
		return;
	cin->getsize(cin, x, y);
}



void Media_PlayFilm_f (void)
{
	if (Cmd_Argc() < 2)
	{
		Con_Printf("playfilm <filename>");
	}
	if (!strcmp(Cmd_Argv(0), "cinematic"))
		Media_PlayFilm(va("video/%s", Cmd_Argv(1)));
	else
		Media_PlayFilm(Cmd_Argv(1));
}



















#if defined(GLQUAKE)
#if defined(WINAVI)
#define WINAVIRECORDING
PAVIFILE recordavi_file;
#define recordavi_video_stream (recordavi_codec_fourcc?recordavi_compressed_video_stream:recordavi_uncompressed_video_stream)
PAVISTREAM recordavi_uncompressed_video_stream;
PAVISTREAM recordavi_compressed_video_stream;
PAVISTREAM recordavi_uncompressed_audio_stream;
WAVEFORMATEX recordavi_wave_format;
unsigned long recordavi_codec_fourcc;
#endif /* WINAVI */

soundcardinfo_t *capture_fakesounddevice;
int recordavi_video_frame_counter;
int recordavi_audio_frame_counter;
float recordavi_frametime;	//length of a frame in fractional seconds
float recordavi_videotime;
float recordavi_audiotime;
int capturesize;
int capturewidth;
char *capturevideomem;
vfsfile_t *captureaudiorawfile;
//short *captureaudiomem;
int captureaudiosamples;
int captureframe;
qboolean capturepaused;
cvar_t capturerate = SCVAR("capturerate", "15");
#if defined(WINAVI)
cvar_t capturecodec = SCVAR("capturecodec", "divx");
#else
cvar_t capturecodec = SCVAR("capturecodec", "tga");
#endif
cvar_t capturesound = SCVAR("capturesound", "1");
cvar_t capturesoundchannels = SCVAR("capturesoundchannels", "1");
cvar_t capturesoundbits = SCVAR("capturesoundbits", "8");
cvar_t capturemessage = SCVAR("capturemessage", "");
qboolean recordingdemo;
enum {
	CT_NONE,
	CT_AVI,
	CT_SCREENSHOT
} capturetype;
char capturefilenameprefix[MAX_QPATH];

qboolean Media_Capturing (void)
{
	if (!capturetype)
		return false;
	return true;
}

void Media_CapturePause_f (void)
{
	capturepaused = !capturepaused;
}

qboolean Media_PausedDemo (void)
{
	//capturedemo doesn't record any frames when the console is visible
	//but that's okay, as we don't load any demo frames either.
	if ((cls.demoplayback && Media_Capturing()) || capturepaused)
		if (key_dest != key_game || scr_con_current > 0 || !cl.validsequence || capturepaused)
			return true;

	return false;
}

double Media_TweekCaptureFrameTime(double time)
{
	if (cls.demoplayback && Media_Capturing() && recordavi_frametime)
	{
		return time = recordavi_frametime;
	}
	return time;
}

void Media_RecordFrame (void)
{
	if (!capturetype)
		return;

	if (Media_PausedDemo())
	{
		int y = vid.height -32-16;
		if (y < scr_con_current) y = scr_con_current;
		if (y > vid.height-8)
			y = vid.height-8;
		Draw_FunString((strlen(capturemessage.string)+1)*8, y, S_COLOR_RED "PAUSED");
		return;
	}

	if (cls.findtrack)
		return;	//skip until we're tracking the right player.

//overlay this on the screen, so it appears in the film
	if (*capturemessage.string)
	{
		int y = vid.height -32-16;
		if (y < scr_con_current) y = scr_con_current;
		if (y > vid.height-8)
			y = vid.height-8;
		Draw_FunString(0, y, capturemessage.string);
	}

	//time for annother frame?
	if (recordavi_videotime > realtime+1)
		recordavi_videotime = realtime;	//urm, wrapped?..
	if (recordavi_videotime > realtime)
		goto skipframe;
	recordavi_videotime += recordavi_frametime;
	//audio is mixed to match the video times

	switch (capturetype)
	{
	case CT_AVI:
#if defined(WINAVI)
		{
			HRESULT hr;
			char *framebuffer = capturevideomem;
			qbyte temp;
			int i, c;

			if (!framebuffer)
			{
				Con_Printf("framebuffer = NULL with AVI capture type (this shouldn't happen)\n");
				return;
			}
		//ask gl for it
			qglReadPixels (0, 0, vid.pixelwidth, vid.pixelheight, GL_RGB, GL_UNSIGNED_BYTE, framebuffer );

			// swap rgb to bgr
			c = vid.pixelwidth*vid.pixelheight*3;
			for (i=0 ; i<c ; i+=3)
			{
				temp = framebuffer[i];
				framebuffer[i] = framebuffer[i+2];
				framebuffer[i+2] = temp;
			}
			//write it
			hr = qAVIStreamWrite(recordavi_video_stream, captureframe++, 1, framebuffer, vid.pixelwidth*vid.pixelheight * 3, ((captureframe%15) == 0)?AVIIF_KEYFRAME:0, NULL, NULL);
			if (FAILED(hr)) Con_Printf("Recoring error\n");
		}
#endif /* WINAVI */
		break;
	case CT_SCREENSHOT:
		{
			char filename[MAX_OSPATH];
			Q_snprintfz(filename, sizeof(filename), "%s/%8.8i.%s", capturefilenameprefix, captureframe++, capturecodec.string);
			SCR_ScreenShot(filename);
		}
		break;
	case CT_NONE:	//non issue.
		;
	}

	//this is drawn to the screen and not the film
skipframe:
{
	int y = vid.height -32-16;
	if (y < scr_con_current) y = scr_con_current;
	if (y > vid.height-8)
		y = vid.height-8;
	Draw_FunString((strlen(capturemessage.string)+1)*8, y, S_COLOR_RED"RECORDING");
}
}

static void MSD_SetUnderWater(soundcardinfo_t *sc, qboolean underwater)
{
}

static void *MSD_Lock (soundcardinfo_t *sc, unsigned int *sampidx)
{
	return sc->sn.buffer;
}
static void MSD_Unlock (soundcardinfo_t *sc, void *buffer)
{
}

static unsigned int MSD_GetDMAPos(soundcardinfo_t *sc)
{
	int		s;

	s = captureframe*(snd_speed*recordavi_frametime);


//	s >>= (sc->sn.samplebits/8) - 1;
	s *= sc->sn.numchannels;
	return s;
}

static void MSD_Submit(soundcardinfo_t *sc, int start, int end)
{
	//Fixme: support outputting to wav
	//http://www.borg.com/~jglatt/tech/wave.htm


	int lastpos;
	int newpos;
	int samplestosubmit;
	int offset;
	int bytespersample;

	lastpos = sc->snd_completed;
	newpos = sc->paintedtime;

	samplestosubmit = newpos - lastpos;
	if (samplestosubmit < (snd_speed*recordavi_frametime))
		return;

	bytespersample = sc->sn.numchannels*sc->sn.samplebits/8;

	sc->snd_completed = newpos;
	offset = (lastpos % (sc->sn.samples/sc->sn.numchannels));

	//we could just use a buffer size equal to the number of samples in each frame
	//but that isn't as robust when it comes to floating point imprecisions
	//namly: that it would loose a sample each frame with most framerates.

	switch (capturetype)
	{
	case CT_AVI:
#if defined(WINAVI)
		if ((sc->snd_completed % (sc->sn.samples/sc->sn.numchannels)) < offset)
		{
			int partialsamplestosubmit;
			//wraped, two chunks to send
			partialsamplestosubmit = ((sc->sn.samples/sc->sn.numchannels)) - offset;
			qAVIStreamWrite(recordavi_uncompressed_audio_stream, recordavi_audio_frame_counter++, 1, sc->sn.buffer+offset*bytespersample, partialsamplestosubmit*bytespersample, AVIIF_KEYFRAME, NULL, NULL);
			samplestosubmit -= partialsamplestosubmit;
			offset = 0;
		}
		qAVIStreamWrite(recordavi_uncompressed_audio_stream, recordavi_audio_frame_counter++, 1, sc->sn.buffer+offset*bytespersample, samplestosubmit*bytespersample, AVIIF_KEYFRAME, NULL, NULL);
#endif /* WINAVI */
		break;
	case CT_NONE:
		break;
	case CT_SCREENSHOT:
		if ((sc->snd_completed % (sc->sn.samples/sc->sn.numchannels)) < offset)
		{
			int partialsamplestosubmit;
			//wraped, two chunks to send
			partialsamplestosubmit = ((sc->sn.samples/sc->sn.numchannels)) - offset;
			VFS_WRITE(captureaudiorawfile, sc->sn.buffer+offset*bytespersample, partialsamplestosubmit*bytespersample);
			samplestosubmit -= partialsamplestosubmit;
			offset = 0;
		}
		VFS_WRITE(captureaudiorawfile, sc->sn.buffer+offset*bytespersample, samplestosubmit*bytespersample);
		break;
	}
}

static void MSD_Shutdown (soundcardinfo_t *sc)
{
	Z_Free(sc->sn.buffer);
	capture_fakesounddevice = NULL;
}

void Media_InitFakeSoundDevice (int channels, int samplebits)
{
	soundcardinfo_t *sc;

	if (capture_fakesounddevice)
		return;

	sc = Z_Malloc(sizeof(soundcardinfo_t));

	sc->snd_sent = 0;
	sc->snd_completed = 0;

	sc->sn.samples = snd_speed*0.5;
	sc->sn.speed = snd_speed;
	sc->sn.samplebits = samplebits;
	sc->sn.samplepos = 0;
	sc->sn.numchannels = channels;
	sc->inactive_sound = true;

	sc->sn.buffer = (unsigned char *) BZ_Malloc(sc->sn.samples*sc->sn.numchannels*(sc->sn.samplebits/8));


	sc->Lock		= MSD_Lock;
	sc->Unlock		= MSD_Unlock;
	sc->SetWaterDistortion = MSD_SetUnderWater;
	sc->Submit		= MSD_Submit;
	sc->Shutdown	= MSD_Shutdown;
	sc->GetDMAPos	= MSD_GetDMAPos;

	sc->next = sndcardinfo;
	sndcardinfo = sc;

	capture_fakesounddevice = sc;

	S_DefaultSpeakerConfiguration(sc);
}



void Media_StopRecordFilm_f (void)
{
#if defined(WINAVI)
    if (recordavi_uncompressed_video_stream)	qAVIStreamRelease(recordavi_uncompressed_video_stream);
    if (recordavi_compressed_video_stream)		qAVIStreamRelease(recordavi_compressed_video_stream);
    if (recordavi_uncompressed_audio_stream)	qAVIStreamRelease(recordavi_uncompressed_audio_stream);
    if (recordavi_file)					qAVIFileRelease(recordavi_file);

	recordavi_uncompressed_video_stream=NULL;
	recordavi_compressed_video_stream = NULL;
	recordavi_uncompressed_audio_stream=NULL;
	recordavi_file = NULL;
#endif /* WINAVI */

	if (capturevideomem)
		BZ_Free(capturevideomem);
	capturevideomem = NULL;

	if (capture_fakesounddevice)
		S_ShutdownCard(capture_fakesounddevice);
	capture_fakesounddevice = NULL;

	if (captureaudiorawfile)
		VFS_CLOSE(captureaudiorawfile);
	captureaudiorawfile = NULL;


	capturevideomem = NULL;

	recordingdemo=false;

	capturetype = CT_NONE;
}
void Media_RecordFilm_f (void)
{
	char *fourcc = capturecodec.string;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("capture <filename>\nRecords video output in an avi file.\nUse capturerate and capturecodec to configure.\n");
		return;
	}

	if (Cmd_IsInsecure())	//err... don't think so sonny.
		return;


	Media_StopRecordFilm_f();

	recordavi_video_frame_counter = 0;
	recordavi_audio_frame_counter = 0;

	if (capturerate.value<=0)
	{
		Con_Printf("Invalid capturerate\n");
		capturerate.value = 15;
	}

	recordavi_frametime = 1/capturerate.value;
	if (recordavi_frametime < 0.001)
		recordavi_frametime = 0.001;	//no more than 1000 images per second.

	captureframe = 0;
	if (*fourcc)
	{
		if (!strcmp(fourcc, "tga") ||
			!strcmp(fourcc, "png") ||
			!strcmp(fourcc, "jpg") ||
			!strcmp(fourcc, "pcx"))
		{
			capturetype = CT_SCREENSHOT;
			Q_strncpyz(capturefilenameprefix, Cmd_Argv(1), sizeof(capturefilenameprefix));
		}
		else
		{
			capturetype = CT_AVI;
		}
	}
	else
	{
		capturetype = CT_AVI;	//uncompressed avi
	}

	if (capturetype == CT_NONE)
	{

	}
	else if (capturetype == CT_SCREENSHOT)
	{
		if (capturesound.value && capturesoundchannels.value >= 1)
		{
			char filename[MAX_OSPATH];
			int chans = capturesoundchannels.value;
			int sbits = capturesoundbits.value;
			if (sbits < 8)
				sbits = 8;
			if (sbits != 8)
				sbits = 16;
			if (chans > 6)
				chans = 6;
			Q_snprintfz(filename, sizeof(filename), "%s/audio_%ichan_%ikhz_%ib.raw", capturefilenameprefix, chans, snd_speed/1000, sbits);
			captureaudiorawfile = FS_OpenVFS(filename, "wb", FS_GAMEONLY);

			if (captureaudiorawfile)
				Media_InitFakeSoundDevice(chans, sbits);
		}
	}
#if defined(WINAVI)
	else if (capturetype == CT_AVI)
	{
		HRESULT hr;
		BITMAPINFOHEADER bitmap_info_header;
		AVISTREAMINFOA stream_header;
		FILE *f;
		char aviname[256];
		char nativepath[256];

		if (strlen(fourcc) == 4)
			recordavi_codec_fourcc = mmioFOURCC(*(fourcc+0), *(fourcc+1), *(fourcc+2), *(fourcc+3));
		else
			recordavi_codec_fourcc = 0;

		if (!qAVIStartup())
		{
			Con_Printf("vfw support not available.\n");
			return;
		}

		/*convert to foo.avi*/
		COM_StripExtension(Cmd_Argv(1), aviname, sizeof(aviname));
		COM_DefaultExtension (aviname, ".avi", sizeof(aviname));
		/*find the system location of that*/
		FS_NativePath(aviname, FS_ROOT, nativepath, sizeof(nativepath));

		//wipe it.
		f = fopen(nativepath, "rb");
		if (f)
		{
			fclose(f);
			unlink(nativepath);
		}

		hr = qAVIFileOpenA(&recordavi_file, nativepath, OF_WRITE | OF_CREATE, NULL);
		if (FAILED(hr))
		{
			Con_Printf("Failed to open %s\n", nativepath);
			return;
		}


		memset(&bitmap_info_header, 0, sizeof(BITMAPINFOHEADER));
		bitmap_info_header.biSize = 40;
		bitmap_info_header.biWidth = vid.pixelwidth;
		bitmap_info_header.biHeight = vid.pixelheight;
		bitmap_info_header.biPlanes = 1;
		bitmap_info_header.biBitCount = 24;
		bitmap_info_header.biCompression = BI_RGB;
		bitmap_info_header.biSizeImage = vid.pixelwidth*vid.pixelheight * 3;


		memset(&stream_header, 0, sizeof(stream_header));
		stream_header.fccType = streamtypeVIDEO;
		stream_header.fccHandler = recordavi_codec_fourcc;
		stream_header.dwScale = 100;
		stream_header.dwRate = (unsigned long)(0.5 + 100.0/recordavi_frametime);
		SetRect(&stream_header.rcFrame, 0, 0, vid.pixelwidth, vid.pixelheight);

		hr = qAVIFileCreateStreamA(recordavi_file, &recordavi_uncompressed_video_stream, &stream_header);
		if (FAILED(hr))
		{
			Con_Printf("Couldn't initialise the stream, check codec\n");
			Media_StopRecordFilm_f();
			return;
		}

		if (recordavi_codec_fourcc)
		{
			AVICOMPRESSOPTIONS opts;
			AVICOMPRESSOPTIONS* aopts[1] = { &opts };
			memset(&opts, 0, sizeof(opts));
			opts.fccType = stream_header.fccType;
			opts.fccHandler = recordavi_codec_fourcc;
			// Make the stream according to compression
			hr = qAVIMakeCompressedStream(&recordavi_compressed_video_stream, recordavi_uncompressed_video_stream, &opts, NULL);
			if (FAILED(hr))
			{
				Con_Printf("Failed to init compressor\n");
				Media_StopRecordFilm_f();
				return;
			}
		}


		hr = qAVIStreamSetFormat(recordavi_video_stream, 0, &bitmap_info_header, sizeof(BITMAPINFOHEADER));
		if (FAILED(hr))
		{
			Con_Printf("Failed to set format\n");
			Media_StopRecordFilm_f();
			return;
		}

		if (capturesoundbits.value != 8 && capturesoundbits.value != 16)
			Cvar_Set(&capturesoundbits, "8");
		if (capturesoundchannels.value < 1 && capturesoundchannels.value > 6)
			Cvar_Set(&capturesoundchannels, "1");

		if (capturesound.value)
		{
			memset(&recordavi_wave_format, 0, sizeof(WAVEFORMATEX));
			recordavi_wave_format.wFormatTag = WAVE_FORMAT_PCM;
			recordavi_wave_format.nChannels = capturesoundchannels.value;
			recordavi_wave_format.nSamplesPerSec = snd_speed;
			recordavi_wave_format.wBitsPerSample = capturesoundbits.value;
			recordavi_wave_format.nBlockAlign = recordavi_wave_format.wBitsPerSample/8 * recordavi_wave_format.nChannels;
			recordavi_wave_format.nAvgBytesPerSec = recordavi_wave_format.nSamplesPerSec * recordavi_wave_format.nBlockAlign;
			recordavi_wave_format.cbSize = 0;


			memset(&stream_header, 0, sizeof(stream_header));
			stream_header.fccType = streamtypeAUDIO;
			stream_header.dwScale = recordavi_wave_format.nBlockAlign;
			stream_header.dwRate = stream_header.dwScale * (unsigned long)recordavi_wave_format.nSamplesPerSec;
			stream_header.dwSampleSize = recordavi_wave_format.nBlockAlign;

			hr = qAVIFileCreateStreamA(recordavi_file, &recordavi_uncompressed_audio_stream, &stream_header);
			if (FAILED(hr)) return;

			hr = qAVIStreamSetFormat(recordavi_uncompressed_audio_stream, 0, &recordavi_wave_format, sizeof(WAVEFORMATEX));
			if (FAILED(hr)) return;

			Media_InitFakeSoundDevice(recordavi_wave_format.nChannels, recordavi_wave_format.wBitsPerSample);
		}


		recordavi_videotime = realtime;
		recordavi_audiotime = realtime;

//		if (recordavi_wave_format.nSamplesPerSec)
//			captureaudiomem = BZ_Malloc(recordavi_wave_format.nSamplesPerSec*2);

		capturevideomem = BZ_Malloc(vid.pixelwidth*vid.pixelheight*3);
	}
#endif /* WINAVI */
	else
	{
		Con_Printf("That sort of video capturing is not supported in this build\n");
		capturetype = CT_NONE;
	}
}
void Media_CaptureDemoEnd(void)
{
	if (recordingdemo)
		Media_StopRecordFilm_f();
}
void Media_RecordDemo_f(void)
{
	CL_PlayDemo_f();
	Media_RecordFilm_f();
	scr_con_current=0;
	key_dest = key_game;

	if (capturetype != CT_NONE)
		recordingdemo = true;
	else
		CL_Stopdemo_f();	//capturing failed for some reason
}
#else /* GLQUAKE */
void Media_CaptureDemoEnd(void){}
void Media_RecordAudioFrame (short *sample_buffer, int samples){}
double Media_TweekCaptureFrameTime(double time) { return time ; }
void Media_RecordFrame (void) {}
qboolean Media_PausedDemo (void) {return false;} //should not return a value
#endif /* GLQUAKE */

#ifdef _WIN32
typedef struct ISpNotifySink ISpNotifySink;
typedef void *ISpNotifyCallback;
typedef void __stdcall SPNOTIFYCALLBACK(WPARAM wParam, LPARAM lParam);
typedef struct SPEVENT
{
    WORD        eEventId : 16;
    WORD  elParamType : 16;
    ULONG       ulStreamNum;
    ULONGLONG   ullAudioStreamOffset;
    WPARAM      wParam;
    LPARAM      lParam;
} SPEVENT;

#define SPEVENTSOURCEINFO void
#define ISpObjectToken void
#define ISpStreamFormat void
#define SPVOICESTATUS void
#define SPVPRIORITY int
#define SPEVENTENUM int

typedef struct ISpVoice ISpVoice;
typedef struct ISpVoiceVtbl
{
    HRESULT ( STDMETHODCALLTYPE *QueryInterface )(
        ISpVoice * This,
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ void **ppvObject);

    ULONG ( STDMETHODCALLTYPE *AddRef )(
        ISpVoice * This);

    ULONG ( STDMETHODCALLTYPE *Release )(
        ISpVoice * This);

    HRESULT ( STDMETHODCALLTYPE *SetNotifySink )(
        ISpVoice * This,
        /* [in] */ ISpNotifySink *pNotifySink);

    /* [local] */ HRESULT ( STDMETHODCALLTYPE *SetNotifyWindowMessage )(
        ISpVoice * This,
        /* [in] */ HWND hWnd,
        /* [in] */ UINT Msg,
        /* [in] */ WPARAM wParam,
        /* [in] */ LPARAM lParam);

    /* [local] */ HRESULT ( STDMETHODCALLTYPE *SetNotifyCallbackFunction )(
        ISpVoice * This,
        /* [in] */ SPNOTIFYCALLBACK *pfnCallback,
        /* [in] */ WPARAM wParam,
        /* [in] */ LPARAM lParam);

    /* [local] */ HRESULT ( STDMETHODCALLTYPE *SetNotifyCallbackInterface )(
        ISpVoice * This,
        /* [in] */ ISpNotifyCallback *pSpCallback,
        /* [in] */ WPARAM wParam,
        /* [in] */ LPARAM lParam);

    /* [local] */ HRESULT ( STDMETHODCALLTYPE *SetNotifyWin32Event )(
        ISpVoice * This);

    /* [local] */ HRESULT ( STDMETHODCALLTYPE *WaitForNotifyEvent )(
        ISpVoice * This,
        /* [in] */ DWORD dwMilliseconds);

    /* [local] */ HANDLE ( STDMETHODCALLTYPE *GetNotifyEventHandle )(
        ISpVoice * This);

    HRESULT ( STDMETHODCALLTYPE *SetInterest )(
        ISpVoice * This,
        /* [in] */ ULONGLONG ullEventInterest,
        /* [in] */ ULONGLONG ullQueuedInterest);

    HRESULT ( STDMETHODCALLTYPE *GetEvents )(
        ISpVoice * This,
        /* [in] */ ULONG ulCount,
        /* [size_is][out] */ SPEVENT *pEventArray,
        /* [out] */ ULONG *pulFetched);

    HRESULT ( STDMETHODCALLTYPE *GetInfo )(
        ISpVoice * This,
        /* [out] */ SPEVENTSOURCEINFO *pInfo);

    HRESULT ( STDMETHODCALLTYPE *SetOutput )(
        ISpVoice * This,
        /* [in] */ IUnknown *pUnkOutput,
        /* [in] */ BOOL fAllowFormatChanges);

    HRESULT ( STDMETHODCALLTYPE *GetOutputObjectToken )(
        ISpVoice * This,
        /* [out] */ ISpObjectToken **ppObjectToken);

    HRESULT ( STDMETHODCALLTYPE *GetOutputStream )(
        ISpVoice * This,
        /* [out] */ ISpStreamFormat **ppStream);

    HRESULT ( STDMETHODCALLTYPE *Pause )(
        ISpVoice * This);

    HRESULT ( STDMETHODCALLTYPE *Resume )(
        ISpVoice * This);

    HRESULT ( STDMETHODCALLTYPE *SetVoice )(
        ISpVoice * This,
        /* [in] */ ISpObjectToken *pToken);

    HRESULT ( STDMETHODCALLTYPE *GetVoice )(
        ISpVoice * This,
        /* [out] */ ISpObjectToken **ppToken);

    HRESULT ( STDMETHODCALLTYPE *Speak )(
        ISpVoice * This,
        /* [string][in] */ const WCHAR *pwcs,
        /* [in] */ DWORD dwFlags,
        /* [out] */ ULONG *pulStreamNumber);

    HRESULT ( STDMETHODCALLTYPE *SpeakStream )(
        ISpVoice * This,
        /* [in] */ IStream *pStream,
        /* [in] */ DWORD dwFlags,
        /* [out] */ ULONG *pulStreamNumber);

    HRESULT ( STDMETHODCALLTYPE *GetStatus )(
        ISpVoice * This,
        /* [out] */ SPVOICESTATUS *pStatus,
        /* [string][out] */ WCHAR **ppszLastBookmark);

    HRESULT ( STDMETHODCALLTYPE *Skip )(
        ISpVoice * This,
        /* [string][in] */ WCHAR *pItemType,
        /* [in] */ long lNumItems,
        /* [out] */ ULONG *pulNumSkipped);

    HRESULT ( STDMETHODCALLTYPE *SetPriority )(
        ISpVoice * This,
        /* [in] */ SPVPRIORITY ePriority);

    HRESULT ( STDMETHODCALLTYPE *GetPriority )(
        ISpVoice * This,
        /* [out] */ SPVPRIORITY *pePriority);

    HRESULT ( STDMETHODCALLTYPE *SetAlertBoundary )(
        ISpVoice * This,
        /* [in] */ SPEVENTENUM eBoundary);

    HRESULT ( STDMETHODCALLTYPE *GetAlertBoundary )(
        ISpVoice * This,
        /* [out] */ SPEVENTENUM *peBoundary);

    HRESULT ( STDMETHODCALLTYPE *SetRate )(
        ISpVoice * This,
        /* [in] */ long RateAdjust);

    HRESULT ( STDMETHODCALLTYPE *GetRate )(
        ISpVoice * This,
        /* [out] */ long *pRateAdjust);

    HRESULT ( STDMETHODCALLTYPE *SetVolume )(
        ISpVoice * This,
        /* [in] */ USHORT usVolume);

    HRESULT ( STDMETHODCALLTYPE *GetVolume )(
        ISpVoice * This,
        /* [out] */ USHORT *pusVolume);

    HRESULT ( STDMETHODCALLTYPE *WaitUntilDone )(
        ISpVoice * This,
        /* [in] */ ULONG msTimeout);

    HRESULT ( STDMETHODCALLTYPE *SetSyncSpeakTimeout )(
        ISpVoice * This,
        /* [in] */ ULONG msTimeout);

    HRESULT ( STDMETHODCALLTYPE *GetSyncSpeakTimeout )(
        ISpVoice * This,
        /* [out] */ ULONG *pmsTimeout);

    /* [local] */ HANDLE ( STDMETHODCALLTYPE *SpeakCompleteEvent )(
        ISpVoice * This);

    /* [local] */ HRESULT ( STDMETHODCALLTYPE *IsUISupported )(
        ISpVoice * This,
        /* [in] */ const WCHAR *pszTypeOfUI,
        /* [in] */ void *pvExtraData,
        /* [in] */ ULONG cbExtraData,
        /* [out] */ BOOL *pfSupported);

    /* [local] */ HRESULT ( STDMETHODCALLTYPE *DisplayUI )(
        ISpVoice * This,
        /* [in] */ HWND hwndParent,
        /* [in] */ const WCHAR *pszTitle,
        /* [in] */ const WCHAR *pszTypeOfUI,
        /* [in] */ void *pvExtraData,
        /* [in] */ ULONG cbExtraData);

    END_INTERFACE
} ISpVoiceVtbl;

struct ISpVoice
{
    struct ISpVoiceVtbl *lpVtbl;
};
void TTS_SayUnicodeString(wchar_t *stringtosay)
{
	static CLSID CLSID_SpVoice = {0x96749377, 0x3391, 0x11D2,
								0x9E,0xE3,0x00,0xC0,0x4F,0x79,0x73,0x96};
	static GUID IID_ISpVoice = {0x6C44DF74,0x72B9,0x4992,
								0xA1,0xEC,0xEF,0x99,0x6E,0x04,0x22,0xD4};
	static ISpVoice *sp = NULL;

	if (!sp)
		CoCreateInstance(
				&CLSID_SpVoice,
				NULL,
				CLSCTX_SERVER,
				&IID_ISpVoice,
				&sp);

	if (sp)
	{
		sp->lpVtbl->Speak(sp, stringtosay, 1, NULL);
	}
}
void TTS_SayAsciiString(char *stringtosay)
{
	wchar_t bigbuffer[8192];
	mbstowcs(bigbuffer, stringtosay, sizeof(bigbuffer)/sizeof(bigbuffer[0]) - 1);
	bigbuffer[sizeof(bigbuffer)/sizeof(bigbuffer[0]) - 1] = 0;
	TTS_SayUnicodeString(bigbuffer);
}

cvar_t tts_mode = CVAR("tts_mode", "1");
void TTS_SayChatString(char **stringtosay)
{
	if (!strncmp(*stringtosay, "tts ", 4))
	{
		*stringtosay += 4;
		if (tts_mode.ival != 1 && tts_mode.ival != 2)
			return;
	}
	else
	{
		if (tts_mode.ival != 2)
			return;
	}

	TTS_SayAsciiString(*stringtosay);
}
void TTS_SayConString(conchar_t *stringtosay)
{
	wchar_t bigbuffer[8192];
	int i;

	if (tts_mode.ival < 3)
		return;
	
	for (i = 0; i < 8192-1 && *stringtosay; i++, stringtosay++)
	{
		if ((*stringtosay & 0xff00) == 0xe000)
			bigbuffer[i] = *stringtosay & 0x7f;
		else
			bigbuffer[i] = *stringtosay & CON_CHARMASK;
	}
	bigbuffer[i] = 0;
	if (i)
		TTS_SayUnicodeString(bigbuffer);
}
void TTS_Say_f(void)
{
	TTS_SayAsciiString(Cmd_Args());
}
#endif

qboolean S_LoadMP3Sound (sfx_t *s, qbyte *data, int datalen, int sndspeed);

void Media_Init(void)
{
#ifdef _WIN32
	Cmd_AddCommand("tts", TTS_Say_f);
	Cvar_Register(&tts_mode, "Gimmicks");
#endif

	Cmd_AddCommand("playfilm", Media_PlayFilm_f);
	Cmd_AddCommand("cinematic", Media_PlayFilm_f);
	Cmd_AddCommand("music_fforward", Media_FForward_f);
	Cmd_AddCommand("music_rewind", Media_Rewind_f);
	Cmd_AddCommand("music_next", Media_Next_f);

#if defined(GLQUAKE)
	Cmd_AddCommand("capture", Media_RecordFilm_f);
	Cmd_AddCommand("capturedemo", Media_RecordDemo_f);
	Cmd_AddCommand("capturestop", Media_StopRecordFilm_f);
	Cmd_AddCommand("capturepause", Media_CapturePause_f);

	Cvar_Register(&capturemessage,	"AVI capture controls");
	Cvar_Register(&capturesound,	"AVI capture controls");
	Cvar_Register(&capturerate,	"AVI capture controls");
	Cvar_Register(&capturecodec,	"AVI capture controls");

#if defined(WINAVI)
	Cvar_Register(&capturesoundbits,	"AVI capture controls");
	Cvar_Register(&capturesoundchannels,	"AVI capture controls");

	S_RegisterSoundInputPlugin(S_LoadMP3Sound);
#endif

#endif

#ifdef WINAMP
	Cvar_Register(&media_hijackwinamp,	"Media player things");
#endif
	Cvar_Register(&media_shuffle,	"Media player things");
	Cvar_Register(&media_repeat,	"Media player things");
}



#ifdef WINAVI
typedef struct
{
	HACMSTREAM acm;

	unsigned int dstbuffer; /*in frames*/
	unsigned int dstcount; /*in frames*/
	unsigned int dststart; /*in frames*/
	qbyte *dstdata;

	unsigned int srcspeed;
	unsigned int srcwidth;
	unsigned int srcchannels;
	unsigned int srcoffset; /*in bytes*/
	unsigned int srclen;	/*in bytes*/
	qbyte srcdata[1];
} mp3decoder_t;

/*must be thread safe*/
sfxcache_t *S_MP3_Locate(sfx_t *sfx, sfxcache_t *buf, int start, int length)
{
	int newlen;
	if (buf)
	{
		mp3decoder_t *dec = sfx->decoder.buf;
		ACMSTREAMHEADER strhdr;
		char buffer[8192];
		extern cvar_t snd_linearresample_stream;
		int framesz = (dec->srcwidth/8 * dec->srcchannels);

		if (dec->dststart > start)
		{
			/*I don't know where the compressed data is for each sample. acm doesn't have a seek. so reset to start, for music this should be the most common rewind anyway*/
			dec->dststart = 0;
			dec->dstcount = 0;
			dec->srcoffset = 0;
		}

		if (dec->dstcount > snd_speed*6)
		{
			int trim = dec->dstcount - snd_speed; //retain a second of buffer in case we have multiple sound devices
//			if (trim < 0)
//				trim = 0;
///			if (trim > dec->dstcount)
//				trim = dec->dstcount;
			memmove(dec->dstdata, dec->dstdata + trim*framesz, (dec->dstcount - trim)*framesz);
			dec->dststart += trim;
			dec->dstcount -= trim;
		}

		while(start+length >= dec->dststart+dec->dstcount)
		{
			memset(&strhdr, 0, sizeof(strhdr));
			strhdr.cbStruct = sizeof(strhdr);
			strhdr.pbSrc = dec->srcdata + dec->srcoffset;
			strhdr.cbSrcLength = dec->srclen - dec->srcoffset;
			strhdr.pbDst = buffer;
			strhdr.cbDstLength = sizeof(buffer);

			qacmStreamPrepareHeader(dec->acm, &strhdr, 0);
			qacmStreamConvert(dec->acm, &strhdr, ACM_STREAMCONVERTF_BLOCKALIGN);
			qacmStreamUnprepareHeader(dec->acm, &strhdr, 0);
			dec->srcoffset += strhdr.cbSrcLengthUsed;
			if (!strhdr.cbDstLengthUsed)
			{
				if (strhdr.cbSrcLengthUsed)
					continue;
				break;
			}

			newlen = dec->dstcount + (strhdr.cbDstLengthUsed * ((float)snd_speed / dec->srcspeed))/framesz;
			if (dec->dstbuffer < newlen+64)
			{
				dec->dstbuffer = newlen+64 + snd_speed;
				dec->dstdata = BZ_Realloc(dec->dstdata, dec->dstbuffer*framesz);
			}

			SND_ResampleStream(strhdr.pbDst, 
				dec->srcspeed, 
				dec->srcwidth/8, 
				dec->srcchannels, 
				strhdr.cbDstLengthUsed / framesz,
				dec->dstdata+dec->dstcount*framesz,
				snd_speed,
				dec->srcwidth/8,
				dec->srcchannels,
				snd_linearresample_stream.ival);
			dec->dstcount = newlen;
		}

		buf->data = dec->dstdata;
		buf->length = dec->dstcount;
		buf->loopstart = -1;
		buf->numchannels = dec->srcchannels;
		buf->soundoffset = dec->dststart;
		buf->speed = snd_speed;
		buf->width = dec->srcwidth/8;
	}
	return buf;
}

#ifndef WAVE_FORMAT_MPEGLAYER3
#define WAVE_FORMAT_MPEGLAYER3 0x0055
#define MPEGLAYER3_WFX_EXTRA_BYTES 12
#define MPEGLAYER3_FLAG_PADDING_OFF 2
#define MPEGLAYER3_ID_MPEG 1

typedef struct
{
	WAVEFORMATEX  wfx;
	WORD          wID;
	DWORD         fdwFlags;
	WORD          nBlockSize;
	WORD          nFramesPerBlock;
	WORD          nCodecDelay;
} MPEGLAYER3WAVEFORMAT;
#endif

qboolean S_LoadMP3Sound (sfx_t *s, qbyte *data, int datalen, int sndspeed)
{
	WAVEFORMATEX pcm_format;
	MPEGLAYER3WAVEFORMAT mp3format;
	HACMDRIVER drv = NULL;
	mp3decoder_t *dec;

	char *ext = COM_FileExtension(s->name);
	if (stricmp(ext, "mp3"))
		return false;

	dec = BZF_Malloc(sizeof(*dec) + datalen);
	if (!dec)
		return false;
	memcpy(dec->srcdata, data, datalen);
	dec->srclen = datalen;
	s->decoder.buf = dec;
	s->decoder.abort = NULL;
	s->decoder.decodedata = S_MP3_Locate;
	
	dec->dstdata = NULL;
	dec->dstcount = 0;
	dec->dststart = 0;
	dec->dstbuffer = 0;
	dec->srcoffset = 0;

	dec->srcspeed = 44100;
	dec->srcchannels = 2;
	dec->srcwidth = 16;

	memset (&pcm_format, 0, sizeof(pcm_format));
	pcm_format.wFormatTag = WAVE_FORMAT_PCM;
	pcm_format.nChannels = dec->srcchannels;
	pcm_format.nSamplesPerSec = dec->srcspeed;
	pcm_format.nBlockAlign = dec->srcwidth/8*dec->srcchannels;
	pcm_format.nAvgBytesPerSec = pcm_format.nSamplesPerSec*dec->srcwidth/8*dec->srcchannels;
	pcm_format.wBitsPerSample = dec->srcwidth;
	pcm_format.cbSize = 0;

	mp3format.wfx.cbSize = MPEGLAYER3_WFX_EXTRA_BYTES;
	mp3format.wfx.wFormatTag = WAVE_FORMAT_MPEGLAYER3;
	mp3format.wfx.nChannels = dec->srcchannels;
	mp3format.wfx.nAvgBytesPerSec = 128 * (1024 / 8);  // not really used but must be one of 64, 96, 112, 128, 160kbps
	mp3format.wfx.wBitsPerSample = 0;                  // MUST BE ZERO
	mp3format.wfx.nBlockAlign = 1;                     // MUST BE ONE
	mp3format.wfx.nSamplesPerSec = dec->srcspeed;       // 44.1kHz
	mp3format.fdwFlags = MPEGLAYER3_FLAG_PADDING_OFF;
	mp3format.nBlockSize = 522;					       // voodoo value #1 - 144 x (bitrate / sample rate) + padding
	mp3format.nFramesPerBlock = 1;                     // MUST BE ONE
	mp3format.nCodecDelay = 0;//1393;                      // voodoo value #2
	mp3format.wID = MPEGLAYER3_ID_MPEG;

	if (!qacmStartup() || 0!=qacmStreamOpen(&dec->acm, drv, (WAVEFORMATEX*)&mp3format, &pcm_format, NULL, 0, 0, 0))
	{
		Con_Printf("Couldn't init decoder\n");
		return false;
	}

	S_MP3_Locate(s, NULL, 0, 100);
	return true;
}
#endif




#else
void M_Media_Draw (void){}
void M_Media_Key (int key) {}
qboolean Media_ShowFilm(void){return false;}

double Media_TweekCaptureFrameTime(double time) { return time ; }
void Media_RecordFrame (void) {}
void Media_CaptureDemoEnd(void) {}
void Media_RecordDemo_f(void) {}
void Media_RecordAudioFrame (short *sample_buffer, int samples) {}
void Media_StopRecordFilm_f (void) {}
void Media_RecordFilm_f (void){}
void M_Menu_Media_f (void) {}

char *Media_NextTrack(int musicchannelnum) {return NULL;}
qboolean Media_PausedDemo(void) {return false;}

int filmtexture;
#endif
