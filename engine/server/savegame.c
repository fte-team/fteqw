#include "quakedef.h"

#ifndef CLIENTONLY

//#ifdef _DEBUG
#define NEWSAVEFORMAT
//#endif

extern cvar_t skill;
extern cvar_t deathmatch;
extern cvar_t coop;
extern cvar_t teamplay;

//Writes a SAVEGAME_COMMENT_LENGTH character comment describing the current
void SV_SavegameComment (char *text)
{
	int		i;
	char	kills[20];

	char *mapname = sv.mapname;

	for (i=0 ; i<SAVEGAME_COMMENT_LENGTH ; i++)
		text[i] = ' ';
	if (!mapname)
		strcpy( text, "Unnamed_Level");
	else
	{
		i = strlen(mapname);
		if (i > SAVEGAME_COMMENT_LENGTH)
			i = SAVEGAME_COMMENT_LENGTH;
		memcpy (text, mapname, i);
	}
#ifdef Q2SERVER
	if (ge)	//q2
	{
		kills[0] = '\0';
	}
	else
#endif
#ifdef HLSERVER
	if (svs.gametype == GT_HALFLIFE)
	{
		sprintf (kills,"kills:moo");
	}
	else
#endif
		sprintf (kills,"kills:%3i/%3i", (int)pr_global_struct->killed_monsters, (int)pr_global_struct->total_monsters);
	memcpy (text+22, kills, strlen(kills));
// convert space to _ to make stdio happy
	for (i=0 ; i<SAVEGAME_COMMENT_LENGTH ; i++)
	{
		if (text[i] == ' ')
			text[i] = '_';
		else if (text[i] == '\n')
			text[i] = '\0';
	}
	text[SAVEGAME_COMMENT_LENGTH] = '\0';
}

#ifndef NEWSAVEFORMAT
void SV_Savegame_f (void)
{
	int len;
	char *s = NULL;
	client_t *cl;
	int clnum;

	int version = SAVEGAME_VERSION;

	char	name[256];
	FILE	*f;
	int		i;
	char	comment[SAVEGAME_COMMENT_LENGTH+1];

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf (STL_SAVESYNTAX);
		return;
	}

	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_TPrintf (STL_NORELATIVEPATHS);
		return;
	}

	if (sv.state != ss_active)
	{
		Con_Printf("Can't apply: Server isn't running or is still loading\n");
		return;
	}

	sprintf (name, "%s/saves/%s", com_gamedir, Cmd_Argv(1));
	COM_DefaultExtension (name, ".sav");

	Con_TPrintf (STL_SAVEGAMETO, name);
	f = fopen (name, "w");
	if (!f)
	{
		Con_TPrintf (STL_ERRORCOULDNTOPEN);
		return;
	}

	//if there are 1 of 1 players connected
	if (sv.allocated_client_slots == 1 && svs.clients->state < cs_spawned)
	{//try to go for nq/zq compatability as this is a single player game.
		s = PR_SaveEnts(svprogfuncs, NULL, &len, 2);	//get the entity state now, so that we know if we can get the full state in a q1 format.
		if (s)
		{
			if (progstype == PROG_QW)
				version = 6;
			else
				version = 5;
		}
	}


	fprintf (f, "%i\n", version);
	SV_SavegameComment (comment);
	fprintf (f, "%s\n", comment);

	if (version != SAVEGAME_VERSION)
	{
		for (i=0; i<NUM_SPAWN_PARMS ; i++)
				fprintf (f, "%f\n", svs.clients->spawn_parms[i]);	//client 1.
		fprintf (f, "%f\n", skill.value);
	}
	else
	{
		fprintf(f, "%i\n", sv.allocated_client_slots);
		for (cl = svs.clients, clnum=0; clnum < sv.allocated_client_slots; cl++,clnum++)
		{
			if (cl->state < cs_spawned && !cl->istobeloaded)	//don't save if they are still connecting
			{
				fprintf(f, "\"\"\n");
				continue;
			}

			fprintf(f, "\"%s\"\n", cl->name);
			for (i=0; i<NUM_SPAWN_PARMS ; i++)
				fprintf (f, "%f\n", cl->spawn_parms[i]);
		}
		fprintf (f, "%i\n", progstype);
		fprintf (f, "%f\n", skill.value);
		fprintf (f, "%f\n", deathmatch.value);
		fprintf (f, "%f\n", coop.value);
		fprintf (f, "%f\n", teamplay.value);
	}
	fprintf (f, "%s\n", sv.name);
	fprintf (f, "%f\n",sv.time);

// write the light styles

	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		if (sv.lightstyles[i])
			fprintf (f, "%s\n", sv.lightstyles[i]);
		else
			fprintf (f,"m\n");
	}

	if (!s)
		s = PR_SaveEnts(svprogfuncs, NULL, &len, 1);
	fprintf(f, "%s\n", s);
	svprogfuncs->parms->memfree(s);

	fclose (f);
	Con_TPrintf (STL_SAVEDONE);

	SV_BroadcastTPrintf(2, STL_GAMESAVED);
}


//FIXME: Multiplayer save probably won't work with spectators.

