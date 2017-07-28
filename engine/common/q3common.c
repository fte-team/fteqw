#include "quakedef.h"

//this file contains q3 netcode related things.
//field info, netchan, and the WriteBits stuff (which should probably be moved to common.c with the others) 
//also contains vm filesystem

#define MAX_VM_FILES 8

typedef struct {
	char name[256];
	char *data;
	int bufferlen;
	qofs_t len;
	qofs_t ofs;
	int accessmode;
	int owner;
} vm_fopen_files_t;
vm_fopen_files_t vm_fopen_files[MAX_VM_FILES];
//FIXME: why does this not use the VFS system?
qofs_t VM_fopen (const char *name, int *handle, int fmode, int owner)
{
	int i;
	size_t insize;

	if (!handle)
		return !!FS_FLocateFile(name, FSLF_IFFOUND, NULL);

	*handle = 0;

	for (i = 0; i < MAX_VM_FILES; i++)
		if (!vm_fopen_files[i].data)
			break;

	if (i == MAX_VM_FILES)	//too many already open
	{
		return -1;
	}

	if (name[1] == ':' ||	//dos filename absolute path specified - reject.
		*name == '\\' || *name == '/' ||	//absolute path was given - reject
		strstr(name, ".."))	//someone tried to be cleaver.
	{
		return -1;
	}

	switch (fmode)
	{
	case VM_FS_READ:
		vm_fopen_files[i].data = FS_LoadMallocFile(name, &insize);
		vm_fopen_files[i].bufferlen = vm_fopen_files[i].len = insize;
		vm_fopen_files[i].ofs = 0;
		if (vm_fopen_files[i].data)
			break;
		else
			return -1;
		break;
		/*
	case VM_FS_APPEND:
	case VM_FS_APPEND2:
		vm_fopen_files[i].data = FS_LoadMallocFile(name);
		vm_fopen_files[i].ofs = vm_fopen_files[i].bufferlen = vm_fopen_files[i].len = com_filesize;
		if (vm_fopen_files[i].data)
			break;
		//fall through
	case VM_FS_WRITE:
		vm_fopen_files[i].bufferlen = 8192;
		vm_fopen_files[i].data = BZ_Malloc(vm_fopen_files[i].bufferlen);
		vm_fopen_files[i].len = 0;
		vm_fopen_files[i].ofs = 0;
		break;
		*/
	default: //bad
		return -1;
	}

	Q_strncpyz(vm_fopen_files[i].name, name, sizeof(vm_fopen_files[i].name));
	vm_fopen_files[i].accessmode = fmode;
	vm_fopen_files[i].owner = owner;

	*handle = i+1;
	return vm_fopen_files[i].len;
}

void VM_fclose (int fnum, int owner)
{
	fnum--;

	if (fnum < 0 || fnum >= MAX_VM_FILES)
		return;	//out of range

	if (vm_fopen_files[fnum].owner != owner)
		return;	//cgs?

	if (!vm_fopen_files[fnum].data)
		return;	//not open

	switch(vm_fopen_files[fnum].accessmode)
	{
	case VM_FS_READ:
		BZ_Free(vm_fopen_files[fnum].data);
		break;
	case VM_FS_WRITE:
	case VM_FS_APPEND:
	case VM_FS_APPEND2:
		COM_WriteFile(vm_fopen_files[fnum].name, FS_GAMEONLY, vm_fopen_files[fnum].data, vm_fopen_files[fnum].len);
		BZ_Free(vm_fopen_files[fnum].data);
		break;
	}
	vm_fopen_files[fnum].data = NULL;
}

int VM_FRead (char *dest, int quantity, int fnum, int owner)
{
	fnum--;
	if (fnum < 0 || fnum >= MAX_VM_FILES)
		return 0;	//out of range

	if (vm_fopen_files[fnum].owner != owner)
		return 0;	//cgs?

	if (!vm_fopen_files[fnum].data)
		return 0;	//not open

	if (quantity > vm_fopen_files[fnum].len - vm_fopen_files[fnum].ofs)
		quantity = vm_fopen_files[fnum].len - vm_fopen_files[fnum].ofs;
	memcpy(dest, vm_fopen_files[fnum].data + vm_fopen_files[fnum].ofs, quantity);
	vm_fopen_files[fnum].ofs += quantity;

	return quantity;
}
int VM_FWrite (const char *dest, int quantity, int fnum, int owner)
{
/*
	int fnum = G_FLOAT(OFS_PARM0);
	char *msg = PF_VarString(prinst, 1, pr_globals);
	int len = strlen(msg);
	if (fnum < 0 || fnum >= MAX_QC_FILES)
		return;	//out of range

	if (!pf_fopen_files[fnum].data)
		return;	//not open

	if (pf_fopen_files[fnum].prinst != prinst)
		return;	//this just isn't ours.

	if (pf_fopen_files[fnum].bufferlen < pf_fopen_files[fnum].ofs + len)
	{
		char *newbuf;
		pf_fopen_files[fnum].bufferlen = pf_fopen_files[fnum].bufferlen*2 + len;
		newbuf = BZF_Malloc(pf_fopen_files[fnum].bufferlen);
		memcpy(newbuf, pf_fopen_files[fnum].data, pf_fopen_files[fnum].len);
		BZ_Free(pf_fopen_files[fnum].data);
		pf_fopen_files[fnum].data = newbuf;
	}

	memcpy(pf_fopen_files[fnum].data + pf_fopen_files[fnum].ofs, msg, len);
	if (pf_fopen_files[fnum].len < pf_fopen_files[fnum].ofs + len)
		pf_fopen_files[fnum].len = pf_fopen_files[fnum].ofs + len;
	pf_fopen_files[fnum].ofs+=len;
*/
	return 0;
}
qboolean VM_FSeek (int fnum, qofs_t offset, int seektype, int owner)
{
	fnum--;
	if (fnum < 0 || fnum >= MAX_VM_FILES)
		return false;	//out of range
	if (vm_fopen_files[fnum].owner != owner)
		return false;	//cgs?
	if (!vm_fopen_files[fnum].data)
		return false;	//not open

	switch(seektype)
	{
	case 0:
		offset = vm_fopen_files[fnum].ofs + offset;
	case 1:
		offset = vm_fopen_files[fnum].len + offset;
		break;
	default:
	case 2:
		//offset = 0 + offset;
		break;
	}
//	if (offset < 0)
//		offset = 0;
	if (offset > vm_fopen_files[fnum].len)
		offset = vm_fopen_files[fnum].len;
	vm_fopen_files[fnum].ofs = offset;
	return true;
}
qofs_t VM_FTell (int fnum, int owner)
{
	fnum--;
	if (fnum < 0 || fnum >= MAX_VM_FILES)
		return 0;	//out of range
	if (vm_fopen_files[fnum].owner != owner)
		return 0;	//cgs?
	if (!vm_fopen_files[fnum].data)
		return 0;	//not open

	return vm_fopen_files[fnum].ofs;
}
void VM_fcloseall (int owner)
{
	int i;
	for (i = 1; i <= MAX_VM_FILES; i++)
	{
		VM_fclose(i, owner);
	}
}












