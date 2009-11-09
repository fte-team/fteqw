#define mag1 currentammo
#define maxmag1 cnt

void () OgreFirePlasma =
{

	local float x;
	local vector dist, vec;

	newmis = spawn ();
	newmis.owner = self;
	newmis.movetype = MOVETYPE_FLYMISSILE;
	newmis.solid = SOLID_BBOX;
	newmis.classname = "grenade";

	newmis.skin = 0;
	makevectors (self.angles);
	sound (self, CHAN_WEAPON, "weapons/energy2.wav", PLAT_LOW_TRIGGER, ATTN_NORM);

	newmis.touch = PlasmaBolt;
	newmis.velocity = v_forward * 1200;
	newmis.angles = vectoangles (newmis.velocity);
	newmis.touch = PlasmaBolt;
	newmis.voided = 0;
	

	makevectors (self.enemy.angles);	
	dist = self.enemy.origin - x*self.movedir;

	vec = normalize(dist - self.origin);
	sound (self, CHAN_WEAPON, "wizard/wattack.wav", 1, ATTN_NORM);
	launch_spike (self.origin, vec);
	newmis.velocity = vec*600;
	newmis.owner = self.owner;
	newmis.classname = "wizspike";
	newmis.touch = wizspike_touch;
	setmodel (newmis, "progs/w_spike.mdl");
	setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);		


	newmis.nextthink = time + 5;
	newmis.think = SUB_Remove;
	newmis.nextthink = (time + 1.5);

	newmis.effects = EF_BLUE;
	setmodel (newmis, "progs/plasma.mdl");
	setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);
	setorigin (newmis, ((self.origin + (v_right * TE_BLOOD)) + (v_up * TE_BLOOD)));
};


void () ogre_load1 = [ 136, ogre_load2 ]
{
};
void () ogre_load2 = [ 136, ogre_load3 ]
{
};
void () ogre_load3 = [ 137, ogre_load4 ]
{
};
void () ogre_load4 = [ 137, ogre_load5 ]
{
};
void () ogre_load5 = [ 138, ogre_load6 ]
{
};
void () ogre_load6 = [ 138, ogre_load7 ]
{
};
void () ogre_load7 = [ 139, ogre_load8 ]
{
};
void () ogre_load8 = [ 139, ogre_load9 ]
{
};
void () ogre_load9 = [ 140, ogre_load10 ]
{
};
void () ogre_load10 = [ 140, ogre_load11 ]
{
};
void () ogre_load11 = [ 141, ogre_load12 ]
{
};
void () ogre_load12 = [ 141, ogre_load13 ]
{
};
void () ogre_load13 = [ 142, ogre_load14 ]
{
};
void () ogre_load14 = [ 142, ogre_load16 ]
{
};
void () ogre_load15 = [ 143, ogre_load16 ]
{
};
void () ogre_load16 = [ 143, ogre_run1 ]
{
};

//SMG
void (float tmp, float dam) ogre_smg =
{
	local vector src;
	local vector dir;
	local vector direction;
	local entity en;
	local vector org;

	if (self.mag1 <= 0)
	{
		sound (self, CHAN_WEAPON, "misc/greload.wav", PLAT_LOW_TRIGGER, ATTN_NORM);
		ogre_load1();
		self.mag1 = 60;
		return;
	}

	muzzleflash ();

	self.weapon = 5;

	sound (self, CHAN_WEAPON, "weapons/mp7.wav", 1, ATTN_NORM);


	tmp = 350;


	dam = 15;

	if (self.enemy.sneak == 1)
		tmp = tmp * 2;

	self.mag1 = self.mag1 - 1;

	makevectors (self.angles);

	src = self.origin + v_forward*10;
	src_z = self.absmin_z + self.size_z * 0.7;

	en = self.enemy;
	
	dir = en.origin - en.velocity*0.2;
	if (en.position == 1)
		dir = dir - '0 0 16';
	if (en.position == 2)
		dir = dir - '0 0 32';
	dir = normalize (dir - self.origin);

	direction = dir;


	traceline (src, src + direction*2000 + v_right*crandom()*tmp + v_up*crandom()*tmp, FALSE, self);

	if (trace_fraction == PLAT_LOW_TRIGGER)
		return;

	if (trace_ent.takedamage)
	{
		SpawnBlood (org, PLAT_LOW_TRIGGER);
		dam = 1 + random()*dam + random()*dam;
		dam = dam * (1 - (trace_fraction));
		T_Damage (trace_ent, self, self, dam);
	}
	else
		bullet_hole (trace_endpos);
};

