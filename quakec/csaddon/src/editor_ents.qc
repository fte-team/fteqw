//#define ENTS_NYI

#ifdef ENTS_NYI
void() editor_ents_add =
{
};

void(vector mousepos) editor_ents_overlay =
{
	drawrawstring('0 32 0', "Not Yet Implemented", '8 8 0', '1 1 1', 1);
};
#else
static int selectedent;
static entity tempent;

typedef struct
{
	//quick access for rendering.
	float modelindex;
	float isbsp;
	float alpha;
	vector colourmod;
	vector org;
	vector ang;
	vector mins;
	vector maxs;
	float scale;

	//full data
	hashtable fields;
} entedit_t;

static entedit_t *editents;
static int numents;

struct
{	//FIXME: should probably parse quakeed comments or something.
	//FIXME: bbox colours.
	string classn;
	string model;
	vector colour;
	vector mins, maxs;
	float spawnflags_match;
	float spawnflags_mask;
} entclasses[] =
{
	{"info_player_start", 				"progs/player.mdl",		'0 1 0',	'-16 -16 -24',	'16 16 32'},
	{"info_player_start", 				"progs/player.mdl",		'1 0 0',	'-16 -16 -24',	'16 16 32'},
	{"info_player_start2",				"progs/player.mdl",		'1 0 0',	'-16 -16 -24',	'16 16 32'},
	{"info_player_deathmatch",		"progs/player.mdl",		'1 0 1',	'-16 -16 -24',	'16 16 32'},
	{"info_player_coop", 				"progs/player.mdl",		'1 0 1',	'-16 -16 -24',	'16 16 32'},

	{"item_armor1", 				"progs/armor.mdl",		'0 0.5 0.8',	'-16 -16 0',	'16 16 56'},
	{"item_armor2", 				"progs/armor.mdl",		'0 0.5 0.8',	'-16 -16 0',	'16 16 56'},
	{"item_armorInv", 				"progs/armor.mdl",		'0 0.5 0.8',	'-16 -16 0',	'16 16 56'},

	{"weapon_supershotgun", 			"progs/g_shot.mdl",		'0 0.5 0.8',	'-16 -16 0',	'16 16 56'},
	{"weapon_nailgun", 				"progs/g_nail.mdl",		'0 0.5 0.8',	'-16 -16 0',	'16 16 56'},
	{"weapon_supernailgun", 			"progs/g_nail2.mdl",		'0 0.5 0.8',	'-16 -16 0',	'16 16 56'},
	{"weapon_grenadelauncher",		"progs/g_rock.mdl",		'0 0.5 0.8',	'-16 -16 0',	'16 16 56'},
	{"weapon_rocketlauncher", 		"progs/g_rock2.mdl",		'0 0.5 0.8',	'-16 -16 0',	'16 16 56'},
	{"weapon_lightning", 				"progs/g_light.mdl",		'0 0.5 0.8',	'-16 -16 0',	'16 16 56'},

	{"item_key1", 					"progs/w_s_key.mdl",	'0 0.5 0.8',	'-16 -16 -24',	'16 16 32'},
	{"item_key2", 					"progs/w_g_key.mdl",	'0 0.5 0.8',	'-16 -16 -24',	'16 16 32'},
	{"item_sigil", 					"progs/end1.mdl",		'0 0.5 0.8',	'-16 -16 -24',	'16 16 32'},

	{"item_artifact_invulnerability", 		"progs/invulner.mdl",		'0 0.5 0.8',	'-16 -16 -24',	'16 16 32'},
	{"item_artifact_envirosuit", 		"progs/suit.mdl",		'0 0.5 0.8',	'-16 -16 -24',	'16 16 32'},
	{"item_artifact_invisibility", 		"progs/invisibl.mdl",		'0 0.5 0.8',	'-16 -16 -24',	'16 16 32'},
	{"item_artifact_super_damage", 		"progs/quaddama.mdl",	'0 0.5 0.8',	'-16 -16 -24',	'16 16 32'},



//	{"func_button",					0,				'0 .5 .8', 	?
//	{"trigger_changelevel",			0,				'0.5 0.5 0.5'	? //NO_INTERMISSION
//	{"func_door",					0,				'0 .5 .8,'		? //START_OPEN x DOOR_DONT_LINK GOLD_KEY SILVER_KEY TOGGLE
//	{"func_door_secret",				0,				'0 .5 .8',		? //open_once 1st_left 1st_down no_shoot always_shoot
//	{"trigger_portaldespawn",			0,				'.5 .5 .5',	?
//	{"func_wall",					0,				'0 .5 .8', 	?
//	{"func_illusionary",				0,				'0 .5 .8',		?
//	{"func_episodegate",				0,				'0 .5 .8',		? //E1 E2 E3 E4
//	{"func_bossgate",				0,				'0 .5 .8',		?
//	{"func_plat",					0,				'0 .5 .8'		? PLAT_LOW_TRIGGER
//	{"func_train",					0,				'0 .5 .8'		?
//	{"trigger_multiple",				0,				'.5 .5 .5' 	? notouch
//	{"trigger_once",					0,				'.5 .5 .5' 	? notouch
//	{"trigger_secret",				0,				'.5 .5 .5' 	?
//	{"trigger_counter",				0,				'.5 .5 .5' 	? nomessage
//	{"trigger_teleport",				0,				'.5 .5 .5' 	? PLAYER_ONLY SILENT
//	{"trigger_setskill",				0,				'.5 .5 .5' 	?
//	{"trigger_onlyregistered",			0,				'.5 .5 .5' 	?
//	{"trigger_hurt",					0,				'.5 .5 .5' 	?
//	{"trigger_push",					0,				'.5 .5 .5' 	? PUSH_ONCE
//	{"trigger_monsterjump",			0,				'.5 .5 .5', 	?
//	{"trigger_motionsickness",			0,				'.5 .5 .5', 	?
//	{"worldspawn",					0,				'0 0 0) ?

	{"path_corner",					0,				'0.5 0.3 0', 	'-8 -8 -8',	'8 8 8'},
	{"event_lightning",				0,				'0 1 1', 		'-16 -16 -16',	'16 16 16'},
	{"info_intermission",				0,				'1 0.5 0.5', 	'-16 -16 -16',	'16 16 16'},
	{"misc_portalspawn",				0,				'.5 .5 .5', 		'-8 -8 -8',	'8 8 8'},
	{"noclass",					0,				'0 0 0', 		'-8 -8 -8',	'8 8 8'},
	{"item_health",					"maps/b_bh25.bsp",	'.3 .3 1', 		'0 0 0',		'32 32 32'}, //rotten megahealth
	{"item_shells",					"maps/b_shell0.bsp",	'0 .5 .8', 		'0 0 0',		'32 32 32'}, //big
	{"item_spikes",					"maps/b_nail0.bsp",	'0 .5 .8', 		'0 0 0',		'32 32 32'}, //big
	{"item_rockets",				"maps/b_rock0.bsp",	'0 .5 .8', 		'0 0 0',		'32 32 32'}, //big
	{"item_cells",					"maps/b_batt0.bsp",	'0 .5 .8', 		'0 0 0',		'32 32 32'}, //big
	{"item_weapon",				0,				'0 .5 .8', 		'0 0 0',		'32 32 32'}, //shotgun rocket spikes big
	{"info_null",					0,				'0 0.5 0', 		'-4 -4 -4',	'4 4 4'},
	{"info_notnull",					0,				'0 0.5 0', 		'-4 -4 -4',	'4 4 4'},
	{"light",						0,				'0 1 0', 		'-8 -8 -8',	'8 8 8'}, //START_OFF
	{"light_fluoro",					0,				'0 1 0', 		'-8 -8 -8',	'8 8 8'},//START_OFF
	{"light_fluorospark",				0,				'0 1 0', 		'-8 -8 -8',	'8 8 8'},
	{"light_globe",					0,				'0 1 0', 		'-8 -8 -8',	'8 8 8'},
	{"light_torch_small_walltorch",		0,				'0 .5 0', 		'-10 -10 -20',	'10 10 20'},
	{"light_flame_large_yellow",		0,				'0 1 0', 		'-10 -10 -12',	'12 12 18'},
	{"light_flame_small_yellow",		0,				'0 1 0', 		'-8 -8 -8',	'8 8 8'}, //START_OFF
	{"light_flame_small_white",		0,				'0 1 0', 		'-10 -10 -40',	'10 10 40'}, //START_OFF
	{"misc_fireball",					0,				'0 .5 .8', 		'-8 -8 -8',	'8 8 8'},
	{"misc_explobox",				0,				'0 .5 .8', 		'0 0 0',		'32 32 64'},
	{"misc_explobox2",				0,				'0 .5 .8', 		'0 0 0',		'32 32 64'},
	{"trap_spikeshooter",				0,				'0 .5 .8', 		'-8 -8 -8',	'8 8 8'}, //superspike laser
	{"trap_shooter",				0,				'0 .5 .8', 		'-8 -8 -8',	'8 8 8'}, //superspike laser
	{"air_bubbles",					0,				'0 .5 .8', 		'-8 -8 -8',	'8 8 8'},
	{"viewthing",					0,				'0 .5 .8', 		'-8 -8 -8',	'8 8 8'},
	{"ambient_suck_wind",			0,				'0.3 0.1 0.6', 	'-10 -10 -8',	'10 10 8'},
	{"ambient_drone",				0,				'0.3 0.1 0.6', 	'-10 -10 -8',	'10 10 8'},
	{"ambient_flouro_buzz",			0,				'0.3 0.1 0.6', 	'-10 -10 -8',	'10 10 8'},
	{"ambient_drip",				0,				'0.3 0.1 0.6', 	'-10 -10 -8',	'10 10 8'},
	{"ambient_comp_hum",			0,				'0.3 0.1 0.6', 	'-10 -10 -8',	'10 10 8'},
	{"ambient_thunder",				0,				'0.3 0.1 0.6', 	'-10 -10 -8',	'10 10 8'},
	{"ambient_light_buzz",			0,				'0.3 0.1 0.6', 	'-10 -10 -8',	'10 10 8'},
	{"ambient_swamp1",				0,				'0.3 0.1 0.6', 	'-10 -10 -8',	'10 10 8'},
	{"ambient_swamp2",				0,				'0.3 0.1 0.6', 	'-10 -10 -8',	'10 10 8'},
	{"misc_noisemaker",				0,				'1 0.5 0', 		'-10 -10 -10',	'10 10 10'},
	{"misc_teleporttrain",				0,				'0 .5 .8', 		'-8 -8 -8',	'8 8 8'},
	{"trigger_relay",				0,				'.5 .5 .5', 		'-8 -8 -8', 	'8 8 8'},
	{"info_teleport_destination",		0,				'.5 .5 .5', 		'-8 -8 -8', 	'8 8 32'},

	{"monster_boss",				"progs/boss.mdl",	'1 0 0', 		'-128 -128 -24','128 128 256'},
	{"monster_hell_knight",			"progs/hknight.mdl",	'1 0 0', 		'-16 -16 -24',	'16 16 40'}, //Ambush
	{"monster_demon1",				"progs/demon.mdl",	'1 0 0', 		'-32 -32 -24',	'32 32 64'}, //Ambush
	{"monster_dog",				"progs/dog.mdl",	'1 0 0', 		'-32 -32 -24',	'32 32 40'}, //Ambush
	{"monster_enforcer",				"progs/enforcer.mdl",	'1 0 0', 		'-16 -16 -24',	'16 16 40'}, //Ambush
	{"monster_fish",				"progs/fish.mdl",	'1 0 0', 		'-16 -16 -24',	'16 16 24'}, //Ambush
	{"monster_ogre",				"progs/ogre.mdl",	'1 0 0', 		'-32 -32 -24', 	'32 32 64'}, //Ambush
	{"monster_oldone",				"progs/oldone.mdl",	'1 0 0', 		'-16 -16 -24', 	'16 16 32'},
	{"monster_knight",				"progs/knight.mdl",	'1 0 0', 		'-16 -16 -24',	'16 16 40'}, //Ambush
	{"monster_shalrath",				"progs/shalrath.mdl",	'1 0 0', 		'-32 -32 -24',	'32 32 48'}, //Ambush
	{"monster_shambler",			"progs/shambler.mdl",'1 0 0', 		'-32 -32 -24',	'32 32 64'}, //Ambush
	{"monster_army",				"progs/soldier.mdl",	'1 0 0', 		'-16 -16 -24',	'16 16 40'}, //Ambush
	{"monster_tarbaby",				"progs/tarbaby.mdl",	'1 0 0', 		'-16 -16 -24',	'16 16 24'}, //Ambush
	{"monster_wizard",				"progs/wizard.mdl",	'1 0 0', 		'-16 -16 -24', 	'16 16 40'}, // Ambush
	{"monster_zombie",				"progs/zombie.mdl",	'1 0 0', 		'-16 -16 -24', 	'16 16 32'} //Crucified ambush
};

