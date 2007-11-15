/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


//connection notes
//The connection is like http.
//The stream starts with a small header.
//The header is a list of 'key: value' pairs, seperated by new lines.
//The header ends with a totally blank line.
//to record an mvd from telnet or somesuch, you would use:
//"QTV\nRAW: 1\n\n"

//VERSION: a list of the different qtv protocols supported. Multiple versions can be specified. The first is assumed to be the prefered version.
//RAW: if non-zero, send only a raw mvd with no additional markup anywhere (for telnet use). Doesn't work with challenge-based auth, so will only be accepted when proxy passwords are not required.
//AUTH: specifies an auth method, the exact specs varies based on the method
//		PLAIN: the password is sent as a PASSWORD line
//		MD4: the server responds with an "AUTH: MD4\n" line as well as a "CHALLENGE: somerandomchallengestring\n" line, the client sends a new 'initial' request with CHALLENGE: MD4\nRESPONSE: hexbasedmd4checksumhere\n"
//		MD5: same as md4
//		CCITT: same as md4, but using the CRC stuff common to all quake engines.
//		if the supported/allowed auth methods don't match, the connection is silently dropped.
//SOURCE: which stream to play from, DEFAULT is special. Without qualifiers, it's assumed to be a tcp address.
//COMPRESSION: Suggests a compression method (multiple are allowed). You'll get a COMPRESSION response, and compression will begin with the binary data.
//SOURCELIST: Asks for a list of active sources from the proxy.
//DEMOLIST:	Asks for a list of available mvd demos.

//Response:
//if using RAW, there will be no header or anything
//Otherwise you'll get a QTVSV %f response (%f being the protocol version being used)
//same structure, terminated by a \n
//AUTH: Server requires auth before proceeding. If you don't support the method the server says, then, urm, the server shouldn't have suggested it.
//CHALLENGE: used with auth
//COMPRESSION: Method of compression used. Compression begins with the raw data after the connection process.
//ASOURCE: names a source
//ADEMO: gives a demo file name




#include "qtv.h"

#ifndef _WIN32
#include <signal.h>
#endif

#define RECONNECT_TIME (1000*30)
#define RECONNECT_TIME_DEMO (1000*5)
#define UDPRECONNECT_TIME (1000)
#define PINGSINTERVAL_TIME (1000*5)
#define UDPTIMEOUT_LENGTH (1000*20)
#define UDPPACKETINTERVAL (1000/72)

void Net_SendConnectionMVD(sv_t *qtv, oproxy_t *prox);
void Net_QueueUpstream(sv_t *qtv, int size, char *buffer);


qboolean	NET_StringToAddr (char *s, netadr_t *sadr, int defaultport)
{
	struct hostent	*h;
	char	*colon;
	char	copy[128];

	memset (sadr, 0, sizeof(netadr_t));

#ifdef USEIPX
	if ((strlen(s) >= 23) && (s[8] == ':') && (s[21] == ':'))	// check for an IPX address
	{
		unsigned int val;

		((struct sockaddr_ipx *)sadr)->sa_family = AF_IPX;
		copy[2] = 0;
		DO(0, sa_netnum[0]);
		DO(2, sa_netnum[1]);
		DO(4, sa_netnum[2]);
		DO(6, sa_netnum[3]);
		DO(9, sa_nodenum[0]);
		DO(11, sa_nodenum[1]);
		DO(13, sa_nodenum[2]);
		DO(15, sa_nodenum[3]);
		DO(17, sa_nodenum[4]);
		DO(19, sa_nodenum[5]);
		sscanf (&s[22], "%u", &val);
		((struct sockaddr_ipx *)sadr)->sa_socket = htons((unsigned short)val);
	}
	else
#endif
#if 0//def IPPROTO_IPV6
		if (getaddrinfo)
	{//ipv6 method (can return ipv4 addresses too)
		struct addrinfo *addrinfo, *pos;
		struct addrinfo udp6hint;
		int error;
		char *port;
		char dupbase[256];
		int len;

		memset(&udp6hint, 0, sizeof(udp6hint));
		udp6hint.ai_family = 0;//Any... we check for AF_INET6 or 4
		udp6hint.ai_socktype = SOCK_DGRAM;
		udp6hint.ai_protocol = IPPROTO_UDP;

		port = s + strlen(s);
		while(port >= s)
		{
			if (*port == ':')
			break;
			port--;
		}

		if (port == s)
			port = NULL;
		if (port)
		{
			len = port - s;
			if (len >= sizeof(dupbase))
				len = sizeof(dupbase)-1;
			strncpy(dupbase, s, len);
			dupbase[len] = '\0';
			error = getaddrinfo(dupbase, port+1, &udp6hint, &addrinfo);
		}
		else
			error = EAI_NONAME;
		if (error)	//failed, try string with no port.
			error = getaddrinfo(s, NULL, &udp6hint, &addrinfo);	//remember, this func will return any address family that could be using the udp protocol... (ip4 or ip6)
		if (error)
		{
			return false;
		}
		((struct sockaddr*)sadr)->sa_family = 0;
		for (pos = addrinfo; pos; pos = pos->ai_next)
		{
			switch(pos->ai_family)
			{
			case AF_INET6:
				if (((struct sockaddr_in *)sadr)->sin_family == AF_INET6)
					break;	//first one should be best...
				//fallthrough
			case AF_INET:
				memcpy(sadr, addrinfo->ai_addr, addrinfo->ai_addrlen);
				if (pos->ai_family == AF_INET)
					goto dblbreak;	//don't try finding any more, this is quake, they probably prefer ip4...
				break;
			}
		}
dblbreak:
		pfreeaddrinfo (addrinfo);
		if (!((struct sockaddr*)sadr)->sa_family)	//none suitablefound
			return false;
	}
	else
#endif
	{ //old fashioned method
		((struct sockaddr_in *)sadr)->sin_family = AF_INET;

		((struct sockaddr_in *)sadr)->sin_port = htons(defaultport);

		strcpy (copy, s);
		// strip off a trailing :port if present
		for (colon = copy ; *colon ; colon++)
			if (*colon == ':')
			{
				*colon = 0;
				((struct sockaddr_in *)sadr)->sin_port = htons((short)atoi(colon+1));
			}

		if (copy[0] >= '0' && copy[0] <= '9')	//this is the wrong way to test. a server name may start with a number.
		{
			*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
		}
		else
		{
			if (! (h = gethostbyname(copy)) )
				return 0;
			if (h->h_addrtype != AF_INET)
				return 0;
			*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
		}
	}

	return true;
}

qboolean Net_CompareAddress(netadr_t *s1, netadr_t *s2, int qp1, int qp2)
{
	struct sockaddr_in *i1=(void*)s1, *i2=(void*)s2;
	if (i1->sin_family != i2->sin_family)
		return false;
	if (i1->sin_family == AF_INET)
	{
		if (*(unsigned int*)&i1->sin_addr != *(unsigned int*)&i2->sin_addr)
			return false;
		if (i1->sin_port != i2->sin_port && qp1 != qp2)	//allow qports to match instead of ports, if required.
			return false;
		return true;
	}
	return false;
}

