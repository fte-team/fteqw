
// prototypes
void () W_WeaponFrame;
void() W_SetCurrentAmmo;
void() player_pain;
void() player_stand1;
void (vector org) spawn_tfog;
void (vector org, entity death_owner) spawn_tdeath;
void(float slot) WeaponAmmo;
void() Identify;
void(entity to, float iid, float amount) AddStackable;
void(entity to, float iid, float amount) AddNonStackable;
void() shop_face;

float modelindex_dead, modelindex_sneak, modelindex_prone, modelindex_eyes, modelindex_player, modelindex_gone, modelindex_melee, modelindex_crawl;

/*
=============================================================================

				LEVEL CHANGING / INTERMISSION

=============================================================================
*/

string nextmap;

float   intermission_exittime;

/*QUAKED info_intermission (1 0.5 0.5) (-16 -16 -16) (16 16 16)
This is the camera point for the intermission.
Use mangle instead of angle, so you can set pitch or roll as well as yaw.  'pitch roll yaw'
*/
void() info_intermission =
{
	self.angles = self.mangle;      // so C can get at it
};



void() DecodeLevelParms;

void() SetChangeParms =
{
	if (!self.current_slot)	//changing without spawning?
		DecodeLevelParms();	//make sure we have our inventory!

	if (self.supertime > 0)
	{
		self.supertime = 0;
		sprint(self, 2, "you are now back to your normal self.\n");
		self.skill_combat = self.skill_combat - 10;
		self.skill_doctor = self.skill_doctor - 10;
		self.skill_sneak = self.skill_sneak - 10;
		self.skill_science = self.skill_science - 10;
		self.skill_speech = self.skill_speech - 10;
	}

	self.items = self.items - (self.items & 
	(IT_KEY1 | IT_KEY2 | IT_INVISIBILITY | IT_INVULNERABILITY | IT_SUIT | IT_QUAD) );
	

	parm5 = self.skill_combat;
	parm6 = self.skill_doctor;
	parm7 = self.skill_sneak;
	parm14 = self.skill_science;
	parm16 = self.skill_speech;


	parm8 = self.score;
	parm9 = self.perk1;
	parm10 = self.perk2;
	parm17 = self.islot1;
	parm18 = self.islot2;
	parm19 = self.islot3;
	parm20 = self.islot4;
	parm21 = self.islot5;
	parm22 = self.islot6;
	parm23 = self.islot7;
	parm24 = self.islot8;
	parm25 = self.islot9;
	parm26 = self.islot10;
	parm27 = self.islot11;
	parm28 = self.islot12;
	parm29 = self.islot13;
	parm30 = self.islot14;
	parm31 = self.islot15;
	parm32 = self.islot16;
	parm1 = self.islot17;
	parm2 = self.islot18;
	parm3 = self.islot19;
	parm4 = self.islot20;
	parm11 = self.islot21;
	parm12 = self.islot22;
	parm13 = self.islot23;
	parm15 = self.islot24;

};

void() SetNewParms =
{	

//16 is the current_slot
//17 to 32 are inventory slots.
//the assignments were moved into PutClientInServer
//on account of changing maps before spawning was resulting in no inventory.
	parm1 = 0;
	parm2 = 0;
	parm3 = SlotVal(IID_ARM_LEATHER, 1);
	parm4 = 0;
	parm5 = 0;
	parm6 = 0;
	parm7 = 0;
	parm8 = 0;
	parm9 = 0;
	parm10 = 0;
	parm11 = 0;
	parm12 = 0;
	parm13 = 0;
	parm14 = 0;
	parm15 = 0;
	parm16 = 0;
	parm17 = 0;
	parm18 = 0;
	parm19 = 0;
	parm20 = 0;
	parm21 = 0;
	parm22 = 0;
	parm23 = 0;
	parm24 = 0;
	parm25 = 0;
	parm26 = 0;
	parm27 = 0;
	parm28 = 0;
	parm29 = 0;
	parm30 = 0;
	parm31 = 0;
	parm32 = 0;
};

void() DecodeLevelParms =
{
	setspawnparms(self);


	self.skill_combat = parm5;
	self.skill_doctor = parm6;
	self.skill_sneak = parm7;
	self.skill_science = parm14;
	self.skill_speech = parm16;

	self.score = parm8;
	self.perk1 = parm9;
	self.perk2 = parm10;


	self.current_slot = 1;

	self.islot1 = parm17;
	self.islot2 = parm18;
	self.islot3 = parm19;
	self.islot4 = parm20;
	self.islot5 = parm21;
	self.islot6 = parm22;
	self.islot7 = parm23;
	self.islot8 = parm24;
	self.islot9 = parm25;
	self.islot10= parm26;
	self.islot11= parm27;
	self.islot12 = parm28;
	self.islot13 = parm29;
	self.islot14 = parm30;
	self.islot15 = parm31;
	self.islot16 = parm32;
	self.islot17 = parm1;
	self.islot18 = parm2;
	self.islot19 = parm3;
        self.islot20 = parm4;
	self.islot21 = parm11;
	self.islot22 = parm12;
	self.islot23 = parm13;
        self.islot24 = parm15;

};

/*
============
FindIntermission

Returns the entity to view from
============
*/
entity() FindIntermission =
{
	local   entity spot;
	local   float cyc;

// look for info_intermission first
	spot = find (world, classname, "info_intermission");
	if (spot)
	{       // pick a random one
		cyc = random() * 4;
		while (cyc > 1)
		{
			spot = find (spot, classname, "info_intermission");
			if (!spot)
				spot = find (spot, classname, "info_intermission");
			cyc = cyc - 1;
		}
		return spot;
	}

// then look for the start position
	spot = find (world, classname, "info_player_start");
	if (spot)
		return spot;
	
	objerror ("FindIntermission: no spot");
	return world;
};


void() GotoNextMap =
{
	local string newmap;

//ZOID: 12-13-96, samelevel is overloaded, only 1 works for same level

	if (cvar("samelevel") == 1)     // if samelevel is set, stay on same level
		changelevel (mapname);
	else {
		// configurable map lists, see if the current map exists as a
		// serverinfo/localinfo var
		newmap = infokey(world, mapname);
		if (newmap != "")
			changelevel (newmap);
		else
			changelevel (nextmap);
	}
};



/*
============
IntermissionThink

When the player presses attack or jump, change to the next level
============
*/
void() IntermissionThink =
{
	if (time < intermission_exittime)
		return;

	if (!self.button0 && !self.button1 && !self.button2)
		return;
	
	GotoNextMap ();
};


void() get_new_map =
{
	if (random()<0.04)
		m_map = "e1m1";	
	else if (random()<0.04)
		m_map = "e2m1";
	else if (random()<0.10)
		m_map = "e3m1";
	else if (random()<0.04)
		m_map = "e4m1";
	else if (random()<0.08)
		m_map = "bbelief1";
	else if (random()<0.08)
		m_map = "bbelief2";
	else if (random()<0.08)
		m_map = "bbelief3";
	else if (random()<0.08)
		m_map = "bbelief4";
	else if (random()<0.08)
		m_map = "bbelief5";
	else if (random()<0.08)
		m_map = "bbelief6";
	else if (random()<0.08)
		m_map = "e1m4";
	else if (random()<0.08)
		m_map = "e2m3";
	else if (random()<0.08)
		m_map = "e3m3";
	else if (random()<0.08)
		m_map = "e1m3";
	else if (random()<0.08)
		m_map = "e1m5";
	else
		m_map = "bbelief1";

};

void(float type) get_new_mission =
{
	local float x;
	local string savemap;


	if (type == 1)
		savemap = "hostage";
	if (type == 2)
		savemap = "supplies";
	if (type == 3)
		savemap = "shadow";
	if (type == 4)
		savemap = "deadtown";
	if (type == 5)
		savemap = "destroy";
	if (type == 6)
		savemap = "defuse";

	localcmd("serverinfo objective ");
	localcmd(savemap);
	localcmd("\n");

	get_new_map();

	bprint(2, "new mission has been received: ");
	bprint(2, savemap);
	bprint(2, "\n");
	bprint(2, "location coordinates are near ");
	bprint(2, m_map);
	bprint(2, "\n");

};

/*
============
execute_changelevel

The global "nextmap" has been set previously.
Take the players to the intermission spot
============
*/
void() execute_changelevel =
{
	local string savemap;
	local entity    pos;
	local float x;



	localcmd(strcat("map ", self.map, "\n"));
	localcmd(strcat("serverinfo direction ", self.direction, "\n"));


};