entedit_t*() editor_ents_new =
{
	local float nent;
	local entedit_t *newents;
	nent = numents;
	numents += 1;

	//extend the list
	newents = memalloc(sizeof(entedit_t)*numents);
	memcpy((__variant*)newents, (__variant*)editents, sizeof(entedit_t)*nent);
	memfree((__variant*)editents);
	editents = newents;

	editents[nent].fields = hash_createtab(12, EV_STRING);

	return &editents[nent];
};

void(float num) editor_ents_delete =
{
};

inline float(string model) modelindexforname =
{
	print("precaching \"");
	print(model);
	print("\"\n");
	precache_model(model);
	setmodel(tempent, model);
	return tempent.modelindex;
};
void(entedit_t *nent) editor_ents_updated =
{
	float ang;

	nent->alpha = stof(nent->fields["alpha"]);
	nent->scale = stof(nent->fields["scale"]);
	nent->modelindex = modelindexforname(nent->fields["model"]);
	nent->mins = tempent.mins;
	nent->maxs = tempent.maxs;
	nent->org = stov(nent->fields["origin"]);
	nent->isbsp = FALSE;

	ang = stof(nent->fields["angle"]);
	if (ang == -1)
		nent->ang = [90, 0, 0];
	else if (ang == -2)
		nent->ang = [-90, 0, 0];
	else
		nent->ang = [0, ang, 0];

	if (!nent->modelindex)
	{
		int i;
		string classn;
		classn = nent->fields["classname"];
		for (i = 0; i < entclasses.length; i++)
		{
			if (classn == entclasses[i].classn)
			{
				nent->modelindex = modelindexforname(entclasses[i].model);
				nent->alpha = 0.3;
				nent->colourmod = entclasses[i].colour;
				nent->mins = entclasses[i].mins;
				nent->maxs = entclasses[i].maxs;
				break;
			}
		}
	}
	else
		nent->isbsp = TRUE;

	if (nent->isbsp)
		nent->ang = '0 0 0';	//bsp entities should not be displayed rotated, because that just messes everything up.
};

