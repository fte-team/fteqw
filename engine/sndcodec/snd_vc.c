//voice chat routines.

//needs quite a bit of work.

//it needs a proper protocol.
//the server needs to be able to shutdown again.
//options about who gets the sound data is also needed.

#include "bothdefs.h"

#ifdef VOICECHAT

#include "quakedef.h"
#ifdef _WIN32
#include "winquake.h"
#endif

#include "netinc.h"

#include "voicechat.h"

static int CLVS_socket;
static int SVVS_socket;
static qboolean SVVS_inited;

static qbyte inputbuffer[44100];
static int readpoint;
static qbyte outputbuffer[44100];
static int sendpoint;

/*
Protocol:
	Sound chunk:
		(char)	data begins with codec id.
		(short)	followed by number of samples
		(short)	then size in bytes of chunk.
*/

#ifndef CLIENTONLY
void SVVC_ServerInit(void)
{	
	netadr_t adr;
	struct sockaddr_in address;
	unsigned long _true = true;
	int i;
	int port = PORT_SERVER;

	if ((SVVS_socket = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		Sys_Error ("FTP_TCP_OpenSocket: socket:", strerror(qerrno));
	}

	if (ioctlsocket (SVVS_socket, FIONBIO, &_true) == -1)
	{
		Sys_Error ("FTP_TCP_OpenSocket: ioctl FIONBIO:", strerror(qerrno));
	}

	address.sin_family = AF_INET;
//ZOID -- check for interface binding option
	if ((i = COM_CheckParm("-ip")) != 0 && i < com_argc) {
		address.sin_addr.s_addr = inet_addr(com_argv[i+1]);
		Con_TPrintf(TL_NETBINDINTERFACE,
				inet_ntoa(address.sin_addr));
	} else
		address.sin_addr.s_addr = INADDR_ANY;

	if (port == PORT_ANY)
		address.sin_port = 0;
	else
		address.sin_port = htons((short)port);
	
	if( bind (SVVS_socket, (void *)&address, sizeof(address)) == -1)
	{
		closesocket(SVVS_socket);
		return;
	}
	
	listen(SVVS_socket, 3);

	SVVS_inited = true;


	Con_Printf("VC server is running\n");
	SockadrToNetadr((struct sockaddr_qstorage*)&address, &adr);
	Info_SetValueForKey(svs.info, "voiceaddress", NET_AdrToString(adr), MAX_SERVERINFO_STRING);
	return;
}

//currently a dum forwarding/broadcasting mechanism
void SVVC_Frame (qboolean running)
{
	struct sockaddr_in frm;
	int size = sizeof(frm);
	int newcl;
	int i, j;
	char buffer[1400];

	if (!running)
		return;

	if (!SVVS_socket)
	{
		SVVC_ServerInit();
		return;
	}
	newcl = accept(SVVS_socket, (struct sockaddr *)&frm, &size);
	if (newcl != INVALID_SOCKET)
	{
		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if (!svs.clients[i].voicechat.socket)	//this really isn't the right way...
			{
				svs.clients[i].voicechat.socket = newcl;
				break;
			}
		}
	}

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		host_client = &svs.clients[i];

		if (host_client->voicechat.socket)
		{
			size = recv(host_client->voicechat.socket, buffer, sizeof(buffer), 0);
			if (size > 0)
			{
				for (j = 0; j < MAX_CLIENTS; j++)
				{
					if (j != i && svs.clients[j].voicechat.socket)	//gotta be capable of receiving, and not the sender (that would be dumb).
						send(svs.clients[j].voicechat.socket, buffer, size, 0);
				}
			}
		}
	}
}
#endif

#ifndef SERVERONLY