void SV_Loadgame_f(void)
{
	char	filename[MAX_OSPATH];
	FILE	*f;
	char	mapname[MAX_QPATH];
	float	time, tfloat;
	char	str[32768];
	int		i;
	edict_t	*ent;
	int		version;
	int pt;

	int slots;
	int current_skill;

	client_t *cl;
	int clnum;
	char plname[32];

	int filelen, filepos;
	char *file;

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf (STL_LOADSYNTAX);
		return;
	}

//	if (sv.state != ss_active)
//	{
//		Con_Printf("Can't apply: Server isn't running or is still loading\n");
//		return;
//	}

	sprintf (filename, "%s/saves/%s", com_gamedir, Cmd_Argv(1));
	COM_DefaultExtension (filename, ".sav");

// we can't call SCR_BeginLoadingPlaque, because too much stack space has
// been used.  The menu calls it before stuffing loadgame command
//	SCR_BeginLoadingPlaque ();

	Con_TPrintf (STL_LOADGAMEFROM, filename);
	f = fopen (filename, "rb");
	if (!f)
	{
		Con_TPrintf (STL_ERRORCOULDNTOPEN);
		return;
	}

	fscanf (f, "%i\n", &version);
	if (version != SAVEGAME_VERSION && version != 5 && version != 6)	//5 for NQ, 6 for ZQ/FQ
	{
		fclose (f);
		Con_TPrintf (STL_BADSAVEVERSION, version, SAVEGAME_VERSION);
		return;
	}
	fscanf (f, "%s\n", str);
	if (version == 5)
	{
		Con_Printf("loading single player game\n");
	}
	else if (version == 6)	//this is fuhquake's single player games
	{
		Con_Printf("loading single player qw game\n");
	}
	else
		Con_Printf("loading FTE saved game\n");



	for (clnum = 0; clnum < sv.allocated_client_slots; clnum++)	//clear the server for the level change.
	{
		cl = &svs.clients[clnum];
		if (cl->state <= cs_zombie)
			continue;

		MSG_WriteByte (&cl->netchan.message, svc_stufftext);
		MSG_WriteString (&cl->netchan.message, "disconnect;wait;reconnect\n");	//kindly ask the client to come again.
		cl->drop = true;
	}
	SV_SendMessagesToAll();

	if (version == 5 || version == 6)
	{
		SV_UpdateMaxPlayers(1);
		cl = &svs.clients[0];
#ifdef SERVERONLY
		strcpy(cl->name, "");
#else
		strcpy(cl->name, name.string);
#endif
		cl->state = cs_zombie;
		cl->connection_started = realtime+20;
		cl->istobeloaded = true;

		for (i=0 ; i<16 ; i++)
			fscanf (f, "%f\n", &cl->spawn_parms[i]);
		for (; i < NUM_SPAWN_PARMS; i++)
			cl->spawn_parms[i] = 0;
	}
	else	//fte saves ALL the clients on the server.
	{
		fscanf (f, "%f\n", &tfloat);
		slots = tfloat;
		if (!slots)	//err
		{
			fclose (f);
			Con_Printf ("Corrupted save game");
			return;
		}
		SV_UpdateMaxPlayers(slots);
		for (clnum = 0; clnum < sv.allocated_client_slots; clnum++)	//work out which players we had when we saved, and hope they accepted the reconnect.
		{
			cl = &svs.clients[clnum];
			fscanf(f, "%s\n", plname);

			cl->istobeloaded = false;

			cl->state = cs_free;

			COM_Parse(plname);

			if (!*com_token)
				continue;

			strcpy(cl->name, com_token);
			cl->state = cs_zombie;
			cl->connection_started = realtime+20;
			cl->istobeloaded = true;

			for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
				fscanf (f, "%f\n", &cl->spawn_parms[i]);
		}
	}
	if (version == 5 || version == 6)
	{
		fscanf (f, "%f\n", &tfloat);
		current_skill = (int)(tfloat + 0.1);
		Cvar_Set ("skill", va("%i", current_skill));
		Cvar_SetValue ("deathmatch", 0);
		Cvar_SetValue ("coop", 0);
		Cvar_SetValue ("teamplay", 0);

		if (version == 5)
		{
			progstype = PROG_NQ;
			Cvar_SetVar (pr_ssqc_progs, "progs.dat");	//NQ's progs.
		}
		else
		{
			progstype = PROG_QW;
			Cvar_SetVar (&pr_ssqc_progs, "spprogs.dat");	//zquake's single player qw progs.
		}
		pt = 0;
	}
	else
	{
		fscanf (f, "%f\n", &tfloat);
		pt = tfloat;

	// this silliness is so we can load 1.06 save files, which have float skill values
		fscanf (f, "%f\n", &tfloat);
		current_skill = (int)(tfloat + 0.1);
		Cvar_Set ("skill", va("%i", current_skill));

		fscanf (f, "%f\n", &tfloat);
		Cvar_SetValue ("deathmatch", tfloat);
		fscanf (f, "%f\n", &tfloat);
		Cvar_SetValue ("coop", tfloat);
		fscanf (f, "%f\n", &tfloat);
		Cvar_SetValue ("teamplay", tfloat);
	}
	fscanf (f, "%s\n",mapname);
	fscanf (f, "%f\n",&time);

	SV_SpawnServer (mapname, NULL, false, false);	//always inits MAX_CLIENTS slots. That's okay, because we can cut the max easily.
	if (sv.state != ss_active)
	{
		fclose (f);
		Con_TPrintf (STL_LOADFAILED);
		return;
	}

	sv.allocated_client_slots = slots;

