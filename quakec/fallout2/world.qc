void () monster_dog;
void () monster_zombie;
void () monster_tarbaby;
void () monster_army;
void () monster_ogre;
void () monster_wizard;
void () monster_knight;
void () monster_demon1;
void () monster_enforcer;
void () monster_shalrath;
void () monster_shambler;
void () monster_fish;

void (vector jojo) spawn_ghoul;



float (float int) getperk =
{
	if (self.perk1 == int)
		return TRUE;
	else if (self.perk2 == int)
		return TRUE;
	else
		return FALSE;
};

void(float stnum, float sttype, .float fieldname) clientstat = #232;
void() SetupStats =
{
	//2 is ev_float
	//32 is the first stat we are allowed to use
	clientstat(32, 2, islot1);
	clientstat(33, 2, islot2);
	clientstat(34, 2, islot3);
	clientstat(35, 2, islot4);
	clientstat(36, 2, islot5);
	clientstat(37, 2, islot6);
	clientstat(38, 2, islot7);
	clientstat(39, 2, islot8);
	clientstat(40, 2, islot9);
	clientstat(41, 2, islot10);
	clientstat(42, 2, islot11);
	clientstat(43, 2, islot12);
	clientstat(44, 2, islot13);
	clientstat(45, 2, islot14);
	clientstat(46, 2, islot15);
	clientstat(47, 2, islot16);
	clientstat(48, 2, health);
	clientstat(49, 2, max_health);
	clientstat(50, 2, ammo_shells);
	clientstat(51, 2, ammo_cells);
	clientstat(52, 2, weight);
	clientstat(53, 2, max_weight);
	clientstat(54, 2, position);
	clientstat(55, 2, rage);
	clientstat(56, 2, regen);
	clientstat(57, 2, missionbrief);
	clientstat(58, 2, sneak);
	clientstat(59, 2, perk2);
	clientstat(60, 2, perk1);
	clientstat(61, 2, current_slot);
	clientstat(62, 2, team);
	clientstat(63, 2, class);
	clientstat(64, 2, start_time);
	clientstat(65, 2, location);
	clientstat(66, 2, map_obj);
	clientstat(67, 2, map_ent1);
	clientstat(68, 2, map_ent2);
	clientstat(69, 2, map_ent3);
	clientstat(71, 2, xslot1);
	clientstat(72, 2, xslot2);
	clientstat(73, 2, xslot3);
	clientstat(74, 2, xslot4);
	clientstat(75, 2, xslot5);
	clientstat(76, 2, xslot6);
	clientstat(77, 2, xslot7);
	clientstat(78, 2, xslot8);
	clientstat(79, 2, xslot9);
	clientstat(80, 2, xslot10);
	clientstat(81, 2, xslot11);
	clientstat(82, 2, xslot12);
	clientstat(83, 2, xslot13);
	clientstat(84, 2, xslot14);
	clientstat(85, 2, xslot15);
	clientstat(86, 2, xslot16);
	clientstat(87, 2, xslot17);
	clientstat(88, 2, xslot18);
	clientstat(89, 2, xslot19);
	clientstat(90, 2, xslot20);
	clientstat(91, 2, islot17);
	clientstat(92, 2, islot18);
	clientstat(93, 2, islot19);
	clientstat(94, 2, islot20);
	clientstat(95, 2, islot21);
	clientstat(96, 2, islot22);
	clientstat(97, 2, islot23);
	clientstat(98, 2, islot24);
	clientstat(99, 2, recoil);
	clientstat(100, 2, skill_combat);
	clientstat(101, 2, skill_doctor);
	clientstat(102, 2, skill_sneak);
	clientstat(103, 2, skill_science);
	clientstat(104, 2, shadowtime);
	clientstat(105, 2, announcercache);
	clientstat(106, 2, radiation);
	clientstat(108, 2, safezone);
	clientstat(109, 2, connected);
	clientstat(110, 2, steadyaim);
	clientstat(125, 2, timer);
	clientstat(126, 2, popup);
	clientstat(127, 2, score);


};

//assesses all current players
void() get_difficulty =
{
	local entity te;
	local float cr;

	//challenge rating system
	//
	//0-24 level of difficulty
      //0 = barely equipped players
      //24 = using the best arms and equipment

	challenge_rating = 0;

	te = find (world, classname, "player");
	while (te)
	{

		if (te.ammo_shells > 200)//money
			challenge_rating += 1;
		if (te.ammo_shells > 500)//money
			challenge_rating += 1;
		if (ToIID(te.islot1) >= IID_WP_AK112 || ToIID(te.islot2) >= IID_WP_AK112)//good weapon
			challenge_rating += 1;
		if (ToIID(te.islot3) >= IID_ARM_COMBAT)//good armor
			challenge_rating += 1;

		te = te.chain;
	}

	if (challenge_rating > 24)
		challenge_rating = 24;
};

void() load_monster =
{
	local float x, y, z;

/*
	if (infokey(world, "total") == "one")
		bprint(2, "test - one player\n");

	if (infokey(world, "total") == "two")
		bprint(2, "test - two players\n");

	if (infokey(world, "total") == "three")
		bprint(2, "test - three players\n");

	if (infokey(world, "total") == "four")
		bprint(2, "test - four players\n");*/

/*
	if (infokey(world, "total") == "one" && random()<0.40)
		return;

	if (infokey(world, "total") == "two" && random()<0.30)
		return;

	if (infokey(world, "total") == "three" && random()<0.20)
		return;

	if (infokey(world, "total") == "four" && random()<0.10)
		return;*/


	if (self.waterlevel >= 2)
		return;

	x = random()*100;
	y = random()*100;


	if (world.map_obj == OBJ_DEADTOWN)
	{
		self.zone = 1;
		monster_zombie();
	}
	else if (x <= world.map_ent1_percent)
	{
		self.zone = 1;

			
		monster_army();


	}
	else if (y <= world.map_ent2_percent)
	{
		self.zone = 1;

		if (world.map_ent2 == 1)
			monster_dog();
		if (world.map_ent2 == 2)
			monster_zombie();
		if (world.map_ent2 == 3 || world.map_ent2 == 4)
			monster_army();
		if (world.map_ent2 == 5 || world.map_ent2 == 6)
			monster_ogre();
		if (world.map_ent2 == 7)
			monster_army();
		if (world.map_ent2 == 8)
			monster_demon1();
		if (world.map_ent2 == 9)
			monster_demon1();
		if (world.map_ent2 == 10)
			monster_enforcer();
		if (world.map_ent2 == 11)
			monster_shalrath();
		if (world.map_ent2 == 12)
			monster_shambler();
	}
	else
	{
		self.zone = 1;

		if (random()<0.10)
			monster_dog();
		else if (random()<0.10)
			monster_zombie();
		else if (random()<0.10)
			monster_army();
		else if (random()<0.10)
			monster_ogre();
		else if (random()<0.10)
			monster_army();
		else if (random()<0.10)
			monster_army();


	}
};

//the big one
//randomly generates map information
//objective, monsters, etc
//1=dog, 2=ghoul, 3=soldier, 4=mutant, 5=scrag, 6=knight, 7=deathclaw,8=shalrath