void () OgreGrenadeExplode =
{
	T_RadiusDamage (self, self.owner, (IDLE2A + (random () * TE_BLOOD)), world, "");
	sound (self, CHAN_VOICE, "weapons/r_exp3.wav", PLAT_LOW_TRIGGER, ATTN_NORM);
	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_EXPLOSION);
	WriteCoord (MSG_BROADCAST, self.origin_x);
	WriteCoord (MSG_BROADCAST, self.origin_y);
	WriteCoord (MSG_BROADCAST, self.origin_z);
	self.velocity = VEC_ORIGIN;
	self.touch = SUB_Null;
	self.solid = SOLID_NOT;
	remove (self);
};

void () LobAGrenade =
{
	newmis = spawn ();
	newmis.owner = self;
	newmis.movetype = MOVETYPE_BOUNCE;
	newmis.solid = SOLID_BBOX;
	newmis.classname = "grenade";

	newmis.skin = 0;
	makevectors (self.angles);

	if (random() < 0.5)
	{
		newmis.think = StunExplode;
		newmis.velocity = v_forward * 600;
		newmis.velocity_z = (newmis.velocity_z + 200);
		newmis.angles = vectoangles (newmis.velocity);
		newmis.avelocity_x = (random () * 200);
		newmis.avelocity_y = (random () * 200);
		newmis.avelocity_z = (random () * 200);
		newmis.touch = HandGrenBounce;
		newmis.nextthink = (time + 1.5);
	}
	else
	{
		newmis.think = FlashExplode;
		newmis.velocity = v_forward * 80;
		newmis.velocity_z = (newmis.velocity_z + 20);
		newmis.angles = vectoangles (newmis.velocity);
		newmis.avelocity_x = (random () * 30);
		newmis.avelocity_y = (random () * 30);
		newmis.avelocity_z = (random () * 30);
		newmis.touch = HandGrenBounce;
		newmis.nextthink = (time + 3);
	}

	newmis.frame = 1;
	setmodel (newmis, "progs/handgren.mdl");
	setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);
	setorigin (newmis, ((self.origin + (v_right * TE_BLOOD)) + (v_up * TE_BLOOD)));
};




void (float side) chainsaw =
{
	local vector delta;
	local float ldmg;

	if (!self.enemy)
		return;

	if (!CanDamage (self.enemy, self))
		return;

	ai_charge (TE_LAVASPLASH);
	delta = (self.enemy.origin - self.origin);
	if ((vlen (delta) > 100))
		return;

	ldmg = (6 + (random()*6));
	T_Damage (self.enemy, self, self, ldmg);
	T_Damage (self.enemy, self, self, ldmg);
};

void () ogre_stand1 = [ 0, ogre_stand2 ]
{
	ai_stand ();
};

void () ogre_stand2 = [ 1, ogre_stand3 ]
{
	ai_stand ();
};

void () ogre_stand3 = [ 2, ogre_stand4 ]
{
	ai_stand ();
};

void () ogre_stand4 = [ 3, ogre_stand5 ]
{
	ai_stand ();
};

void () ogre_stand5 = [ 4, ogre_stand6 ]
{
	if ((random () < 0.2))
	{
		sound (self, CHAN_VOICE, "ogre/ogidle.wav", PLAT_LOW_TRIGGER, ATTN_IDLE);
	}
	ai_stand ();
};

void () ogre_stand6 = [ 5, ogre_stand7 ]
{
	ai_stand ();
};

void () ogre_stand7 = [ 6, ogre_stand8 ]
{
	ai_stand ();
};

void () ogre_stand8 = [ 7, ogre_stand9 ]
{
	ai_stand ();
};

void () ogre_stand9 = [ 8, ogre_stand1 ]
{
	ai_stand ();
};