// load the light styles

	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		fscanf (f, "%s\n", str);
		if (sv.lightstyles[i])
			Z_Free(sv.lightstyles[i]);
		sv.lightstyles[i] = Z_Malloc (strlen(str)+1);
		strcpy (sv.lightstyles[i], str);
	}

// load the edicts out of the savegame file
// the rest of the file is sent directly to the progs engine.

	if (version == 5 || version == 6)
		Q_InitProgs();	//reinitialize progs entirly.
	else
	{
		Q_SetProgsParms(false);
		svs.numprogs = 0;

		PR_Configure(svprogfuncs, NULL, -1, MAX_PROGS);
		PR_RegisterFields();
		PR_InitEnts(svprogfuncs, sv.max_edicts);	//just in case the max edicts isn't set.
		progstype = pt;	//presumably the progs.dat will be what they were before.
	}

	filepos = ftell(f);
	fseek(f, 0, SEEK_END);
	filelen = ftell(f);
	fseek(f, filepos, SEEK_SET);
	filelen -= filepos;
	file = BZ_Malloc(filelen+1+8);
	memset(file, 0, filelen+1+8);
	strcpy(file, "loadgame");
	clnum=fread(file+8, 1, filelen, f);
	file[filelen+8]='\0';
	pr_edict_size=svprogfuncs->load_ents(svprogfuncs, file, 0);
	BZ_Free(file);

	PR_LoadGlabalStruct();

	sv.time = time;

	pr_global_struct->time = sv.physicstime;

	fclose (f);

	SV_ClearWorld ();

	for (i=0 ; i<sv.num_edicts ; i++)
	{
		ent = EDICT_NUM(svprogfuncs, i);

		if (!ent)
			break;
		if (ent->isfree)
			continue;

		SV_LinkEdict (ent, false);
	}

	for (i=0 ; i<sv.allocated_client_slots ; i++)
	{
		ent = EDICT_NUM(svprogfuncs, i+1);
		svs.clients[i].edict = ent;
	}
}
#endif




#define CACHEGAME_VERSION 513




void SV_FlushLevelCache(void)
{
	levelcache_t *cache;

	while(svs.levcache)
	{
		cache = svs.levcache->next;
		Z_Free(svs.levcache);
		svs.levcache = cache;
	}

}

void LoadModelsAndSounds(vfsfile_t *f)
{
	char	str[32768];
	int i;

	sv.strings.model_precache[0] = PR_AddString(svprogfuncs, "", 0, false);
	for (i=1; i < MAX_MODELS; i++)
	{
		VFS_GETS(f, str, sizeof(str));
		if (!*str)
			break;

		sv.strings.model_precache[i] = PR_AddString(svprogfuncs, str, 0, false);
	}
	if (i == MAX_MODELS)
	{
		VFS_GETS(f, str, sizeof(str));
		if (*str)
			SV_Error("Too many model precaches in loadgame cache");
	}
	for (; i < MAX_MODELS; i++)
		sv.strings.model_precache[i] = NULL;

//	sv.sound_precache[0] = PR_AddString(svprogfuncs, "", 0);
	for (i=1; i < MAX_SOUNDS; i++)
	{
		VFS_GETS(f, str, sizeof(str));
		if (!*str)
			break;

//		sv.sound_precache[i] = PR_AddString(svprogfuncs, str, 0);
	}
	if (i == MAX_SOUNDS)
	{
		VFS_GETS(f, str, sizeof(str));
		if (*str)
			SV_Error("Too many sound precaches in loadgame cache");
	}
	for (; i < MAX_SOUNDS; i++)
		*sv.strings.sound_precache[i] = 0;
}

