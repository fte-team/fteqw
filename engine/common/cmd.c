/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// cmd.c -- Quake script command processing module

#include "quakedef.h"

cvar_t com_fs_cache			= SCVARF("fs_cache", IFMINIMAL("2","1"), CVAR_ARCHIVE);
cvar_t rcon_level			= SCVAR("rcon_level", "20");
cvar_t cmd_maxbuffersize	= SCVAR("cmd_maxbuffersize", "65536");
int	Cmd_ExecLevel;

void Cmd_ForwardToServer (void);

#define	MAX_ALIAS_NAME	32

typedef struct cmdalias_s
{
	struct cmdalias_s	*next;
	char	name[MAX_ALIAS_NAME];
	char	*value;
	qbyte execlevel;
	qbyte restriction;
	int flags;
} cmdalias_t;

#define ALIAS_FROMSERVER	1

cmdalias_t	*cmd_alias;

cvar_t cl_warncmd			= SCVAR("cl_warncmd", "0");
cvar_t cl_aliasoverlap		= SCVARF("cl_aliasoverlap", "1", CVAR_NOTFROMSERVER);

cvar_t tp_disputablemacros	= SCVARF("tp_disputablemacros", "1", CVAR_SEMICHEAT);


//=============================================================================










#define MAX_MACROS 64

typedef struct {
	char name[32];
	char *(*func) (void);
	int disputableintentions;
} macro_command_t;

static macro_command_t macro_commands[MAX_MACROS];
static int macro_count = 0;

void Cmd_AddMacro(char *s, char *(*f)(void), int disputableintentions)
{
	int i;
	for (i = 0; i < macro_count; i++)
	{
		if (!strcmp(macro_commands[i].name, s))
			break;
	}

	if (i == MAX_MACROS)
		Sys_Error("Cmd_AddMacro: macro_count == MAX_MACROS");

	Q_strncpyz(macro_commands[macro_count].name, s, sizeof(macro_commands[macro_count].name));
	macro_commands[macro_count].func = f;
	macro_commands[macro_count].disputableintentions = disputableintentions;

	if (i == macro_count)
		macro_count++;
}

char *TP_MacroString (char *s, int *len)
{
	int i;
	macro_command_t	*macro;

	for (i = 0; i < macro_count; i++)
	{
		macro = &macro_commands[i];
		if (!Q_strcasecmp(s, macro->name))
		{
			if (macro->disputableintentions)
				if (!tp_disputablemacros.ival)
					continue;
			if (len)
				*len = strlen(macro->name);
			return macro->func();
		}
	}
	return NULL;
}

void Cmd_MacroList_f (void)
{
	int	i;

	if (!macro_count)
	{
		Con_Printf("No macros!");
		return;
	}

	for (i = 0; i < macro_count; i++)
		Con_Printf ("$%s\n", macro_commands[i].name);
}








/*
=============================================================================

						COMMAND BUFFER

=============================================================================
*/

struct {
	sizebuf_t	buf;
	int noclear;
	double waitattime;
} cmd_text[RESTRICT_MAX+1+MAX_SPLITS];	//max is local.
							//RESTRICT_MAX+1 is the from sever buffer (max+2 is for second player...)

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
============
*/
void Cmd_Wait_f (void)
{
	cmd_text[Cmd_ExecLevel].waitattime = realtime;
}

/*
============
Cbuf_Init
============
*/
void Cbuf_Init (void)
{
	int level;
	for (level = 0; level <= RESTRICT_MAX+1; level++)
		cmd_text[level].waitattime = -1;
}

/*
============
Cbuf_AddText

Adds command text at the end of the buffer
============
*/
void Cbuf_AddText (const char *text, int level)
{
	int		l;

	if (!strcmp(text, "cmd "))
		Con_Printf("cmd text\n");

	if (level > sizeof(cmd_text)/sizeof(cmd_text[0]) || level < 0)
	{
		Con_Printf("Bad execution level\n");
		return;	//reject.
	}

	l = Q_strlen (text);

	if (!cmd_text[level].buf.maxsize)
	{
		cmd_text[level].buf.data = (qbyte*)BZ_Malloc(8192);
		cmd_text[level].buf.maxsize = 8192;
	}
	if (cmd_text[level].buf.cursize + l >= cmd_text[level].buf.maxsize)
	{
		int newmax;

		newmax = cmd_text[level].buf.maxsize*2;

		if (newmax > cmd_maxbuffersize.ival && cmd_maxbuffersize.ival)
		{
			Con_TPrintf (TL_FUNCOVERFLOW, "Cbuf_AddText");
			return;
		}
		while (newmax < cmd_text[level].buf.cursize + l)
			newmax*=2;
		cmd_text[level].buf.data = (qbyte*)BZ_Realloc(cmd_text[level].buf.data, newmax);
		cmd_text[level].buf.maxsize = newmax;
	}
	SZ_Write (&cmd_text[level].buf, text, Q_strlen (text));
}


/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
FIXME: actually change the command buffer to do less copying
============
*/
void Cbuf_InsertText (const char *text, int level, qboolean addnl)
{
	char	*temp;
	int		templen;

	if (level > sizeof(cmd_text)/sizeof(cmd_text[0]) || level < 0)
	{
		Con_Printf("Bad execution level\n");
		return;	//reject.
	}

// copy off any commands still remaining in the exec buffer
	templen = cmd_text[level].buf.cursize;
	if (templen)
	{
		temp = (char*)Z_Malloc (templen+1);
		Q_memcpy (temp, cmd_text[level].buf.data, templen);
		SZ_Clear (&cmd_text[level].buf);
	}
	else
		temp = NULL;	// shut up compiler

// add the entire text of the file
	Cbuf_AddText (text, level);
	if (addnl)
		SZ_Write (&cmd_text[level].buf, "\n", 1);
// add the copied off data
	if (templen)
	{
		temp[templen] = '\0';
		Cbuf_AddText(temp, level);
//		SZ_Write (&cmd_text[level].buf, temp, templen);
		Z_Free (temp);
	}
}

char *Cbuf_GetNext(int level)
{
	int		i;
	char	*text;
	int		quotes;
	static char	line[1024];

start:

	text = (char *)cmd_text[level].buf.data;

	quotes = 0;
	for (i=0 ; i< cmd_text[level].buf.cursize ; i++)
	{
		if (text[i] == '"')
			quotes++;
		if ( !(quotes&1) &&  text[i] == ';')
			break;	// don't break if inside a quoted string
		if (text[i] == '\n')
			break;
	}

	if (i >= sizeof(line)-1)
	{
		Con_Printf("Statement too long\n");
		return "";
	}


	memcpy (line, text, i);
	line[i] = 0;

// delete the text from the command buffer and move remaining commands down
// this is necessary because commands (exec, alias) can insert data at the
// beginning of the text buffer

	if (i == cmd_text[level].buf.cursize)
		cmd_text[level].buf.cursize = 0;
	else
	{
		i++;
		cmd_text[level].buf.cursize -= i;
		Q_memcpy (text, text+i, cmd_text[level].buf.cursize);
	}

//	Con_Printf("Found \"%s\"\n", line);
	text=line;
	while(*text == ' ' || *text == '\t')
		text++;

	if (!*text)
		if (cmd_text[level].buf.cursize)
			goto start;	//should be a while.

	return text;
}

char *Cbuf_StripText(int level)	//remove all text in the command buffer and return it (so it can be readded later)
{
	char *buf;
	buf = (char*)Z_Malloc(cmd_text[level].buf.cursize+1);
	Q_memcpy (buf, cmd_text[level].buf.data, cmd_text[level].buf.cursize);
	cmd_text[level].buf.cursize = 0;
	return buf;
}

void Cbuf_ExecuteLevel (int level)
{
	int		i;
	char	*text;
	char	line[1024];
	qboolean	quotes, comment;

	while (cmd_text[level].buf.cursize)
	{
		if (cmd_text[level].waitattime == realtime)
		{	// skip out while text still remains in buffer, leaving it
			// for next frame
			break;
		}

// find a \n or ; line break
		text = (char *)cmd_text[level].buf.data;

		quotes = false;
		comment = false;
		for (i=0 ; i< cmd_text[level].buf.cursize ; i++)
		{
			if (text[i] == '\n')
				break;
			if (text[i] == '"')
			{
				quotes++;
				continue;
			}

			if (comment || (quotes&1))
				continue;

			if (text[i] == '/' && i+1 < cmd_text[level].buf.cursize && text[i+1] == '/')
				comment = true;
			else if (text[i] == ';')
				break;	// don't break if inside a quoted string
		}

		if (i >= sizeof(line))
			i = sizeof(line)-1;
		memcpy (line, text, i);
		line[i] = 0;

// delete the text from the command buffer and move remaining commands down
// this is necessary because commands (exec, alias) can insert data at the
// beginning of the text buffer

		if (i == cmd_text[level].buf.cursize)
			cmd_text[level].buf.cursize = 0;
		else
		{
			i++;
			cmd_text[level].buf.cursize -= i;
			Q_memcpy (text, text+i, cmd_text[level].buf.cursize);
		}

// execute the command line
		Cmd_ExecuteString (line, level);
	}
}

/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute (void)
{
	int level;

	for (level = 0; level < sizeof(cmd_text)/sizeof(cmd_text[0]); level++)
		Cbuf_ExecuteLevel(level);
}