void () ogre_walk1 = [ 9, ogre_walk2 ]
{
	ai_walk (AS_MELEE);
};

void () ogre_walk2 = [ 10, ogre_walk3 ]
{
	ai_walk (SILENT);
};

void () ogre_walk3 = [ 11, ogre_walk4 ]
{
	ai_walk (SILENT);
	if ((random () < 0.2))
	{
		sound (self, CHAN_VOICE, "ogre/ogidle.wav", PLAT_LOW_TRIGGER, ATTN_IDLE);
	}
};

void () ogre_walk4 = [ 12, ogre_walk5 ]
{
	ai_walk (SILENT);
};

void () ogre_walk5 = [ 13, ogre_walk6 ]
{
	ai_walk (SILENT);
};

void () ogre_walk6 = [ 14, ogre_walk7 ]
{
	ai_walk (MULTICAST_PVS_R);
	if ((random () < 0.1))
	{
		sound (self, CHAN_VOICE, "ogre/ogdrag.wav", PLAT_LOW_TRIGGER, ATTN_IDLE);
	}
};

void () ogre_walk7 = [ 15, ogre_walk8 ]
{
	ai_walk (AS_MELEE);
};

void () ogre_walk8 = [ 16, ogre_walk9 ]
{
	ai_walk (SILENT);
};

void () ogre_walk9 = [ 17, ogre_walk10 ]
{
	ai_walk (AS_MELEE);
};

void () ogre_walk10 = [ 18, ogre_walk11 ]
{
	ai_walk (PLAT_LOW_TRIGGER);
};

void () ogre_walk11 = [ 19, ogre_walk12 ]
{
	ai_walk (SILENT);
};

void () ogre_walk12 = [ 20, ogre_walk13 ]
{
	ai_walk (AS_MELEE);
};

void () ogre_walk13 = [ 21, ogre_walk14 ]
{
	ai_walk (AS_MELEE);
};

void () ogre_walk14 = [ 22, ogre_walk15 ]
{
	ai_walk (AS_MELEE);
};

void () ogre_walk15 = [ 23, ogre_walk16 ]
{
	ai_walk (AS_MELEE);
};

void () ogre_walk16 = [ 24, ogre_walk1 ]
{
	ai_walk (SECRET_1ST_DOWN);
};

void () ogre_run1 = [ 25, ogre_run2 ]
{
	ai_run (TE_LIGHTNING3+2);
	if ((random () < 0.2))
	{
		sound (self, CHAN_VOICE, "ogre/ogidle2.wav", PLAT_LOW_TRIGGER, ATTN_IDLE);
	}
};

void () ogre_run2 = [ 26, ogre_run3 ]
{
	ai_run (TE_BLOOD+2);
};

void () ogre_run3 = [ 27, ogre_run4 ]
{
	ai_run (SECRET_NO_SHOOT+2);
};

void () ogre_run4 = [ 28, ogre_run5 ]
{
	ai_run (IDLE10A);
};

void () ogre_run5 = [ 29, ogre_run6 ]
{
	ai_run (SECRET_YES_SHOOT+2);
};

void () ogre_run6 = [ 30, ogre_run7 ]
{
	ai_run (SECRET_1ST_DOWN+2);
};

void () ogre_run7 = [ 31, ogre_run8 ]
{
	ai_run (TE_LIGHTNINGBLOOD+2);
};

void () ogre_run8 = [ 32, ogre_run1 ]
{
	ai_run (DRAW2+2);
};

void () ogre_swing1 = [ 33, ogre_swing2 ]
{
	ai_charge (TE_TELEPORT);
	sound (self, CHAN_WEAPON, "ogre/ogsawatk.wav", PLAT_LOW_TRIGGER, ATTN_NORM);
};

void () ogre_swing2 = [ 34, ogre_swing3 ]
{
	ai_charge (PLAT_LOW_TRIGGER);
};

void () ogre_swing3 = [ 35, ogre_swing4 ]
{
	ai_charge (SECRET_1ST_DOWN);
};

void () ogre_swing4 = [ 36, ogre_swing5 ]
{
	ai_charge (TE_LIGHTNINGBLOOD);
};

