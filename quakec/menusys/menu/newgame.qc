static string newgameinfo;
class mitem_newgame : mitem_exmenu
{
	virtual string(string key) get =
	{
		return infoget(newgameinfo, key);
	};
	virtual void(string key, string newval) set =
	{
		string old = newgameinfo;
		newgameinfo = strzone(infoadd(newgameinfo, key, newval));
		if (old)
			strunzone(old);
	};
};

nonstatic void(mitem_desktop desktop) M_NewGame =
{
	mitem_pic banner;
	string gametype = argv(1);
	local float pos;
	mitem_exmenu m;
	if (gametype == "sp")
	{
		//single player has no options. the start map itself gives skill+episode options.
		localcmd("\ndeathmatch 0; coop 0; maxplayers 0; timelimit 0; fraglimit 0; teamplay 0; samelevel 0; startmap_sp\n");
		return;
	}
	if (gametype == "begin")
	{
		cvar_set("hostname", 		infoget(newgameinfo, "hostname"));
		cvar_set("deathmatch", 	infoget(newgameinfo, "deathmatch"));
		cvar_set("coop", 		infoget(newgameinfo, "coop"));
		cvar_set("teamplay", 		infoget(newgameinfo, "teamplay"));
		cvar_set("sv_public", 	infoget(newgameinfo, "sv_public"));
		cvar_set("maxclients", 	infoget(newgameinfo, "maxclients"));
		cvar_set("timelimit", 	infoget(newgameinfo, "timelimit"));
		cvar_set("fraglimit", 	infoget(newgameinfo, "fraglimit"));
		string map = 			infoget(newgameinfo, "map");
		if (map == "")
			map = sprintf("dm%g", floor(random(1, 6)));
		localcmd(sprintf("\nmap \"%s\"\n", map));
		return;
	}

	if (newgameinfo)
		strunzone(newgameinfo);
	newgameinfo = "";
	newgameinfo = infoadd(newgameinfo, "hostname", cvar_string("hostname"));
	newgameinfo = infoadd(newgameinfo, "deathmatch", cvar_string("deathmatch"));
	newgameinfo = infoadd(newgameinfo, "teamplay", cvar_string("teamplay"));
	newgameinfo = infoadd(newgameinfo, "sv_public", cvar_string("sv_public"));
	newgameinfo = infoadd(newgameinfo, "maxclients", cvar_string("maxclients"));
	newgameinfo = infoadd(newgameinfo, "timelimit", cvar_string("timelimit"));
	newgameinfo = infoadd(newgameinfo, "fraglimit", cvar_string("fraglimit"));
	newgameinfo = strzone(newgameinfo);

	m = spawn(mitem_newgame, item_text:_("New Game"), item_flags:IF_SELECTABLE, item_command:"m_main");
	desktop.add(m, RS_X_MIN_PARENT_MIN|RS_Y_MIN_PARENT_MIN | RS_X_MAX_PARENT_MAX|RS_Y_MAX_PARENT_MAX, '0 0', '0 0');
	desktop.item_focuschange(m, IF_KFOCUSED);
	m.totop();

	switch(gametype)
	{
	case "tdm":
	case "dm":
	case "coop":
	case "sp":
		break;
	default:
		//show game type selection
		pos = (16/-2)*(4);
		banner = spawn(mitem_pic, item_text:"gfx/p_option.lmp", item_size_y:24, item_flags:IF_CENTERALIGN);
		m.addm(banner, [(160-banner.item_size_x)*0.5, pos-32], [(160+banner.item_size_x)*0.5, pos-8]);
		m.addm(spawn(mitem_text, item_text:"Single Player", item_command:"m_pop;m_newgame sp", item_scale:16, item_flags:IF_CENTERALIGN), [0, pos], [160, pos+16]);	pos += 16;
		m.addm(spawn(mitem_text, item_text:"Cooperative", item_command:"m_pop;m_newgame coop", item_scale:16, item_flags:IF_CENTERALIGN), [0, pos], [160, pos+16]);	pos += 16;
		m.addm(spawn(mitem_text, item_text:"Deathmatch", item_command:"m_pop;m_newgame dm", item_scale:16, item_flags:IF_CENTERALIGN), [0, pos], [160, pos+16]);	pos += 16;
		m.addm(spawn(mitem_text, item_text:"Team Deathmatch", item_command:"m_pop;m_newgame tdm", item_scale:16, item_flags:IF_CENTERALIGN), [0, pos], [160, pos+16]);	pos += 16;

#if 1//def CSQC
		m.add(spawn (mitem_spinnymodel, item_text: "progs/soldier.mdl",firstframe:73, framecount:8, shootframe:81, shootframes:9), RS_X_MIN_PARENT_MID|RS_Y_MIN_PARENT_MID | RS_X_MAX_PARENT_MID|RS_Y_MAX_PARENT_MID, [-160, 12*-16/2], [0, 12*16/2]);
#else
		//need some art for menuqc
#endif
		return;
	}
	
	pos = (16/-2)*(4);

	banner = spawn(mitem_pic, item_text:"gfx/p_multi.lmp", item_size_y:24, item_flags:IF_CENTERALIGN);
	m.addm(banner, [(160-banner.item_size_x)*0.5, pos-32], [(160+banner.item_size_x)*0.5, pos-8]);

	m.addm(menuitemeditt_spawn(_("Hostname"), 		"hostname", '280 8'), 					[-160, pos], [160, pos+8]); pos += 8;
	m.addm(menuitemcombo_spawn(_("Public"),			"sv_public",				'280 8', _(
																			"-1	\"Reject All (Splitscreen)\" "
																			"0	\"Private (Manual IP Sharing)\" "
																			"1	\"Public (Manual Config)\" "
																			"2	\"Public (Holepunch)\" "
																			)),	[-160, pos], [160, pos+8]); pos += 8;
	m.addm(menuitemcombo_spawn(_("Max Clients"),	"maxclients",				'280 8', _(
																			"2	\"Two\" "
																			"3	\"Three\" "
																			"4	\"Four\" "
																			"8	\"Eight\" "
																			"16	\"Sixteen\" "
																			"32	\"Thirty Two\" "
																			)),	[-160, pos], [160, pos+8]); pos += 8;

	if (gametype == "dm" || gametype == "tdm")
	{
		if (m.get("deathmatch") == "0")
		{
			m.set("deathmatch", "1");
			m.set("coop", "0");
		}
		m.addm(menuitemcombo_spawn(_("Deathmatch Mode"),	"deathmatch",					'280 8', _(
																			"1	\"Weapons Respawn\" "
																			"2	\"Weapons Stay\" "
																			"3	\"Powerups Respawn\" "
																			"4	\"Start With Weapons\" "
																			)),	[-160, pos], [160, pos+8]); pos += 8;
	}
	else
	{
		if (m.get("coop") == "0")
		{
			m.set("deathmatch", "0");
			m.set("coop", "1");
		}
	}
	if (gametype == "tdm")
	{
		if (m.get("teamplay") == "0")
			m.set("teamplay", "1");
	}
	if (gametype == "dm")
	{
		if (m.get("teamplay") != "0")
			m.set("teamplay", "0");
	}
	if (gametype == "coop")
		m.addm(menuitemcheck_spawn(_("No Friendly Fire"),		"teamplay", 			'280 8'),	[-160, pos], [160, pos+8]); pos += 8;
//	if (gametype == "dm" || gametype == "tdm")

	if (gametype == "coop")
		m.set("map", "start");
	else
	{
		m.addm(menuitemcombo_spawn(_("Time Limit"),	"timelimit",					'280 8', _(
																			"0	\"No Limit\" "
																			"5	\"5 minutes\" "
																			"10	\"10 minutes\" "
																			"15	\"15 minutes\" "
																			"20	\"20 minutes\" "
																			"25	\"25 minutes\" "
																			"30	\"30 minutes\" "
																			"35	\"35 minutes\" "
																			"40	\"40 minutes\" "
																			"45	\"45 minutes\" "
																			"50	\"50 minutes\" "
																			"55	\"55 minutes\" "
																			"60	\"1 hour\" "
																			)),	[-160, pos], [160, pos+8]); pos += 8;
		m.addm(menuitemcombo_spawn(_("Frag Limit"),	"fraglimit",					'280 8', _(
																			"0	\"No Limit\" "
																			"10	\"10 frags\" "
																			"20	\"20 frags\" "
																			"30	\"30 frags\" "
																			"40	\"40 frags\" "
																			"50	\"50 frags\" "
																			"60	\"60 frags\" "
																			"70	\"70 frags\" "
																			"80	\"80 frags\" "
																			"90	\"90 frags\" "
																			"100	\"100 frags\" "
																			)),	[-160, pos], [160, pos+8]); pos += 8;

		m.set("map", "");
		m.addm(menuitemcombo_spawn(_("Initial Map"),	"map",					'280 8', _(
																			"dm1		\"DM1 (dm1)\" "
																			"dm2		\"DM2 (dm2)\" "
																			"dm3		\"DM3 (dm3)\" "
																			"dm4		\"DM4 (dm4)\" "
																			"dm5		\"DM5 (dm5)\" "
																			"dm6		\"DM6 (dm6)\" "
																			"start	\"Start (Introduction)\" "
																			"e1m1		\"E1M1 (The Slipgate Complex)\" "
																			"e1m2		\"E1M2 (Castle Of The Damned)\" "
																			"e1m3		\"E1M3 (The Necropolis)\" "
																			"e1m4		\"E1M4 (The Grisly Grotto)\" "
																			"e1m5		\"E1M5 (Gloom Keep)\" "
																			"e1m6		\"E1M6 (The Door To Chthon)\" "
																			"e1m7		\"E1M7 (The House Of Chthon)\" "
																			"e1m8		\"E1M8 (Ziggarat Vertigo)\" "
																			"e2m1		\"E2M1 (The Installation)\" "
																			"e2m2		\"E2M2 (The Ogre Citadel)\" "
																			"e2m3		\"E2M3 (The Crypt Of Decay)\" "
																			"e2m4		\"E2M4 (The Ebon Fortress)\" "
																			"e2m5		\"E2M5 (The Wizard's Manse)\" "
																			"e2m6		\"E2M6 (The Dismal Oubliette\" "
																			"e2m7		\"E2M7 (The Underearth)\" "
																			"e3m1		\"E3M1 (Termination Central)\" "
																			"e3m2		\"E3M2 (The Vaults Of Zin)\" "
																			"e3m3		\"E3M3 (The Tomb Of Terror)\" "
																			"e3m4		\"E3M4 (Satan's Dark Delight)\" "
																			"e3m5		\"E3M5 (The Wind Tunnels)\" "
																			"e3m6		\"E3M6 (Chambers Of Torment)\" "
																			"e3m7		\"E3M7 (Tha Haunted Halls)\" "
																			"e4m1		\"E4M1 (The Sewage System)\" "
																			"e4m2		\"E4M2 (The Tower Of Despair)\" "
																			"e4m3		\"E4M3 (The Elder God Shrine)\" "
																			"e4m4		\"E4M4 (The Palace Of Hate)\" "
																			"e4m5		\"E4M5 (Hell's Atrium)\" "
																			"e4m6		\"E4M6 (The Pain Maze)\" "
																			"e4m7		\"E4M7 (Azure Agony)\" "
																			"e4m8		\"E4M8 (The Nameless City)\" "
																			"end		\"End (Shub-Niggurath's Pit)\" "
																			)),	[-160, pos], [160, pos+8]); pos += 8;
	}

	m.addm(spawn(mitem_text, item_text:"BEGIN!", item_command:"m_pop;m_newgame begin", item_scale:16, item_flags:IF_CENTERALIGN), [-160, pos], [160, pos+16]);

	if (gametype == "coop")
	{
		//random art for style
#if 1//def CSQC
		m.add(spawn (mitem_spinnymodel, item_text: "progs/soldier.mdl", firstframe:73, framecount:8, shootframe:81, shootframes:9), RS_X_MIN_PARENT_MID|RS_Y_MIN_PARENT_MID | RS_X_MAX_PARENT_MID|RS_Y_MAX_PARENT_MID, [-160, -240/2], [0, 240/2]);
#else
		//need some art for menuqc
#endif
	}
	else
	{
		//random art for style
#if 1//def CSQC
		m.add(spawn (mitem_spinnymodel, item_text: "progs/player.mdl", firstframe:0, framecount:6, shootframe:119, shootframes:6), RS_X_MIN_PARENT_MID|RS_Y_MIN_PARENT_MID | RS_X_MAX_PARENT_MID|RS_Y_MAX_PARENT_MID, [-160, 12*-16/2], [0, 12*16/2]);
#else
		//need some art for menuqc
#endif
	}

	addmenuback(m);
};
