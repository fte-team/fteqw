
void() InitBodyQue;

void() sci_think;
void() hostage_pain;
void() hostage_think;
void() hostage_die;
void(entity stuff) spawn_zombie;
void() GameControl;
void() VoteBoy;

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
	newmis = spawn ();
	newmis.owner = self;
	newmis.movetype = MOVETYPE_NONE;
	setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);
	newmis.solid = SOLID_BBOX;
	newmis.velocity = VEC_ORIGIN;
	newmis.touch = SUB_Null;
	setorigin (newmis, '0 0 -300');
	newmis.nextthink = (time + MULTICAST_PVS_R);
	newmis.think = VoteBoy;
	newmis.classname = "voteboy";
	newmis.vote_time = (time + MULTICAST_PVS_R);
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

void () ResetHostages =
{
	local entity te;
	local entity ze;
	local entity ge;


	te = find (world, classname, "hostage");
	while (te)
	{
		te.cnt = MULTICAST_ALL;
		te.rescued = MULTICAST_ALL;
		te.deathtype = "";
		setorigin (te, te.home);
		traceline (te.origin, (te.origin - (v_up * IT_NAILS)), FALSE, te);
		setorigin (te, (trace_endpos + '0 0 24'));
		te.movetype = MOVETYPE_STEP;
		te.velocity = VEC_ORIGIN;
		te.touch = SUB_Null;
		te.classname = "hostage";
		te.takedamage = DAMAGE_AIM;
		setsize (te, VEC_HULL_MIN, VEC_HULL_MAX);
		te.health = 70;
		te.think = hostage_think;
		te.nextthink = (time + WEAPON_SHOTGUN);
		te.solid = SOLID_BBOX;
		te.th_die = hostage_die;
		te.th_pain = hostage_pain;
		te.angles_y = floor ((random () * 360));
		te.netname = "citizen";
		te.angles_z = MULTICAST_ALL;
		te.angles_x = MULTICAST_ALL;
		te = find (te, classname, "hostage");
	}
	te = find (world, classname, "scientist");
	while (te)
	{
		te.frame = 1;
		te.movetype = MOVETYPE_STEP;
		te.velocity = VEC_ORIGIN;
		te.touch = SUB_Null;
		te.classname = "scientist";
		te.takedamage = DAMAGE_AIM;
		setsize (te, VEC_HULL_MIN, VEC_HULL_MAX);
		setmodel (te, "progs/hosguy.mdl");
		te.health = 70;
		te.think = hostage_think;
		te.nextthink = (time + 3);
		te.team = 3;
		te.solid = SOLID_BBOX;
		te.th_die = hostage_die;
		te.th_pain = hostage_pain;
		te.rescued = MULTICAST_ALL;
		te.think = sci_think;
		te.nextthink = (time + 1);
	
		te = find (te, classname, "scientist");
	}
};

void () ResetGhouls =
{
	local entity te;
	local entity ze;

	ze = find (world, classname, "ighoul");
	while (ze)
	{
		remove (ze);
		ze = find (ze, classname, "ighoul");
	}
	ze = find (world, classname, "ghoul");
	while (ze)
	{
		spawn_zombie (self);
		ze = find (ze, classname, "ghoul");
	}
};

void (float won) ReplacePlayers =
{
	local entity te;
	local entity spot;
	local float qq;

	round_end = 1.9;
	te = find (world, classname, "camera");
	while (te)
	{
		setmodel (te, "progs/camera.mdl");
		te = find (te, classname, "camera");
	}
	te = find (world, classname, "player");
	while (te)
	{
		if ((te.team == WEAPON_SHOTGUN))
		{
			qq = team1win;
		}
		if ((te.team == WEAPON_ROCKET))
		{
			qq = team2win;
		}
		if ((te.team == won))
		{
			te.ammo_shells = ((te.ammo_shells + TE_LIGHTNING3) + (qq / 1.25));
		}
		else
		{
			te.ammo_shells = ((te.ammo_shells + TE_LIGHTNING2) + (qq / WEAPON_ROCKET));
		}
		te.nextthink = time;
		te.think = PutClientInServer;
		te.maxspeed = 0;
		te = find (te, classname, "player");
	}
	gtimer = 180;
};

void () ObjectSweep =
{
	local entity te;

	te = find (world, classname, "doggy");
	while (te)
	{
		remove (te);
		te = find (te, classname, "doggy");
	}
	te = find (world, classname, "bot");
	while (te)
	{
		remove (te);
		te = find (te, classname, "bot");
	}
	te = find (world, classname, "dropped_weapon");
	while (te)
	{
		remove (te);
		te = find (te, classname, "dropped_weapon");
	}
	te = find (world, classname, "c4");
	while (te)
	{
		remove (te);
		te = find (te, classname, "c4");
	}
	te = find (world, classname, "active_c4");
	while (te)
	{
		remove (te);
		te = find (te, classname, "active_c4");
	}
};