typedef struct {
	char *initialbuffer;
	char *buffer;
	char *dir;
	int found;
	int bufferleft;
	int skip;
} vmsearch_t;
static int QDECL VMEnum(const char *match, qofs_t size, time_t mtime, void *args, searchpathfuncs_t *spath)
{
	char *check;
	int newlen;
	match += ((vmsearch_t *)args)->skip;
	newlen = strlen(match)+1;
	if (newlen > ((vmsearch_t *)args)->bufferleft)
		return false;	//too many files for the buffer

	check = ((vmsearch_t *)args)->initialbuffer;
	while(check < ((vmsearch_t *)args)->buffer)
	{
		if (!stricmp(check, match))
			return true;	//we found this one already
		check += strlen(check)+1;
	}

	memcpy(((vmsearch_t *)args)->buffer, match, newlen);
	((vmsearch_t *)args)->buffer+=newlen;
	((vmsearch_t *)args)->bufferleft-=newlen;
	((vmsearch_t *)args)->found++;
	return true;
}

static int QDECL IfFound(const char *match, qofs_t size, time_t modtime, void *args, searchpathfuncs_t *spath)
{
	*(qboolean*)args = true;
	return true;
}

static int QDECL VMEnumMods(const char *match, qofs_t size, time_t modtime, void *args, searchpathfuncs_t *spath)
{
	char *check;
	char desc[1024];
	int newlen;
	int desclen;
	qboolean foundone;
	vfsfile_t *f;

	newlen = strlen(match)+1;

	if (newlen <= 2)
		return true;

	//make sure match is a directory
	if (match[newlen-2] != '/')
		return true;

	if (!stricmp(match, "baseq3/"))
		return true;	//we don't want baseq3. FIXME: should be any basedir, rather than hardcoded.

	foundone = false;
	Sys_EnumerateFiles(va("%s%s/", ((vmsearch_t *)args)->dir, match), "*.pk3", IfFound, &foundone, spath);
	if (foundone == false)
		return true;	//we only count directories with a pk3 file

	Q_strncpyz(desc, match, sizeof(desc));
	f = FS_OpenVFS(va("%sdescription.txt", match), "rb", FS_ROOT);
	if (f)
	{
		VFS_GETS(f, desc, sizeof(desc));
		VFS_CLOSE(f);
	}

	desclen = strlen(desc)+1;

	if (newlen+desclen+5 > ((vmsearch_t *)args)->bufferleft)
		return false;	//too many files for the buffer

	check = ((vmsearch_t *)args)->initialbuffer;
	while(check < ((vmsearch_t *)args)->buffer)
	{
		if (!stricmp(check, match))
			return true;	//we found this one already
		check += strlen(check)+1;
		check += strlen(check)+1;
	}

	memcpy(((vmsearch_t *)args)->buffer, match, newlen);
	((vmsearch_t *)args)->buffer+=newlen;
	((vmsearch_t *)args)->bufferleft-=newlen;

	memcpy(((vmsearch_t *)args)->buffer, desc, desclen);
	((vmsearch_t *)args)->buffer+=desclen;
	((vmsearch_t *)args)->bufferleft-=desclen;

	((vmsearch_t *)args)->found++;
	return true;
}

int VM_GetFileList(const char *path, const char *ext, char *output, int buffersize)
{
	vmsearch_t vms;
	vms.initialbuffer = vms.buffer = output;
	vms.skip = strlen(path)+1;
	vms.bufferleft = buffersize;
	vms.found=0;
	if (!strcmp(path, "$modlist"))
	{
		vms.skip=0;
		Sys_EnumerateFiles((vms.dir=com_gamepath), "*", VMEnumMods, &vms, NULL);
		if (*com_homepath)
			Sys_EnumerateFiles((vms.dir=com_homepath), "*", VMEnumMods, &vms, NULL);
	}
	else if (*(char *)ext == '.' || *(char *)ext == '/')
		COM_EnumerateFiles(va("%s/*%s", path, ext), VMEnum, &vms);
	else
		COM_EnumerateFiles(va("%s/*.%s", path, ext), VMEnum, &vms);
	return vms.found;
}










#if defined(Q3SERVER) || defined(Q3CLIENT)

#include "clq3defs.h"	//okay, urr, this is bad for dedicated servers. urhum. Maybe they're not looking? It's only typedefs and one extern.

#define MAX_VMQ3_CVARS 256	//can be blindly increased
cvar_t *q3cvlist[MAX_VMQ3_CVARS];
int VMQ3_Cvar_Register(q3vmcvar_t *v, char *name, char *defval, int flags)
{
	int i;
	int fteflags = 0;
	cvar_t *c;

	fteflags = flags & (CVAR_ARCHIVE | CVAR_USERINFO | CVAR_SERVERINFO);

	c = Cvar_Get(name, defval, fteflags, "Q3VM cvars");
	if (!c)	//command name, etc
		return 0;
	for (i = 0; i < MAX_VMQ3_CVARS; i++)
	{
		if (!q3cvlist[i])
			q3cvlist[i] = c;
		if (q3cvlist[i] == c)
		{
			if (v)
			{
				v->handle = i+1;

				VMQ3_Cvar_Update(v);
			}
			return i+1;
		}
	}

	Con_Printf("Ran out of VMQ3 cvar handles\n");

	return 0;
}
int VMQ3_Cvar_Update(q3vmcvar_t *v)
{
	cvar_t *c;
	int i;
	i = v->handle;
	if (!i)
		return 0;	//not initialised
	i--;
	if ((unsigned)i >= MAX_VMQ3_CVARS)
		return 0;	//a hack attempt

	c = q3cvlist[i];
	if (!c)
		return 0;	//that slot isn't active yet

	v->integer = c->ival;
	v->value = c->value;
	v->modificationCount = c->modified;
	Q_strncpyz(v->string, c->string, sizeof(v->string));

	return 1;
}