void() changelevel_touch =
{
	if (coop == 0)
		return;

	if (other.classname != "player")
		return;

	if ((world.map_obj == OBJ_HOSTAGE && hostages > 0) && random()<0.02)
	{
		sprint (other, 2,"there are more hostages to save.\n");
		return;
	}
	else if ((world.map_obj == OBJ_DEADTOWN && hostages > 0) && random()<0.02)
	{
		sprint (other, 2,"there are more hostages to save.\n");
		return;
	}
	else if ((world.map_obj == OBJ_SUPPLY && supplies > 0) && random()<0.02)
	{
		sprint (other, 2,"there are more supplies to secure.\n");
		return;
	}
	else if ((world.map_obj == OBJ_DESTROY && supplies > 0) && random()<0.02)
	{
		sprint (other, 2,"there are more fuel tanks to destroy.\n");
		return;
	}
	else if ((world.map_obj == OBJ_DEFUSE && supplies > 0) && random()<0.02)
	{
		sprint (other, 2,"there are still bombs to disarm.\n");
		return;
	}
	else
	{

		if (infokey(world, "worldtime") == "1")
			localcmd("serverinfo worldtime 2\n");
		else if (infokey(world, "worldtime") == "2")
			localcmd("serverinfo worldtime 3\n");
		else if (infokey(world, "worldtime") == "3")
			localcmd("serverinfo worldtime 3\n");
		else if (infokey(world, "worldtime") == "4")
			localcmd("serverinfo worldtime 5\n");
		else if (infokey(world, "worldtime") == "5")
			localcmd("serverinfo worldtime 6\n");
		else if (infokey(world, "worldtime") == "6")
			localcmd("serverinfo worldtime 1\n");

		localcmd(strcat("map ", self.map, "\n"));
		localcmd(strcat("serverinfo direction ", self.direction, "\n"));
		return;
	}

	if (world.map_obj == OBJ_HOSTAGE && hostages > 0)
		return;
	else if (world.map_obj == OBJ_SUPPLY && supplies > 0)
		return;
	else if (world.map_obj == OBJ_DESTROY && supplies > 0)
		return;
	else if (world.map_obj == OBJ_DEFUSE && supplies > 0)
		return;


	bprint (PRINT_HIGH, other.netname);
	bprint (PRINT_HIGH," exited the level\n");
	
	other = find (world, classname, "player");
	while (other)
	{
		stuffcmd(other, "play music/victory\n");
		other.missionbrief = 3;
		other.takedamage = DAMAGE_NO;
		other.solid = SOLID_NOT;
		other.movetype = MOVETYPE_NONE;
		other.modelindex = 0;

		other = find (other, classname, "player");
	}      

	//localcmd("serverinfo objective return\n");

	SUB_UseTargets ();

	self.touch = SUB_Null;

// we can't move people right now, because touch functions are called
// in the middle of C movement code, so set a think time to do it
	self.think = execute_changelevel;
	self.nextthink = time + 10;


		if (infokey(world, "worldtime") == "1")
			localcmd("serverinfo worldtime 2\n");
		else if (infokey(world, "worldtime") == "2")
			localcmd("serverinfo worldtime 3\n");
		else if (infokey(world, "worldtime") == "3")
			localcmd("serverinfo worldtime 3\n");
		else if (infokey(world, "worldtime") == "4")
			localcmd("serverinfo worldtime 5\n");
		else if (infokey(world, "worldtime") == "5")
			localcmd("serverinfo worldtime 6\n");
		else if (infokey(world, "worldtime") == "6")
			localcmd("serverinfo worldtime 1\n");
};

/*QUAKED trigger_changelevel (0.5 0.5 0.5) ? NO_INTERMISSION
When the player touches this, he gets sent to the map listed in the "map" variable.  Unless the NO_INTERMISSION flag is set, the view will go to the info_intermission spot and display stats.
*/
void() trigger_changelevel =
{
	if (!self.map)
		objerror ("chagnelevel trigger doesn't have map");
	
	InitTrigger ();
	self.touch = changelevel_touch;
};


/*
=============================================================================

				PLAYER GAME EDGE FUNCTIONS

=============================================================================
*/

void() set_suicide_frame;

// called by ClientKill and DeadThink
void() respawn =
{
	// make a copy of the dead body for appearances sake
	//CopyToBodyQue (self);
	// set default spawn parms
	//SetNewParms ();
	// respawn              
	PutClientInServer ();
};


/*
============
ClientKill

Player entered the suicide command
============
*/
void() ClientKill =
{
	sprint(self, 2, "you chicken out!\n");
};

float(vector v) CheckSpawnPoint =
{
	return FALSE;
};

/*
============
SelectSpawnPoint

Returns the entity to spawn at
============
*/
entity() SelectSpawnPoint =
{
	local   entity spot, thing;
	local   float   numspots, totalspots;
	local   float   pcount;
	local entity spots;
	local string ent1;

	numspots = 0;
	totalspots = 0;


	if (infokey(world, "coop") == "0")
		coop = 0;
	if (infokey(world, "coop") == "1")
		coop = 1;

// testinfo_player_start is only found in regioned levels

	spot = find (world, classname, "testplayerstart");
	if (spot)
		return spot;
	
	spot = find (world, classname, "spawn3");

	if (coop == 0)
		ent1 = "info_player_deathmatch";
	else
	{

		if (infokey(world, "direction") == "north")
			ent1 = "info_player_coop_south";
		else if (infokey(world, "direction") == "west")
			ent1 = "info_player_coop_east";
		else if (infokey(world, "direction") == "east")
			ent1 = "info_player_coop_west";
		else if (infokey(world, "direction") == "south")
			ent1 = "info_player_coop_north";
		else
			ent1 = "info_player_coop";
	}
	if (ent1 == "spawn3")
	{
		if (self.team == 1)
			ent1 = "spawn1";       
		if (self.team == 2)
			ent1 = "spawn2";
	}    
   
// choose a info_player_deathmatch point

// ok, find all spots that don't have players nearby

	spots = world;
	spot = find (world, classname, ent1);       
	while (spot)
	{
		totalspots = totalspots + 1;

		thing=findradius(spot.origin, 40);
		pcount=0;               
		while (thing)
		{
			if (thing.classname == "player")
				pcount=pcount + 1;                      
			thing=thing.chain;      
		}
		if (pcount == 0)
		{
			spot.goalentity = spots;
			spots = spot;
			numspots = numspots + 1;
		}

		// Get the next spot in the chain
		spot = find (spot, classname, ent1);                
	}


	totalspots=totalspots - 1;
		if (!numspots)//full, find random spot
		{
			totalspots = rint((random() * totalspots));
			spot = find (world, classname, ent1);       
			while (totalspots > 0)
			{
				totalspots = totalspots - 1;
				spot = find (spot, classname, ent1);
			}
		return spot;
	}
		
// We now have the number of spots available on the map in numspots

	// Generate a random number between 1 and numspots

	numspots = numspots - 1;
	
	numspots = rint((random() * numspots ) );

	spot = spots;
	while (numspots > 0)
	{
		spot = spot.goalentity;
		numspots = numspots - 1;
	}
	return spot;

};


void() DecodeLevelParms;
void() PlayerDie;