/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/

/*
===============
Cmd_StuffCmds_f

Adds command line parameters as script statements
Commands lead with a +, and continue until a - or another +
quake +prog jctest.qp +cmd amlev1
quake -nosound +cmd amlev1
===============
*/
void Cmd_StuffCmds (void)
{
	int		i, j;
	int		s;
	char	*text, *build, c;


// build the combined string to parse from
	s = 0;
	for (i=1 ; i<com_argc ; i++)
	{
		if (!com_argv[i])
			continue;		// NEXTSTEP nulls out -NXHost
		s += Q_strlen (com_argv[i]) + 3;
	}
	if (!s)
		return;

	text = (char*)Z_Malloc (s+1);
	text[0] = 0;
	for (i=1 ; i<com_argc ; i++)
	{
		if (!com_argv[i])
			continue;		// NEXTSTEP nulls out -NXHost
		if (strchr(com_argv[i], ' ') || strchr(com_argv[i], '\t') || strchr(com_argv[i], '@'))
		{
			Q_strcat (text,"\"");
			Q_strcat (text,com_argv[i]);
			Q_strcat (text,"\"");
		}
		else
			Q_strcat (text,com_argv[i]);
		if (i != com_argc-1)
			Q_strcat (text, " ");
	}

// pull out the commands
	build = (char*)Z_Malloc (s+1);
	build[0] = 0;

	for (i=0 ; i<s-1 ; i++)
	{
		if (text[i] == '+')
		{
			i++;

			for (j=i ; ((text[j-1] != ' ') || ((text[j] != '+') && (text[j] != '-'))) && (text[j] != 0) ; j++)
				;

			c = text[j];
			text[j] = 0;

			Q_strcat (build, text+i);
			Q_strcat (build, "\n");
			text[j] = c;
			i = j-1;
		}
	}

	if (build[0])
		Cbuf_AddText (build, RESTRICT_LOCAL);

	Z_Free (text);
	Z_Free (build);
}


/*
===============
Cmd_Exec_f
===============
*/
void Cmd_Exec_f (void)
{
	char	*f;
	char	name[256];

	if (Cmd_Argc () != 2)
	{
		Con_TPrintf (TL_EXECCOMMANDUSAGE);
		return;
	}


	if (!strcmp(Cmd_Argv(0), "cfg_load"))
	{
		f = Cmd_Argv(1);
		if (!*f)
			f = "fte";
		snprintf(name, sizeof(name)-5, "configs/%s", f);
		COM_DefaultExtension(name, ".cfg", sizeof(name));
	}
	else
		Q_strncpyz(name, Cmd_Argv(1), sizeof(name));

	FS_LoadFile(name, &f);
	if (FS_LoadFile(name, &f) != -1)
		;
	else if (FS_LoadFile(va("%s.cfg", name), &f) != -1)
		;
	else
	{
		Con_TPrintf (TL_EXECFAILED,name);
		return;
	}
	if (cl_warncmd.ival || developer.ival)
		Con_TPrintf (TL_EXECING,name);

	// don't execute anything as if it was from server
	Cbuf_InsertText (f, Cmd_FromGamecode() ? RESTRICT_INSECURE : Cmd_ExecLevel, true);
	FS_FreeFile(f);
}


/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
void Cmd_Echo_f (void)
{
	int		i;

	for (i=1 ; i<Cmd_Argc() ; i++)
		Con_Printf ("%s ",Cmd_Argv(i));
	Con_Printf ("\n");
}

char *CopyString (char *in)
{
	char	*out;

	out = (char*)Z_Malloc (strlen(in)+1);
	strcpy (out, in);
	return out;
}

void Cmd_ShowAlias_f (void)
{
	cmdalias_t	*a;
	char *s;

	s = Cmd_Argv(1);

	//find it, print it
	for (a = cmd_alias ; a ; a=a->next)
	{
		if (!strcmp(s, a->name))
		{
			Con_Printf ("alias %s %s\n", a->name, a->value);
			return;
		}
	}

	Con_Printf("Alias doesn't exist\n");
}

/*
===============
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; seperated)
===============
*/

void Cmd_Alias_f (void)
{
	cmdalias_t	*a, *b;
	char		cmd[1024];
	int			i, c;
	char		*s;
	qboolean multiline;

	if (Cmd_Argc() == 1)	//list em all.
	{
		if (Cmd_FromGamecode())
		{
			if (Cmd_ExecLevel==RESTRICT_SERVER)
			{
				Con_TPrintf (TL_CURRENTALIASCOMMANDS);
				for (a = cmd_alias ; a ; a=a->next)
				{
					if (a->flags & ALIAS_FROMSERVER)
						Con_Printf ("%s : %s\n", a->name, a->value);
				}
			}
		}
		else
		{
			Con_TPrintf (TL_CURRENTALIASCOMMANDS);
			for (a = cmd_alias ; a ; a=a->next)
			{
	/*			extern int con_linewidth;
				if (strlen(a->value)+strlen(a->name)+3 > con_linewidth)
					Con_Printf ("%s ...\n", a->name);
				else*/
					Con_Printf ("%s : %s\n", a->name, a->value);
			}
		}
		return;
	}

	s = Cmd_Argv(1);
	if (strlen(s) >= MAX_ALIAS_NAME || !strcmp(s, "say"))	//reject aliasing the say command. We use it as an easy way to warn that our player is cheating.
	{
		Con_TPrintf (TL_ALIASNAMETOOLONG);
		return;
	}

	if (!cl_aliasoverlap.value)
	{
		if (Cvar_FindVar (s))
		{
			if (Cmd_FromGamecode())
			{
				snprintf(cmd, sizeof(cmd), "%s_a", s);
				Con_Printf ("Can't register alias, %s is a cvar\nAlias has been named %s instead\n", s, cmd);
				s = cmd;
			}
			else
			{
				Con_Printf ("Can't register alias, %s is a cvar\n", s);
				return;
			}
		}

	// check for overlap with a command
		if (Cmd_Exists (s))
		{
			if (Cmd_FromGamecode())
			{
				snprintf(cmd, sizeof(cmd), "%s_a", s);
				Con_Printf ("Can't register alias, %s is a command\nAlias has been named %s instead\n", s, cmd);
				s = cmd;
			}
			else
			{
				Con_Printf ("Can't register alias, %s is a command\n", s);
				return;
			}
		}
	}

	// if the alias already exists, reuse it
	for (a = cmd_alias ; a ; a=a->next)
	{
		if (!strcmp(s, a->name))
		{
			if ((a->restriction?a->restriction:rcon_level.ival) > Cmd_ExecLevel)
			{
				Con_TPrintf (TL_ALIASRESTRICTIONLEVELERROR);
				return;
			}

			if (!stricmp(Cmd_Argv(0), "newalias"))
				return;	//newalias command only registers the alias if it is new, and does not change it if it already exists

			Z_Free (a->value);
			break;
		}
	}

	if (!a)
	{
		a = (cmdalias_t*)Z_Malloc (sizeof(cmdalias_t));
		a->next = cmd_alias;
		cmd_alias = a;
	}
	if (Cmd_FromGamecode())
		a->flags |= ALIAS_FROMSERVER;
	else
		a->flags &= ~ALIAS_FROMSERVER;

	strcpy (a->name, s);
	multiline = false;
	if (Cmd_Argc() == 2)	//check the next statement for being '{'
	{
		char *line, *end;
		line = Cbuf_GetNext(Cmd_ExecLevel);

		while(*line <= ' ' && *line)	//skip leading whitespace.
			line++;

		for (end = line + strlen(line)-1; end >= line && *end <= ' '; end--)	//skip trailing
			*end = '\0';
		if (!strcmp(line, "{"))
			multiline = true;
		else
			Cbuf_InsertText(line, Cmd_ExecLevel, true);	//whoops. Stick the trimmed string back in to the cbuf.
	}
	else if (!strcmp(Cmd_Argv(2), "{"))
		multiline = true;

	if (multiline)
	{	//fun! MULTILINE ALIASES!!!!
		char *newv;
		char *end;
		int in = 1;
		a->value = NULL;
		for(;;)
		{
			s = Cbuf_GetNext(Cmd_ExecLevel);
			if (!*s)
			{
				Con_Printf(CON_WARNING "WARNING: Multiline alias was not terminated\n");
				break;
			}
			while (*s <= ' ' && *s)
				s++;
			for (end = s + strlen(s)-1; end >= s && *end <= ' '; end--)
				*end = '\0';
			if (!strcmp(s, "{"))
				in++;
			else if (!strcmp(s, "}"))
			{
				in--;
				if (!in)
					break;	//phew
			}
			if (a->value)
			{
				newv = (char*)Z_Malloc(strlen(a->value) + strlen(s) + 2);
				sprintf(newv, "%s;%s", a->value, s);
				Z_Free(a->value);
				a->value = newv;
			}
			else
				a->value = CopyString(s);
		}

		return;
	}

// copy the rest of the command line
	cmd[0] = 0;		// start out with a null string
	c = Cmd_Argc();
	for (i=2 ; i< c ; i++)
	{
		strcat (cmd, Cmd_Argv(i));
		if (i != c-1)
			strcat (cmd, " ");
	}

	if (!*cmd)	//someone wants to wipe it. let them
	{
		if (a == cmd_alias)
		{
			cmd_alias = a->next;
			Z_Free(a);
			return;
		}
		else
		{
			for (b = cmd_alias ; b ; b=b->next)
			{
				if (b->next == a)
				{
					b->next = a->next;
					Z_Free(a);
					return;
				}
			}
		}
	}
	if (Cmd_FromGamecode())
	{
		a->execlevel = RESTRICT_SERVER;	//server-set aliases MUST run at the server's level.
		a->restriction = 1;				//and be runnable at the user's level
	}
	else
	{
		a->execlevel = 0;	//run at users exec level
		a->restriction = 1;	//this is possibly a security risk if the admin also changes execlevel
	}
	a->value = CopyString (cmd);
}