/*
============
MSG_WriteRawBytes
============
*/
static void MSG_WriteRawBytes( sizebuf_t *msg, int value, int bits )
{
	qbyte	*buf;

	if( bits <= 8 )
	{ 
		buf = SZ_GetSpace( msg, 1 );
		buf[0] = value;		
	}
	else if( bits <= 16 )
	{
		buf = SZ_GetSpace( msg, 2 );
		buf[0] = value & 0xFF;
		buf[1] = value >> 8;
	}
	else if( bits <= 32 )
	{
		buf = SZ_GetSpace( msg, 4 );
		buf[0] = value & 0xFF;
		buf[1] = (value >> 8) & 0xFF;
		buf[2] = (value >> 16) & 0xFF;
		buf[3] = value >> 24;
	}
}

/*
============
MSG_WriteRawBits
============
*/
static void MSG_WriteRawBits( sizebuf_t *msg, int value, int bits )
{
	// TODO
}

/*
============
MSG_WriteHuffBits
============
*/
#ifdef HUFFNETWORK
static void MSG_WriteHuffBits( sizebuf_t *msg, int value, int bits )
{
	int		remaining;
	int		i;

	value &= 0xFFFFFFFFU >> (32 - bits);
	remaining = bits & 7;

	for( i=0; i<remaining ; i++ )
	{
		if( !(msg->currentbit & 7) )
		{
			msg->data[msg->currentbit >> 3] = 0;
		} 
		msg->data[msg->currentbit >> 3] |= (value & 1) << (msg->currentbit & 7);
		msg->currentbit++;
		value >>= 1;
	}
	bits -= remaining;

	if( bits > 0 )
	{
		for( i=0 ; i<(bits+7)>>3 ; i++ )
		{
			Huff_EmitByte( value & 255, msg->data, &msg->currentbit );
			value >>= 8;
		}
	}

	msg->cursize = (msg->currentbit >> 3) + 1;
}
#endif

/*
============
MSG_WriteBits
============
*/
void MSG_WriteBits(sizebuf_t *msg, int value, int bits)
{
#ifdef MSG_PROFILING
	int	maxval;
#endif // MSG_PROFILING

	if( msg->maxsize - msg->cursize < 4 )
	{
		msg->overflowed = true;
		return;
	}

	if( !bits || bits < -31 || bits > 32 )
	{
		Sys_Error("MSG_WriteBits: bad bits %i", bits);
	}

#ifdef MSG_PROFILING
	msg_bitsWritten += bits;

	if( bits != 32 )
	{
		if( bits > 0 )
		{
			maxval = (1 << bits) - 1;
			if( value > maxval || maxval < 0 )
			{
				msg_overflows++;
			}
		}
		else
		{
			maxval = (1 << (bits - 1)) - 1;
			if( value > maxval || value < -maxval - 1 )
			{
				msg_overflows++;
			}
		}
	}
#endif // MSG_PROFILING

	if( bits < 0 )
	{
		bits = -bits;
	}

	switch( msg->packing )
	{
	default:
	case SZ_BAD:
		Sys_Error("MSG_WriteBits: bad msg->packing %i", msg->packing );
		break;
	case SZ_RAWBYTES:
		MSG_WriteRawBytes( msg, value, bits );
		break;
	case SZ_RAWBITS:
		MSG_WriteRawBits( msg, value, bits );
		break;
#ifdef HUFFNETWORK
	case SZ_HUFFMAN:
		MSG_WriteHuffBits( msg, value, bits );
		break;
#endif
	}

}



////////////////////////////////////////////////////////////////////////////////
//q3 netchan
//note that the sv and cl both have their own wrappers, to handle encryption.







#define	MAX_PACKETLEN			1400
#define FRAGMENT_MASK			0x80000000
#define FRAGMENTATION_TRESHOLD	(MAX_PACKETLEN-100)
qboolean Netchan_ProcessQ3 (netchan_t *chan)
{
//incoming_reliable_sequence is perhaps wrongly used...
	int			sequence;
	qboolean	fragment;
	int			fragmentStart;
	int			fragmentLength;
	char		adr[MAX_ADR_SIZE];

	// Get sequence number
	MSG_BeginReading(msg_nullnetprim);
	sequence = MSG_ReadBits(32);

	// Read the qport if we are a server
	if (chan->sock == NS_SERVER)
	{
		MSG_ReadBits(16);
	}

	// Check if packet is a message fragment
	if (sequence & FRAGMENT_MASK)
	{
		sequence &= ~FRAGMENT_MASK;

		fragment = true;
		fragmentStart = MSG_ReadBits(16);
		fragmentLength = MSG_ReadBits(16);
	}
	else
	{
		fragment = false;
		fragmentStart = 0;
		fragmentLength = 0;
	}

/*	if (net_showpackets->integer)
	{
		if (fragment)
		{
			Con_Printf("%s recv %4i : s=%i fragment=%i,%i\n", (chan->sock == NS_CLIENT) ? "client" : "server", net_message.cursize, sequence, fragmentStart, fragmentLength);
		}
		else
		{
			Con_Printf("%s recv %4i : s=%i\n", (chan->sock == NS_CLIENT) ? "client" : "server", net_message.cursize, sequence);
		}
	}*/

	// Discard stale or duplicated packets
	if (sequence <= chan->incoming_sequence)
	{
/*		if (net_showdrop->integer || net_showpackets->integer)
		{
			Con_Printf("%s:Out of order packet %i at %i\n", NET_AdrToString(chan->remote_address), chan->incoming_sequence);
		}*/
		return false;
	}

	// Dropped packets don't keep the message from being used
	chan->drop_count = sequence - (chan->incoming_sequence + 1);

	if (chan->drop_count > 0)// && (net_showdrop->integer || net_showpackets->integer))
	{
		Con_DPrintf("%s:Dropped %i packets at %i\n", NET_AdrToString(adr, sizeof(adr), &chan->remote_address), chan->drop_count, sequence);
	}

	if (!fragment)
	{ // not fragmented
		chan->incoming_sequence = sequence;
		chan->last_received = realtime;
		return true;
	}

	// Check for new fragmented message
	if (chan->incoming_reliable_sequence != sequence)
	{
		chan->incoming_reliable_sequence = sequence;
		chan->in_fragment_length = 0;
	}

	// Check fragments sequence
	if (chan->in_fragment_length != fragmentStart)
	{
//		if(net_showdrop->integer || net_showpackets->integer)
		{
			Con_Printf("%s:Dropped a message fragment\n", NET_AdrToString(adr, sizeof(adr), &chan->remote_address));
		}
		return false;
	}

	// Check if fragmentLength is valid
	if (fragmentLength < 0 || fragmentLength > FRAGMENTATION_TRESHOLD || msg_readcount + fragmentLength > net_message.cursize || chan->in_fragment_length + fragmentLength > sizeof(chan->in_fragment_buf))
	{
/*		if (net_showdrop->integer || net_showpackets->integer)
		{
			Con_Printf("%s:illegal fragment length\n", NET_AdrToString(chan->remote_address));
		}
*/		return false;
	}

	// Append to the incoming fragment buffer
	memcpy( chan->in_fragment_buf + chan->in_fragment_length, net_message.data + msg_readcount, fragmentLength);

	chan->in_fragment_length += fragmentLength;
	if (fragmentLength == FRAGMENTATION_TRESHOLD)
	{
		return false; // there are more fragments of this message
	}

	// Check if assembled message fits in buffer
	if (chan->in_fragment_length > net_message.maxsize)
	{
		Con_Printf("%s:fragmentLength %i > net_message.maxsize\n", NET_AdrToString(adr, sizeof(adr), &chan->remote_address), chan->in_fragment_length);
		return false;
	}

	//
	// Reconstruct message properly
	//
	SZ_Clear(&net_message);
	MSG_WriteLong(&net_message, sequence);
	SZ_Write(&net_message, chan->in_fragment_buf, chan->in_fragment_length);

	MSG_BeginReading(msg_nullnetprim);
	MSG_ReadLong();

	// No more fragments
	chan->in_fragment_length = 0;
	chan->incoming_reliable_sequence = 0;
	chan->incoming_sequence = sequence;
	chan->last_received = realtime;

	return true;
}