void () ogre_swing5 = [ 37, ogre_swing6 ]
{
	ai_charge (TE_LIGHTNING3);
	chainsaw (MULTICAST_ALL);
	self.angles_y = (self.angles_y + (random () * DRAW3));
};

void () ogre_swing6 = [ 38, ogre_swing7 ]
{
};

void () ogre_swing7 = [ 39, ogre_swing8 ]
{
};

void () ogre_swing8 = [ 40, ogre_swing9 ]
{
	chainsaw (MULTICAST_ALL);
	self.angles_y = (self.angles_y + (random () * DRAW3));
};

void () ogre_swing9 = [ 41, ogre_swing10 ]
{
};

void () ogre_swing10 = [ 42, ogre_swing11 ]
{
};

void () ogre_swing11 = [ 43, ogre_swing12 ]
{
};

void () ogre_swing12 = [ 44, ogre_swing13 ]
{
	ai_charge (AS_MELEE);
};

void () ogre_swing13 = [ 45, ogre_swing14 ]
{
	ai_charge (SECRET_NO_SHOOT);
};

void () ogre_swing14 = [ 46, ogre_run1 ]
{
	ai_charge (TE_LIGHTNING3);
};

void () ogre_smash1 = [ 47, ogre_smash2 ]
{
	ai_charge (TE_LIGHTNING2);
	sound (self, CHAN_WEAPON, "ogre/ogsawatk.wav", PLAT_LOW_TRIGGER, ATTN_NORM);
};

void () ogre_smash2 = [ 48, ogre_smash3 ]
{
	ai_charge (MULTICAST_ALL);
};

void () ogre_smash3 = [ 49, ogre_smash4 ]
{
	ai_charge (MULTICAST_ALL);
};

void () ogre_smash4 = [ 50, ogre_smash5 ]
{
	ai_charge (PLAT_LOW_TRIGGER);
};

void () ogre_smash5 = [ 51, ogre_smash6 ]
{
	ai_charge (SECRET_1ST_DOWN);
};

void () ogre_smash6 = [ 52, ogre_smash7 ]
{
	ai_charge (SECRET_1ST_DOWN);
	chainsaw (MULTICAST_ALL);
};

void () ogre_smash7 = [ 53, ogre_smash8 ]
{
	ai_charge (SECRET_1ST_DOWN);
	chainsaw (MULTICAST_ALL);
};

void () ogre_smash8 = [ 54, ogre_smash9 ]
{
	ai_charge (TE_LAVASPLASH);
	chainsaw (MULTICAST_ALL);
};

void () ogre_smash9 = [ 55, ogre_smash10 ]
{
	ai_charge (TE_LIGHTNINGBLOOD);
	chainsaw (MULTICAST_ALL);
};

void () ogre_smash10 = [ 56, ogre_smash11 ]
{
	chainsaw (PLAT_LOW_TRIGGER);
};

void () ogre_smash11 = [ 57, ogre_smash12 ]
{
	ai_charge (SILENT);
	chainsaw (MULTICAST_ALL);
	self.nextthink = (self.nextthink + (random () * 0.2));
};

void () ogre_smash12 = [ 58, ogre_smash13 ]
{
	ai_charge (0);
};

void () ogre_smash13 = [ 59, ogre_smash14 ]
{
	ai_charge (SECRET_1ST_DOWN);
};

void () ogre_smash14 = [ 60, ogre_run1 ]
{
	ai_charge (TE_BLOOD);
};

void () ogre_nail1 = [ 61, ogre_nail2 ]
{
	ai_face ();
};

void () ogre_nail2 = [ 62, ogre_nail3 ]
{
	ai_face ();
};

void () ogre_nail3 = [ 62, ogre_nail4 ]
{
	OgreFirePlasma();
	ai_face ();
};

void () ogre_nail4 = [ 63, ogre_nail5 ]
{
	ai_face ();
};

void () ogre_nail5 = [ 64, ogre_nail6 ]
{
	ai_face ();
};

void () ogre_nail6 = [ 65, ogre_nail7 ]
{
	ai_face ();
};