SOCKET Net_MVDListen(int port)
{
	SOCKET sock;

	struct sockaddr_in	address;
//	int fromlen;

	unsigned long nonblocking = true;

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons((u_short)port);



	if ((sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
	{
		return INVALID_SOCKET;
	}

	if (ioctlsocket (sock, FIONBIO, &nonblocking) == -1)
	{
		closesocket(sock);
		return INVALID_SOCKET;
	}

	if( bind (sock, (void *)&address, sizeof(address)) == -1)
	{
		closesocket(sock);
		return INVALID_SOCKET;
	}

	listen(sock, 2);	//don't listen for too many clients.

	return sock;
}

char *strchrrev(char *str, char chr)
{
	char *firstchar = str;
	for (str = str + strlen(str)-1; str>=firstchar; str--)
		if (*str == chr)
			return str;

	return NULL;
}

void Net_SendQTVConnectionRequest(sv_t *qtv, char *authmethod, char *challenge)
{
	char *at;
	char *str;
	char hash[512];

	//due to mvdsv sucking and stuff, we try using raw connections where possibleso that we don't end up expecting a header.
	//at some point, this will be forced to 1
	qtv->parsingqtvheader = true;//!!*qtv->connectpassword;
	qtv->buffersize = 0;
	qtv->forwardpoint = 0;

	str =	"QTV\n";			Net_QueueUpstream(qtv, strlen(str), str);
	str =	"VERSION: 1\n";		Net_QueueUpstream(qtv, strlen(str), str);

	if (qtv->serverquery)
	{
		if (qtv->serverquery == 2)
		{
			str =	"DEMOLIST\n";		Net_QueueUpstream(qtv, strlen(str), str);
		}
		else
		{
			str =	"SOURCELIST\n";		Net_QueueUpstream(qtv, strlen(str), str);
		}
	}
	else
	{

		at = strchrrev(qtv->server, '@');
		if (at)
		{
			*at = '\0';
			str =	"SOURCE: ";		Net_QueueUpstream(qtv, strlen(str), str);

			if (strncmp(qtv->server, "tcp:", 4))
			{
				str = qtv->server;
				Net_QueueUpstream(qtv, strlen(str), str);
			}
			else
			{
				str = strchr(qtv->server, ':');
				if (str)
				{
					str++;
					Net_QueueUpstream(qtv, strlen(str), str);
				}
			}

			str =	"\n";			Net_QueueUpstream(qtv, strlen(str), str);
			*at = '@';
		}
		else
		{
			str =	"RECEIVE\n";	Net_QueueUpstream(qtv, strlen(str), str);
		}

		if (!qtv->parsingqtvheader)
		{
			str =	"RAW: 1\n";		Net_QueueUpstream(qtv, strlen(str), str);
		}
		else
		{
			if (authmethod)
			{
				if (!strcmp(authmethod, "PLAIN"))
				{
					str = "AUTH: PLAIN\n";	Net_QueueUpstream(qtv, strlen(str), str);
					str = "PASSWORD: \"";	Net_QueueUpstream(qtv, strlen(str), str);
					str = qtv->connectpassword;	Net_QueueUpstream(qtv, strlen(str), str);
					str = "\"\n";			Net_QueueUpstream(qtv, strlen(str), str);
				}
				else if (challenge && strlen(challenge)>=32 && !strcmp(authmethod, "CCITT"))
				{
					unsigned short crcvalue;
					str = "AUTH: CCITT\n";	Net_QueueUpstream(qtv, strlen(str), str);
					str = "PASSWORD: \"";	Net_QueueUpstream(qtv, strlen(str), str);

					snprintf(hash, sizeof(hash), "%s%s", challenge, qtv->connectpassword);
					crcvalue = QCRC_Block(hash, strlen(hash));
					sprintf(hash, "0x%X", (unsigned int)QCRC_Value(crcvalue));

					str = hash;				Net_QueueUpstream(qtv, strlen(str), str);
					str = "\"\n";			Net_QueueUpstream(qtv, strlen(str), str);
				}
				else if (challenge && strlen(challenge)>=8 && !strcmp(authmethod, "MD4"))
				{
					unsigned int md4sum[4];
					str = "AUTH: MD4\n";	Net_QueueUpstream(qtv, strlen(str), str);
					str = "PASSWORD: \"";	Net_QueueUpstream(qtv, strlen(str), str);

					snprintf(hash, sizeof(hash), "%s%s", challenge, qtv->connectpassword);
					Com_BlockFullChecksum (hash, strlen(hash), (unsigned char*)md4sum);
					sprintf(hash, "%X%X%X%X", md4sum[0], md4sum[1], md4sum[2], md4sum[3]);

					str = hash;				Net_QueueUpstream(qtv, strlen(str), str);
					str = "\"\n";			Net_QueueUpstream(qtv, strlen(str), str);
				}
				else if (!strcmp(authmethod, "NONE"))
				{
					str = "AUTH: NONE\n";	Net_QueueUpstream(qtv, strlen(str), str);
					str = "PASSWORD: \n";	Net_QueueUpstream(qtv, strlen(str), str);
				}
				else
				{
					qtv->drop = true;
					qtv->upstreambuffersize = 0;
					Sys_Printf(qtv->cluster, "Auth method %s was not usable\n", authmethod);
					return;
				}
			}
			else
			{
				str = "AUTH: MD4\n";		Net_QueueUpstream(qtv, strlen(str), str);
				str = "AUTH: CCITT\n";		Net_QueueUpstream(qtv, strlen(str), str);
				str = "AUTH: PLAIN\n";		Net_QueueUpstream(qtv, strlen(str), str);
				str = "AUTH: NONE\n";		Net_QueueUpstream(qtv, strlen(str), str);
			}
		}
	}
	str =	"\n";		Net_QueueUpstream(qtv, strlen(str), str);
}

qboolean Net_ConnectToTCPServer(sv_t *qtv, char *ip)
{
	int err;
	netadr_t from;
	unsigned long nonblocking = true;

	if (!NET_StringToAddr(ip, &qtv->serveraddress, 27500))
	{
		Sys_Printf(qtv->cluster, "Unable to resolve %s\n", ip);
		return false;
	}
	qtv->sourcesock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (qtv->sourcesock == INVALID_SOCKET)
		return false;

	memset(&from, 0, sizeof(from));
	((struct sockaddr*)&from)->sa_family = ((struct sockaddr*)&qtv->serveraddress)->sa_family;
	if (bind(qtv->sourcesock, (struct sockaddr *)&from, sizeof(from)) == -1)
	{
		closesocket(qtv->sourcesock);
		qtv->sourcesock = INVALID_SOCKET;
		return false;
	}

	if (ioctlsocket (qtv->sourcesock, FIONBIO, &nonblocking) == -1)
	{
		closesocket(qtv->sourcesock);
		qtv->sourcesock = INVALID_SOCKET;
		return false;
	}

	if (connect(qtv->sourcesock, (struct sockaddr *)&qtv->serveraddress, sizeof(qtv->serveraddress)) == INVALID_SOCKET)
	{
		err = qerrno;
		if (err != EINPROGRESS && err != EAGAIN && err != EWOULDBLOCK)	//bsd sockets are meant to return EINPROGRESS, but some winsock drivers use EWOULDBLOCK instead. *sigh*...
		{
			closesocket(qtv->sourcesock);
			qtv->sourcesock = INVALID_SOCKET;
			return false;
		}
	}

	//read the notes at the start of this file for what these text strings mean
	Net_SendQTVConnectionRequest(qtv, NULL, NULL);
	return true;
}
qboolean Net_ConnectToUDPServer(sv_t *qtv, char *ip)
{
	netadr_t from;
	unsigned long nonblocking = true;

	if (!NET_StringToAddr(ip, &qtv->serveraddress, 27500))
	{
		Sys_Printf(qtv->cluster, "Unable to resolve %s\n", ip);
		return false;
	}
	qtv->sourcesock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (qtv->sourcesock == INVALID_SOCKET)
		return false;

	memset(&from, 0, sizeof(from));
	((struct sockaddr*)&from)->sa_family = ((struct sockaddr*)&qtv->serveraddress)->sa_family;
	if (bind(qtv->sourcesock, (struct sockaddr *)&from, sizeof(from)) == -1)
	{
		closesocket(qtv->sourcesock);
		qtv->sourcesock = INVALID_SOCKET;
		return false;
	}

	if (ioctlsocket (qtv->sourcesock, FIONBIO, &nonblocking) == -1)
	{
		closesocket(qtv->sourcesock);
		qtv->sourcesock = INVALID_SOCKET;
		return false;
	}

	qtv->qport = Sys_Milliseconds()*1000+Sys_Milliseconds();

	return true;
}

qboolean DemoFilenameIsOkay(char *fname)
{
	int len;
	if (strchr(fname, '/'))
		return false;	//unix path seperator
	if (strchr(fname, '\\'))
		return false;	//windows path seperator
	if (strchr(fname, ':'))
		return false;	//mac path seperator

	//now make certain that the last four characters are '.mvd' and not something like '.cfg' perhaps
	len = strlen(fname);
	if (len < 5)
		return false;
	if (strcmp(fname+len-4, ".mvd"))
		return false;

	return true;

/*
	if (strchr(fname, '\\'))
	{
		char *s;
		Con_Printf("Warning: \\ charactures in filename %s\n", fname);
		while((s = strchr(fname, '\\')))
			*s = '/';
	}

	if (strstr(fname, ".."))
	{
		Con_Printf("Error: '..' charactures in filename %s\n", fname);
	}
	else if (fname[0] == '/')
	{
		Con_Printf("Error: absolute path in filename %s\n", fname);
	}
	else if (strstr(fname, ":")) //win32 drive seperator (or mac path seperator, but / works there and they're used to it)
	{
		Con_Printf("Error: absolute path in filename %s\n", fname);
	}
	else
		return false;
	return true;
*/
}

qboolean Net_ConnectToServer(sv_t *qtv)
{
	char *at;
	sourcetype_t type = SRC_BAD;
	char *ip = qtv->server;

	if (!strncmp(ip, "udp:", 4))
	{
		type = SRC_UDP;
		ip += 4;
	}
	else if (!strncmp(ip, "tcp:", 4))
	{
		type = SRC_TCP;
		ip += 4;
	}
	else if (!strncmp(ip, "demo:", 5))
	{
		type = SRC_DEMO;
		ip += 5;
	}
	else if (!strncmp(ip, "file:", 5))
	{
		type = SRC_DEMO;
		ip += 5;
	}

	at = strchrrev(ip, '@');
	if (at && (type == SRC_DEMO || type == SRC_TCP))
	{
		if (type == SRC_DEMO)
			type = SRC_TCP;
		ip = at+1;
	}

	qtv->usequakeworldprotocols = false;

	if (qtv->sourcetype == SRC_DEMO)
		qtv->nextconnectattempt = qtv->curtime + RECONNECT_TIME_DEMO;	//wait half a minuite before trying to reconnect
	else
		qtv->nextconnectattempt = qtv->curtime + RECONNECT_TIME;	//wait half a minuite before trying to reconnect

	qtv->sourcetype = type;

	switch(type)
	{
	case SRC_DEMO:
		qtv->sourcesock = INVALID_SOCKET;
		if (DemoFilenameIsOkay(ip))
		{
			char fullname[512];
			snprintf(fullname, sizeof(fullname), "%s%s", qtv->cluster->demodir, ip);
			qtv->sourcefile = fopen(fullname, "rb");
		}
		else
			qtv->sourcefile = NULL;
		if (qtv->sourcefile)
		{
			char smallbuffer[17];
			fseek(qtv->sourcefile, 0, SEEK_END);
			qtv->filelength = ftell(qtv->sourcefile);

			//attempt to detect the end of the file
			fseek(qtv->sourcefile, -sizeof(smallbuffer), SEEK_CUR);
			fread(smallbuffer, 1, 17, qtv->sourcefile);
			//0 is the time
			if (smallbuffer[1] == dem_all || smallbuffer[1] == dem_read) //mvdsv changed it to read...
			{
				//2,3,4,5 are the length
				if (smallbuffer[6] == svc_disconnect)
				{
					if (!strcmp(smallbuffer+7, "EndOfDemo"))
					{
						qtv->filelength -= 17;
					}
				}
			}

			fseek(qtv->sourcefile, 0, SEEK_SET);
			return true;
		}
		Sys_Printf(qtv->cluster, "Unable to open file %s\n", ip);
		return false;


	case SRC_UDP:
		qtv->usequakeworldprotocols = true;
		return Net_ConnectToUDPServer(qtv, ip);
	
	case SRC_TCP:
		return Net_ConnectToTCPServer(qtv, ip);
	
	default:
		Sys_Printf(qtv->cluster, "Unknown source type %s\n", ip);
		return false;
	}
}

void Net_QueueUpstream(sv_t *qtv, int size, char *buffer)
{
	if (qtv->usequakeworldprotocols)
		return;

	if (qtv->upstreambuffersize + size > sizeof(qtv->upstreambuffer))
	{
		Sys_Printf(qtv->cluster, "Upstream queue overflowed for %s\n", qtv->server);
		qtv->drop = true;
		return;
	}
	memcpy(qtv->upstreambuffer + qtv->upstreambuffersize, buffer, size);
	qtv->upstreambuffersize += size;
}

qboolean Net_WriteUpstream(sv_t *qtv)
{
	int len;

	if (qtv->upstreambuffersize && qtv->sourcesock != INVALID_SOCKET)
	{
		len = send(qtv->sourcesock, qtv->upstreambuffer, qtv->upstreambuffersize, 0);
		if (len == 0)
			return false;
		if (len < 0)
		{
			int err = qerrno;
			if (err != EWOULDBLOCK && err != EAGAIN && err != ENOTCONN)
			{
				int err;
				err = qerrno;
				if (qerrno)
					Sys_Printf(qtv->cluster, "Error: source socket error %i\n", qerrno);
				else
					Sys_Printf(qtv->cluster, "Error: server %s disconnected\n", qtv->server);
				qtv->drop = true;
			}
			return false;
		}
		qtv->upstreambuffersize -= len;
		memmove(qtv->upstreambuffer, qtv->upstreambuffer + len, qtv->upstreambuffersize);
	}
	return true;
}

void SV_SendUpstream(sv_t *qtv, netmsg_t *nm)
{
	char size[2];

	size[0] = (nm->cursize&0x00ff)>>0;
	size[1] = (nm->cursize&0xff00)>>8;
	Net_QueueUpstream(qtv, 2, size);
	Net_QueueUpstream(qtv, nm->cursize, nm->data);
	Net_WriteUpstream(qtv);	//try and flush it
}

int SV_SayToUpstream(sv_t *qtv, char *message)
{
	char buffer[1024];
	netmsg_t nm;

	if (!qtv->upstreamacceptschat)
	{
#ifndef _MSC_VER
#warning This is incomplete!
#endif
		//Sys_Printf(qtv->cluster, "not forwarding say\n"); 
		return 0;
	}

	InitNetMsg(&nm, buffer, sizeof(buffer));

	WriteByte(&nm, qtv_clc_stringcmd);
	WriteString2(&nm, "say ");
	WriteString(&nm, message);
	SV_SendUpstream(qtv, &nm);

	return 1;
}

void SV_SayToViewers(sv_t *qtv, char *message)
{
	Fwd_SayToDownstream(qtv, message);
#ifndef _MSC_VER
	#warning Send to viewers here too
#endif
}

//This function 1: parses the 'don't delay' packets in the stream
//              2: returns the length of continuous data (that is, whole-packet bytes that have not been truncated by the networking layer)
//                 this means we know that the client proxies have valid data, at least from our side.
int SV_EarlyParse(sv_t *qtv, unsigned char *buffer, int remaining)
{
	int lengthofs;
	int length;
	int available = 0;
	while(1)
	{
		if (remaining < 2)
			return available;

		//buffer[0] is time

		switch (buffer[1]&dem_mask)
		{
		case dem_set:
			lengthofs = 0;	//to silence gcc, nothing more
			break;
		case dem_multiple:
			lengthofs = 6;
			break;
		default:
			lengthofs = 2;
			break;
		}

		if (lengthofs > 0)
		{
			if (lengthofs+4 > remaining)
				return available;

			length = (buffer[lengthofs]<<0) + (buffer[lengthofs+1]<<8) + (buffer[lengthofs+2]<<16) + (buffer[lengthofs+3]<<24);

			length += lengthofs+4;
			if (length > MAX_MSGLEN)
				printf("Probably corrupt mvd (length %i)\n", length);
		}
		else
			length = 10;

		if (remaining < length)
			return available;

		if ((buffer[1]&dem_mask) == dem_all && (buffer[1] & ~dem_mask) && qtv->sourcetype != SRC_DEMO)	//dem_qtvdata
		{
			ParseMessage(qtv, buffer+lengthofs+4, length - (lengthofs+4), buffer[1], 0xffffffff);
		}
		
		remaining -= length;
		available += length;
		buffer += length;
	}
}

qboolean Net_ReadStream(sv_t *qtv)
{
	int maxreadable;
	int read;
	void *buffer;
	int err;

	maxreadable = MAX_PROXY_BUFFER - qtv->buffersize;
	if (!maxreadable)
		return true;	//this is bad!
	buffer = qtv->buffer + qtv->buffersize;

	if (qtv->sourcefile)
	{
		if (maxreadable > PREFERED_PROXY_BUFFER-qtv->buffersize)
			maxreadable = PREFERED_PROXY_BUFFER-qtv->buffersize;
		if (maxreadable<=0)
			return true;

		//reuse read a little...
		read = ftell(qtv->sourcefile);
		if (read+maxreadable > qtv->filelength)
			maxreadable = qtv->filelength-read;	//clamp to the end of the file
								//even if that 'end' is before the svc_disconnect

		read = fread(buffer, 1, maxreadable, qtv->sourcefile);
	}
	else
	{
		unsigned int errsize;
		errsize = sizeof(err);
		err = 0;
		getsockopt(qtv->sourcesock, SOL_SOCKET, SO_ERROR, (char*)&err, &errsize);
		if (err == ECONNREFUSED)
		{
			Sys_Printf(qtv->cluster, "Error: server %s refused connection\n", qtv->server);
			closesocket(qtv->sourcesock);
			qtv->sourcesock = INVALID_SOCKET;
			qtv->upstreambuffersize = 0;	//probably contains initial connection request info
			return false;
		}

		read = recv(qtv->sourcesock, buffer, maxreadable, 0);
	}
	if (read > 0)
	{
		qtv->buffersize += read;
		if (!qtv->cluster->lateforward && !qtv->parsingqtvheader)	//qtv header being the auth part of the connection rather than the stream
		{
			int forwardable;
			//this has the effect of not only parsing early packets, but also saying how much complete data there is.
			forwardable = SV_EarlyParse(qtv, qtv->buffer+qtv->forwardpoint, qtv->buffersize - qtv->forwardpoint);
			if (forwardable > 0)
			{
				SV_ForwardStream(qtv, qtv->buffer+qtv->forwardpoint, forwardable);
				qtv->forwardpoint += forwardable;
			}
		}
	}
	else
	{
		if (read == 0)
			err = 0;
		else
			err = qerrno;
		if (read == 0 || (err != EWOULDBLOCK && err != EAGAIN && err != ENOTCONN))	//ENOTCONN can be returned whilst waiting for a connect to finish.
		{
			if (qtv->sourcefile)
				Sys_Printf(qtv->cluster, "Error: End of file\n");
			else if (read)
				Sys_Printf(qtv->cluster, "Error: source socket error %i\n", qerrno);
			else
				Sys_Printf(qtv->cluster, "Error: server %s disconnected\n", qtv->server);
			if (qtv->sourcesock != INVALID_SOCKET)
			{
				closesocket(qtv->sourcesock);
				qtv->sourcesock = INVALID_SOCKET;
			}
			if (qtv->sourcefile)
			{
				fclose(qtv->sourcefile);
				qtv->sourcefile = NULL;
			}
			return false;
		}
	}
	return true;
}

#define BUFFERTIME 10	//secords for artificial delay, so we can buffer things properly.

unsigned int Sys_Milliseconds(void)
{
#ifdef _WIN32
	#ifdef _MSC_VER
		#pragma comment(lib, "winmm.lib")
	#endif

#if 0
	static firsttime = 1;
	static starttime;
	if (firsttime)
	{
		starttime = timeGetTime() + 1000*20;
		firsttime = 0;
	}
	return timeGetTime() - starttime;
#endif



	return timeGetTime();
#else
	//assume every other system follows standards.
	unsigned int t;
	struct timeval tv;

	gettimeofday(&tv, NULL);
	t = ((unsigned int)tv.tv_sec)*1000 + (((unsigned int)tv.tv_usec)/1000);
	return t;
#endif
}
/*
void NetSleep(sv_t *tv)
{
	int m;
	struct timeval timeout;
	fd_set socketset;

	FD_ZERO(&socketset);
	m = 0;
	if (tv->sourcesock != INVALID_SOCKET)
	{
		FD_SET(tv->sourcesock, &socketset);
		if (tv->sourcesock >= m)
			m = tv->sourcesock+1;
	}
	if (tv->qwdsocket != INVALID_SOCKET)
	{
		FD_SET(tv->qwdsocket, &socketset);
		if (tv->sourcesock >= m)
			m = tv->sourcesock+1;
	}

#ifndef _WIN32
	#ifndef STDIN
		#define STDIN 0
	#endif
	FD_SET(STDIN, &socketset);
	if (STDIN >= m)
		m = STDIN+1;
#endif

	timeout.tv_sec = 100/1000;
	timeout.tv_usec = (100%1000)*1000;

	select(m, &socketset, NULL, NULL, &timeout);

#ifdef _WIN32
	for (;;)
	{
		char buffer[8192];
		char *result;
		char c;

		if (!kbhit())
			break;
		c = getch();

		if (c == '\n' || c == '\r')
		{
			printf("\n");
			if (tv->inputlength)
			{
				tv->commandinput[tv->inputlength] = '\0';
				result = Rcon_Command(tv->cluster, tv, tv->commandinput, buffer, sizeof(buffer), true);
				printf("%s", result);
				tv->inputlength = 0;
				tv->commandinput[0] = '\0';
			}
		}
		else if (c == '\b')
		{
			if (tv->inputlength > 0)
			{
				tv->inputlength--;
				tv->commandinput[tv->inputlength] = '\0';
			}
		}
		else
		{
			if (tv->inputlength < sizeof(tv->commandinput)-1)
			{
				tv->commandinput[tv->inputlength++] = c;
				tv->commandinput[tv->inputlength] = '\0';
			}
		}if (FD_ISSET(STDIN, &socketset))
		printf("\r%s \b", tv->commandinput);
	}
#else
	if (FD_ISSET(STDIN, &socketset))
	{
		char buffer[8192];
		char *result;
		tv->inputlength = read (0, tv->commandinput, sizeof(tv->commandinput));
		if (tv->inputlength >= 1)
		{
			tv->commandinput[tv->inputlength-1] = 0;        // rip off the /n and terminate

			if (tv->inputlength)
			{
				tv->commandinput[tv->inputlength] = '\0';
				result = Rcon_Command(tv, tv->commandinput, buffer, sizeof(buffer), true);
				printf("%s", result);
				tv->inputlength = 0;
				tv->commandinput[0] = '\0';
			}
		}
	}
#endif
}
*/

void Trim(char *s)
{
	char *f;
	f = s;
	while(*f <= ' ' && *f > '\0')
		f++;
	while(*f > ' ')
		*s++ = *f++;
	*s = '\0';
}

qboolean QTV_Connect(sv_t *qtv, char *serverurl)
{
	if (qtv->sourcesock != INVALID_SOCKET)
	{
		closesocket(qtv->sourcesock);
		qtv->sourcesock = INVALID_SOCKET;
	}

	if (qtv->sourcefile)
	{
		fclose(qtv->sourcefile);
		qtv->sourcefile = NULL;
	}

	*qtv->serverinfo = '\0';
	Info_SetValueForStarKey(qtv->serverinfo, "*version",	"FTEQTV",	sizeof(qtv->serverinfo));
	Info_SetValueForStarKey(qtv->serverinfo, "*qtv",		VERSION,	sizeof(qtv->serverinfo));
	Info_SetValueForStarKey(qtv->serverinfo, "hostname",	qtv->cluster->hostname,	sizeof(qtv->serverinfo));
	Info_SetValueForStarKey(qtv->serverinfo, "maxclients",	"99",	sizeof(qtv->serverinfo));
	if (!strncmp(qtv->server, "file:", 5))
		Info_SetValueForStarKey(qtv->serverinfo, "server",		"file",	sizeof(qtv->serverinfo));
	else
		Info_SetValueForStarKey(qtv->serverinfo, "server",		qtv->server,	sizeof(qtv->serverinfo));

	memcpy(qtv->server, serverurl, sizeof(qtv->server)-1);

	if (qtv->disconnectwhennooneiswatching == 2)
	{	//added because of paranoia rather than need. Should never occur.
		printf("bug: autoclose==2\n");
		return false;
	}
	else if (!Net_ConnectToServer(qtv))
	{
		Sys_Printf(qtv->cluster, "Couldn't connect (%s)\n", qtv->server);
		return false;
	}

	if (qtv->sourcesock == INVALID_SOCKET)
	{
		qtv->parsetime = Sys_Milliseconds();
//		Sys_Printf(qtv->cluster, "Playing from file\n");
	}
	else
	{
		qtv->parsetime = Sys_Milliseconds() + BUFFERTIME*1000;
	}
	return true;
}

void QTV_Shutdown(sv_t *qtv)
{
	oproxy_t *prox;
	oproxy_t *old;
	viewer_t *v;
	sv_t *peer;
	cluster_t *cluster;
	int i;
	Sys_Printf(qtv->cluster, "Closing source %s\n", qtv->server);

	if (qtv->sourcesock != INVALID_SOCKET)
	{
		if (qtv->usequakeworldprotocols)
		{
			char dying[] = {clc_stringcmd, 'd', 'r', 'o', 'p', '\0'};
			Netchan_Transmit (qtv->cluster, &qtv->netchan, sizeof(dying), dying);
			Netchan_Transmit (qtv->cluster, &qtv->netchan, sizeof(dying), dying);
			Netchan_Transmit (qtv->cluster, &qtv->netchan, sizeof(dying), dying);
		}
		closesocket(qtv->sourcesock);
		qtv->sourcesock = INVALID_SOCKET;
	}
	if (qtv->sourcefile)
	{
		fclose(qtv->sourcefile);
		qtv->sourcefile = NULL;
	}
	if (qtv->downloadfile)
	{
		fclose(qtv->downloadfile);
		qtv->downloadfile = NULL;
		unlink(qtv->downloadname);
	}
//	if (qtv->tcpsocket != INVALID_SOCKET)
//		closesocket(qtv->tcpsocket);
	BSP_Free(qtv->bsp);
	qtv->bsp = NULL;

	cluster = qtv->cluster;
	if (cluster->servers == qtv)
		cluster->servers = qtv->next;
	else
	{
		for (peer = cluster->servers; peer->next; peer = peer->next)
		{
			if (peer->next == qtv)
			{
				peer->next = qtv->next;
				break;
			}
		}
	}

	if (cluster->viewserver == qtv)
		cluster->viewserver = NULL;

	for (v = cluster->viewers; v; v = v->next)
	{
		if (v->server == qtv)
		{
			QW_SetViewersServer(qtv->cluster, v, NULL);
			QW_SetMenu(v, MENU_NONE);
			QTV_SayCommand(cluster, v->server, v, "menu");
			QW_PrintfToViewer(v, "Stream %s is closing\n", qtv->server);
		}
	}

	for (i = 0; i < ENTITY_FRAMES; i++)
	{
		if (qtv->frame[i].ents)
			free(qtv->frame[i].ents);
		if (qtv->frame[i].entnums)
			free(qtv->frame[i].entnums);
	}

	for (prox = qtv->proxies; prox; )
	{
		if (prox->file)
			fclose(prox->file);
		if (prox->sock != INVALID_SOCKET)
			closesocket(prox->sock);
		old = prox;
		prox = prox->next;
		free(old);
		cluster->numproxies--;
	}


	free(qtv);
	cluster->numservers--;
}









void SendClientCommand(sv_t *qtv, char *fmt, ...)
{
	va_list		argptr;
	char buf[1024];

	va_start (argptr, fmt);
	vsnprintf (buf, sizeof(buf), fmt, argptr);
	va_end (argptr);

	WriteByte(&qtv->netchan.message, clc_stringcmd);
	WriteString(&qtv->netchan.message, buf);
}






void ChooseFavoriteTrack(sv_t *tv)
{
	int frags, best, pnum;
	char buffer[64];

	frags = -10000;
	best = -1;
	if (tv->controller || tv->proxyplayer)
		best = tv->trackplayer;
	else
	{
		for (pnum = 0; pnum < MAX_CLIENTS; pnum++)
		{
			if (*tv->players[pnum].userinfo && !atoi(Info_ValueForKey(tv->players[pnum].userinfo, "*spectator", buffer, sizeof(buffer))))
			{
				if (tv->thisplayer == pnum)
					continue;
				if (frags < tv->players[pnum].frags)
				{
					best = pnum;
					frags = tv->players[pnum].frags;
				}
			}
		}
	}
	if (best != tv->trackplayer)
	{
		SendClientCommand (tv, "ptrack %i\n", best);
		tv->trackplayer = best;

		if (tv->usequakeworldprotocols)
			QW_StreamStuffcmd(tv->cluster, tv, "track %i\n", best);
	}
}






static const unsigned char chktbl[1024] = {
0x78,0xd2,0x94,0xe3,0x41,0xec,0xd6,0xd5,0xcb,0xfc,0xdb,0x8a,0x4b,0xcc,0x85,0x01,
0x23,0xd2,0xe5,0xf2,0x29,0xa7,0x45,0x94,0x4a,0x62,0xe3,0xa5,0x6f,0x3f,0xe1,0x7a,
0x64,0xed,0x5c,0x99,0x29,0x87,0xa8,0x78,0x59,0x0d,0xaa,0x0f,0x25,0x0a,0x5c,0x58,
0xfb,0x00,0xa7,0xa8,0x8a,0x1d,0x86,0x80,0xc5,0x1f,0xd2,0x28,0x69,0x71,0x58,0xc3,
0x51,0x90,0xe1,0xf8,0x6a,0xf3,0x8f,0xb0,0x68,0xdf,0x95,0x40,0x5c,0xe4,0x24,0x6b,
0x29,0x19,0x71,0x3f,0x42,0x63,0x6c,0x48,0xe7,0xad,0xa8,0x4b,0x91,0x8f,0x42,0x36,
0x34,0xe7,0x32,0x55,0x59,0x2d,0x36,0x38,0x38,0x59,0x9b,0x08,0x16,0x4d,0x8d,0xf8,
0x0a,0xa4,0x52,0x01,0xbb,0x52,0xa9,0xfd,0x40,0x18,0x97,0x37,0xff,0xc9,0x82,0x27,
0xb2,0x64,0x60,0xce,0x00,0xd9,0x04,0xf0,0x9e,0x99,0xbd,0xce,0x8f,0x90,0x4a,0xdd,
0xe1,0xec,0x19,0x14,0xb1,0xfb,0xca,0x1e,0x98,0x0f,0xd4,0xcb,0x80,0xd6,0x05,0x63,
0xfd,0xa0,0x74,0xa6,0x86,0xf6,0x19,0x98,0x76,0x27,0x68,0xf7,0xe9,0x09,0x9a,0xf2,
0x2e,0x42,0xe1,0xbe,0x64,0x48,0x2a,0x74,0x30,0xbb,0x07,0xcc,0x1f,0xd4,0x91,0x9d,
0xac,0x55,0x53,0x25,0xb9,0x64,0xf7,0x58,0x4c,0x34,0x16,0xbc,0xf6,0x12,0x2b,0x65,
0x68,0x25,0x2e,0x29,0x1f,0xbb,0xb9,0xee,0x6d,0x0c,0x8e,0xbb,0xd2,0x5f,0x1d,0x8f,
0xc1,0x39,0xf9,0x8d,0xc0,0x39,0x75,0xcf,0x25,0x17,0xbe,0x96,0xaf,0x98,0x9f,0x5f,
0x65,0x15,0xc4,0x62,0xf8,0x55,0xfc,0xab,0x54,0xcf,0xdc,0x14,0x06,0xc8,0xfc,0x42,
0xd3,0xf0,0xad,0x10,0x08,0xcd,0xd4,0x11,0xbb,0xca,0x67,0xc6,0x48,0x5f,0x9d,0x59,
0xe3,0xe8,0x53,0x67,0x27,0x2d,0x34,0x9e,0x9e,0x24,0x29,0xdb,0x69,0x99,0x86,0xf9,
0x20,0xb5,0xbb,0x5b,0xb0,0xf9,0xc3,0x67,0xad,0x1c,0x9c,0xf7,0xcc,0xef,0xce,0x69,
0xe0,0x26,0x8f,0x79,0xbd,0xca,0x10,0x17,0xda,0xa9,0x88,0x57,0x9b,0x15,0x24,0xba,
0x84,0xd0,0xeb,0x4d,0x14,0xf5,0xfc,0xe6,0x51,0x6c,0x6f,0x64,0x6b,0x73,0xec,0x85,
0xf1,0x6f,0xe1,0x67,0x25,0x10,0x77,0x32,0x9e,0x85,0x6e,0x69,0xb1,0x83,0x00,0xe4,
0x13,0xa4,0x45,0x34,0x3b,0x40,0xff,0x41,0x82,0x89,0x79,0x57,0xfd,0xd2,0x8e,0xe8,
0xfc,0x1d,0x19,0x21,0x12,0x00,0xd7,0x66,0xe5,0xc7,0x10,0x1d,0xcb,0x75,0xe8,0xfa,
0xb6,0xee,0x7b,0x2f,0x1a,0x25,0x24,0xb9,0x9f,0x1d,0x78,0xfb,0x84,0xd0,0x17,0x05,
0x71,0xb3,0xc8,0x18,0xff,0x62,0xee,0xed,0x53,0xab,0x78,0xd3,0x65,0x2d,0xbb,0xc7,
0xc1,0xe7,0x70,0xa2,0x43,0x2c,0x7c,0xc7,0x16,0x04,0xd2,0x45,0xd5,0x6b,0x6c,0x7a,
0x5e,0xa1,0x50,0x2e,0x31,0x5b,0xcc,0xe8,0x65,0x8b,0x16,0x85,0xbf,0x82,0x83,0xfb,
0xde,0x9f,0x36,0x48,0x32,0x79,0xd6,0x9b,0xfb,0x52,0x45,0xbf,0x43,0xf7,0x0b,0x0b,
0x19,0x19,0x31,0xc3,0x85,0xec,0x1d,0x8c,0x20,0xf0,0x3a,0xfa,0x80,0x4d,0x2c,0x7d,
0xac,0x60,0x09,0xc0,0x40,0xee,0xb9,0xeb,0x13,0x5b,0xe8,0x2b,0xb1,0x20,0xf0,0xce,
0x4c,0xbd,0xc6,0x04,0x86,0x70,0xc6,0x33,0xc3,0x15,0x0f,0x65,0x19,0xfd,0xc2,0xd3
};


unsigned char	COM_BlockSequenceCRCByte (void *base, int length, int sequence)
{
	unsigned short crc;
	const unsigned char	*p;
	unsigned char chkb[60 + 4];

	p = chktbl + (sequence % (sizeof(chktbl) - 4));

	if (length > 60)
		length = 60;
	memcpy (chkb, base, length);

	chkb[length] = (sequence & 0xff) ^ p[0];
	chkb[length+1] = p[1];
	chkb[length+2] = ((sequence>>8) & 0xff) ^ p[2];
	chkb[length+3] = p[3];

	length += 4;

	crc = QCRC_Block(chkb, length);

	crc &= 0xff;

	return crc;
}
void SetMoveCRC(sv_t *qtv, netmsg_t *msg)
{
	char *outbyte;
	outbyte = (char*)msg->data + msg->startpos+1;

	*outbyte = COM_BlockSequenceCRCByte(
				outbyte+1, msg->cursize - (msg->startpos+2),
				qtv->netchan.outgoing_sequence);
}





void QTV_ParseQWStream(sv_t *qtv)
{
	char buffer[1500];
	netadr_t from;
	unsigned int fromlen;
	int readlen;
	netmsg_t msg;
	fromlen = sizeof(from);	//bug: this won't work on (free)bsd

	for (;;)
	{
		readlen = recvfrom(qtv->sourcesock, buffer, sizeof(buffer)-1, 0, (struct sockaddr*)&from, &fromlen);
		if (readlen < 0)
		{
			//FIXME: Check for error
			break;
		}
		if (readlen > sizeof(buffer)-1)
			break;	//oversized!

		buffer[readlen] = 0;
		if (*(int*)buffer == -1)
		{
			if (buffer[4] == 'c')
			{	//got a challenge
				strcpy(qtv->status, "Attemping connection\n");
				qtv->challenge = atoi(buffer+5);
				if (qtv->controller)
					sprintf(buffer, "connect %i %i %i \"%s\\*qtv\\1\\Qizmo\\2.9 notimer\"", 28, qtv->qport, qtv->challenge, qtv->controller->userinfo);
				else if (qtv->proxyplayer)
					sprintf(buffer, "connect %i %i %i \"%s\\name\\%s\"", 28, qtv->qport, qtv->challenge, "\\*ver\\fteqtv\\spectator\\0\\rate\\10000", qtv->cluster->hostname);
				else
					sprintf(buffer, "connect %i %i %i \"%s\\name\\%s\"", 28, qtv->qport, qtv->challenge, "\\*ver\\fteqtv\\spectator\\1\\rate\\10000", qtv->cluster->hostname);
				Netchan_OutOfBand(qtv->cluster, qtv->sourcesock, qtv->serveraddress, strlen(buffer), buffer);
				continue;
			}
			if (buffer[4] == 'n')
			{
				strncpy(qtv->status, buffer+5, sizeof(qtv->status));
				qtv->status[sizeof(qtv->status)-1] = 0;
				Sys_Printf(qtv->cluster, "%s: %s", qtv->server, buffer+5);
				continue;
			}
			if (buffer[4] == 'j')
			{
				strcpy(qtv->status, "Waiting for gamestate\n");
				Netchan_Setup(qtv->sourcesock, &qtv->netchan, qtv->serveraddress, qtv->qport, true);

				qtv->trackplayer = -1;

				qtv->isconnected = true;
				qtv->timeout = qtv->curtime + UDPTIMEOUT_LENGTH;
				SendClientCommand(qtv, "new");
				Sys_Printf(qtv->cluster, "Connected!\n");
				continue;
			}
			Sys_Printf(qtv->cluster, "%s: unrecognised connectionless packet:\n%s\n", qtv->server, buffer+4);
			continue;
		}
		memset(&msg, 0, sizeof(msg));
		msg.cursize = readlen;
		msg.data = buffer;
		msg.maxsize = readlen;
		qtv->timeout = qtv->curtime + UDPTIMEOUT_LENGTH;
		if (!Netchan_Process(&qtv->netchan, &msg))
			continue;
		ParseMessage(qtv, (char*)msg.data + msg.readpos, msg.cursize - msg.readpos, dem_all, -1);

		qtv->oldpackettime = qtv->nextpackettime;
		qtv->nextpackettime = qtv->parsetime;
		qtv->parsetime = qtv->curtime;

		if (qtv->simtime < qtv->oldpackettime)
			qtv->simtime = qtv->oldpackettime;	//too old

		if (qtv->controller)
		{
			qtv->controller->maysend = true;
//if (qtv->controller->netchan.outgoing_sequence != qtv->controller->netchan.incoming_sequence)
//printf("bug is here\n");
		}
	}
}

#ifdef COMMENTARY
#include <speex/speex.h> 
#endif

void QTV_CollectCommentry(sv_t *qtv)
{
#define usespeex 0
#ifdef COMMENTARY
	int samps;
	unsigned char buffer[8192+6];
	unsigned char *uchar;
	signed char *schar;
	int bytesleft;
	if (!qtv->comentrycapture)
	{
		if (0)
		{
//			if (usespeex)
//				qtv->comentrycapture = SND_InitCapture(11025, 16);
//			else
				qtv->comentrycapture = SND_InitCapture(11025, 8);
		}
		return;
	}

	while(1)
	{
		//the protocol WILL be different. Don't add compatability for this code.
		buffer[0] = 0;
		buffer[1] = dem_audio;
		buffer[2] = 255;
		buffer[3] = 255;
		buffer[4] = 8;
		buffer[5] = 11*5;

	/*	if (usespeex)
		{

			SpeexBits bits; 
			void *enc_state; 

			int frame_size;

			spx_int16_t pcmdata[8192/2];

			samps=qtv->comentrycapture->update(qtv->comentrycapture, 2048, (char*)pcmdata);


			speex_bits_init(&bits);

			enc_state = speex_encoder_init(&speex_nb_mode); 


			speex_encoder_ctl(enc_state,SPEEX_GET_FRAME_SIZE,&frame_size); 


			speex_bits_reset(&bits);

			speex_encode_int(enc_state, (spx_int16_t*)pcmdata, &bits);

			samps = speex_bits_write(&bits, buffer+6, sizeof(buffer)-6); 


			speex_bits_destroy(&bits);

			speex_encoder_destroy(enc_state); 

		}
		else*/
		{
			samps=qtv->comentrycapture->update(qtv->comentrycapture, 2048, buffer+6);

			bytesleft = samps;
			schar = buffer+6;
			uchar = buffer+6;
			while(bytesleft-->0)
			{
				*schar++ = *uchar++ - 128;
			}
		}

		buffer[2] = samps&255;
		buffer[3] = samps>>8;

		if (samps)
			SV_ForwardStream(qtv, buffer, 6 + samps);

		if (samps < 64)
			break;
	}
#endif
}

void QTV_Run(sv_t *qtv)
{
	int from;
	int to;
	int lengthofs;
	unsigned int length;
	unsigned char *buffer;
	int oldcurtime;
	int packettime;

	if (qtv->disconnectwhennooneiswatching == 1 && qtv->numviewers == 0 && qtv->proxies == NULL)
	{
		Sys_Printf(qtv->cluster, "Stream %s became inactive\n", qtv->server);
		qtv->drop = true;
	}
	if (qtv->drop)
	{
		QTV_Shutdown(qtv);
		return;
	}


//we will read out as many packets as we can until we're up to date
//note: this can cause real issues when we're overloaded for any length of time
//each new packet comes with a leading msec byte (msecs from last packet)
//then a type, an optional destination mask, and a 4byte size.
//the 4 byte size is probably excessive, a short would do.
//some of the types have thier destination mask encoded inside the type byte, yielding 8 types, and 32 max players.


//if we've no got enough data to read a new packet, we print a message and wait an extra two seconds. this will add a pause, connected clients will get the same pause, and we'll just try to buffer more of the game before playing.
//we'll stay 2 secs behind when the tcp stream catches up, however. This could be bad especially with long up-time.
//All timings are in msecs, which is in keeping with the mvd times, but means we might have issues after 72 or so days.
//the following if statement will reset the parse timer. It might cause the game to play too soon, the buffersize checks in the rest of the function will hopefully put it back to something sensible.

	oldcurtime = qtv->curtime;
	qtv->curtime = Sys_Milliseconds();
	if (oldcurtime > qtv->curtime)
	{
		Sys_Printf(qtv->cluster, "Time wrapped\n");
		qtv->parsetime = qtv->curtime;
	}




	if (qtv->sourcetype == SRC_UDP)
	{
		qtv->simtime += qtv->curtime - oldcurtime;

		if (qtv->simtime > qtv->nextpackettime)
			qtv->simtime = qtv->nextpackettime;	//too old

		if (!qtv->isconnected && (qtv->curtime >= qtv->nextconnectattempt || qtv->curtime < qtv->nextconnectattempt - UDPRECONNECT_TIME*2))
		{
			strcpy(qtv->status, "Attemping challenge\n");
			Netchan_OutOfBand(qtv->cluster, qtv->sourcesock, qtv->serveraddress, 13, "getchallenge\n");
			qtv->nextconnectattempt = qtv->curtime + UDPRECONNECT_TIME;
		}
		QTV_ParseQWStream(qtv);

		if (qtv->isconnected)
		{
			char buffer[128];
			netmsg_t msg;
			memset(&msg, 0, sizeof(msg));
			msg.data = buffer;
			msg.maxsize = sizeof(buffer);

			if (qtv->curtime >= qtv->timeout || qtv->curtime < qtv->timeout - UDPTIMEOUT_LENGTH*2)
			{
				Sys_Printf(qtv->cluster, "Timeout\n");
				qtv->isconnected = false;
				return;
			}

			if (qtv->controller && !qtv->controller->netchan.isnqprotocol)
			{
				qtv->netchan.outgoing_sequence = qtv->controller->netchan.incoming_sequence;
				if (qtv->maysend)
				{
					qtv->maysend = false;
					qtv->packetratelimiter = qtv->curtime;
				}
				else
					qtv->packetratelimiter = qtv->curtime + 1;
			}
			else
			{
				if (qtv->curtime < qtv->packetratelimiter - UDPPACKETINTERVAL*2)
					qtv->packetratelimiter = qtv->curtime;
			}
			if (qtv->curtime >= qtv->packetratelimiter)
			{
				if (qtv->curtime >= qtv->nextsendpings || qtv->curtime < qtv->nextsendpings - PINGSINTERVAL_TIME*2)
				{
					qtv->nextsendpings = qtv->curtime + PINGSINTERVAL_TIME;
					SendClientCommand(qtv, "pings\n");

				}
				ChooseFavoriteTrack(qtv);

				if (qtv->trackplayer >= 0)
				{
					qtv->packetratelimiter += UDPPACKETINTERVAL;

					WriteByte(&msg, clc_tmove);
					WriteShort(&msg, qtv->players[qtv->trackplayer].current.origin[0]);
					WriteShort(&msg, qtv->players[qtv->trackplayer].current.origin[1]);
					WriteShort(&msg, qtv->players[qtv->trackplayer].current.origin[2]);
				}
				else if (qtv->controller)
				{
					qtv->packetratelimiter += UDPPACKETINTERVAL;

					WriteByte(&msg, clc_tmove);
					WriteShort(&msg, qtv->controller->origin[0]);
					WriteShort(&msg, qtv->controller->origin[1]);
					WriteShort(&msg, qtv->controller->origin[2]);

/*					qtv->controller->ucmds[0].angles[1] = qtv->curtime*120;
					qtv->controller->ucmds[1].angles[1] = qtv->curtime*120;
					qtv->controller->ucmds[2].angles[1] = qtv->curtime*120;
*/
					msg.startpos = msg.cursize;
					WriteByte(&msg, clc_move);
					WriteByte(&msg, 0);
					WriteByte(&msg, 0);
					WriteDeltaUsercmd(&msg, &nullcmd, &qtv->controller->ucmds[0]);
					WriteDeltaUsercmd(&msg, &qtv->controller->ucmds[0], &qtv->controller->ucmds[1]);
					WriteDeltaUsercmd(&msg, &qtv->controller->ucmds[1], &qtv->controller->ucmds[2]);

					SetMoveCRC(qtv, &msg);
				}
				else if (qtv->proxyplayer || qtv->trackplayer < 0)
				{
					usercmd_t *cmd[3];
					cmd[0] = &qtv->proxyplayerucmds[(qtv->proxyplayerucmdnum-2)%3];
					cmd[1] = &qtv->proxyplayerucmds[(qtv->proxyplayerucmdnum-1)%3];
					cmd[2] = &qtv->proxyplayerucmds[(qtv->proxyplayerucmdnum-0)%3];

					cmd[2]->angles[0] = qtv->proxyplayerangles[0]/360*65535;
					cmd[2]->angles[1] = qtv->proxyplayerangles[1]/360*65535;
					cmd[2]->angles[2] = qtv->proxyplayerangles[2]/360*65535;
					cmd[2]->buttons = qtv->proxyplayerbuttons & 255;
					cmd[2]->forwardmove = (qtv->proxyplayerbuttons & (1<<8))?800:0 + (qtv->proxyplayerbuttons & (1<<9))?-800:0;
					cmd[2]->sidemove = (qtv->proxyplayerbuttons & (1<<11))?800:0 + (qtv->proxyplayerbuttons & (1<<10))?-800:0;
					cmd[2]->msec = qtv->curtime - qtv->packetratelimiter;
					cmd[2]->impulse = qtv->proxyplayerimpulse;
					if (cmd[2]->msec < 13)
						cmd[2]->msec = 13;
					qtv->packetratelimiter += cmd[2]->msec;
					qtv->proxyplayerimpulse = 0;


					msg.startpos = msg.cursize;
					WriteByte(&msg, clc_move);
					WriteByte(&msg, 0);
					WriteByte(&msg, 0);
					WriteDeltaUsercmd(&msg, &nullcmd, cmd[0]);
					WriteDeltaUsercmd(&msg, cmd[0], cmd[1]);
					WriteDeltaUsercmd(&msg, cmd[1], cmd[2]);
					qtv->proxyplayerucmdnum++;

					SetMoveCRC(qtv, &msg);
				}

				to = qtv->netchan.outgoing_sequence & (ENTITY_FRAMES-1);
				from = qtv->netchan.incoming_sequence & (ENTITY_FRAMES-1);
				if (qtv->frame[from].numents)
				{
					//remember which one we came from
					qtv->frame[to].oldframe = from;

					WriteByte(&msg, clc_delta);
					WriteByte(&msg, qtv->frame[to].oldframe);	//let the server know
				}
				else
					qtv->frame[to].oldframe = -1;

				qtv->frame[to].numents = 0;

				Netchan_Transmit(qtv->cluster, &qtv->netchan, msg.cursize, msg.data);
			}
		}
		return;
	}
	else
		qtv->simtime = qtv->curtime;


	if (qtv->sourcesock == INVALID_SOCKET && !qtv->sourcefile)
	{
		if (qtv->curtime >= qtv->nextconnectattempt || qtv->curtime < qtv->nextconnectattempt - RECONNECT_TIME*2)
		{
			if (qtv->disconnectwhennooneiswatching == 2)
			{
				qtv->drop = true;
				return;
			}
			if (!QTV_Connect(qtv, qtv->server))
			{
				return;
			}
		}
	}


//	SV_FindProxies(qtv->tcpsocket, qtv->cluster, qtv);	//look for any other proxies wanting to muscle in on the action.

	if (qtv->sourcefile || qtv->sourcesock != INVALID_SOCKET)
	{
		if (!Net_ReadStream(qtv))
		{	//if we have an error reading it
			//if it's valid, give up
			//what should we do here?
			//obviously, we need to keep reading the stream to keep things smooth
		}

		Net_WriteUpstream(qtv);
	}


	if (qtv->parsingqtvheader)
	{
		float svversion;
		int length;
		char *start;
		char *nl;
		char *colon;
		char *end;
		char value[128];
		char challenge[128];
		char authmethod[128];

//		qtv->buffer[qtv->buffersize] = 0;
//		Sys_Printf(qtv->cluster, "msg: ---%s---\n", qtv->buffer);

		*authmethod = 0;

		qtv->parsetime = qtv->curtime;
		length = qtv->buffersize;
		if (length > 6)
			length = 6;
		if (ustrncmp(qtv->buffer, "QTVSV ", length))
		{
			Sys_Printf(qtv->cluster, "Server is not a QTV server (or is incompatable)\n");
printf("%i, %s\n", qtv->buffersize, qtv->buffer);
			qtv->drop = true;
			return;
		}
		if (length < 6)
			return;	//not ready yet
		end = (char*)qtv->buffer + qtv->buffersize - 1;
		for (nl = (char*)qtv->buffer; nl < end; nl++)
		{
			if (nl[0] == '\n' && nl[1] == '\n')
				break;
		}
		if (nl == end)
			return;	//we need more header still

		//we now have a complete packet.

		svversion = atof((char*)qtv->buffer + 6);
		if ((int)svversion != 1)
		{
			Sys_Printf(qtv->cluster, "QTV server doesn't support a compatable protocol version (returned %i)\n", atoi((char*)qtv->buffer + 6));
			qtv->drop = true;
			return;
		}


		qtv->upstreamacceptschat = svversion>=1.1;
		qtv->upstreamacceptsdownload = svversion>=1.1;

		length = (nl - (char*)qtv->buffer) + 2;
		end = nl;
		nl[1] = '\0';
		start = strchr((char*)qtv->buffer, '\n')+1;

		while((nl = strchr(start, '\n')))
		{
			*nl = '\0';
			colon = strchr(start, ':');
			if (colon)
			{
				*colon = '\0';
				colon++;
				while (*colon == ' ')
					colon++;
				COM_ParseToken(colon, value, sizeof(value), NULL);
			}
			else
			{
				colon = "";
				*value = '\0';
			}


			//read the notes at the top of this file for which messages to expect
			if (!strcmp(start, "AUTH"))
				strcpy(authmethod, value);
			else if (!strcmp(start, "CHALLENGE"))
				strcpy(challenge, colon);
			else if (!strcmp(start, "COMPRESSION"))
			{	//we don't support compression, we didn't ask for it.
				Sys_Printf(qtv->cluster, "QTV server wrongly used compression\n");
				qtv->drop = true;
				return;
			}
			else if (!strcmp(start, "PERROR"))
			{
				Sys_Printf(qtv->cluster, "\nServer PERROR from %s: %s\n\n", qtv->server, colon);
				qtv->drop = true;
				qtv->buffersize = 0;
				qtv->forwardpoint = 0;
				return;
			}
			else if (!strcmp(start, "TERROR") || !strcmp(start, "ERROR"))
			{	//we don't support compression, we didn't ask for it.
				Sys_Printf(qtv->cluster, "\nServer TERROR from %s: %s\n\n", qtv->server, colon);
				qtv->buffersize = 0;
				qtv->forwardpoint = 0;

				if (qtv->disconnectwhennooneiswatching)
					qtv->drop = true;	//if its a user registered stream, drop it immediatly
				else
				{	//otherwise close the socket (this will result in a timeout and reconnect)
					if (qtv->sourcesock != INVALID_SOCKET)
					{
						closesocket(qtv->sourcesock);
						qtv->sourcesock = INVALID_SOCKET;
					}
				}
				return;
			}
			else if (!strcmp(start, "ASOURCE"))
			{
				Sys_Printf(qtv->cluster, "SRC: %s\n", colon);
			}
			else if (!strcmp(start, "ADEMO"))
			{
				int size;
				size = atoi(colon);
				colon = strchr(colon, ':');
				if (!colon)
					colon = "";
				else
					colon = colon+1;
				while(*colon == ' ')
					colon++;
				if (size > 1024*1024)
					Sys_Printf(qtv->cluster, "DEMO: (%3imb) %s\n", size/(1024*1024), colon);
				else
					Sys_Printf(qtv->cluster, "DEMO: (%3ikb) %s\n", size/1024, colon);
			}
			else if (!strcmp(start, "PRINT"))
			{
				Sys_Printf(qtv->cluster, "QTV server: %s\n", colon);
			}
			else if (!strcmp(start, "BEGIN"))
			{
				qtv->parsingqtvheader = false;
			}
			else
			{
				Sys_Printf(qtv->cluster, "DBG: QTV server responded with a %s key\n", start);
			}

			start = nl+1;
		}

		qtv->buffersize -= length;
		memmove(qtv->buffer, qtv->buffer + length, qtv->buffersize);

		if (qtv->serverquery)
		{
			Sys_Printf(qtv->cluster, "End of list\n");
			qtv->drop = true;
			qtv->buffersize = 0;
			qtv->forwardpoint = 0;
			return;
		}
		else if (*authmethod)
		{	//we need to send a challenge response now.
			Net_SendQTVConnectionRequest(qtv, authmethod, challenge);
			return;
		}
		else if (qtv->parsingqtvheader)
		{
			Sys_Printf(qtv->cluster, "QTV server sent no begin command - assuming incompatable\n\n");
			qtv->drop = true;
			qtv->buffersize = 0;
			qtv->forwardpoint = 0;
			return;
		}

		qtv->parsetime = Sys_Milliseconds() + BUFFERTIME*1000;
		if (!qtv->usequakeworldprotocols)
			Sys_Printf(qtv->cluster, "Connection established, buffering for %i seconds\n", BUFFERTIME);

		SV_ForwardStream(qtv, qtv->buffer, qtv->forwardpoint);
	}

	QTV_CollectCommentry(qtv);

	while (qtv->curtime >= qtv->parsetime)
	{
		if (qtv->buffersize < 2)
		{	//not enough stuff to play.
			if (qtv->parsetime < qtv->curtime)
			{
				qtv->parsetime = qtv->curtime + 2*1000;	//add two seconds
				if (qtv->sourcefile || qtv->sourcesock != INVALID_SOCKET)
					QTV_Printf(qtv, "Not enough buffered\n");
			}
			break;
		}

		buffer = qtv->buffer;

		switch (qtv->buffer[1]&dem_mask)
		{
		case dem_set:
			length = 10;
			if (qtv->buffersize < length)
			{	//not enough stuff to play.
				qtv->parsetime = qtv->curtime + 2*1000;	//add two seconds
				if (qtv->sourcefile || qtv->sourcesock != INVALID_SOCKET)
					QTV_Printf(qtv, "Not enough buffered\n");
				continue;
			}
			qtv->parsetime += buffer[0];	//well this was pointless

			if (qtv->forwardpoint < length)	//we're about to destroy this data, so it had better be forwarded by now!
			{
				SV_ForwardStream(qtv, qtv->buffer, length);
				qtv->forwardpoint += length;
			}

			memmove(qtv->buffer, qtv->buffer+10, qtv->buffersize-(length));
			qtv->buffersize -= length;
			qtv->forwardpoint -= length;
			continue;
		case dem_multiple:
			lengthofs = 6;
			break;
		default:
			lengthofs = 2;
			break;
		}

		if (qtv->buffersize < lengthofs+4)
		{	//the size parameter doesn't fit.
			if (qtv->sourcefile || qtv->sourcesock != INVALID_SOCKET)
				QTV_Printf(qtv, "Not enough buffered\n");
			qtv->parsetime = qtv->curtime + 2*1000;	//add two seconds
			break;
		}


		length = (buffer[lengthofs]<<0) + (buffer[lengthofs+1]<<8) + (buffer[lengthofs+2]<<16) + (buffer[lengthofs+3]<<24);
		if (length > 1500)
		{	//THIS SHOULDN'T HAPPEN!
			//Blame the upstream proxy!
			QTV_Printf(qtv, "Warning: corrupt input packet (%i bytes) too big! Flushing and reconnecting!\n", length);
			if (qtv->sourcefile)
			{
				fclose(qtv->sourcefile);
				qtv->sourcefile = NULL;
			}
			else
			{
				closesocket(qtv->sourcesock);
				qtv->sourcesock = INVALID_SOCKET;
			}
			qtv->buffersize = 0;
			qtv->forwardpoint = 0;
			break;
		}

		if (length+lengthofs+4 > qtv->buffersize)
		{
			if (qtv->sourcefile || qtv->sourcesock != INVALID_SOCKET)
				QTV_Printf(qtv, "Not enough buffered\n");
			qtv->parsetime = qtv->curtime + 2*1000;	//add two seconds
			break;	//can't parse it yet.
		}

//		if (qtv->sourcesock != INVALID_SOCKET)
//		{
//			QTV_Printf(qtv, "Forcing demo speed to play at 100% speed\n");
//			qtv->parsespeed = 1000;	//no speeding up/slowing down routed demos
//		}

		packettime = buffer[0];
		if (qtv->parsespeed>0)
			packettime = ((1000*packettime) / qtv->parsespeed);
		qtv->nextpackettime = qtv->parsetime + packettime;

		if (qtv->nextpackettime < qtv->curtime)
		{
			switch(qtv->buffer[1]&dem_mask)
			{
			case dem_multiple:
				ParseMessage(qtv, buffer+lengthofs+4, length, qtv->buffer[1]&dem_mask, (buffer[lengthofs-4]<<0) + (buffer[lengthofs-3]<<8) + (buffer[lengthofs-2]<<16) + (buffer[lengthofs-1]<<24));
				break;
			case dem_single:
			case dem_stats:
				ParseMessage(qtv, buffer+lengthofs+4, length, qtv->buffer[1]&dem_mask, 1<<(qtv->buffer[1]>>3));
				break;
			case dem_all:
				if (qtv->buffer[1] & ~dem_mask)	//dem_qtvdata
					if (qtv->sourcetype != SRC_DEMO)
						break;
				//fallthrough
			case dem_read:
				ParseMessage(qtv, buffer+lengthofs+4, length, qtv->buffer[1], 0xffffffff);
				break;
			default:
				Sys_Printf(qtv->cluster, "Message type %i\n", qtv->buffer[1]&dem_mask);
				break;
			}

			length = lengthofs+4+length;	//make length be the length of the entire packet

			qtv->oldpackettime = qtv->curtime;

			if (qtv->buffersize)
			{	//svc_disconnect can flush our input buffer (to prevent the EndOfDemo part from interfering)

				if (qtv->forwardpoint < length)	//we're about to destroy this data, so it had better be forwarded by now!
				{
					SV_ForwardStream(qtv, qtv->buffer, length);
					qtv->forwardpoint += length;
				}

				memmove(qtv->buffer, qtv->buffer+length, qtv->buffersize-(length));
				qtv->buffersize -= length;
				qtv->forwardpoint -= length;
			}

			if (qtv->sourcefile)
				Net_ReadStream(qtv);

			qtv->parsetime += packettime;

			qtv->nextconnectattempt = qtv->curtime + RECONNECT_TIME;
		}
		else
			break;
	}
}

sv_t *QTV_NewServerConnection(cluster_t *cluster, char *server, char *password, qboolean force, qboolean autoclose, qboolean noduplicates, qboolean query)
{
	sv_t *qtv;

	if (noduplicates)
	{
		for (qtv = cluster->servers; qtv; qtv = qtv->next)
		{
			if (!strcmp(qtv->server, server))
				return qtv;
		}
	}
	if (autoclose)
		if (cluster->nouserconnects)
			return NULL;

	qtv = malloc(sizeof(sv_t));
	if (!qtv)
		return NULL;

	memset(qtv, 0, sizeof(*qtv));
	//set up a default config
//	qtv->tcplistenportnum = PROX_DEFAULTLISTENPORT;
	strcpy(qtv->server, PROX_DEFAULTSERVER);

	memcpy(qtv->connectpassword, password, sizeof(qtv->connectpassword)-1);

//	qtv->tcpsocket = INVALID_SOCKET;
	qtv->sourcesock = INVALID_SOCKET;
	qtv->disconnectwhennooneiswatching = autoclose;
	qtv->parsingconnectiondata = true;
	qtv->serverquery = query;
	qtv->silentstream = true;
	qtv->parsespeed = 1000;

	qtv->streamid = ++cluster->nextstreamid;

	qtv->cluster = cluster;
	qtv->next = cluster->servers;

	if (autoclose != 2)	//2 means reverse connection (don't ever try reconnecting)
	{
		if (!QTV_Connect(qtv, server) && !force)
		{
			free(qtv);
			return NULL;
		}
	}
	cluster->servers = qtv;
	cluster->numservers++;

	return qtv;
}