/*ignoreplayers - says to not tell gamecode (a loadgame rather than a level change)*/
qboolean SV_LoadLevelCache(char *savename, char *level, char *startspot, qboolean isloadgame)
{
	eval_t *eval, *e2;

	char	name[MAX_OSPATH];
	vfsfile_t	*f;
	char	mapname[MAX_QPATH];
	float	time;
	char	str[32768];
	int		i,j;
	edict_t	*ent;
	int		version;

	int current_skill;

	int pt;

	int modelpos;

	int filelen, filepos;
	char *file;
	gametype_e gametype;

	levelcache_t *cache;
	int numstyles;

	if (isloadgame)
	{
		gametype = svs.gametype;
	}
	else
	{
		cache = svs.levcache;
		while(cache)
		{
			if (!strcmp(cache->mapname, level))
				break;

			cache = cache->next;
		}
		if (!cache)
			return false;	//not visited yet. Ignore the existing caches as fakes.

		gametype = cache->gametype;
	}

	if (savename)
		Q_snprintfz (name, sizeof(name), "saves/%s/%s", savename, level);
	else
		Q_snprintfz (name, sizeof(name), "saves/%s", level);
	COM_DefaultExtension (name, ".lvc", sizeof(name));

//	Con_TPrintf (STL_LOADGAMEFROM, name);

#ifdef Q2SERVER
	if (gametype == GT_QUAKE2)
	{
		char syspath[MAX_OSPATH];
		SV_SpawnServer (level, startspot, false, false);

		World_ClearWorld(&sv.world);
		if (!ge)
		{
			Con_Printf("Incorrect gamecode type.\n");
			return false;
		}

		if (!FS_NativePath(name, FS_GAME, syspath, sizeof(syspath)))
			return false;

		ge->ReadLevel(syspath);

		for (i=0 ; i<100 ; i++)	//run for 10 secs to iron out a few bugs.
			ge->RunFrame ();
		return true;
	}
#endif

// we can't call SCR_BeginLoadingPlaque, because too much stack space has
// been used.  The menu calls it before stuffing loadgame command
//	SCR_BeginLoadingPlaque ();

	f = FS_OpenVFS(name, "rb", FS_GAME);
	if (!f)
	{
		Con_Printf ("ERROR: Couldn't load \"%s\"\n", name);
		return false;
	}

	VFS_GETS(f, str, sizeof(str));
	version = atoi(str);
	if (version != CACHEGAME_VERSION)
	{
		VFS_CLOSE (f);
		Con_TPrintf (STL_BADSAVEVERSION, version, CACHEGAME_VERSION);
		return false;
	}
	VFS_GETS(f, str, sizeof(str));	//comment

	SV_SendMessagesToAll();

	VFS_GETS(f, str, sizeof(str));
	pt = atof(str);

// this silliness is so we can load 1.06 save files, which have float skill values
	VFS_GETS(f, str, sizeof(str));
	current_skill = (int)(atof(str) + 0.1);
	Cvar_Set (&skill, va("%i", current_skill));

	VFS_GETS(f, str, sizeof(str));
	Cvar_SetValue (&deathmatch, atof(str));
	VFS_GETS(f, str, sizeof(str));
	Cvar_SetValue (&coop, atof(str));
	VFS_GETS(f, str, sizeof(str));
	Cvar_SetValue (&teamplay, atof(str));

	VFS_GETS(f, mapname, sizeof(mapname));
	VFS_GETS(f, str, sizeof(str));
	time = atof(str);

	SV_SpawnServer (mapname, startspot, false, false);
	sv.time = time;
	if (svs.gametype != gametype)
	{
		Con_Printf("Incorrect gamecode type. Cannot load game.\n");
		return false;
	}
	if (sv.state != ss_active)
	{
		VFS_CLOSE (f);
		Con_TPrintf (STL_LOADFAILED);
		return false;
	}

//	sv.paused = true;		// pause until all clients connect
//	sv.loadgame = true;

// load the light styles

	VFS_GETS(f, str, sizeof(str));
	numstyles = atoi(str);
	if (numstyles > MAX_LIGHTSTYLES)
	{
		VFS_CLOSE (f);
		Con_Printf ("load failed - invalid number of lightstyles\n");
		return false;
	}

// load the edicts out of the savegame file
// the rest of the file is sent directly to the progs engine.

	/*hexen2's gamecode doesn't have SAVE set on all variables, in which case we must clobber them, and run the risk that they were set at map load time, but clear in the savegame.*/
	if (progstype != PROG_H2)
	{
		Q_SetProgsParms(false);
		PR_Configure(svprogfuncs, -1, MAX_PROGS);
		PR_RegisterFields();
		PR_InitEnts(svprogfuncs, sv.world.max_edicts);
	}

	for (i = 0; i<MAX_LIGHTSTYLES ; i++)
	{
		if (sv.strings.lightstyles[i])
			BZ_Free(sv.strings.lightstyles[i]);
		sv.strings.lightstyles[i] = NULL;
	}

	for (i=0 ; i<numstyles ; i++)
	{
		VFS_GETS(f, str, sizeof(str));
		sv.strings.lightstyles[i] = BZ_Malloc(strlen(str)+1);
		strcpy (sv.strings.lightstyles[i], str);
	}
	for ( ; i<MAX_LIGHTSTYLES ; i++)
	{
		sv.strings.lightstyles[i] = BZ_Malloc(1);
		strcpy (sv.strings.lightstyles[i], "");
	}

	modelpos = VFS_TELL(f);
	LoadModelsAndSounds(f);

	filepos = VFS_TELL(f);
	filelen = VFS_GETLEN(f);
	filelen -= filepos;
	file = BZ_Malloc(filelen+1);
	memset(file, 0, filelen+1);
	VFS_READ(f, file, filelen);
	file[filelen]='\0';
	sv.world.edict_size=svprogfuncs->load_ents(svprogfuncs, file, 0);
	BZ_Free(file);

	progstype = pt;

	PR_LoadGlabalStruct();

	pr_global_struct->serverflags = svs.serverflags;
	pr_global_struct->time = sv.time = sv.world.physicstime = time;
	sv.starttime = Sys_DoubleTime() - sv.time;

	VFS_SEEK(f, modelpos);
	LoadModelsAndSounds(f);

	VFS_CLOSE(f);

	PF_InitTempStrings(svprogfuncs);

	World_ClearWorld (&sv.world);

	for (i=0 ; i<svs.allocated_client_slots ; i++)
	{
		if (i < sv.allocated_client_slots)
			ent = EDICT_NUM(svprogfuncs, i+1);
		else
			ent = NULL;
		svs.clients[i].edict = ent;

		svs.clients[i].name = PR_AddString(svprogfuncs, svs.clients[i].namebuf, sizeof(svs.clients[i].namebuf), false);
		svs.clients[i].team = PR_AddString(svprogfuncs, svs.clients[i].teambuf, sizeof(svs.clients[i].teambuf), false);

		if (ent)
			svs.clients[i].playerclass = ent->xv->playerclass;
		else
			svs.clients[i].playerclass = 0;
	}

	if (!isloadgame)
	{
		eval = PR_FindGlobal(svprogfuncs, "startspot", 0, NULL);
		if (eval) eval->_int = (int)PR_NewString(svprogfuncs, startspot, 0);

		eval = PR_FindGlobal(svprogfuncs, "ClientReEnter", 0, NULL);
		if (eval)
		for (i=0 ; i<sv.allocated_client_slots ; i++)
		{
			if (svs.clients[i].spawninfo)
			{
				globalvars_t *pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
				ent = svs.clients[i].edict;
				j = strlen(svs.clients[i].spawninfo);
				svprogfuncs->restoreent(svprogfuncs, svs.clients[i].spawninfo, &j, ent);

				e2 = svprogfuncs->GetEdictFieldValue(svprogfuncs, ent, "stats_restored", NULL);
				if (e2)
					e2->_float = 1;
				for (j=0 ; j< NUM_SPAWN_PARMS ; j++)
				{
					if (pr_global_ptrs->spawnparamglobals[j])
						*pr_global_ptrs->spawnparamglobals[j] = host_client->spawn_parms[j];
				}
				pr_global_struct->time = sv.world.physicstime;
				pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, ent);
				ent->area.next = ent->area.prev = NULL;
				G_FLOAT(OFS_PARM0) = sv.time-host_client->spawninfotime;
				PR_ExecuteProgram(svprogfuncs, eval->function);
			}
		}
	}

	for (i=0 ; i<sv.world.num_edicts ; i++)
	{
		ent = EDICT_NUM(svprogfuncs, i);
		if (ent->isfree)
			continue;

		World_LinkEdict (&sv.world, (wedict_t*)ent, false);
	}
	for (i=0 ; i<sv.world.num_edicts ; i++)
	{
		ent = EDICT_NUM(svprogfuncs, i);
		if (ent->isfree)
			continue;

		/*hexen2 instead overwrites ents, which can theoretically be unreliable (ents with this flag are not saved in the first place, and thus are effectively reset instead of reloaded).
		  fte purges all ents beforehand in a desperate attempt to remain sane.
		  this behaviour does not match exactly, but is enough for vanilla hexen2/POP.
		*/
		if ((unsigned int)ent->v->flags & FL_HUBSAVERESET)
		{
			func_t f;
			/*set some minimal fields*/
			ent->v->solid = SOLID_NOT;
			ent->v->use = 0;
			ent->v->touch = 0;
			ent->v->think = 0;
			ent->v->nextthink = 0;
			/*reinvoke the spawn function*/
			pr_global_struct->time = 0.1;
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, ent);
			f = PR_FindFunction(svprogfuncs, PR_GetString(svprogfuncs, ent->v->classname), PR_ANY);

			if (f)
				PR_ExecuteProgram(svprogfuncs, f);
		}
	}
	pr_global_struct->time = sv.world.physicstime;

	return true;	//yay
}