void () ogre_nail7 = [ 66, ogre_run1 ]
{
	ai_face ();
};

void () ogre_naila = [ 61, ogre_nailb ]
{
	ai_face ();
};

void () ogre_nailb = [ 62, ogre_nailc ]
{
	ai_face ();
};

void () ogre_nailc = [ 62, ogre_naild ]
{
	ogre_smg(110, 17);

	if (visible(self.enemy))
		ai_face ();
};

void () ogre_naild = [ 63, ogre_nailc ]
{
	ogre_smg(110, 17);

	if (visible(self.enemy))
		ai_face ();
};

void () ogre_naile = [ 63, ogre_nailf ]
{
	ogre_smg(120, 17);
	if (visible(self.enemy))
		ai_face ();
};

void () ogre_nailf = [ 62, ogre_nailg ]
{
	ogre_smg(120, 17);
	if (visible(self.enemy))
		ai_face ();
};

void () ogre_nailg = [ 62, ogre_nailh ]
{
	ogre_smg(130, 17);
	if (visible(self.enemy))
		ai_face ();
};

void () ogre_nailh = [ 63, ogre_naili ]
{
	ogre_smg(130, 17);
	if (visible(self.enemy))
		ai_face ();
};
void () ogre_naili = [ 63, ogre_nailj ]
{
	ogre_smg(140, 17);
	if (visible(self.enemy))
		ai_face ();
};

void () ogre_nailj = [ 63, ogre_nailk ]
{
	ogre_smg(140, 17);
	if (visible(self.enemy))
		ai_face ();
};

void () ogre_nailk = [ 63, ogre_naill ]
{
	ogre_smg(150, 17);
	if (visible(self.enemy))
		ai_face ();
};

void () ogre_naill = [ 64, ogre_run1 ]
{
	ai_face ();
};

void () ogre_pain1 = [ 67, ogre_pain2 ]
{
};

void () ogre_pain2 = [ 68, ogre_pain3 ]
{
};

void () ogre_pain3 = [ 69, ogre_pain4 ]
{
};

void () ogre_pain4 = [ 70, ogre_pain5 ]
{
};

void () ogre_pain5 = [ 71, ogre_run1 ]
{
};

void () ogre_painb1 = [ 72, ogre_painb2 ]
{
};

void () ogre_painb2 = [ 73, ogre_painb3 ]
{
};

void () ogre_painb3 = [ 74, ogre_run1 ]
{
};

void () ogre_painc1 = [ 75, ogre_painc2 ]
{
};

void () ogre_painc2 = [ 76, ogre_painc3 ]
{
};

void () ogre_painc3 = [ 77, ogre_painc4 ]
{
};

void () ogre_painc4 = [ 78, ogre_painc5 ]
{
};

void () ogre_painc5 = [ 79, ogre_painc6 ]
{
};

void () ogre_painc6 = [ 80, ogre_run1 ]
{
};

void () ogre_paind1 = [ 81, ogre_paind2 ]
{
};

void () ogre_paind2 = [ 82, ogre_paind3 ]
{
	ai_pain (TE_LAVASPLASH);
};

void () ogre_paind3 = [ 83, ogre_paind4 ]
{
	ai_pain (TE_LIGHTNING3);
};

void () ogre_paind4 = [ 84, ogre_paind5 ]
{
	ai_pain (SECRET_1ST_DOWN);
};

void () ogre_paind5 = [ 85, ogre_paind6 ]
{
};

void () ogre_paind6 = [ 86, ogre_paind7 ]
{
};

void () ogre_paind7 = [ 87, ogre_paind8 ]
{
};

void () ogre_paind8 = [ 88, ogre_paind9 ]
{
};

void () ogre_paind9 = [ 89, ogre_paind10 ]
{
};

void () ogre_paind10 = [ 90, ogre_paind11 ]
{
};

void () ogre_paind11 = [ 91, ogre_paind12 ]
{
};

void () ogre_paind12 = [ 92, ogre_paind13 ]
{
};

void () ogre_paind13 = [ 93, ogre_paind14 ]
{
};

void () ogre_paind14 = [ 94, ogre_paind15 ]
{
};