/*
===========
PutClientInServer

called each time a player enters a new level
============
*/
void() PutClientInServer =
{
	local entity spot, gun;
	local float x, xx;

	self.classname = "player";
	self.health = 100;
	self.takedamage = DAMAGE_AIM;
	self.movetype = MOVETYPE_WALK;
	self.show_hostile = 0;
	self.max_health = 150 + (self.skill_combat*2);
	self.health = 100;
	self.flags = FL_CLIENT;
	self.air_finished = time + 12;
	self.dmg = 2;                   // initial water damage
	self.super_damage_finished = 0;
	self.radsuit_finished = 0;
	self.invisible_finished = 0;
	self.currentmenu = "none";
	self.invincible_finished = 0;
	self.effects = 0;
	self.invincible_time = 0;
	self.active = 1;
	self.solid = SOLID_NOT;
	self.materialize = 20;


	DecodeLevelParms ();


	if (self.islot1 == 0 && self.islot2 == 0 && self.islot3 == 0 && self.islot4 == 0 && self.islot5 == 0 && self.islot6 == 0 && self.islot7 == 0 && self.islot8 == 0 && self.islot9 == 0 && self.islot10 == 0 && self.islot11 == 0 && self.islot12 == 0)
	{
		self.islot1 = SlotVal(IID_WP_KNIFE, 1);
		self.islot2 = 0;
		self.islot3 = SlotVal(IID_ARM_LEATHER, 1);
		self.islot4 = SlotVal(IID_EQUIP_CLIMBINGGEAR, 1);
		self.islot5 = SlotVal(IID_CHEM_STIMPACK, 5);
		self.islot7 = 0;
		self.islot8 = SlotVal(IID_MONEY, 100);
		self.islot9 = 0;
		self.islot10 = 0;
		self.islot11 = SlotVal(IID_CHEM_MEDICALBAG, 25);
		self.islot12 = 0;
		self.islot13 = 0;
		self.islot14 = 0;
		self.islot15 = 0;
		self.islot16 = 0;
		self.islot17 = 0;
		self.islot18 = 0;
		self.islot19 = 0;
      		self.islot20 = 0;
     		self.islot21 = 0;
     		self.islot22 = 0;
     		self.islot23 = 0;
     		self.islot24 = 0;
		self.score = 200;
	}

	self.attack_finished = time;
	self.th_pain = player_pain;
	self.th_die = PlayerDie;

	self.deadflag = DEAD_NO;
// paustime is set by teleporters to keep the player from moving a while
	self.pausetime = 0;
	
	spot = SelectSpawnPoint ();

	self.origin = spot.origin + '0 0 1';
	self.angles = spot.angles;
	self.fixangle = TRUE;           // turn this way immediately


	self.max_health = 100 + (self.skill_combat*4) + (self.skill_doctor*2) + (self.skill_sneak*2) + (self.skill_speech*2) + (self.skill_science);

	self.equipment_slot = 0;

	spawn_dot(self);

	self.health = self.max_health;

// oh, this is a hack!
	setmodel (self, "progs/eyes.mdl");
	modelindex_eyes = self.modelindex;

	setmodel (self, "progs/prone.mdl");
	modelindex_crawl = self.modelindex;

	setmodel (self, "progs/lay.mdl");
	modelindex_prone = self.modelindex;

	setmodel (self, "progs/sneak.mdl");
	modelindex_sneak = self.modelindex;

	setmodel (self, "progs/melee.mdl");
	modelindex_melee = self.modelindex;

	setmodel (self, "progs/dead.mdl");
	modelindex_dead = self.modelindex;

	setmodel (self, "");
	modelindex_gone = self.modelindex;

	setmodel (self, "progs/guy.mdl");
	modelindex_player = self.modelindex;

	setsize (self, VEC_HULL_MIN, VEC_HULL_MAX);
	
	self.view_ofs = '0 0 22';
	self.view2 = world;


// Mod - Xian (May.20.97)
// Bug where player would have velocity from their last kill

	self.velocity = '0 0 0';

	player_stand1 ();
	
	makevectors(self.angles);
	spawn_tfog (self.origin + v_forward*20);

	spawn_tdeath (self.origin, self);

	// Set Rocket Jump Modifiers
	if (stof(infokey(world, "rj")) != 0)
	{                
		rj = stof(infokey(world, "rj"));
	}

	if (deathmatch == 4)
	{
		self.ammo_shells = 0;
		if (stof(infokey(world, "axe")) == 0)
		{
			self.ammo_nails = 255;
			self.ammo_shells = 255;
			self.ammo_rockets = 255;
			self.ammo_cells = 255;
			self.items = self.items | IT_NAILGUN;
			self.items = self.items | IT_SUPER_NAILGUN;
			self.items = self.items | IT_SUPER_SHOTGUN;
			self.items = self.items | IT_ROCKET_LAUNCHER;
//		self.items = self.items | IT_GRENADE_LAUNCHER;
			self.items = self.items | IT_LIGHTNING;
		}
		self.items = self.items - (self.items & (IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) + IT_ARMOR3;
		self.armorvalue = 200;
		self.armortype = 0.8;
		self.health = 250;
		self.items = self.items | IT_INVULNERABILITY;
		self.invincible_time = 1;
		self.invincible_finished = time + 3;
	}

	if (deathmatch == 5)
	{
		self.ammo_nails = 80;
		self.ammo_shells = 30;
		self.ammo_rockets = 10;
		self.ammo_cells = 30;
		self.items = self.items | IT_NAILGUN;
		self.items = self.items | IT_SUPER_NAILGUN;
		self.items = self.items | IT_SUPER_SHOTGUN;
		self.items = self.items | IT_ROCKET_LAUNCHER;
		self.items = self.items | IT_GRENADE_LAUNCHER;
		self.items = self.items | IT_LIGHTNING;
		self.items = self.items - (self.items & (IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) + IT_ARMOR3;
		self.armorvalue = 200;
		self.armortype = 0.8;
		self.health = 200;
		self.items = self.items | IT_INVULNERABILITY;
		self.invincible_time = 1;
		self.invincible_finished = time + 3;
	}

	W_SetCurrentAmmo();
	stuffcmd(self, "impulse 2\n");
	stuffcmd(self, "impulse 1\n");


	if (world.model == "maps/r_bunker.bsp")
		stuffcmd(self, "play music/vault\n");
	else if (world.map_obj == OBJ_DEADTOWN)
		stuffcmd(self, "play music/afterglow\n");

	if (infokey(world, "worldtime") == "1" || infokey(world, "worldtime") == "2")
		stuffcmd(self, "r_skybox dawn\n");
	else if (infokey(world, "worldtime") == "3" || infokey(world, "worldtime") == "4")
		stuffcmd(self, "r_skybox dusk\n");
	else if (infokey(world, "worldtime") == "5" || infokey(world, "worldtime") == "6")
		stuffcmd(self, "r_skybox night\n");
};


/*
=============================================================================

				QUAKED FUNCTIONS

=============================================================================
*/


/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 24)
The normal starting point for a level.
*/
void() info_player_start =
{
};


/*QUAKED info_player_start2 (1 0 0) (-16 -16 -24) (16 16 24)
Only used on start map for the return point from an episode.
*/
void() info_player_start2 =
{
/*
	local entity start;
	local float x;

	x = 10;

	while (x > 0)
	{
		start = spawn();
		setsize(start, '-16 -16 -24', '16 16 32');
		setorigin(start, self.origin + '0 0 4');
		start.solid = SOLID_BBOX;
		start.health = 0;
		start.movetype = MOVETYPE_BOUNCE;
		start.velocity = '500 100 9000';
		setmodel (start, "progs/guy.mdl");
		start.avelocity = '3000 1000 2000';
		start.frame = 1;
		//start.think = become_startpoint;
		//start.nextthink = time + 2;

		x = x - 1;
	}
*/

};

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 24)
potential spawning position for deathmatch games
*/
void() info_player_deathmatch =
{
};

/*QUAKED info_player_coop (1 0 1) (-16 -16 -24) (16 16 24)
potential spawning position for coop games
*/
void() info_player_coop =
{
};

void() info_player_coop_north =
{
};

void() info_player_coop_west =
{
};

void() info_player_coop_east =
{
};

void() info_player_coop_south =
{
};

void() spawn1 =
{
};

void() spawn2 =
{
};

void() spawn3 =
{
};

/*
===============================================================================

RULES

===============================================================================
*/

/*
go to the next level for deathmatch
*/
void() NextLevel =
{
	local entity o;

	if (nextmap != "")
		return; // already done

	if (mapname == "start")
	{
		if (!cvar("registered"))
		{
			mapname = "e1m1";
		}
		else if (!(serverflags & 1))
		{
			mapname = "e1m1";
			serverflags = serverflags | 1;
		}
		else if (!(serverflags & 2))
		{
			mapname = "e2m1";
			serverflags = serverflags | 2;
		}
		else if (!(serverflags & 4))
		{
			mapname = "e3m1";
			serverflags = serverflags | 4;
		}
		else if (!(serverflags & 8))
		{
			mapname = "e4m1";
			serverflags = serverflags - 7;
		}
 
		o = spawn();
		o.map = mapname;
	}
	else
	{
		// find a trigger changelevel
		o = find(world, classname, "trigger_changelevel");
		if (!o || mapname == "start")
		{       // go back to same map if no trigger_changelevel
			o = spawn();
			o.map = mapname;
		}
	}

	nextmap = o.map;

	if (o.nextthink < time)
	{
		o.think = execute_changelevel;
		o.nextthink = time + 0.1;
	}
};

/*
============
CheckRules

Exit deathmatch games upon conditions
============
*/
void() CheckRules =
{       
	if (timelimit && time >= timelimit)
		NextLevel ();
	
	if (fraglimit && self.frags >= fraglimit)
		NextLevel ();
};


void () LocateSpectatorTarget =
{
	local float playercnt;
	local float total;
	local float player;
	local entity ke;
	local entity de;

	if (self.button0)
	{
		bprint(PRINT_MEDIUM, self.classname);
		bprint(PRINT_MEDIUM, " is dying...\n");
		return;
	}

	if (self.ghost == 1)
	{
		ke = find (world, classname, "player");
		de = ke;
		total = -1;
		player = 0;
		playercnt = 0;
		while (ke)
		{
			if (ke.classname == "player" && ke.ghost == 0 && ke.health > 0)
			{
				total = total + 1;
			}
			ke = find (ke, classname, "player");
		}
		while (de)
		{
			if (de.classname == "player" && de.ghost == 0 && de.health > 0)
			{
				if (player == self.ghostcnt)
				{
					makevectors (de.angles);
					self.view2 = de;
					self.ghostcnt = self.ghostcnt + 1;
					self.cnt = 0;
					sprint(self, 2, "now following ");
					sprint(self, 2, de.netname);
					sprint(self, 2, ".\n");

					if (self.ghostcnt > total)
						self.ghostcnt = 0;

					return;
				}
				player = player + 1;
				if (self.ghostcnt > total)
				{
					self.ghostcnt = 0;
				}
			}
			de = find (de, classname, "player");
		}
		if (player == 0)
		{
			self.view2 = world;
			centerprint (self, "No players found...\n");
			if (!deathmatch)
				localcmd("restart\n");
			return;
		}
	}
};

//============================================================================

void() PlayerDeathThink =
{
	local float             forward;

	self.view_ofs = '0 0 -12';
	self.modelindex = modelindex_dead;
//	self.solid = SOLID_NOT;

	if ((self.flags & FL_ONGROUND))
	{
		forward = vlen (self.velocity);
		forward = forward - 20;
		if (forward <= 0)
			self.velocity = '0 0 0';
		else    
			self.velocity = forward * normalize(self.velocity);
	}

// wait for all buttons released
	if (self.deadflag == DEAD_DEAD)
	{
		if (self.button2 || self.button1 || self.button0)
			return;
		self.deadflag = DEAD_RESPAWNABLE;
		return;
	}

// wait for any button down
	if (!self.button2 && !self.button1 && !self.button0)
		return;

	if (self.dtime < time)
	{
		LocateSpectatorTarget();
		self.dtime = time + 1;
	}

	self.button0 = 0;
	self.button1 = 0;
	self.button2 = 0;
	//respawn();
};


