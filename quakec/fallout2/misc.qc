
/*QUAKED info_null (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for spotlights, etc.
*/
void() info_null =
{
	remove(self);
};

/*QUAKED info_notnull (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for lightning.
*/
void() info_notnull =
{
};

//============================================================================

float START_OFF = 1;

void() light_use =
{
	if (self.spawnflags & START_OFF)
	{
		lightstyle(self.style, "m");
		self.spawnflags = self.spawnflags - START_OFF;
	}
	else
	{
		lightstyle(self.style, "a");
		self.spawnflags = self.spawnflags + START_OFF;
	}
};

/*QUAKED light (0 1 0) (-8 -8 -8) (8 8 8) START_OFF
Non-displayed light.
Default light value is 300
Default style is 0
If targeted, it will toggle between on or off.
*/
void() light =
{
	if (!self.targetname)
	{       // inert light
		remove(self);
		return;
	}
	
	if (self.style >= 32)
	{
		self.use = light_use;
		if (self.spawnflags & START_OFF)
			lightstyle(self.style, "a");
		else
			lightstyle(self.style, "m");
	}
};

/*QUAKED light_fluoro (0 1 0) (-8 -8 -8) (8 8 8) START_OFF
Non-displayed light.
Default light value is 300
Default style is 0
If targeted, it will toggle between on or off.
Makes steady fluorescent humming sound
*/
void() light_fluoro =
{
	if (self.style >= 32)
	{
		self.use = light_use;
		if (self.spawnflags & START_OFF)
			lightstyle(self.style, "a");
		else
			lightstyle(self.style, "m");
	}
	
	precache_sound ("ambience/fl_hum1.wav");
	ambientsound (self.origin, "ambience/fl_hum1.wav", 0.5, ATTN_STATIC);
};

/*QUAKED light_fluorospark (0 1 0) (-8 -8 -8) (8 8 8)
Non-displayed light.
Default light value is 300
Default style is 10
Makes sparking, broken fluorescent sound
*/
void() light_fluorospark =
{
	if (!self.style)
		self.style = 10;

	precache_sound ("ambience/buzz1.wav");
	ambientsound (self.origin, "ambience/buzz1.wav", 0.5, ATTN_STATIC);
};

/*QUAKED light_globe (0 1 0) (-8 -8 -8) (8 8 8)
Sphere globe light.
Default light value is 300
Default style is 0
*/
void() light_globe =
{
	precache_model ("progs/s_light.spr");
	setmodel (self, "progs/s_light.spr");
	makestatic (self);
};

void() FireAmbient =
{
	precache_sound ("ambience/fire1.wav");
// attenuate fast
	ambientsound (self.origin, "ambience/fire1.wav", 0.5, ATTN_STATIC);
};

/*QUAKED light_torch_small_walltorch (0 .5 0) (-10 -10 -20) (10 10 20)
Short wall torch
Default light value is 200
Default style is 0
*/
void() light_torch_small_walltorch =
{
	precache_model ("progs/flame.mdl");
	setmodel (self, "progs/flame.mdl");
	FireAmbient ();
	makestatic (self);
};

/*QUAKED light_flame_large_yellow (0 1 0) (-10 -10 -12) (12 12 18)
Large yellow flame ball
*/
void() light_flame_large_yellow =
{
	precache_model ("progs/flame2.mdl");
	setmodel (self, "progs/flame2.mdl");
	self.frame = 1;
	FireAmbient ();
	makestatic (self);
};

/*QUAKED light_flame_small_yellow (0 1 0) (-8 -8 -8) (8 8 8) START_OFF
Small yellow flame ball
*/
void() light_flame_small_yellow =
{
	precache_model ("progs/flame2.mdl");
	setmodel (self, "progs/flame2.mdl");
	FireAmbient ();
	makestatic (self);
};

/*QUAKED light_flame_small_white (0 1 0) (-10 -10 -40) (10 10 40) START_OFF
Small white flame ball
*/
void() light_flame_small_white =
{
	precache_model ("progs/flame2.mdl");
	setmodel (self, "progs/flame2.mdl");
	FireAmbient ();
	makestatic (self);
};

//============================================================================


/*QUAKED misc_fireball (0 .5 .8) (-8 -8 -8) (8 8 8)
Lava Balls
*/

void() fire_fly;
void() fire_touch;
void() misc_fireball =
{
	
	precache_model ("progs/lavaball.mdl");
	self.classname = "fireball";
	self.nextthink = time + (random() * 5);
	self.think = fire_fly;
	if (!self.speed)
		self.speed = 1000;
};