/*
=================
Netchan_TransmitNextFragment
=================
*/
void Netchan_TransmitNextFragment( netchan_t *chan )
{
	//'reliable' is badly named. it should be 'fragment' instead.
	//but in the interests of a smaller netchan_t...
	int i;
	sizebuf_t	send;
	qbyte		send_buf[MAX_PACKETLEN];
	int			fragmentLength;
	
	// Write the packet header
	memset(&send, 0, sizeof(send));
	send.packing = SZ_RAWBYTES;
	send.maxsize = sizeof(send_buf);
	send.data = send_buf;
	MSG_WriteLong( &send, chan->outgoing_sequence | FRAGMENT_MASK );
#ifndef SERVERONLY
	// Send the qport if we are a client
	if( chan->sock == NS_CLIENT )
	{
		MSG_WriteShort( &send, chan->qport);
	}
#endif
	fragmentLength = chan->reliable_length - chan->reliable_start;
	if( fragmentLength > FRAGMENTATION_TRESHOLD ) {
		// remaining fragment is still too large
		fragmentLength = FRAGMENTATION_TRESHOLD;
	}

	// Write the fragment header
	MSG_WriteShort( &send, chan->reliable_start );
	MSG_WriteShort( &send, fragmentLength );

	// Copy message fragment to the packet
	SZ_Write( &send, chan->reliable_buf + chan->reliable_start, fragmentLength );

	// Send the datagram
	NET_SendPacket( chan->sock, send.cursize, send.data, &chan->remote_address );

//	if( net_showpackets->integer )
//	{
//		Con_Printf( "%s send %4i : s=%i fragment=%i,%i\n", (chan->sock == NS_CLIENT) ? "client" : "server", send.cursize, chan->outgoing_sequence, chan->reliable_start, fragmentLength );
//	}

	// Even if we have sent the whole message,
	// but if fragmentLength == FRAGMENTATION_TRESHOLD we have to write empty
	// fragment later, because Netchan_Process expects it...
	chan->reliable_start += fragmentLength;
	if( chan->reliable_start == chan->reliable_length && fragmentLength != FRAGMENTATION_TRESHOLD )
	{
		// we have sent the whole message!
		chan->outgoing_sequence++;
		chan->reliable_length = 0;
		chan->reliable_start = 0;

		i = chan->outgoing_sequence & (MAX_LATENT-1);
		chan->outgoing_size[i] = send.cursize;
		chan->outgoing_time[i] = realtime;
	}
}

/*
=================
Netchan_Transmit
=================
*/
void Netchan_TransmitQ3( netchan_t *chan, int length, const qbyte *data )
{
	int i;
	sizebuf_t	send;
	qbyte		send_buf[MAX_OVERALLMSGLEN+6];
	char		adr[MAX_ADR_SIZE];
	
	// Check for message overflow
	if( length > MAX_OVERALLMSGLEN )
	{
		Con_Printf( "%s: outgoing message overflow\n", NET_AdrToString( adr, sizeof(adr), &chan->remote_address ) );
		return;
	}

	if( length < 0 )
	{
		Sys_Error("Netchan_Transmit: length = %i", length);
	}

	// Don't send if there are still unsent fragments
	if( chan->reliable_length )
	{
		Netchan_TransmitNextFragment( chan );
		if( chan->reliable_length )
		{
			Con_DPrintf( "%s: unsent fragments\n", NET_AdrToString( adr, sizeof(adr), &chan->remote_address ) );
			return;
		}
		/*drop the outgoing packet if we fragmented*/
		/*failure to do this results in the wrong encoding due to the outgoing sequence*/
		return;
	}

	// See if this message is too large and should be fragmented
	if( length >= FRAGMENTATION_TRESHOLD )
	{
		chan->reliable_length = length;
		chan->reliable_start = 0;
		memcpy( chan->reliable_buf, data, length );
		Netchan_TransmitNextFragment( chan );
		return;
	}

	// Write the packet header
	memset(&send, 0, sizeof(send));
	send.packing = SZ_RAWBYTES;
	send.maxsize = sizeof(send_buf);
	send.data = send_buf;
	MSG_WriteLong( &send, chan->outgoing_sequence );
#ifndef SERVERONLY
	// Send the qport if we are a client
	if( chan->sock == NS_CLIENT )
	{
		MSG_WriteShort( &send, chan->qport);
	}
#endif
	// Copy the message to the packet
	SZ_Write( &send, data, length );

	// Send the datagram
	NET_SendPacket( chan->sock, send.cursize, send.data, &chan->remote_address );

/*	if( net_showpackets->integer )
	{
		Con_Printf( "%s send %4i : s=%i ack=%i\n", (chan->sock == NS_SERVER) ? "server" : "client", send.cursize , chan->outgoing_sequence, chan->incoming_sequence );
	}
*/
	chan->outgoing_sequence++;

	i = chan->outgoing_sequence & (MAX_LATENT-1);
	chan->outgoing_size[i] = send.cursize;
	chan->outgoing_time[i] = realtime;
}


//////////////


int StringKey( const char *string, int length )
{
	int i;
	int key = 0;

	for( i=0 ; i<length && string[i] ; i++ )
	{
		key += string[i] * (119 + i);
	}

	return (key ^ (key >> 10) ^ (key >> 20));
}