void Cmd_DeleteAlias(char *name)
{
	cmdalias_t	*a, *b;
	if (!strcmp(cmd_alias->name, name))
	{
		a = cmd_alias;
		cmd_alias = cmd_alias->next;
		Z_Free(a);
		return;
	}
	for (a = cmd_alias ; a ; a=a->next)
	{
		if (!strcmp(a->next->name, name))
		{
			b = a->next;
			a->next = b->next;
			Z_Free(b);
			return;
		}
	}
}

char *Cmd_AliasExist(char *name, int restrictionlevel)
{
	cmdalias_t	*a;
	// if the alias already exists, reuse it
	for (a = cmd_alias ; a ; a=a->next)
	{
		if (!strcmp(name, a->name))
		{
			if ((a->restriction?a->restriction:rcon_level.ival) > restrictionlevel)
			{
				return NULL;	//not at this level...
			}
			return a->value;
		}
	}
	return NULL;
}

void Cmd_AliasLevel_f (void)
{
	cmdalias_t	*a;
	char *s = Cmd_Argv(1);
	int level;
	if (Cmd_Argc() < 2 || Cmd_Argc() > 3)
	{
		Con_TPrintf(TL_ALIASLEVELCOMMANDUSAGE);
		return;
	}

	for (a = cmd_alias ; a ; a=a->next)
	{
		if (!strcmp(s, a->name))
		{
			break;
		}
	}
	if (!a)
	{
		Con_TPrintf(TL_ALIASNOTFOUND);
		return;
	}

	if (Cmd_Argc() == 3)
	{
		level = atoi(Cmd_Argv(2));
		if (level > RESTRICT_MAX)
		{
			level = RESTRICT_MAX;
		}
		else if (level < RESTRICT_MIN)
			level = RESTRICT_MIN;

		if (level > Cmd_ExecLevel || (a->restriction?a->restriction:rcon_level.ival) > Cmd_ExecLevel)
		{
			Con_TPrintf(TL_ALIASRAISELEVELERROR);
			return;
		}

		a->execlevel = level;

		if (a->restriction == 1)
			Con_TPrintf(TL_ALIASRESTRICTIONLEVELWARN, a->name);
	}
	else
		Con_TPrintf(TL_ALIASRESTRICTLEVEL, s, a->execlevel);
}

//lists commands, also prints restriction level
void Cmd_AliasList_f (void)
{
	cmdalias_t	*cmd;
	int num=0;
	int flags;

	if (!strcmp(Cmd_Argv(1), "server"))
		flags = ALIAS_FROMSERVER;
	else
		flags = 0;

	for (cmd=cmd_alias ; cmd ; cmd=cmd->next)
	{
		if ((cmd->restriction?cmd->restriction:rcon_level.ival) > Cmd_ExecLevel)
			continue;
		if (flags && !(cmd->flags & flags))
			continue;
		if (!num)
			Con_TPrintf(TL_ALIASLIST);
		if (cmd->execlevel)
			Con_Printf("(%2i)(%2i) %s\n", (int)(cmd->restriction?cmd->restriction:rcon_level.ival), cmd->execlevel, cmd->name);
		else
			Con_Printf("(%2i)     %s\n", (int)(cmd->restriction?cmd->restriction:rcon_level.ival), cmd->name);
		num++;
	}
	if (num)
		Con_Printf("\n");
}

void Alias_WriteAliases (vfsfile_t *f)
{
	char *s;
	cmdalias_t	*cmd;
	int num=0;
	for (cmd=cmd_alias ; cmd ; cmd=cmd->next)
	{
//		if ((cmd->restriction?cmd->restriction:rcon_level.ival) > Cmd_ExecLevel)
//			continue;
		if (cmd->flags & ALIAS_FROMSERVER)
			continue;
		if (!num)
		{
			s = va("\n//////////////////\n//Aliases\n");
			VFS_WRITE(f, s, strlen(s));
		}
		s = va("alias %s \"%s\"\n", cmd->name, cmd->value);
		VFS_WRITE(f, s, strlen(s));
		if (cmd->restriction != 1)	//1 is default
		{
			s = va("restrict %s %i\n", cmd->name, cmd->restriction);
			VFS_WRITE(f, s, strlen(s));
		}
		if (cmd->execlevel != 0)	//0 is default (runs at user's level)
		{
			s = va("aliaslevel %s %i\n", cmd->name, cmd->execlevel);
			VFS_WRITE(f, s, strlen(s));
		}
		num++;
	}
}

void Alias_WipeStuffedAliaes(void)
{
	cmdalias_t	*cmd, *n;
	for (cmd=cmd_alias ; cmd ; )
	{
		if (cmd->flags & ALIAS_FROMSERVER)
		{
			n = cmd->next;
			Cmd_DeleteAlias(cmd->name);
			cmd = n;
		}
		else
			cmd=cmd->next;
	}
}

void Cvar_List_f (void);
void Cvar_Reset_f (void);

/*
=============================================================================

					COMMAND EXECUTION

=============================================================================
*/

typedef struct cmd_function_s
{
	struct cmd_function_s	*next;
	char					*name;
	xcommand_t				function;

	qbyte	restriction;	//restriction of admin level
	qbyte	zmalloced;
} cmd_function_t;


#define	MAX_ARGS		80

static	int			cmd_argc;
static	char		*cmd_argv[MAX_ARGS];
static	char		*cmd_null_string = "";
static	char		*cmd_args = NULL, *cmd_args_buf;



static	cmd_function_t	*cmd_functions;		// possible commands to execute

