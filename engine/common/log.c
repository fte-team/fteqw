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

	// cmd line options, debug options
#ifdef CRAZYDEBUGGING
	Cvar_ForceSet(&log_enable[LOG_CONSOLE], "1");
	TRACE(("dbg: Con_Init: log_enable forced\n"));
#endif

	if (COM_CheckParm("-condebug"))
		Cvar_ForceSet(&log_enable[LOG_CONSOLE], "1");
}