typedef struct {
#ifdef MSG_SHOWNET
	const char	*name;
#endif // MSG_SHOWNET
	int		offset;
	int		bits; 	// bits > 0  -->  unsigned integer
					// bits = 0  -->  float value
					// bits < 0  -->  signed integer
} q3field_t;

// field declarations
#ifdef MSG_SHOWNET
#	define PS_FIELD(n,b)	{ #n, ((size_t)&(((q3playerState_t *)0)->n)), b }
#	define ES_FIELD(n,b)	{ #n, ((size_t)&(((q3entityState_t *)0)->n)), b }
#else
#	define PS_FIELD(n,b)	{ ((size_t)&(((q3playerState_t *)0)->n)), b }
#	define ES_FIELD(n,b)	{ ((size_t)&(((q3entityState_t *)0)->n)), b }
#endif

// field data accessing
#define FIELD_INTEGER(s)	(*(int   *)((qbyte *)(s)+field->offset))
#define FIELD_FLOAT(s)		(*(float *)((qbyte *)(s)+field->offset))

#define SNAPPED_BITS		13
#define MAX_SNAPPED			(1<<SNAPPED_BITS)


//
// entityState_t
//
static const q3field_t esFieldTable[] = {
	ES_FIELD( pos.trTime,			32 ),
	ES_FIELD( pos.trBase[0],		 0 ),
	ES_FIELD( pos.trBase[1],		 0 ),
	ES_FIELD( pos.trDelta[0],		 0 ),
	ES_FIELD( pos.trDelta[1],		 0 ),
	ES_FIELD( pos.trBase[2],		 0 ),
	ES_FIELD( apos.trBase[1],		 0 ),
	ES_FIELD( pos.trDelta[2],		 0 ),
	ES_FIELD( apos.trBase[0],		 0 ),
	ES_FIELD( event,				10 ),
	ES_FIELD( angles2[1],			 0 ),
	ES_FIELD( eType,				 8 ),
	ES_FIELD( torsoAnim,			 8 ),
	ES_FIELD( eventParm,			 8 ),
	ES_FIELD( legsAnim,				 8 ),
	ES_FIELD( groundEntityNum,		10 ),
	ES_FIELD( pos.trType,			 8 ),
	ES_FIELD( eFlags,				19 ),
	ES_FIELD( otherEntityNum,		10 ),
	ES_FIELD( weapon,				 8 ),
	ES_FIELD( clientNum,			 8 ),
	ES_FIELD( angles[1],			 0 ),
	ES_FIELD( pos.trDuration,		32 ),
	ES_FIELD( apos.trType,			 8 ),
	ES_FIELD( origin[0],			 0 ),
	ES_FIELD( origin[1],			 0 ),
	ES_FIELD( origin[2],			 0 ),
	ES_FIELD( solid,				24 ),
	ES_FIELD( powerups,				16 ),
	ES_FIELD( modelindex,			 8 ),
	ES_FIELD( otherEntityNum2,		10 ),
	ES_FIELD( loopSound,			 8 ),
	ES_FIELD( generic1,				 8 ),
	ES_FIELD( origin2[2],			 0 ),
	ES_FIELD( origin2[0],			 0 ),
	ES_FIELD( origin2[1],			 0 ),
	ES_FIELD( modelindex2,			 8 ),
	ES_FIELD( angles[0],			 0 ),
	ES_FIELD( time,					32 ),
	ES_FIELD( apos.trTime,			32 ),
	ES_FIELD( apos.trDuration,		32 ),
	ES_FIELD( apos.trBase[2],		 0 ),
	ES_FIELD( apos.trDelta[0],		 0 ),
	ES_FIELD( apos.trDelta[1],		 0 ),
	ES_FIELD( apos.trDelta[2],		 0 ),
	ES_FIELD( time2,				32 ),
	ES_FIELD( angles[2],			 0 ),
	ES_FIELD( angles2[0],			 0 ),
	ES_FIELD( angles2[2],			 0 ),
	ES_FIELD( constantLight,		32 ),
	ES_FIELD( frame,				16 )
};

static const int esTableSize = sizeof( esFieldTable ) / sizeof( esFieldTable[0] );

q3entityState_t nullEntityState;

/*
============
MSG_ReadDeltaEntity

  'from' == NULL  -->  nodelta update
  'to'   == NULL  -->  do nothing

returns false if the ent was removed.
============
*/
#ifndef SERVERONLY
qboolean MSG_Q3_ReadDeltaEntity( const q3entityState_t *from, q3entityState_t *to, int number )
{
	const q3field_t	*field;
	int				to_integer;
	int				maxFieldNum;
#ifdef MSG_SHOWNET
	int				startbits;
	qboolean		dump;
#endif
	int				i;


	if( number < 0 || number >= MAX_GENTITIES )
	{
		Host_EndGame("MSG_ReadDeltaEntity: Bad delta entity number: %i\n", number);
	}

	if( !to )
	{
		return true;
	}

#ifdef MSG_SHOWNET
	dump = (qboolean)(cl_shownet->integer >= 2);

	if( dump )
	{
		startbits = msg->bit;
	}
#endif

	if (MSG_ReadBits(1))
	{ 
		memset( to, 0, sizeof( *to ) );
		to->number = ENTITYNUM_NONE;

#ifdef MSG_SHOWNET
		if( dump )
		{
			Con_Printf( "%3i: #%-3i remove\n", msg->readcount, number );
		}
#endif
		return false;	// removed	
	}

	if( !from )
	{
		memset( to, 0, sizeof( *to ) );
	}
	else
	{
		memcpy( to, from, sizeof( *to ) );
	}
	to->number = number;

	if( !MSG_ReadBits( 1 ) )
	{
		return true; // unchanged
	}

#ifdef MSG_SHOWNET
	if( dump )
	{
		Con_Printf( "%3i: #%-3i ", msg->readcount, to->number );
	}
#endif

	maxFieldNum = MSG_ReadByte();

#ifdef MSG_SHOWNET
	if( dump )
	{
		Con_Printf( "<%i> ", maxFieldNum );
	}
#endif

	if( maxFieldNum > esTableSize )
	{
		Host_EndGame("MSG_ReadDeltaEntity: maxFieldNum > esTableSize");
	}

	for( i=0, field=esFieldTable ; i<maxFieldNum ; i++, field++ )
	{
		if( !MSG_ReadBits( 1 ) )
			continue; // field unchanged

		if( !MSG_ReadBits( 1 ) )
		{
			FIELD_INTEGER( to ) = 0;
#ifdef MSG_SHOWNET
			if( dump )
			{
				Con_Printf( "%s:%i ", field->name, 0 );
			}
#endif	
			continue; // field set to zero
		}

		if( field->bits )
		{
			to_integer = MSG_ReadBits( field->bits );
			FIELD_INTEGER( to ) = to_integer;
#ifdef MSG_SHOWNET
			if( dump )
			{
				Con_Printf( "%s:%i ", field->name, to_integer );
			}
#endif	
			continue;	// integer value
		}

	
		if( !MSG_ReadBits( 1 ) )
		{
			to_integer = MSG_ReadBits( 13 ) - 0x1000;
			FIELD_FLOAT( to ) = (float)to_integer;
#ifdef MSG_SHOWNET
			if( dump )
			{
				Con_Printf( "%s:%i ", field->name, to_integer );
			}
#endif		
		}
		else
		{
			FIELD_INTEGER( to ) = MSG_ReadLong();
#ifdef MSG_SHOWNET
			if( dump )
			{
				Con_Printf( "%s:%f ", field->name, FIELD_FLOAT( to ) );
			}
#endif
		}
	}

#ifdef MSG_SHOWNET
	if( dump )
	{
		Con_Printf( " (%i bits)\n", msg->bit - startbits );
	}
#endif

	return true;
}
#endif