void() fire_fly =
{
local entity    fireball;

	fireball = spawn();
	fireball.solid = SOLID_TRIGGER;
	fireball.movetype = MOVETYPE_TOSS;
	fireball.velocity = '0 0 1000';
	fireball.velocity_x = (random() * 100) - 50;
	fireball.velocity_y = (random() * 100) - 50;
	fireball.velocity_z = self.speed + (random() * 200);
	fireball.classname = "fireball";
	setmodel (fireball, "progs/lavaball.mdl");
	setsize (fireball, '0 0 0', '0 0 0');
	setorigin (fireball, self.origin);
	fireball.nextthink = time + 5;
	fireball.think = SUB_Remove;
	fireball.touch = fire_touch;
	
	self.nextthink = time + (random() * 5) + 3;
	self.think = fire_fly;
};


void() fire_touch =
{
	T_Damage (other, self, self, 20);
	remove(self);
};

//============================================================================


void() barrel_explode =
{
	self.takedamage = DAMAGE_NO;
	self.classname = "explo_box";
	T_RadiusDamage2 (self, self.owner, 65+random()*65, other, 800);
	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_EXPLOSION);
	WriteCoord (MSG_MULTICAST, self.origin_x);
	WriteCoord (MSG_MULTICAST, self.origin_y);
	WriteCoord (MSG_MULTICAST, self.origin_z+32);
	multicast (self.origin, MULTICAST_PHS);
	remove (self);
};



/*QUAKED misc_explobox (0 .5 .8) (0 0 0) (32 32 64)
TESTING THING
*/

void() misc_explobox =
{
	local float     oldz;
	
	self.solid = SOLID_BBOX;
	self.movetype = MOVETYPE_NONE;
	precache_model ("maps/b_explob.bsp");
	setmodel (self, "maps/b_explob.bsp");
	setsize (self, '0 0 0', '32 32 64');
	precache_sound ("weapons/r_exp3.wav");
	self.health = 20;
	self.th_die = barrel_explode;
	self.takedamage = DAMAGE_AIM;

	self.origin_z = self.origin_z + 2;
	oldz = self.origin_z;
	droptofloor();
	if (oldz - self.origin_z > 250)
	{
		dprint ("item fell out of level at ");
		dprint (vtos(self.origin));
		dprint ("\n");
		remove(self);
	}
};


void () hostage_die =
{
	local entity ze;

	self.frame = START_OFF;
	if (self.model == "progs/hosfem.mdl")
		sound (self, CHAN_VOICE, "misc/hosdie2.wav", 1, ATTN_NONE);
	else
		sound (self, CHAN_VOICE, "misc/hosdie1.wav", 1, ATTN_NONE);


		ze = find (world, classname, "player");

		while (ze)
		{
			if (ze.classname == "player")
				ze.score = ze.score - 100;

			ze = find(ze, classname, "player");
		}

	hos_deatha1 ();

	setsize (self, VEC_ORIGIN, VEC_ORIGIN);
};

void () hostage_pain =
{
	if (((random () * SECRET_NO_SHOOT) < SECRET_1ST_DOWN))
	{
		sound (self, CHAN_VOICE, "player/paina.wav", START_OFF, ATTN_NORM);
	}
	else
	{
		sound (self, CHAN_VOICE, "player/painb.wav", START_OFF, ATTN_NORM);
	}
	if (((random () * SECRET_NO_SHOOT) < SECRET_1ST_DOWN))
	{
		sound (self, CHAN_BODY, "player/hit1.wav", START_OFF, ATTN_NORM);
	}
	hos_run1 ();
};

void () hostage_think =
{
	self.nextthink = (time + MULTICAST_PVS_R);
	self.think = hostage_think;
	if ((self.health <= MULTICAST_ALL))
	{
		return;
	}
	if ((self.cnt == MULTICAST_ALL))
	{
		hos_stand1 ();
	}
	if ((self.cnt == START_OFF))
	{
		hos_run1 ();
	}
};

void () sci_think =
{
	self.think = sci_think;
	self.nextthink = (time + 15);

	if (self.health <= 0)
		return;
};