void() PlayerJump =
{
	if (self.flags & FL_WATERJUMP)
		return;

	if (self.health <= (self.max_health*0.20))
		return;

	if (self.waterlevel >= 2)
	{
// play swiming sound
		if (self.swim_flag < time)
		{
			self.swim_flag = time + 1;
			if (random() < 0.5)
				sound (self, CHAN_BODY, "misc/water1.wav", 1, ATTN_NORM);
			else
				sound (self, CHAN_BODY, "misc/water2.wav", 1, ATTN_NORM);
		}

		return;
	}

	if (!(self.flags & FL_ONGROUND))
		return;

	if (self.position > 0)
		return;

	if ( !(self.flags & FL_JUMPRELEASED) )
		return;         // don't pogo stick

	self.flags = self.flags - (self.flags & FL_JUMPRELEASED);       
	self.button2 = 0;

// player jumping sound
	sound (self, CHAN_BODY, "player/ax1.wav", 1, ATTN_NORM);
};


/*
===========
WaterMove

============
*/
.float  dmgtime;

void() WaterMove =
{
//dprint (ftos(self.waterlevel));
	if (self.movetype == MOVETYPE_NOCLIP)
		return;
	if (self.health < 0)
		return;

	if (self.waterlevel != 3)
	{
		if (self.air_finished < time)
			sound (self, CHAN_VOICE, "player/gasp2.wav", 1, ATTN_NORM);
		else if (self.air_finished < time + 9)
			sound (self, CHAN_VOICE, "player/gasp1.wav", 1, ATTN_NORM);
		self.air_finished = time + 12;
		self.dmg = 2;
	}
	else if (self.air_finished < time)
	{       // drown!
		if (self.pain_finished < time)
		{
			self.dmg = self.dmg + 2;
			if (self.dmg > 15)
				self.dmg = 10;
			T_Damage (self, world, world, self.dmg);
			self.pain_finished = time + 1;
		}
	}
	
	if (!self.waterlevel)
	{
		if (self.flags & FL_INWATER)
		{       
			// play leave water sound
			sound (self, CHAN_BODY, "misc/outwater.wav", 1, ATTN_NORM);
			self.flags = self.flags - FL_INWATER;
		}
		return;
	}

	if (self.watertype == CONTENT_LAVA)
	{       // do damage
		if (self.dmgtime < time)
		{
			if (self.radsuit_finished > time)
				self.dmgtime = time + 1;
			else
				self.dmgtime = time + 0.2;

			self.radiation = self.radiation + (1*self.waterlevel);
		}
	}
	else if (self.watertype == CONTENT_SLIME)
	{       // do damage
		if (self.dmgtime < time && self.rage != IID_CHEM_RADX)
		{
			self.dmgtime = time + 1;

			self.radiation = self.radiation + ((1*self.waterlevel)/2);
		}
	}
	
	if ( !(self.flags & FL_INWATER) )
	{       

// player enter water sound

		if (self.watertype == CONTENT_LAVA)
			sound (self, CHAN_BODY, "player/inlava.wav", 1, ATTN_NORM);
		if (self.watertype == CONTENT_WATER)
			sound (self, CHAN_BODY, "player/inh2o.wav", 1, ATTN_NORM);
		if (self.watertype == CONTENT_SLIME)
			sound (self, CHAN_BODY, "player/slimbrn2.wav", 1, ATTN_NORM);

		self.flags = self.flags + FL_INWATER;
		self.dmgtime = 0;
	}       
};

void() CheckWaterJump =
{
	local vector start, end;

// check for a jump-out-of-water
	makevectors (self.angles);
	start = self.origin;
	start_z = start_z + 8; 
	v_forward_z = 0;
	normalize(v_forward);
	end = start + v_forward*24;
	traceline (start, end, TRUE, self);
	if (trace_fraction < 1)
	{       // solid at waist
		start_z = start_z + self.maxs_z - 8;
		end = start + v_forward*24;
		self.movedir = trace_plane_normal * -50;
		traceline (start, end, TRUE, self);
		if (trace_fraction == 1)
		{       // open at eye level
			self.flags = self.flags | FL_WATERJUMP;
			self.velocity_z = 225;
			self.flags = self.flags - (self.flags & FL_JUMPRELEASED);
			self.teleport_time = time + 2;  // safety net
			return;
		}
	}
};


//this is used to check if the player is directly in front of a monster
//or off to the side, on the corner of the monsters vision

float(entity targ) check_fov =
{
	local vector	vec;
	local float		dot;
	
	makevectors (targ.angles);
	vec = normalize (self.origin - targ.origin);
	dot = vec * v_forward;
	
	if ( dot > 0.5)
	{
		return TRUE;
	}

	return FALSE;
};

float(entity targ) check_fov_num =
{
	local vector	vec;
	local float		dot;
	
	makevectors (targ.angles);
	vec = normalize (self.origin - targ.origin);
	dot = vec * v_forward;
	

	return dot;
};

float(entity targ) check_fov_side =
{
	local vector	vec;
	local float		dot;
	
	makevectors (targ.angles);
	vec = normalize (self.origin - targ.origin);
	dot = vec * v_forward;
	
	if ( dot > 0.6 && dot < 0.9)
	{
		return TRUE;
	}

	return FALSE;
};

//determines chance of someone getting detected while sneaking

void() SneakLevel =
{
	local float sn, st, front, r, q, x, b, c, y, z, t;
	local entity te, ze;
	local string report;
	

	front = 0;
	q = 1200;
	q = q - (self.skill_sneak*50);


	sn = 99;
	self.sneak = 99;


	//light level gives a bonus/penalty to stealth
	b = light_level(self.origin);
	
	if (b <= 50)
		q = q - (200);
	if (b >= 200)
		q = q + (200);


	//standing still gives a bonus to stealth
	if (self.velocity == '0 0 0')
		q = q - (200);


	//stealthboy gives a bonus to stealth
	st = ToIID(self.islot4);
	if (st == IID_EQUIP_STEALTHBOY)
		q = q - (100);

	//combat armor gives a penalty to stealth
	st = ToIID(self.islot3);
	if (st == IID_ARM_COMBAT || st == IID_ARM_SEVA)
		q = q + (100);

	//metal armor gives a large penalty to stealth
	st = ToIID(self.islot3);
	if (st == IID_ARM_METAL)
		q = q + (200);

	//power armor gives a huge penalty to stealth
	st = ToIID(self.islot3);
	if (st == IID_ARM_LPOWER)
		q = q + (300);

	//tesla armor gives a massive penalty to stealth
	st = ToIID(self.islot3);
	if (st == IID_ARM_TESLA)
		q = q + (400);

	//walking in front of a monster at close range is very bad
	//at longer ranges it isn't so bad to do so
	//thus even healer and barbarian types can sneak if they do so smartly


	c = 0;

	te = findradius(self.origin, q);
	while (te)
	{
		if ((te.enemy.classname != "player" || te.enemy != self) && visible(te) && te.classname == "monster" && (check_fov_num(te) > 0.5) && te.health > 3)
		{	
			if (c == 0)
			{

				r = vlen(self.origin - te.origin);
				t = q * (check_fov_num(te));
				t = t * (check_fov_num(te));
				t = t * (check_fov_num(te));
				y = r / t;
				ze = te;
				c = 1;

			}
			else
			{

				r = vlen(self.origin - te.origin);
				t = q * (check_fov_num(te));
				t = t * (check_fov_num(te));
				t = t * (check_fov_num(te));
				z = r / t;
		

				if (z < y)
				{
					ze = te;
					y = z;
				}

			}
		}
			
		te = te.chain;
	}


	if (r > 0 && r <= q)
	{
		sn = 150 * (r/t);
		sn = sn - 50;
	}

	if (self.invistime > 0)
	{
		bprint(2, "invistime\n");
		self.sneak = 99;
	}


	if (sn <= 1 && sn > 0)
	{
		sprint(self, 2, "you've been detected!\n");
		self.sneak = 0;
		return;
	}

	if (sn >= 99)
		sn = 99;

	if (self.sneak > 0)
		self.sneak = sn;
};