/*
============
MSG_WriteDeltaEntity

  If 'force' parm is false, this won't result any bits
  emitted if entity didn't changed at all

  'from' == NULL  -->  nodelta update
  'to'   == NULL  -->  entity removed
============
*/
#ifndef CLIENTONLY
void MSGQ3_WriteDeltaEntity(sizebuf_t *msg, const q3entityState_t *from, const q3entityState_t *to, qboolean force)
{
	const q3field_t	*field;
	int				to_value;
	int				to_integer;
	float			to_float;
	int				maxFieldNum;
	int				i;

	if(!to)
	{
		if(from)
		{
			MSG_WriteBits(msg, from->number, GENTITYNUM_BITS);
			MSG_WriteBits(msg, 1, 1);
		}
		return; // removed
	}

	if(to->number < 0 || to->number > MAX_GENTITIES)
		SV_Error("MSG_WriteDeltaEntity: Bad entity number: %i", to->number);

	if(!from)
		from = &nullEntityState; // nodelta update

	//
	// find last modified field in table
	//
	maxFieldNum = 0;
	for(i=0, field=esFieldTable; i<esTableSize; i++, field++ )
	{
		if( FIELD_INTEGER( from ) != FIELD_INTEGER(to))
			maxFieldNum = i + 1;
	}

	if(!maxFieldNum)
	{
		if(!force)
			return; // don't emit any bits at all

		MSG_WriteBits(msg, to->number, GENTITYNUM_BITS);
		MSG_WriteBits(msg, 0, 1);
		MSG_WriteBits(msg, 0, 1);
		return; // unchanged
	}

	MSG_WriteBits(msg, to->number, GENTITYNUM_BITS);
	MSG_WriteBits(msg, 0, 1);
	MSG_WriteBits(msg, 1, 1);
	MSG_WriteBits(msg, maxFieldNum, 8);

	//
	// write all modified fields
	//
	for(i=0, field=esFieldTable; i<maxFieldNum ; i++, field++)
	{
		to_value = FIELD_INTEGER(to);
		
		if(FIELD_INTEGER(from) == to_value)
		{
			MSG_WriteBits( msg, 0, 1 );
			continue; // field unchanged
		}
		MSG_WriteBits(msg, 1, 1);

		if(!to_value)
		{
			MSG_WriteBits(msg, 0, 1);
			continue; // field set to zero
		}
		MSG_WriteBits(msg, 1, 1);

		if(field->bits)
		{
			MSG_WriteBits(msg, to_value, field->bits);
			continue; // integer value
		}

		//
		// figure out how to pack float value
		//
		to_float = FIELD_FLOAT(to);
		to_integer = (int)to_float;

#ifdef MSG_PROFILING
		msg_vectorsEmitted++;
#endif // MSG_PROFILING

		if((float)to_integer == to_float
			&& to_integer + MAX_SNAPPED/2 >= 0
			&& to_integer + MAX_SNAPPED/2 < MAX_SNAPPED)
		{
			MSG_WriteBits(msg, 0, 1 ); // pack in 13 bits
			MSG_WriteBits(msg, to_integer + MAX_SNAPPED/2, SNAPPED_BITS);

#ifdef MSG_PROFILING
			msg_vectorsCompressed++;
#endif // MSG_PROFILING

		} else {
			MSG_WriteBits(msg, 1, 1 ); // pack in 32 bits
			MSG_WriteBits(msg, to_value, 32);
		}
	}
}
#endif





/////////////////////////////////////////////////////
//player state


//
// playerState_t
//
static const q3field_t psFieldTable[] = {
	PS_FIELD( commandTime,			32 ),
	PS_FIELD( origin[0],			 0 ),
	PS_FIELD( origin[1],			 0 ),
	PS_FIELD( bobCycle,				 8 ),
	PS_FIELD( velocity[0],			 0 ),
	PS_FIELD( velocity[1],			 0 ),
	PS_FIELD( viewangles[1],		 0 ),
	PS_FIELD( viewangles[0],		 0 ),
	PS_FIELD( weaponTime,		   -16 ),
	PS_FIELD( origin[2],			 0 ),
	PS_FIELD( velocity[2],			 0 ),
	PS_FIELD( legsTimer,			 8 ),
	PS_FIELD( pm_time,			   -16 ),
	PS_FIELD( eventSequence,		16 ),
	PS_FIELD( torsoAnim,			 8 ),
	PS_FIELD( movementDir,			 4 ),
	PS_FIELD( events[0],			 8 ),
	PS_FIELD( legsAnim,				 8 ),
	PS_FIELD( events[1],			 8 ),
	PS_FIELD( pm_flags,				16 ),
	PS_FIELD( groundEntityNum,		10 ),
	PS_FIELD( weaponstate,			 4 ),
	PS_FIELD( eFlags,				16 ),
	PS_FIELD( externalEvent,		10 ),
	PS_FIELD( gravity,				16 ),
	PS_FIELD( speed,				16 ),
	PS_FIELD( delta_angles[1],		16 ),
	PS_FIELD( externalEventParm,	 8 ),
	PS_FIELD( viewheight,			-8 ),
	PS_FIELD( damageEvent,			 8 ),
	PS_FIELD( damageYaw,			 8 ),
	PS_FIELD( damagePitch,			 8 ),
	PS_FIELD( damageCount,			 8 ),
	PS_FIELD( generic1,				 8 ),
	PS_FIELD( pm_type,				 8 ),
	PS_FIELD( delta_angles[0],		16 ),
	PS_FIELD( delta_angles[2],		16 ),
	PS_FIELD( torsoTimer,			12 ),
	PS_FIELD( eventParms[0],		 8 ),
	PS_FIELD( eventParms[1],		 8 ),
	PS_FIELD( clientNum,			 8 ),
	PS_FIELD( weapon,				 5 ),
	PS_FIELD( viewangles[2],		 0 ),
	PS_FIELD( grapplePoint[0],		 0 ),
	PS_FIELD( grapplePoint[1],		 0 ),
	PS_FIELD( grapplePoint[2],		 0 ),
	PS_FIELD( jumppad_ent,			10 ),
	PS_FIELD( loopSound,			16 )
};
static const int psTableSize = sizeof( psFieldTable ) / sizeof( psFieldTable[0] );
q3playerState_t nullPlayerState;