void SV_SaveLevelCache(char *savedir, qboolean dontharmgame)
{
	int len;
	char *s;
	client_t *cl;
	int clnum;

	char	name[256];
	vfsfile_t	*f;
	int		i;
	char	comment[SAVEGAME_COMMENT_LENGTH+1];
	levelcache_t *cache;

	if (!sv.state)
		return;

	if (!dontharmgame)
	{
		cache = svs.levcache;
		while(cache)
		{
			if (!strcmp(cache->mapname, sv.name))
				break;

			cache = cache->next;
		}
		if (!cache)	//not visited yet. Let us know that we went there.
		{
			cache = Z_Malloc(sizeof(levelcache_t)+strlen(sv.name)+1);
			cache->mapname = (char *)(cache+1);
			strcpy(cache->mapname, sv.name);

			cache->gametype = svs.gametype;
			cache->next = svs.levcache;
			svs.levcache = cache;
		}
	}

	if (savedir)
		Q_snprintfz (name, sizeof(name), "saves/%s/%s", savedir, sv.name);
	else
		Q_snprintfz (name, sizeof(name), "saves/%s", sv.name);
	COM_DefaultExtension (name, ".lvc", sizeof(name));

	FS_CreatePath(name, FS_GAMEONLY);

	if (!dontharmgame)	//save game in progress
		Con_TPrintf (STL_SAVEGAMETO, name);

#ifdef Q2SERVER
	if (ge)
	{
		char	syspath[256];

		if (!FS_NativePath(name, FS_GAMEONLY, syspath, sizeof(syspath)))
			return;
		ge->WriteLevel(syspath);
		FS_FlushFSHashReally();
		return;
	}
#endif

#ifdef HLSERVER
	if (svs.gametype == GT_HALFLIFE)
	{
		SVHL_SaveLevelCache(name);
		return;
	}
#endif

	f = FS_OpenVFS (name, "wb", FS_GAME);
	if (!f)
	{
		Con_TPrintf (STL_ERRORCOULDNTOPEN);
		return;
	}

	VFS_PRINTF (f, "%i\n", CACHEGAME_VERSION);
	SV_SavegameComment (comment);
	VFS_PRINTF (f, "%s\n", comment);
	if (!dontharmgame)
	{
		for (cl = svs.clients, clnum=0; clnum < sv.allocated_client_slots; cl++,clnum++)//fake dropping
		{
			if (cl->state < cs_spawned && !cl->istobeloaded)	//don't drop if they are still connecting
			{
				cl->edict->v->solid = 0;
			}
			else if (!cl->spectator)
			{
				// call the prog function for removing a client
				// this will set the body to a dead frame, among other things
				pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, cl->edict);
				PR_ExecuteProgram (svprogfuncs, pr_global_struct->ClientDisconnect);
				sv.spawned_client_slots--;
			}
			else if (SpectatorDisconnect)
			{
				// call the prog function for removing a client
				// this will set the body to a dead frame, among other things
				pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, cl->edict);
				PR_ExecuteProgram (svprogfuncs, SpectatorDisconnect);
				sv.spawned_observer_slots--;
			}
		}
	}
	VFS_PRINTF (f, "%d\n", progstype);
	VFS_PRINTF (f, "%f\n", skill.value);
	VFS_PRINTF (f, "%f\n", deathmatch.value);
	VFS_PRINTF (f, "%f\n", coop.value);
	VFS_PRINTF (f, "%f\n", teamplay.value);
	VFS_PRINTF (f, "%s\n", sv.name);
	VFS_PRINTF (f, "%f\n",sv.time);