void() editor_ents_reload =
{
	local string field, value;

	//reset ent state
	getentitytoken(__NULL__);

	for(;;)
	{
		field = getentitytoken();
		if not (field)
		{
			break;
		}
		if (field == "{")
		{
			local entedit_t *nent;
			nent = editor_ents_new();

			for(;;)
			{
				field = getentitytoken();
				if not (field)
				{
					print("Truncated ent lump\n");
					return;
				}
				if (field == "}")
					break;
				value = getentitytoken();

				nent.fields[field] = value;
				print(sprintf("%s: %s\n", field, value));
			}

			editor_ents_updated(nent);
		}
		else
		{
			print(sprintf("Corrupt ent lump, found \"%s\"\n", field));
			return;
		}
	}
};

void(string shadername, vector min, vector max, vector col) editor_ents_drawbbox =
{
	if (min == max)
		return;
	R_BeginPolygon(shadername);
#define line(x,y) 			R_PolygonVertex(x, '0 0', col, 1);	R_PolygonVertex(y, '0 0', col, 1);	R_EndPolygon()
	line(min, ([max[0], min[1], min[2]]));
	line(min, ([min[0], max[1], min[2]]));
	line(min, ([min[0], min[1], max[2]]));
	line(max, ([min[0], max[1], max[2]]));
	line(max, ([max[0], min[1], max[2]]));
	line(max, ([max[0], max[1], min[2]]));

	line(([max[0], min[1], min[2]]), ([max[0], max[1], min[2]]));
	line(([max[0], min[1], min[2]]), ([max[0], min[1], max[2]]));
	line(([min[0], max[1], min[2]]), ([min[0], max[1], max[2]]));
	line(([min[0], max[1], min[2]]), ([max[0], max[1], min[2]]));
	line(([min[0], min[1], max[2]]), ([min[0], max[1], max[2]]));
	line(([min[0], min[1], max[2]]), ([max[0], min[1], max[2]]));
#undef line
};