void() ArmorCheck =
{
	local float type, aid, slow;
	aid = ToIID(self.islot3);

	if (self.hold > 0)
		self.speedmulti = 0;

	slow = self.weight/self.max_weight;

	self.speedmulti = 1.00 - (slow/2);

	if (self.health <= (self.max_health*0.20))
		self.speedmulti = 0.75;
	else if (self.health <= (self.max_health*0.10))
		self.speedmulti = 0.50;

	if (self.walk == 1)
		self.speedmulti = self.speedmulti * 0.50;

	self.speedmulti = self.speedmulti*(1+(self.skill_sneak*0.01));

	//Armor
	if (aid == IID_ARM_DESERT)
		type = 0.10;
	if (aid == IID_ARM_LEATHER)
		type = 0.25;
	if (aid == IID_ARM_VANDAL)
		type = 0.30;
	if (aid == IID_ARM_METAL)
		type = 0.50;
	if (aid == IID_ARM_TESLA)
		type = 0.25;
	if (aid == IID_ARM_COMBAT)
		type = 0.40;
	if (aid == IID_ARM_SEVA)
		type = 0.45;
	if (aid == IID_ARM_FORCE)
		type = 0.15;
	if (aid == IID_ARM_LPOWER)
		type = 0.50;
	if (self.rage == IID_CHEM_PSYCHO)
		type += 0.20;


	if (aid <= IID_ARM_METAL)
		self.items = (self.items - (self.items & ((IT_ARMOR1 | IT_ARMOR2) | IT_ARMOR3)) + IT_ARMOR1);
	else if (aid <= IID_ARM_SEVA)
		self.items = (self.items - (self.items & ((IT_ARMOR1 | IT_ARMOR2) | IT_ARMOR3)) + IT_ARMOR1);
	else
		self.items = (self.items - (self.items & ((IT_ARMOR1 | IT_ARMOR2) | IT_ARMOR3)) + IT_ARMOR1);

	if (aid == IID_ARM_LEATHER)
		self.armornoise = "misc/thud.wav";
	if (aid == IID_ARM_VANDAL)
		self.armornoise = "misc/thud.wav";
	if (aid == IID_ARM_METAL)
		self.armornoise = "misc/thud.wav";
	if (aid == IID_ARM_TESLA)
		self.armornoise = "weapons/ric1.wav";
	if (aid == IID_ARM_COMBAT)
		self.armornoise = "misc/thud.wav";
	if (aid == IID_ARM_SEVA)
		self.armornoise = "misc/thud.wav";
	if (aid == IID_ARM_FORCE)
		self.armornoise = "misc/laserdef.wav";
	if (aid == IID_ARM_LPOWER)
		self.armornoise = "weapons/ric1.wav";

	self.armorvalue = type * 100;
	self.armortype = type;
};

void () WeightControl =
{
	//for each inventory item, add up it's weight.
	local float wt;
	wt = wt + GetItemsWeight(self.islot1);
	wt = wt + GetItemsWeight(self.islot2);
	wt = wt + GetItemsWeight(self.islot3);
	wt = wt + GetItemsWeight(self.islot4);
	wt = wt + GetItemsWeight(self.islot5);
	wt = wt + GetItemsWeight(self.islot6);
	wt = wt + GetItemsWeight(self.islot7);
	wt = wt + GetItemsWeight(self.islot8);
	wt = wt + GetItemsWeight(self.islot9);
	wt = wt + GetItemsWeight(self.islot10);
	wt = wt + GetItemsWeight(self.islot11);
	wt = wt + GetItemsWeight(self.islot12);
	wt = wt + GetItemsWeight(self.islot13);
	wt = wt + GetItemsWeight(self.islot14);
	wt = wt + GetItemsWeight(self.islot15);
	wt = wt + GetItemsWeight(self.islot16);

	self.weight = wt;
	self.max_weight = 30;

	self.max_weight = self.max_weight + self.skill_combat;
};


void () PositionControl =
{
	local float exp, slow;

	if (self.position == 0)
		self.view_ofs = '0 0 22';
	if (self.position == 1)
		self.view_ofs = '0 0 5';
	if (self.position == 2)
		self.view_ofs = '0 0 -10';

	if (self.position == 0)
		setsize (self, '-16 -16 -24', '16 16 32');
	if (self.position == 1)
		setsize (self, '-16 -16 -24', '16 16 16');
	if (self.position == 2)
		setsize (self, '-16 -16 -24', '16 16 0');


	if (self.currentmenu != "none")
	{
		self.maxspeed = 0;
		return;
	}

	if (self.position == 0)
		self.maxspeed = 225;
	else if (self.position == 1)
		self.maxspeed = 100;
	else
		self.maxspeed = 50;



	if (self.rage == IID_CHEM_ADRENALINE)
			self.maxspeed = self.maxspeed*1.20;

	if (self.sneak > 0)
			self.maxspeed = self.maxspeed*0.50;

	self.maxspeed = self.maxspeed*self.speedmulti;

	if (self.maxspeed < 50)
		self.maxspeed = 50;


};

.float clientcolors;
.float gravity;
.vector movement;
/*
================
PlayerPreThink

Called every frame before physics are run
================
*/
void() PlayerPreThink =
{
	local   float   r, grip;
	local entity te;

	if (intermission_running)
	{
		IntermissionThink ();   // otherwise a button could be missed between
		return;                                 // the think tics
	}

	if (self.view_ofs == '0 0 0')
		return;         // intermission or finale

	makevectors (self.v_angle);             // is this still used

	if (self.deadflag >= DEAD_DEAD)
	{
		PlayerDeathThink ();
		return;
	}


	if (self.hold > 0)
		self.hold = self.hold - 1;

	if (self.team == 0)
		self.clientcolors = 0;
	else if (self.team == 1)
		self.clientcolors = 13 + 13*16;
	else
		self.clientcolors = 4 + 4*16;
	if (self.team == 0 && self.currentmenu == "none")
	{
		self.currentmenu = "display_enter_screen";
		DisplayMenu ();
		return;
	}



	self.tnl = self.skill_combat + self.skill_doctor + self.skill_sneak + self.skill_science + self.skill_speech;
	self.tnl = 10 + 10*self.tnl;

	if (self.score > self.tnl)
	{
		self.score = self.tnl;
		sound (self, CHAN_BODY, "player/levelup.wav", 1, ATTN_NORM);
		sprint(self, 2, "you have gained a level!\npress 'p' to assign a skill point.\n");
	}

	//perk1timer and perk2timer are "cooldowns" for each perk
		if (self.perk1timer > 0)
			self.perk1timer = self.perk1timer - 1;
		if (self.perk2timer > 0)
			self.perk2timer = self.perk2timer - 1;

	//speech skill provides faster perk regeneration
		if (random()*30 < self.skill_speech)
		{
			if (self.perk1timer > 0)
				self.perk1timer = self.perk1timer - 1;
			if (self.perk2timer > 0)
				self.perk2timer = self.perk2timer - 1;
		}




	self.shadowtime = time_left;

	if (self.cycle1 < time)
	{
		if (self.currentmenu != "none")
			DisplayMenu();

		//if (class == 0 || self.team == 0)
		//	return;

		Identify();
		PositionControl();
		WeightControl();
		ArmorCheck();
		if (self.sneak > 0)
			SneakLevel();
		self.accept = 0;
		

		if (self.safezone > 0)
			self.safezone = self.safezone - 1;

		self.endinfo1 = rescue;
		self.endinfo2 = dead_hostage;
		self.endinfo3 = hostages;

		if (self.invistime > 1)
			self.invistime = self.invistime -1;
		else if (self.invistime == 1)
		{
			self.invistime = 0;
			sound (self, CHAN_BODY, "items/inv2.wav", 1, ATTN_NORM);
			sprint(self, 2, "your disguise wears off.\n");
		}

		if (self.supertime > 1)
			self.supertime = self.supertime -1;
		else if (self.supertime == 1)
		{
			self.supertime = 0;
			sound (self, CHAN_BODY, "items/damage2.wav", 1, ATTN_NORM);
			sprint(self, 2, "you are now back to your normal self.\n");
			self.skill_combat = self.skill_combat - 10;
			self.skill_doctor = self.skill_doctor - 10;
			self.skill_sneak = self.skill_sneak - 10;
			self.skill_science = self.skill_science - 10;
			self.skill_speech = self.skill_speech - 10;
		}

		local float lgrip, rgrip;


		if (self.health > self.max_health)
			self.health = self.max_health;


		if (self.flash < time || ToIID(self.islot4) == IID_EQUIP_GOGGLES)
		{
			if (ToIID(self.islot4) == IID_EQUIP_GOGGLES)
				stuffcmd(self, "v_cshift 25 0 25 25\n");
			else if (self.ragetime > time && self.rage != IID_CHEM_RADX)
				stuffcmd(self, "v_cshift 75 0 0 75\n");
			else if (self.sneak > 0)
				stuffcmd(self, "v_cshift 0 0 75 75\n");
			else if (self.zoom == 0)
				stuffcmd(self, "v_cshift 0 0 0 0\n");
		}
		self.cycle1 = time + 0.5;
	}

/*
	if (walkmove(0, 0) && self.solid != SOLID_SLIDEBOX)
	{
		self.materialize = 0;
		self.solid = SOLID_SLIDEBOX;
	}
	else
	{
		self.materialize = 5;
		self.solid = SOLID_NOT;
	}*/

	if (self.materialize > 0)
	{
		self.materialize = self.materialize - 1;

		if (self.materialize <= 0)
		{
			if (walkmove(0, 0))
			{
				self.solid = SOLID_SLIDEBOX;
				bprint(2, "turning solid\n");
			}
			else
			{
				self.materialize = 50;
				bprint(2, "stuck in object, returning non-solid\n");
				self.solid = SOLID_NOT;
			}
		}
	}





      self.deathtype = "";



	if (self.cycle2 < time)
	{



		if (self.blurtime > 0 && self.blurtime <= 1)
			stuffcmd(self, "gl_motionblur 0\n");
		else if (self.blurtime > 0)
		{
			stuffcmd(self, "gl_motionblur 0.9\n");
			self.blurtime -= 1;
		}

		if (self.ragetime > 0)
			self.ragetime -= 1;

		if (self.ragetime <= 0)
			self.rage = 0;

			traceline ((self.origin), ((self.origin - (v_right * 24))) + (v_forward * 64), FALSE, self);
			if (trace_fraction < 1 && trace_fraction > 0)
				lgrip = 1;

			traceline ((self.origin), ((self.origin - (v_right * 48))) + (v_forward * 32), FALSE, self);
			if (trace_fraction < 1 && trace_fraction > 0)
				lgrip = 1;

			traceline ((self.origin), ((self.origin + (v_right * 24))) + (v_forward * 64), FALSE, self);
			if (trace_fraction < 1 && trace_fraction > 0)
				rgrip = 1;

			traceline ((self.origin), ((self.origin + (v_right * 48))) + (v_forward * 32), FALSE, self);
			if (trace_fraction < 1 && trace_fraction > 0)
				rgrip = 1;

		if (rgrip == 1 && lgrip == 0)
			self.rage = 777;
		if (rgrip == 0 && lgrip == 1)
			self.rage = 777;

		
		if (self.musictimer == 0)
		{
			if (self.soundmode == 1)
				stuffcmd(self, "play music/exploring\n");
			else if (self.soundmode == 2)
				stuffcmd(self, "play music/vault\n");
			else if (world.model == "maps/r_bunker.bsp")
				stuffcmd(self, "play music/vault\n");
			else if (world.map_obj == OBJ_DEADTOWN)
				stuffcmd(self, "play music/afterglow\n");

			self.musictimer = 1;
		}
		else if (self.musictimer <= 120)
			self.musictimer = self.musictimer + 1;
		else if (self.musictimer > 120)
			self.musictimer = 0;

		if (self.regen > 0 && !self.deadflag)	//don't come back to life.
		{
			self.health = self.health + 3;
			self.regen = self.regen - 1;
		}
		else if (self.rage == IID_CHEM_BESERK && !self.deadflag)	//don't come back to life.
			self.health = self.health + 1;

		if (self.flash < time || ToIID(self.islot4) == IID_EQUIP_GOGGLES)
		{
			local string x;

			if (ToIID(self.islot4) == IID_EQUIP_GOGGLES)
				stuffcmd(self, "v_cshift 25 0 25 25\n");

			if (self.position == 0)
				x = ftos(self.recoil);
			else if (self.position == 1)
				x = ftos(self.recoil/2);
			else if (self.position == 2)
				x = ftos(self.recoil/4);

			if (self.steadyaim == 0 && self.velocity == '0 0 0')
				stuffcmd(self, "v_idlescale 0\n");
			else if (self.recoil > 0)
				stuffcmd(self, strcat("v_idlescale ", x, "\n"));
			else if (self.velocity != '0 0 0')
				stuffcmd(self, "v_idlescale 4\n");

		}
		self.cycle2 = time + 1;
	}


	CheckRules ();
	WaterMove ();



	if (self.deadflag == DEAD_DYING)
		return; // dying, so do nothing

	if (self.button2)
	{
		PlayerJump ();
	}
	else if (self.position > 0)
		self.flags = self.flags | FL_JUMPRELEASED;

// teleporters can force a non-moving pause time        
	if (time < self.pausetime)
		self.velocity = '0 0 0';

	if (!self.gravity)
	{	//climbing gear
		if (self.velocity != '0 0 0' || self.button2)
			self.gravity = 1;
	}
	if (self.equipment_slot)
	{
		if ((ToIID(ItemInSlot(self, self.equipment_slot))) == IID_EQUIP_CLIMBINGGEAR)
		{
			grip = 0;


			traceline ((self.origin), ((self.origin + (v_forward * 48))), FALSE, self);
			if (trace_fraction < 1 && trace_fraction > 0)
				grip = 1;


			if (grip == 1)
			{
				self.attack_finished = time + 0.1;
				self.rtime = time + 0.2;
				self.gravity = 0;
				self.maxspeed = 50;
				self.velocity = v_forward*20 * self.movement_x;
			}
			else
			{
				self.gravity = 1;
			}
		}
		if ((ToIID(ItemInSlot(self, self.equipment_slot))) == IID_EQUIP_HOVERBOOTS && self.button2)
		{
			if (self.attack_finished < time)
			{
				self.velocity = self.velocity + v_up*800*0.4;
				self.attack_finished = time + 0.4;
			}
		}
	}
};
	
