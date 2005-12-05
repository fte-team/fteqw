// log.c: handles console logging functions and cvars

#include "quakedef.h"

// cvars
#define CONLOGGROUP "Console logging"
cvar_t		log_name = {"log_name", "", NULL, CVAR_NOTFROMSERVER};
cvar_t		log_dir = {"log_dir", "", NULL, CVAR_NOTFROMSERVER};
cvar_t		log_readable = {"log_readable", "0", NULL, CVAR_NOTFROMSERVER};
cvar_t		log_enable = {"log_enable", "0", NULL, CVAR_NOTFROMSERVER};
cvar_t		log_developer = {"log_developer", "0", NULL, CVAR_NOTFROMSERVER};
cvar_t		log_rotate_files = {"log_rotate_files", "0", NULL, CVAR_NOTFROMSERVER};
cvar_t		log_rotate_size = {"log_rotate_size", "131072", NULL, CVAR_NOTFROMSERVER};
cvar_t		log_dosformat = {"log_dosformat", "0", NULL, CVAR_NOTFROMSERVER};

// externals
int COM_FileSize(char *path);
extern char gamedirfile[];
extern char *com_basedir;

// table of readable characters, same as ezquake
char readable[256] = 
{
	'.', '_', '_', '_', '_', '.', '_', '_', 
	'_', '_', '\n', '_', '\n', '>', '.', '.', 
	'[', ']', '0', '1', '2', '3', '4', '5', 
	'6', '7', '8', '9', '.', '_', '_', '_', 
	' ', '!', '\"', '#', '$', '%', '&', '\'', 
	'(', ')', '*', '+', ',', '-', '.', '/', 
	'0', '1', '2', '3', '4', '5', '6', '7', 
	'8', '9', ':', ';', '<', '=', '>', '?', 
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 
	'X', 'Y', 'Z', '[', '\\', ']', '^', '_', 
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 
	'x', 'y', 'z', '{', '|', '}', '~', '_', 
	'_', '_', '_', '_', '_', '.', '_', '_', 
	'_', '_', '_', '_', '_', '>', '.', '.', 
	'[', ']', '0', '1', '2', '3', '4', '5', 
	'6', '7', '8', '9', '.', '_', '_', '_', 
	' ', '!', '\"', '#', '$', '%', '&', '\'', 
	'(', ')', '*', '+', ',', '-', '.', '/', 
	'0', '1', '2', '3', '4', '5', '6', '7', 
	'8', '9', ':', ';', '<', '=', '>', '?', 
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 
	'X', 'Y', 'Z', '[', '\\', ']', '^', '_', 
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 
	'x', 'y', 'z', '{', '|', '}', '~', '_'
};

// Con_Log: log string to console log
void Con_Log (char *s)
{
	char *d; // directory
	char *f; // filename
	char *t;
	char logbuf[1024];
	int i;

	if (!log_enable.value)
		return;

	// cvar sanity checks
	if (log_dir.modified)
	{
		t = log_dir.string;
		if (strstr(t, "..") || strstr(t, ":") || *t == '/' || *t == '\\')
		{
			Con_Printf("log_dir forced to default due to invalid characters.\n");
			Cvar_ForceSet(&log_dir, log_dir.defaultstr);
		}

		log_dir.modified = false;
	}

	if (log_name.modified)
	{
		t = log_name.string;
		if (strstr(t, "..") || strstr(t, ":") || strstr(t, "/") || strstr(t, "\\"))
		{
			Con_Printf("log_name forced to default due to invalid characters.\n");
			Cvar_ForceSet(&log_name, log_name.defaultstr);
		}

		log_name.modified = false;
	}

	// get directory/filename
	d = gamedirfile;
	if (log_dir.string[0])
		d = log_dir.string;
	
	f = "qconsole";
	if (log_name.string[0])
		f = log_name.string;

	// readable translation and Q3 code removal, use t for final string to write
	t = logbuf;
	// max debuglog buf is 1024
	for (i = 0; i < 1023; i++, s++)
	{
		if (*s == 0)
			break;
		else if (((int)(log_readable.value) & 2) && *s == '^')
		{
			// log_readable 2 removes Q3 codes as well
			char c;
			c = *(s+1);

			if ((c >= '0' && c < '8') || c == 'a' || c == 'b' || c == 's' || c == 'r')
			{
				i--;
				s++;
			}
			else
			{
				*t = '^';
				t++;
			}
		}
		else if (log_dosformat.value && *s == '\n')
		{
			// convert \n to \r\n
			*t = '\r';
			t++;
			i++;
			if (i < 1023)
			{
				*t = '\n';
				t++;
			}
		}
		else
		{
			// use readable table to convert quake chars to reabable text
			if ((int)(log_readable.value) & 1)
				*t = readable[(unsigned char)(*s)]; // translate
			else
				*t = *s; // copy
			t++;
		}
	}

	*t = 0;

	f = va("%s/%s/%s.log",com_basedir,d,f); // temp string in va()

	// file rotation
	if (log_rotate_size.value >= 4096 && log_rotate_files.value >= 1) 
	{
		int x;
		FILE *fi;

		// check file size, use x as temp
		if ((fi = fopen(f, "rb")))
		{
			x = COM_filelength(fi);
			fclose(fi);
		}
		else
			x = 0;

		if (x > (int)log_rotate_size.value)
		{
			char newf[MAX_OSPATH];
			char oldf[MAX_OSPATH];

			i = log_rotate_files.value;
		
			// unlink file at the top of the chain
			_snprintf(oldf, sizeof(oldf)-1, "%s.%i", f, i);
			unlink(oldf);

			// rename files through chain
			for (x = i-1; x > 0; x--)
			{
				strcpy(newf, oldf);
				_snprintf(oldf, sizeof(oldf)-1, "%s.%i", f, x);

				// check if file exists, otherwise skip
				if ((fi = fopen(oldf, "rb")))
					fclose(fi);
				else
					continue; // skip nonexistant files

				if (rename(oldf, newf))
				{
					// rename failed, disable log and bug out
					Cvar_ForceSet(&log_enable, "0");
					Con_Printf("Unable to rotate log files. Logging disabled.\n");
					return;
				}
			}

			// TODO: option to compress file somewhere in here?
			// rename our base file, which better exist...
			if (rename(f, oldf))
			{
				// rename failed, disable log and bug out
				Cvar_ForceSet(&log_enable, "0");
				Con_Printf("Unable to rename base log file. Logging disabled.\n");
				return;
			}
		}
	}

	// write to log file
	if (Sys_DebugLog(f, "%s", logbuf))
	{
		// write failed, bug out
		Cvar_ForceSet(&log_enable, "0");
		Con_Printf("Unable to write to log file. Logging disabled.\n");
		return;
	}
}

void Log_Init(void)
{
	// register cvars
	Cvar_Register (&log_name, CONLOGGROUP);
	Cvar_Register (&log_dir, CONLOGGROUP);
	Cvar_Register (&log_readable, CONLOGGROUP);
	Cvar_Register (&log_enable, CONLOGGROUP);
	Cvar_Register (&log_developer, CONLOGGROUP);
	Cvar_Register (&log_rotate_size, CONLOGGROUP);
	Cvar_Register (&log_rotate_files, CONLOGGROUP);
	Cvar_Register (&log_dosformat, CONLOGGROUP);

	// cmd line options, debug options
#ifdef CRAZYDEBUGGING
	Cvar_ForceSet(&log_enable, "1");
	TRACE(("dbg: Con_Init: log_enable forced\n"));
#endif

	if (COM_CheckParm("-condebug"))
		Cvar_ForceSet(&log_enable, "1");
}