/*
============
MSG_WriteDeltaPlayerstate

  'from' == NULL  -->  nodelta update
  'to'   == NULL  -->  do nothing
============
*/
#ifndef CLIENTONLY
void MSGQ3_WriteDeltaPlayerstate(sizebuf_t *msg, const q3playerState_t *from, const q3playerState_t *to)
{
	const q3field_t	*field;
	int				to_value;
	float			to_float;
	int				to_integer;
	int				maxFieldNum;
	int				statsMask;
	int				persistantMask;
	int				ammoMask;
	int				powerupsMask;	
	int				i;

	if(!to)
	{
		return;
	}

	if(!from)
	{
		from = &nullPlayerState; // nodelta update
	}

	//
	// find last modified field in table
	//
	maxFieldNum = 0;
	for(i=0, field=psFieldTable ; i<psTableSize ; i++, field++)
	{
		if(FIELD_INTEGER(from) != FIELD_INTEGER(to))
		{
			maxFieldNum = i + 1;
		}
	}

	MSG_WriteBits(msg, maxFieldNum, 8);

	//
	// write all modified fields
	//
	for( i=0, field=psFieldTable ; i<maxFieldNum ; i++, field++ )
	{
		to_value = FIELD_INTEGER( to );
		
		if( FIELD_INTEGER( from ) == to_value )
		{
			MSG_WriteBits( msg, 0, 1 );
			continue; // field unchanged
		}
		MSG_WriteBits( msg, 1, 1 );

		if( field->bits )
		{
			MSG_WriteBits( msg, to_value, field->bits ); 
			continue; // integer value
		}

		//
		// figure out how to pack float value
		//
		to_float = FIELD_FLOAT( to );
		to_integer = (int)to_float;

#ifdef MSG_PROFILING
		msg_vectorsEmitted++;
#endif // MSG_PROFILING

		if( (float)to_integer == to_float
			&& to_integer + MAX_SNAPPED/2 >= 0
			&& to_integer + MAX_SNAPPED/2 < MAX_SNAPPED )
		{
			MSG_WriteBits( msg, 0, 1 ); // pack in 13 bits
			MSG_WriteBits( msg, to_integer + MAX_SNAPPED/2, SNAPPED_BITS );

#ifdef MSG_PROFILING
			msg_vectorsCompressed++;
#endif // MSG_PROFILING

		} else {
			MSG_WriteBits(msg, 1, 1); // pack in 32 bits
			MSG_WriteBits(msg, to_value, 32);
		}
	}

	//
	// find modified arrays
	//
	statsMask = 0;
	for(i=0; i<MAX_Q3_STATS; i++)
	{
		if(from->stats[i] != to->stats[i])
			statsMask |= (1 << i);
	}

	persistantMask = 0;
	for(i=0 ; i<MAX_Q3_PERSISTANT ; i++)
	{
		if(from->persistant[i] != to->persistant[i])
			persistantMask |= (1 << i);
	}

	ammoMask = 0;
	for(i=0 ; i<MAX_Q3_WEAPONS ; i++ )
	{
		if(from->ammo[i] != to->ammo[i])
			ammoMask |= (1 << i);
	}

	powerupsMask = 0;
	for( i=0 ; i<MAX_Q3_POWERUPS ; i++ )
	{
		if(from->powerups[i] != to->powerups[i])
			powerupsMask |= (1 << i);
	}

	if(!statsMask && !persistantMask && !ammoMask && !powerupsMask)
	{
		MSG_WriteBits(msg, 0, 1);
		return; // no arrays modified
	}

	//
	// write all modified arrays
	//
	MSG_WriteBits(msg, 1, 1);

	// PS_STATS
	if(statsMask)
	{
		MSG_WriteBits(msg, 1, 1);
		MSG_WriteBits(msg, statsMask, 16);
		for(i=0; i<MAX_Q3_STATS; i++)
			if(statsMask & (1 << i))
				MSG_WriteBits(msg, to->stats[i], -16);
	}
	else
		MSG_WriteBits(msg, 0, 1); // unchanged

	// PS_PERSISTANT
	if(persistantMask)
	{
		MSG_WriteBits(msg, 1, 1);
		MSG_WriteBits(msg, persistantMask, 16);
		for(i=0; i<MAX_Q3_PERSISTANT; i++)
			if(persistantMask & (1 << i))
				MSG_WriteBits(msg, to->persistant[i], -16);
	}
	else
		MSG_WriteBits(msg, 0, 1); // unchanged


	// PS_AMMO
	if( ammoMask )
	{
		MSG_WriteBits(msg, 1, 1);
		MSG_WriteBits(msg, ammoMask, 16);
		for(i=0; i<MAX_Q3_WEAPONS; i++)
			if(ammoMask & (1 << i))
				MSG_WriteBits(msg, to->ammo[i], 16);
	}
	else
		MSG_WriteBits(msg, 0, 1); // unchanged

	// PS_POWERUPS
	if(powerupsMask)
	{
		MSG_WriteBits(msg, 1, 1);
		MSG_WriteBits(msg, powerupsMask, 16);
		for(i=0; i<MAX_Q3_POWERUPS; i++)
		{
			if(powerupsMask & (1 << i))
				MSG_WriteBits(msg, to->powerups[i], 32); // WARNING: powerups use 32 bits, not 16
		}
	}
	else
		MSG_WriteBits( msg, 0, 1 ); // unchanged
}
#endif