void() map_startup =
{
	local float r, x, t, e1, e2, e3, d;
	local string savemap;

	if (s_map_obj != 0)
		return;

	r = random();

	get_difficulty();
	d = challenge_rating;


	localcmd("serverinfo total zero\n");

	if (infokey(world, "objective") == "return")
		world.map_obj = 0;
	if (infokey(world, "objective") == "hostage")
		world.map_obj = 1;
	if (infokey(world, "objective") == "supplies")
		world.map_obj = 2;
	if (infokey(world, "objective") == "shadow")
		world.map_obj = 3;
	if (infokey(world, "objective") == "deadtown")
		world.map_obj = 4;
	if (infokey(world, "objective") == "destroy")
		world.map_obj = 5;
	if (infokey(world, "objective") == "defuse")
		world.map_obj = 6;

	if (infokey(world, "objective") == "hostage")
		time_left = 600;
	if (infokey(world, "objective") == "supplies")
		time_left = 600;
	if (infokey(world, "objective") == "shadow")
		time_left = 600;
	if (infokey(world, "objective") == "deadtown")
		time_left = 600;
	if (infokey(world, "objective") == "destroy")
		time_left = 600;
	if (infokey(world, "objective") == "defuse")
		time_left = 600;

/*
	if (world.map_obj == 0)
		savemap = "return";
	if (world.map_obj == 1)
		savemap = "hostage";
	if (world.map_obj == 2)
		savemap = "supplies";
	if (world.map_obj == 3)
		savemap = "shadow";
	if (world.map_obj == 4)
		savemap = "deadtown";
	if (world.map_obj == 5)
		savemap = "destroy";
	if (world.map_obj == 6)
		savemap = "defuse";


	localcmd("serverinfo objective ");
	localcmd(savemap);
	localcmd("\n");*/

	x = 100;
	t = floor(30+random()*30);
	x -= t;

	world.map_ent1_percent = t;

	t = floor(15+random()*15);
	x -= t;

	world.map_ent2_percent = t;
	world.map_ent3_percent = x;

if (challenge_rating < 4)
{
	world.map_ent1 = 2 + ceil(random()*6);
	world.map_ent2 = ceil(random()*8);
	world.map_ent3 = ceil(random()*8);
}
else if (challenge_rating < 8)
{
	world.map_ent1 = 2 + ceil(random()*8);
	world.map_ent2 = 4 + ceil(random()*6);
	world.map_ent3 = 6 + ceil(random()*4);
}
else
{
	world.map_ent1 = 4 + ceil(random()*8);
	world.map_ent2 = 6 + ceil(random()*6);
	world.map_ent3 = 8 + ceil(random()*4);
}
	
	if (world.map_obj == OBJ_HOSTAGE)
		world.map_ent2 = 5;
	if (world.map_obj == OBJ_SUPPLY)
		world.map_ent2 = 5;
	if (world.map_obj == OBJ_SHADOW)
		world.map_ent1 = 5;
	if (world.map_obj == OBJ_DEADTOWN)
	{
		world.map_ent1 = 3;
		world.map_ent2 = 3;
		world.map_ent3 = 8;
	}
	if (world.map_obj == OBJ_DESTROY)
		world.map_ent2 = 5;
	if (world.map_obj == OBJ_DEFUSE)
	{
		world.map_ent1 = 7;
		world.map_ent2 = 16;
		world.map_ent3 = 7;
	}
	s_map_obj = world.map_obj;
	s_ent1 = world.map_ent1;
	s_ent2 = world.map_ent2;
	s_ent3 = world.map_ent3;
	s_p1 = world.map_ent1_percent;
	s_p2 = world.map_ent2_percent;
	s_p3 = world.map_ent3_percent;

};


void() InitBodyQue;

void() sci_think;
void() hostage_pain;
void() hostage_think;
void() hostage_die;
void(entity stuff) spawn_zombie;
void() GameControl;
void() RespawnPlayer;



void () create_referees =
{
	newmis = spawn ();
	newmis.owner = self;
	newmis.movetype = MOVETYPE_NONE;
	setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);
	newmis.solid = SOLID_BBOX;
	newmis.velocity = VEC_ORIGIN;
	newmis.touch = SUB_Null;
	setorigin (newmis, '0 0 -300');
	newmis.nextthink = (time + WEAPON_SHOTGUN);
	newmis.think = GameControl;
	newmis.classname = "referee";
};

void() main =
{
	dprint ("main function\n");
	
// these are just commands the the prog compiler to copy these files

	precache_file ("progs.dat");
	precache_file ("gfx.wad");
	precache_file ("quake.rc");
	precache_file ("default.cfg");

	precache_file ("end1.bin");
	precache_file2 ("end2.bin");

	precache_file ("demo1.dem");
	precache_file ("demo2.dem");
	precache_file ("demo3.dem");

//
// these are all of the lumps from the cached.ls files
//
	precache_file ("gfx/palette.lmp");
	precache_file ("gfx/colormap.lmp");

	precache_file2 ("gfx/pop.lmp");

	precache_file ("gfx/complete.lmp");
	precache_file ("gfx/inter.lmp");

	precache_file ("gfx/ranking.lmp");
	precache_file ("gfx/vidmodes.lmp");
	precache_file ("gfx/finale.lmp");
	precache_file ("gfx/conback.lmp");
	precache_file ("gfx/qplaque.lmp");

	precache_file ("gfx/menudot1.lmp");
	precache_file ("gfx/menudot2.lmp");
	precache_file ("gfx/menudot3.lmp");
	precache_file ("gfx/menudot4.lmp");
	precache_file ("gfx/menudot5.lmp");
	precache_file ("gfx/menudot6.lmp");

	precache_file ("gfx/menuplyr.lmp");
	precache_file ("gfx/bigbox.lmp");
	precache_file ("gfx/dim_modm.lmp");
	precache_file ("gfx/dim_drct.lmp");
	precache_file ("gfx/dim_ipx.lmp");
	precache_file ("gfx/dim_tcp.lmp");
	precache_file ("gfx/dim_mult.lmp");
	precache_file ("gfx/mainmenu.lmp");
	
	precache_file ("gfx/box_tl.lmp");
	precache_file ("gfx/box_tm.lmp");
	precache_file ("gfx/box_tr.lmp");
	
	precache_file ("gfx/box_ml.lmp");
	precache_file ("gfx/box_mm.lmp");
	precache_file ("gfx/box_mm2.lmp");
	precache_file ("gfx/box_mr.lmp");
	
	precache_file ("gfx/box_bl.lmp");
	precache_file ("gfx/box_bm.lmp");
	precache_file ("gfx/box_br.lmp");
	
	precache_file ("gfx/sp_menu.lmp");
	precache_file ("gfx/ttl_sgl.lmp");
	precache_file ("gfx/ttl_main.lmp");
	precache_file ("gfx/ttl_cstm.lmp");
	
	precache_file ("gfx/mp_menu.lmp");
	
	precache_file ("gfx/netmen1.lmp");
	precache_file ("gfx/netmen2.lmp");
	precache_file ("gfx/netmen3.lmp");
	precache_file ("gfx/netmen4.lmp");
	precache_file ("gfx/netmen5.lmp");
	
	precache_file ("gfx/sell.lmp");
	
	precache_file ("gfx/help0.lmp");
	precache_file ("gfx/help1.lmp");
	precache_file ("gfx/help2.lmp");
	precache_file ("gfx/help3.lmp");
	precache_file ("gfx/help4.lmp");
	precache_file ("gfx/help5.lmp");

	precache_file ("gfx/pause.lmp");
	precache_file ("gfx/loading.lmp");

	precache_file ("gfx/p_option.lmp");
	precache_file ("gfx/p_load.lmp");
	precache_file ("gfx/p_save.lmp");
	precache_file ("gfx/p_multi.lmp");

// sounds loaded by C code
	precache_sound ("misc/menu1.wav");
	precache_sound ("misc/menu2.wav");
	precache_sound ("misc/menu3.wav");

	precache_sound ("ambience/water1.wav");
	precache_sound ("ambience/wind2.wav");

// shareware
	precache_file ("maps/start.bsp");

	precache_file ("maps/e1m1.bsp");
	precache_file ("maps/e1m2.bsp");
	precache_file ("maps/e1m3.bsp");
	precache_file ("maps/e1m4.bsp");
	precache_file ("maps/e1m5.bsp");
	precache_file ("maps/e1m6.bsp");
	precache_file ("maps/e1m7.bsp");
	precache_file ("maps/e1m8.bsp");

// registered
	precache_file2 ("gfx/pop.lmp");

	precache_file2 ("maps/e2m1.bsp");
	precache_file2 ("maps/e2m2.bsp");
	precache_file2 ("maps/e2m3.bsp");
	precache_file2 ("maps/e2m4.bsp");
	precache_file2 ("maps/e2m5.bsp");
	precache_file2 ("maps/e2m6.bsp");
	precache_file2 ("maps/e2m7.bsp");

	precache_file2 ("maps/e3m1.bsp");
	precache_file2 ("maps/e3m2.bsp");
	precache_file2 ("maps/e3m3.bsp");
	precache_file2 ("maps/e3m4.bsp");
	precache_file2 ("maps/e3m5.bsp");
	precache_file2 ("maps/e3m6.bsp");
	precache_file2 ("maps/e3m7.bsp");

	precache_file2 ("maps/e4m1.bsp");
	precache_file2 ("maps/e4m2.bsp");
	precache_file2 ("maps/e4m3.bsp");
	precache_file2 ("maps/e4m4.bsp");
	precache_file2 ("maps/e4m5.bsp");
	precache_file2 ("maps/e4m6.bsp");
	precache_file2 ("maps/e4m7.bsp");
	precache_file2 ("maps/e4m8.bsp");

	precache_file2 ("maps/end.bsp");

	precache_file2 ("maps/dm1.bsp");
	precache_file2 ("maps/dm2.bsp");
	precache_file2 ("maps/dm3.bsp");
	precache_file2 ("maps/dm4.bsp");
	precache_file2 ("maps/dm5.bsp");
	precache_file2 ("maps/dm6.bsp");
};

