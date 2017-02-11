// log.c: handles console logging functions and cvars

#include "quakedef.h"

// cvar callbacks
static void QDECL Log_Dir_Callback (struct cvar_s *var, char *oldvalue);
static void QDECL Log_Name_Callback (struct cvar_s *var, char *oldvalue);

// cvars
#define CONLOGGROUP "Console logging"
cvar_t		log_enable[LOG_TYPES]	= {	CVARF("log_enable", "0", CVAR_NOTFROMSERVER),
										CVARF("log_enable_players", "0", CVAR_NOTFROMSERVER),
										CVARF("log_enable_rcon", "1", CVAR_NOTFROMSERVER)
								};
cvar_t		log_name[LOG_TYPES] = { CVARFC("log_name", "", CVAR_NOTFROMSERVER, Log_Name_Callback),
									CVARFC("log_name_players", "players", CVAR_NOTFROMSERVER, Log_Name_Callback),
									CVARFC("log_name_rcon", "rcon", CVAR_NOTFROMSERVER, Log_Name_Callback)};
cvar_t		log_dir = CVARFC("log_dir", "", CVAR_NOTFROMSERVER, Log_Dir_Callback);
cvar_t		log_readable = CVARFD("log_readable", "7", CVAR_NOTFROMSERVER, "Bitfield describing what to convert/strip. If 0, exact byte representation will be used.\n&1: Dequakify text.\n&2: Strip special markup.\n&4: Strip ansi control codes.");
cvar_t		log_developer = CVARFD("log_developer", "0", CVAR_NOTFROMSERVER, "Enables logging of console prints when set to 1. Otherwise unimportant messages will not fill up your log files.");
cvar_t		log_rotate_files = CVARF("log_rotate_files", "0", CVAR_NOTFROMSERVER);
cvar_t		log_rotate_size = CVARF("log_rotate_size", "131072", CVAR_NOTFROMSERVER);
cvar_t		log_timestamps = CVARF("log_timestamps", "1", CVAR_NOTFROMSERVER);
#ifdef _WIN32
cvar_t		log_dosformat = CVARF("log_dosformat", "1", CVAR_NOTFROMSERVER);
#else
cvar_t		log_dosformat = CVARF("log_dosformat", "0", CVAR_NOTFROMSERVER);
#endif
qboolean	log_newline[LOG_TYPES];

// externals
extern char gamedirfile[];

// Log_Dir_Callback: called when a log_dir is changed
static void QDECL Log_Dir_Callback (struct cvar_s *var, char *oldvalue)
{
	char *t = var->string;
	char *e = t + (*t?strlen(t):0);

	// sanity check for directory. // is equivelent to /../ on some systems, so make sure that can't be used either. : is for drives on windows or amiga, or alternative thingies on windows, so block thoses completely.
	if (strstr(t, "..") || strstr(t, ":") || *t == '/' || *t == '\\' || *e == '/' || *e == '\\' || strstr(t, "//") || strstr(t, "\\\\"))
	{
		Con_Printf(CON_NOTICE "%s forced to default due to invalid characters.\n", var->name);
		// recursion is avoided by assuming the default value is sane
		Cvar_ForceSet(var, var->defaultstr);
	}
}

// Log_Name_Callback: called when a log_name is changed
static void QDECL Log_Name_Callback (struct cvar_s *var, char *oldvalue)
{
	char *t = var->string;

	// sanity check for directory
	if (strstr(t, "..") || strstr(t, ":") || strstr(t, "/") || strstr(t, "\\"))
	{
		Con_Printf(CON_NOTICE "%s forced to default due to invalid characters.\n", var->name);
		// recursion is avoided by assuming the default value is sane
		Cvar_ForceSet(var, var->defaultstr);
	}
}

