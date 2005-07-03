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
// cvar.c -- dynamic variable tracking

#include "quakedef.h"

cvar_group_t *cvar_groups;

//cvar_t	*cvar_vars;
char	*cvar_null_string = "";

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar (const char *var_name)
{
	cvar_group_t	*grp;
	cvar_t	*var;

	for (grp=cvar_groups ; grp ; grp=grp->next)
		for (var=grp->cvars ; var ; var=var->next)
			if (!Q_strcasecmp (var_name, var->name))
				return var;

	for (grp=cvar_groups ; grp ; grp=grp->next)
		for (var=grp->cvars ; var ; var=var->next)
			if (var->name2 && !Q_strcasecmp (var_name, var->name2))
				return var;

	return NULL;
}

cvar_group_t *Cvar_FindGroup (const char *group_name)
{
	cvar_group_t	*grp;

	for (grp=cvar_groups ; grp ; grp=grp->next)
		if (!Q_strcasecmp (group_name, grp->name))
			return grp;

	return NULL;
}
cvar_group_t *Cvar_GetGroup(const char *gname)
{
	cvar_group_t *g;
	if (!gname)
		gname = "Miscilaneous vars";
	g = Cvar_FindGroup(gname);
	if (g)
		return g;

	g = (cvar_group_t*)Z_Malloc(sizeof(cvar_group_t));
	g->name = (char*)gname;
	g->next = NULL;
	g->next = cvar_groups;
	cvar_groups = g;
	return g;
}

//lists commands, also prints restriction level
void Cvar_List_f (void)
{
	cvar_group_t	*grp;
	cvar_t	*cmd;
	int num=0;
	for (grp=cvar_groups ; grp ; grp=grp->next)
	for (cmd=grp->cvars ; cmd ; cmd=cmd->next)
	{
		if ((cmd->restriction?cmd->restriction:rcon_level.value) > Cmd_ExecLevel)
			continue;
		if (!num)
			Con_TPrintf(TL_CVARLISTHEADER);
		Con_Printf("(%2i) %s\n", (int)(cmd->restriction?cmd->restriction:rcon_level.value), cmd->name);
		num++;
	}
	if (num)
		Con_Printf("\n");
}

/*
============
Cvar_VariableValue
============
*/
float	Cvar_VariableValue (const char *var_name)
{
	cvar_t	*var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return Q_atof (var->string);
}


/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString (const char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return cvar_null_string;
	return var->string;
}


/*
============
Cvar_CompleteVariable
============
*/
/* moved to cmd_compleatevariable
char *Cvar_CompleteVariable (char *partial)
{
	cvar_group_t	*grp;
	cvar_t		*cvar;
	int			len;

	len = Q_strlen(partial);

	if (!len)
		return NULL;

	// check exact match
	for (grp=cvar_groups ; grp ; grp=grp->next)
	for (cvar=grp->cvars ; cvar ; cvar=cvar->next)
		if (!strcmp (partial,cvar->name))
			return cvar->name;

	// check partial match
	for (grp=cvar_groups ; grp ; grp=grp->next)
	for (cvar=grp->cvars ; cvar ; cvar=cvar->next)
		if (!Q_strncmp (partial,cvar->name, len))
			return cvar->name;

	return NULL;
}
*/