entity() SelectSpawnPoint;

void () RespawnPlayer =
{
	local entity spot;
	
	bprint(2, "test\n");
	spot = SelectSpawnPoint ();
	self.origin = spot.origin + '0 0 1';
	self.angles = spot.angles;

	self.deadflag = DEAD_NO;
	self.takedamage = DAMAGE_AIM;
	self.movetype = MOVETYPE_WALK;
	self.solid = SOLID_NOT;
	self.materialize = 200;
	self.ghost = 0;
	self.air_finished = time + 10;
	self.view_ofs = '0 0 22';
	self.health = self.max_health;

	player_run();
};

void () ResetMap =
{
	local entity te;

	te = find(world, classname, "player");
	while (te)
	{
		te.nextthink = time + 1;
		te.think = RespawnPlayer;

		te = find(te, classname, "player");
	}

};


void () GameControl =
{
	local float pcount, pdead, hcount, hdead, bcount;
	local entity te, ze;

	if (world.model == "maps/r_bunker.bsp")
		return;

	if (world.model == "maps/r_wastes1.bsp")
		return;

	if (random()<0.05)
		sound (self, CHAN_AUTO, "ambience/gunfire1.wav", 0.75, ATTN_NONE);
	else if (random()<0.05)
		sound (self, CHAN_AUTO, "ambience/gunfire5.wav", 0.75, ATTN_NONE);

if (coop == 1) // Players vs Monsters
{
	local float combat;

	combat = 0;
	get_difficulty();

	te = find(world, classname, "monster");
	while (te)
	{

		if (te.enemy != world && te.enemy.classname == "player" && te.health > 0)
			combat = 1;

		te = find(te, classname, "monster");
	}

	if (musicplay == 0)
	{
		musicplay = 1;
		musictrack = 1;

		if (world.map_obj == OBJ_DEADTOWN)
			musictrack = 2;

		ze = find (world, classname, "player");
		while (ze != world)
		{
			if (musictrack == 1)
				stuffcmd(ze, "play music/battle\n");
			if (musictrack == 2)
				stuffcmd(ze, "play misc/zombies\n");

			ze = find (ze, classname, "player");
		}       
      
	}
	else if (musicplay < 50)
		musicplay = musicplay + 1;
	else if (musicplay >= 100)
		musicplay = 0;

	te = find(world, classname, "player");
	while (te)
	{

		if (te.health > 0 && te.class > 0)
			pcount = pcount + 1;
		if (te.health <= 0)
			pdead = pdead + 1;

		te = find(te, classname, "player");
	}

	if (pcount == 0 && self.timer > 5)
		localcmd("restart\n");


	if (endgame_timer > 0)
	{
		ze = find (world, classname, "monster");
		while (ze != world)
		{
			remove(ze);
	
	
			ze = find (ze, classname, "monster");
		}
	}

	if (world.map_obj == OBJ_HOSTAGE || world.map_obj == OBJ_DEADTOWN)
	{
		time_left = time_left - 1;

		te = find(world, classname, "rhostage");
		while (te)
		{

			if (te.health > 0 && te.rescued == 0)
				hcount = hcount + 1;
			if (te.health <= 0)
				hdead = hdead + 1;

			te = find(te, classname, "rhostage");
		}

		hostages = hcount;
		dead_hostage = hdead;

		if (pcount == 0 && pdead > 0) //everyone died
		{
			announcer = "all players were slain";
			ze = find (world, classname, "player");
			while (ze != world)
			{
				ze.missionbrief = 3;
				ze.announcercache = 1;


				ze = find (ze, classname, "player");
			}
			if (endgame_timer == 5)
				localcmd("restart\n");

			sound (world, CHAN_BODY, "misc/menu3.wav", 1, ATTN_NONE);
		}
		else if (hdead > rescue + hcount) //hostages killed
		{
			if (endgame_timer == 0)
			{
				ze = find (world, classname, "player");
				while (ze != world)
				{
					ze.missionbrief = 3;
					ze.announcercache = 2;


					ze = find (ze, classname, "player");
				}
			}
			if (endgame_timer == 5)
				localcmd("restart\n");

			endgame_timer = endgame_timer + 1;
		}
		else if (hcount == 0 && hdead <= rescue) //hostages rescued
		{
			if (endgame_timer == 0)
			{
				ze = find (world, classname, "player");
				while (ze != world)
				{
					ze.missionbrief = 3;
					ze.announcercache = 3;


					ze = find (ze, classname, "player");
				}
			}
			if (endgame_timer == 10)
			{
				localcmd("serverinfo objective return\n");
				changelevel ("r_bunker");
			}
		}
		else if (time_left <= 0 && rescue > 0) //time ran out and at least one was rescued
		{
			if (endgame_timer == 0)
			{
				ze = find (world, classname, "player");
				while (ze != world)
				{
					ze.missionbrief = 3;
					ze.announcercache = 4;


					ze = find (ze, classname, "player");
				}
			}
			if (endgame_timer >= 10)
			{
				localcmd("serverinfo objective return\n");
				changelevel ("r_bunker");
			}
		}
		else if (time_left <= 0 && rescue == 0) //time ran out and no rescues were made
		{
			if (endgame_timer == 0)
			{
				ze = find (world, classname, "player");
				while (ze != world)
				{
					ze.missionbrief = 3;
					ze.announcercache = 5;


					ze = find (ze, classname, "player");
				}
			}
			if (endgame_timer >= 10)
				localcmd("restart\n");
		}
		if (pcount == 0 && pdead > 0)
			endgame_timer = endgame_timer + 1;
		else if (hcount == 0 && hdead <= rescue)
			endgame_timer = endgame_timer + 1;
		else if (time_left <= 0)
			endgame_timer = endgame_timer + 1;
	}
	else if (world.map_obj == OBJ_DEFUSE)
	{
		time_left = time_left - 1;

		bcount = 0;

		te = find(world, classname, "bomb");
		while (te)
		{
			if (self.owner.picking != 2)
				bcount = bcount + 1;

			te = find(te, classname, "bomb");
		}

		if (pcount == 0 && pdead > 0) //everyone died
		{
			ze = find (world, classname, "player");
			while (ze != world)
			{
				ze.missionbrief = 3;
				ze.announcercache = 1;


				ze = find (ze, classname, "player");
			}

			if (endgame_timer == 10)
				localcmd("restart\n");

		}
		else if (bcount == 0) //bombs defused
		{
			if (endgame_timer == 0)
			{
				ze = find (world, classname, "player");
				while (ze != world)
				{
					ze.ammo_shells = ze.ammo_shells + 25;
					ze.missionbrief = 3;
					ze.announcercache = 6;

					stuffcmd(ze, "play music/victory\n");
					ze = find (ze, classname, "player");
				}
			}

			if (endgame_timer == 10)
			{
				localcmd("serverinfo objective return\n");
				changelevel ("r_bunker");
			}
		}
		else if (time_left <= 0) //time ran out
		{
			if (endgame_timer == 0)
			{
				ze = find (world, classname, "player");
				while (ze != world)
				{
					ze.missionbrief = 3;
					ze.announcercache = 7;

					ze = find (ze, classname, "player");
				}
			}
			if (endgame_timer == 5)
				localcmd("restart\n");
		}
		if (pcount == 0 && pdead > 0)
			endgame_timer = endgame_timer + 1;
		else if (bcount == 0)
			endgame_timer = endgame_timer + 1;
		else if (self.timer >= 300)
			endgame_timer = endgame_timer + 1;
		else if (time_left <= 0)
			endgame_timer = endgame_timer + 1;
	}
	else if (world.map_obj == OBJ_SUPPLY)
	{
		time_left = time_left - 1;

		if (pcount == 0 && pdead > 0) //everyone died
		{
			ze = find (world, classname, "player");
			while (ze != world)
			{
				ze.missionbrief = 3;
				ze.announcercache = 1;


				ze = find (ze, classname, "player");
			}

			if (endgame_timer == 10)
				localcmd("restart\n");

		}
		else if (time_left <= 0) //time ran out
		{
			if (endgame_timer == 0)
			{
				ze = find (world, classname, "player");
				while (ze != world)
				{
					ze.missionbrief = 3;
					ze.announcercache = 8;


					ze = find (ze, classname, "player");
				}
			}
			if (endgame_timer == 10)
				localcmd("restart\n");

		}
		else if (supplies == 0) //supplies secured
		{
			if (endgame_timer == 0)
			{
				ze = find (world, classname, "player");
				while (ze != world)
				{
					ze.missionbrief = 3;
					ze.announcercache = 9;


					ze = find (ze, classname, "player");
				}
			}
			if (endgame_timer >= 10)
			{
				localcmd("serverinfo objective return\n");
				changelevel ("r_bunker");
			}
			endgame_timer = endgame_timer + 1;
		}
		if (pcount == 0 && pdead > 0)
			endgame_timer = endgame_timer + 1;
		else if (supplies == 0)
			endgame_timer = endgame_timer + 1;
		else if (time_left <= 0)
			endgame_timer = endgame_timer + 1;
	}
	else if (world.map_obj == OBJ_SHADOW)
	{
		time_left = time_left - 1;

		if (pcount == 0 && pdead > 0) //everyone died
		{
			if (endgame_timer == 0)
			{
				ze = find (world, classname, "player");
				while (ze != world)
				{
					ze.missionbrief = 3;
					ze.announcercache = 1;


					ze = find (ze, classname, "player");
				}
			}
			if (endgame_timer == 10)
				localcmd("restart\n");
		}
		else if (commanders == 0) //commanders assassinated
		{
			if (endgame_timer == 0)
			{
				ze = find (world, classname, "player");
				while (ze != world)
				{
					ze.missionbrief = 3;
					ze.announcercache = 10;


					ze = find (ze, classname, "player");
				}
			}
			if (endgame_timer == 10)
			{
				localcmd("serverinfo objective return\n");
				changelevel ("r_bunker");
			}
		}
		else if (time_left <= 0) //time ran out
		{
			if (endgame_timer == 0)
			{
				ze = find (world, classname, "player");
				while (ze != world)
				{
					ze.missionbrief = 3;
					ze.announcercache = 11;


					ze = find (ze, classname, "player");
				}
			}
			if (endgame_timer == 10)
				localcmd("restart\n");
		}
		else if (called_in >= 5) //soldier radioed in
		{
			if (endgame_timer == 0)
			{
				ze = find (world, classname, "player");
				while (ze != world)
				{
					ze.missionbrief = 3;
					ze.announcercache = 12;


					ze = find (ze, classname, "player");
				}
			}
			if (endgame_timer == 10)
				localcmd("restart\n");
		}

		if (pcount == 0 && pdead > 0)
			endgame_timer = endgame_timer + 1;
		else if (called_in >= 5)
			endgame_timer = endgame_timer + 1;
		else if (time_left <= 0)
			endgame_timer = endgame_timer + 1;
	}
	else if (world.map_obj == OBJ_DESTROY)
	{
		time_left = time_left - 1;

		if (pcount == 0 && pdead > 0 && called_in == 0) //everyone died
		{
			if (endgame_timer == 0)
			{
				ze = find (world, classname, "player");
				while (ze != world)
				{
					ze.missionbrief = 3;
					ze.announcercache = 1;


					ze = find (ze, classname, "player");
				}
			}
			if (endgame_timer == 10)
				localcmd("restart\n");

		}
		else if (supplies == 0) //fuel tanks destroyed
		{
			if (endgame_timer == 0)
			{
				ze = find (world, classname, "player");
				while (ze != world)
				{
					ze.missionbrief = 3;
					ze.announcercache = 13;


					ze = find (ze, classname, "player");
				}
			}
			if (endgame_timer == 10)
			{
				localcmd("serverinfo objective return\n");
				changelevel ("r_bunker");
			}
		}
		else if (time_left <= 0) //time ran out
		{
			if (endgame_timer == 0)
			{
				ze = find (world, classname, "player");
				while (ze != world)
				{
					ze.missionbrief = 3;
					ze.announcercache = 14;


					ze = find (ze, classname, "player");
				}
			}
			if (endgame_timer == 10)
			{
				localcmd("serverinfo objective return\n");
				changelevel ("r_bunker");
			}
		}

		if (pcount == 0 && pdead > 0)
			endgame_timer = endgame_timer + 1;
		else if (supplies == 0)
			endgame_timer = endgame_timer + 1;
		else if (time_left <= 0)
			endgame_timer = endgame_timer + 1;
	}
	self.think = GameControl;
	self.nextthink = time + 1;
	return;
}
if (coop == 0) // Players vs Players
{
	t1alive = 0;
	t1dead = 0;
	t2alive = 0;
	t2dead = 0;

	pcount = 0;
	pdead = 0;

	bprint(2, "gc test\n");

	te = find(world, classname, "player");
	while (te)
	{

		if (te.health > 0)
			pcount = pcount + 1;
		if (te.health <= 0)
			pdead = pdead + 1;

		te = find(te, classname, "player");
	}

	if (pcount == 0 || (pcount == 1 && pdead == 0))
	{
		bprint(2, "^4** ^bTHE ROUND WILL NOT BEGIN UNTIL^b **\n");
		bprint(2, "^4** ^bBOTH TEAMS HAVE ENOUGH PLAYERS^b **\n");
		self.think = GameControl;
		self.nextthink = time + 10;
		return;
	}

	te = find(world, classname, "player");
	while (te)
	{
		if (te.team == 1)
		{
			if (te.health > 0)
				t1alive = t1alive + 1;
			if (te.health <= 0)
				t1dead = t1dead + 1;
		}
		if (te.team == 2)
		{
			if (te.health > 0)
				t2alive = t2alive + 1;
			if (te.health <= 0)
				t2dead = t2dead + 1;
		}
		te = find(te, classname, "player");
	}

	if (t1alive > 0 && t2alive == 0) //team 1 has won
	{
		self.think = GameControl;
		self.nextthink = time + 1;

		if (endgame_timer == 0)
			bprint(2, "^4** ^bTHE RANGERS HAVE WON!^b **\n");
		if (endgame_timer == 1)
			bprint(2, "^4** ^bTHIS MAP WILL RESTART^b **\n");
		if (endgame_timer == 5)
			ResetMap();

		winningteam = 1;
		sound (world, CHAN_BODY, "misc/yourturn.wav", 1, ATTN_NONE);
		endgame_timer = endgame_timer + 1;
	}
	if (t2alive > 0 && t1alive == 0) //team 2 has won
	{
		self.think = GameControl;
		self.nextthink = time + 1;

		if (endgame_timer == 0)
			bprint(2, "^4** ^bTHE RAIDERS HAVE WON!^b **\n");
		if (endgame_timer == 1)
			bprint(2, "^4** ^bTHIS MAP WILL RESTART^b **\n");
		if (endgame_timer == 5)
			ResetMap();

		winningteam = 2;
		sound (world, CHAN_BODY, "misc/yourturn.wav", 1, ATTN_NONE);
		endgame_timer = endgame_timer + 1;
	}
	if (t2alive == 0 && t1alive == 0) //team 2 has won
	{
		self.think = GameControl;
		self.nextthink = time + 1;

		if (endgame_timer == 0)
			bprint(2, "^4** ^bBOTH SIDES ARE DEFEATED!^b **\n");
		if (endgame_timer == 1)
			bprint(2, "^4** ^bTHIS MAP WILL RESTART^b **\n");
		if (endgame_timer == 5)
			ResetMap();

		winningteam = 0;
		sound (world, CHAN_BODY, "misc/yourturn.wav", 1, ATTN_NONE);
		endgame_timer = endgame_timer + 1;
	}

	self.think = GameControl;
	self.nextthink = time + 1;
	return;
}
};

