#include "qcc.h"
#include "gui.h"

//common gui things

pbool fl_hexen2;
pbool fl_autohighlight;
pbool fl_compileonstart;
pbool fl_showall;
pbool fl_log;

char parameters[16384];
char progssrcname[256];
char progssrcdir[256];

void GoToDefinition(char *name)
{
	QCC_def_t *def;
	QCC_dfunction_t *fnc;

	char *strip;	//trim whitespace (for convieniance).
	while (*name <= ' ' && *name)
		name++;
	for (strip = name + strlen(name)-1; *strip; strip++)
	{
		if (*strip <= ' ')
			*strip = '\0';
		else	//got some part of a word
			break;
	}

	if (!globalstable.numbuckets)
	{
		GUI_DialogPrint("Not found", "You need to compile first.");
		return;
	}


	def = QCC_PR_GetDef(NULL, name, NULL, false, 0);

	if (def)
	{
		if (def->type->type == ev_function && def->constant)
		{
			fnc = &functions[((int *)qcc_pr_globals)[def->ofs]];
			if (fnc->first_statement>=0 && fnc->s_file)
			{
				EditFile(fnc->s_file+strings, statement_linenums[fnc->first_statement]);
				return;
			}
		}
		EditFile(def->s_file+strings, def->s_line-1);
	}
	else
		GUI_DialogPrint("Not found", "Global instance of var was not found.");
}




//this function takes the windows specified commandline and strips out all the options menu items.
void GUI_ParseCommandLine(char *args)
{
	int paramlen=0;
	int l, p;
	char *next;
	while(*args)
	{
		while (*args <= ' ' && *args)
			args++;

		for (next = args; *next>' '; next++)
			;

		strncpy(parameters+paramlen, args, next-args);
		parameters[paramlen+next-args] = '\0';
		l = strlen(parameters+paramlen)+1;

		if (!strnicmp(parameters+paramlen, "-O", 2) || !strnicmp(parameters+paramlen, "/O", 2))
		{	//strip out all -O
			if (parameters[paramlen+2])
			{
				if (parameters[paramlen+2] >= '0' && parameters[paramlen+2] <= '3')
				{
					p = parameters[paramlen+2]-'0';
					for (l = 0; optimisations[l].enabled; l++)
					{
						if (optimisations[l].optimisationlevel<=p)
							optimisations[l].flags |= FLAG_SETINGUI;
						else
							optimisations[l].flags &= ~FLAG_SETINGUI;
					}
				}
				else if (!strncmp(parameters+paramlen+2, "no-", 3))
				{
					if (parameters[paramlen+5])
					{
						for (p = 0; optimisations[p].enabled; p++)
							if ((*optimisations[p].abbrev && !strcmp(parameters+paramlen+5, optimisations[p].abbrev)) || !strcmp(parameters+paramlen+5, optimisations[p].fullname))
							{
								optimisations[p].flags &= ~FLAG_SETINGUI;
								break;
							}

						if (!optimisations[p].enabled)
						{
							parameters[paramlen+next-args] = ' ';
							paramlen += l;
						}
					}
				}
				else
				{
					for (p = 0; optimisations[p].enabled; p++)
						if ((*optimisations[p].abbrev && !strcmp(parameters+paramlen+2, optimisations[p].abbrev)) || !strcmp(parameters+paramlen+2, optimisations[p].fullname))
						{
							optimisations[p].flags |= FLAG_SETINGUI;
							break;
						}

					if (!optimisations[p].enabled)
					{
						parameters[paramlen+next-args] = ' ';
						paramlen += l;
					}
				}
			}
		}
/*
		else if (!strnicmp(parameters+paramlen, "-Fno-kce", 8) || !strnicmp(parameters+paramlen, "/Fno-kce", 8))	//keywords stuph
		{
			fl_nokeywords_coexist = true;
		}
		else if (!strnicmp(parameters+paramlen, "-Fkce", 5) || !strnicmp(parameters+paramlen, "/Fkce", 5))
		{
			fl_nokeywords_coexist = false;
		}
		else if (!strnicmp(parameters+paramlen, "-Facc", 5) || !strnicmp(parameters+paramlen, "/Facc", 5))
		{
			fl_acc = true;
		}
		else if (!strnicmp(parameters+paramlen, "-autoproto", 10) || !strnicmp(parameters+paramlen, "/autoproto", 10))
		{
			fl_autoprototype = true;
		}
*/
		else if (!strnicmp(parameters+paramlen, "-showall", 8) || !strnicmp(parameters+paramlen, "/showall", 8))
		{
			fl_showall = true;
		}
		else if (!strnicmp(parameters+paramlen, "-ah", 3) || !strnicmp(parameters+paramlen, "/ah", 3))
		{
			fl_autohighlight = true;
		}
		else if (!strnicmp(parameters+paramlen, "-ac", 3) || !strnicmp(parameters+paramlen, "/ac", 3))
		{
			fl_compileonstart = true;
		}
		else if (!strnicmp(parameters+paramlen, "-log", 4) || !strnicmp(parameters+paramlen, "/log", 4))
		{
			fl_log = true;
		}
		else if (!strnicmp(parameters+paramlen, "-T", 2) || !strnicmp(parameters+paramlen, "/T", 2))	//the target
		{
			if (!strnicmp(parameters+paramlen+2, "h2", 2))
			{
				fl_hexen2 = true;
			}
			else
			{
				fl_hexen2 = false;
				parameters[paramlen+next-args] = ' ';
				paramlen += l;
			}
		}
		else
		{
			parameters[paramlen+next-args] = ' ';
			paramlen += l;
		}

		args=next;
	}
	if (paramlen)
		parameters[paramlen-1] = '\0';
	else
		*parameters = '\0';
}