// Con_Log: log string to console log
void Log_String (logtype_t lognum, char *s)
{
	vfsfile_t *fi;
	char *d; // directory
	char *f; // filename
	char *t;
	char utf8[2048];
	int i;
	char fbase[MAX_QPATH];
	char fname[MAX_QPATH];
	conchar_t cline[2048], *c;
	unsigned int u, flags;

	f = NULL;
	switch(lognum)
	{
	case LOG_CONSOLE:
		f = "qconsole";
		break;
	case LOG_PLAYER:
		f = "players";
		break;
	case LOG_RCON:
		f = "rcon";
		break;
	default:
		return;
	}

	if (!log_enable[lognum].value)
		return;

	// get directory/filename
	if (log_dir.string[0])
		d = log_dir.string;
	else
		d = "";//gamedirfile;

	if (log_name[lognum].string[0])
		f = log_name[lognum].string;

	if (!f)
		return;

	COM_ParseFunString(CON_WHITEMASK, s, cline, sizeof(cline), !(log_readable.ival & 2));
	t = utf8;
	for (c = cline; *c; )
	{
		c = Font_Decode(c, &flags, &u);
		if ((flags & CON_HIDDEN) && (log_readable.ival & 2))
			continue;
		if (log_readable.ival&1)
			u = COM_DeQuake(u);

		//at the start of a new line, we might want a timestamp (so timestamps are correct for the first char of the line, instead of the preceeding \n)
		if (log_newline[lognum])
		{
			if (log_timestamps.ival)
			{
				time_t unixtime = time(NULL);
				int bufferspace = utf8+sizeof(utf8)-1-t;
				if (bufferspace > 0)
				{
					strftime(t, bufferspace, "%Y-%m-%d %H:%M:%S ", localtime(&unixtime));
					t += strlen(t);
				}
			}
			log_newline[lognum] = false;
		}

		//make sure control codes are stripped. no exploiting xterm bugs please.
		if ((log_readable.ival & 4) && ((u < 32 && u != '\t' && u != '\n') || u == 127 || (u >= 128 && u < 128+32))) //\r is stripped too
			u = '?';
		//if dos format logs, we insert a \r before every \n (also flag next char as the start of a new line)
		if (u == '\n')
		{
			log_newline[lognum] = true;
			if (log_dosformat.ival)
				t += utf8_encode(t, '\r', utf8+sizeof(utf8)-1-t);
		}
		t += utf8_encode(t, u, utf8+sizeof(utf8)-1-t);
	}
	*t = 0;

	if (*d)
		Q_snprintfz(fbase, sizeof(fname)-4, "%s/%s", d, f);
	else
		Q_snprintfz(fbase, sizeof(fname)-4, "%s", f);
	Q_snprintfz(fname, sizeof(fname), "%s.log", fbase);

	// file rotation
	if (log_rotate_size.value >= 4096 && log_rotate_files.value >= 1)
	{
		int x;
		vfsfile_t *fi;

		// check file size, use x as temp
		if ((fi = FS_OpenVFS(fname, "rb", FS_GAMEONLY)))
		{
			x = VFS_GETLEN(fi);
			VFS_CLOSE(fi);
			x += strlen(utf8); // add string size to file size to never go over
		}
		else
			x = 0;

		if (x > (int)log_rotate_size.value)
		{
			char newf[MAX_QPATH];
			char oldf[MAX_QPATH];

			i = log_rotate_files.value;

			// unlink file at the top of the chain
			Q_snprintfz(oldf, sizeof(oldf), "%s.%i.log", fbase, i);
			FS_Remove(oldf, FS_GAMEONLY);

			// rename files through chain
			for (x = i-1; x > 0; x--)
			{
				strcpy(newf, oldf);
				Q_snprintfz(oldf, sizeof(oldf), "%s.%i.log", fbase, x);

				// check if file exists, otherwise skip
				if ((fi = FS_OpenVFS(oldf, "rb", FS_GAMEONLY)))
					VFS_CLOSE(fi);
				else
					continue; // skip nonexistant files

				if (!FS_Rename(oldf, newf, FS_GAMEONLY))
				{
					// rename failed, disable log and bug out
					Cvar_ForceSet(&log_enable[lognum], "0");
					Con_Printf("Unable to rotate log files. Logging disabled.\n");
					return;
				}
			}

			// TODO: option to compress file somewhere in here?
			// rename our base file, which had better exist...
			if (!FS_Rename(fname, oldf, FS_GAMEONLY))
			{
				// rename failed, disable log and bug out
				Cvar_ForceSet(&log_enable[lognum], "0");
				Con_Printf("Unable to rename base log file. Logging disabled.\n");
				return;
			}
		}
	}

	FS_CreatePath(fname, FS_GAMEONLY);
	if ((fi = FS_OpenVFS(fname, "ab", FS_GAMEONLY)))
	{
		VFS_WRITE(fi, utf8, strlen(utf8));
		VFS_CLOSE(fi);
	}
	else
	{
		// write failed, bug out
		Cvar_ForceSet(&log_enable[lognum], "0");
		Con_Printf("Unable to write to log file. Logging disabled.\n");
		return;
	}
}