/*
============
Cvar_Set
============
*/
cvar_t *Cvar_SetCore (cvar_t *var, const char *value, qboolean force)
{
	char *latch=NULL;

	if (!var)
		return NULL;

	if ((var->flags & CVAR_NOSET) && !force)
	{
		Con_Printf ("variable %s is readonly\n", var->name);
		return NULL;
	}

	if (var->flags & CVAR_SERVEROVERRIDE && !force)
		latch = "variable %s is under server control - latched\n";
	else if (var->flags & CVAR_LATCH)
		latch = "variable %s is latched\n";
	else if (var->flags & CVAR_RENDERERLATCH && qrenderer)
		latch = "variable %s will be changed after a renderer restart\n";
#ifndef SERVERONLY
	else if (var->flags & CVAR_CHEAT && !cls.allow_cheats && cls.state)
		latch = "variable %s is a cheat variable - latched\n";
	else if (var->flags & CVAR_SEMICHEAT && !cls.allow_semicheats && cls.state)
		latch = "variable %s is a cheat variable - latched\n";
#endif

	if (latch && !force)
	{
		if (cl_warncmd.value)
		{
			if (var->latched_string)
			{	//already latched
				if (strcmp(var->latched_string, value))
					Con_Printf (latch, var->name);
			}
			else
			{	//new latch
				if (strcmp(var->string, value))
					Con_Printf (latch, var->name);
			}
		}

		if (var->latched_string && !strcmp(var->latched_string, value))	//no point, this would force the same
			return NULL;
		if (var->latched_string)
			Z_Free(var->latched_string);
		if (!strcmp(var->string, value))	//latch to the origional value? remove the latch.
		{
			var->latched_string = NULL;
			return NULL;
		}
		var->latched_string = (char*)Z_Malloc(strlen(value)+1);
		strcpy(var->latched_string, value);
		return NULL;
	}

#ifndef CLIENTONLY
	if (var->flags & CVAR_SERVERINFO)
	{
		Info_SetValueForKey (svs.info, var->name, value, MAX_SERVERINFO_STRING);
		SV_SendServerInfoChange(var->name, value);
//		SV_BroadcastCommand ("fullserverinfo \"%s\"\n", svs.info);
	}
#endif
#ifndef SERVERONLY
	if (var->flags & CVAR_USERINFO)
	{
		Info_SetValueForKey (cls.userinfo, var->name, value, MAX_INFO_STRING);
		if (cls.state >= ca_connected)
		{
#ifdef Q2CLIENT
			if (cls.protocol == CP_QUAKE2)	//q2 just resends the lot. Kinda bad...
			{
				cls.resendinfo = true;
			}
			else
#endif
			{
				CL_SendClientCommand(true, "setinfo \"%s\" \"%s\"\n", var->name, value);
			}
		}
	}
#endif

	latch = var->string;

	var->string = (char*)Z_Malloc (Q_strlen(value)+1);
	Q_strcpy (var->string, value);
	var->value = Q_atof (var->string);

	if (latch)
	{
		if (strcmp(latch, value))
			var->modified++;	//only modified if it changed.

		Z_Free (latch);	// free the old value string
	}

	if (var->latched_string)	//we may as well have this here.
	{
		Z_Free(var->latched_string);
		var->latched_string = NULL;
	}

	return var;
}

void Cvar_ForceCheatVars(qboolean semicheats, qboolean absolutecheats)
{	//this either unlatches if the cheat type is allowed, or enforces a default for full cheats and blank for semicheats.
	//this is clientside only.
	//if a value is enforced, it is latched to the old value.
	cvar_group_t	*grp;
	cvar_t	*var;

	char *latch;

	for (grp=cvar_groups ; grp ; grp=grp->next)
	for (var=grp->cvars ; var ; var=var->next)
	{
		if (!(var->flags & (CVAR_CHEAT|CVAR_SEMICHEAT)))
			continue;

		latch = var->latched_string;
		var->latched_string = NULL;
		if (!latch)
		{
			latch = var->string;
			var->string = NULL;
		}

		if (var->flags & CVAR_CHEAT)
		{
			if (!absolutecheats)
				Cvar_ForceSet(var, var->defaultstr);
			else
				Cvar_ForceSet(var, latch);
		}
		if (var->flags & CVAR_SEMICHEAT)
		{
			if (!semicheats)
				Cvar_ForceSet(var, "");
			else
				Cvar_ForceSet(var, latch);
		}

		if (latch)
		{
			if (!strcmp(var->string, latch))
				Z_Free(latch);
			else
				var->latched_string = latch;
		}
	}
}

void Cvar_ApplyLatches(int latchflag)
{
	cvar_group_t	*grp;
	cvar_t	*var;
	int mask = ~0;

	if (latchflag == CVAR_SERVEROVERRIDE)	//these ones are cleared
		mask = ~CVAR_SERVEROVERRIDE;

	for (grp=cvar_groups ; grp ; grp=grp->next)
	for (var=grp->cvars ; var ; var=var->next)
	{
		if (var->flags & latchflag)
		{
			if (var->latched_string)
			{
				Cvar_ForceSet(var, var->latched_string);
			}
			var->flags &= mask;
		}
	}
}