/*
================
CheckPowerups

Check for turning off powerups
================
*/
void() CheckPowerups =
{
	local float weap;

	if (self.health <= 0)
		return;

// invisibility
	if (self.equipment_slot)
		if ((ToIID(ItemInSlot(self, self.equipment_slot))) == IID_EQUIP_STEALTHBOY)
			goto beinvis;

	if (self.invisible_finished)
	{
// sound and screen flash when items starts to run out
		if (self.invisible_sound < time)
		{
			sound (self, CHAN_AUTO, "items/inv3.wav", 0.5, ATTN_IDLE);
			self.invisible_sound = time + ((random() * 3) + 1);
		}


		if (self.invisible_finished < time + 3)
		{
			if (self.invisible_time == 1)
			{
				sprint (self, PRINT_HIGH, "Ring of Shadows magic is fading\n");
				stuffcmd (self, "bf\n");
				sound (self, CHAN_AUTO, "items/inv2.wav", 1, ATTN_NORM);
				self.invisible_time = time + 1;
			}
			
			if (self.invisible_time < time)
			{
				self.invisible_time = time + 1;
				stuffcmd (self, "bf\n");
			}
		}

		if (self.invisible_finished < time)
		{       // just stopped
			self.invisible_finished = 0;
			self.invisible_time = 0;
		}
		
:beinvis
		self.items = self.items | IT_INVISIBILITY;
		self.frame = 0;
		self.modelindex = modelindex_sneak;
	}
	else
	{
		self.items = self.items - (self.items & IT_INVISIBILITY);
		if (self.ghost == 1 || self.team == 0)
		{
			self.modelindex = modelindex_gone;
			return;
		}

		weap = ToIID(self.(SlotField(self.current_slot)));

		if (self.position <= 1)
			self.modelindex = modelindex_player;
		if (self.position == 2 && self.velocity == '0 0 0')
			self.modelindex = modelindex_prone;
		if (self.position == 2 && self.velocity != '0 0 0')
			self.modelindex = modelindex_crawl;
		if (self.sneak > 0 && coop == 0)
			self.modelindex = modelindex_gone;
		if (self.sneak > 0 && coop == 1)
			self.modelindex = modelindex_sneak;
		if (IsMelee(weap))
			self.modelindex = modelindex_melee;
		if (self.health <= 0)
			self.modelindex = modelindex_dead;
	}


// invincibility
	if (self.invincible_finished)
	{
// sound and screen flash when items starts to run out
		if (self.invincible_finished < time + 3)
		{
			if (self.invincible_time == 1)
			{
				sprint (self, PRINT_HIGH, "Protection is almost burned out\n");
				stuffcmd (self, "bf\n");
				sound (self, CHAN_AUTO, "items/protect2.wav", 1, ATTN_NORM);
				self.invincible_time = time + 1;
			}
			
			if (self.invincible_time < time)
			{
				self.invincible_time = time + 1;
				stuffcmd (self, "bf\n");
			}
		}
		
		if (self.invincible_finished < time)
		{       // just stopped
			self.items = self.items - IT_INVULNERABILITY;
			self.invincible_time = 0;
			self.invincible_finished = 0;
		}
		if (self.invincible_finished > time)
		{
			self.effects = self.effects | EF_DIMLIGHT;
			self.effects = self.effects | EF_RED;
		}
		else
		{
			self.effects = self.effects - (self.effects & EF_DIMLIGHT);
			self.effects = self.effects - (self.effects & EF_RED);
		}
	}

// super damage
	if (self.super_damage_finished)
	{

// sound and screen flash when items starts to run out

		if (self.super_damage_finished < time + 3)
		{
			if (self.super_time == 1)
			{
				if (deathmatch == 4)
					sprint (self, PRINT_HIGH, "OctaPower is wearing off\n");
				else
					sprint (self, PRINT_HIGH, "Quad Damage is wearing off\n");
				stuffcmd (self, "bf\n");
				sound (self, CHAN_AUTO, "items/damage2.wav", 1, ATTN_NORM);
				self.super_time = time + 1;
			}         
			
			if (self.super_time < time)
			{
				self.super_time = time + 1;
				stuffcmd (self, "bf\n");
			}
		}

		if (self.super_damage_finished < time)
		{       // just stopped
			self.items = self.items - IT_QUAD;
			if (deathmatch == 4)
			{
				self.ammo_cells = 255;
				self.armorvalue = 1;
				self.armortype = 0.8;
				self.health = 100;
			}
			self.super_damage_finished = 0;
			self.super_time = 0;
		}
		if (self.super_damage_finished > time)
		{
			self.effects = self.effects | EF_DIMLIGHT;
			self.effects = self.effects | EF_BLUE;
		}
		else
		{
			self.effects = self.effects - (self.effects & EF_DIMLIGHT);
			self.effects = self.effects - (self.effects & EF_BLUE);
		}
	}       

// suit 
	if (self.radsuit_finished)
	{
		self.air_finished = time + 12;          // don't drown

// sound and screen flash when items starts to run out
		if (self.radsuit_finished < time + 3)
		{
			if (self.rad_time == 1)
			{
				sprint (self, PRINT_HIGH, "Air supply in Biosuit expiring\n");
				stuffcmd (self, "bf\n");
				sound (self, CHAN_AUTO, "items/suit2.wav", 1, ATTN_NORM);
				self.rad_time = time + 1;
			}
			
			if (self.rad_time < time)
			{
				self.rad_time = time + 1;
				stuffcmd (self, "bf\n");
			}
		}

		if (self.radsuit_finished < time)
		{       // just stopped
			self.items = self.items - IT_SUIT;
			self.rad_time = 0;
			self.radsuit_finished = 0;
		}
	}       

};