void Con_Log (char *s)
{
	Log_String(LOG_CONSOLE, s);
}


#ifndef CLIENTONLY
//still to add stuff at:
//connects
//disconnects
//kicked
void SV_LogPlayer(client_t *cl, char *msg)
{
	char line[2048];
	char remote_adr[MAX_ADR_SIZE];
	char realip_adr[MAX_ADR_SIZE];

	if (cl->protocol == SCP_BAD)
		return;	//don't log botclients

	snprintf(line, sizeof(line),
			"%s\\%s\\%i\\%s\\%s\\%i\\guid\\%s%s\n",
			msg, cl->name, cl->userid,
			NET_BaseAdrToString(remote_adr, sizeof(remote_adr), &cl->netchan.remote_address), (cl->realip_status > 0 ? NET_BaseAdrToString(realip_adr, sizeof(realip_adr), &cl->realip) : "??"),
			cl->netchan.remote_address.port, cl->guid, cl->userinfo);

	Log_String(LOG_PLAYER, line);
}
#endif





void Log_Logfile_f (void)
{
	extern char gamedirfile[];

	if (log_enable[LOG_CONSOLE].value)
	{
		Cvar_SetValue(&log_enable[LOG_CONSOLE], 0);
		Con_Printf("Logging disabled.\n");
	}
	else
	{
		char *d, *f;

		d = gamedirfile;
		if (log_dir.string[0])
			d = log_dir.string;

		f = "qconsole";
		if (log_name[LOG_CONSOLE].string[0])
			f = log_name[LOG_CONSOLE].string;

		Con_Printf("%s", va("Logging to %s/%s.log.\n", d, f));
		Cvar_SetValue(&log_enable[LOG_CONSOLE], 1);
	}

}
/*
void SV_Fraglogfile_f (void)
{
	char	name[MAX_QPATH];
	int		i;

	if (sv_fraglogfile)
	{
		Con_TPrintf ("Frag file logging off.\n");
		VFS_CLOSE (sv_fraglogfile);
		sv_fraglogfile = NULL;
		return;
	}

	// find an unused name
	for (i=0 ; i<1000 ; i++)
	{
		sprintf (name, "frag_%i.log", i);
		sv_fraglogfile = FS_OpenVFS(name, "rb", FS_GAME);
		if (!sv_fraglogfile)
		{	// can't read it, so create this one
			sv_fraglogfile = FS_OpenVFS (name, "wb", FS_GAME);
			if (!sv_fraglogfile)
				i=1000;	// give error
			break;
		}
		VFS_CLOSE (sv_fraglogfile);
	}
	if (i==1000)
	{
		Con_TPrintf ("Can't open any logfiles.\n");
		sv_fraglogfile = NULL;
		return;
	}

	Con_TPrintf ("Logging frags to %s.\n", name);
}
*/