void () ResetTriggers =
{
	local entity te;

	te = find (world, classname, "trigger_once");
	while (te)
	{
		te.wait = 0.5;
		te = find (te, classname, "trigger_once");
	}
	te = find (world, classname, "func_button");
	while (te)
	{
		te.wait = 0.5;
		te = find (te, classname, "func_button");
	}
};

void () RoundReset =
{
	round_end = WEAPON_ROCKET;
	red_kill = MULTICAST_ALL;
	blue_kill = MULTICAST_ALL;
        teamthatwon = 0;
	team_blue = MULTICAST_ALL;
	team_red = MULTICAST_ALL;
	bombed = MULTICAST_ALL;
	ResetTriggers ();
	ResetGhouls ();
	ResetHostages ();
	ObjectSweep ();
	ReplacePlayers (0);
};

void () CountGhouls =
{
	local entity te;

	zombies = 0;
	te = find (world, classname, "ighoul");
	while (te)
	{
		if ((te.health > 0))
		{
			zombies = (zombies + 1);
		}
		te = find (te, classname, "ighoul");
	}
};

void () CountScientists =
{
	local entity te;

	hostages = MULTICAST_ALL;
	te = find (world, classname, "scientist");
	while (te)
	{
		if ((te.health > MULTICAST_ALL))
		{
			hostages = (hostages + WEAPON_SHOTGUN);
		}
		te = find (te, classname, "scientist");
	}
};

void () RemoveDeadGhouls =
{
	local entity te;

	te = find (world, classname, "ighoul");
	while (te)
	{
		if ((te.health < MULTICAST_ALL))
		{
			te.nextthink = (time + WEAPON_SHOTGUN);
			te.think = SUB_Remove;
		}
		te = find (te, classname, "ighoul");
	}
};

void () SpawnRandomGhouls =
{
	local entity te;
	local float playercount;
	local float zomcount;

	te = find (world, classname, "info");

	CountGhouls ();
	if (te.attack == 1)
	{
		te = find (world, classname, "player");
		while (te)
		{
			if (((te.ghost == 0) && (te.health > 0)))
			{
				playercount = (playercount + 1);
			}
			te = find (te, classname, "player");
		}
		zomcount = (15 * playercount);
		if ((zomcount > 55))
		{
			zomcount = 55;
		}
	}
	if (te.attack == 2)
	{
		te = find (world, classname, "player");
		while (te)
		{
			if (((te.ghost == 0) && (te.health > 0)))
			{
				playercount = (playercount + 1);
			}
			te = find (te, classname, "player");
		}
		zomcount = (30 * playercount);
		if ((zomcount > 200))
		{
			zomcount = 200;
		}
	}
	te = find (world, classname, "ghoul");
	while (te)
	{
		if ((zombies < zomcount))
		{
			spawn_zombie (te);
			zombies = (zombies + 1);
		}
		te = find (te, classname, "ghoul");
	}
};

void () DisplayGhoulsLeft =
{
	local string ghoulsleft;

	bprint (WEAPON_ROCKET, "rangers: ");
	ghoulsleft = ftos (blue_kill);
	bprint (WEAPON_ROCKET, ghoulsleft);
	bprint (WEAPON_ROCKET, "   raiders: ");
	ghoulsleft = ftos (red_kill);
	bprint (WEAPON_ROCKET, ghoulsleft);
	bprint (WEAPON_ROCKET, "   ");
	bprint (WEAPON_ROCKET, "left: ");
	ghoulsleft = ftos (zombies);
	bprint (WEAPON_ROCKET, ghoulsleft);
	bprint (WEAPON_ROCKET, "\n");
};

void () CountHostages =
{
	local entity te;

	hostages = MULTICAST_ALL;
	dead_hostage = MULTICAST_ALL;
	te = find (world, classname, "hostage");
	while (te)
	{
		if (((te.health > MULTICAST_ALL) && (te.rescued == MULTICAST_ALL)))
		{
			hostages = (hostages + WEAPON_SHOTGUN);
		}
		if ((te.health < MULTICAST_ALL))
		{
			dead_hostage = (dead_hostage + WEAPON_SHOTGUN);
		}
		te = find (te, classname, "hostage");
	}
};

