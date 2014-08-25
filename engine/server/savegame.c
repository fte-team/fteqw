#include "quakedef.h"

#ifndef CLIENTONLY

extern cvar_t skill;
extern cvar_t deathmatch;
extern cvar_t coop;
extern cvar_t teamplay;
extern cvar_t pr_enable_profiling;

void SV_Savegame_f (void);

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

//expects the version to have already been parsed
void SV_Loadgame_Legacy(char *filename, vfsfile_t *f, int version)
{
	//FIXME: Multiplayer save probably won't work with spectators.
	char	mapname[MAX_QPATH];
	float	time;
	char	str[32768];
	int		i;
	edict_t	*ent;
	int pt;
	int lstyles;

	int slots;

	client_t *cl;
	int clnum;
	char plname[32];

	int filelen, filepos;
	char *file;

	char *modelnames[MAX_MODELS];

	if (version != 667 && version != 5 && version != 6)	//5 for NQ, 6 for ZQ/FQ
	{
		VFS_CLOSE (f);
		Con_TPrintf ("Unable to load savegame of version %i\n", version);
		return;
	}
	VFS_GETS(f, str, sizeof(str));	//discard comment.
	Con_Printf("loading legacy game from %s...\n", filename);



	for (clnum = 0; clnum < svs.allocated_client_slots; clnum++)	//clear the server for the level change.
	{
		cl = &svs.clients[clnum];
		if (cl->state <= cs_loadzombie)
			continue;

#ifndef SERVERONLY
		if (cl->netchan.remote_address.type == NA_LOOPBACK)
			CL_Disconnect();
		else
#endif
		{
			MSG_WriteByte (&cl->netchan.message, svc_stufftext);
			MSG_WriteString (&cl->netchan.message, "disconnect;wait;reconnect\n");	//kindly ask the client to come again.
		}
		cl->drop = true;
	}
	SV_SendMessagesToAll();

	if (version == 5 || version == 6)
	{
		slots = 1;
		SV_UpdateMaxPlayers(1);
		cl = &svs.clients[0];
#ifdef SERVERONLY
		Q_strncpyz(cl->namebuf, "", sizeof(cl->namebuf));
#else
		Q_strncpyz(cl->namebuf, name.string, sizeof(cl->namebuf));
#endif
		Q_strncpyz(cl->namebuf, com_token, sizeof(cl->namebuf));
		cl->name = cl->namebuf;
		cl->state = cs_loadzombie;
		cl->connection_started = realtime+20;
		cl->istobeloaded = true;

		for (i=0 ; i<16 ; i++)
		{
			VFS_GETS(f, str, sizeof(str));
			cl->spawn_parms[i] = atof(str);
		}
		for (; i < NUM_SPAWN_PARMS; i++)
			cl->spawn_parms[i] = 0;
	}
	else	//fte saves ALL the clients on the server.
	{
		VFS_GETS(f, str, strlen(str));
		slots = atoi(str);
		if (!slots)	//err
		{
			VFS_CLOSE(f);
			Con_Printf ("Corrupted save game");
			return;
		}
		SV_UpdateMaxPlayers(slots);
		for (clnum = 0; clnum < sv.allocated_client_slots; clnum++)	//work out which players we had when we saved, and hope they accepted the reconnect.
		{
			cl = &svs.clients[clnum];
			VFS_GETS(f, plname, sizeof(plname));

			cl->istobeloaded = false;

			cl->state = cs_free;

			COM_Parse(plname);

			if (!*com_token)
				continue;

			Q_strncpyz(cl->namebuf, com_token, sizeof(cl->namebuf));
			cl->name = cl->namebuf;
			cl->state = cs_loadzombie;
			cl->connection_started = realtime+20;
			cl->istobeloaded = true;

			//probably should be 32, rather than NUM_SPAWN_PARMS(64)
			for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
			{
				VFS_GETS(f, str, sizeof(str));
				cl->spawn_parms[i] = atof(str);
			}
		}
	}
	if (version == 5 || version == 6)
	{
		VFS_GETS(f, str, sizeof(str));
		Cvar_SetValue (Cvar_FindVar("skill"), atof(str));
		Cvar_SetValue (Cvar_FindVar("deathmatch"), 0);
		Cvar_SetValue (Cvar_FindVar("coop"), 0);
		Cvar_SetValue (Cvar_FindVar("teamplay"), 0);

		if (version == 5)
		{
			progstype = PROG_NQ;
			Cvar_Set (&pr_ssqc_progs, "progs.dat");	//NQ's progs.
		}
		else
		{
			progstype = PROG_QW;
			Cvar_Set (&pr_ssqc_progs, "spprogs.dat");	//zquake's single player qw progs.
		}
		pt = 0;
	}
	else
	{
		VFS_GETS(f, str, sizeof(str));
		pt = atoi(str);

	// this silliness is so we can load 1.06 save files, which have float skill values
		VFS_GETS(f, str, sizeof(str));
		Cvar_SetValue (Cvar_FindVar("skill"), atof(str));

		VFS_GETS(f, str, sizeof(str));
		Cvar_SetValue (Cvar_FindVar("deathmatch"), atof(str));
		VFS_GETS(f, str, sizeof(str));
		Cvar_SetValue (Cvar_FindVar("coop"), atof(str));
		VFS_GETS(f, str, sizeof(str));
		Cvar_SetValue (Cvar_FindVar("teamplay"), atof(str));
	}
	VFS_GETS(f, mapname, sizeof(mapname));
	VFS_GETS(f, str, sizeof(str));
	time = atof(str);

	SV_SpawnServer (mapname, NULL, false, false);	//always inits MAX_CLIENTS slots. That's okay, because we can cut the max easily.
	if (sv.state != ss_active)
	{
		VFS_CLOSE (f);
		Con_TPrintf ("Couldn't load map\n");
		return;
	}

	sv.allocated_client_slots = slots;

// load the light styles

	lstyles = 64;
	for (i=0 ; i<lstyles ; i++)
	{
		VFS_GETS(f, str, sizeof(str));
		if (sv.strings.lightstyles[i])
			Z_Free((char*)sv.strings.lightstyles[i]);
		sv.strings.lightstyles[i] = Z_StrDup(str);
	}
	for (; i < MAX_LIGHTSTYLES; i++)
	{
		if (sv.strings.lightstyles[i])
			Z_Free((char*)sv.strings.lightstyles[i]);
		sv.strings.lightstyles[i] = NULL;
	}

	//model names are pointers to vm-accessible memory. as that memory is going away, we need to destroy and recreate, which requires preserving them.
	for (i = 1; i < MAX_MODELS; i++)
	{
		if (!sv.strings.model_precache[i])
		{
			modelnames[i] = NULL;
			break;
		}
		modelnames[i] = Z_StrDup(sv.strings.model_precache[i]);
	}

// load the edicts out of the savegame file
// the rest of the file is sent directly to the progs engine.

	if (version == 5 || version == 6)
		Q_InitProgs();	//reinitialize progs entirly.
	else
	{
		Q_SetProgsParms(false);
		svs.numprogs = 0;

		PR_Configure(svprogfuncs, -1, MAX_PROGS, pr_enable_profiling.ival);
		PR_RegisterFields();
		PR_InitEnts(svprogfuncs, sv.world.max_edicts);	//just in case the max edicts isn't set.
		progstype = pt;	//presumably the progs.dat will be what they were before.
	}

	//reload model names.
	for (i = 1; i < MAX_MODELS; i++)
	{
		if (!modelnames[i])
			break;
		sv.strings.model_precache[i] = PR_AddString(svprogfuncs, modelnames[i], 0, false);
	}

	filepos = VFS_TELL(f);
	filelen = VFS_GETLEN(f);
	filelen -= filepos;
	file = BZ_Malloc(filelen+1+8);
	memset(file, 0, filelen+1+8);
	strcpy(file, "loadgame");
	clnum=VFS_READ(f, file+8, filelen);
	file[filelen+8]='\0';
	sv.world.edict_size=svprogfuncs->load_ents(svprogfuncs, file, 0);
	BZ_Free(file);

	PR_LoadGlabalStruct();

	pr_global_struct->time = sv.world.physicstime = sv.time = time;
	sv.starttime = Sys_DoubleTime() - sv.time;

	VFS_CLOSE(f);

	//FIXME: DP saved games have some / *\nkey values\nkey values\n* / thing in them to save precaches and stuff

	World_ClearWorld(&sv.world);

	for (i=0 ; i<sv.world.num_edicts ; i++)
	{
		ent = EDICT_NUM(svprogfuncs, i);

		if (!ent)
			break;
		if (ent->isfree)
			continue;

		World_LinkEdict (&sv.world, (wedict_t*)ent, false);
	}

	sv.spawned_client_slots = 0;
	for (i=0 ; i<svs.allocated_client_slots ; i++)
	{
		cl = &svs.clients[i];
		if (i < sv.allocated_client_slots)
		{
			if (cl->state)
				sv.spawned_client_slots += 1;
			ent = EDICT_NUM(svprogfuncs, i+1);
		}
		else
			ent = NULL;
		cl->edict = ent;

		cl->name = PR_AddString(svprogfuncs, cl->namebuf, sizeof(cl->namebuf), false);
		cl->team = PR_AddString(svprogfuncs, cl->teambuf, sizeof(cl->teambuf), false);

		if (ent)
			cl->playerclass = ent->xv->playerclass;
		else
			cl->playerclass = 0;
	}
}