void () ogre_paind15 = [ 95, ogre_paind16 ]
{
};

void () ogre_paind16 = [ 96, ogre_run1 ]
{
};

void () ogre_paine1 = [ 97, ogre_paine2 ]
{
};

void () ogre_paine2 = [ 98, ogre_paine3 ]
{
	ai_pain (TE_LAVASPLASH);
};

void () ogre_paine3 = [ 99, ogre_paine4 ]
{
	ai_pain (TE_LIGHTNING3);
};

void () ogre_paine4 = [ 100, ogre_paine5 ]
{
	ai_pain (SECRET_1ST_DOWN);
};

void () ogre_paine5 = [ 101, ogre_paine6 ]
{
};

void () ogre_paine6 = [ 102, ogre_paine7 ]
{
};

void () ogre_paine7 = [ 103, ogre_paine8 ]
{
};

void () ogre_paine8 = [ 104, ogre_paine9 ]
{
};

void () ogre_paine9 = [ 105, ogre_paine10 ]
{
};

void () ogre_paine10 = [ 106, ogre_paine11 ]
{
};

void () ogre_paine11 = [ 107, ogre_paine12 ]
{
};

void () ogre_paine12 = [ 108, ogre_paine13 ]
{
};

void () ogre_paine13 = [ 109, ogre_paine14 ]
{
};

void () ogre_paine14 = [ 110, ogre_paine15 ]
{
};

void () ogre_paine15 = [ 111, ogre_run1 ]
{
};

void (entity attacker, float damage) ogre_pain =
{
	if (self.pain_finished > time)
		return;

	if (random()*8 <= 1)
	{
		ogre_paind1();
		self.pain_finished = time + 7;
		sound (self, CHAN_VOICE, "ogre/ogpain1.wav", PLAT_LOW_TRIGGER, ATTN_NORM);
		return;
	}
};

void () ogre_die1 = [ 112, ogre_die2 ]
{
};

void () ogre_die2 = [ 113, ogre_die3 ]
{
};

void () ogre_die3 = [ 114, ogre_die4 ]
{

	self.rtime = 1;
};

void () ogre_die4 = [ 115, ogre_die5 ]
{
};

void () ogre_die5 = [ 116, ogre_die6 ]
{
};

void () ogre_die6 = [ 117, ogre_die7 ]
{
};

void () ogre_die7 = [ 118, ogre_die8 ]
{
};

void () ogre_die8 = [ 119, ogre_die9 ]
{
};

void () ogre_die9 = [ 120, ogre_die10 ]
{
};

void () ogre_die10 = [ 121, ogre_die11 ]
{
};

void () ogre_die11 = [ 122, ogre_die12 ]
{
};

void () ogre_die12 = [ 123, ogre_die13 ]
{
};

void () ogre_die13 = [ 124, ogre_die14 ]
{
};

void () ogre_die14 = [ 125, ogre_die14 ]
{
};

void () ogre_bdie1 = [ 126, ogre_bdie2 ]
{
};

void () ogre_bdie2 = [ 127, ogre_bdie3 ]
{
	ai_forward (MULTICAST_PVS_R);
};

void () ogre_bdie3 = [ 128, ogre_bdie4 ]
{
	self.rtime = 1;
	self.ammo_rockets = SILENT;
};

void () ogre_bdie4 = [ 129, ogre_bdie5 ]
{
	ai_forward (PLAT_LOW_TRIGGER);
};

void () ogre_bdie5 = [ 130, ogre_bdie6 ]
{
	ai_forward (AS_MELEE);
};

void () ogre_bdie6 = [ 131, ogre_bdie7 ]
{
	ai_forward (TE_WIZSPIKE);
};

void () ogre_bdie7 = [ 132, ogre_bdie8 ]
{
	ai_forward (DRAW3);
};

void () ogre_bdie8 = [ 133, ogre_bdie9 ]
{
};

void () ogre_bdie9 = [ 134, ogre_bdie10 ]
{
};

void () ogre_bdie10 = [ 135, ogre_bdie10 ]
{
};