/*for fuck sake, why can people still not write simple files. proquake is writing binary files as text ones. this function is to try to deal with that fuckup*/
static size_t IPLog_Read_Fucked(qbyte *file, size_t *offset, size_t totalsize, qbyte *out, size_t outsize)
{
	size_t read = 0;
	while (outsize-- > 0 && *offset < totalsize)
	{
		if (file[*offset] == '\r' && *offset+1 < totalsize && file[*offset+1] == '\n')
		{
			out[read] = '\n';
			*offset += 2;
			read += 1;
		}
		else
		{
			out[read] = file[*offset];
			*offset += 1;
			read += 1;
		}
	}
	return read;
}
/*need to make sure any 13 bytes followed by 10s don't bug out when read back in *sigh* */
static size_t IPLog_Write_Fucked(vfsfile_t *file, qbyte *out, size_t outsize)
{
	qbyte tmp[64];
	size_t write = 0;
	size_t block = 0;
	while (outsize-- > 0)
	{
		if (block >= sizeof(tmp)-4)
		{
			VFS_WRITE(file, tmp, block);
			write += block;
			block = 0;
		}
		if (*out == '\n')
			tmp[block++] = '\r';
		tmp[block++] = *out++;
	}
	if (block)
	{
		VFS_WRITE(file, tmp, block);
		write += block;
	}
	return write;
}
static qboolean IPLog_Merge_File(const char *fname)
{
	char ip[MAX_ADR_SIZE];
	char name[256];
	char line[1024];
	vfsfile_t *f;
	if (!*fname)
		fname = "iplog.txt";
	f = FS_OpenVFS(fname, "rb", FS_PUBBASEGAMEONLY);
	if (!f)
		f = FS_OpenVFS(fname, "rb", FS_GAME);
	if (!f)
		return false;
	if (!Q_strcasecmp(COM_FileExtension(fname, name, sizeof(name)), "dat"))
	{	//we don't write this format because of it being limited to ipv4, as well as player name lengths
		size_t l = VFS_GETLEN(f), offset = 0;
		qbyte *ffs = malloc(l+1);
		VFS_READ(f, ffs, l);
		ffs[l] = 0;
		while (IPLog_Read_Fucked(ffs, &offset, l, line, 20) == 20)
		{	//yes, these addresses are weird.
			Q_snprintfz(ip, sizeof(ip), "%i.%i.%i.xxx", (qbyte)line[2], (qbyte)line[1], (qbyte)line[0]);
			memcpy(name, line+4, 20-4);
			name[20-4] = 0;
			IPLog_Add(ip, name);
		}
		free(ffs);
	}
	else
	{
		while (VFS_GETS(f, line, sizeof(line)-1))
		{
			//whether the name contains quotes or what is an awkward one.
			//we always write quotes (including string markup to avoid issues)
			//dp doesn't, and our parser is lazy, so its possible we'll get gibberish that way
			if (COM_ParseOut(COM_ParseOut(line, ip, sizeof(ip)), name, sizeof(name)))
				IPLog_Add(ip, name);
		}
	}
	VFS_CLOSE(f);
	return true;
}
struct iplog_entry
{
	netadr_t adr;
	netadr_t mask;
	char name[1];
} **iplog_entries;
size_t iplog_num, iplog_max;
void IPLog_Add(const char *ipstr, const char *name)
{
	size_t i;
	netadr_t a, m;
	while (*ipstr == ' ' || *ipstr == '\t')
		ipstr++;
	if (*ipstr != '[' && *ipstr < '0' && *ipstr > '9')
		return;
	if (*ipstr == '[')
		ipstr++;
	//some names are dodgy.
	if (!*name 
		//|| !Q_strcasecmp(name, /*nq default*/"player") || !Q_strcasecmp(name, /*qw default*/"unnamed")
		|| !strcmp(name, /*nq fallback*/"unconnected") || !strncmp(name, "BOT:", 4))
		return;
	memset(&a, 0, sizeof(a));
	memset(&m, 0, sizeof(m));
	if (!NET_StringToAdrMasked(ipstr, false, &a, &m))
		return;
	//might be x.y.z.w:port
	//might be x.y.z.FUCKED
	//might be x.y.z.0/24
	//might be [::]:port
	//might be [::]/bits
	//or other ways to express an ip address

	//FIXME: ignore private addresses?

	//check for dupes
	for (i = 0; i < iplog_num; i++)
	{
		if (!memcmp(&a, &iplog_entries[i]->adr, sizeof(netadr_t)) && !memcmp(&m, &iplog_entries[i]->mask, sizeof(netadr_t)) && !Q_strcasecmp(name, iplog_entries[i]->name))
			return;
	}

	//looks like its new...
	if (iplog_num == iplog_max)
		Z_ReallocElements((void**)&iplog_entries, &iplog_max, iplog_max+64, sizeof(*iplog_entries));
	iplog_entries[iplog_num] = BZ_Malloc(sizeof(struct iplog_entry) + strlen(name));
	iplog_entries[iplog_num]->adr = a;
	iplog_entries[iplog_num]->mask = m;
	strcpy(iplog_entries[iplog_num]->name, name);
	iplog_num++;
}
static void IPLog_Identify(netadr_t *adr, netadr_t *mask, char *fmt, ...)
{
	va_list		argptr;

	qboolean found = false;
	char line[256];
	size_t i;
		
	va_start(argptr, fmt);
	vsnprintf(line, sizeof(line), fmt, argptr);
	va_end(argptr);
	Con_Printf("%s: ", line);

	for (i = 0; i < iplog_num; i++)
	{
		if (NET_CompareAdrMasked(adr, &iplog_entries[i]->adr, mask?mask:&iplog_entries[i]->mask))
		{
			if (found)
				Con_Printf(", ");
			found=true;
			Con_Printf("%s", iplog_entries[i]->name);
		}
	}
	if (!found)
		Con_Printf("<no matches>");
	Con_Printf("\n");
}
#include "cl_ignore.h"
static void IPLog_Identify_f(void)
{
	const char *nameorip = Cmd_Argv(1);
	netadr_t adr, mask;
	char clean[256];
	//if *, use a mask that includes all ips
	if (NET_StringToAdrMasked (nameorip, false, &adr, &mask))
	{	//try to parse as an ip
		//treading carefully here, to avoid dns name lookups weirding everything out.
		IPLog_Identify(&adr, &mask, "Identity of %s", NET_AdrToStringMasked(clean, sizeof(clean), &adr, &mask));
	}
#ifndef CLIENTONLY
	else if (sv.active)
	{	//if server is active, walk players to see if there's a name match to get their address and guess an address mask
		client_t *cl;
		int clnum = -1;
		while((cl = SV_GetClientForString(nameorip, &clnum)))
		{
			if (cl->realip_status)
			{
				IPLog_Identify(&cl->realip, NULL, "Identity of %s (real) [%s]", cl->name, NET_AdrToString(clean, sizeof(clean), &cl->realip));
				IPLog_Identify(&cl->netchan.remote_address, NULL, "Identity of %s (proxy) [%s]", cl->name, NET_AdrToString(clean, sizeof(clean), &cl->realip));
			}
			else
				IPLog_Identify(&cl->netchan.remote_address, NULL, "Identity of %s [%s]", cl->name, NET_AdrToString(clean, sizeof(clean), &cl->realip));
		}
	}
#endif
#ifndef SERVERONLY
	else if (cls.state >= ca_connected)
	{	//else if client is active, walk players to see if there's a name match, to get their address+mask if known via nq hacks
		int slot;
		netadr_t adr;
		if ((slot = Player_StringtoSlot(nameorip)) < 0)
			Con_Printf("%s: no player with userid %s\n", Cmd_Argv(0), nameorip);
		else if (!*cl.players[slot].ip)
			Con_Printf("%s: ip address of %s is not known\n", Cmd_Argv(0), cl.players[slot].name);
		else
		{
			NET_StringToAdr(cl.players[slot].ip, 0, &adr);
			IPLog_Identify(&adr, NULL, "Identity of %s [%s]", cl.players[slot].name, cl.players[slot].ip);
		}
	}
#endif
	else
		Con_Printf("%s: not connected, nor raw address\n", Cmd_Argv(0));
}
static qboolean IPLog_Dump(const char *fname)
{
	size_t i;
	vfsfile_t *f;
	qbyte line[20];
	if (!*fname)
		fname = "iplog.txt";

	f = FS_OpenVFS(fname, "wb", FS_PUBBASEGAMEONLY);
	if (!f)
		return false;
	if (!Q_strcasecmp(COM_FileExtension(fname, line, sizeof(line)), "dat"))
	{
		for (i = 0; i < iplog_num; i++)
		{
			//this shitty format supports only ipv4.
			if (iplog_entries[i]->adr.type != NA_IP)
				continue;
			line[0] = iplog_entries[i]->adr.address.ip[2];
			line[1] = iplog_entries[i]->adr.address.ip[1];
			line[2] = iplog_entries[i]->adr.address.ip[0];
			line[3] = 0;
			strncpy(line+4, iplog_entries[i]->name, sizeof(line)-4);
			IPLog_Write_Fucked(f, line, sizeof(line));	//convert \n to \r\n, to avoid fucking up any formatting with binary data (inside the address part, so *.13.10.* won't corrupt the file)
		}
	}
	else
	{
		VFS_PRINTF(f, "//generated by "FULLENGINENAME"\n");
		for (i = 0; i < iplog_num; i++)
		{
			char ip[512];
			char buf[1024];
			char buf2[1024];
			VFS_PRINTF(f, log_dosformat.value?"%s %s\r\n":"%s %s\n", COM_QuotedString(NET_AdrToStringMasked(ip, sizeof(ip), &iplog_entries[i]->adr, &iplog_entries[i]->mask), buf2, sizeof(buf2), false), COM_QuotedString(iplog_entries[i]->name, buf, sizeof(buf), false));
		}
	}
	VFS_CLOSE(f);
	return true;
}
static void IPLog_Dump_f(void)
{
	char native[MAX_OSPATH];
	const char *fname = Cmd_Argv(1);
	if (FS_NativePath(fname, FS_GAMEONLY, native, sizeof(native)))
		Q_strncpyz(native, fname, sizeof(native));
	IPLog_Merge_File(fname);	//merge from the existing file, so that we're hopefully more robust if multiple processes are poking the same file.
	if (!IPLog_Dump(fname))
		Con_Printf("unable to write %s\n", fname);
	else
	{
		Con_Printf("wrote %s\n", native);
	}
}
static void IPLog_Merge_f(void)
{
	const char *fname = Cmd_Argv(1);
	if (!IPLog_Merge_File(fname))
		Con_Printf("unable to read %s\n", fname);
}
void Log_ShutDown(void)
{
	IPLog_Dump("iplog.txt");
//	IPLog_Dump("iplog.dat");

	while(iplog_num > 0)
	{
		iplog_num--;
		BZ_Free(iplog_entries[iplog_num]);
	}
	BZ_Free(iplog_entries);
	iplog_entries = NULL;
	iplog_max = iplog_num = 0;
}