void SV_LegacySavegame_f (void)
{
	int len;
	char *s = NULL;
	client_t *cl;
	int clnum;

	int version = SAVEGAME_VERSION;

	char	native[MAX_OSPATH];
	char	name[MAX_QPATH];
	vfsfile_t	*f;
	int		i;
	char	comment[SAVEGAME_COMMENT_LENGTH+1];

	if (Cmd_Argc() != 2)
	{
		Con_TPrintf ("save <savename> : save a game\n");
		return;
	}

	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_TPrintf ("Relative pathnames are not allowed\n");
		return;
	}

	if (sv.state != ss_active)
	{
		Con_TPrintf("Can't apply: Server isn't running or is still loading\n");
		return;
	}

	if (sv.allocated_client_slots != 1 || svs.clients->state != cs_spawned)
	{
		//we don't care about fte-format legacy.
		Con_TPrintf("Unable to use legacy savegame format to save multiplayer games\n");
		SV_Savegame_f();
		return;
	}

	sprintf (name, "%s", Cmd_Argv(1));
	COM_RequireExtension (name, ".sav", sizeof(name));
	if (!FS_NativePath(name, FS_GAMEONLY, native, sizeof(native)))
		return;
	Con_TPrintf (U8("Saving game to %s...\n"), native);
	f = FS_OpenVFS(name, "wb", FS_GAMEONLY);
	if (!f)
	{
		Con_TPrintf ("ERROR: couldn't open %s.\n", name);
		return;
	}

	//if there are 1 of 1 players connected
	if (sv.allocated_client_slots == 1 && svs.clients->state == cs_spawned)
	{//try to go for nq/zq compatability as this is a single player game.
		s = PR_SaveEnts(svprogfuncs, NULL, &len, 0, 2);	//get the entity state now, so that we know if we can get the full state in a q1 format.
		if (s)
		{
			if (progstype == PROG_QW)
				version = 6;
			else
				version = 5;
		}
	}


	VFS_PRINTF(f, "%i\n", version);
	SV_SavegameComment (comment);
	VFS_PRINTF(f, "%s\n", comment);

	if (version != SAVEGAME_VERSION)
	{
		//only 16 spawn parms.
		for (i=0; i < 16; i++)
			VFS_PRINTF(f, "%f\n", svs.clients->spawn_parms[i]);	//client 1.
		VFS_PRINTF(f, "%f\n", skill.value);
	}
	else
	{
		VFS_PRINTF(f, "%i\n", sv.allocated_client_slots);
		for (cl = svs.clients, clnum=0; clnum < sv.allocated_client_slots; cl++,clnum++)
		{
			if (cl->state < cs_spawned && !cl->istobeloaded)	//don't save if they are still connecting
			{
				VFS_PRINTF(f, "\"\"\n");
				continue;
			}

			VFS_PRINTF(f, "\"%s\"\n", cl->name);
			for (i=0; i<NUM_SPAWN_PARMS ; i++)
				VFS_PRINTF(f, "%f\n", cl->spawn_parms[i]);
		}
		VFS_PRINTF(f, "%i\n", progstype);
		VFS_PRINTF(f, "%f\n", skill.value);
		VFS_PRINTF(f, "%f\n", deathmatch.value);
		VFS_PRINTF(f, "%f\n", coop.value);
		VFS_PRINTF(f, "%f\n", teamplay.value);
	}
	VFS_PRINTF(f, "%s\n", sv.name);
	VFS_PRINTF(f, "%f\n",sv.time);