void (vector place) hostage_new =
{
	local entity te;
	local float qq;

	precache_model ("progs/hosfem.mdl");
	precache_model ("progs/hosguy.mdl");
	precache_sound ("misc/hosdie1.wav");
	precache_sound ("misc/hosdie2.wav");
	precache_sound ("misc/hosdown.wav");
	precache_sound ("misc/rescued.wav");

	hostages = hostages + 1;
	setorigin(self, place + '0 0 24');
	hos_stand1 ();
	self.movetype = MOVETYPE_STEP;
	self.velocity = VEC_ORIGIN;
	self.touch = SUB_Null;
	self.classname = "rhostage";
	self.takedamage = DAMAGE_AIM;
	setsize (self, VEC_HULL_MIN, VEC_HULL_MAX);
	self.home = self.origin;
	setmodel (self, "progs/hosfem.mdl");
	self.health = 70;
	self.think = hostage_think;
	self.nextthink = (time + START_OFF);
	self.solid = SOLID_BBOX;
	self.th_die = hostage_die;
	self.th_pain = hostage_pain;
	self.rescued = MULTICAST_ALL;
	self.angles_y = floor ((random () * 360));
	self.netname = "citizen";
};

void () hostage_spawn =
{
	local entity te;
	local float qq;

	precache_model ("progs/hosfem.mdl");
	precache_model ("progs/hosguy.mdl");
	precache_sound ("misc/hosdie1.wav");
	precache_sound ("misc/hosdie2.wav");
	precache_sound ("misc/hosdown.wav");
	precache_sound ("misc/rescued.wav");
	if (((random () * SECRET_NO_SHOOT) <= SILENT))
	{
		return;
	}
	if (((random () * SECRET_NO_SHOOT) <= SECRET_NO_SHOOT))
	{
		te = find (world, classname, "hostage");
		while (te)
		{
			qq = (qq + START_OFF);
			te = find (te, classname, "hostage");
		}
		if ((qq >= MULTICAST_PVS_R))
		{
			remove (self);
			return;
		}
	}
	hos_stand1 ();
	self.movetype = MOVETYPE_STEP;
	self.velocity = VEC_ORIGIN;
	self.touch = SUB_Null;
	self.classname = "hostage";
	self.takedamage = DAMAGE_AIM;
	setsize (self, VEC_HULL_MIN, VEC_HULL_MAX);
	self.home = self.origin;
	if (((random () * SECRET_NO_SHOOT) <= SECRET_1ST_DOWN))
	{
		setmodel (self, "progs/hosfem.mdl");
	}
	else
	{
		setmodel (self, "progs/hosguy.mdl");
	}
	self.health = 70;
	self.think = hostage_think;
	self.nextthink = (time + START_OFF);
	self.solid = SOLID_BBOX;
	self.th_die = hostage_die;
	self.th_pain = hostage_pain;
	self.rescued = MULTICAST_ALL;
	self.angles_y = floor ((random () * 360));
	self.netname = "citizen";
};

void () scientist =
{
	precache_model ("progs/hosguy.mdl");
	precache_sound ("misc/hosdie1.wav");
	precache_sound ("misc/hosdie2.wav");
	hos_stand1 ();
	self.movetype = MOVETYPE_STEP;
	self.velocity = VEC_ORIGIN;
	self.touch = SUB_Null;
	self.classname = "scientist";
	self.takedamage = DAMAGE_AIM;
	setsize (self, VEC_HULL_MIN, VEC_HULL_MAX);
	setmodel (self, "progs/hosguy.mdl");
	self.health = 70;
	self.think = hostage_think;
	self.nextthink = (time + START_OFF);
	self.team = START_OFF;
	self.solid = SOLID_BBOX;
	self.th_die = hostage_die;
	self.th_pain = hostage_pain;
	self.rescued = MULTICAST_ALL;
	self.netname = "scientist";
};


/*QUAKED misc_explobox2 (0 .5 .8) (0 0 0) (32 32 64)
Smaller exploding box, REGISTERED ONLY
*/

void() misc_explobox2 =
{
	local float     oldz;
	
	self.solid = SOLID_BBOX;
	self.movetype = MOVETYPE_NONE;
	precache_model2 ("maps/b_exbox2.bsp");
	setmodel (self, "maps/b_exbox2.bsp");
	setsize (self, '0 0 0', '32 32 32');
	precache_sound ("weapons/r_exp3.wav");
	self.health = 20;
	self.th_die = barrel_explode;
	self.takedamage = DAMAGE_AIM;

	self.origin_z = self.origin_z + 2;
	oldz = self.origin_z;
	droptofloor();
	if (oldz - self.origin_z > 250)
	{
		dprint ("item fell out of level at ");
		dprint (vtos(self.origin));
		dprint ("\n");
		remove(self);
	}
};

//============================================================================

float SPAWNFLAG_SUPERSPIKE      = 1;
float SPAWNFLAG_LASER = 2;