void Log_Init(void)
{
	int i;
	// register cvars
	for (i = 0; i < LOG_TYPES; i++)
	{
		Cvar_Register (&log_enable[i], CONLOGGROUP);
		Cvar_Register (&log_name[i], CONLOGGROUP);
		log_newline[i] = true;
	}
	Cvar_Register (&log_dir, CONLOGGROUP);
	Cvar_Register (&log_readable, CONLOGGROUP);
	Cvar_Register (&log_developer, CONLOGGROUP);
	Cvar_Register (&log_rotate_size, CONLOGGROUP);
	Cvar_Register (&log_rotate_files, CONLOGGROUP);
	Cvar_Register (&log_dosformat, CONLOGGROUP);
	Cvar_Register (&log_timestamps, CONLOGGROUP);

	Cmd_AddCommand("logfile", Log_Logfile_f);

	Cmd_AddCommand("identify", IPLog_Identify_f);
	Cmd_AddCommand("ipmerge", IPLog_Merge_f);
	Cmd_AddCommand("ipdump", IPLog_Dump_f);

	// cmd line options, debug options
#ifdef CRAZYDEBUGGING
	Cvar_ForceSet(&log_enable[LOG_CONSOLE], "1");
	TRACE(("dbg: Con_Init: log_enable forced\n"));
#endif

	if (COM_CheckParm("-condebug"))
		Cvar_ForceSet(&log_enable[LOG_CONSOLE], "1");
}