void GUI_SetDefaultOpts(void)
{
	int i;
	for (i = 0; compiler_flag[i].enabled; i++)	//enabled is a pointer
	{
		if (compiler_flag[i].flags & FLAG_ASDEFAULT)
			compiler_flag[i].flags |= FLAG_SETINGUI;
		else
			compiler_flag[i].flags &= ~FLAG_SETINGUI;
	}
	for (i = 0; optimisations[i].enabled; i++)	//enabled is a pointer
	{
		if (optimisations[i].flags & FLAG_ASDEFAULT)
			optimisations[i].flags |= FLAG_SETINGUI;
		else
			optimisations[i].flags &= ~FLAG_SETINGUI;
	}
}

void GUI_RevealOptions(void)
{
	int i;
	for (i = 0; compiler_flag[i].enabled; i++)	//enabled is a pointer
	{
		if (fl_showall && compiler_flag[i].flags & FLAG_HIDDENINGUI)
			compiler_flag[i].flags &= ~FLAG_HIDDENINGUI;
	}
	for (i = 0; optimisations[i].enabled; i++)	//enabled is a pointer
	{
		if (fl_showall && optimisations[i].flags & FLAG_HIDDENINGUI)
			optimisations[i].flags &= ~FLAG_HIDDENINGUI;

		if (optimisations[i].flags & FLAG_HIDDENINGUI)	//hidden optimisations are disabled as default
			optimisations[i].optimisationlevel = 4;
	}
}




int GUI_BuildParms(char *args, char **argv)
{
	static char param[2048];
	int paramlen = 0;
	int argc;
	char *next;
	int i;


	argc = 1;
	argv[0] = "fteqcc";

	while(*args)
	{
		while (*args <= ' '&& *args)
			args++;

		for (next = args; *next>' '; next++)
			;
		strncpy(param+paramlen, args, next-args);
		param[paramlen+next-args] = '\0';
		argv[argc++] = param+paramlen;
		paramlen += strlen(param+paramlen)+1;

		args=next;
	}

	if (fl_hexen2)
	{
		strcpy(param+paramlen, "-Th2");
		argv[argc++] = param+paramlen;
		paramlen += strlen(param+paramlen)+1;
	}

	for (i = 0; optimisations[i].enabled; i++)	//enabled is a pointer
	{
		if (optimisations[i].flags & FLAG_SETINGUI)
			sprintf(param+paramlen, "-O%s", optimisations[i].abbrev);
		else
			sprintf(param+paramlen, "-Ono-%s", optimisations[i].abbrev);
		argv[argc++] = param+paramlen;
		paramlen += strlen(param+paramlen)+1;
	}


/*	while(*args)
	{
		while (*args <= ' '&& *args)
			args++;

		for (next = args; *next>' '; next++)
			;
		strncpy(param+paramlen, args, next-args);
		param[paramlen+next-args] = '\0';
		argv[argc++] = param+paramlen;
		paramlen += strlen(param+paramlen)+1;
		args=next;
	}*/

	if (*progssrcname)
	{
		argv[argc++] = "-srcfile";
		argv[argc++] = progssrcname;
	}
	if (*progssrcdir)
	{
		argv[argc++] = "-src";
		argv[argc++] = progssrcdir;
	}

	return argc;
}