sfxcache_t *voicesoundbuffer[2];
sfx_t recordaudio[2] = {
	{"recordaudio1",
	{NULL, false},
	NULL},
	{"recordaudio2",
	{NULL, false},
	NULL}
};
int audiobuffer;
void SNDVC_Submit(int codec, qbyte *buffer, int samples, int freq, int width)
{
	S_RawAudio(0, buffer, freq, samples, 1, width);
		/*
	soundcardinfo_t *cursndcard;
	audiobuffer=0;
	if (!recordaudio[audiobuffer].cache.data)
	{
		voicesoundbuffer[audiobuffer] = BZ_Malloc(44100*2+sizeof(sfxcache_t));					
		recordaudio[audiobuffer].cache.data = voicesoundbuffer[audiobuffer];
		recordaudio[audiobuffer].cache.fake = true;
	}

	cursndcard = sndcardinfo;
	if (!cursndcard)
	{
		Con_Printf("Accepting voice chat with no sound card\n");
		return;
	}
	voicesoundbuffer[audiobuffer]->length = samples;
	voicesoundbuffer[audiobuffer]->stereo = false;
	voicesoundbuffer[audiobuffer]->speed = sndcardinfo->sn.speed;
	voicesoundbuffer[audiobuffer]->width = width;
	voicesoundbuffer[audiobuffer]->loopstart=-1;

//	Con_DPrintf("Submit %i\n", (int)samples);

	if (codec == 0)	//codec 0 is special. (A straight copy)
		ResampleSfx(&recordaudio[audiobuffer], freq, width, buffer);
	else
	{
		qbyte *temp = BZ_Malloc(samples*width);
		audiocodecs[codec].decode(buffer, (short*)temp, samples);
		ResampleSfx(&recordaudio[audiobuffer], freq, width, temp);
		BZ_Free(temp);
	}

	for (cursndcard = sndcardinfo; cursndcard; cursndcard=cursndcard->next)
	{
		if (0&&cursndcard->channel[MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS+1-audiobuffer].sfx == &recordaudio[1-audiobuffer])	//other buffer is playing.
		{
			cursndcard->channel[MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS+audiobuffer].sfx = &recordaudio[audiobuffer];
			cursndcard->channel[MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS+audiobuffer].pos = cursndcard->channel[MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS+1-audiobuffer].end - voicesoundbuffer[1-audiobuffer]->length- cursndcard->channel[MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS+1-audiobuffer].pos;
			cursndcard->channel[MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS+audiobuffer].end = cursndcard->channel[MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS+audiobuffer].end + samples;

			if (cursndcard->channel[MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS+audiobuffer].pos >= voicesoundbuffer[audiobuffer]->length)
			{
				cursndcard->channel[MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS+1-audiobuffer].pos = voicesoundbuffer[1-audiobuffer]->length;
				cursndcard->channel[MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS+audiobuffer].pos = 0;
				Con_Printf("Sound out of sync\n");
			}
		}
		else
		{
			cursndcard->channel[MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS+audiobuffer].sfx = &recordaudio[audiobuffer];
			cursndcard->channel[MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS+audiobuffer].pos = 0;
			cursndcard->channel[MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS+audiobuffer].end = cursndcard->paintedtime + samples;
		}
	}
	audiobuffer = 1-audiobuffer;*/
}