void() Footstep;

/*
================
PlayerPostThink

Called every frame after physics are run
================
*/
void() PlayerPostThink =
{
//dprint ("post think\n");
	if (self.view_ofs == '0 0 0')
		return;         // intermission or finale
	if (self.deadflag)
		return;

// check to see if player landed and play landing sound 
	if ((self.jump_flag < -50) && (self.flags & FL_ONGROUND) )
	{
		if (self.watertype == CONTENT_WATER)
			sound (self, CHAN_BODY, "player/h2ojump.wav", 1, ATTN_NORM);
		else if (self.jump_flag < -650)
		{
			self.deathtype = "falling";
			T_Damage (self, world, world, 5); 
			sound (self, CHAN_VOICE, "player/land2.wav", 1, ATTN_NORM);
		}
		else
			Footstep();
	}

	self.jump_flag = self.velocity_z;

	if (self.rtime > time)
		self.weaponframe = 3;

	CheckPowerups ();

	W_WeaponFrame ();

};


/*
===========
ClientConnect

called when a player connects to a server
============
*/
void() ClientConnect =
{
	local entity te;
	local float t;

	bprint (PRINT_HIGH, self.netname);
	bprint (PRINT_HIGH, " entered the wasteland\n");

	te = find(world, classname, "player");

/*
	t = 0;

	while (te)
	{
		t = t + 1;

		te = find(te, classname, "player");
	}*/			

/*
	if (infokey(world, "total") == "zero")
	{
		bprint(2, "setting difficulty for one player.\n");
		localcmd("serverinfo total one\n");
	}
	else if (infokey(world, "total") == "one")
	{
		bprint(2, "setting difficulty for two players.\n");
		localcmd("serverinfo total two\n");
	}
	else if (infokey(world, "total") == "two")
	{
		bprint(2, "setting difficulty for three players.\n");
		localcmd("serverinfo total three\n");
	}
	else if (infokey(world, "total") == "three")
	{
		bprint(2, "setting difficulty for four players.\n");
		localcmd("serverinfo total four\n");
	}
	else if (infokey(world, "total") == "four")
	{
		bprint(2, "setting difficulty maxed out.\n");
		localcmd("serverinfo total four\n");
	}
*/
	self.start_time = world.start_time;
	self.map_ent1 = world.map_ent1;
	self.map_ent2 = world.map_ent2;
	self.map_ent3 = world.map_ent3;
	self.map_obj = world.map_obj;
	self.location = world.location;

	stuffcmd(self, "v_damagecshift 1\n");
	stuffcmd(self, "alias +walk impulse 66\n");
	stuffcmd(self, "alias -walk impulse 67\n");
	stuffcmd(self, "alias duck impulse 200\n");
	stuffcmd(self, "alias prone impulse 201\n");
	stuffcmd(self, "alias reload impulse 50\n");
	stuffcmd(self, "alias chem impulse 51\n");
	stuffcmd(self, "alias buy impulse 52\n");
	stuffcmd(self, "alias special impulse 53\n");
	stuffcmd(self, "alias exit impulse 100\n");
	stuffcmd(self, "alias drop impulse 55\n");
	stuffcmd(self, "alias zoom impulse 60\n");
	stuffcmd(self, "alias hostage impulse 65\n");
	stuffcmd(self, "alias perk1 impulse 61\n");
	stuffcmd(self, "alias perk2 impulse 62\n");

	stuffcmd(self, "alias stimpack impulse 225\n");
	stuffcmd(self, "alias radx impulse 228\n");
	stuffcmd(self, "alias equip \"cmd invuse 4\"\n");
	stuffcmd(self, "alias q1 \"cmd invuse 5\"\n");
	stuffcmd(self, "alias q2 \"cmd invuse 11\"\n");
	stuffcmd(self, "alias quick1 \"cmd invuse 5\"\n");
	stuffcmd(self, "alias quick2 \"cmd invuse 11\"\n");

	stuffcmd(self, "alias toolkit \"cmd invswap toolkit 2;impulse 2\"\n");


	stuffcmd(self, "bind 9 impulse 9\n");
	stuffcmd(self, "bind 0 impulse 10\n");
	stuffcmd(self, "bind c equip\n");
	stuffcmd(self, "bind g drop\n");
	stuffcmd(self, "bind e exit\n");
	stuffcmd(self, "bind z special\n");
	stuffcmd(self, "bind r reload\n");
	stuffcmd(self, "bind h hostage\n");
	stuffcmd(self, "bind x stimpack\n");
	stuffcmd(self, "bind b buy\n");
	stuffcmd(self, "bind q perk1\n");
	stuffcmd(self, "bind a perk2\n");
	stuffcmd(self, "bind shift prone\n");
	stuffcmd(self, "bind ctrl duck\n");

	stuffcmd(self, "exec fallout.cfg\n");

// a client connecting during an intermission can cause problems
	if (intermission_running)
		GotoNextMap ();
};


/*
===========
ClientDisconnect

called when a player disconnects from a server
============
*/
void() ClientDisconnect =
{
	local entity te;
	local float t;

/*
	if (infokey(world, "total") == "zero")
	{
		bprint(2, "need players to begin map.\n");
		localcmd("serverinfo total zero\n");
	}
	else if (infokey(world, "total") == "one")
	{
		bprint(2, "need players to begin map.\n");
		localcmd("serverinfo total zero\n");
	}
	else if (infokey(world, "total") == "two")
	{
		bprint(2, "setting difficulty for one player.\n");
		localcmd("serverinfo total one\n");
	}
	else if (infokey(world, "total") == "three")
	{
		bprint(2, "setting difficulty for two players.\n");
		localcmd("serverinfo total two\n");
	}
	else if (infokey(world, "total") == "four")
	{
		bprint(2, "setting difficulty for three players.\n");
		localcmd("serverinfo total three\n");
	}
*/

	// let everyone else know
	bprint (PRINT_HIGH, self.netname);
	bprint (PRINT_HIGH, " left the game.\n");
	if (self.team == 1)
		teamblue = teamblue - 1;
	else if (self.team == 2)
		teamred = teamred - 1;

	self.classname = "disconnected";
	sound (self, CHAN_BODY, "player/tornoff2.wav", 1, ATTN_NONE);
	set_suicide_frame ();
	total_players = total_players - 1;
};

/*
===========
ClientObituary

called when a player dies
============
*/