#ifndef SERVERONLY
void MSG_Q3_ReadDeltaPlayerstate( const q3playerState_t *from, q3playerState_t *to ) {
	const q3field_t	*field;
	int				to_integer;
	int				maxFieldNum;
	int				bitmask;
#ifdef MSG_SHOWNET
	int				startbits;
	qboolean		dump;
	qboolean		moredump;
#endif
	int				i;

	if( !to )
	{
		return;
	}

#ifdef MSG_SHOWNET
	dump = (qboolean)(cl_shownet->integer >= 2);
	moredump = (qboolean)(cl_shownet->integer >= 4);

	if( dump )
	{
		startbits = msg->bit;

		Com_Printf( "%3i: playerstate ", msg->readcount );
	}
#endif

	if( !from )
	{
		memset( to, 0, sizeof( *to ) );
	}
	else
	{
		memcpy( to, from, sizeof( *to ) );
	}

	maxFieldNum = MSG_ReadByte();

	if( maxFieldNum > psTableSize )
	{
		Host_EndGame( "MSG_ReadDeltaPlayerstate: maxFieldNum > psTableSize" );
	}

	for( i=0, field=psFieldTable ; i<maxFieldNum ; i++, field++ )
	{
		if(!MSG_ReadBits(1))
		{
			continue; // field unchanged
		}

		if( field->bits )
		{
			to_integer = MSG_ReadBits(field->bits);
			FIELD_INTEGER( to ) = to_integer;
#ifdef MSG_SHOWNET
			if( dump )
			{
				Com_Printf( "%s:%i ", field->name, to_integer );
			}
#endif	
			continue;	// integer value
		}

	
		if(!MSG_ReadBits(1))
		{
			to_integer = MSG_ReadBits(13) - 0x1000;
			FIELD_FLOAT( to ) = (float)to_integer;
#ifdef MSG_SHOWNET
			if( dump )
			{
				Com_Printf( "%s:%i ", field->name, to_integer );
			}
#endif		
		}
		else
		{
			FIELD_INTEGER( to ) = MSG_ReadLong();
#ifdef MSG_SHOWNET
			if( dump )
			{
				Com_Printf( "%s:%f ", field->name, FIELD_FLOAT( to ) );
			}
#endif
		}
	}

	if( MSG_ReadBits(1) )
	{
		// PS_STATS
		if( MSG_ReadBits(1) )
		{ 
#ifdef MSG_SHOWNET
			if( moredump )
			{
				Com_Printf( "PS_STATS " );
			}
#endif
			bitmask = MSG_ReadBits(16);
			for( i=0 ; i<MAX_Q3_STATS ; i++ )
			{
				if( bitmask & (1 << i) )
				{
					to->stats[i] = (signed short)MSG_ReadBits(-16);
				}
			}
		}

		// PS_PERSISTANT
		if( MSG_ReadBits(1 ) )
		{
#ifdef MSG_SHOWNET
			if( moredump )
			{
				Com_Printf( "PS_PERSISTANT " );
			}
#endif

			bitmask = MSG_ReadBits(16);
			for( i=0 ; i<MAX_Q3_PERSISTANT ; i++ )
			{
				if( bitmask & (1 << i) )
				{
					to->persistant[i] = (signed short)MSG_ReadBits(-16);
				}
			}
		}

		// PS_AMMO
		if( MSG_ReadBits(1) )
		{
#ifdef MSG_SHOWNET
			if( moredump )
			{
				Com_Printf( "PS_AMMO " );
			}
#endif

			bitmask = MSG_ReadBits(16);
			for( i=0 ; i<MAX_Q3_WEAPONS ; i++ )
			{
				if( bitmask & (1 << i) )
				{
					to->ammo[i] = (signed short)MSG_ReadBits(16);
				}
			}
		}

		// PS_POWERUPS
		if( MSG_ReadBits(1) )
		{
#ifdef MSG_SHOWNET
			if( moredump ) {
				Com_Printf( "PS_POWERUPS " );
			}
#endif

			bitmask = MSG_ReadBits(16);
			for( i=0 ; i<MAX_Q3_POWERUPS ; i++ )
			{
				if( bitmask & (1 << i) )
				{
					to->powerups[i] = MSG_ReadLong();
				}
			}
		}
	}

#ifdef MSG_SHOWNET
	if( dump )
	{
		Com_Printf( "       (%i bits)\n", msg->bit - startbits );
	}
#endif
}
#endif

////////////////////////////////////////////////////////////
//user commands

int kbitmask[] = {
	0,
	0x00000001, 0x00000003, 0x00000007, 0x0000000F,
	0x0000001F,	0x0000003F,	0x0000007F,	0x000000FF,
	0x000001FF,	0x000003FF,	0x000007FF,	0x00000FFF,
	0x00001FFF,	0x00003FFF,	0x00007FFF,	0x0000FFFF,
	0x0001FFFF,	0x0003FFFF,	0x0007FFFF,	0x000FFFFF,
	0x001FFFFf,	0x003FFFFF,	0x007FFFFF,	0x00FFFFFF,
	0x01FFFFFF,	0x03FFFFFF,	0x07FFFFFF,	0x0FFFFFFF,
	0x1FFFFFFF,	0x3FFFFFFF,	0x7FFFFFFF,	0xFFFFFFFF,
};

static int MSG_ReadDeltaKey(int key, int from, int bits)
{
	if (MSG_ReadBits(1))
		return MSG_ReadBits(bits) ^ (key & kbitmask[bits]);
	else
		return from;
}
void MSG_Q3_ReadDeltaUsercmd(int key, const usercmd_t *from, usercmd_t *to)
{
	if (MSG_ReadBits(1))
		to->servertime = MSG_ReadBits(8) + from->servertime;
	else
		to->servertime = MSG_ReadBits(32);
	to->msec = 0;	//first of a packet should always be an absolute value, which makes the old value awkward.

	if (!MSG_ReadBits(1))
	{
		to->angles[0] = from->angles[0];
		to->angles[1] = from->angles[1];
		to->angles[2] = from->angles[2];
		to->forwardmove = from->forwardmove;
		to->sidemove = from->sidemove;
		to->upmove = from->upmove;
		to->buttons = from->buttons;
		to->weapon = from->weapon;
	}
	else
	{
		key ^= to->servertime;
		to->angles[0]	= MSG_ReadDeltaKey(key, from->angles[0],	16);
		to->angles[1]	= MSG_ReadDeltaKey(key, from->angles[1],	16);
		to->angles[2]	= MSG_ReadDeltaKey(key, from->angles[2],	16);
		//yeah, this is messy
		to->forwardmove	= (signed char)(unsigned char)MSG_ReadDeltaKey(key, (unsigned char)(signed char)from->forwardmove,	8);
		to->sidemove	= (signed char)(unsigned char)MSG_ReadDeltaKey(key, (unsigned char)(signed char)from->sidemove,		8);
		to->upmove	= (signed char)(unsigned char)MSG_ReadDeltaKey(key, (unsigned char)(signed char)from->upmove,		8);
		to->buttons		= MSG_ReadDeltaKey(key, from->buttons,		16);
		to->weapon		= MSG_ReadDeltaKey(key, from->weapon,		8);
	}
}

#endif