void () GameTimer =
{
	local entity te, ze;
	local float switched;
	local float c, z;


	switched = 0;

	te = findradius(self.origin, 25000);
	while (c < 300 && switched == 0)
	{
		if (te.classname == "raider" && te.enemy != world && te.processed == 0 && te.active == 0 && switched == 0)
		{
			te.active = 1;
			te.processed = 1;
			bprint(2, "it is now ");
			bprint(2, te.netname);
			bprint(2, "'s turn.\n");
			te.maxspeed = 300;
			switched = 1;

			ze = findradius(self.origin, 25000);
			z = 0;
			while (z < 300)
			{
				if ((ze.classname == "player" || ze.classname == "raider") && ze != te)
				{
					ze.active = 0;
					ze.maxspeed = 0;
				}
				z = z + 1;
				ze = ze.chain;
			}
		}
		if (te.classname == "player" && te.processed == 0 && te.active == 0 && switched == 0)
		{
			te.active = 1;
			te.processed = 1;
			bprint(2, "it is now ");
			bprint(2, te.netname);
			bprint(2, "'s turn.\n");
			te.maxspeed = 300;
			switched = 1;

			ze = findradius(self.origin, 25000);
			z = 0;
			while (z < 300)
			{
				if ((ze.classname == "player" || ze.classname == "raider") && ze != te)
				{
					ze.active = 0;
					ze.maxspeed = 0;
				}
				z = z + 1;
				ze = ze.chain;
			}
		}
		c = c + 1;
		te = te.chain;
	}

	c = 0;
	if (switched == 0) // no entities left, turn is over
	{
		bprint(2, "round is now over.\n");

		te = findradius(self.origin, 25000);
		while (c < 300)
		{
			if (te.classname == "player" || te.classname == "raider")
			{
				te.active = 0;
				te.processed = 0;
				te.maxspeed = 0;
			}

			c = c + 1;
			te = te.chain;
		}
	}

	self.think = GameTimer;
	self.nextthink = time + 1;
};