void CLVC_SetServer (char *addy)
{
	unsigned long _true = true;
	struct sockaddr_qstorage	from;
	

	if ((CLVS_socket = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		Sys_Error ("FTP_UDP_OpenSocket: socket: %s\n", strerror(qerrno));
	}


	{//quake routines using dns and stuff (Really, I wanna keep quake and ftp fairly seperate)
		netadr_t qaddy;		
		if (!NET_StringToAdr (addy, &qaddy))	//server doesn't exist.
			return;
		if (qaddy.type != NA_IP)	//Only TCP is supported.
			return;
		if (!qaddy.port)
			qaddy.port = htons(PORT_SERVER);
		NetadrToSockadr(&qaddy, &from);

	}

	//not yet non blocking.
	if (connect(CLVS_socket, (struct sockaddr*)&from, sizeof(from)) == -1)
	{
		Con_Printf ("FTP_TCP_OpenSocket: connect: %i %s\n", qerrno, strerror(qerrno));
		closesocket(CLVS_socket);		
		CLVS_socket = 0;
		return;
	}
	
	if (ioctlsocket (CLVS_socket, FIONBIO, &_true) == -1)	//now make it non blocking.
	{
		Sys_Error ("FTP_TCP_OpenSocket: ioctl FIONBIO: %s\n", strerror(qerrno));
	}
}

void CLVC_Disconnect (void)
{
	closesocket(CLVS_socket);		
	CLVS_socket = 0;
}

void CLVC_Poll (void)
{
	int codec;
	int size;

	if (!CLVS_socket)
		return;
	
	while (1)
	{
		size = recv(CLVS_socket, &inputbuffer[readpoint], sizeof(inputbuffer) - readpoint, 0);
		if (size>0)
		{
			int samps;
			int bytes;
			readpoint+=size;
			if (readpoint >= 1)
			{
				codec = *inputbuffer;

				if (codec >= 0 && codec <= 127 && readpoint>=5)	//just in case.
				{
					samps = LittleLong(*(signed short *)(inputbuffer+1));
					bytes = LittleLong(*(unsigned short *)(inputbuffer+3));
//					Con_DPrintf("read %i\n", size);
					if (samps < 0)	//something special
					{
						readpoint=0;
					}
					else
					{
						if (readpoint >= bytes+5)
						{
							if (codec == 1)
								Con_Printf("Reading\n");
							if (codec < audionumcodecs && audiocodecs[codec].decode)
							{
								SNDVC_Submit(codec, ((qbyte *)inputbuffer)+5, samps, 11025, 2);
								readpoint -= bytes+5;
								memmove(inputbuffer, &inputbuffer[readpoint+bytes+5], readpoint);
							}
							else
							{
								Con_Printf("Bad codec %i\n", (int)codec);
								readpoint=0;
								closesocket(CLVS_socket);
								CLVS_socket = 0;
							}
						}
					}
				}
			}
		}
		else if (readpoint >= sizeof(inputbuffer) || readpoint < 0)
		{
			Con_Printf("Too small buffer or extended client %i\n", (int)readpoint);
			readpoint=0;
			closesocket(CLVS_socket);
			CLVS_socket = 0;
		}
		else
		{
			break;
		}
	}
}

void SNDVC_MicInput(qbyte *buffer, int samples, int freq, int width)	//this correctly buffers data ready to be sent.
{
	int sent;
	qbyte codec;
	unsigned short *sampleswritten;
	samples/=width;
	if (!CLVS_socket)
	{
		if (cls.state)
		{
			char *server;
			server = Info_ValueForKey(cl.serverinfo, "voiceaddress");
			if (*server)
				CLVC_SetServer(server);
		}

		SNDVC_Submit(0, buffer, samples, freq, width);
		return;
	}
	else if (!cls.state)
	{
		readpoint = 0;
		sendpoint= 0;
		SNDVC_Submit(0, buffer, samples, freq, width);
		return;
	}

	SNDVC_Submit(0, buffer, samples, freq, width);	//remembering at all times that we will not be allowed to hear it ourselves.

	//add to send buffer.
	if (samples > 0x7ffe)
		samples = 0x7ffe;

	if (sendpoint + samples*width+sizeof(unsigned char)+sizeof(short)+sizeof(*sampleswritten) < sizeof(outputbuffer))
	{
//		Con_DPrintf("sending %i\n", (int)samples);
		codec = 1;
		outputbuffer[sendpoint] = codec; sendpoint += sizeof(unsigned char);
		*(unsigned short*)(&outputbuffer[sendpoint]) = samples; sendpoint += sizeof(unsigned short);
		sampleswritten = (short *)&outputbuffer[sendpoint]; sendpoint += sizeof(*sampleswritten);
		*sampleswritten = audiocodecs[codec].encode((short *)buffer, &outputbuffer[sendpoint], samples);
		sendpoint += *sampleswritten;
	}
	else
	{
		Con_Printf("Connection overflowing\n");
	}

//try and send it
	sent = send(CLVS_socket, outputbuffer, sendpoint, 0);
	if (sent > 0)
	{
//		Con_DPrintf("sent %i\n", (int)sent);
		sendpoint -= sent;
	}
	else
	{
CLVS_socket=0;
	}


//	SNDVC_Submit(buffer, samples, freq, width);
}

#endif
#endif