void(entity targ, entity attacker) ClientObituary =
{
	local   float rnum;
	local   string deathstring, deathstring2;
	local   string  attackerteam, targteam;

	rnum = random();
	//ZOID 12-13-96: self.team doesn't work in QW.  Use keys
	attackerteam = infokey(attacker, "team");
	targteam = infokey(targ, "team");

	if (targ.classname == "player")
	{
		if (coop == 1 && no_connect < 4)
			no_connect = no_connect + 1;

		if (no_connect > 4)
			no_connect = 4;
	}

	if (attacker.classname == "player")
	{
		if (coop == 1 && no_connect < 4)
			no_connect = no_connect + 1;

		if (attacker == targ)
			attacker.kills = attacker.kills - 1;
		else if (attacker.team == targ.team)
			attacker.kills = attacker.kills - 1;
		else if (targ.classname == "player")
			attacker.kills = attacker.kills + 1;
		else if (targ.classname == "monster")
			attacker.kills = attacker.kills + 1;
	}

	if (targ == attacker && targ.deathtype == "suicide")
	{
		bprint(2, attacker.netname);
		bprint(2, " takes the easy way out\n");
		return;
	}

	if (targ == attacker && targ.deathtype == "bleed")
	{
		bprint(2, attacker.netname);
		bprint(2, " bled to death\n");
		return;
	}

	if (targ != attacker && (targ.classname == "monster" || targ.classname == "player") && (attacker.classname == "monster" || attacker.classname == "player"))
	{
			bprint (PRINT_MEDIUM, "[ ");

		if (attacker.critical == 3) //headshot
		{
			bprint (PRINT_MEDIUM, "X ");
			attacker.score = attacker.score + 1;
		}
		if (attacker.critical == 777) //through the wall
		{
			bprint (PRINT_MEDIUM, "% ");
			attacker.score = attacker.score + 5;
		}
		if (attacker.critical == 778) //wall headshot
		{
			bprint (PRINT_MEDIUM, "& ");
			attacker.score = attacker.score + 10;
		}
		if (attacker.velocity_z != 0)
		{
			bprint (PRINT_MEDIUM, "@ ");
			attacker.score = attacker.score + 2;
		}
//		if (attacker.current_slot == 1 && attacker.mag1 == 0)
//			bprint (PRINT_MEDIUM, "! ");

			bprint (PRINT_MEDIUM, "] ");

		bprint(2, attacker.netname);
		bprint(2, " >>> ");
		bprint(2, targ.netname);
		bprint(2, "\n");
		return;
	}

	if (targ != attacker && (targ.classname == "monster" || targ.classname == "player") && (attacker.classname == "robofang" || attacker.classname == "player"))
	{
		bprint (PRINT_MEDIUM, "[;;] ");
		bprint(2, attacker.owner.netname);
		bprint(2, " >>> ");
		bprint(2, targ.netname);
		bprint(2, "\n");
		return;
	}

	if (targ.classname == "player")
	{

		if (deathmatch > 3)	
		{
			if (targ.deathtype == "selfwater")
			{
				bprint (PRINT_MEDIUM, targ.netname);
				bprint (PRINT_MEDIUM," electrocutes himself.\n ");
				targ.frags = targ.frags - 1;
				return;
			}
		}

		if (attacker.classname == "teledeath")
		{
			bprint (PRINT_MEDIUM,targ.netname);
			bprint (PRINT_MEDIUM," was telefragged by ");
			bprint (PRINT_MEDIUM,attacker.owner.netname);
			bprint (PRINT_MEDIUM,"\n");
			logfrag (attacker.owner, targ);

			attacker.owner.frags = attacker.owner.frags + 1;
			return;
		}

		if (attacker.classname == "teledeath2")
		{
			bprint (PRINT_MEDIUM,"Satan's power deflects ");
			bprint (PRINT_MEDIUM,targ.netname);
			bprint (PRINT_MEDIUM,"'s telefrag\n");

			targ.frags = targ.frags - 1;
			logfrag (targ, targ);
			return;
		}

		// double 666 telefrag (can happen often in deathmatch 4)
		if (attacker.classname == "teledeath3") 
		{
			bprint (PRINT_MEDIUM,targ.netname);
			bprint (PRINT_MEDIUM," was telefragged by ");
			bprint (PRINT_MEDIUM,attacker.owner.netname);
			bprint (PRINT_MEDIUM, "'s Satan's power\n");
			targ.frags = targ.frags - 1;
			logfrag (targ, targ);
			return;
		}
	

		if (targ.deathtype == "squish")
		{
			if (teamplay && targteam == attackerteam && attackerteam != "" && targ != attacker)
			{
				logfrag (attacker, attacker);
				attacker.frags = attacker.frags - 1; 
				bprint (PRINT_MEDIUM,attacker.netname);
				bprint (PRINT_MEDIUM," squished a teammate\n");
				return;
			}
			else if (attacker.classname == "player" && attacker != targ)
			{
				bprint (PRINT_MEDIUM, attacker.netname);
				bprint (PRINT_MEDIUM," squishes ");
				bprint (PRINT_MEDIUM,targ.netname);
				bprint (PRINT_MEDIUM,"\n");
				logfrag (attacker, targ);
				attacker.frags = attacker.frags + 1;
				return;
			}
			else
			{
				logfrag (targ, targ);
				targ.frags = targ.frags - 1;            // killed self
				bprint (PRINT_MEDIUM,targ.netname);
				bprint (PRINT_MEDIUM," was squished\n");
				return;
			}
		}

		if (attacker.classname == "player")
		{
			if (targ == attacker)
			{
				// killed self
				logfrag (attacker, attacker);
				attacker.frags = attacker.frags - 1;
				bprint (PRINT_MEDIUM,targ.netname);
				if (targ.deathtype == "grenade")
					bprint (PRINT_MEDIUM," tries to put the pin back in\n");
				else if (targ.deathtype == "rocket")
					bprint (PRINT_MEDIUM," becomes bored with life\n");
				else if (targ.weapon == 64 && targ.waterlevel > 1)
				{
					if (targ.watertype == CONTENT_SLIME)
						bprint (PRINT_MEDIUM," discharges into the slime\n");
					else if (targ.watertype == CONTENT_LAVA)
						bprint (PRINT_MEDIUM," discharges into the lava\n");
					else
						bprint (PRINT_MEDIUM," discharges into the water.\n");
				}
				else
					bprint (PRINT_MEDIUM," becomes bored with life\n");
				return;
			}
			else if ( (teamplay == 2) && (targteam == attackerteam) &&
				(attackerteam != "") )
			{
				if (rnum < 0.25)
					deathstring = " mows down a teammate\n";
				else if (rnum < 0.50)
					deathstring = " checks his glasses\n";
				else if (rnum < 0.75)
					deathstring = " gets a frag for the other team\n";
				else
					deathstring = " loses another friend\n";
				bprint (PRINT_MEDIUM, attacker.netname);
				bprint (PRINT_MEDIUM, deathstring);
				attacker.frags = attacker.frags - 1;
				//ZOID 12-13-96:  killing a teammate logs as suicide
				logfrag (attacker, attacker);
				return;
			}
			else
			{
				logfrag (attacker, targ);
				attacker.frags = attacker.frags + 1;

				rnum = attacker.weapon;
				if (targ.deathtype == "nail")
				{
					deathstring = " was nailed by ";
					deathstring2 = "\n";
				}
				else if (targ.deathtype == "supernail")
				{
					deathstring = " was punctured by ";
					deathstring2 = "\n";
				}
				else if (targ.deathtype == "grenade")
				{
					deathstring = " eats ";
					deathstring2 = "'s pineapple\n";
					if (targ.health < -40)
					{
						deathstring = " was gibbed by ";
						deathstring2 = "'s grenade\n";
					}
				}
				else if (targ.deathtype == "rocket")
				{
					if (attacker.super_damage_finished > 0 && targ.health < -40)
					{
						rnum = random();
						if (rnum < 0.3)
							deathstring = " was brutalized by ";
						else if (rnum < 0.6)
							deathstring = " was smeared by ";
						else
						{
							bprint (PRINT_MEDIUM, attacker.netname);
							bprint (PRINT_MEDIUM, " rips ");
							bprint (PRINT_MEDIUM, targ.netname);
							bprint (PRINT_MEDIUM, " a new one\n");
							return;
						}
						deathstring2 = "'s quad rocket\n";
					}
					else
					{
						deathstring = " rides ";
						deathstring2 = "'s rocket\n";
						if (targ.health < -40)
						{
							deathstring = " was gibbed by ";
							deathstring2 = "'s rocket\n" ;
						}
					}
				}
				else if (rnum == IT_AXE)
				{
					deathstring = " was ax-murdered by ";
					deathstring2 = "\n";
				}
				else if (rnum == IT_SHOTGUN)
				{
					deathstring = " chewed on ";
					deathstring2 = "'s boomstick\n";
				}
				else if (rnum == IT_SUPER_SHOTGUN)
				{
					deathstring = " ate 2 loads of ";
					deathstring2 = "'s buckshot\n";
				}
				else if (rnum == IT_LIGHTNING)
				{
					deathstring = " accepts ";
					if (attacker.waterlevel > 1)
						deathstring2 = "'s discharge\n";
					else
						deathstring2 = "'s shaft\n";
				}
				bprint (PRINT_MEDIUM,targ.netname);
				bprint (PRINT_MEDIUM,deathstring);
				bprint (PRINT_MEDIUM,attacker.netname);
				bprint (PRINT_MEDIUM,deathstring2);
			}
			return;
		}
		else
		{
			logfrag (targ, targ);
			targ.frags = targ.frags - 1;            // killed self
			rnum = targ.watertype;

			bprint (PRINT_MEDIUM,targ.netname);
			if (rnum == -3)
			{
				if (random() < 0.5)
					bprint (PRINT_MEDIUM," sleeps with the fishes\n");
				else
					bprint (PRINT_MEDIUM," sucks it down\n");
				return;
			}
			else if (rnum == -4)
			{
				if (random() < 0.5)
					bprint (PRINT_MEDIUM," gulped a load of slime\n");
				else
					bprint (PRINT_MEDIUM," can't exist on slime alone\n");
				return;
			}
			else if (rnum == -5)
			{
				if (targ.health < -15)
				{
					bprint (PRINT_MEDIUM," burst into flames\n");
					return;
				}
				if (random() < 0.5)
					bprint (PRINT_MEDIUM," turned into hot slag\n");
				else
					bprint (PRINT_MEDIUM," visits the Volcano God\n");
				return;
			}

			if (attacker.classname == "explo_box")
			{
				bprint (PRINT_MEDIUM," blew up\n");
				return;
			}
			if (targ.deathtype == "falling")
			{
				bprint (PRINT_MEDIUM," fell to his death\n");
				return;
			}
			if (targ.deathtype == "nail" || targ.deathtype == "supernail")
			{
				bprint (PRINT_MEDIUM," was spiked\n");
				return;
			}
			if (targ.deathtype == "laser")
			{
				bprint (PRINT_MEDIUM," was zapped\n");
				return;
			}
			if (attacker.classname == "fireball")
			{
				bprint (PRINT_MEDIUM," ate a lavaball\n");
				return;
			}
			if (attacker.classname == "trigger_changelevel")
			{
				bprint (PRINT_MEDIUM," tried to leave\n");
				return;
			}

			bprint (PRINT_MEDIUM," died\n");
		}
	}
};



void() Identify =
{
	local vector source;

	makevectors (self.v_angle);
	source = self.origin + '0 0 0';
	traceline (source, source + v_forward*128, FALSE, self);

	if (trace_ent.netname)
		centerprint (self, trace_ent.netname);
};