void() editor_ents_add =
{
	float e;
	if (!tempent)
	{
		tempent = spawn();
		editor_ents_reload();
	}

	//make sure this shader was generated already.
	shaderforname("entbox",
		"{"
			"cull disable\n"
			"{\n"
				"map $whiteimage\n"
				"blendfunc add\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
			"}\n"
		"}");
	//make sure this shader was generated already.
	shaderforname("entboxsel",
		"{"
			"cull disable\n"
			"{\n"
				"map $whiteimage\n"
				"blendfunc add\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
				"sort nearest\n"
				"nodepthtest\n"
			"}\n"
		"}");

	for (e = 1; e < numents; e++)
	{
		if (e == selectedent)
		{
			if (gettime(0)*5f & 1)
				continue;
			tempent.effects |= 8192f;
			editor_ents_drawbbox("entboxsel", editents[e].org+editents[e].mins, editents[e].org+editents[e].maxs, editents[e].colourmod);
		}
		else
		{
			tempent.effects &~= 8192f;
			editor_ents_drawbbox("entbox", editents[e].org+editents[e].mins, editents[e].org+editents[e].maxs, editents[e].colourmod);
		}

		if (!editents[e].modelindex)
			continue;

		tempent.modelindex = editents[e].modelindex;
		tempent.alpha = editents[e].alpha;
		tempent.scale = editents[e].scale;
		tempent.angles = editents[e].ang;
		tempent.colormod = editents[e].colourmod;

		setorigin(tempent, editents[e].org);
		addentity(tempent);
	}
};