void () CheckZombie1 =
{
	CountGhouls ();
	if ((msgcount == MULTICAST_ALL))
	{
		DisplayGhoulsLeft ();
		RemoveDeadGhouls ();
		SpawnRandomGhouls ();
		msgcount = TE_LAVASPLASH;
	}
	CountHostages ();
	msgcount = (msgcount - WEAPON_SHOTGUN);
	if ((dead_hostage >= WEAPON_ROCKET))
	{
		max_zombies = 400;
		sound (self, CHAN_BODY, "misc/zombies.wav", WEAPON_SHOTGUN, ATTN_NONE);
		bprint (PRINT_MEDIUM, "***       THE ZOMBIES WIN!        ***\n");
		bprint (PRINT_MEDIUM, "***   HOSTAGES HAVE BEEN KILLED   ***\n");
		RoundReset ();
		return;
	}
	if ((hostages == MULTICAST_ALL))
	{
		max_zombies = 400;
		sound (self, CHAN_BODY, "misc/zombies.wav", WEAPON_SHOTGUN, ATTN_NONE);
		bprint (PRINT_MEDIUM, "***       THE PLAYERS WIN!        ***\n");
		bprint (PRINT_MEDIUM, "***   HOSTAGES HAVE BEEN SAVED!   ***\n");
		team1win = (team1win + WEAPON_SHOTGUN);
		RoundReset ();
		return;
	}
	if ((gtimer <= WEAPON_SHOTGUN))
	{
		max_zombies = 400;
		sound (self, CHAN_BODY, "misc/zombies.wav", WEAPON_SHOTGUN, ATTN_NONE);
		bprint (PRINT_MEDIUM, "***       THE ZOMBIES WIN!        ***\n");
		bprint (PRINT_MEDIUM, "*** HOSTAGES HAVE NOT BEEN SAVED! ***\n");
		RoundReset ();
		return;
	}
};

void () CheckZombie2 =
{
	local entity te;
	local float team1, team2;

	CountGhouls ();
	if ((msgcount == 0))
	{
		DisplayGhoulsLeft ();
		RemoveDeadGhouls ();
		SpawnRandomGhouls ();
		msgcount = TE_LAVASPLASH;
	}
	msgcount = (msgcount - WEAPON_SHOTGUN);
	CountScientists ();
	if ((hostages <= 2))
	{
		max_zombies = 400;
		bprint (PRINT_MEDIUM, ">>>       THE ZOMBIES WIN!        <<<\n");
		bprint (PRINT_MEDIUM, ">>>  A SCIENTIST HAS BEEN KILLED  <<<\n");
		RoundReset ();
		return;
	}
	if ((zombies <= 0))
	{
		max_zombies = 400;
		bprint (PRINT_MEDIUM, ">>>       THE PLAYERS WIN!        <<<\n");
		bprint (PRINT_MEDIUM, ">>> ZOMBIES HAVE BEEN SLAUGHTERED <<<\n");
		team1win = (team1win + 1);
		RoundReset ();
		return;
	}
	if ((gtimer <= 1))
	{
		max_zombies = 400;
		bprint (PRINT_MEDIUM, "***       THE PLAYERS WIN!        ***\n");
		bprint (PRINT_MEDIUM, "***  SCIENTISTS WERE PROTECTED!!  ***\n");
		team1win = (team1win + 1);
		RoundReset ();
		return;
	}

	te = find (world, classname, "player");
	while (te)
	{
		if (te.classname == "player" && te.team == 1 && te.ghost == 0)
			team1 = team1 + 1;

		te = find (te, classname, "player");
	}

	if (team1 == 0 && team_blue != 0)
	{
		max_zombies = 400;
		bprint (PRINT_MEDIUM, "***       THE PLAYERS WIN!        ***\n");
		bprint (PRINT_MEDIUM, "***  YOUR TEAM WAS SLAUGHTERED!!  ***\n");
		RoundReset ();
		return;
	}
};

void () CheckHostage =
{
	local entity te;

	hostages = 0;
	dead_hostage = 0;
	te = find (world, classname, "hostage");
	while (te)
	{
		if (((te.health > 0) && (te.rescued == 0)))
		{
			hostages = (hostages + WEAPON_SHOTGUN);
		}
		if ((te.health < 0))
		{
			dead_hostage = (dead_hostage + 1);
		}
		te = find (te, classname, "hostage");
	}
	if (((dead_hostage >= 2) && (blue_kill >= 2)))
	{
		bprint (PRINT_MEDIUM, ">>> RANGERS KILLED HOSTAGES <<<\n");
		bprint (PRINT_MEDIUM, ">>>      RANGERS LOSE!!     <<<\n");
		team2win = (team2win + WEAPON_SHOTGUN);
		RoundReset ();
		return;
	}
	if (((dead_hostage >= 2) && (red_kill >= 2)))
	{
		bprint (PRINT_MEDIUM, ">>> RAIDERS KILLED HOSTAGES <<<\n");
		bprint (PRINT_MEDIUM, ">>>      RAIDERS LOSE!!     <<<\n");
		team1win = (team1win + WEAPON_SHOTGUN);
		RoundReset ();
		return;
	}
	if ((hostages == 0))
	{
		bprint (PRINT_MEDIUM, ">>> THE HOSTAGES WERE SAVED <<<\n");
		bprint (PRINT_MEDIUM, ">>> RANGERS WIN THE ROUND!! <<<\n");
		team1win = (team1win + WEAPON_SHOTGUN);
		RoundReset ();
		return;
	}
};

void () CheckDefuse =
{
	if ((bombed == 1))
	{
		bprint (PRINT_MEDIUM, ">>> RAIDERS WIN THE ROUND! <<<\n");
		team2win = (team2win + WEAPON_SHOTGUN);
		RoundReset ();
		return;
	}
	if ((bombed == 2))
	{
		bprint (PRINT_MEDIUM, ">>> RANGERS WIN THE ROUND! <<<\n");
		team1win = (team1win + WEAPON_SHOTGUN);
		RoundReset ();
		return;
	}
};