// write the light styles

	VFS_PRINTF (f, "%i\n",MAX_LIGHTSTYLES);
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		VFS_PRINTF (f, "%s\n", sv.strings.lightstyles[i]?sv.strings.lightstyles[i]:"");
	}

	for (i=1 ; i<MAX_MODELS ; i++)
	{
		if (sv.strings.model_precache[i] && *sv.strings.model_precache[i])
			VFS_PRINTF (f, "%s\n", sv.strings.model_precache[i]);
		else
			break;
	}
	VFS_PRINTF (f,"\n");
	for (i=1 ; i<MAX_SOUNDS ; i++)
	{
		if (*sv.strings.sound_precache[i])
			VFS_PRINTF (f, "%s\n", sv.strings.sound_precache[i]);
		else
			break;
	}
	VFS_PRINTF (f,"\n");

	s = PR_SaveEnts(svprogfuncs, NULL, &len, 0, 1);
	VFS_PUTS(f, s);
	VFS_PUTS(f, "\n");
	svprogfuncs->parms->memfree(s);

	VFS_CLOSE (f);
}

#ifdef NEWSAVEFORMAT

#define FTESAVEGAME_VERSION 25000

void SV_Savegame (char *savename)
{
	extern cvar_t	nomonsters;
	extern cvar_t	gamecfg;
	extern cvar_t	scratch1;
	extern cvar_t	scratch2;
	extern cvar_t	scratch3;
	extern cvar_t	scratch4;
	extern cvar_t	savedgamecfg;
	extern cvar_t	saved1;
	extern cvar_t	saved2;
	extern cvar_t	saved3;
	extern cvar_t	saved4;
	extern cvar_t	temp1;
	extern cvar_t	noexit;
	extern cvar_t	pr_maxedicts;


	client_t *cl;
	int clnum;
	char	comment[SAVEGAME_COMMENT_LENGTH+1];
	vfsfile_t *f;
	int len;
	levelcache_t *cache;
	char str[MAX_LOCALINFO_STRING+1];
	char *savefilename;

	if (!sv.state)
	{
		Con_Printf("Server is not active - unable to save\n");
		return;
	}

	if (sv.allocated_client_slots == 1 && svs.gametype == GT_PROGS)
	{
		if (svs.clients->state > cs_connected && svs.clients[0].edict->v->health <= 0)
		{
			Con_Printf("Refusing to save while dead.\n");
			return;
		}
	}
	//FIXME: we should probably block saving during intermission too.

	/*catch invalid names*/
	if (!*savename || strstr(savename, ".."))
		savename = "quicksav";

	savefilename = va("saves/%s/info.fsv", savename);
	FS_CreatePath(savefilename, FS_GAMEONLY);
	f = FS_OpenVFS(savefilename, "wb", FS_GAMEONLY);
	if (!f)
	{
		Con_Printf("Couldn't open file saves/%s/info.fsv\n", savename);
		return;
	}
	SV_SavegameComment(comment);
	VFS_PRINTF (f, "%d\n", FTESAVEGAME_VERSION+svs.gametype);
	VFS_PRINTF (f, "%s\n", comment);

	VFS_PRINTF(f, "%i\n", sv.allocated_client_slots);
	for (cl = svs.clients, clnum=0; clnum < sv.allocated_client_slots; cl++,clnum++)
	{
		if (cl->state < cs_spawned && !cl->istobeloaded)	//don't save if they are still connecting
		{
			VFS_PRINTF(f, "\n");
			continue;
		}
		VFS_PRINTF(f, "%s\n", cl->name);

		if (*cl->name)
			for (len = 0; len < NUM_SPAWN_PARMS; len++)
				VFS_PRINTF(f, "%i (%f)\n", *(int*)&cl->spawn_parms[len], cl->spawn_parms[len]);	//write ints as not everyone passes a float in the parms.
																					//write floats too so you can use it to debug.
	}

	Q_strncpyz(str, svs.info, sizeof(str));
	Info_RemovePrefixedKeys(str, '*');
	VFS_PRINTF (f, "%s\n",	str);

	Q_strncpyz(str, localinfo, sizeof(str));
	Info_RemovePrefixedKeys(str, '*');
	VFS_PUTS(f, str);

	VFS_PRINTF (f, "\n{\n");	//all game vars. FIXME: Should save the ones that have been retrieved/set by progs.
	VFS_PRINTF (f, "skill			\"%s\"\n",	skill.string);
	VFS_PRINTF (f, "deathmatch		\"%s\"\n",	deathmatch.string);
	VFS_PRINTF (f, "coop			\"%s\"\n",	coop.string);
	VFS_PRINTF (f, "teamplay		\"%s\"\n",	teamplay.string);

	VFS_PRINTF (f, "nomonsters		\"%s\"\n",	nomonsters.string);
	VFS_PRINTF (f, "gamecfg\t		\"%s\"\n",	gamecfg.string);
	VFS_PRINTF (f, "scratch1		\"%s\"\n",	scratch1.string);
	VFS_PRINTF (f, "scratch2		\"%s\"\n",	scratch2.string);
	VFS_PRINTF (f, "scratch3		\"%s\"\n",	scratch3.string);
	VFS_PRINTF (f, "scratch4		\"%s\"\n",	scratch4.string);
	VFS_PRINTF (f, "savedgamecfg\t	\"%s\"\n",	savedgamecfg.string);
	VFS_PRINTF (f, "saved1			\"%s\"\n",	saved1.string);
	VFS_PRINTF (f, "saved2			\"%s\"\n",	saved2.string);
	VFS_PRINTF (f, "saved3			\"%s\"\n",	saved3.string);
	VFS_PRINTF (f, "saved4			\"%s\"\n",	saved4.string);
	VFS_PRINTF (f, "temp1			\"%s\"\n",	temp1.string);
	VFS_PRINTF (f, "noexit			\"%s\"\n",	noexit.string);
	VFS_PRINTF (f, "pr_maxedicts\t	\"%s\"\n",	pr_maxedicts.string);
	VFS_PRINTF (f, "progs			\"%s\"\n",	pr_ssqc_progs.string);
	VFS_PRINTF (f, "set nextserver		\"%s\"\n",	Cvar_Get("nextserver", "", 0, "")->string);
	VFS_PRINTF (f, "}\n");

	SV_SaveLevelCache(savename, true);	//add the current level.

	cache = svs.levcache;	//state from previous levels - just copy it all accross.
	VFS_PRINTF(f, "{\n");
	while(cache)
	{
		VFS_PRINTF(f, "%s\n", cache->mapname);
		if (strcmp(cache->mapname, sv.name))
		{
			FS_Copy(va("saves/%s.lvc", cache->mapname), va("saves/%s/%s.lvc", savename, cache->mapname), FS_GAME, FS_GAME);
		}
		cache = cache->next;
	}
	VFS_PRINTF(f, "}\n");

	VFS_PRINTF (f, "%s\n", sv.name);

	VFS_CLOSE(f);
}