void() Laser_Touch =
{
	local vector org;
	
	if (other == self.owner)
		return;         // don't explode on owner

	if (pointcontents(self.origin) == CONTENT_SKY)
	{
		remove(self);
		return;
	}
	
	sound (self, CHAN_WEAPON, "enforcer/enfstop.wav", 1, ATTN_STATIC);
	org = self.origin - 8*normalize(self.velocity);

	if (other.health)
	{
		SpawnBlood (org, 15);
		other.deathtype = "laser";
		T_Damage (other, self, self.owner, 15);
	}
	else
	{
		WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
		WriteByte (MSG_MULTICAST, TE_GUNSHOT);
		WriteByte (MSG_MULTICAST, 5);
		WriteCoord (MSG_MULTICAST, org_x);
		WriteCoord (MSG_MULTICAST, org_y);
		WriteCoord (MSG_MULTICAST, org_z);
		multicast (org, MULTICAST_PVS);
	}
	
	remove(self);   
};

void(vector org, vector vec) LaunchLaser =
{
	if (self.classname == "monster_enforcer")
		sound (self, CHAN_WEAPON, "enforcer/enfire.wav", 1, ATTN_NORM);

	vec = normalize(vec);
	
	newmis = spawn();
	newmis.owner = self;
	newmis.movetype = MOVETYPE_FLY;
	newmis.solid = SOLID_BBOX;
	newmis.effects = EF_DIMLIGHT;

	setmodel (newmis, "progs/laser.mdl");
	setsize (newmis, '0 0 0', '0 0 0');             

	setorigin (newmis, org);

	newmis.velocity = vec * 600;
	newmis.angles = vectoangles(newmis.velocity);

	newmis.nextthink = time + 5;
	newmis.think = SUB_Remove;
	newmis.touch = Laser_Touch;
};

void() spikeshooter_use =
{
	if (self.spawnflags & SPAWNFLAG_LASER)
	{
		sound (self, CHAN_VOICE, "enforcer/enfire.wav", 1, ATTN_NORM);
		LaunchLaser (self.origin, self.movedir);
	}
	else
	{
		sound (self, CHAN_VOICE, "weapons/spike2.wav", 1, ATTN_NORM);
		launch_spike (self.origin, self.movedir);
		newmis.velocity = self.movedir * 500;
		if (self.spawnflags & SPAWNFLAG_SUPERSPIKE)
			newmis.touch = superspike_touch;
	}
};

void() shooter_think =
{
	spikeshooter_use ();
	self.nextthink = time + self.wait;
	newmis.velocity = self.movedir * 500;
};


/*QUAKED trap_spikeshooter (0 .5 .8) (-8 -8 -8) (8 8 8) superspike laser
When triggered, fires a spike in the direction set in QuakeEd.
Laser is only for REGISTERED.
*/

void() trap_spikeshooter =
{
	SetMovedir ();
	self.use = spikeshooter_use;
	if (self.spawnflags & SPAWNFLAG_LASER)
	{
		precache_model2 ("progs/laser.mdl");
		
		precache_sound2 ("enforcer/enfire.wav");
		precache_sound2 ("enforcer/enfstop.wav");
	}
	else
		precache_sound ("weapons/spike2.wav");
};


/*QUAKED trap_shooter (0 .5 .8) (-8 -8 -8) (8 8 8) superspike laser
Continuously fires spikes.
"wait" time between spike (1.0 default)
"nextthink" delay before firing first spike, so multiple shooters can be stagered.
*/
void() trap_shooter =
{
	trap_spikeshooter ();
	
	if (self.wait == 0)
		self.wait = 1;
	self.nextthink = self.nextthink + self.wait + self.ltime;
	self.think = shooter_think;
};



/*
===============================================================================


===============================================================================
*/


void() make_bubbles;
void() bubble_remove;
void() bubble_bob;

/*QUAKED air_bubbles (0 .5 .8) (-8 -8 -8) (8 8 8)

testing air bubbles
*/

void() air_bubbles =
{
	remove (self);
};

void() make_bubbles =
{
local entity    bubble;

	bubble = spawn();
	setmodel (bubble, "progs/s_bubble.spr");
	setorigin (bubble, self.origin);
	bubble.movetype = MOVETYPE_NOCLIP;
	bubble.solid = SOLID_NOT;
	bubble.velocity = '0 0 15';
	bubble.nextthink = time + 0.5;
	bubble.think = bubble_bob;
	bubble.touch = bubble_remove;
	bubble.classname = "bubble";
	bubble.frame = 0;
	bubble.cnt = 0;
	setsize (bubble, '-8 -8 -8', '8 8 8');
	self.nextthink = time + random() + 0.5;
	self.think = make_bubbles;
};