void () mutant_pain =
{
	local vector dir;

	if (self.rtime > 0)
	{
		self.health = 180;
		return;
	}

	dir = normalize(self.origin - self.enemy.origin);
	self.velocity = dir * 10;

	if (self.frame == 69)
	{
		self.health = 180;
		self.frame = 70;
		self.think = ogre_die1;
		self.nextthink = time + 0.12;
	}
	else
	{
		self.health = 180;
		ThrowGib ("progs/zom_gib.mdl", -40);
		self.frame = 69;
		self.think = ogre_bdie1;
		self.nextthink = time + 0.12;
	}

	self.attack = self.attack + 1;
};

void (vector stuff, vector ang) spawn_live_ogre =
{
	local entity ogre;

	ogre = spawn ();
	ogre.origin = stuff;
	ogre.enemy = self.enemy;
	ogre.attack_finished = time + 10;
	ogre.solid = SOLID_SLIDEBOX;
	ogre.movetype = MOVETYPE_STEP;
	ogre.takedamage = DAMAGE_YES;
	setmodel (ogre, "progs/ogre.mdl");
	setsize (self, '-16 -16 -24', '16 16 48');
	ogre.classname = "body";
	ogre.netname = "dead";
	ogre.health = 180;
	ogre.angles = ang;
	ogre.max_health = ogre.health;
	ogre.th_pain = mutant_pain;
	ogre.think = mutant_pain;
	ogre.th_die = corpse_gib;
	ogre.nextthink = time + 0.05;
};

void () ogre_die =
{

	if (random()<0.75)
		DropMoney();
	if (random()<0.50)
		DropMoney();
	if (random()<0.25)
		DropMoney();

	sound (self, CHAN_VOICE, "ogre/ogdth.wav", PLAT_LOW_TRIGGER, ATTN_IDLE);

/*
	spawn_live_ogre(self.origin, self.angles);
	remove(self);*/


	self.solid = SOLID_NOT;

	if (self.frame == 69)
	{
		self.health = 180;
		self.frame = 70;
		self.think = ogre_die1;
		self.nextthink = time + 0.05;
	}
	else
	{
		self.health = 180;
		ThrowGib ("progs/zom_gib.mdl", -40);
		self.frame = 69;
		self.think = ogre_bdie1;
		self.nextthink = time + 0.05;
	}
};

void () ogre_melee =
{
	if ((random () > 0.5))
	{
		ogre_smash1 ();
	}
	else
	{
		ogre_swing1 ();
	}
};

void () monster_ogre =
{
	if (self.zone == 0)
	{
		load_monster();
		return;
	}
	precache_model ("progs/ogre.mdl");
	precache_model ("progs/h_ogre.mdl");
	precache_model ("progs/grenade.mdl");
	precache_sound ("ogre/ogdrag.wav");
	precache_sound ("ogre/ogdth.wav");
	precache_sound ("ogre/ogidle.wav");
	precache_sound ("ogre/ogidle2.wav");
	precache_sound ("ogre/ogpain1.wav");
	precache_sound ("ogre/ogsawatk.wav");
	precache_sound ("ogre/ogwake.wav");


	self.weapon = 1;//ak47


	self.solid = SOLID_SLIDEBOX;
	self.movetype = MOVETYPE_STEP;
	setmodel (self, "progs/ogre.mdl");
	self.netname = "mutant";
	self.classname = "monster";
	setsize (self, '-16 -16 -24', '16 16 48');
	self.health = 200;
	self.team = 3;
      self.islot3 = SlotVal(IID_ARM_LEATHER, 1);
	self.armornoise = "misc/thud.wav";
	self.th_stand = ogre_stand1;
	self.th_walk = ogre_walk1;
	self.th_run = ogre_run1;
	self.th_die = ogre_die;
	self.th_melee = ogre_melee;
	self.th_pain = ogre_pain;

	if (self.weapon == 1)
		self.th_missile = ogre_naila;
	else if (self.weapon == 2)
		self.th_missile = ogre_nail1;

	self.mag1 = 30;
	self.maxmag1 = 30;
	self.weapon = ceil(random()*3);
	walkmonster_start ();
};


void () monster_ogre_marksman =
{
	monster_ogre ();
};