cvar_t *Cvar_Set (cvar_t *var, const char *value)
{
	return Cvar_SetCore(var, value, false);
}
cvar_t *Cvar_ForceSet (cvar_t *var, const char *value)
{
	return Cvar_SetCore(var, value, true);
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue (cvar_t *var, float value)
{
	char	val[32];

	if (value == (int)value)
		sprintf (val, "%i",(int)value);	//make it look nicer.
	else
		sprintf (val, "%f",value);
	Cvar_Set (var, val);
}

void Cvar_Free(cvar_t *tbf)
{
	cvar_t *var;
	cvar_group_t *grp;
	if (!(tbf->flags & CVAR_POINTER))
		return;	//only freeable if it was a pointer to begin with.

	for (grp=cvar_groups ; grp ; grp=grp->next)
	{
		if (grp->cvars == tbf)
		{
			grp->cvars = tbf->next;
			goto unlinked;
		}
		for (var=grp->cvars ; var->next ; var=var->next)
		{
			if (var->next == tbf)
			{
				var->next = tbf->next;
				goto unlinked;
			}
		}
	}
unlinked:
	Z_Free(tbf->string);
	Z_Free(tbf->defaultstr);
	if (tbf->latched_string)
		Z_Free(tbf->latched_string);
	Z_Free(tbf);
}

/*
============
Cvar_RegisterVariable

Adds a freestanding variable to the variable list.
============
*/
void Cvar_Register (cvar_t *variable, const char *groupname)
{
	cvar_t *old;
	cvar_group_t *group;
	char	value[512];

// copy the value off, because future sets will Z_Free it
	strcpy (value, variable->string);

// check to see if it has allready been defined
	old = Cvar_FindVar (variable->name);
	if (old)
	{
		if (old->flags & CVAR_POINTER)
		{
			group = Cvar_GetGroup(groupname);

			variable->modified = old->modified;
			variable->flags |= old->flags & CVAR_ARCHIVE;

// link the variable in
			variable->next = group->cvars;
			variable->restriction = old->restriction;	//exe registered vars
			group->cvars = variable;

// make sure it can be zfreed
			variable->string = (char*)Z_Malloc (1);

//cheat prevention - engine set default is the one that stays.
			variable->defaultstr = (char*)Z_Malloc (strlen(value)+1);	//give it it's default (for server controlled vars and things)
			strcpy (variable->defaultstr, value);

// set it through the function to be consistant
			if (old->latched_string)
				Cvar_SetCore (variable, old->latched_string, true);
			else
				Cvar_SetCore (variable, old->string, true);

			Cvar_Free(old);
			return;
		}

		Con_Printf ("Can't register variable %s, allready defined\n", variable->name);
		return;
	}

// check for overlap with a command
	if (Cmd_Exists (variable->name))
	{
		Con_Printf ("Cvar_RegisterVariable: %s is a command\n", variable->name);
		return;
	}

	group = Cvar_GetGroup(groupname);

// link the variable in
	variable->next = group->cvars;
	variable->restriction = 0;	//exe registered vars
	group->cvars = variable;

	variable->string = (char*)Z_Malloc (1);

	variable->defaultstr = (char*)Z_Malloc (strlen(value)+1);	//give it it's default (for server controlled vars and things)
	strcpy (variable->defaultstr, value);

// set it through the function to be consistant
	Cvar_SetCore (variable, value, true);
}
/*
void Cvar_RegisterVariable (cvar_t *variable)
{
	Cvar_Register(variable, NULL);
}
*/
cvar_t *Cvar_Get(const char *name, const char *defaultvalue, int flags, const char *group)
{
	cvar_t *var;
	var = Cvar_FindVar(name);

	if (var)
	{
		//allow this to change all < cvar_latch values.
		//this allows q2 dlls to apply different flags to a cvar without destroying our important ones (like cheat).
		var->flags = (var->flags & ~(CVAR_NOSET)) | (flags & (CVAR_NOSET|CVAR_SERVERINFO|CVAR_USERINFO|CVAR_ARCHIVE));
		return var;
	}

	var = (cvar_t*)Z_Malloc(sizeof(cvar_t)+strlen(name)+1);
	var->name = (char *)(var+1);
	strcpy(var->name, name);
	var->string = (char*)defaultvalue;
	var->flags = flags|CVAR_POINTER|CVAR_USERCREATED;

	Cvar_Register(var, group);

	return var;
}

//prevent the client from altering the cvar until they change map or the server resets the var to the default.
void Cvar_LockFromServer(cvar_t *var, const char *str)
{
	char *oldlatch;

	Con_DPrintf("Server taking control of cvar %s (%s)\n", var->name, str);

	var->flags |= CVAR_SERVEROVERRIDE;

	oldlatch = var->latched_string;
	if (oldlatch)	//maintaining control
		var->latched_string = NULL;
	else	//taking control
	{
		oldlatch = (char*)Z_Malloc(strlen(var->string)+1);
		strcpy(oldlatch, var->string);
	}

	Cvar_SetCore (var, str, true);	//will use all, quote included

	var->latched_string = oldlatch;	//keep track of the origional value.
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean	Cvar_Command (int level)
{
	cvar_t			*v;
	char *str;

// check variables
	v = Cvar_FindVar (Cmd_Argv(0));
	if (!v)
		return false;

	if ((v->restriction?v->restriction:rcon_level.value) > level)
	{
		Con_Printf ("You do not have the priveledges for %s\n", v->name);
		return true;
	}

	if (v->flags & CVAR_NOTFROMSERVER && Cmd_FromGamecode())
	{
		Con_Printf ("Server tried setting %s cvar\n", v->name);
		return true;
	}
// perform a variable print or set
	if (Cmd_Argc() == 1)
	{
		Con_Printf ("\"%s\" is \"%s\"\n", v->name, v->string);
		if (v->latched_string)
			Con_Printf ("Latched as \"%s\"\n", v->latched_string);
		Con_Printf("Default: \"%s\"\n", v->defaultstr);
		return true;
	}

	if (Cmd_Argc() == 2)
		str = Cmd_Argv(1);
	else
		str = Cmd_Args();

	if (v->flags & CVAR_NOSET)
	{
		Con_Printf ("Cvar %s may not be set via the console\n", v->name);
		return true;
	}
#ifndef SERVERONLY
	if (Cmd_ExecLevel > RESTRICT_SERVER)
	{	//directed at a secondary player.
		CL_SendClientCommand(true, "%i setinfo %s \"%s\"", Cmd_ExecLevel - RESTRICT_SERVER-1, v->name, str);
		return true;
	}

	if (v->flags & CVAR_SERVEROVERRIDE)
	{
		if (Cmd_FromGamecode())
		{
			if (!strcmp(v->defaultstr, str))	//returning to default
			{
				v->flags &= ~CVAR_SERVEROVERRIDE;
				if (v->latched_string)
					str = v->latched_string;	//set to the latched
			}
			else
			{
				Cvar_LockFromServer(v, str);
				return true;
			}
		}
		//let cvar_set latch if needed.
	}
	else if (Cmd_FromGamecode())
	{//it's not latched yet
		if (strcmp(v->defaultstr, str))
		{	//lock the cvar, unless it's going to it's default value.
			Cvar_LockFromServer(v, str);
			return true;
		}
	}
#endif
	Cvar_Set (v, str);	//will use all, quote included
	return true;
}


/*
============
Cvar_WriteVariables

Writes lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/
void Cvar_WriteVariables (FILE *f, qboolean all)
{
	qboolean writtengroupheader;
	cvar_group_t *grp;
	cvar_t	*var;
	char *val;

	for (grp=cvar_groups ; grp ; grp=grp->next)
	{
		writtengroupheader = false;
		for (var = grp->cvars ; var ; var = var->next)
			if (var->flags & CVAR_ARCHIVE || all)
			{
				if (!writtengroupheader)
				{
					writtengroupheader = true;
					fprintf(f, "\n// %s\n", grp->name);
				}

				val = var->string;	//latched vars should act differently.
				if (var->latched_string)
					val = var->latched_string;

				if (var->flags & CVAR_USERCREATED)
				{
					if (var->flags & CVAR_ARCHIVE)
						fprintf (f, "seta %s \"%s\"\n", var->name, val);
					else
						fprintf (f, "set %s \"%s\"\n", var->name, val);
				}
				else
					fprintf (f, "%s \"%s\"\n", var->name, val);
			}
	}
}

void Cvar_Shutdown(void)
{
	cvar_t	*var;
	cvar_group_t *grp;
	while(cvar_groups)
	{
		while(cvar_groups->cvars)
		{
			var = cvar_groups->cvars;
			cvar_groups->cvars = var->next;

			Z_Free(var->string);
			if (var->flags & CVAR_POINTER)
				Z_Free(var);
		}

		grp = cvar_groups;
		cvar_groups = grp->next;
		Z_Free(grp);
	}
}