void() bubble_split =
{
local entity    bubble;
	bubble = spawn();
	setmodel (bubble, "progs/s_bubble.spr");
	setorigin (bubble, self.origin);
	bubble.movetype = MOVETYPE_NOCLIP;
	bubble.solid = SOLID_NOT;
	bubble.velocity = self.velocity;
	bubble.nextthink = time + 0.5;
	bubble.think = bubble_bob;
	bubble.touch = bubble_remove;
	bubble.classname = "bubble";
	bubble.frame = 1;
	bubble.cnt = 10;
	setsize (bubble, '-8 -8 -8', '8 8 8');
	self.frame = 1;
	self.cnt = 10;
	if (self.waterlevel != 3)
		remove (self);
};

void() bubble_remove =
{
	if (other.classname == self.classname)
	{
//              dprint ("bump");
		return;
	}
	remove(self);
};

void() bubble_bob =
{
local float             rnd1, rnd2, rnd3;

	self.cnt = self.cnt + 1;
	if (self.cnt == 4)
		bubble_split();
	if (self.cnt == 20)
		remove(self);

	rnd1 = self.velocity_x + (-10 + (random() * 20));
	rnd2 = self.velocity_y + (-10 + (random() * 20));
	rnd3 = self.velocity_z + 10 + random() * 10;

	if (rnd1 > 10)
		rnd1 = 5;
	if (rnd1 < -10)
		rnd1 = -5;
		
	if (rnd2 > 10)
		rnd2 = 5;
	if (rnd2 < -10)
		rnd2 = -5;
		
	if (rnd3 < 10)
		rnd3 = 15;
	if (rnd3 > 30)
		rnd3 = 25;
	
	self.velocity_x = rnd1;
	self.velocity_y = rnd2;
	self.velocity_z = rnd3;
		
	self.nextthink = time + 0.5;
	self.think = bubble_bob;
};

/*~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>
~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~<~>~*/

/*QUAKED viewthing (0 .5 .8) (-8 -8 -8) (8 8 8)

Just for the debugging level.  Don't use
*/

void() viewthing =

{
	self.movetype = MOVETYPE_NONE;
	self.solid = SOLID_NOT;
	precache_model ("progs/player.mdl");
	setmodel (self, "progs/player.mdl");
};


/*
==============================================================================

SIMPLE BMODELS

==============================================================================
*/

void() func_wall_use =
{       // change to alternate textures
	self.frame = 1 - self.frame;
};

/*QUAKED func_wall (0 .5 .8) ?
This is just a solid wall if not inhibitted
*/
void() func_wall =
{
	self.angles = '0 0 0';
	self.movetype = MOVETYPE_PUSH;  // so it doesn't get pushed by anything
	self.solid = SOLID_BSP;
	self.use = func_wall_use;
	setmodel (self, self.model);
};


/*QUAKED func_illusionary (0 .5 .8) ?
A simple entity that looks solid but lets you walk through it.
*/
void() func_illusionary =

{
	self.angles = '0 0 0';
	self.movetype = MOVETYPE_NONE;
	self.solid = SOLID_NOT;
	setmodel (self, self.model);
	makestatic (self);
};

/*QUAKED func_episodegate (0 .5 .8) ? E1 E2 E3 E4
This bmodel will appear if the episode has allready been completed, so players can't reenter it.
*/
void() func_episodegate =

{
	if (!(serverflags & self.spawnflags))
		return;                 // can still enter episode

	self.angles = '0 0 0';
	self.movetype = MOVETYPE_PUSH;  // so it doesn't get pushed by anything
	self.solid = SOLID_BSP;
	self.use = func_wall_use;
	setmodel (self, self.model);
};

/*QUAKED func_bossgate (0 .5 .8) ?
This bmodel appears unless players have all of the episode sigils.
*/
void() func_bossgate =

{
	if ( (serverflags & 15) == 15)
		return;         // all episodes completed
	self.angles = '0 0 0';
	self.movetype = MOVETYPE_PUSH;  // so it doesn't get pushed by anything
	self.solid = SOLID_BSP;
	self.use = func_wall_use;
	setmodel (self, self.model);
};