void () GameControl =
{
	local float r;
	local float qq;
	local float qqq;
	local entity de;
	local entity te;
	local float team1;
	local float team2;
	local float team_won;
	local float gcnt;
	local float red;
	local float blue;
	local float botx, x, y;
	local entity iinfo;
	local entity z;
	local entity boss2;
	local entity ke;
	local entity spot;
	local float boss;
	local string vv;
	local string hos1;
	local string hos2;
	local string zz;
	local string ghoulsleft;
	local float randy;
	local float end;

	if (world.model == "maps/sv_town.bsp")
		respawnvar = 1;

	gtimer = (gtimer - WEAPON_SHOTGUN);
	self.nextthink = (time + WEAPON_SHOTGUN);
	self.think = GameControl;
	red = MULTICAST_ALL;
	blue = MULTICAST_ALL;
	team1 = MULTICAST_ALL;
	team2 = MULTICAST_ALL;


	if (((random () * IDLE8A) < WEAPON_SHOTGUN))
	{

		randy = (random () * TE_WIZSPIKE);
		if ((randy < WEAPON_SHOTGUN))
		{
			sound (self, CHAN_AUTO, "ambience/gunfire1.wav", 0.75, ATTN_NONE);
		}
		else
		{
			if ((randy < WEAPON_ROCKET))
			{
				sound (self, CHAN_AUTO, "ambience/gunfire2.wav", 0.75, ATTN_NONE);
			}
			else
			{
				if ((randy < AS_MELEE))
				{
					sound (self, CHAN_AUTO, "ambience/gunfire3.wav", 0.75, ATTN_NONE);
				}
				else
				{
					if ((randy < WEAPON_SPIKES))
					{
						sound (self, CHAN_AUTO, "ambience/gunfire4.wav", 0.75, ATTN_NONE);
					}
					else
					{
						if ((randy < MULTICAST_PVS_R))
						{
							sound (self, CHAN_AUTO, "ambience/gunfire5.wav", 0.75, ATTN_NONE);
						}
						else
						{
							if ((randy < TE_LIGHTNING2))
							{
								sound (self, CHAN_AUTO, "ambience/gunfire6.wav", 0.75, ATTN_NONE);
							}
							else
							{
								sound (self, CHAN_AUTO, "ambience/gunfire7.wav", 0.75, ATTN_NONE);
							}
						}
					}
				}
			}
		}
	}

	if (coop == 1)
	{
		x = 0;
		y = 0;
		te = find (world, classname, "player");

		while (te)
		{
			if (te.health > 0 && te.ghost == 0)
				x = x + 1;

			if (te.health < 0)
				y = y + 1;

			te = find (te, classname, "player");
		}

		if (x == 0 && y > 0)
		{
			bprint (PRINT_MEDIUM, "*** PLAYERS SLAIN ***\n*** ROUND   RESET ***");
			localcmd("restart\n");
			return;
		}

		return;
	}

	te = find (world, classname, "ighoul");
/*
	while (te)
	{
		if (te.enemy)
			te.enemy = world;

		te = find (te, classname, "ighoul");
	}*/

	te = find (world, classname, "info");
	if ((te.attack == 1))
	{
		CheckZombie1 ();
	}
	else
	{
		if ((te.attack == 2))
		{
			CheckZombie2 ();
		}
		else
		{
			if ((te.hostage == 1))
			{
				CheckHostage ();
			}
			else
			{
				if ((bombed >= 1))
				{
					CheckDefuse ();
				}
			}
		}
	}
	if (round_end == 2)
	{
		ReplacePlayers (0);
		round_end = 1.9;
		teamthatwon = MULTICAST_ALL;
		team_blue = MULTICAST_ALL;
		team_red = MULTICAST_ALL;
		return;
	}
	if (((round_end <= 1.9) && (round_end >= 1.1)))
	{
		round_end = (round_end - 0.15);
		te = find (world, classname, "player");
		while (te)
		{
			if ((te.currentmenu == "none"))
			{
				centerprint (te, "** press your b key to buy items **\n");
			}
			te = find (te, classname, "player");
		}
		if ((round_end < 1.1))
		{
			round_end = WEAPON_SHOTGUN;
		}
		return;
	}
	if ((round_end == 1))
	{
		te = find (world, classname, "player");

		bprint (PRINT_MEDIUM, "** The next round has now started **\n");
		sound (self, CHAN_BODY, "radio/3_letsgo.wav", 0.6, ATTN_NONE);
		gtimer = 180;
		round_end = MULTICAST_ALL;
		teamthatwon = MULTICAST_ALL;
		team_blue = MULTICAST_ALL;
		team_red = MULTICAST_ALL;
		return;
	}
	if ((respawnvar == WEAPON_SHOTGUN))
	{
		round_end = MULTICAST_ALL;
		teamthatwon = MULTICAST_ALL;
		team_blue = MULTICAST_ALL;
		team_red = MULTICAST_ALL;
		return;
	}

	team1 = 0;

		te = find (world, classname, "player");
		while (te)
		{
			if (te.classname == "player" && te.team == 1 && te.ghost == 0)
				team1 = (team1 + 1);
			if (te.classname == "player" && te.team == 2 && te.ghost == 0)
				team2 = (team2 + 1);

			te = find (te, classname, "player");
		}

	if (((((team1 == 0) && (team2 == 0)) && (team_red == 0)) && (team_blue == 0)))
	{
		gtimer = 180;
		self.nextthink = (time + WEAPON_SPIKES);
		self.think = GameControl;
		round_end = 0;
		teamthatwon = 0;
		team_blue = 0;
		team_red = 0;
		return;
	}
	te = find (world, classname, "info");

	if ((((team1 >= 1) && (team2 == 0)) && (team_red == 0)))//Only Rangers
	{
			gtimer = 180;
			self.nextthink = (time + WEAPON_BIG);
			self.think = GameControl;
			round_end = MULTICAST_ALL;
			teamthatwon = MULTICAST_ALL;
			team_blue = MULTICAST_ALL;
			team_red = MULTICAST_ALL;
			return;
	}
	if ((((team1 == 0) && (team2 >= 1)) && (team_blue == 0)))//Only Raiders
	{
			gtimer = 180;
			self.nextthink = (time + WEAPON_BIG);
			self.think = GameControl;
			round_end = MULTICAST_ALL;
			teamthatwon = MULTICAST_ALL;
			team_blue = MULTICAST_ALL;
			team_red = MULTICAST_ALL;
			return;
	}

	if (((team1 >= 1) && (team2 >= 1)))
	{
		return;
	}
	if (((((team2 == 0) && (team1 >= 1)) && (team_red > 0)) && (bombed != 3)))
	{
		bprint (PRINT_MEDIUM, ">>> RANGERS WIN THE ROUND! <<<\n");
		team1win = (team1win + WEAPON_SHOTGUN);
		RoundReset ();
		return;
	}

	if (team1 == 0 && team2 >= 1)
	{
		bprint (PRINT_MEDIUM, ">>> RAIDERS WIN THE ROUND! <<<\n");
		team2win = (team2win + 1);
		RoundReset ();
		return;
	}
	end = 0;
	if (((team_red == 0) && (team1 != 0)))
	{
		te = find (world, classname, "bot");
		while (te)
		{
			if (((te.team == WEAPON_ROCKET) && (te.health > 0)))
			{
				end = WEAPON_SHOTGUN;
			}
			te = find (te, classname, "bot");
		}
		if (((end == 0) && (world.model != "maps/ag_wastes.bsp")))
		{
			bprint (PRINT_MEDIUM, ">>> RANGERS WIN THE ROUND! <<<\n");
			team2win = (team2win + WEAPON_SHOTGUN);
			RoundReset ();
			return;
		}
		round_end = 0;
		teamthatwon = 0;
		return;
	}
	if (((team_blue == 0) && (team2 != 0)))
	{
		te = find (world, classname, "bot");
		while (te)
		{
			if (((te.team == WEAPON_SHOTGUN) && (te.health > 0)))
			{
				end = WEAPON_SHOTGUN;
			}
			te = find (te, classname, "bot");
		}
		if (((end == 0) && (world.model != "maps/ag_wastes.bsp")))
		{
			if (coop == 1)
			{
				bprint (PRINT_MEDIUM, ">>> RAIDERS WIN THE ROUND! <<<\n");
				team2win = (team2win + 1);
				RoundReset ();
				return;
			}
			if (coop == 1)
			{
				bprint (PRINT_MEDIUM, "*** PLAYERS SLAIN ***\n*** ROUND   RESET ***");
				localcmd("restart\n");
				return;
			}
		}
		round_end = 0;
		teamthatwon = 0;
		return;
	}
	if (((team1 == 0) && (team2 == 0)))
	{
		bprint (PRINT_MEDIUM, ">>> TEAM DRAW <<<\n");
		RoundReset ();
		return;
	}
};