void SV_Savegame_f (void)
{
	SV_Savegame(Cmd_Argv(1));
}

void SV_Loadgame_f (void)
{
	levelcache_t *cache;
	unsigned char str[MAX_LOCALINFO_STRING+1], *trim;
	unsigned char savename[MAX_QPATH];
	vfsfile_t *f;
	unsigned char filename[MAX_OSPATH];
	int version;
	int clnum;
	int slots;
	int loadzombies = 0;
	client_t *cl;
	gametype_e gametype;

	int len;

	Q_strncpyz(savename, Cmd_Argv(1), sizeof(savename));

	if (!*savename || strstr(savename, ".."))
		strcpy(savename, "quicksav");

	Q_snprintfz (filename, sizeof(filename), "saves/%s/info.fsv", savename);
	f = FS_OpenVFS (filename, "rb", FS_GAME);
	if (!f)
	{
		Con_TPrintf (STL_ERRORCOULDNTOPEN);
		return;
	}

	VFS_GETS(f, str, sizeof(str)-1);
	version = atoi(str);
	if (version < FTESAVEGAME_VERSION || version >= FTESAVEGAME_VERSION+GT_MAX)
	{
		VFS_CLOSE (f);
		Con_TPrintf (STL_BADSAVEVERSION, version, FTESAVEGAME_VERSION);
		return;
	}
	gametype = version - FTESAVEGAME_VERSION;
	VFS_GETS(f, str, sizeof(str)-1);
#ifndef SERVERONLY
	if (!cls.state)
#endif
		Con_TPrintf (STL_LOADGAMEFROM, filename);


	for (clnum = 0; clnum < svs.allocated_client_slots; clnum++)	//clear the server for the level change.
	{
		cl = &svs.clients[clnum];
		if (cl->state <= cs_zombie)
			continue;

		if (cl->protocol == SCP_QUAKE2)
			MSG_WriteByte (&cl->netchan.message, svcq2_stufftext);
		else
			MSG_WriteByte (&cl->netchan.message, svc_stufftext);
		MSG_WriteString (&cl->netchan.message, "echo Loading Game;disconnect;wait;wait;reconnect\n");	//kindly ask the client to come again.
		cl->istobeloaded = false;
	}

	SV_SendMessagesToAll();

	VFS_GETS(f, str, sizeof(str)-1);
	slots = atoi(str);
	for (cl = svs.clients, clnum=0; clnum < slots; cl++,clnum++)
	{
		if (cl->state > cs_zombie)
			SV_DropClient(cl);

		VFS_GETS(f, str, sizeof(str)-1);
		str[sizeof(cl->namebuf)-1] = '\0';
		for (trim = str+strlen(str)-1; trim>=str && *trim <= ' '; trim--)
			*trim='\0';
		for (trim = str; *trim <= ' ' && *trim; trim++)
			;
		strcpy(cl->namebuf, str);
		cl->name = cl->namebuf;
		if (*str)
		{
			cl->state = cs_zombie;
			cl->connection_started = realtime+20;
			cl->istobeloaded = true;
			loadzombies++;
			memset(&cl->netchan, 0, sizeof(cl->netchan));

			for (len = 0; len < NUM_SPAWN_PARMS; len++)
			{
				VFS_GETS(f, str, sizeof(str)-1);
				for (trim = str+strlen(str)-1; trim>=str && *trim <= ' '; trim--)
					*trim='\0';
				for (trim = str; *trim <= ' ' && *trim; trim++)

				if (*str == '(')
					cl->spawn_parms[len] = atof(str);
				else
				{
					version = atoi(str);
					cl->spawn_parms[len] = *(float *)&version;
				}
			}
		}
	}


	VFS_GETS(f, str, sizeof(str)-1);
	for (trim = str+strlen(str)-1; trim>=str && *trim <= ' '; trim--)
		*trim='\0';
	for (trim = str; *trim <= ' ' && *trim; trim++)
		;
	Info_RemovePrefixedKeys(str, '*');	//just in case
	Info_RemoveNonStarKeys(svs.info);
	len = strlen(svs.info);
	Q_strncpyz(svs.info+len, str, sizeof(svs.info)-len);

	VFS_GETS(f, str, sizeof(str)-1);
	for (trim = str+strlen(str)-1; trim>=str && *trim <= ' '; trim--)
		*trim='\0';
	for (trim = str; *trim <= ' ' && *trim; trim++)
		;
	Info_RemovePrefixedKeys(str, '*');	//just in case
	Info_RemoveNonStarKeys(localinfo);
	len = strlen(localinfo);
	Q_strncpyz(localinfo+len, str, sizeof(localinfo)-len);

	VFS_GETS(f, str, sizeof(str)-1);
	for (trim = str+strlen(str)-1; trim>=str && *trim <= ' '; trim--)
		*trim='\0';
	for (trim = str; *trim <= ' ' && *trim; trim++)
		;
	if (strcmp(str, "{"))
		SV_Error("Corrupt saved game\n");
	while(1)
	{
		if (!VFS_GETS(f, str, sizeof(str)-1))
			SV_Error("Corrupt saved game\n");
		for (trim = str+strlen(str)-1; trim>=str && *trim <= ' '; trim--)
			*trim='\0';
		for (trim = str; *trim <= ' ' && *trim; trim++)
			;
		if (!strcmp(str, "}"))
			break;
		else if (*str)
			Cmd_ExecuteString(str, RESTRICT_RCON);
	}

	SV_FlushLevelCache();

	VFS_GETS(f, str, sizeof(str)-1);
	for (trim = str+strlen(str)-1; trim>=str && *trim <= ' '; trim--)
		*trim='\0';
	for (trim = str; *trim <= ' ' && *trim; trim++)
		;
	if (strcmp(str, "{"))
		SV_Error("Corrupt saved game\n");
	while(1)
	{
		if (!VFS_GETS(f, str, sizeof(str)-1))
			SV_Error("Corrupt saved game\n");
		for (trim = str+strlen(str)-1; trim>=str && *trim <= ' '; trim--)
			*trim='\0';
		for (trim = str; *trim <= ' ' && *trim; trim++)
			;
		if (!strcmp(str, "}"))
			break;
		if (!*str)
			continue;

		cache = Z_Malloc(sizeof(levelcache_t)+strlen(str)+1);
		cache->mapname = (char *)(cache+1);
		strcpy(cache->mapname, str);
		cache->gametype = gametype;

		cache->next = svs.levcache;


		FS_Copy(va("saves/%s/%s.lvc", savename, cache->mapname), va("saves/%s.lvc", cache->mapname), FS_GAME, FS_GAMEONLY);

		svs.levcache = cache;
	}

	VFS_GETS(f, str, sizeof(str)-1);
	for (trim = str+strlen(str)-1; trim>=str && *trim <= ' '; trim--)
		*trim='\0';
	for (trim = str; *trim <= ' ' && *trim; trim++)
		;

	VFS_CLOSE(f);

	svs.gametype = gametype;
	SV_LoadLevelCache(savename, str, "", true);
	sv.allocated_client_slots = slots;
	sv.spawned_client_slots += loadzombies;
}
#endif

#endif