//============================================================================
/*QUAKED ambient_suck_wind (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void() ambient_suck_wind =
{
	precache_sound ("ambience/suck1.wav");
	ambientsound (self.origin, "ambience/suck1.wav", 1, ATTN_STATIC);
};

/*QUAKED ambient_drone (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void() ambient_drone =
{
	precache_sound ("ambience/drone6.wav");
	ambientsound (self.origin, "ambience/drone6.wav", 0.5, ATTN_STATIC);
};

/*QUAKED ambient_flouro_buzz (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void() ambient_flouro_buzz =
{
	precache_sound ("ambience/buzz1.wav");
	ambientsound (self.origin, "ambience/buzz1.wav", 1, ATTN_STATIC);
};
/*QUAKED ambient_drip (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void() ambient_drip =
{
	precache_sound ("ambience/drip1.wav");
	ambientsound (self.origin, "ambience/drip1.wav", 0.5, ATTN_STATIC);
};
/*QUAKED ambient_comp_hum (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void() ambient_comp_hum =
{
	precache_sound ("ambience/comp1.wav");
	ambientsound (self.origin, "ambience/comp1.wav", 1, ATTN_STATIC);
};
/*QUAKED ambient_thunder (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void() ambient_thunder =
{
	precache_sound ("ambience/thunder1.wav");
	ambientsound (self.origin, "ambience/thunder1.wav", 0.5, ATTN_STATIC);
};
/*QUAKED ambient_light_buzz (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void() ambient_light_buzz =
{
	precache_sound ("ambience/fl_hum1.wav");
	ambientsound (self.origin, "ambience/fl_hum1.wav", 0.5, ATTN_STATIC);
};
/*QUAKED ambient_swamp1 (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void() ambient_swamp1 =
{
	precache_sound ("ambience/swamp1.wav");
	ambientsound (self.origin, "ambience/swamp1.wav", 0.5, ATTN_STATIC);
};
/*QUAKED ambient_swamp2 (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void() ambient_swamp2 =
{
	precache_sound ("ambience/swamp2.wav");
	ambientsound (self.origin, "ambience/swamp2.wav", 0.5, ATTN_STATIC);
};

//============================================================================

void() noise_think =
{
	self.nextthink = time + 0.5;
	sound (self, 1, "enforcer/enfire.wav", 1, ATTN_NORM);
	sound (self, 2, "enforcer/enfstop.wav", 1, ATTN_NORM);
	sound (self, 3, "enforcer/sight1.wav", 1, ATTN_NORM);
	sound (self, 4, "enforcer/sight2.wav", 1, ATTN_NORM);
	sound (self, 5, "enforcer/sight3.wav", 1, ATTN_NORM);
	sound (self, 6, "enforcer/sight4.wav", 1, ATTN_NORM);
	sound (self, 7, "enforcer/pain1.wav", 1, ATTN_NORM);
};

/*QUAKED misc_noisemaker (1 0.5 0) (-10 -10 -10) (10 10 10)

For optimzation testing, starts a lot of sounds.
*/

void() misc_noisemaker =

{
	precache_sound2 ("enforcer/enfire.wav");
	precache_sound2 ("enforcer/enfstop.wav");
	precache_sound2 ("enforcer/sight1.wav");
	precache_sound2 ("enforcer/sight2.wav");
	precache_sound2 ("enforcer/sight3.wav");
	precache_sound2 ("enforcer/sight4.wav");
	precache_sound2 ("enforcer/pain1.wav");
	precache_sound2 ("enforcer/pain2.wav");
	precache_sound2 ("enforcer/death1.wav");
	precache_sound2 ("enforcer/idle1.wav");

	self.nextthink = time + 0.1 + random();
	self.think = noise_think;
};