void () create_timer =
{
	newmis = spawn ();
	newmis.owner = self;
	newmis.movetype = MOVETYPE_NONE;
	setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);
	newmis.solid = SOLID_BBOX;
	newmis.velocity = VEC_ORIGIN;
	newmis.touch = SUB_Null;
	setorigin (newmis, '0 0 -300');
	newmis.nextthink = (time + 1);
	newmis.think = GameTimer;
	newmis.classname = "referee";
};


void () RecoilControl =
{
	local entity te, ze;
	local float switched;
	local float c, z;


	self.think = RecoilControl;
	self.nextthink = time + 0.01;

	te = find (world, classname, "player");

	while (te)
	{

		te.lastman = te.lastman - 0.01;

		if (te.lastman < 0)
			te.lastman = 0;

	//superstims: heal at an incredible rate!
		if ((te.xregen > 0 && te.health < te.max_health) && random()<0.5)
		{
			te.health = te.health + 1;
			te.xregen = te.xregen - 1;

			if (te.health > te.max_health)
				te.health = te.max_health;
		}

	//superstims: decay if not healing

		if ((te.xregen > 0 && te.health >= te.max_health) && random()<0.05)
		{
			te.xregen = te.xregen - 1;

			if (te.health > te.max_health)
				te.health = te.max_health;
		}

		te.lastman = te.lastman - 0.01;

	//steady aim: for living anatomy, and aimed shots by standing still
		if (te.recoil != 0)
			te.steadyaim = 6;
		else if (te.recoil == 0 && te.steadyaim > 0)
		{
			te.steadyaim = te.steadyaim - 0.05;
			
			if (te.perk1 == 13 || te.perk2 == 13)
				te.steadyaim = te.steadyaim - 0.05;
		}

		if (te.steadyaim > 0 && te.steadyaim <= 0.10)
			te.steadyaim = 0;


			if (te.pain_finished < time && te.missionbrief == 6) // remove pain
				te.missionbrief = 0;
			else if (te.flash < time && te.missionbrief == 5) // remove flash
				te.missionbrief = 0;
			else if (te.currentmenu == "menu_none" && te.missionbrief == 4) // remove shop
				te.missionbrief = 0;
			if (te.velocity == '0 0 0')
			{
				if (te.attack == 0)
				{
					if (te.driftx >= 1)
						te.driftx = te.driftx - 1;
					if (te.driftx <= -1)
						te.driftx = te.driftx + 1;
		
					if (te.drifty >= 1)
						te.drifty = te.drifty - 1;
					if (te.drifty <= -1)
						te.drifty = te.drifty + 1;

				}


					te.recoil = te.recoil - (0.20+(te.skill_combat*0.005));
			}
			else
			{
				if (te.attack == 0)
				{
					if (te.driftx >= 0.5)
						te.driftx = te.driftx - 0.5;
					if (te.driftx <= -0.5)
						te.driftx = te.driftx + 0.5;
	
					if (te.drifty >= 0.5)
						te.drifty = te.drifty - 0.5;
					if (te.drifty <= -0.5)
						te.drifty = te.drifty + 0.5;
				}

				if (te.recoil > 5)
					te.recoil = te.recoil - 0.05;

				if (te.recoil <= 5)
				{
					te.recoil = te.recoil + 0.10;

						if (te.recoil >= 5)
							te.recoil = 5;
				}
			}

			if (te.driftx > 250)
				te.driftx = 250;
			if (te.drifty > 250)
				te.drifty = 250;

	//support fire: going prone gives a -10 recoil bonus with this perk during heavy fire
		if (getperk(1) && self.position == 2 && te.recoil > 3)
			te.recoil = te.recoil - 0.10;


	//prone: going prone gives a -5 recoil bonus

		if (self.position == 2)
			te.recoil = te.recoil - 0.05;

		if (te.recoil <= 0)
		{
			te.recoil = 0;
			te.driftx = 0;
			te.drifty = 0;
		}

		te = find (te, classname, "player");
	}
};