void () VoteBoy =
{
	local entity te;
	local float q;

	self.nextthink = (time + DRAW2);
	self.think = VoteBoy;
	if ((mode == 0))
	{
		mode = AS_MELEE;
	}
	if ((ff == 0))
	{
		ff = 0;
	}
	if ((self.vote_time > time))
	{
		return;
	}
	if ((self.vote_on == 0))
	{
		return;
	}
	if ((self.vote_on == WEAPON_ROCKET))
	{
		self.vote_on = 0;
		return;
	}
	te = find (world, classname, "player");
	self.vote_num = 0;
	while (te)
	{
		if (((te.classname == "player") && (te.ghost == 0)))
		{
			self.vote_num = (self.vote_num + WEAPON_SHOTGUN);
		}
		te = find (te, classname, "player");
	}
	bprint (PRINT_MEDIUM, "** VOTE COMPLETED **\n");
	if ((self.vote_type == WEAPON_ROCKET))
	{
		if ((self.vote1 >= (self.vote_num * 0.5)))
		{
			bprint (PRINT_MEDIUM, "** HEADSHOTS ARE IN! **\n");
			headshot = 0;
		}
		else
		{
			if ((self.vote2 >= (self.vote_num * 0.5)))
			{
				bprint (PRINT_MEDIUM, "** HEADSHOTS ARE OUT! **\n");
				headshot = WEAPON_SHOTGUN;
			}
			else
			{
				bprint (PRINT_MEDIUM, "** Not enough votes to switch **\n");
			}
		}
	}
	if ((self.vote_type == WEAPON_SPIKES))
	{
		if ((self.vote1 >= (self.vote_num * 0.5)))
		{
			bprint (PRINT_MEDIUM, "** QUAKE MODE **\n");
			ReplacePlayers (0);
			respawnvar = 1;
		}
		else
		{
			if ((self.vote2 >= (self.vote_num * 0.5)))
			{
				bprint (PRINT_MEDIUM, "** CS MODE **\n");
				ReplacePlayers (0);
				respawnvar = 0;
			}
			else
			{
				bprint (PRINT_MEDIUM, "** Not enough votes to switch **\n");
			}
		}
	}
	if ((self.vote_type == WEAPON_SHOTGUN))
	{
		if ((self.vote1 >= (self.vote_num * 0.5)))
		{
			bprint (PRINT_MEDIUM, "** FF IS NOW OFF **\n");
			ff = 0;
		}
		else
		{
			if ((self.vote2 >= (self.vote_num * 0.5)))
			{
				bprint (PRINT_MEDIUM, "** FF IS NOW ON **\n");
				ff = WEAPON_SHOTGUN;
			}
			else
			{
				bprint (PRINT_MEDIUM, "** Not enough votes to switch **\n");
			}
		}
	}
	if ((self.vote_type == AS_MELEE))
	{
		if ((self.vote6 >= (self.vote_num * 0.5)))
		{
			bprint (PRINT_MEDIUM, "** CURRENT MAP WILL STAY **\n");
		}
		else
		{
			if ((self.vote1 >= (self.vote_num * 0.5)))
			{
				bprint (PRINT_MEDIUM, "** MAP WILL NOW SWITCH **\n");
				changelevel ("speedrun");
			}
			else
			{
				if ((self.vote2 >= (self.vote_num * 0.5)))
				{
					bprint (PRINT_MEDIUM, "** MAP WILL NOW SWITCH **\n");
					changelevel ("ag_town");
				}
				else
				{
					if ((self.vote3 >= (self.vote_num * 0.5)))
					{
						bprint (PRINT_MEDIUM, "** MAP WILL NOW SWITCH **\n");
						changelevel ("ag_kahn");
					}
					else
					{
						if ((self.vote4 >= (self.vote_num * 0.5)))
						{
							bprint (PRINT_MEDIUM, "** MAP WILL NOW SWITCH **\n");
							changelevel ("tanker");
						}
						else
						{
							if ((self.vote5 >= (self.vote_num * 0.5)))
							{
								bprint (PRINT_MEDIUM, "** MAP WILL NOW SWITCH **\n");
								changelevel ("ag_wastes");
							}
							else
							{
								if ((self.vote6 >= (self.vote_num * 0.5)))
								{
									bprint (PRINT_MEDIUM, "** MAP WILL NOW SWITCH **\n");
									changelevel ("ag_vault");
								}
								else
								{
									if ((self.vote7 >= (self.vote_num * 0.5)))
									{
										bprint (PRINT_MEDIUM, "** MAP WILL NOW SWITCH **\n");
										changelevel ("de_dust2");
									}
									else
									{
										bprint (PRINT_MEDIUM, "** Not enough votes to switch **\n");
									}
								}
							}
						}
					}
				}
			}
		}
	}
	if ((self.vote_type == MULTICAST_PVS_R))
	{
		if ((self.vote1 >= (self.vote_num * 0.5)))
		{
			bprint (PRINT_MEDIUM, "** WIMPY MODE **\n");
			z_skill = 0;
		}
		else
		{
			if ((self.vote2 >= (self.vote_num * 0.5)))
			{
				bprint (PRINT_MEDIUM, "** AVERAGE MODE **\n");
				z_skill = WEAPON_SHOTGUN;
			}
			else
			{
				if ((self.vote3 >= (self.vote_num * 0.5)))
				{
					bprint (PRINT_MEDIUM, "** SUPER ZOMBIE MODE **\n");
					z_skill = WEAPON_ROCKET;
				}
				else
				{
					if ((self.vote4 >= (self.vote_num * 0.5)))
					{
						bprint (PRINT_MEDIUM, "** NIGHT OF TEH FRIGG'N LIVING DEAD, RUN FER UR LIVES U FOOLS **\n");
						z_skill = AS_MELEE;
					}
					else
					{
						bprint (PRINT_MEDIUM, "** Not enough votes to switch **\n");
					}
				}
			}
		}
	}
	self.vote_on = WEAPON_ROCKET;
	self.vote_num = 0;
	self.vote1 = 0;
	self.vote2 = 0;
	self.vote3 = 0;
	self.vote4 = 0;
	self.vote5 = 0;
	self.vote6 = 0;
	self.vote7 = 0;
	self.vote8 = 0;
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
	lastspawn = world;
	InitBodyQue ();
	coop = 1;
	create_referees();
	SetupStats();

// custom map attributes

	if (self.model == "maps/e1m8.bsp")
		cvar_set ("sv_gravity", "100");
	else
		cvar_set ("sv_gravity", "800");



// the area based ambient sounds MUST be the first precache_sounds

// player precaches     
	W_Precache ();                  // get weapon precaches

// sounds used from C physics code
	precache_sound ("demon/dland2.wav");            // landing thud
	precache_sound ("misc/h2ohit1.wav");            // landing splash

// setup precaches allways needed
	precache_sound ("items/itembk2.wav");           // item respawn sound
	precache_sound ("player/plyrjmp8.wav");         // player jump
	precache_sound ("player/land.wav");                     // player landing
	precache_sound ("player/land2.wav");            // player hurt landing
	precache_sound ("player/drown1.wav");           // drowning pain
	precache_sound ("player/drown2.wav");           // drowning pain
	precache_sound ("player/gasp1.wav");            // gasping for air
	precache_sound ("player/gasp2.wav");            // taking breath
	precache_sound ("player/h2odeath.wav");         // drowning death

	precache_sound ("misc/talk.wav");                       // talk
	precache_sound ("player/teledth1.wav");         // telefrag
	precache_sound ("misc/r_tele1.wav");            // teleport sounds
	precache_sound ("misc/r_tele2.wav");
	precache_sound ("misc/r_tele3.wav");
	precache_sound ("misc/r_tele4.wav");
	precache_sound ("misc/r_tele5.wav");
	precache_sound ("weapons/lock4.wav");           // ammo pick up
	precache_sound ("weapons/pkup.wav");            // weapon up
	precache_sound ("items/armor1.wav");            // armor up
	precache_sound ("weapons/lhit.wav");            //lightning
	precache_sound ("weapons/lstart.wav");          //lightning start
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

// player death sounds
	precache_sound ("player/death1.wav");
	precache_sound ("player/death2.wav");
	precache_sound ("player/death3.wav");
	precache_sound ("player/death4.wav");
	precache_sound ("player/death5.wav");

	precache_sound ("boss1/sight1.wav");

// ax sounds    
	precache_sound ("weapons/ax1.wav");                     // ax swoosh
	precache_sound ("player/axhit1.wav");           // ax hit meat
	precache_sound ("player/axhit2.wav");           // ax hit world

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

	precache_model ("progs/player.mdl");
	precache_model ("progs/guy.mdl");
	precache_model ("progs/lay.mdl");
	precache_model ("progs/eyes.mdl");
	precache_model ("progs/h_player.mdl");
	precache_model ("progs/gib1.mdl");
	precache_model ("progs/gib2.mdl");
	precache_model ("progs/gib3.mdl");

	precache_model ("progs/s_bubble.spr");  // drowning bubbles
	precache_model ("progs/s_explod.spr");  // sprite explosion

	precache_model ("progs/v_axe.mdl");
	precache_model ("progs/v_shot.mdl");
	precache_model ("progs/v_span.mdl");
	precache_model ("progs/v_nail.mdl");
	precache_model ("progs/v_rock.mdl");
	precache_model ("progs/v_shot2.mdl");
	precache_model ("progs/v_nail2.mdl");
	precache_model ("progs/v_rock2.mdl");
	precache_model ("progs/v_fist.mdl");
	precache_model ("progs/v_knife.mdl");
	precache_model ("progs/v_1911.mdl");
	precache_model ("progs/v_ak47.mdl");
	precache_model ("progs/v_smg.mdl");
	precache_model ("progs/v_rangem.mdl");
	precache_model ("progs/v_pipe.mdl");
	precache_model ("progs/v_shotgun.mdl");
	precache_model ("progs/v_double.mdl");
	precache_model ("progs/v_mp9.mdl");
	precache_model ("progs/v_sa80.mdl");
	precache_model ("progs/v_deagle.mdl");
	precache_model ("progs/v_alien.mdl");
	precache_model ("progs/v_srifle.mdl");
	precache_model ("progs/v_night.mdl");
	precache_model ("progs/v_nailer.mdl");
	precache_model ("progs/v_piperifle.mdl");
	precache_model ("progs/v_handgren.mdl");

	precache_model ("progs/sneak.mdl");
	precache_model ("progs/dead.mdl");
	precache_model ("progs/hbar.spr");

	precache_model ("progs/w_knife.mdl");
	precache_model ("progs/w_pipe.mdl");
	precache_model ("progs/w_shotgun.mdl");
	precache_model ("progs/w_axe.mdl");
	precache_model ("progs/w_1911.mdl");
	precache_model ("progs/w_deagle.mdl");
	precache_model ("progs/w_alien.mdl");
	precache_model ("progs/w_mp7.mdl");
	precache_model ("progs/w_mp9.mdl");
	precache_model ("progs/w_jackhammer.mdl");
	precache_model ("progs/w_rangem.mdl");
	precache_model ("progs/w_sa80.mdl");
	precache_model ("progs/w_ak47.mdl");
	precache_model ("progs/w_night.mdl");
	precache_model ("progs/w_srifle.mdl");
	precache_model ("progs/w_gauss.mdl");
	precache_model ("progs/w_carbine.mdl");
	precache_model ("progs/w_laser.mdl");

	precache_model ("progs/grenade2.mdl");
	precache_model ("maps/b_bh10.bsp");
	precache_model ("maps/b_bh25.bsp");
	precache_model ("maps/b_bh100.bsp");
	precache_model ("progs/s_light.spr");

	precache_sound ("misc/thud.wav");
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
	precache_sound ("weapons/1911.wav");
	precache_sound ("weapons/auto.wav");
	precache_sound ("weapons/auto2.wav");
	precache_sound ("weapons/ak112.wav");
	precache_sound ("weapons/ak47.wav");
	precache_sound ("weapons/amr.wav");
	precache_sound ("weapons/blaster.wav");
	precache_sound ("weapons/blaster2.wav");
	precache_sound ("weapons/blowoff.wav");
	precache_sound ("weapons/bounce.wav");
	precache_sound ("weapons/burst.wav");
	precache_sound ("weapons/caws.wav");
	precache_sound ("weapons/click.wav");
	precache_sound ("weapons/deagle.wav");
	precache_sound ("weapons/dks-1.wav");
	precache_sound ("weapons/energy1.wav");
	precache_sound ("weapons/energy2.wav");
	precache_sound ("weapons/energy3.wav");
	precache_sound ("weapons/energy4.wav");
	precache_sound ("weapons/flamer.wav");
	precache_sound ("weapons/fnfal.wav");
	precache_sound ("weapons/g11.wav");
	precache_sound ("weapons/gauss.wav");
	precache_sound ("weapons/gl-1.wav");
	precache_sound ("weapons/gpull.wav");
	precache_sound ("weapons/helmet.wav");
	precache_sound ("weapons/jackhammer.wav");
	precache_sound ("weapons/laser.wav");
	precache_sound ("weapons/m249.wav");
	precache_sound ("weapons/m4-nw.wav");
	precache_sound ("weapons/needler.wav");
	precache_sound ("weapons/shotgun1.wav");
	precache_sound ("weapons/shotgun2.wav");
	precache_sound ("weapons/mp5.wav");
	precache_sound ("weapons/mp7.wav");
	precache_sound ("weapons/rangem.wav");
	precache_sound ("weapons/ric1.wav");
	precache_sound ("weapons/ric2.wav");
	precache_sound ("weapons/ric3.wav");
	precache_sound ("weapons/ric4.wav");
	precache_sound ("weapons/ric5.wav");
	precache_sound ("weapons/tink1.wav");
	precache_sound ("weapons/sa80-1.wav");
	precache_sound ("weapons/shell.wav");
	precache_sound ("misc/build1.wav");
	precache_sound ("misc/build2.wav");
	precache_sound ("misc/build3.wav");
	precache_sound ("player/agdie1.wav");
	precache_sound ("player/agdie2.wav");
	precache_sound ("player/agdie3.wav");
	precache_sound ("player/agdie4.wav");
	precache_sound ("player/agdie5.wav");
	precache_sound ("player/berserk.wav");
	precache_sound ("player/breathe.wav");
	precache_sound ("player/headshot.wav");
	precache_sound ("player/hit1.wav");
	precache_sound ("player/paina.wav");
	precache_sound ("player/painb.wav");
	precache_model ("progs/bolt.mdl");              // for lightning gun
	precache_model ("progs/bolt2.mdl");             // for lightning gun
	precache_model ("progs/bolt3.mdl");             // for boss shock
	precache_model ("progs/lavaball.mdl");  // for testing
	
	precache_model ("progs/missile.mdl");
	precache_model ("progs/grenade.mdl");
	precache_model ("progs/spike.mdl");
	precache_model ("progs/s_spike.mdl");

	precache_model ("progs/backpack.mdl");

	precache_model ("progs/zom_gib.mdl");

	precache_model ("progs/v_light.mdl");
	


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
};

void() StartFrame =
{
	timelimit = cvar("timelimit") * 60;
	fraglimit = cvar("fraglimit");
	teamplay = cvar("teamplay");
	deathmatch = cvar("deathmatch");
	
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