// write the light styles (only 64 are saved in legacy saved games)
	for (i=0 ; i < 64; i++)
	{
		if (sv.strings.lightstyles[i] && *sv.strings.lightstyles[i])
			VFS_PRINTF(f, "%s\n", sv.strings.lightstyles[i]);
		else
			VFS_PRINTF(f,"m\n");
	}

	if (!s)
		s = PR_SaveEnts(svprogfuncs, NULL, &len, 0, 1);
	VFS_PUTS(f, s);
	VFS_PUTS(f, "\n");
	svprogfuncs->parms->memfree(s);

	VFS_CLOSE(f);
}




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

//	Con_TPrintf ("Loading game from %s...\n", name);

#ifdef Q2SERVER
	if (gametype == GT_QUAKE2)
	{
		flocation_t loc;
		SV_SpawnServer (level, startspot, false, false);

		World_ClearWorld(&sv.world);
		if (!ge)
		{
			Con_Printf("Incorrect gamecode type.\n");
			return false;
		}

		if (!FS_FLocateFile(name, FSLFRT_IFFOUND, &loc))
		{
			Con_Printf("Couldn't find %s.\n", name);
			return false;
		}
		if (!*loc.rawname || loc.offset)
		{
			Con_Printf("%s is inside a package and cannot be used by the quake2 gamecode.\n", name);
			return false;
		}
		ge->ReadLevel(loc.rawname);

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
		if (isloadgame)
			Con_Printf ("ERROR: Couldn't load \"%s\"\n", name);
		return false;
	}

	VFS_GETS(f, str, sizeof(str));
	version = atoi(str);
	if (version != CACHEGAME_VERSION)
	{
		VFS_CLOSE (f);
		Con_TPrintf ("Savegame is version %i, not %i\n", version, CACHEGAME_VERSION);
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
		Con_TPrintf ("Couldn't load map\n");
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
		PR_Configure(svprogfuncs, -1, MAX_PROGS, pr_enable_profiling.ival);
		PR_RegisterFields();
		PR_InitEnts(svprogfuncs, sv.world.max_edicts);
	}

	for (i = 0; i<MAX_LIGHTSTYLES ; i++)
	{
		if (sv.strings.lightstyles[i])
			BZ_Free((void*)sv.strings.lightstyles[i]);
		sv.strings.lightstyles[i] = NULL;
	}

	for (i=0 ; i<numstyles ; i++)
	{
		VFS_GETS(f, str, sizeof(str));
		sv.strings.lightstyles[i] = Z_StrDup(str);
	}
	for ( ; i<MAX_LIGHTSTYLES ; i++)
	{
		sv.strings.lightstyles[i] = Z_StrDup("");
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
		Con_TPrintf ("Saving game to %s...\n", name);

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

	f = FS_OpenVFS (name, "wb", FS_GAMEONLY);
	if (!f)
	{
		Con_TPrintf ("ERROR: couldn't open %s.\n", name);
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

	VFS_PRINTF (f, "%g\n", (float)svs.serverflags);

	VFS_CLOSE(f);

#ifdef Q2SERVER
	//save the player's inventory and other map-persistant state that is owned by the gamecode.
	if (ge)
	{
		char syspath[256];
		if (!FS_NativePath(va("saves/%s/game.gsv", savename), FS_GAMEONLY, syspath, sizeof(syspath)))
			return;
		ge->WriteGame(syspath, false);
		FS_FlushFSHashReally();
	}
#endif

	FS_FlushFSHashReally();
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
		f = FS_OpenVFS (va("%s.sav", savename), "rb", FS_GAME);
		if (f)
			Q_snprintfz (filename, sizeof(filename), "%s.sav", savename);
	}

	if (!f)
	{
		Con_TPrintf ("ERROR: couldn't open %s.\n", filename);
		return;
	}

	VFS_GETS(f, str, sizeof(str)-1);
	version = atoi(str);
	if (version < FTESAVEGAME_VERSION || version >= FTESAVEGAME_VERSION+GT_MAX)
	{
		SV_Loadgame_Legacy(filename, f, version);
		return;
	}
#ifndef SERVERONLY
	if (cls.state)
		CL_Disconnect_f();
#endif

	gametype = version - FTESAVEGAME_VERSION;
	VFS_GETS(f, str, sizeof(str)-1);
#ifndef SERVERONLY
	if (!cls.state)
#endif
		Con_TPrintf ("Loading game from %s...\n", filename);


	for (clnum = 0; clnum < svs.allocated_client_slots; clnum++)	//clear the server for the level change.
	{
		cl = &svs.clients[clnum];
		if (cl->state <= cs_loadzombie)
			continue;

#ifndef SERVERONLY
		if (cl->netchan.remote_address.type == NA_LOOPBACK)
			CL_Disconnect();
		else
#endif
		{
			if (cl->protocol == SCP_QUAKE2)
				MSG_WriteByte (&cl->netchan.message, svcq2_stufftext);
			else
				MSG_WriteByte (&cl->netchan.message, svc_stufftext);
			MSG_WriteString (&cl->netchan.message, "echo Loading Game;disconnect;wait;wait;reconnect\n");	//kindly ask the client to come again.
		}
		cl->istobeloaded = false;
	}

	SV_SendMessagesToAll();

	VFS_GETS(f, str, sizeof(str)-1);
	slots = atoi(str);
	if (slots > svs.allocated_client_slots)
		SV_UpdateMaxPlayers(slots);
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
			cl->state = cs_loadzombie;
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
					;
				if (*trim == '(')
					cl->spawn_parms[len] = atof(trim+1);
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

	//serverflags is reset on restart, so we need to read the value as it was at the start of the current map.
	VFS_GETS(f, filename, sizeof(filename)-1);
	svs.serverflags = atof(filename);

	VFS_CLOSE(f);

#ifdef Q2SERVER
	if (gametype == GT_QUAKE2)
	{
		flocation_t loc;
		char *name = va("saves/%s/game.gsv", savename);
		if (!FS_FLocateFile(name, FSLFRT_IFFOUND, &loc))
			Con_Printf("Couldn't find %s.\n", name);
		else if (!*loc.rawname || loc.offset)
			Con_Printf("%s is inside a package and cannot be used by the quake2 gamecode.\n", name);
		else
		{
			SVQ2_InitGameProgs();
			if (ge)
				ge->ReadGame(loc.rawname);
		}
	}
#endif

	svs.gametype = gametype;
	SV_LoadLevelCache(savename, str, "", true);
	sv.allocated_client_slots = slots;
	sv.spawned_client_slots += loadzombies;
}
#endif