void () recoil_control =
{
	newmis = spawn ();
	newmis.owner = self;
	newmis.movetype = MOVETYPE_NONE;
	setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);
	newmis.solid = SOLID_BBOX;
	newmis.velocity = VEC_ORIGIN;
	newmis.touch = SUB_Null;
	setorigin (newmis, '0 0 -300');
	newmis.nextthink = (time + 1);
	newmis.think = RecoilControl;
	newmis.classname = "recoil";
};

entity  lastspawn;

//=======================
/*QUAKED worldspawn (0 0 0) ?
Only used for the world entity.
Set message to the level name.
Set sounds to the cd track to play.

World Types:
0: medieval
1: metal
2: base
*/
//=======================
void() worldspawn =
{
	local entity te;
	local string x;

	lastspawn = world;
	InitBodyQue ();
	coop = !cvar("deathmatch");
	recoil_control();
	create_referees();
	SetupStats();

// custom map attributes

	cvar_set ("skill", "3");

	world.location = ceil(random()*5);

	total_players = 0;

	te = find (world, classname, "player");
	while (te)
	{
		if (te.classname == "player")
			total_players = total_players + 1;

		te = find(te, classname, "player");
	}

	map_startup();
// the area based ambient sounds MUST be the first precache_sounds

// player precaches     
	W_Precache ();                  // get weapon precaches

// sounds used from C physics code
	precache_sound ("demon/dland2.wav");            // landing thud
	precache_sound ("misc/h2ohit1.wav");            // landing splash

// setup precaches allways needed
	precache_sound ("player/plyrjmp8.wav");         // player jump
	precache_sound ("player/land.wav");                     // player landing
	precache_sound ("player/land2.wav");            // player hurt landing
	precache_sound ("player/drown1.wav");           // drowning pain
	precache_sound ("player/drown2.wav");           // drowning pain
	precache_sound ("player/gasp1.wav");            // gasping for air
	precache_sound ("player/gasp2.wav");            // taking breath
	precache_sound ("player/h2odeath.wav");         // drowning death

	precache_sound ("radio/9_secure.wav");         // drowning death
	precache_sound ("misc/talk.wav");                       // talk
	precache_sound ("player/teledth1.wav");         // telefrag

	precache_sound ("items/inv1.wav");
	precache_sound ("weapons/lock4.wav");           // ammo pick up
	precache_sound ("weapons/pkup.wav");            // weapon up
	precache_sound ("items/damage3.wav");

	precache_sound ("misc/power.wav");                      //lightning for boss

// player gib sounds
	precache_sound ("player/gib.wav");                      // player gib sound
	precache_sound ("player/udeath.wav");           // player gib sound
	precache_sound ("player/tornoff2.wav");         // gib sound

// player pain sounds

	precache_sound ("player/pain1.wav");
	precache_sound ("player/pain2.wav");
	precache_sound ("player/pain3.wav");
	precache_sound ("player/pain4.wav");
	precache_sound ("player/pain5.wav");
	precache_sound ("player/pain6.wav");

	precache_sound ("boss1/sight1.wav");

// ax sounds    
	precache_sound ("weapons/knifehit.wav");                     // ax swoosh
	precache_sound ("weapons/ax1.wav");                     // ax swoosh
	precache_sound ("player/axhit1.wav");           // ax hit meat
	precache_sound ("player/axhit2.wav");           // ax hit world
	precache_sound ("weapons/brawl-1.wav");           // ax hit world
	precache_sound ("weapons/brawl-2.wav");           // ax hit world

	precache_sound ("player/h2ojump.wav");          // player jumping into water
	precache_sound ("player/slimbrn2.wav");         // player enter slime
	precache_sound ("player/inh2o.wav");            // player enter water
	precache_sound ("player/inlava.wav");           // player enter lava
	precache_sound ("misc/outwater.wav");           // leaving water sound

	precache_sound ("player/lburn1.wav");           // lava burn
	precache_sound ("player/lburn2.wav");           // lava burn

	precache_sound ("misc/water1.wav");                     // swimming
	precache_sound ("misc/water2.wav");                     // swimming

// Invulnerability sounds
	precache_sound ("items/protect.wav");
	precache_sound ("items/protect2.wav");
	precache_sound ("items/protect3.wav");

	precache_model ("progs/melee.mdl");
	precache_model ("progs/guy.mdl");
	precache_model ("progs/lay.mdl");
	precache_model ("progs/eyes.mdl");
	precache_model ("progs/h_player.mdl");
	precache_model ("progs/gib1.mdl");
	precache_model ("progs/gib2.mdl");
	precache_model ("progs/gib3.mdl");

	precache_model ("progs/s_bubble.spr");  // drowning bubbles
	precache_model ("progs/s_explod.spr");  // sprite explosion
	precache_model ("progs/ammobox.mdl");
	precache_model ("progs/ammobox2.mdl");
	precache_model ("progs/stop.spr");
	precache_model ("progs/healing.spr");

	precache_model ("progs/v_axe.mdl");
	precache_model ("progs/v_span.mdl");
	precache_model ("progs/v_nail.mdl");
	precache_model ("progs/v_rock.mdl");
	precache_model ("progs/v_nail2.mdl");
	precache_model ("progs/v_rock2.mdl");
	precache_model ("progs/v_fist.mdl");
	precache_model ("progs/v_knife.mdl");
	precache_model ("progs/v_spear.mdl");
	precache_model ("progs/v_usp.mdl");
	precache_model ("progs/v_needler.mdl");
	precache_model ("progs/v_glock.mdl");
	precache_model ("progs/v_fnfal.mdl");
	precache_model ("progs/v_smg.mdl");
	precache_model ("progs/v_rangem.mdl");
	precache_model ("progs/v_pipe.mdl");
	precache_model ("progs/v_bozar.mdl");
	precache_model ("progs/v_lsw.mdl");
	precache_model ("progs/v_double.mdl");
	precache_model ("progs/v_combat.mdl");
	precache_model ("progs/v_mp9.mdl");
	precache_model ("progs/v_g11.mdl");
	precache_model ("progs/v_ak47.mdl");
	precache_model ("progs/v_acr.mdl");
	precache_model ("progs/v_deagle.mdl");
	precache_model ("progs/v_blaster.mdl");
	precache_model ("progs/v_srifle.mdl");
	precache_model ("progs/v_night.mdl");
	precache_model ("progs/v_handgren.mdl");
	precache_model ("progs/v_jackhammer.mdl");
	precache_model ("progs/v_rocket.mdl");
	precache_model ("progs/v_pipe.mdl");
	precache_model ("progs/v_gauss.mdl");
	precache_model ("progs/v_carbine.mdl");
	precache_model ("progs/v_plasma.mdl");
	precache_model ("progs/v_scope1.mdl");
	precache_model ("progs/v_scope2.mdl");
	precache_model ("progs/v_chain.mdl");
	precache_model ("progs/g_spear.mdl");
	precache_model ("progs/g_knife.mdl");
	precache_model ("progs/g_axe.mdl");
	precache_model ("progs/g_wrench.mdl");

	precache_model ("progs/junk1.mdl");
	precache_model ("progs/junk2.mdl");
	precache_model ("progs/chest.mdl");
	precache_model ("progs/hosfem.mdl");
	precache_model ("progs/hosguy.mdl");
	precache_model ("progs/handgren.mdl");
	precache_model ("progs/plasma.mdl");
	precache_model ("progs/ray.mdl");
	precache_model ("progs/sneak.mdl");
	precache_model ("progs/dead.mdl");
	precache_model ("progs/hbar.spr");
	precache_model ("progs/blast.mdl");
	precache_model ("progs/ray.mdl");
	precache_model ("progs/radio.spr");


	precache_model ("progs/w_spear.mdl");
	precache_model ("progs/w_span.mdl");
	precache_model ("progs/w_knife.mdl");
	precache_model ("progs/w_pipe.mdl");
	precache_model ("progs/w_double.mdl");
	precache_model ("progs/w_combat.mdl");
	precache_model ("progs/w_axe.mdl");
	precache_model ("progs/w_glock.mdl");
	precache_model ("progs/w_usp.mdl");
	precache_model ("progs/w_deagle.mdl");
	precache_model ("progs/w_alien.mdl");
	precache_model ("progs/w_mp9.mdl");
	precache_model ("progs/w_mp5.mdl");
	precache_model ("progs/w_jackhammer.mdl");
	precache_model ("progs/w_rangem.mdl");
	precache_model ("progs/w_g11.mdl");
	precache_model ("progs/w_acr.mdl");
	precache_model ("progs/w_ak47.mdl");
	precache_model ("progs/w_night.mdl");
	precache_model ("progs/w_srifle.mdl");
	precache_model ("progs/w_gauss.mdl");
	precache_model ("progs/w_carbine.mdl");
	precache_model ("progs/w_laser.mdl");
	precache_model ("progs/w_rocket.mdl");
	precache_model ("progs/w_plasma.mdl");
	precache_model ("progs/w_chain.mdl");
	precache_model ("progs/w_bozar.mdl");
	precache_model ("progs/money.mdl");

	precache_model ("progs/turrgun.mdl");
	precache_model ("progs/turrbase.mdl");
	precache_model ("progs/miniturr.mdl");
	precache_model ("progs/minibase.mdl");

	precache_model ("progs/grenade2.mdl");
	precache_model ("maps/b_bh10.bsp");
	precache_model ("maps/b_bh25.bsp");
	precache_model ("maps/b_bh100.bsp");
	precache_model ("progs/s_light.spr");

	precache_sound ("misc/basekey.wav");
	precache_sound ("misc/thud.wav");
	precache_sound ("misc/exp1.wav");
	precache_sound ("misc/exp2.wav");
	precache_sound ("misc/exp3.wav");
	precache_sound ("effects/openlock.wav");
	precache_sound ("effects/miss.wav");
	precache_sound ("effects/radio1.wav");
	precache_sound ("effects/radio2.wav");
	precache_sound ("effects/radio3.wav");
	precache_sound ("effects/radio4.wav");
	precache_sound ("effects/blip1.wav");
	precache_sound ("effects/blip2.wav");
	precache_sound ("effects/axe_hit1.wav");
	precache_sound ("effects/axe_hit2.wav");
	precache_sound ("effects/axe_swing.wav");
	precache_sound ("effects/metal-1.wav");
	precache_sound ("effects/metal-2.wav");
	precache_sound ("effects/spear_hit1.wav");
	precache_sound ("effects/spear_hit2.wav");

	precache_sound ("misc/item1.wav");
	precache_sound ("misc/item2.wav");
	precache_sound ("items/r_item1.wav");
	precache_sound ("items/r_item2.wav");
	precache_sound ("items/locked.wav");
	precache_sound ("items/lockpick1.wav");
	precache_sound ("items/lockpick2.wav");


	precache_sound ("radio/1_yes.wav");
	precache_sound ("radio/2_no.wav");
	precache_sound ("radio/3_letsgo.wav");
	precache_sound ("radio/4_cover.wav");
	precache_sound ("radio/5_recon.wav");
	precache_sound ("radio/6_backup.wav");
	precache_sound ("radio/7_hold.wav");
	precache_sound ("radio/8_behind.wav");
	precache_sound ("radio/9_secure.wav");
	precache_sound ("radio/10_suppressing.wav");
	precache_sound ("radio/11_silence.wav");
	precache_sound ("radio/medic.wav");
	precache_sound ("radio/grenade.wav");

	precache_sound ("ambience/gunfire1.wav");
	precache_sound ("ambience/gunfire5.wav");

	precache_sound ("weapons/smoke.wav");
	precache_sound ("weapons/energy2.wav");


	precache_sound ("misc/hosdie1.wav");
	precache_sound ("misc/hosdie2.wav");
	precache_sound ("misc/hosdown.wav");
	precache_sound ("misc/rescued.wav");
	precache_sound ("misc/beep1.wav");
	precache_sound ("misc/r_tele4.wav");
	precache_sound ("buttons/switch02.wav");
	precache_sound ("buttons/switch04.wav");
	precache_sound ("misc/secret.wav");
	precache_sound ("misc/fanfare.wav");

	precache_sound ("player/gib1.wav");
	precache_sound ("player/step1.wav");
	precache_sound ("player/step2.wav");
	precache_sound ("player/step3.wav");
	precache_sound ("player/step4.wav");
	precache_sound ("player/step1a.wav");
	precache_sound ("player/step2a.wav");
	precache_sound ("player/step3a.wav");
	precache_sound ("player/step4a.wav");
	precache_sound ("dog/dattack1.wav");
	precache_sound ("dog/ddeath.wav");
	precache_sound ("dog/dpain1.wav");
	precache_sound ("dog/dsight.wav");
	precache_sound ("dog/idle.wav");
	precache_sound ("ogre/ak74.wav");
	precache_sound ("ogre/ak47.wav");
	precache_sound ("weapons/usp.wav");
	precache_sound ("weapons/glock.wav");
	precache_sound ("weapons/auto2.wav");
	precache_sound ("weapons/ak112.wav");
	precache_sound ("weapons/amr.wav");
	precache_sound ("weapons/blowoff-1.wav");
	precache_sound ("weapons/blowoff-2.wav");
	precache_sound ("misc/bounce_1.wav");
	precache_sound ("misc/bounce_2.wav");
	precache_sound ("misc/bounce_3.wav");
	precache_sound ("weapons/burst.wav");
	precache_sound ("weapons/click.wav");
	precache_sound ("weapons/deagle.wav");
	precache_sound ("weapons/dks-1.wav");
	precache_sound ("weapons/fnfal.wav");
	precache_sound ("weapons/g11.wav");
	precache_sound ("weapons/gauss.wav");
	precache_sound ("weapons/gpull.wav");
	precache_sound ("weapons/citykiller.wav");
	precache_sound ("weapons/laser.wav");
	precache_sound ("weapons/laserburst1.wav");
	precache_sound ("weapons/plasma-1.wav");
	precache_sound ("weapons/plasma-2.wav");
	precache_sound ("weapons/moonlight-1.wav");
	precache_sound ("weapons/moonlight-2.wav");
	precache_sound ("weapons/needler.wav");
	precache_sound ("weapons/bozar.wav");
	precache_sound ("weapons/lsw.wav");
	precache_sound ("weapons/amr.wav");
	precache_sound ("weapons/smg_s.wav");
	precache_sound ("weapons/shotgun1.wav");
	precache_sound ("weapons/mp5.wav");
	precache_sound ("weapons/mp7.wav");
	precache_sound ("weapons/rangem.wav");
	precache_sound ("weapons/ric1.wav");
	precache_sound ("weapons/ric2.wav");
	precache_sound ("weapons/ric3.wav");
	precache_sound ("weapons/ric4.wav");
	precache_sound ("weapons/ric5.wav");
	precache_sound ("weapons/tink1.wav");
	precache_sound ("weapons/shell.wav");
	precache_sound ("weapons/reload.wav");
	precache_sound ("weapons/silenced.wav");
	precache_sound ("misc/greload.wav");
	precache_sound ("misc/build1.wav");
	precache_sound ("misc/build2.wav");
	precache_sound ("misc/build3.wav");
	precache_sound ("player/agdie1.wav");
	precache_sound ("player/agdie2.wav");
	precache_sound ("player/agdie3.wav");
	precache_sound ("player/agdie4.wav");
	precache_sound ("player/agdie5.wav");
	precache_sound ("player/berserk.wav");
	precache_sound ("player/headshot.wav");
	precache_sound ("player/hit1.wav");

	precache_sound ("player/paina.wav");
	precache_sound ("player/painb.wav");
	precache_sound ("player/yourturn.wav");
	precache_model ("progs/bolt.mdl");              // for lightning gun
	precache_model ("progs/bolt2.mdl");             // for lightning gun
	precache_model ("progs/bolt3.mdl");             // for boss shock
	precache_model ("progs/lavaball.mdl");  // for testing


	precache_model ("progs/bomb.mdl");
	precache_model ("progs/missile.mdl");
	precache_model ("progs/grenade.mdl");
	precache_model ("progs/spike.mdl");
	precache_model ("progs/s_spike.mdl");

	precache_model ("progs/backpack.mdl");

	precache_model ("progs/zom_gib.mdl");

	precache_model ("progs/v_light.mdl");

	precache_model ("maps/crate.bsp");

//
// Setup light animation tables. 'a' is total darkness, 'z' is maxbright.
//

	// 0 normal
	lightstyle(0, "m");
	
	// 1 FLICKER (first variety)
	lightstyle(1, "mmnmmommommnonmmonqnmmo");
	
	// 2 SLOW STRONG PULSE
	lightstyle(2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");
	
	// 3 CANDLE (first variety)
	lightstyle(3, "mmmmmaaaaammmmmaaaaaabcdefgabcdefg");
	
	// 4 FAST STROBE
	lightstyle(4, "mamamamamama");
	
	// 5 GENTLE PULSE 1
	lightstyle(5,"jklmnopqrstuvwxyzyxwvutsrqponmlkj");
	
	// 6 FLICKER (second variety)
	lightstyle(6, "nmonqnmomnmomomno");
	
	// 7 CANDLE (second variety)
	lightstyle(7, "mmmaaaabcdefgmmmmaaaammmaamm");
	
	// 8 CANDLE (third variety)
	lightstyle(8, "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa");
	
	// 9 SLOW STROBE (fourth variety)
	lightstyle(9, "aaaaaaaazzzzzzzz");
	
	// 10 FLUORESCENT FLICKER
	lightstyle(10, "mmamammmmammamamaaamammma");

	// 11 SLOW PULSE NOT FADE TO BLACK
	lightstyle(11, "abcdefghijklmnopqrrqponmlkjihgfedcba");
	
	// styles 32-62 are assigned by the light program for switchable lights

	// 63 testing
	lightstyle(63, "a");

	if (infokey(world, "coop") == "0")
		coop = 0;
	if (infokey(world, "coop") == "1")
		coop = 1;
};

void() StartFrame =
{
	timelimit = cvar("timelimit") * 60;
	fraglimit = cvar("fraglimit");
	teamplay = cvar("teamplay");
	deathmatch = !coop;
	
	framecount = framecount + 1;
};

/*
==============================================================================

BODY QUE

==============================================================================
*/

entity  bodyque_head;

void() bodyque =
{       // just here so spawn functions don't complain after the world
	// creates bodyques
};

void() InitBodyQue =
{
	bodyque_head = spawn();
	bodyque_head.classname = "bodyque";
	bodyque_head.owner = spawn();
	bodyque_head.owner.classname = "bodyque";
	bodyque_head.owner.owner = spawn();
	bodyque_head.owner.owner.classname = "bodyque";
	bodyque_head.owner.owner.owner = spawn();
	bodyque_head.owner.owner.owner.classname = "bodyque";
	bodyque_head.owner.owner.owner.owner = bodyque_head;
};


// make a body que entry for the given ent so the ent can be
// respawned elsewhere
void(entity ent) CopyToBodyQue =
{
	bodyque_head.angles = ent.angles;
	bodyque_head.model = ent.model;
	bodyque_head.modelindex = ent.modelindex;
	bodyque_head.frame = ent.frame;
	bodyque_head.colormap = ent.colormap;
	bodyque_head.movetype = ent.movetype;
	bodyque_head.velocity = ent.velocity;
	bodyque_head.flags = 0;
	setorigin (bodyque_head, ent.origin);
	setsize (bodyque_head, ent.mins, ent.maxs);
	bodyque_head = bodyque_head.owner;
};