void() brotherhood_merchant =
{
	setorigin(self, self.origin + '0 0 64');
	self.movetype = MOVETYPE_BOUNCE;
	self.velocity_z = -200;

	setmodel (self, "progs/enforcer.mdl");
	self.touch = SUB_Null;
	self.solid = SOLID_SLIDEBOX;
	setsize (self, '-16 -16 -24', '16 16 32');
	self.takedamage = DAMAGE_NO;
	self.classname = "merchant";
	self.think = shop_face;
	self.nextthink = time + 0.5;


	if (random()*100 < 33)
	{
		self.xslot1 = SlotVal(IID_WP_GREASEGUN, 1);
		self.xslot6 = SlotVal(IID_AM_45ACP, 30);
	}
	else if (random()*100 < 33)
	{
		self.xslot1 = SlotVal(IID_WP_USP, 1);
		self.xslot6 = SlotVal(IID_AM_45ACP, 30);
	}
	else
	{
		self.xslot1 = SlotVal(IID_WP_GLOCK, 1);
		self.xslot6 = SlotVal(IID_AM_10MM, 30);
	}

	if (random()*100 < 33)
	{
		self.xslot2 = SlotVal(IID_WP_DEAGLE, 1);
		self.xslot7 = SlotVal(IID_AM_44MAGNUM, 30);
	}
	else if (random()*100 < 33)
	{
		self.xslot2 = SlotVal(IID_WP_NEEDLER, 1);
		self.xslot7 = SlotVal(IID_AM_NEEDLER, 30);
	}
	else
	{
		self.xslot2 = SlotVal(IID_WP_PIPERIFLE, 1);
		self.xslot7 = SlotVal(IID_AM_44MAGNUM, 30);
	}

	if (random()*100 < 33)
	{
		self.xslot3 = SlotVal(IID_WP_WINCHESTER, 1);
		self.xslot8 = SlotVal(IID_AM_12GAUGESHELLS, 30);
	}
	else if (random()*100 < 33)
	{
		self.xslot3 = SlotVal(IID_WP_RANGEMASTER, 1);
		self.xslot8 = SlotVal(IID_AM_556MM, 30);
	}
	else
	{
		self.xslot3 = SlotVal(IID_WP_DKS1, 1);
		self.xslot8 = SlotVal(IID_AM_762MM, 30);
	}

	if (random()*100 < 33)
	{
		self.xslot4 = SlotVal(IID_WP_WINCHESTER, 1);
		self.xslot9 = SlotVal(IID_AM_12GAUGESHELLS, 30);
	}
	else if (random()*100 < 33)
	{
		self.xslot4 = SlotVal(IID_WP_RANGEMASTER, 1);
		self.xslot9 = SlotVal(IID_AM_556MM, 30);
	}
	else
	{
		self.xslot4 = SlotVal(IID_WP_DKS1, 1);
		self.xslot9 = SlotVal(IID_AM_762MM, 30);
	}

	if (random()*100 < 33)
	{
		self.xslot5 = SlotVal(IID_WP_AK112, 1);
		self.xslot10 = SlotVal(IID_AM_5MMHIGHVEL, 30);
	}
	else if (random()*100 < 33)
	{
		self.xslot5 = SlotVal(IID_WP_MOSSBERG, 1);
		self.xslot10 = SlotVal(IID_AM_12GAUGESHELLS, 30);
	}
	else
	{
		self.xslot5 = SlotVal(IID_WP_MP9, 1);
		self.xslot10 = SlotVal(IID_AM_10MM, 30);
	}


	if (random()*100 < 50)
		self.xslot11 = SlotVal(IID_ARM_LEATHER, 1);
	else
		self.xslot11 = SlotVal(IID_ARM_VANDAL, 1);

	if (random()*100 < 50)
		self.xslot12 = SlotVal(IID_ARM_METAL, 1);
	else
		self.xslot12 = SlotVal(IID_ARM_TESLA, 1);

	if (random()*100 < 75)
		self.xslot13 = SlotVal(IID_ARM_COMBAT, 1);
	else
		self.xslot13 = SlotVal(IID_ARM_SEVA, 1);



	self.xslot14 = SlotVal(IID_CHEM_STIMPACK, 1);


	self.xslot15 = SlotVal(IID_CHEM_MEDICALBAG, 25);


	self.xslot16 = SlotVal(IID_WP_ACR, 1);
	self.xslot17 = SlotVal(IID_AM_762MM, 30);


	self.xslot18 = SlotVal(IID_WP_ALIENBLASTER, 1);
	self.xslot19 = SlotVal(IID_AM_ENERGYCELL, 30);

	if (random()*100 < 20)
		self.xslot20 = SlotVal(IID_AM_ENERGYCELL, 30);
	else if (random()*100 < 20)
		self.xslot20 = SlotVal(IID_EQUIP_BELTPOUCH, 1);
	else if (random()*100 < 20)
		self.xslot20 = SlotVal(IID_EQUIP_STEALTHBOY, 1);
	else if (random()*100 < 20)
		self.xslot20 = SlotVal(IID_EQUIP_CLIMBINGGEAR, 1);
	else
		self.xslot20 = SlotVal(IID_EQUIP_TOOLKIT, 1);
};

void() brotherhood_sergeant =
{
	setorigin(self, self.origin + '0 0 64');
	self.movetype = MOVETYPE_BOUNCE;
	self.velocity_z = -200;

	setmodel (self, "progs/enforcer.mdl");
	self.touch = SUB_Null;
	self.solid = SOLID_SLIDEBOX;
	setsize (self, '-16 -16 -24', '16 16 32');
	self.takedamage = DAMAGE_NO;
};

void() brotherhood_soldier =
{
	setorigin(self, self.origin + '0 0 64');
	self.movetype = MOVETYPE_BOUNCE;
	self.velocity_z = -200;

	setmodel (self, "progs/enforcer.mdl");
	self.touch = SUB_Null;
	self.solid = SOLID_SLIDEBOX;
	setsize (self, '-16 -16 -24', '16 16 32');
	self.takedamage = DAMAGE_NO;
};