/*
============
Cmd_Argc
============
*/
int		VARGS Cmd_Argc (void)
{
	return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
char	*VARGS Cmd_Argv (int arg)
{
	if ( arg >= cmd_argc )
		return cmd_null_string;
	return cmd_argv[arg];
}

/*
============
Cmd_Args

Returns a single string containing argv(1) to argv(argc()-1)
============
*/

char *VARGS Cmd_Args (void)
{
	if (!cmd_args)
		return "";
	return cmd_args;
}

void Cmd_Args_Set(char *newargs)
{
	if (cmd_args_buf)
		Z_Free(cmd_args_buf);

	if (newargs)
	{
		cmd_args_buf = (char*)Z_Malloc (Q_strlen(newargs)+1);
		Q_strcpy (cmd_args_buf, newargs);
		cmd_args = cmd_args_buf;
	}
	else
	{
		cmd_args = NULL;
		cmd_args_buf = NULL;
	}
}

/*
============
Cmd_ShiftArgs

Shifts Cmd_Argv results down one (killing first param)
============
*/
void Cmd_ShiftArgs (int ammount, qboolean expandstring)
{
	int arg;
	while (ammount>0 && cmd_argc)
	{
		arg=0;
		cmd_argc--;
		Z_Free(cmd_argv[0]);
		while ( arg < cmd_argc )
		{
			cmd_argv[arg] = cmd_argv[arg+1];
			arg++;
		}
		cmd_argv[arg]=NULL;

		ammount--;

		if (cmd_args)
		{
			cmd_args = COM_StringParse(cmd_args, expandstring, false);
			if (cmd_args)
				while(*cmd_args == ' ' || *cmd_args == '\t')
					cmd_args++;
		}
	}
}

char *Cmd_ExpandCvar(char *cvarname, int maxaccesslevel, int *len)
{
	char *ret = NULL, *end, *namestart;
	char *fixup = NULL, fixval=0;
	cvar_t	*var;
	static char temp[12];

	namestart = cvarname;
	if (*cvarname == '{')
	{
		fixup = &cvarname[strlen(cvarname)-1];
		if (*fixup != '}')
			return NULL;
		fixval = *fixup;
		*fixup = 0;
		cvarname++;
	}
	else
	{
		fixup = &cvarname[strlen(cvarname)];
		fixval = *fixup;
	}

	strtol(cvarname, &end, 10);
	if (fixval && *end == '\0') //only expand $0 if its actually ${0} - this avoids conflicting with the $0 macro
	{	//purely numerical
		ret = Cmd_Argv(atoi(cvarname));
	}
	else if (!strcmp(cvarname, "*") || !stricmp(cvarname, "cmd_args"))
	{
		ret = Cmd_Args();
	}
	else if (!strnicmp(cvarname, "cmd_argv", 8))
	{
		ret = Cmd_Argv(atoi(cvarname+8));
	}
	else if (!stricmp(cvarname, "cmd_argc"))
	{
		Q_snprintfz(temp, sizeof(temp), "%u", Cmd_Argc());
		ret = temp;
	}
	else if ( (var = Cvar_FindVar(cvarname)) != NULL )
	{
		if (var->restriction <= maxaccesslevel && !((var->flags & CVAR_NOUNSAFEEXPAND) && Cmd_IsInsecure()))
		{
			ret = var->string;
		}
	}
	*fixup = fixval;
	if (ret)
	{
		*len = fixup - namestart;
		if (fixval)
			(*len)++;
	}
	return ret;
}

/*
================
Cmd_ExpandString

Expands all $cvar expressions to cvar values
If not SERVERONLY, also expands $macro expressions
Note: dest must point to a 1024 byte buffer
================
*/
char *Cmd_ExpandString (char *data, char *dest, int destlen, int maxaccesslevel, qboolean expandcvars, qboolean expandmacros)
{
	unsigned int	c;
	char	buf[255];
	int		i, len;
	int		quotes = 0;
	char	*str;
	char	*bestmacro, *bestvar;
	int		name_length, macro_length, var_length;
	qboolean striptrailing;

	len = 0;

	while ( (c = *data) != 0)
	{
		if (c == '"')
			quotes++;

		if (c == '$' && !(quotes&1))
		{
			data++;

			striptrailing = (*data == '-')?true:false;

			// Copy the text after '$' to a temp buffer
			i = 0;
			buf[0] = 0;
			buf[1] = 0;
			bestvar = NULL;
			bestmacro = NULL;
			macro_length=0;
			var_length = 0;
			while ((c = *data) > 32)
			{
				if (c == '$')
					break;
				data++;
				buf[i++] = c;
				buf[i] = 0;
				if (!bestmacro)
				{
					if ((str = Cmd_ExpandCvar(buf+striptrailing, maxaccesslevel, &var_length)))
						bestvar = str;
				}
#ifndef SERVERONLY
				if (expandmacros && (str = TP_MacroString (buf+striptrailing, &macro_length)))
					bestmacro = str;
#endif
			}

			if (bestmacro)
			{
				str = bestmacro;
				name_length = macro_length;
			}
			else if (bestvar)
			{
				str = bestvar;
				name_length = var_length;
			}
			else
			{
				str = NULL;
				name_length = 0;
			}

			if (str)
			{
				// check buffer size
				if (len + strlen(str) >= destlen-1)
					break;

				strcpy(&dest[len], str);
				len += strlen(str);
				i = name_length;
				while (buf[i])
					dest[len++] = buf[i++];

				if (striptrailing && !*str)
					while(*data <= ' ' && *data)
						data++;
			}
			else
			{
				// no matching cvar or macro
				dest[len++] = '$';
				if (len + strlen(buf) >= destlen-1)
					break;
				strcpy (&dest[len], buf);
				len += strlen(buf);
			}
		}
		else
		{
			dest[len] = c;
			data++;
			len++;
			if (len >= destlen-1)
				break;
		}
	};

	dest[len] = 0;

	return dest;
}

char *Cmd_ExpandStringArguments (char *data, char *dest, int destlen)
{
	char c;
	int quotes = 0;
	int len = 0;

	char *str, *strend;
	int old_len;
	while ( (c = *data) != 0)
	{
		if (c == '"')
			quotes++;

		if (c == '%' && !(quotes&1))
		{
			if (data[1] == '%')
			{
				str = "%";
				old_len = 2;
			}
			else if (data[1] == '*')
			{
				str = Cmd_Args();
				old_len = 2;
			}
			else if (strtol(data+1, &strend, 10))
			{
				str = Cmd_Argv(atoi(data+1));
				old_len = strend - data;
			}
			else
			{
				str = NULL;
				old_len = 0;
			}
			
			if (str)
			{
				// check buffer size
				if (len + strlen(str) >= destlen-1)
					break;

				strcpy(&dest[len], str);
				len += strlen(str);
				dest[len] = 0;
				data += old_len;

				continue;
			}
		}

		dest[len] = c;
		data++;
		len++;
		dest[len] = 0;
		if (len >= destlen-1)
			break;
	}

	dest[len] = 0;

	return dest;
}

/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
============
*/
void Cmd_TokenizeString (char *text, qboolean expandmacros, qboolean qctokenize)
{
	int		i;

// clear the args from the last string
	for (i=0 ; i<cmd_argc ; i++)
		Z_Free (cmd_argv[i]);

	cmd_argc = 0;
	Cmd_Args_Set(NULL);

	while (1)
	{
// skip whitespace up to a \n
		while (*text && (unsigned)*text <= ' ' && *text != '\n')
		{
			text++;
		}

		if (*text == '\n')
		{	// a newline seperates commands in the buffer
			text++;
			break;
		}

		if (!*text)
			return;

		if (cmd_argc == 1)
		{
			Cmd_Args_Set(text);
		}

		text = COM_StringParse (text, expandmacros, qctokenize);
		if (!text)
			return;

		if (cmd_argc < MAX_ARGS)
		{
			cmd_argv[cmd_argc] = (char*)Z_Malloc (Q_strlen(com_token)+1);
			Q_strcpy (cmd_argv[cmd_argc], com_token);
			cmd_argc++;
		}
	}
}

void Cmd_TokenizePunctation (char *text, char *punctuation)
{
	int		i;

// clear the args from the last string
	for (i=0 ; i<cmd_argc ; i++)
		Z_Free (cmd_argv[i]);

	cmd_argc = 0;
	Cmd_Args_Set(NULL);

	while (1)
	{
// skip whitespace up to a \n
		while (*text && (unsigned)*text <= ' ' && *text != '\n')
		{
			text++;
		}

		if (*text == '\n')
		{	// a newline seperates commands in the buffer
			text++;
			break;
		}

		if (!*text)
			return;

		if (cmd_argc == 1)
		{
			Cmd_Args_Set(text);
		}

		text = COM_ParseToken (text, punctuation);
		if (!text)
			return;

		if (cmd_argc < MAX_ARGS)
		{
			cmd_argv[cmd_argc] = (char*)Z_Malloc (Q_strlen(com_token)+1);
			Q_strcpy (cmd_argv[cmd_argc], com_token);
			cmd_argc++;
		}
	}
}


/*
============
Cmd_AddCommand
============
*/
qboolean	Cmd_AddCommand (char *cmd_name, xcommand_t function)
{
	cmd_function_t	*cmd;

	if (host_initialized)	// because hunk allocation would get stomped
		Sys_Error ("Cmd_AddCommand after host_initialized");

// fail if the command is a variable name
	if (Cvar_VariableString(cmd_name)[0])
	{
		Con_Printf ("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return false;
	}

// fail if the command already exists
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
	{
		if (!Q_strcmp (cmd_name, cmd->name))
		{
			if (cmd->function == function)	//happens a lot with q3
				Con_DPrintf ("Cmd_AddCommand: %s already defined\n", cmd_name);
			else
				Con_Printf ("Cmd_AddCommand: %s already defined\n", cmd_name);
			return false;
		}
	}

	cmd = (cmd_function_t*)Hunk_AllocName (sizeof(cmd_function_t), cmd_name);
	cmd->name = cmd_name;
	cmd->function = function;
	cmd->next = cmd_functions;
	cmd->restriction = 0;
	cmd_functions = cmd;

	return true;
}

qboolean Cmd_AddRemCommand (char *cmd_name, xcommand_t function)
{
	cmd_function_t	*cmd;

// fail if the command is a variable name
	if (Cvar_VariableString(cmd_name)[0])
	{
		Con_Printf ("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return false;
	}

// fail if the command already exists
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
	{
		if (!Q_strcmp (cmd_name, cmd->name))
		{
			if (cmd->function == function)	//happens a lot with q3
				Con_DPrintf ("Cmd_AddCommand: %s already defined\n", cmd_name);
			else
			{
				Con_Printf ("Cmd_AddCommand: %s already defined\n", cmd_name);
				break;
			}
			return false;
		}
	}

	cmd = (cmd_function_t*)Z_Malloc (sizeof(cmd_function_t)+strlen(cmd_name)+1);
	cmd->name = (char*)(cmd+1);
	strcpy(cmd->name, cmd_name);
	cmd->function = function;
	cmd->next = cmd_functions;
	cmd->restriction = 0;
	cmd->zmalloced = true;
	cmd_functions = cmd;

	return true;
}

void	Cmd_RemoveCommand (char *cmd_name)
{
	cmd_function_t	*cmd, **back;

	back = &cmd_functions;
	while (1)
	{
		cmd = *back;
		if (!cmd)
		{
//			Con_Printf ("Cmd_RemoveCommand: %s not added\n", cmd_name);
			return;
		}
		if (!strcmp (cmd_name, cmd->name))
		{
			*back = cmd->next;
			if (!cmd->zmalloced)
			{
				Con_Printf("Cmd_RemoveCommand: %s was not added dynamically\n", cmd_name);
				return;
			}
			Z_Free (cmd);
			return;
		}
		back = &cmd->next;
	}
}

void Cmd_RestrictCommand_f (void)
{
	cmdalias_t *a;
	cvar_t *v;
	cmd_function_t	*cmd;
	char *cmd_name = Cmd_Argv(1);
	int level;

	if (Cmd_Argc() != 3 && Cmd_Argc() != 2)
	{
		Con_Printf("restrict <commandname> [level]\n");
		return;
	}

	if (Cmd_Argc() > 2)
	{
		level = atoi(Cmd_Argv(2));
		if (level > RESTRICT_MAX)
		{
			level = RESTRICT_MAX;
			if (level > Cmd_ExecLevel)
			{
				Con_TPrintf(TL_RESTRICTCOMMANDRAISE);
				return;
			}
		}
		else if (level < RESTRICT_MIN)
			level = RESTRICT_MIN;
	}
	else level = 0;
//commands
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
	{
		if (!Q_strcmp (cmd_name, cmd->name))
		{
			if (Cmd_Argc() == 2)
			{
				if (cmd->restriction)
					Con_TPrintf (TL_RESTRICTCURRENTLEVEL, cmd_name, (int)cmd->restriction);
				else
					Con_TPrintf (TL_RESTRICTCURRENTLEVELDEFAULT, cmd_name, rcon_level.ival);
			}
			else if ((cmd->restriction?cmd->restriction:rcon_level.ival) > Cmd_ExecLevel)
				Con_TPrintf(TL_RESTRICTCOMMANDTOOHIGH);
			else
				cmd->restriction = level;
			return;
		}
	}

//cvars
	v = Cvar_FindVar(cmd_name);
	if (v)
	{
		if (Cmd_Argc() == 2)
		{
			if (v->restriction)
				Con_TPrintf (TL_RESTRICTCURRENTLEVEL, cmd_name, (int)v->restriction);
			else
				Con_TPrintf (TL_RESTRICTCURRENTLEVELDEFAULT, cmd_name, rcon_level.ival);
		}
		else if ((v->restriction?v->restriction:rcon_level.ival) > Cmd_ExecLevel)
			Con_TPrintf(TL_RESTRICTCOMMANDTOOHIGH);
		else
			v->restriction = level;

		return;
	}

	// check alias
	for (a=cmd_alias ; a ; a=a->next)
	{
		if (!Q_strcasecmp (cmd_name, a->name))
		{
			if (Cmd_Argc() == 2)
			{
				if (a->restriction)
					Con_TPrintf (TL_RESTRICTCURRENTLEVEL, cmd_name, (int)a->restriction);
				else
					Con_TPrintf (TL_RESTRICTCURRENTLEVELDEFAULT, cmd_name, rcon_level.ival);
			}
			else if ((a->restriction?a->restriction:rcon_level.ival) > Cmd_ExecLevel)
				Con_TPrintf(TL_RESTRICTCOMMANDTOOHIGH);
			else
				a->restriction = level;
			return;
		}
	}

	Con_TPrintf (TL_RESTRICTNOTDEFINED, cmd_name);
	return;
}

int Cmd_Level(char *name)
{
	cmdalias_t *a;
	cmd_function_t *cmds;
	for (cmds = cmd_functions; cmds; cmds=cmds->next)
	{
		if (!strcmp(cmds->name, name))
		{
			return cmds->restriction?cmds->restriction:rcon_level.ival;
		}
	}
	for (a=cmd_alias ; a ; a=a->next)
	{
		if (!strcmp(a->name, name))
		{
			return a->restriction?a->restriction:Cmd_ExecLevel;
		}
	}
	return -1;
}

/*
============
Cmd_Exists
============
*/
qboolean	Cmd_Exists (char *cmd_name)
{
	cmd_function_t	*cmd;

	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
	{
		if (!Q_strcmp (cmd_name,cmd->name))
			return true;
	}

	return false;
}



/*
============
Cmd_CompleteCommand
============
*/
typedef struct {
	int matchnum;
	qboolean allowcutdown;
	qboolean cutdown;
	char result[256];
} match_t;
void Cmd_CompleteCheck(char *check, match_t *match)	//compare cumulative strings and join the result
{
	if (*match->result)
	{
		char *r;
		if (match->allowcutdown)
		{
			for(r = match->result; *r == *check && *r; r++, check++)
				;
			*r = '\0';
			match->cutdown = true;
			if (match->matchnum > 0)
				match->matchnum--;
		}
		else if (match->matchnum > 0)
		{
			strcpy(match->result, check);
			match->matchnum--;
		}
	}
	else
	{
		if (match->matchnum > 0)
		{
			strcpy(match->result, check);
			match->matchnum--;
		}
		else
			strcpy(match->result, check);
	}
}
char *Cmd_CompleteCommand (char *partial, qboolean fullonly, qboolean caseinsens, int matchnum)
{
	extern cvar_group_t *cvar_groups;
	cmd_function_t	*cmd;
	int				len;
	cmdalias_t		*a;

	static match_t match;

	cvar_group_t	*grp;
	cvar_t		*cvar;

	len = Q_strlen(partial);

	if (!len)
		return NULL;

	if (matchnum == -1)
		len++;

	match.allowcutdown = !fullonly?true:false;
	match.cutdown = false;
	if (matchnum)
		match.matchnum = matchnum;
	else
		match.matchnum = 0;

	strcpy(match.result, "");

	// check for partial match
	if (caseinsens)
	{
		for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
			if (!Q_strncasecmp (partial,cmd->name, len))
				Cmd_CompleteCheck(cmd->name, &match);
		for (a=cmd_alias ; a ; a=a->next)
			if (!Q_strncasecmp (partial, a->name, len))
				Cmd_CompleteCheck(a->name, &match);
		for (grp=cvar_groups ; grp ; grp=grp->next)
		for (cvar=grp->cvars ; cvar ; cvar=cvar->next)
		{
			if (!Q_strncasecmp (partial,cvar->name, len))
				Cmd_CompleteCheck(cvar->name, &match);
			if (cvar->name2 && !Q_strncasecmp (partial,cvar->name2, len))
				Cmd_CompleteCheck(cvar->name2, &match);
		}

	}
	else
	{
		for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
			if (!Q_strncmp (partial,cmd->name, len))
				Cmd_CompleteCheck(cmd->name, &match);
		for (a=cmd_alias ; a ; a=a->next)
			if (!Q_strncmp (partial, a->name, len))
				Cmd_CompleteCheck(a->name, &match);
		for (grp=cvar_groups ; grp ; grp=grp->next)
		for (cvar=grp->cvars ; cvar ; cvar=cvar->next)
		{
			if (!Q_strncmp (partial,cvar->name, len))
				Cmd_CompleteCheck(cvar->name, &match);
			if (cvar->name2 && !Q_strncmp (partial,cvar->name2, len))
				Cmd_CompleteCheck(cvar->name2, &match);
		}
	}
	if (match.matchnum>0)
		return NULL;
	if (!*match.result)
		return NULL;
	return match.result;
}

//lists commands, also prints restriction level
void Cmd_List_f (void)
{
	cmd_function_t	*cmd;
	int num=0;
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
	{
		if ((cmd->restriction?cmd->restriction:rcon_level.ival) > Cmd_ExecLevel)
			continue;
		if (!num)
			Con_TPrintf(TL_COMMANDLISTHEADER);
		Con_Printf("(%2i) %s\n", (int)(cmd->restriction?cmd->restriction:rcon_level.ival), cmd->name);
		num++;
	}
	if (num)
		Con_Printf("\n");
}


#ifndef SERVERONLY		// FIXME
/*
===================
Cmd_ForwardToServer

adds the current command line as a clc_stringcmd to the client message.
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void Cmd_ForwardToServer (void)
{
	if (cls.state == ca_disconnected)
	{
		Con_TPrintf (TL_CANTXNOTCONNECTED, Cmd_Argv(0));
		return;
	}

	if (cls.demoplayback)
		return;		// not really connected

#ifdef Q3CLIENT
	if (cls.protocol == CP_QUAKE3)
	{
		CLQ3_SendClientCommand("%s %s", Cmd_Argv(0), Cmd_Args());
		return;
	}
#endif

	if (Cmd_Argc() > 1)
		CL_SendClientCommand(true, "%s %s", Cmd_Argv(0), Cmd_Args());
	else
		CL_SendClientCommand(true, "%s", Cmd_Argv(0));
}

// don't forward the first argument
void Cmd_ForwardToServer_f (void)
{
	if (cls.state == ca_disconnected)
	{
		Con_TPrintf (TL_CANTXNOTCONNECTED, Cmd_Argv(0));
		return;
	}

	if (Q_strcasecmp(Cmd_Argv(1), "snap") == 0)
	{
		if (SCR_RSShot())
			return;
	}
	if (Q_strcasecmp(Cmd_Argv(1), "ptrack") == 0)
	{
		if (!*Cmd_Argv(2))
		{
			Cam_Unlock(0);
		}
		else
		{
			Cam_Lock(0, atoi(Cmd_Argv(2)));
			autocam[0] = CAM_TRACK;
		}
		return;
	}

	if (Cmd_Argc() > 1)
		CL_SendClientCommand(true, "%s", Cmd_Args());
}
#else
void Cmd_ForwardToServer (void)
{
}
#endif

/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
FIXME: lookupnoadd the token to speed search?
============
*/
void	Cmd_ExecuteString (char *text, int level)
{
	cmd_function_t	*cmd;
	cmdalias_t		*a;

	char dest[8192];

	Cmd_ExecLevel = level;

	text = Cmd_ExpandString(text, dest, sizeof(dest), level, !Cmd_IsInsecure()?true:false, true);
	Cmd_TokenizeString (text, level == RESTRICT_LOCAL?true:false, false);

// execute the command line
	if (!Cmd_Argc())
		return;		// no tokens

// check functions
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
	{
		if (!Q_strcasecmp (cmd_argv[0],cmd->name))
		{
			if (strcmp (cmd_argv[0],cmd->name))
				break;	//yes, I know we found it... (but it's the wrong case, go for an alias or cvar instead FIRST)

			if ((cmd->restriction?cmd->restriction:rcon_level.ival) > level)
				Con_TPrintf(TL_WASRESTIRCTED, cmd_argv[0]);
			else if (!cmd->function)
			{
#ifdef VM_CG
				if (CG_Command())
					return;
#endif
#ifdef Q3SERVER
				if (SVQ3_Command())
					return;
#endif
#ifdef VM_UI
				if (UI_Command())
					return;
#endif
				if (Cmd_AliasExist(cmd_argv[0], level))
					break;	//server stuffed an alias for a command that it would already have received. use that instead.
				Cmd_ForwardToServer ();
			}
			else
				cmd->function ();
			return;
		}
	}

// check alias
	for (a=cmd_alias ; a ; a=a->next)
	{
		if (!Q_strcasecmp (cmd_argv[0], a->name))
		{
			int execlevel;

#ifndef SERVERONLY	//an emergency escape mechansim, to avoid infinatly recursing aliases.
			extern qboolean keydown[];

			if (keydown[K_SHIFT] && keydown[K_CTRL] && keydown[K_ALT])
				return;
#endif

			if ((a->restriction?a->restriction:rcon_level.ival) > level)
			{
				Con_TPrintf(TL_WASRESTIRCTED, cmd_argv[0]);
				return;
			}
			if (a->execlevel)
				execlevel = a->execlevel;
			else
				execlevel = level;

			Cbuf_InsertText ("\n", execlevel, false);

			// if the alias value is a command or cvar and
			// the alias is called with parameters, add them
			if (Cmd_Argc() > 1 && (!strncmp(a->value, "cmd ", 4) || (!strchr(a->value, ' ') && !strchr(a->value, '\t')	&&
				(Cvar_FindVar(a->value) || (Cmd_Exists(a->value) && a->value[0] != '+' && a->value[0] != '-'))))
			)
			{
				Cbuf_InsertText (Cmd_Args(), execlevel, false);
				Cbuf_InsertText (" ", execlevel, false);
			}

			Cmd_ExpandStringArguments (a->value, dest, sizeof(dest));
			Cbuf_InsertText (dest, execlevel, false);
			return;
		}
	}

// check cvars
	if (Cvar_Command (level))
		return;



	if (cmd)	//go for skipped ones
	{
		if ((cmd->restriction?cmd->restriction:rcon_level.ival) > level)
			Con_TPrintf(TL_WASRESTIRCTED, cmd_argv[0]);
		else if (!cmd->function)
			Cmd_ForwardToServer ();
		else
			cmd->function ();

		return;
	}

#ifndef SERVERONLY
#ifdef CSQC_DAT
	if (CSQC_ConsoleCommand(text))
		return;
#endif
#endif

#ifdef PLUGINS
	if (Plugin_ExecuteString())
		return;
#endif

#ifndef CLIENTONLY
	if (sv.state)
	{
		if (PR_ConsoleCmd())
			return;
	}
#endif

#ifdef VM_CG
	if (CG_Command())
		return;
#endif
#ifdef Q3SERVER
	if (SVQ3_Command())
		return;
#endif
#ifdef VM_UI
	if (UI_Command())
		return;
#endif
#ifdef Q2CLIENT
	if (cls.protocol == CP_QUAKE2 || cls.protocol == CP_QUAKE3)
	{	//q2 servers convert unknown commands to text.
		Cmd_ForwardToServer();
		return;
	}
#endif
	if (cl_warncmd.value || developer.value)
		Con_TPrintf (TL_COMMANDNOTDEFINED, Cmd_Argv(0));
}



/*
================
Cmd_CheckParm

Returns the position (1 to argc-1) in the command's argument list
where the given parameter apears, or 0 if not present
================
*/
int Cmd_CheckParm (char *parm)
{
	int i;

	if (!parm)
		Sys_Error ("Cmd_CheckParm: NULL");

	for (i = 1; i < Cmd_Argc (); i++)
		if (! Q_strcasecmp (parm, Cmd_Argv (i)))
			return i;

	return 0;
}
















typedef struct tempstack_s{
	struct tempstack_s *next;
	char str[1];
} tempstack_t;
tempstack_t *ifstack;

void If_Token_Clear (tempstack_t *mark)
{
	tempstack_t *ois;
	while(ifstack)
	{
		if (ifstack == mark)
			break;
		ois = ifstack;
		ifstack = ifstack->next;
		Z_Free(ois);
	}
}

tempstack_t *If_Token_GetMark (void)
{
	return ifstack;
}


const char *retstring(const char *s)
{
//	return s;
	tempstack_t *ret;
	ret = (tempstack_t*)Z_Malloc(sizeof(tempstack_t)+strlen(s));
	ret->next = ifstack;
	ifstack=ret;
	strcpy(ret->str, s);
	return ret->str;
}
const char *retfloat(float f)
{
	char s[1024];
	tempstack_t *ret;
	if (!f)
		return "";
	sprintf(s, "%f", f);
	ret = (tempstack_t*)Z_Malloc(sizeof(tempstack_t)+strlen(s));
	ret->next = ifstack;
	ifstack=ret;
	strcpy(ret->str, s);
	return ret->str;
}
qboolean is_numeric (const char *c)
{
	return (*c >= '0' && *c <= '9') ||
		((*c == '-' || *c == '+') && (c[1] == '.' || (c[1]>='0' && c[1]<='9'))) ||
		(*c == '.' && (c[1]>='0' && c[1]<='9'))?true:false;
}
#define IFPUNCT "(,{})(\':;=!><&|+*/-"
const char *If_Token(const char *func, const char **end)
{
	const char *s, *s2;
	cvar_t *var;
	int level;
	while(*func <= ' ' && *func)
		func++;

	s = COM_ParseToken(func, IFPUNCT);

	if (*com_token == '(')
	{
		s2 = s;
		level=1;
		while (*s2)
		{
			if (*s2 == ')')
			{
				level--;
				if (!level)
					break;
			}
			else if (*s2 == '(')
				level++;
			s2++;
		}
		func = If_Token(s, end);
		*end = s2+1;
		s = *end;
		s2 = func;
//		return func;
	}
	else if (*com_token == '!')
	{
		func = If_Token(s, end);
		for (s = func; *s; s++);
		if (func && *func)
			return "";
		else
			return "true";
	}
	else if (!strcmp(com_token, "int"))
	{
		func = If_Token(s, end);
		return retfloat(atof(func));
	}
	else if (!strcmp(com_token, "strlen"))
	{
		func = If_Token(s, end);
		return retfloat(strlen(func));
	}
	else if (!strcmp(com_token, "defined"))	//functions
	{
		s = COM_ParseToken(s, IFPUNCT);
		var = Cvar_FindVar(com_token);
		*end = s;
		return retstring((var != NULL)?"true":"");
	}
	else if (!strcmp(com_token, "random"))
	{
		s2 = retfloat((rand()&0x7fff) / (float)0x7fff);
	}
	else if (!strcmp(com_token, "vid"))	//mostly for use with the menu system.
	{
		s = COM_ParseToken(s, IFPUNCT);
#ifndef SERVERONLY
		if (qrenderer == QR_NONE)
			s2 = "";
		else if (!strcmp(com_token, "width"))
			s2 = retfloat(vid.width);
		else if (!strcmp(com_token, "height"))
			s2 = retfloat(vid.height);
		else
#endif
			s2 = "";
	}
	else
	{
		if (*com_token == '$')
			var = Cvar_FindVar(com_token+1);
		else
			var = Cvar_FindVar(com_token);	//for consistancy.
		if (var)
		{
			if ((var->restriction?var->restriction:rcon_level.ival) > Cmd_ExecLevel)
				s2 = "RESTRICTED";
			else
				s2 = var->string;
		}
		else
			s2 = retstring(com_token);
	}

	*end = s;

	s = COM_ParseToken(s, IFPUNCT);
	if (!strcmp(com_token, "="))	//comparisions
	{
		func=COM_ParseToken(s, IFPUNCT);
		if (*com_token == '=')	//lol. "=" == "=="
			return retfloat(!strcmp(s2, If_Token(func, end)));
		else
			return retfloat(!strcmp(s2, If_Token(s, end)));
	}
	if (!strncmp(com_token, "equal", 5))
		return retfloat(!strcmp(s2, If_Token(s, end)));
	if (!strcmp(com_token, "!"))
	{
		func=COM_ParseToken(s, IFPUNCT);
		if (*com_token == '=')
		{
			s = If_Token(func, end);
			if (!is_numeric(s) || !is_numeric(s2))
			{
				if (strcmp(s2, s))
					return "true";
				else
					return "";
			}
			return retfloat(atof(s2)!=atof(s));
		}
		else if (!strcmp(com_token, "isin"))
			return retfloat(NULL==strstr(If_Token(s, end), s2));
	}
	if (!strcmp(com_token, ">"))
	{
		func=COM_ParseToken(s, IFPUNCT);
		if (*com_token == '=')
			return retfloat(atof(s2)>=atof(If_Token(func, end)));
		else if (*com_token == '<')//vb?
		{
			s = If_Token(func, end);
			if (is_numeric(s) && is_numeric(s2))
			{
				if (strcmp(s2, s))
					return "true";
				else
					return "";
			}
			return retfloat(atof(s2)!=atof(s));
		}
		else
			return retfloat(atof(s2)>atof(If_Token(s, end)));
	}
	if (!strcmp(com_token, "<"))
	{
		func=COM_ParseToken(s, IFPUNCT);
		if (*com_token == '=')
			return retfloat(atof(s2)<=atof(If_Token(func, end)));
		else if (*com_token == '>')//vb?
			return retfloat(atof(s2)!=atof(If_Token(func, end)));
		else
			return retfloat(atof(s2)<atof(If_Token(s, end)));
	}
	if (!strcmp(com_token, "isin"))//fuhq
		return retfloat(NULL!=strstr(If_Token(s, end), s2));

	if (!strcmp(com_token, "-"))	//subtract
		return retfloat(atof(s2)-atof(If_Token(s, end)));
	if (!strcmp(com_token, "+"))	//add
		return retfloat(atof(s2)+atof(If_Token(s, end)));
	if (!strcmp(com_token, "*"))
		return retfloat(atof(s2)*atof(If_Token(s, end)));
	if (!strcmp(com_token, "/"))
		return retfloat(atof(s2)/atof(If_Token(s, end)));
	if (!strcmp(com_token, "%"))
	{
		level = (int)atof(If_Token(s, end));
		if (level == 0)
			return retfloat(0);
		else
			return retfloat((int)atof(s2)%level);
	}
	if (!strcmp(com_token, "&"))	//and
	{
		func=COM_ParseToken(s, IFPUNCT);
		if (*com_token == '&')
			return retfloat(*s2&&*If_Token(s, end));
		else
			return retfloat(atoi(s2)&atoi(If_Token(s, end)));
	}
	if (!strcmp(com_token, "div"))	//qw262 compatability
		return retfloat(atof(s2)/atof(If_Token(s, end)));
	if (!strcmp(com_token, "or"))	//qw262 compatability
		return retfloat(atoi(s2)|atoi(If_Token(s, end)));
	if (!strcmp(com_token, "xor"))	//qw262 compatability
		return retfloat(atoi(s2)^atoi(If_Token(s, end)));
	if (!strcmp(com_token, "and"))	//qw262 compatability
		return retfloat(atoi(s2)&atoi(If_Token(s, end)));
	if (!strcmp(com_token, "|"))	//or
	{
		func=COM_ParseToken(s, IFPUNCT);
		if (*com_token == '|')
		{
			func = If_Token(func, end);
			return retfloat(atoi(s2)||atoi(func));
		}
		else
			return retfloat(atoi(s2)|atoi(If_Token(s, end)));
	}

	return s2;
}

void Cbuf_ExecBlock(int level)
{
	char *remainingcbuf;
	char *exectext = NULL;
	char *line, *end;
	line = Cbuf_GetNext(level);

	while(*line <= ' ' && *line)	//skip leading whitespace.
		line++;

	for (end = line + strlen(line)-1; end >= line && *end <= ' '; end--)	//skip trailing
		*end = '\0';

	if (!strcmp(line, "{"))	//multiline block
	{
		int indent = 1;

		for(;;)
		{
			line = Cbuf_GetNext(level);

			while(*line <= ' ' && *line)	//skip leading whitespace.
				line++;

			for (end = line + strlen(line)-1; end >= line && *end <= ' '; end--)	//skip trailing
				*end = '\0';

			if (!strcmp(line, "{"))
				indent++;
			else if (!strcmp(line, "}"))
			{
				indent--;
				if (!indent)
					break;
			}
			else if (!*line)
			{
				Con_Printf("Unterminated block\n");
				break;
			}

			if (exectext)
			{
				char *newv;
				newv = (char*)Z_Malloc(strlen(exectext) + strlen(line) + 2);
				sprintf(newv, "%s;%s", exectext, line);
				Z_Free(exectext);
				exectext = newv;
			}
			else
				exectext = CopyString(line);
//			Con_Printf("Exec \"%s\"\n", line);
		}
	}
	else
	{
		exectext = CopyString(line);
//		Con_Printf("Exec \"%s\"\n", line);
	}
	remainingcbuf = Cbuf_StripText(level);	//this craziness is to prevent an if } from breaking the entire con text
	Cbuf_AddText(exectext, level);
	Z_Free(exectext);
	Cbuf_ExecuteLevel(level);
	Cbuf_AddText(remainingcbuf, level);
	Z_Free(remainingcbuf);
}

void Cbuf_SkipBlock(int level)
{
	char *line, *end;
	line = Cbuf_GetNext(level);

	while(*line <= ' ' && *line)	//skip leading whitespace.
		line++;

	for (end = line + strlen(line)-1; end >= line && *end <= ' '; end--)	//skip trailing
		*end = '\0';

	if (!strcmp(line, "{"))	//multiline block
	{
		int indent = 1;

		for(;;)
		{
			line = Cbuf_GetNext(level);

			while(*line <= ' ' && *line)	//skip leading whitespace.
				line++;

			for (end = line + strlen(line)-1; end >= line && *end <= ' '; end--)	//skip trailing
				*end = '\0';

			if (!strcmp(line, "{"))
				indent++;
			else if (!strcmp(line, "}"))
			{
				indent--;
				if (!indent)
					break;
			}
			else if (!*line)
			{
				Con_Printf("Unterminated block\n");
				break;
			}
//			Con_Printf("Skip \"%s\"\n", line);
		}
	}
//	else
//		Con_Printf("Skip \"%s\"\n", line);
}

void Cmd_if_f(void)
{
	char *text = Cmd_Args();
	const char *ret;
	char *end;
	char *ws;
	int level;
	qboolean trueblock=false;
	tempstack_t *ts;

	if (Cmd_Argc()==1)
	{
		Con_TPrintf(TL_IFSYNTAX);
		return;
	}

	ts = If_Token_GetMark();
	level = Cmd_ExecLevel;

elseif:
//	Con_Printf("if %s\n", text);
	for(ret = If_Token(text, (const char **)&end); *ret; ret++) {if (*ret != '0' && *ret != '.')break;}
	if (!end)
	{
		Con_TPrintf(TL_IFSYNTAXERROR);
		If_Token_Clear(ts);
		return;
	}

skipws:
	while(*end <= ' ' && *end)	//skip leading whitespace.
		end++;

	for (ws = end + strlen(end)-1; ws >= end && *ws <= ' '; ws--)	//skip trailing
		*ws = '\0';

	if (!strncmp(end, "then", 4))	//sigh... trying to make fuhquake's ifs work.
	{
		end+=4;
		goto skipws;
	}

	if (!*end)
	{
		if (ret && *ret)	//equation was true.
		{
			trueblock = true;
			Cbuf_ExecBlock(level);
		}
		else	//equation was false.
		{
skipblock:
			Cbuf_SkipBlock(level);
		}
		end = Cbuf_GetNext(level);
		while(*end <= ' ' && *end)
			end++;
		if (!strncmp(end, "else", 4))
		{
			end+=4;
			while(*end <= ' ' && *end)	//skip leading whitespace.
				end++;

			if (!strncmp(end, "if", 2))
			{
				text = end + 2;
				if (trueblock)
					goto skipblock;	//we've had our true, all others are assumed to be false.
				else
					goto elseif;	//and have annother go.
			}
			else
			{	//we got an else. This is the last block. Don't go through the normal way, cos that would let us follow up with a second else.
				if (trueblock)
					Cbuf_SkipBlock(level);
				else
					Cbuf_ExecBlock(level);

				If_Token_Clear(ts);
				return;
			}
		}
		//whoops. Too far.
		Cbuf_InsertText(end, level, true);
		If_Token_Clear(ts);
		return;
	}

	text = strstr(end, "else");
	if (ret && *ret)
	{
		if (text)	//don't bother execing the else bit...
			*text = '\0';
		Cbuf_InsertText(end, level, true);
	}
	else
	{
		if (text)
			Cbuf_InsertText(text+4, level, true);	//ironically, this will do elseif...
	}

	If_Token_Clear(ts);
}

void Cmd_Vstr_f( void )
{
	char	*v;

	if (Cmd_Argc () != 2)
	{
		Con_Printf ("vstr <variablename> : execute a variable command\n");
		return;
	}

	v = Cvar_VariableString(Cmd_Argv(1));
	Cbuf_InsertText(v, Cmd_ExecLevel, true);
}

void Cmd_toggle_f(void)
{
	cvar_t *v;
	if (Cmd_Argc()<2)
	{
		Con_Printf("missing cvar name\n");
		return;
	}
	v = Cvar_Get(Cmd_Argv(1), "0", 0, "Custom variables");
	if (!v)
		return;

	if (v->value)
		Cvar_Set(v, "0");
	else
		Cvar_Set(v, "1");
}

void Cmd_set_f(void)
{
	void *mark;
	cvar_t *var;
	const char *end;
	const char *text;
	int forceflags = 0;
	qboolean docalc;

	if (Cmd_Argc()<3)
	{
		Con_TPrintf(TL_SETSYNTAX);
		return;
	}

	if (!strcmp(Cmd_Argv(0), "set_calc") || !strcmp(Cmd_Argv(0), "seta_calc"))
		docalc = true;
	else
		docalc = false;

	var = Cvar_Get (Cmd_Argv(1), "", 0, "Custom variables");

	if (Cmd_FromGamecode())	//AAHHHH!!! Q2 set command is different
	{
		text = Cmd_Argv(3);
		if (!strcmp(text, "u"))
			forceflags = CVAR_USERINFO;
		else if (!strcmp(text, "s"))
			forceflags = CVAR_SERVERINFO;
		else if (*text) //err
			return;
		text = Cmd_Argv(2);
	}
	else
	{
		Cmd_ShiftArgs(1, false);
		text = Cmd_Args();
		if (*text == '\"')	//if it's already quoted, dequote it, and ignore trailing stuff, for q2/q3 compatability
			text = Cmd_Argv(1);
		else
		{
			end = strstr(text, "//");
			if (end)
			{
				*end--;
				while (end >= text)
				{
					if (*end == ' ')
						end--;
					else
						break;
				}
				end++;
				*(char*)end = 0;

			}
		}
		forceflags = 0;
	}

	mark = If_Token_GetMark();

	if (var)
	{
		if (var->flags & CVAR_NOTFROMSERVER && Cmd_FromGamecode())
		{
			Con_Printf ("Server tried setting %s cvar\n", var->name);
			return;
		}

		if (Cmd_FromGamecode())
		{
			if (forceflags)
			{
				var->flags &=~(CVAR_USERINFO|CVAR_SERVERINFO);
				var->flags |= forceflags;
			}
			Cvar_LockFromServer(var, text);
		}
		else
		{
			if (docalc)
				text = If_Token(text, &end);
			Cvar_Set(var, text);
			var->flags |= CVAR_USERCREATED;

			if (!stricmp(Cmd_Argv(0), "seta"))
				var->flags |= CVAR_ARCHIVE;
		}
	}
	else
	{
		if (docalc)
			text = If_Token(text, &end);
		if (Cmd_FromGamecode())
		{
			var = Cvar_Get(Cmd_Argv(1), "", 0, "Game variables");
			if (var)
				Cvar_LockFromServer(var, text);
		}
		else
			var = Cvar_Get(Cmd_Argv(1), text, 0, "User variables");
	}

	if (var && !Cmd_FromGamecode())
		if (!stricmp(Cmd_Argv(0), "seta"))
			var->flags |= CVAR_ARCHIVE|CVAR_USERCREATED;

	If_Token_Clear(mark);
}

void Cvar_Inc_f (void)
{
	int c;
	cvar_t *var;
	float delta;

	c = Cmd_Argc();
	if (c != 2 && c != 3)
	{
		Con_Printf ("inc <cvar> [value]\n");
		return;
	}

	var = Cvar_FindVar (Cmd_Argv(1));
	if (!var)
	{
		Con_Printf ("Unknown variable \"%s\"\n", Cmd_Argv(1));
		return;
	}
	if (var->flags & CVAR_NOTFROMSERVER && Cmd_FromGamecode())
	{
		Con_Printf ("Server tried setting %s cvar\n", var->name);
		return;
	}


	delta = (c == 3) ? atof (Cmd_Argv(2)) : 1;

	Cvar_SetValue (var, var->value + delta);
}

void Cmd_WriteConfig_f(void)
{
	vfsfile_t *f;
	char *filename;
	char fname[MAX_OSPATH];

	filename = Cmd_Argv(1);
	if (!*filename)
	{
		snprintf(fname, sizeof(fname), "fte.cfg");
		FS_CreatePath(fname, FS_GAMEONLY);
		f = FS_OpenVFS(fname, "wb", FS_GAMEONLY);
	}
	else
	{
		if (strstr(filename, ".."))
		{
			Con_Printf ("Couldn't write config %s\n",filename);
			return;
		}
		snprintf(fname, sizeof(fname), "configs/%s", filename);
		COM_DefaultExtension(fname, ".cfg", sizeof(fname));

		FS_CreatePath(fname, FS_CONFIGONLY);
		f = FS_OpenVFS(fname, "wb", FS_CONFIGONLY);
	}
	if (!f)
	{
		Con_Printf ("Couldn't write config %s\n",fname);
		return;
	}

	VFS_WRITE(f, "// FTE config file\n\n", 20);
#ifndef SERVERONLY
	Key_WriteBindings (f);
	CL_SaveInfo(f);
#else
	VFS_WRITE(f, "// Dedicated Server config\n\n", 28);
#endif
#ifdef CLIENTONLY
	VFS_WRITE(f, "// no local/server infos\n\n", 26);
#else
	SV_SaveInfos(f);
#endif
	Alias_WriteAliases (f);
	Cvar_WriteVariables (f, true);
	VFS_CLOSE(f);

	FS_FlushFSHash();
}

void Cmd_Reset_f(void)
{
}

#ifndef SERVERONLY
// dumps current console contents to a text file
void Cmd_Condump_f(void)
{
	vfsfile_t *f;
	char *filename;
	unsigned char c;

	if (!con_current)
	{
		Con_Printf ("No console to dump.\n");
		return;
	}

	if (Cmd_IsInsecure()) // don't allow insecure level execute this
		return;

	filename = Cmd_Argv(1);
	if (!*filename)
		filename = "condump";

	filename = va("%s", filename);
	COM_DefaultExtension(filename, ".txt", MAX_QPATH);
	
	f = FS_OpenVFS (filename, "wb", FS_GAME);
	if (!f)
	{
		Con_Printf ("Couldn't write console dump %s\n",filename);
		return;
	}

	// print out current contents of console
	// stripping out starting blank lines and blank spaces
	{
		console_t *curcon = &con_main;
		conline_t *l;
		int i;
		conchar_t *t;
		for (l = curcon->oldest; l; l = l->newer)
		{
			t = (conchar_t*)(l+1);
			//FIXME: utf8?
			for (i = 0; i < l->length; i++)
			{
				c = (qbyte)t[i]&0xff;
				VFS_WRITE(f, &c, 1);
			}
			VFS_WRITE(f, "\n", 1);
		}
	}

	VFS_CLOSE(f);

	Con_Printf ("Dumped console to %s\n",filename);
}
#endif

void Cmd_Shutdown(void)
{
	//make sure we get no other execution
	int level;
	for (level = 0; level < sizeof(cmd_text)/sizeof(cmd_text[0]); level++)
		SZ_Clear (&cmd_text[level].buf);

	cmd_functions = NULL;
	cmd_alias = NULL;
}

/*
============
Cmd_Init
============
*/
void Cmd_Init (void)
{
//
// register our commands
//
	Cmd_AddCommand ("cfg_save",Cmd_WriteConfig_f);

	Cmd_AddCommand ("cfg_load",Cmd_Exec_f);
	Cmd_AddCommand ("cfg_reset",Cmd_Reset_f);

	Cmd_AddCommand ("exec",Cmd_Exec_f);
	Cmd_AddCommand ("echo",Cmd_Echo_f);
	Cmd_AddCommand ("alias",Cmd_Alias_f);
	Cmd_AddCommand ("newalias",Cmd_Alias_f);
	Cmd_AddCommand ("wait", Cmd_Wait_f);
#ifndef SERVERONLY
	Cmd_AddCommand ("cmd", Cmd_ForwardToServer_f);
	Cmd_AddCommand ("condump", Cmd_Condump_f);
#endif
	Cmd_AddCommand ("restrict", Cmd_RestrictCommand_f);
	Cmd_AddCommand ("aliaslevel", Cmd_AliasLevel_f);

	Cmd_AddCommand ("showalias", Cmd_ShowAlias_f);

//	Cmd_AddCommand ("msg_trigger", Cmd_Msg_Trigger_f);
//	Cmd_AddCommand ("filter", Cmd_Msg_Filter_f);

	Cmd_AddCommand ("toggle", Cmd_toggle_f);
	Cmd_AddCommand ("set", Cmd_set_f);
	Cmd_AddCommand ("set_calc", Cmd_set_f);
	Cmd_AddCommand ("seta", Cmd_set_f);
	Cmd_AddCommand ("seta_calc", Cmd_set_f);
	Cmd_AddCommand ("vstr", Cmd_Vstr_f);
	Cmd_AddCommand ("inc", Cvar_Inc_f);
	//FIXME: Add seta some time.
	Cmd_AddCommand ("if", Cmd_if_f);

	Cmd_AddCommand ("cmdlist", Cmd_List_f);
	Cmd_AddCommand ("aliaslist", Cmd_AliasList_f);
	Cmd_AddCommand ("cvarlist", Cvar_List_f);
	Cmd_AddCommand ("cvarreset", Cvar_Reset_f);
	Cmd_AddCommand ("fs_flush", COM_RefreshFSCache_f);

	Cvar_Register(&com_fs_cache, "Filesystem");
	Cvar_Register(&tp_disputablemacros, "Teamplay");

#ifndef SERVERONLY
	rcon_level.ival = atof(rcon_level.string);	//client is restricted to not be allowed to change restrictions.
#else
	Cvar_Register(&rcon_level, "Access controls");		//server gains versatility.
#endif
	rcon_level.restriction = RESTRICT_MAX;	//default. Don't let anyone change this too easily.
	cmd_maxbuffersize.restriction = RESTRICT_MAX;	//filling this causes a loop for quite some time.

	Cvar_Register(&cl_aliasoverlap, "Console");
	//FIXME: go through quake.rc and parameters looking for sets and setas and setting them now.
}