float(float key, float unic, vector mousepos) editor_ents_key =
{
	if (key != K_MOUSE1)
		return FALSE;
	float bestfrac = 1;
	int bestent = selectedent, e;

	vector t = mousefar;
	vector o = mousenear;
	if (vlen(o - t) > 8192)
		t = o + normalize(t)*8192;

	for (e = 1; e < numents; e++)
	{
		if (editents[e].isbsp)
			tempent.solid = SOLID_BSP;
		else
			tempent.solid = SOLID_BBOX;
		tempent.modelindex = editents[e].modelindex;
		tempent.alpha = editents[e].alpha;
		tempent.scale = editents[e].scale;
		tempent.angles = editents[e].ang;
		tempent.colormod = editents[e].colourmod;
		tempent.mins = editents[e].mins;
		tempent.maxs = editents[e].maxs;
		setorigin(tempent, editents[e].org);

		traceline(o, t, 256, tempent);

		if (bestfrac > trace_fraction)
		{
			bestfrac = trace_fraction;
			bestent = e;
		}
	}
	tempent.modelindex = SOLID_NOT;

	selectedent = bestent;
	return TRUE;
};

void(vector mousepos) editor_ents_overlay =
{
	int i;
	vector pos = '0 32 0';
	if (selectedent >= numents)
		return;
	entedit_t *ent = &editents[selectedent];

	for (i = 0; ; i++)
	{
		string key, value;
		key = hash_getkey(ent->fields, i);
		if not (key)
			return;
		value = ent->fields[key];
		drawrawstring(pos, sprintf("%s: %s", key, value), '8 8 0', '1 1 1', 1);
		pos_y += 8;
	}
};
#endif