void() brotherhood_guard =
{
	precache_model ("progs/enforcer.mdl");
	precache_sound ("enforcer/sight1.wav");
	precache_sound ("enforcer/sight2.wav");
	precache_sound ("enforcer/sight3.wav");
	precache_sound ("enforcer/sight4.wav");

	setorigin(self, self.origin + '0 0 64');
	self.movetype = MOVETYPE_BOUNCE;
	self.velocity_z = -200;

	setmodel (self, "progs/enforcer.mdl");
	self.touch = SUB_Null;
	self.solid = SOLID_SLIDEBOX;
	setsize (self, '-16 -16 -24', '16 16 32');
	self.takedamage = DAMAGE_NO;
};

void() brotherhood_doctor =
{
	setorigin(self, self.origin + '0 0 64');
	self.movetype = MOVETYPE_BOUNCE;
	self.velocity_z = -200;

	setmodel (self, "progs/enforcer.mdl");
	self.touch = SUB_Null;
	self.solid = SOLID_SLIDEBOX;
	setsize (self, '-16 -16 -24', '16 16 32');
	self.takedamage = DAMAGE_NO;
	self.classname = "merchant";
	self.think = shop_face;
	self.nextthink = time + 0.5;

	self.xslot1 = SlotVal(IID_WP_KNIFE, 1);
	self.xslot2 = SlotVal(IID_WP_NEEDLER, 1);
	self.xslot3 = SlotVal(IID_ARM_LEATHER, 1);
	self.xslot16 = SlotVal(IID_CHEM_STIMPACK, 1);
	self.xslot17 = SlotVal(IID_CHEM_MEDICALBAG, 25);
	self.xslot18 = SlotVal(IID_CHEM_SUPERSTIM, 1);
	self.xslot19 = SlotVal(IID_CHEM_ADRENALINE, 1);
	self.xslot20 = SlotVal(IID_CHEM_PSYCHO, 1);
};


void() computer_noise =
{

	if (random()<0.10)
		sound (self, CHAN_WEAPON, "effects/beep1.wav", 1, ATTN_IDLE);
	else if (random()<0.10)
		sound (self, CHAN_WEAPON, "effects/beep2.wav", 1, ATTN_IDLE);
	else if (random()<0.10)
		sound (self, CHAN_WEAPON, "effects/beep3.wav", 1, ATTN_IDLE);
	else if (random()<0.10)
		sound (self, CHAN_WEAPON, "effects/beep4.wav", 1, ATTN_IDLE);

	self.nextthink = time + 5;
};

void() misc_computer =
{
	precache_sound ("effects/beep1.wav");
	precache_sound ("effects/beep2.wav");
	precache_sound ("effects/beep3.wav");
	precache_sound ("effects/beep4.wav");

	self.nextthink = time + 5;
	self.think = computer_noise;
};

void() medical_noise =
{

	sound (self, CHAN_WEAPON, "effects/beep5.wav", 1, ATTN_IDLE);

	self.nextthink = time + 2.50;
};

void() misc_medical =
{
	precache_sound ("effects/beep5.wav");

	self.nextthink = time + 2.50;
	self.think = medical_noise;
};

void() radio_noise =
{

	if (random()<0.10)
		sound (self, CHAN_WEAPON, "effects/radio1.wav", 1, ATTN_NORM);
	else if (random()<0.10)
		sound (self, CHAN_WEAPON, "effects/radio2.wav", 1, ATTN_NORM);
	else if (random()<0.10)
		sound (self, CHAN_WEAPON, "effects/radio3.wav", 1, ATTN_NORM);
	else if (random()<0.10)
		sound (self, CHAN_WEAPON, "effects/radio4.wav", 1, ATTN_NORM);

	self.nextthink = time + 4;
};

void() misc_radio =
{
	self.nextthink = time + 5;
	self.think = radio_noise;
};

void() brotherhood_wounded =
{
	remove(self);
};

void() func_tank =
{
	precache_model ("progs/x_tank.mdl");
	setmodel (self, "progs/x_tank.mdl");
	setorigin(self, self.origin + '0 0 64');
	self.movetype = MOVETYPE_BOUNCE;
	self.velocity_z = -200;

	self.touch = SUB_Null;
	self.solid = SOLID_SLIDEBOX;
	setsize (self, '-64 -64 -32', '64 64 32');
	self.takedamage = DAMAGE_NO;
};


