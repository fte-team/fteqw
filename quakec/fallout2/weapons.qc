/*
*/
void (entity targ, entity inflictor, entity attacker, float damage) T_Damage;
void () player_run;
void(entity bomb, entity attacker, float rad, entity ignore, string dtype) T_RadiusDamage;
void(vector org, float damage) SpawnBlood;
void() SuperDamageSound;
void (float rec, float number, float dam, float var, float ran, float auto) W_FireShotgun;
void (float dam, float rec, string snd, float rng, float rate) FireAssaultRifle;
void (float dam, float rec, string snd, float rng, float rate) FirePistol;
void (float dam, float rec, string snd, float rng, float rate) FireSMG;
void () W_PlayerMenu;
void () UseChem;
void () Special;
void () BuyMenu;
void() Sneak;
void() Bandage;
void() Shield;
void() player_knife1;
void() player_knifea;
void() ExitScreen;
void() CharacterSheet;
void() UseEquipment;
void (float weap, float snd, float drop) DropWeapon;

float () weightx;
void (entity guy, float slot) GetWeaponWeight;


// called by worldspawn
void() W_Precache =
{
	precache_sound ("weapons/r_exp3.wav");  // new rocket explosion
	precache_sound ("weapons/rocket1i.wav");        // spike gun
	precache_sound ("weapons/sgun1.wav");
	precache_sound ("weapons/guncock.wav"); // player shotgun
	precache_sound ("weapons/ric1.wav");    // ricochet (used in c code)
	precache_sound ("weapons/ric2.wav");    // ricochet (used in c code)
	precache_sound ("weapons/ric3.wav");    // ricochet (used in c code)
	precache_sound ("weapons/ric4.wav");    // ricochet (used in c code)
	precache_sound ("weapons/ric5.wav");    // ricochet (used in c code)
	precache_sound ("weapons/spike2.wav");  // super spikes
	precache_sound ("weapons/tink1.wav");   // spikes tink (used in c code)
	precache_sound ("weapons/grenade.wav"); // grenade launcher
	precache_sound ("weapons/bounce.wav");          // grenade bounce
	precache_sound ("weapons/shotgn2.wav"); // super shotgun
};

float() crandom =
{
	return 2*(random() - 0.5);
};

/*
================
W_FireMelee
================
*/
void(float damage, float dist, float rate) FireMelee =
{
	local   vector  source;
	local   vector  org;

	makevectors (self.v_angle);
	source = self.origin + '0 0 16';
	traceline (source, source + v_forward*dist, FALSE, self);
	if (trace_fraction == 1.0)
		return;
	
	org = trace_endpos - v_forward*4;

	if (trace_ent.takedamage)
	{
		trace_ent.axhitme = 1;
		SpawnBlood (org, 20);
		T_Damage (trace_ent, self, self, damage+random()*damage);
	}
	else
	{       // hit wall
		sound (self, CHAN_WEAPON, "player/axhit2.wav", 1, ATTN_NORM);

		WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
		WriteByte (MSG_MULTICAST, TE_SPIKE);
		WriteCoord (MSG_MULTICAST, org_x);
		WriteCoord (MSG_MULTICAST, org_y);
		WriteCoord (MSG_MULTICAST, org_z);
		multicast (self.origin, MULTICAST_PHS);
	}
};


//============================================================================

vector() wall_velocity =
{
	local vector    vel;
	
	vel = normalize (self.velocity);
	vel = normalize(vel + v_up*(random()- 0.5) + v_right*(random()- 0.5));
	vel = vel + 2*trace_plane_normal;
	vel = vel * 200;
	
	return vel;
};


/*
================
SpawnMeatSpray
================
*/
void(vector org, vector vel) SpawnMeatSpray =
{
	local   entity missile;
	local   vector  org;

	missile = spawn ();
	missile.owner = self;
	missile.movetype = MOVETYPE_BOUNCE;
	missile.solid = SOLID_NOT;

	makevectors (self.angles);

	missile.velocity = vel;
	missile.velocity_z = missile.velocity_z + 250 + 50*random();

	missile.avelocity = '3000 1000 2000';
	
// set missile duration
	missile.nextthink = time + 1;
	missile.think = SUB_Remove;

	setmodel (missile, "progs/zom_gib.mdl");
	setsize (missile, '0 0 0', '0 0 0');            
	setorigin (missile, org);
};

/*
================
SpawnBlood
================
*/
void(vector org, float damage) SpawnBlood =
{
	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_BLOOD);
	WriteByte (MSG_MULTICAST, 1);
	WriteCoord (MSG_MULTICAST, org_x);
	WriteCoord (MSG_MULTICAST, org_y);
	WriteCoord (MSG_MULTICAST, org_z);
	multicast (org, MULTICAST_PVS);
};

/*
================
spawn_touchblood
================
*/
void(float damage) spawn_touchblood =
{
	local vector    vel;

	vel = wall_velocity () * 0.2;
	SpawnBlood (self.origin + vel*0.01, damage);
};

/*
==============================================================================

MULTI-DAMAGE

Collects multiple small damages into a single damage

==============================================================================
*/

entity  multi_ent;
float   multi_damage;

vector  blood_org;
float   blood_count;

vector  puff_org;
float   puff_count;

void() ClearMultiDamage =
{
	multi_ent = world;
	multi_damage = 0;
	blood_count = 0;
	puff_count = 0;
};

void() ApplyMultiDamage =
{
	if (!multi_ent)
		return;
	T_Damage (multi_ent, self, self, multi_damage);
};

void(entity hit, float damage) AddMultiDamage =
{
	if (!hit)
		return;
	
	if (hit != multi_ent)
	{
		ApplyMultiDamage ();
		multi_damage = damage;
		multi_ent = hit;
	}
	else
		multi_damage = multi_damage + damage;
};

void() Multi_Finish =
{
	if (puff_count)
	{
		WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
		WriteByte (MSG_MULTICAST, TE_GUNSHOT);
		WriteByte (MSG_MULTICAST, puff_count);
		WriteCoord (MSG_MULTICAST, puff_org_x);
		WriteCoord (MSG_MULTICAST, puff_org_y);
		WriteCoord (MSG_MULTICAST, puff_org_z);
		multicast (puff_org, MULTICAST_PVS);
	}

	if (blood_count)
	{
		WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
		WriteByte (MSG_MULTICAST, TE_BLOOD);
		WriteByte (MSG_MULTICAST, blood_count);
		WriteCoord (MSG_MULTICAST, blood_org_x);
		WriteCoord (MSG_MULTICAST, blood_org_y);
		WriteCoord (MSG_MULTICAST, blood_org_z);
		multicast (puff_org, MULTICAST_PVS);
	}
};

/*
==============================================================================
BULLETS
==============================================================================
*/

/*
================
TraceAttack
================
*/
void(float damage, vector dir) TraceAttack =
{
	local   vector  vel, org;
	
	vel = normalize(dir + v_up*crandom() + v_right*crandom());
	vel = vel + 2*trace_plane_normal;
	vel = vel * 200;

	org = trace_endpos - dir*4;

	if (trace_ent.takedamage)
	{
		blood_count = blood_count + 1;
		blood_org = org;
		AddMultiDamage (trace_ent, damage);
	}
	else
	{
		puff_count = puff_count + 1;
	}
};

/*
================
FireBullets

Used by shotgun, super shotgun, and enemy soldier firing
Go to the trouble of combining multiple pellets into a single damage call.
================
*/


/*
==============================================================================

ROCKETS

==============================================================================
*/

void() T_MissileTouch =
{
	local float     damg;

//	if (deathmatch == 4)
//	{
//	if ( ((other.weapon == 32) || (other.weapon == 16)))
//		{	
//			if (random() < 0.1)
//			{
//				if (other != world)
//				{
//	//				bprint (PRINT_HIGH, "Got here\n");
//					other.deathtype = "blaze";
//					T_Damage (other, self, self.owner, 1000 );
//					T_RadiusDamage (self, self.owner, 1000, other);
//				}
//			}
//		}	
//	}

	if (other == self.owner)
		return;         // don't explode on owner

	if (self.voided) {
		return;
	}
	self.voided = 1;

	if (pointcontents(self.origin) == CONTENT_SKY)
	{
		remove(self);
		return;
	}

	damg = 100 + random()*20;
	
	if (other.health)
	{
		other.deathtype = "rocket";
		T_Damage (other, self, self.owner, damg );
	}

	// don't do radius damage to the other, because all the damage
	// was done in the impact


	T_RadiusDamage (self, self.owner, 120, other, "rocket");

//  sound (self, CHAN_WEAPON, "weapons/r_exp3.wav", 1, ATTN_NORM);
	self.origin = self.origin - 8 * normalize(self.velocity);

	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_EXPLOSION);
	WriteCoord (MSG_MULTICAST, self.origin_x);
	WriteCoord (MSG_MULTICAST, self.origin_y);
	WriteCoord (MSG_MULTICAST, self.origin_z);
	multicast (self.origin, MULTICAST_PHS);

	remove(self);
};



/*
================
W_FireRocket
================
*/
void() W_FireRocket =
{
	if (deathmatch != 4)
		self.currentammo = self.ammo_rockets = self.ammo_rockets - 1;
	
	sound (self, CHAN_WEAPON, "weapons/sgun1.wav", 1, ATTN_NORM);

	msg_entity = self;
	WriteByte (MSG_ONE, SVC_SMALLKICK);

	newmis = spawn ();
	newmis.owner = self;
	newmis.movetype = MOVETYPE_FLYMISSILE;
	newmis.solid = SOLID_BBOX;
		
// set newmis speed     

	makevectors (self.v_angle);
	newmis.velocity = aim(self, 1000);
	newmis.velocity = newmis.velocity * 1000;
	newmis.angles = vectoangles(newmis.velocity);
	
	newmis.touch = T_MissileTouch;
	newmis.voided = 0;
	
// set newmis duration
	newmis.nextthink = time + 5;
	newmis.think = SUB_Remove;
	newmis.classname = "rocket";

	setmodel (newmis, "progs/missile.mdl");
	setsize (newmis, '0 0 0', '0 0 0');             
	setorigin (newmis, self.origin + v_forward*8 + '0 0 16');
};

/*
===============================================================================
LIGHTNING
===============================================================================
*/

void(entity from, float damage) LightningHit =
{
	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_LIGHTNINGBLOOD);
	WriteCoord (MSG_MULTICAST, trace_endpos_x);
	WriteCoord (MSG_MULTICAST, trace_endpos_y);
	WriteCoord (MSG_MULTICAST, trace_endpos_z);
	multicast (trace_endpos, MULTICAST_PVS);

	T_Damage (trace_ent, from, from, damage);
};

/*
=================
LightningDamage
=================
*/
void(vector p1, vector p2, entity from, float damage) LightningDamage =
{
	local entity            e1, e2;
	local vector            f;
	
	f = p2 - p1;
	normalize (f);
	f_x = 0 - f_y;
	f_y = f_x;
	f_z = 0;
	f = f*16;

	e1 = e2 = world;

	traceline (p1, p2, FALSE, self);

	if (trace_ent.takedamage)
	{
		LightningHit (from, damage);
		if (self.classname == "player")
		{
			if (other.classname == "player")
				trace_ent.velocity_z = trace_ent.velocity_z + 400;
		}
	}
	e1 = trace_ent;

	traceline (p1 + f, p2 + f, FALSE, self);
	if (trace_ent != e1 && trace_ent.takedamage)
	{
		LightningHit (from, damage);
	}
	e2 = trace_ent;

	traceline (p1 - f, p2 - f, FALSE, self);
	if (trace_ent != e1 && trace_ent != e2 && trace_ent.takedamage)
	{
		LightningHit (from, damage);
	}
};




void() W_FireLightning =
{
	local   vector          org;
	local   float           cells;

	if (self.ammo_cells < 1)
	{
		self.weapon = W_BestWeapon ();
		W_SetCurrentAmmo ();
		return;
	}

// explode if under water
	if (self.waterlevel > 1)
	{
		if (deathmatch > 3)
		{
			if (random() <= 0.5)
			{
				self.deathtype = "selfwater";
				T_Damage (self, self, self.owner, 4000 );
			}
			else
			{
				cells = self.ammo_cells;
				self.ammo_cells = 0;
				W_SetCurrentAmmo ();
				T_RadiusDamage (self, self, 35*cells, world, "");
				return;
			}
		}
		else
		{
			cells = self.ammo_cells;
			self.ammo_cells = 0;
			W_SetCurrentAmmo ();
			T_RadiusDamage (self, self, 35*cells, world,"");
			return;
		}
	}

	if (self.t_width < time)
	{
		sound (self, CHAN_WEAPON, "weapons/lhit.wav", 1, ATTN_NORM);
		self.t_width = time + 0.6;
	}
	msg_entity = self;
	WriteByte (MSG_ONE, SVC_SMALLKICK);

	if (deathmatch != 4)
		self.currentammo = self.ammo_cells = self.ammo_cells - 1;

	org = self.origin + '0 0 16';
	
	traceline (org, org + v_forward*600, TRUE, self);

	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_LIGHTNING2);
	WriteEntity (MSG_MULTICAST, self);
	WriteCoord (MSG_MULTICAST, org_x);
	WriteCoord (MSG_MULTICAST, org_y);
	WriteCoord (MSG_MULTICAST, org_z);
	WriteCoord (MSG_MULTICAST, trace_endpos_x);
	WriteCoord (MSG_MULTICAST, trace_endpos_y);
	WriteCoord (MSG_MULTICAST, trace_endpos_z);
	multicast (org, MULTICAST_PHS);

	LightningDamage (self.origin, trace_endpos + v_forward*4, self, 30);
};


//=============================================================================


void() GrenadeExplode =
{
	if (self.voided) {
		return;
	}
	self.voided = 1;

	T_RadiusDamage (self, self.owner, 120, world, "grenade");

	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_EXPLOSION);
	WriteCoord (MSG_MULTICAST, self.origin_x);
	WriteCoord (MSG_MULTICAST, self.origin_y);
	WriteCoord (MSG_MULTICAST, self.origin_z);
	multicast (self.origin, MULTICAST_PHS);

	remove (self);
};

void() GrenadeTouch =
{
	if (other == self.owner)
		return;         // don't explode on owner
	if (other.takedamage == DAMAGE_AIM)
	{
		GrenadeExplode();
		return;
	}
	sound (self, CHAN_WEAPON, "weapons/bounce.wav", 1, ATTN_NORM);  // bounce sound
	if (self.velocity == '0 0 0')
		self.avelocity = '0 0 0';
};

/*
================
W_FireGrenade
================
*/
void() W_FireGrenade =
{       
	if (deathmatch != 4)
		self.currentammo = self.ammo_rockets = self.ammo_rockets - 1;
	
	sound (self, CHAN_WEAPON, "weapons/grenade.wav", 1, ATTN_NORM);

	msg_entity = self;
	WriteByte (MSG_ONE, SVC_SMALLKICK);

	newmis = spawn ();
	newmis.voided=0;
	newmis.owner = self;
	newmis.movetype = MOVETYPE_BOUNCE;
	newmis.solid = SOLID_BBOX;
	newmis.classname = "grenade";
		
// set newmis speed     

	makevectors (self.v_angle);

	if (self.v_angle_x)
		newmis.velocity = v_forward*600 + v_up * 200 + crandom()*v_right*10 + crandom()*v_up*10;
	else
	{
		newmis.velocity = aim(self, 10000);
		newmis.velocity = newmis.velocity * 600;
		newmis.velocity_z = 200;
	}

	newmis.avelocity = '300 300 300';

	newmis.angles = vectoangles(newmis.velocity);
	
	newmis.touch = GrenadeTouch;
	
// set newmis duration
	if (deathmatch == 4)
	{
		newmis.nextthink = time + 2.5;		
		self.attack_finished = time + 1.1;
//		self.health = self.health - 1;
		T_Damage (self, self, self.owner, 10 );
	}
	else
		newmis.nextthink = time + 2.5;

	newmis.think = GrenadeExplode;

	setmodel (newmis, "progs/grenade.mdl");
	setsize (newmis, '0 0 0', '0 0 0');             
	setorigin (newmis, self.origin);
};


//=============================================================================

void() spike_touch;
void() superspike_touch;


/*
===============
launch_spike

Used for both the player and the ogre
===============
*/
void(vector org, vector dir) launch_spike =
{
	newmis = spawn ();
	newmis.voided=0;
	newmis.owner = self;
	newmis.movetype = MOVETYPE_FLYMISSILE;
	newmis.solid = SOLID_BBOX;

	newmis.angles = vectoangles(dir);
	
	newmis.touch = spike_touch;
	newmis.classname = "spike";
	newmis.think = SUB_Remove;
	newmis.nextthink = time + 6;
	setmodel (newmis, "progs/spike.mdl");
	setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);               
	setorigin (newmis, org);

	newmis.velocity = dir * 1000;
};

void() W_FireSuperSpikes =
{
	local vector    dir;
	local entity    old;
	
	sound (self, CHAN_WEAPON, "weapons/spike2.wav", 1, ATTN_NORM);
	self.attack_finished = time + 0.2;
	if (deathmatch != 4) 
		self.currentammo = self.ammo_nails = self.ammo_nails - 2;
	dir = aim (self, 1000);
	launch_spike (self.origin + '0 0 16', dir);
	newmis.touch = superspike_touch;
	setmodel (newmis, "progs/s_spike.mdl");
	setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);               
	msg_entity = self;
	WriteByte (MSG_ONE, SVC_SMALLKICK);
};

void(float ox) W_FireSpikes =
{
	local vector    dir;
	local entity    old;
	
	makevectors (self.v_angle);
	
	if (self.ammo_nails >= 2 && self.weapon == IT_SUPER_NAILGUN)
	{
		W_FireSuperSpikes ();
		return;
	}

	if (self.ammo_nails < 1)
	{
		self.weapon = W_BestWeapon ();
		W_SetCurrentAmmo ();
		return;
	}

	sound (self, CHAN_WEAPON, "weapons/rocket1i.wav", 1, ATTN_NORM);
	self.attack_finished = time + 0.2;
	if (deathmatch != 4)
		self.currentammo = self.ammo_nails = self.ammo_nails - 1;
	dir = aim (self, 1000);
	launch_spike (self.origin + '0 0 16' + v_right*ox, dir);

	msg_entity = self;
	WriteByte (MSG_ONE, SVC_SMALLKICK);
};



.float hit_z;
void() spike_touch =
{
local float rand;
	if (other == self.owner)
		return;

	if (self.voided) {
		return;
	}
	self.voided = 1;

	if (other.solid == SOLID_TRIGGER)
		return; // trigger field, do nothing

	if (pointcontents(self.origin) == CONTENT_SKY)
	{
		remove(self);
		return;
	}
	
// hit something that bleeds
	if (other.takedamage)
	{
		spawn_touchblood (9);
		other.deathtype = "nail";
		T_Damage (other, self, self.owner, 9);
	}
	else
	{
		WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
		if (self.classname == "wizspike")
			WriteByte (MSG_MULTICAST, TE_WIZSPIKE);
		else if (self.classname == "knightspike")
			WriteByte (MSG_MULTICAST, TE_KNIGHTSPIKE);
		else
			WriteByte (MSG_MULTICAST, TE_SPIKE);
		WriteCoord (MSG_MULTICAST, self.origin_x);
		WriteCoord (MSG_MULTICAST, self.origin_y);
		WriteCoord (MSG_MULTICAST, self.origin_z);
		multicast (self.origin, MULTICAST_PHS);
	}

	remove(self);

};

void() superspike_touch =
{
local float rand;
	if (other == self.owner)
		return;

	if (self.voided) {
		return;
	}
	self.voided = 1;


	if (other.solid == SOLID_TRIGGER)
		return; // trigger field, do nothing

	if (pointcontents(self.origin) == CONTENT_SKY)
	{
		remove(self);
		return;
	}
	
// hit something that bleeds
	if (other.takedamage)
	{
		spawn_touchblood (18);
		other.deathtype = "supernail";
		T_Damage (other, self, self.owner, 18);
	}
	else
	{
		WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
		WriteByte (MSG_MULTICAST, TE_SUPERSPIKE);
		WriteCoord (MSG_MULTICAST, self.origin_x);
		WriteCoord (MSG_MULTICAST, self.origin_y);
		WriteCoord (MSG_MULTICAST, self.origin_z);
		multicast (self.origin, MULTICAST_PHS);
	}

	remove(self);

};


/*
===============================================================================

PLAYER WEAPON USE

===============================================================================
*/

void(float temp_weapon) GetWeaponModel =
{
	if (temp_weapon == 0)
			self.weaponmodel = "progs/v_fist.mdl";
	else if (temp_weapon == 1)
			self.weaponmodel = "progs/v_knife.mdl";
	else if (temp_weapon == 2)
			self.weaponmodel = "progs/v_axe.mdl";
	else if (temp_weapon == 3)
			self.weaponmodel = "progs/v_knife.mdl";
	else if (temp_weapon == 4)
			self.weaponmodel = "progs/v_axe.mdl";
	else if (temp_weapon == 5)
			self.weaponmodel = "progs/v_1911.mdl";
	else if (temp_weapon == 6)
			self.weaponmodel = "progs/v_deagle.mdl";
	else if (temp_weapon == 7)
			self.weaponmodel = "progs/v_1911.mdl";
	else if (temp_weapon == 8)
			self.weaponmodel = "progs/v_alien.mdl";
	else if (temp_weapon == 9)
			self.weaponmodel = "progs/v_pipe.mdl";
	else if (temp_weapon == 10)
			self.weaponmodel = "progs/v_double.mdl";
	else if (temp_weapon == 11)
			self.weaponmodel = "progs/v_shotgun.mdl";
	else if (temp_weapon == 12)
			self.weaponmodel = "progs/v_jackhammer.mdl";
	else if (temp_weapon == 13)
			self.weaponmodel = "progs/v_mp9.mdl";
	else if (temp_weapon == 14)
			self.weaponmodel = "progs/v_mp7.mdl";
	else if (temp_weapon == 15)
			self.weaponmodel = "progs/v_rangem.mdl";
	else if (temp_weapon == 16)
			self.weaponmodel = "progs/v_ak47.mdl";
	else if (temp_weapon == 17)
			self.weaponmodel = "progs/v_ak47.mdl";
	else if (temp_weapon == 18)
			self.weaponmodel = "progs/v_srifle.mdl";
	else if (temp_weapon == 19)
			self.weaponmodel = "progs/v_night.mdl";
	else if (temp_weapon == 99)
			self.weaponmodel = "progs/v_handgren.mdl";
};

void() W_SetCurrentAmmo =
{
      local float temp_weapon;
	local string x;

	player_run ();          // get out of any weapon firing states

	self.items = self.items - ( self.items & (IT_SHELLS | IT_NAILS | IT_ROCKETS | IT_CELLS) );
	

	if (self.current_slot == 1)
		self.currentammo = self.mag1;
	else if (self.current_slot == 2)
		self.currentammo = self.mag2;
	else if (self.current_slot == 3 && self.handgrenade > 0)
		self.currentammo = self.handgrenade;
	else if (self.current_slot == 4)
	{
		if (self.team == 1)
		{
			sound (self, CHAN_WEAPON, "misc/tools.wav", 1, ATTN_NORM);
			self.items = (self.items | IT_LIGHTNING);
			self.currentammo = 0;
			self.weaponmodel = "";
		}
		else
			sound (self, CHAN_WEAPON, "misc/menu3.wav", 1, ATTN_NORM);
	}
	else
		sound (self, CHAN_WEAPON, "misc/menu3.wav", 1, ATTN_NORM);

		if (self.current_slot == 1)
			x = GetWeaponName(self, self.slot1);
		if (self.current_slot == 2)
			x = GetWeaponName(self, self.slot2);
		if (self.current_slot == 3)
			x = "grenade";
		if (self.current_slot == 4)
			x = "tools";

		sprint(self, 2, x);
		sprint (self, PRINT_HIGH, " selected.\n");
		sound (self, CHAN_WEAPON, "misc/weapon.wav", 1, ATTN_NORM);
		if (self.current_slot == 1)
			GetWeaponModel(self.slot1);
		if (self.current_slot == 2)
			GetWeaponModel(self.slot2);
		if (self.current_slot == 3)
			GetWeaponModel(99);

		if (self.current_slot == 1)
			self.currentammo = self.mag1;
		if (self.current_slot == 2)
			self.currentammo = self.mag2;
};

float() W_BestWeapon =
{
	local   float   it;
	
	it = self.items;

	if (self.waterlevel <= 1 && self.ammo_cells >= 1 && (it & IT_LIGHTNING) )
		return IT_LIGHTNING;
	else if(self.ammo_nails >= 2 && (it & IT_SUPER_NAILGUN) )
		return IT_SUPER_NAILGUN;
	else if(self.ammo_shells >= 2 && (it & IT_SUPER_SHOTGUN) )
		return IT_SUPER_SHOTGUN;
	else if(self.ammo_nails >= 1 && (it & IT_NAILGUN) )
		return IT_NAILGUN;
	else if(self.ammo_shells >= 1 && (it & IT_SHOTGUN)  )
		return IT_SHOTGUN;
		
/*
	if(self.ammo_rockets >= 1 && (it & IT_ROCKET_LAUNCHER) )
		return IT_ROCKET_LAUNCHER;
	else if(self.ammo_rockets >= 1 && (it & IT_GRENADE_LAUNCHER) )
		return IT_GRENADE_LAUNCHER;

*/

	return IT_AXE;
};

void () ReloadWeapon =
{
	local float x;

	if (self.current_slot != 1 && self.current_slot != 2)
		return;

	if (self.rtime > 0)
		return;

	if (self.current_slot == 1)
	{
		if (self.ammo1 <= 0)
			return;
		if (self.maxmag1 == self.mag1)
			return;
		if (self.slot1 <= 4)
			return;

		if (self.slot1 >= 9 && self.slot1 <= 11)
		{
			sound (self, CHAN_WEAPON, "weapons/shell.wav", TRUE, ATTN_NORM);
			self.mag1 = self.mag1 + 1;
			self.ammo1 = self.ammo1 - 1;
			self.currentammo = self.mag1;
			self.rtime = 50;
			return;
		}
		sound (self, CHAN_WEAPON, "weapons/reload.wav", TRUE, ATTN_NORM);
		sprint(self, 2, "reloading...\n");

		x = (self.maxmag1 - self.mag1);
		self.mag1 = self.mag1 + x;
		self.ammo1 = self.ammo1 - x;
		self.currentammo = self.mag1;
		self.rtime = 200;
	}
	if (self.current_slot == 2)
	{
		if (self.ammo2 <= 0)
			return;
		if (self.maxmag2 == self.mag2)
			return;
		if (self.slot2 <= 4)
			return;

		if (self.slot2 >= 9 && self.slot2 <= 11)
		{
			sound (self, CHAN_WEAPON, "weapons/shell.wav", TRUE, ATTN_NORM);
			self.mag2 = self.mag2 + 1;
			self.ammo2 = self.ammo2 - 1;
			self.currentammo = self.mag2;
			self.rtime = 50;
			return;
		}
		sound (self, CHAN_WEAPON, "weapons/reload.wav", TRUE, ATTN_NORM);
		sprint(self, 2, "reloading...\n");

		x = (self.maxmag2 - self.mag2);
		self.mag2 = self.mag2 + x;
		self.ammo2 = self.ammo2 - x;
		self.currentammo = self.mag2;
		self.rtime = 200;
	}
};


float() W_CheckNoAmmo =
{
	if (self.current_slot == 1)
	{
		if (self.slot1 <= 4)
			return FALSE;

		if (self.mag1 < 1 && self.ammo1 < 1)
		{
			self.attack_finished = (time + 0.2);
			stuffcmd (self, "-attack\n");
			sound (self, CHAN_WEAPON, "weapons/click.wav", TRUE, ATTN_NORM);
			return TRUE;
		}
		if (self.mag1 <= 0)
		{
			stuffcmd (self, "-attack\n");
			ReloadWeapon ();
			return TRUE;
		}
	}
	if (self.current_slot == 2)
	{
		if (self.slot2 <= 4)
			return FALSE;

		if (self.mag2 < 2 && self.ammo2 < 2)
		{
			self.attack_finished = (time + 0.2);
			stuffcmd (self, "-attack\n");
			sound (self, CHAN_WEAPON, "weapons/click.wav", TRUE, ATTN_NORM);
			return TRUE;
		}
		if (self.mag2 <= 0)
		{
			stuffcmd (self, "-attack\n");
			ReloadWeapon ();
			return TRUE;
		}
	}

	return FALSE;
};

/*
============
W_Attack

An attack impulse can be triggered now
============
*/

void() W_Attack =
{
	local float weap, r;

	makevectors     (self.v_angle);                 // calculate forward angle for velocity
	self.show_hostile = time + 1;   // wake monsters up

	if (self.current_slot == 1)
		weap = self.slot1;
	if (self.current_slot == 2)
		weap = self.slot2;

	if (W_CheckNoAmmo())
		return;

	if (self.rtime > 0)
		return;

	if (weap >= 0 && weap <= 4)
	{
		self.attack_finished = time + 0.25;
		player_knife1 ();
	}
	else if (weap == 5)
		FirePistol(10, 2, "weapons/1911.wav", 2000, 0.25);
	else if (weap == 6)
		FirePistol(10, 2, "weapons/deagle.wav", 2000, 0.25);
	else if (weap == 7)
		FirePistol(10, 2, "weapons/needler.wav", 2000, 0.25);
	//if (weap == 8)
	//	FireAlienBlaster();
	else if (weap == 9)
		FireAssaultRifle(18, 2, "weapons/rangem.wav", 3000, 0.1);
	else if (weap == 10)
		W_FireShotgun (1, 5, 6, 160, 3000, 0);
	else if (weap == 11)
		W_FireShotgun (1, 5, 6, 160, 3000, 0);
	else if (weap == 12)
		W_FireShotgun (1, 5, 6, 160, 3000, 1);
	else if (weap == 13)
		FireSMG(12, 2, "weapons/mp9.wav", 2000, 0.06);
	else if (weap == 14)
		FireSMG(12, 2, "weapons/mp7.wav", 2000, 0.06);
	else if (weap == 15)
		FireAssaultRifle(14, 2, "weapons/rangem.wav", 4000, 0.5);
	else if (weap == 16)
		FireAssaultRifle(14, 2, "weapons/rangem.wav", 4000, 0.1);
	else if (weap == 17)
		FireAssaultRifle(18, 2, "weapons/ak47.wav", 6000, 0.1);
	else if (weap == 18)
		FireAssaultRifle(30, 2, "weapons/dks-1.wav", 8000, 0.5);
	else if (weap == 19)
		FireAssaultRifle(16, 2, "weapons/m4a1.wav", 4000, 0.1);
};

/*
============
W_ChangeWeapon

============
*/
void() W_ChangeWeapon =
{
	local   float   it, am, fl, r;
	
	it = self.items;
	am = 0;
	
	if (self.impulse == 1)
	{
		fl = IT_NAILGUN;
		self.current_slot = 1;
	}
	if (self.impulse == 2)
	{
		fl = IT_SUPER_NAILGUN;
		self.current_slot = 2;
	}
	if (self.impulse == 3)
	{
		if (self.handgrenade == 0)
		{
			sprint (self, PRINT_HIGH, "no grenade.\n");
			sound (self, CHAN_AUTO, "misc/noweapon.wav", 1, ATTN_STATIC);
			return;
		}
		else
			self.current_slot = 3;		
	}

	self.weapon = fl;               
	W_SetCurrentAmmo ();
};

/*
============
CheatCommand
============
*/
void() CheatCommand =
{
//      if (deathmatch || coop)
		return;

	self.ammo_rockets = 100;
	self.ammo_nails = 200;
	self.ammo_shells = 100;
	self.items = self.items | 
		IT_AXE |
		IT_SHOTGUN |
		IT_SUPER_SHOTGUN |
		IT_NAILGUN |
		IT_SUPER_NAILGUN |
		IT_GRENADE_LAUNCHER |
		IT_ROCKET_LAUNCHER |
		IT_KEY1 | IT_KEY2;

	self.ammo_cells = 200;
	self.items = self.items | IT_LIGHTNING;

	self.weapon = IT_ROCKET_LAUNCHER;
	self.impulse = 0;
	W_SetCurrentAmmo ();
};

/*
============
CycleWeaponCommand

Go to the next weapon with ammo
============
*/
void() CycleWeaponCommand =
{
	local   float   it, am;
	
	it = self.items;
	self.impulse = 0;

	while (1)
	{
		am = 0;

		if (self.weapon == IT_LIGHTNING)
		{
			self.weapon = IT_AXE;
		}
		else if (self.weapon == IT_AXE)
		{
			self.weapon = IT_SHOTGUN;
			if (self.ammo_shells < 1)
				am = 1;
		}
		else if (self.weapon == IT_SHOTGUN)
		{
			self.weapon = IT_SUPER_SHOTGUN;
			if (self.ammo_shells < 2)
				am = 1;
		}               
		else if (self.weapon == IT_SUPER_SHOTGUN)
		{
			self.weapon = IT_NAILGUN;
			if (self.ammo_nails < 1)
				am = 1;
		}
		else if (self.weapon == IT_NAILGUN)
		{
			self.weapon = IT_SUPER_NAILGUN;
			if (self.ammo_nails < 2)
				am = 1;
		}
		else if (self.weapon == IT_SUPER_NAILGUN)
		{
			self.weapon = IT_GRENADE_LAUNCHER;
			if (self.ammo_rockets < 1)
				am = 1;
		}
		else if (self.weapon == IT_GRENADE_LAUNCHER)
		{
			self.weapon = IT_ROCKET_LAUNCHER;
			if (self.ammo_rockets < 1)
				am = 1;
		}
		else if (self.weapon == IT_ROCKET_LAUNCHER)
		{
			self.weapon = IT_LIGHTNING;
			if (self.ammo_cells < 1)
				am = 1;
		}
	
		if ( (self.items & self.weapon) && am == 0)
		{
			W_SetCurrentAmmo ();
			return;
		}
	}

};




void () ProneOff =
{
	sprint (self, 2, "position: stand.\n");
	self.position = 0;
	player_run ();
};

void () ProneOn =
{
	local string x;

	if (self.velocity_z != 0)
		return;

	if (self.position == 2)
	{
		ProneOff();
		return;
	}

	self.maxspeed = (self.maxspeed * 0.25);
	self.position = 2;
	self.view_ofs = '0 0 -10';
	sprint (self, 2, "position: prone.\n");
};


void () DuckOff =
{
	sprint (self, 2, "position: stand.\n");
	self.position = 0;
	player_run ();
};


void () DuckOn =
{
	if (self.velocity_z != 0)
		return;

	if (self.position == 1)
	{
		DuckOff();
		return;
	}

	self.maxspeed = (self.maxspeed * 0.50);
	self.position = 1;
	self.view_ofs = '0 0 12';
	sprint (self, 2, "position: duck.\n");
};


/*
============
CycleWeaponReverseCommand

Go to the prev weapon with ammo
============
*/
void() CycleWeaponReverseCommand =
{
	local   float   it, am;
	
	it = self.items;
	self.impulse = 0;

	while (1)
	{
		am = 0;

		if (self.weapon == IT_LIGHTNING)
		{
			self.weapon = IT_ROCKET_LAUNCHER;
			if (self.ammo_rockets < 1)
				am = 1;
		}
		else if (self.weapon == IT_ROCKET_LAUNCHER)
		{
			self.weapon = IT_GRENADE_LAUNCHER;
			if (self.ammo_rockets < 1)
				am = 1;
		}
		else if (self.weapon == IT_GRENADE_LAUNCHER)
		{
			self.weapon = IT_SUPER_NAILGUN;
			if (self.ammo_nails < 2)
				am = 1;
		}
		else if (self.weapon == IT_SUPER_NAILGUN)
		{
			self.weapon = IT_NAILGUN;
			if (self.ammo_nails < 1)
				am = 1;
		}
		else if (self.weapon == IT_NAILGUN)
		{
			self.weapon = IT_SUPER_SHOTGUN;
			if (self.ammo_shells < 2)
				am = 1;
		}               
		else if (self.weapon == IT_SUPER_SHOTGUN)
		{
			self.weapon = IT_SHOTGUN;
			if (self.ammo_shells < 1)
				am = 1;
		}
		else if (self.weapon == IT_SHOTGUN)
		{
			self.weapon = IT_AXE;
		}
		else if (self.weapon == IT_AXE)
		{
			self.weapon = IT_LIGHTNING;
			if (self.ammo_cells < 1)
				am = 1;
		}
	
		if ( (it & self.weapon) && am == 0)
		{
			W_SetCurrentAmmo ();
			return;
		}
	}

};



/*
============
ServerflagsCommand

Just for development
============
*/
void() ServerflagsCommand =
{
	serverflags = serverflags * 2 + 1;
};


/*
============
ImpulseCommands

============
*/
void() ImpulseCommands =
{
	if (self.impulse >= 1 && self.impulse <= 4 && self.currentmenu == "none")
		W_ChangeWeapon ();

	if (self.impulse >= 1 && self.impulse <= 10 && self.currentmenu != "none")
		W_PlayerMenu ();

	if (self.impulse == 11)
		ServerflagsCommand ();
	if (self.impulse == 12)
		CycleWeaponReverseCommand ();
	if (self.impulse == 200)
		DuckOn ();
	if (self.impulse == 201)
		ProneOn ();

	if (self.impulse == 50)
		ReloadWeapon ();
	if (self.impulse == 51)
		UseChem ();
	if (self.impulse == 52)
		BuyMenu ();
	if (self.impulse == 53)
		Special ();
	if (self.impulse == 54)
		ExitScreen ();
	if (self.impulse == 55)
	{
		if (self.current_slot == 1)
			DropWeapon (self.slot1, 1, 0);
		else
			DropWeapon (self.slot2, 1, 0);
	}
	if (self.impulse == 56)
		CharacterSheet ();
	if (self.impulse == 57)
		UseEquipment ();

	self.impulse = 0;
};

/*
============
W_WeaponFrame

Called every frame so impulse events can be handled as well as possible
============
*/
void() W_WeaponFrame =
{
	if (time < self.attack_finished)
		return;

	ImpulseCommands ();
	
// check for attack
	if (self.button0)
	{
		SuperDamageSound ();
		W_Attack ();
	}
};

/*
========
SuperDamageSound

Plays sound if needed
========
*/
void() SuperDamageSound =
{
	if (self.super_damage_finished > time)
	{
		if (self.super_sound < time)
		{
			self.super_sound = time + 1;
			sound (self, CHAN_BODY, "items/damage3.wav", 1, ATTN_NORM);
		}
	}
	return;
};


void () DropAmmo =
{
	if (self.current_slot == 1)
	{
		self.currentammo = (self.mag1 - 1);
		self.mag1 = (self.mag1 - 1);
		self.ammo_nails = self.ammo1;
	}
	if (self.current_slot == 2)
	{
		self.currentammo = (self.mag2 - 1);
		self.mag2 = (self.mag2 - 1);
		self.ammo_nails = self.ammo2;
	}
};

void()muzzleflash =
{
	WriteByte (MSG_MULTICAST, SVC_MUZZLEFLASH);
	WriteEntity (MSG_MULTICAST, self);
	multicast (self.origin, MULTICAST_PVS);
};

void () autofire =
{
	if (self.frame == 88)
		self.frame = 89;
	else
		self.frame = 88;


	if (self.weaponframe == 1)
		self.weaponframe == 2;
	else if (self.weaponframe == 2)
		self.weaponframe == 1;

	muzzleflash ();
};

void () autofire_s =
{
	if (self.frame == 88)
		self.frame = 89;
	else
		self.frame = 88;

	if (self.weaponframe == 1)
		self.weaponframe == 2;
	else if (self.weaponframe == 2)
		self.weaponframe == 1;

	muzzleflash ();
};


void () player_single1 = [ 88, player_single2 ]
{
	self.weaponframe = 1;
	muzzleflash ();
};

void () player_single2 = [ 89, player_run ]
{
	self.weaponframe = 2;
};

void () player_single1_s = [ 183, player_single2_s ]
{
	self.weaponframe = 1;
	muzzleflash ();
};

void () player_single2_s = [ 184, player_run ]
{
	self.weaponframe = 2;
};

void (vector org) bullet_hole =
{
	local float r;
	local entity ric;

	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_SPIKE);
	WriteCoord (MSG_MULTICAST, org_x);
	WriteCoord (MSG_MULTICAST, org_y);
	WriteCoord (MSG_MULTICAST, org_z);
	multicast (self.origin, MULTICAST_PHS);

	r = random();
	ric = spawn();
	setorigin(ric, org);

	if (r <= 0.20)
		sound (ric, CHAN_WEAPON, "weapons/ric1.wav", TRUE, ATTN_NORM);
	else if (r <= 0.40)
		sound (ric, CHAN_WEAPON, "weapons/ric2.wav", TRUE, ATTN_NORM);
	else if (r <= 0.60)
		sound (ric, CHAN_WEAPON, "weapons/ric3.wav", TRUE, ATTN_NORM);
	else if (r <= 0.80)
		sound (ric, CHAN_WEAPON, "weapons/ric4.wav", TRUE, ATTN_NORM);
	else
		sound (ric, CHAN_WEAPON, "weapons/ric5.wav", TRUE, ATTN_NORM);

	remove(ric);
};

void (vector test, float length, float dam) penetrate =
{
	local vector org;
	local vector org2;
	local vector start;
	local vector end;
	local vector end2;
	local float zdif;
	local float ydif;
	local float xdif;
	local float true;
	local float go;
	local float tl;


	go = 0;
	tl = 8;

	length = 32 + dam;

	while (tl < length)
	{
		makevectors (self.v_angle);
		start = (test + v_forward*tl);

		if (pointcontents (start) != CONTENT_SOLID && go == 0) //object penetrated
		{
				makevectors (self.v_angle);
				end = (test + (v_forward * 8 * length));
				traceline (start, end, FALSE, self);
				if (trace_fraction == 1) //nothing behind object
					return;

				if (trace_fraction > 0)
				{
					go = 1;

					if (trace_ent.takedamage)
					{
                                                
						if (trace_ent.solid != SOLID_BSP)
							SpawnBlood (org, 1);
	
						T_Damage (trace_ent, self, self, dam);
					}
					else
					{
						WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
						WriteByte (MSG_MULTICAST, TE_SPIKE);
						WriteCoord (MSG_MULTICAST, trace_endpos_x);
						WriteCoord (MSG_MULTICAST, trace_endpos_y);
						WriteCoord (MSG_MULTICAST, trace_endpos_z);
						multicast (trace_endpos, MULTICAST_PHS);
					}
				}
		}
		tl = tl + 4;
}
};

void (entity temp, vector org, float damage) SpawnWood =
{

		if (random()*6 <= 3)
			sound (temp, CHAN_WEAPON, "misc/woodhit.wav", TRUE, ATTN_NORM);
		else
			sound (temp, CHAN_WEAPON, "misc/woodhit2.wav", TRUE, ATTN_NORM);

		WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
		WriteByte (MSG_MULTICAST, TE_SUPERSPIKE);
		WriteCoord (MSG_MULTICAST, org_x);
		WriteCoord (MSG_MULTICAST, org_y);
		WriteCoord (MSG_MULTICAST, org_z);
		multicast (self.origin, MULTICAST_PHS);
};

void () EMPExplode =
{
	local entity te;

	self.velocity = VEC_ORIGIN;
	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_TAREXPLOSION);
	WriteCoord (MSG_BROADCAST, self.origin_x);
	WriteCoord (MSG_BROADCAST, self.origin_y);
	WriteCoord (MSG_BROADCAST, self.origin_z);
	sound (self, CHAN_BODY, "misc/flash.wav", 1, ATTN_NORM);
	sound (self, CHAN_BODY, "misc/flash.wav", 0.2, ATTN_NONE);
	te = findradius (self.origin, 300);
	while (te)
	{
		if ((((te.classname == "camera") || (te.classname == "alarm")) && (te.owner.pcamera == 0)))
		{
			te.owner.pcamera = 0;
			te.owner.pcamera2 = 0;
			te.owner.equipment_state = 0;
			sprint (self.owner, 2, te.owner.netname);
			sprint (self.owner, 2, "'s ");
			sprint (self.owner, 2, te.classname);
			sprint (self.owner, 2, " was wiped out!\n");
			remove (te);
		}
		te = te.chain;
	}
	T_RadiusDamage (self, self.owner, 45+random()*45, other, "");
	remove (self);
};

void (vector org) CreateSmoke =
{
	newmis = spawn ();
	setmodel (newmis, "progs/smoke.mdl");
	setorigin (newmis, org);
	newmis.movetype = MOVETYPE_NONE;
	newmis.solid = SOLID_NOT;
	newmis.velocity = VEC_ORIGIN;
	newmis.nextthink = (time + SVC_BIGKICK);
	newmis.think = SUB_Remove;
	newmis.touch = SUB_Null;
	newmis.classname = "smoke";
	newmis.frame = 0;
	newmis.cnt = 0;
	newmis.avelocity_x = (random () * 100);
	newmis.avelocity_y = (random () * 100);
	setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);
};


void () SmokeThink =
{
	local entity te;
	local float ct;

	self.cnt = (self.cnt + 1);

	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_GUNSHOT);
	WriteByte (MSG_MULTICAST, 2);
	WriteCoord (MSG_MULTICAST, self.origin_x);
	WriteCoord (MSG_MULTICAST, self.origin_y);
	WriteCoord (MSG_MULTICAST, self.origin_z);
	multicast (self.origin, MULTICAST_PVS);

	self.nextthink = (time + 0.33);
	if (self.cnt >= 90)
		remove (self);
};

void () FragExplode =
{
	local float r;

	sound (self, CHAN_VOICE, "ambience/gunfire7.wav", 1, ATTN_NONE);
	self.origin = (self.origin + '0 0 16');
	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_EXPLOSION);
	WriteCoord (MSG_BROADCAST, self.origin_x);
	WriteCoord (MSG_BROADCAST, self.origin_y);
	WriteCoord (MSG_BROADCAST, self.origin_z);
	r = random ();
	if ((r < 0.3))
	{
		sound (self, CHAN_BODY, "misc/exp1.wav", 1, ATTN_NORM);
	}
	if ((r < 0.65))
	{
		sound (self, CHAN_BODY, "misc/exp2.wav", 1, ATTN_NORM);
	}
	else
	{
		sound (self, CHAN_BODY, "misc/exp3.wav", 1, ATTN_NORM);
	}
	T_RadiusDamage (self, self.owner, 50+random()*50, other, "");
	remove (self);
};


void () PlasmaExplode =
{
	local float r;

	sound (self, CHAN_VOICE, "ambience/gunfire7.wav", 1, ATTN_NONE);
	self.origin = (self.origin + '0 0 16');
	self.velocity = VEC_ORIGIN;
	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_TAREXPLOSION);
	WriteCoord (MSG_BROADCAST, self.origin_x);
	WriteCoord (MSG_BROADCAST, self.origin_y);
	WriteCoord (MSG_BROADCAST, self.origin_z);
	sound (self, CHAN_BODY, "misc/flash.wav", 1, ATTN_NORM);
	T_RadiusDamage (self, self.owner, 80+random()*80, other, "");
	remove (self);
};

void () FlashExplode =
{
	local entity te;
	local float dot;
	local float dot2;
	local vector vec;

	self.velocity = VEC_ORIGIN;
	setmodel (self, "progs/blast.mdl");
	WriteByte (0, SVC_TEMPENTITY);
	WriteByte (0, WEAPON_SPIKES);
	WriteCoord (0, self.origin_x);
	WriteCoord (0, self.origin_y);
	WriteCoord (0, self.origin_z);
	sound (self, CHAN_BODY, "misc/flash.wav", 1, ATTN_NORM);
	te = findradius (self.origin, 1200);
	while (te)
	{
		if (te.classname == "raider")
			te.attack_finished = time + 6;
		if (te.classname == "radwolf")
			te.attack_finished = time + 6;

		if ((te.classname == "player"))
		{
			makevectors (te.angles);
			vec = normalize ((self.origin - te.origin));
			dot = (vec * v_forward);

			if ((((dot > 0.3) && CanDamage (self, te)) && (dot2 == 0)))
			{
				stuffcmd (te, "v_cshift 255 255 255 255\n");
				stuffcmd (te, "v_idlescale 10\n");
				te.flash = 6;
			}
		}
		te = te.chain;
	}
	remove (self);
};

void () HandGrenExplode =
{
	if ((self.cnt == 0))
	{
		FragExplode ();
	}
	else
	{
		if ((self.cnt == 1))
		{
			EMPExplode ();
		}
		else
		{
			if ((self.cnt == 2))
			{
				self.nextthink = (time + 0.5);
				self.think = SmokeThink;
			}
			else
			{
				if ((self.cnt == AS_MELEE))
				{
					FlashExplode ();
				}
				else
				{
					if ((self.cnt == WEAPON_SPIKES))
					{
						PlasmaExplode ();
					}
				}
			}
		}
	}
};

void () HandGrenBounce =
{
	local float r;

	r = (random () * TE_LIGHTNING3);

	self.velocity = self.velocity * 0.75;

	if ((r < AS_MELEE))
	{
		sound (self, CHAN_VOICE, "misc/bounce_1.wav", 0.9, ATTN_NORM);
	}
	else
	{
		if ((r < TE_LIGHTNING2))
		{
			sound (self, CHAN_VOICE, "misc/bounce_2.wav", 0.9, ATTN_NORM);
		}
		else
		{
			sound (self, CHAN_VOICE, "misc/bounce_3.wav", 0.9, ATTN_NORM);
		}
	}
};


void () FireHandGrenade =
{
      local float type;

	if (self.handgrenade <= 0)
		return;

	type = self.grenadetype;


	self.handgrenade = self.handgrenade - 1;
	self.grenadetype = 0;
	self.currentammo = 0;

	msg_entity = self;
	WriteByte (MSG_ONE, SVC_SMALLKICK);
	newmis = spawn ();
	newmis.owner = self;
	newmis.movetype = MOVETYPE_BOUNCE;
	newmis.solid = SOLID_BBOX;
	newmis.classname = "grenade";

	newmis.skin = 0;
	makevectors (self.v_angle);
	newmis.velocity = aim (self, 800);
	newmis.velocity = (newmis.velocity * 800);
	newmis.velocity_z = (newmis.velocity_z + 200);
	newmis.angles = vectoangles (newmis.velocity);
	newmis.avelocity_x = (random () * 300);
	newmis.avelocity_y = (random () * 300);
	newmis.avelocity_z = (random () * 300);
	newmis.touch = HandGrenBounce;
	newmis.nextthink = (time + 2.5);

	if (type == 1)
		newmis.think = FragExplode;
	if (type == 2)
		newmis.think = EMPExplode;
	if (type == 3)
		newmis.think = SmokeThink;
	if (type == 4)
		newmis.think = FlashExplode;

	newmis.frame = 1;
	setmodel (newmis, "progs/handgren.mdl");
	setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);
	setorigin (newmis, ((self.origin + (v_right * TE_BLOOD)) + (v_up * TE_BLOOD)));
};



void(float slot, float magazine) GiveAmmo =
{
		if (slot == 1)
		{
			self.maxmag1 = magazine;
			self.ammo1 = magazine * 4;
			self.mag1 = magazine;
		}
		if (slot == 2)
		{
			self.maxmag2 = magazine;
			self.ammo2 = magazine * 4;
			self.mag2 = magazine;
		}
};


void (float dam, float rec, string snd, float rng, float rate) FirePistol =
{
      local float var, var1, var2, zdif, xdif, ydif, true;
      local vector dir, source, targ, org, org2, adjust;
	local string x;


	stuffcmd(self, "-attack\n");
	sound (self, CHAN_WEAPON, snd, 1, ATTN_NORM);
	self.attack_finished = (time + rate);


	if (self.attack == 0 && self.position == POS_STAND)
		player_single1 ();
	if (self.attack == 0 && self.position >= POS_DUCK)
		player_single1_s ();

	if (self.position == 0)
		adjust = '0 0 0';
	if (self.position == 1)
		adjust = '0 0 -16';
	if (self.position == 2)
		adjust = '0 0 -32';


	DropAmmo ();
	makevectors (self.v_angle);
	if (self.recoil >= 15)
		self.recoil = 15;

	if (self.attack >= 1)
	{
		if (self.position == 0)
			player_single1 ();
		if (self.position == 1)
			player_single1_s ();
		if (self.position == 2)
			player_single1 ();
	}
	if (self.attack >= 1)
	{
		if (self.position == 0)
			autofire ();
		if (self.position == 1)
			autofire_s ();
		if (self.position == 2)
			player_single1 ();
	}

	var = 50;

	if (self.velocity != '0 0 0')
		var = 400;

	var = var + (40 * self.recoil);

	if (self.attack <= 3 && self.position == 1 && self.velocity_z == 0)
		var = (var * 0.75);

	if (self.attack <= 3 && self.position == 2 && self.velocity_z == 0)
		var = (var * 0.5);

	self.attack = self.attack + 1;
	self.recoil = self.recoil + 4;

	source = self.origin + '0 0 22';

	targ = self.origin + '0 0 22' + v_right*crandom()* var + v_up*crandom()*var;

	traceline (source+adjust, targ+adjust+v_forward*4000, FALSE, self);
	if (trace_fraction == 1)
		return;

	org = trace_endpos - v_forward * 1;
	org2 = trace_endpos + (v_forward * ((trace_ent.size_y / 2) + (trace_ent.size_x / 2)));
	if (trace_ent.takedamage)
	{
		org2 = (trace_endpos + (v_forward * ((trace_ent.size_y / 2) + (trace_ent.size_x / 2))));
		zdif = org_z - trace_ent.origin_z;
		ydif = org2_y - trace_ent.origin_y;
		xdif = org2_x - trace_ent.origin_x;
		true = 0;

		if (((ydif >= CONTENT_SKY) && (ydif <= TE_LIGHTNING2)))
			true = 1;
		if (((xdif >= CONTENT_SKY) && (xdif <= TE_LIGHTNING2)))
			true = 1;
		if (self.attack <= 5 && true == 1 && zdif >= (trace_ent.size_z / 2 * 0.8))
			self.critical = 3;

		dam = (dam * (1 - trace_fraction));
		if (trace_ent.solid != SOLID_BSP)
			SpawnBlood (org, 1);

		if (trace_ent.solid == SOLID_BSP)
			SpawnWood (trace_ent, org, 1);

		T_Damage (trace_ent, self, self, dam);

		if (trace_ent.solid == SOLID_BSP)
			penetrate (org, (dam / 2), (dam / 2));
	}
	else
	{
		bullet_hole (org);
		dir = vectoangles (source - targ);
		penetrate (org, (dam / 2), (dam / 2));
		return;
	}
};

void (float dam, float rec, string snd, float rng, float rate) FireSMG =
{
      local float var, var1, var2, zdif, xdif, ydif, true;
      local vector dir, source, targ, org, org2, adjust;
	local string x;


	sound (self, CHAN_WEAPON, snd, 1, ATTN_NORM);
	self.attack_finished = (time + rate);


	if (self.attack == 0 && self.position == POS_STAND)
		player_single1 ();
	if (self.attack == 0 && self.position >= POS_DUCK)
		player_single1_s ();

	if (self.position == 0)
		adjust = '0 0 0';
	if (self.position == 1)
		adjust = '0 0 -16';
	if (self.position == 2)
		adjust = '0 0 -32';


	DropAmmo ();
	makevectors (self.v_angle);
	if (self.recoil >= 15)
		self.recoil = 15;

	if (self.attack >= 1)
	{
		if (self.position == 0)
			player_single1 ();
		if (self.position == 1)
			player_single1_s ();
		if (self.position == 2)
			player_single1 ();
	}
	if (self.attack >= 1)
	{
		if (self.position == 0)
			autofire ();
		if (self.position == 1)
			autofire_s ();
		if (self.position == 2)
			player_single1 ();
	}

	var = 200;

	if (self.velocity != '0 0 0')
		var = 200;

	var = var + (40 * self.recoil);

	if (self.attack <= 3 && self.position == 1 && self.velocity_z == 0)
		var = (var * 0.75);

	if (self.attack <= 3 && self.position == 2 && self.velocity_z == 0)
		var = (var * 0.5);

	self.attack = self.attack + 1;
	self.recoil = self.recoil + 3;

	source = self.origin + '0 0 22';

	targ = self.origin + '0 0 22' + v_right*crandom()* var + v_up*crandom()*var;

	traceline (source+adjust, targ+adjust+v_forward*4000, FALSE, self);
	if (trace_fraction == 1)
		return;

	org = trace_endpos - v_forward * 2;
	org2 = trace_endpos + (v_forward * ((trace_ent.size_y / 2) + (trace_ent.size_x / 2)));
	if (trace_ent.takedamage)
	{
		org2 = (trace_endpos + (v_forward * ((trace_ent.size_y / 2) + (trace_ent.size_x / 2))));
		zdif = org_z - trace_ent.origin_z;
		ydif = org2_y - trace_ent.origin_y;
		xdif = org2_x - trace_ent.origin_x;
		true = 0;
		if (((ydif >= CONTENT_SKY) && (ydif <= TE_LIGHTNING2)))
			true = 1;
		if (((xdif >= CONTENT_SKY) && (xdif <= TE_LIGHTNING2)))
			true = 1;
		if (self.attack <= 5 && true == 1 && zdif >= (trace_ent.size_z / 2 * 0.8))
			self.critical = 3;

		dam = (dam * (1 - trace_fraction));
		if (trace_ent.solid != SOLID_BSP)
			SpawnBlood (org, 1);

		if (trace_ent.solid == SOLID_BSP)
			SpawnWood (trace_ent, org, 1);

		T_Damage (trace_ent, self, self, dam);

		if (trace_ent.solid == SOLID_BSP)
			penetrate (org, (dam / 2), (dam / 2));
	}
	else
	{
		bullet_hole (org);
		dir = vectoangles (source - targ);
		penetrate (org, (dam / 2), (dam / 2));
		return;
	}
};

void (float dam, float rec, string snd, float rng, float rate) FireAssaultRifle =
{
      local float var, var1, var2, zdif, xdif, ydif, true;
      local vector dir, source, targ, org, org2, adjust;
	local string x;

	sound (self, CHAN_WEAPON, snd, 1, ATTN_NORM);

	self.attack_finished = (time + rate);


	if (self.attack == 0 && self.position == POS_STAND)
		player_single1 ();
	if (self.attack == 0 && self.position >= POS_DUCK)
		player_single1_s ();
	if (self.attack >= 1 && self.position == POS_STAND)
		autofire ();
	if (self.attack >= 1 && self.position >= POS_DUCK)
		autofire_s ();

	if (self.position == 0)
		adjust = '0 0 0';
	if (self.position == 1)
		adjust = '0 0 -16';
	if (self.position == 2)
		adjust = '0 0 -32';


	DropAmmo ();
	makevectors (self.v_angle);
	if (self.recoil >= 15)
		self.recoil = 15;

	if (self.attack >= 1)
	{
		if (self.position == 0)
			player_single1 ();
		if (self.position == 1)
			player_single1_s ();
		if (self.position == 2)
			player_single1 ();
	}
	if (self.attack >= 1)
	{
		if (self.position == 0)
			autofire ();
		if (self.position == 1)
			autofire_s ();
		if (self.position == 2)
			player_single1 ();
	}

	var = 50;

	if (self.velocity != '0 0 0')
		var = 400;

	var = var + (40 * self.recoil);

	if (self.attack <= 3 && self.position == 1 && self.velocity_z == 0)
		var = (var * 0.75);

	if (self.attack <= 3 && self.position == 2 && self.velocity_z == 0)
		var = (var * 0.5);

	self.attack = self.attack + 1;
	self.recoil = self.recoil + 5;

	source = self.origin + '0 0 22';

	targ = self.origin + '0 0 22' + v_right*crandom()* var + v_up*crandom()*var;

	traceline (source+adjust, targ+adjust+v_forward*4000, FALSE, self);
	if (trace_fraction == 1)
		return;

	org = trace_endpos - v_forward * 2;
	org2 = trace_endpos + (v_forward * ((trace_ent.size_y / 2) + (trace_ent.size_x / 2)));
	if (trace_ent.takedamage)
	{
		org2 = (trace_endpos + (v_forward * ((trace_ent.size_y / 2) + (trace_ent.size_x / 2))));
		zdif = org_z - trace_ent.origin_z;
		ydif = org2_y - trace_ent.origin_y;
		xdif = org2_x - trace_ent.origin_x;
		true = 0;
		if (((ydif >= CONTENT_SKY) && (ydif <= TE_LIGHTNING2)))
			true = 1;
		if (((xdif >= CONTENT_SKY) && (xdif <= TE_LIGHTNING2)))
			true = 1;
		if (self.attack <= 5 && true == 1 && zdif >= (trace_ent.size_z / 2 * 0.8))
			self.critical = 3;

		dam = (dam * (1 - trace_fraction));
		if (trace_ent.solid != SOLID_BSP)
			SpawnBlood (org, 1);

		if (trace_ent.solid == SOLID_BSP)
			SpawnWood (trace_ent, org, 1);

		T_Damage (trace_ent, self, self, dam);

		if (trace_ent.solid == SOLID_BSP)
			penetrate (org, (dam / 2), (dam / 2));
	}
	else
	{
		bullet_hole (org);
		dir = vectoangles (source - targ);
		penetrate (org, (dam / 2), (dam / 2));
		return;
	}
};


void () Screenshake =
{
	local entity te;

	te = findradius (self.origin, 700);
	while (te)
	{
		if (((te.classname == "player") && (te.ghost == 0)))
		{
			stuffcmd (te, "v_iyaw_level 2\n");
			stuffcmd (te, "v_iyaw_cycle 32\n");
			stuffcmd (te, "v_ipitch_level 2\n");
			stuffcmd (te, "v_ipitch_cycle 32\n");
			stuffcmd (te, "v_iroll_level 2\n");
			stuffcmd (te, "v_iroll_cycle 32\n");
			stuffcmd (te, "v_idlescale 1\n");
		}
		te = te.chain;
	}
};

void () ScreenshakeSingle =
{
	stuffcmd (self, "v_iyaw_level 2\n");
	stuffcmd (self, "v_iyaw_cycle 32\n");
	stuffcmd (self, "v_ipitch_level 2\n");
	stuffcmd (self, "v_ipitch_cycle 32\n");
	stuffcmd (self, "v_iroll_level 2\n");
	stuffcmd (self, "v_iroll_cycle 32\n");
	stuffcmd (self, "v_idlescale 1\n");
};

void () ExplosionFrames =
{
	self.avelocity = '300 300 250';
	self.nextthink = (time + 0.02);
	self.frame = (self.frame + 1);
	if (self.frame == 16)
		remove (self);
};

void (float input) Explosion =
{
	local float r;

	self.effects = EF_DIMLIGHT;
	self.touch = SUB_Null;
	setmodel (self, "progs/blast.mdl");
	r = random ();
	if ((r < 0.3))
		sound (self, CHAN_BODY, "misc/exp1.wav", 1, ATTN_NORM);
	if ((r < 0.65))
		sound (self, CHAN_BODY, "misc/exp2.wav", 1, ATTN_NORM);
	else
		sound (self, CHAN_BODY, "misc/exp3.wav", 1, ATTN_NORM);

	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_EXPLOSION);
	WriteCoord (MSG_BROADCAST, self.origin_x);
	WriteCoord (MSG_BROADCAST, self.origin_y);
	WriteCoord (MSG_BROADCAST, self.origin_z);
	Screenshake ();
	self.frame = AS_MELEE;
	self.velocity = VEC_ORIGIN;
	self.avelocity = '300 300 250';
	self.think = ExplosionFrames;
	self.nextthink = (time + 0.02);
};


void () WeaponTouch =
{
	if (other.classname != "player")
		return;

	if (other.ghost != 0)
		return;

	if (other.current_slot == WEAPON_SPIKES)
	{
		return;
	}
	if (other.current_slot == AS_MELEE)
		return;

	if (self.handgrenade == 1)
	{
		if (other.handgrenade > 0)
		{
			sprint(other, 2, "already have a grenade.\n");
			return;
		}

		sound (other, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);
		other.handgrenade = self.handgrenade;
		other.grenadetype = self.grenadetype;
		sprint(other, 2, "picked up a grenade.\n");
		remove(self);
		return;
	}

	if (((other.current_slot == 1) && (other.slot1 != 0)))
		return;

	if (((other.current_slot == 2) && (other.slot2 != 0)))
		return;

		sound (other, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);

	if ((other.current_slot == 1))
	{
		GetWeaponWeight (other, self.cnt);
		other.slot1 = self.cnt;
		stuffcmd (other, "impulse 1\n");
		other.ammo1 = self.slot1;
		other.maxmag1 = self.slot2;
		other.mag1 = self.class;
		GetWeaponModel (self.slot1);
		remove (self);
	}
	if ((other.current_slot == 2))
	{
		GetWeaponWeight (other, self.cnt);
		other.slot2 = self.cnt;
		stuffcmd (other, "impulse 2\n");
		other.ammo2 = self.slot1;
		other.maxmag2 = self.slot2;
		other.mag2 = self.class;
		GetWeaponModel (self.slot2);
		remove (self);
	}
};

void (float weap, float snd, float drop) DropWeapon =
{
	local string mdel;

	if (self.attack_finished > time && drop == 0)
		return;

	if (self.current_slot == 3 && self.handgrenade == 0)
		return;

	if (self.current_slot == 4 && self.c4 == 0)
		return;

	if (weap == 0)
		return;

	if (snd == 1)
		sound (self, CHAN_WEAPON, "weapons/lock4.wav", 1, ATTN_NORM);

	makevectors (self.v_angle);
	newmis = spawn ();
	newmis.owner = self;
	newmis.classname = "dropped_weapon";
	newmis.movetype = MOVETYPE_TOSS;
	newmis.solid = SOLID_TRIGGER;
	newmis.velocity = aim (self, 500);
	newmis.velocity = (newmis.velocity * 500);
	newmis.angles_y = (random () * 360);

	mdel = "progs/w_1911.mdl";

	if (weap == 1)
		mdel = "progs/w_knife.mdl";
	if (weap == 2)
		mdel = "progs/w_knife.mdl";
	if (weap == 3)
		mdel = "progs/w_axe.mdl";
	if (weap == 4)
		mdel = "progs/w_axe.mdl";

	if (weap == 5)
		mdel = "progs/w_1911.mdl";
	if (weap == 6)
		mdel = "progs/w_deagle.mdl";
	if (weap == 7)
		mdel = "progs/w_1911.mdl";
	if (weap == 8)
		mdel = "progs/w_alien.mdl";

	if (weap == 9)
		mdel = "progs/w_pipe.mdl";
	if (weap == 10)
		mdel = "progs/w_shotgun.mdl";
	if (weap == 11)
		mdel = "progs/w_pipe.mdl";
	if (weap == 12)
		mdel = "progs/w_jackhammer.mdl";

	if (weap == 13)
		mdel = "progs/w_mp9.mdl";
	if (weap == 14)
		mdel = "progs/w_mp7.mdl";

	if (weap == 15)
		mdel = "progs/w_rangem.mdl";
	if (weap == 16)
		mdel = "progs/w_ak47.mdl";
	if (weap == 17)
		mdel = "progs/w_ak47.mdl";
	if (weap == 18)
		mdel = "progs/w_srifle.mdl";
	if (weap == 19)
		mdel = "progs/w_night.mdl";
	if (weap == 20)
		mdel = "progs/w_sa80.mdl";
	if (weap == 21)
		mdel = "progs/w_gauss.mdl";
	if (weap == 22)
		mdel = "progs/w_carbine.mdl";
	if (self.current_slot == 3)
		mdel = "progs/grenade2.mdl";

	if (self.current_slot == 4 && self.c4 == 1)
		mdel = "progs/c4.mdl";

	setmodel (newmis, mdel);
	setsize (newmis, '-2 -2 0', '2 2 1');
	makevectors (self.v_angle);
	traceline (self.origin, ((self.origin + (v_forward * IT_LIGHTNING)) + '0 0 32'), FALSE, self);
	trace_endpos = (trace_endpos - (v_forward * WEAPON_SPIKES));
	setorigin (newmis, trace_endpos);
	newmis.origin_z = self.origin_z;
	newmis.nextthink = (time + 180);
	newmis.think = SUB_Remove;
	newmis.touch = WeaponTouch;
	newmis.cnt = weap;
	if (self.current_slot == 1)
	{
		self.attack_finished = time;
		self.slot1_weight = 0;
		self.slot1 = 0;
		newmis.slot1 = self.ammo1;
		newmis.slot2 = self.maxmag1;
		newmis.class = self.mag1;
		self.mag1 = 0;
		self.maxmag1 = 0;
		GetWeaponModel ();
		stuffcmd(self, "impulse 1\n");
	}
	if (self.current_slot == 2)
	{
		self.attack_finished = time;
		self.slot2_weight = 0;
		self.slot2 = 0;
		newmis.slot1 = self.ammo2;
		newmis.slot2 = self.maxmag2;
		newmis.class = self.mag2;
		self.mag2 = 0;
		self.maxmag2 = 0;
		GetWeaponModel ();
		stuffcmd(self, "impulse 2\n");
	}
	if (self.current_slot == 3)
	{
		self.attack_finished = time;
		newmis.handgrenade = self.handgrenade;
		newmis.grenadetype = self.grenadetype;

		stuffcmd(self, "impulse 1\n");
	}
};

void () GetArmorWeight =
{
	local float wt;

	if (self.armor == 0)
		wt = 0;
	if (self.armor == 1)
		wt = 3;
	if (self.armor == 2)
		wt = 5;
	if (self.armor == 3)
		wt = 9;
	if (self.armor == 4)
		wt = 15;
	if (self.armor == 5)
		wt = 12;
	if (self.armor == 6)
		wt = 17;
	if (self.armor == 7)
		wt = 6;
	if (self.armor == 8)
		wt = 20;

	self.armor_weight = wt;
};

void (entity guy, float slot) GetWeaponWeight =
{
	local float wt;

	if (slot == 1)
		wt = 1;
	else if (slot == 2)
		wt = 2;
	else if (slot == 3)
		wt = 8;
	else if (slot == 4)
		wt = 6;
	else if (slot == 5)
		wt = 1;
	else if (slot == 6)
		wt = 2;
	else if (slot == 7)
		wt = 2;
	else if (slot == 8)
		wt = 2;
	else if (slot == 9)
		wt = 3;
	else if (slot == 10)
		wt = 4;
	else if (slot == 11)
		wt = 5;
	else if (slot == 12)
		wt = 6;
	else if (slot == 13)
		wt = 3;
	else if (slot == 14)
		wt = 3;
	else if (slot == 15)
		wt = 5;
	else if (slot == 16)
		wt = 5;
	else if (slot == 17)
		wt = 5;
	else if (slot == 18)
		wt = 7;
	else if (slot == 19)
		wt = 5;
	else if (slot == 20)
		wt = 5;
	else if (slot == 21)
		wt = 9;
	else if (slot == 22)
		wt = 10;
	else if (slot == 23)
		wt = 1;

	if (self.current_slot == 1)
		self.slot1_weight = wt;
	if (self.current_slot == 2)
		self.slot2_weight = wt;
};



void (float slot) WeaponAmmo =
{
	local float weap, amount;

	if (slot == 1)
		weap = self.slot1;
	if (slot == 2)
		weap = self.slot2;

	if (weap <= 4)
		amount = 0;
	else if (weap == 5)
		amount = 12;
	else if (weap == 6)
		amount = 7;
	else if (weap == 7)
		amount = 15;
	else if (weap == 8)
		amount = 6;
	else if (weap == 9)
		amount = 1;
	else if (weap == 10)
		amount = 2;
	else if (weap == 11)
		amount = 6;
	else if (weap == 12)
		amount = 10;
	else if (weap == 13)
		amount = 30;
	else if (weap == 14)
		amount = 30;
	else if (weap == 15)
		amount = 10;
	else if (weap == 16)
		amount = 24;
	else if (weap == 17)
		amount = 30;
	else if (weap == 18)
		amount = 8;
	else if (weap == 19)
		amount = 30;
	else if (weap == 20)
		amount = 30;
	else if (weap == 21)
		amount = 10;
	else if (weap == 22)
		amount = 40;

	if (slot == 1)
		GiveAmmo (1, amount);
	if (slot == 2)
		GiveAmmo (2, amount);
};

void() Crosshair =
{
	local float r;
	local string new;

	r = 32 + (self.recoil*8);
	if (r > 256)
		r = 256;
	if (r < 0)
		r = 0;

	new = ftos(r);	
	stuffcmd(self, "crosshairsize ");
	stuffcmd(self, new);
	stuffcmd(self, "\n");
	r = 0.75 - (self.recoil*0.03);
	if (r <= 0.25)
		r = 0.25;

	new = ftos(r);	
	stuffcmd(self, "crosshairalpha ");
	stuffcmd(self, new);
	stuffcmd(self, "\n");
};


string () GetChemName =
{
	if (self.chem == 1)
		return "stimpack";
	if (self.chem == 2)
		return "medical bag";
	if (self.chem == 3)
		return "superstim";
	if (self.chem == 4)
		return "adrenaline";
	if (self.chem == 5)
		return "psycho";
	if (self.chem == 6)
		return "berserk";
};

void (entity healer, entity saved) RevivePlayer =
{
	local entity oself;

	saved.deadflag = DEAD_NO;
	saved.takedamage = DAMAGE_AIM;
	saved.movetype = MOVETYPE_WALK;
	saved.solid = SOLID_SLIDEBOX;
	saved.ghost = 0;
	saved.health = 2;
	saved.air_finished = time + 10;
	saved.view_ofs = '0 0 22';
	oself = self;
	self = saved;
	player_run();
	self = oself;
	stuffcmd(saved, "impulse 1\n");
	sprint (healer, PRINT_HIGH, "you revive ");
	sprint (healer, PRINT_HIGH, trace_ent.netname);
	sprint (healer, PRINT_HIGH, ".\n ");
	sprint (saved, PRINT_HIGH, healer.netname);
	sprint (saved, PRINT_HIGH, " saves you from death.\n");
	saved.view2 = world;
};

void(float type) UseBoostingChem =
{
	local vector source;
	local vector org;
	local string x;

	x = GetChemName();

	if (self.attack_finished > time)
		return;

	self.attack_finished = time + 1;

	makevectors (self.v_angle);
	source = self.origin + '0 0 0';
	traceline (source, source + v_forward*64, FALSE, self);
	if (trace_fraction == 1.0)
	{
		if (self.health < self.max_health && self.rage == 0)
		{
			sound (self, CHAN_BODY, "player/berserk.wav", 1, ATTN_NORM);
			self.rage = type;
			self.ragetime = (20+type)+random()*30;
			return;
		}
	}
	if (trace_ent.classname == "player" && trace_ent.team == self.team)
	{
		if (trace_ent.health <= 0)
			return;

		if (trace_ent.rage >= 1)
		{
			sprint (self, 2, trace_ent.netname);
			sprint (self, PRINT_HIGH, " is already affected.\n");
			return;
		}
		sprint (trace_ent, 2, self.netname);
		sprint (trace_ent, PRINT_HIGH, " used a ");
		sprint (trace_ent, PRINT_HIGH, x);
		sprint (trace_ent, PRINT_HIGH, " on you.\n");
		sound (trace_ent, CHAN_BODY, "player/berserk.wav", 1, ATTN_NORM);
		trace_ent.rage = type;
		trace_ent.ragetime = (20+type)+random()*30;
	}
};

void(float type) UseHealingChem =
{
	local vector source;
	local vector org;
	local float heal;
	local string x;

	x = GetChemName();
	if (type == 1)
		x = "bandage";

	if (self.attack_finished > time)
		return;

	heal = type*5;

	self.attack_finished = time + 1;

	makevectors (self.v_angle);
	source = self.origin + '0 0 0';
	traceline (source, source + v_forward*64, FALSE, self);
	if (trace_fraction == 1.0)
	{
		if (self.health >= self.max_health)
		{
			sprint(self, 2, "you aren't injured.\n");
			return;
		}
		if (self.regen >= 1)
		{
			sprint (self, PRINT_HIGH, "you are already healing.\n");
			return;
		}
		if (self.health < self.max_health && self.regen == 0)
		{
			sound (self, CHAN_BODY, "items/r_item2.wav", 1, ATTN_NORM);
			self.health = self.health + heal;
			self.regen = heal;
			sprint (self, PRINT_HIGH, "used a ");
			sprint (self, PRINT_HIGH, x);
			sprint (self, PRINT_HIGH, " to heal yourself.\n");
			self.chemcount = self.chemcount - 1;
			return;
		}
	}
	if (trace_ent.classname == "player" && trace_ent.team == self.team)
	{
		if (trace_ent.health <= 0 && coop == 1)
		{
			RevivePlayer(self, trace_ent);
			return;
		}
		if (trace_ent.health <= 0 && coop == 0)
			return;

		if (trace_ent.regen >= 1)
		{
			sprint (self, 2, trace_ent.netname);
			sprint (self, PRINT_HIGH, " is already healing.\n");
			return;
		}
		if (trace_ent.health >= trace_ent.max_health)
		{
			sprint(self, 2, trace_ent.netname);
			sprint(self, 2, " isn't injured.\n");
			return;
		}
		sprint (trace_ent, 2, self.netname);
		sprint (trace_ent, PRINT_HIGH, " used a ");
		sprint (trace_ent, PRINT_HIGH, x);
		sprint (trace_ent, PRINT_HIGH, " to heal you.\n");

		trace_ent.regen = heal;
		trace_ent.health = trace_ent.health + heal;
		stuffcmd (trace_ent, "v_cshift 0 0 0 0\n");
		sound (self, CHAN_BODY, "items/r_item2.wav", 1, ATTN_NORM);
		self.chemcount = self.chemcount - 1;
	}
};

void() UseChem =
{
	if (self.chemcount <= 0 || self.chem == 0)
	{
		self.chem = 0;
		self.chemcount = 0;
		sound (self, CHAN_BODY, "misc/menu3.wav", 1, ATTN_IDLE);
		sprint(self, 2, "you are out of chems!\n");
		return;
	}

	if (self.chem == 1)//stimpack
		UseHealingChem(2);
	if (self.chem == 2)//medical bag
		UseHealingChem(3);
	if (self.chem == 3)//super-stim
		UseHealingChem(4);
	if (self.chem == 4)//adrenaline
		UseBoostingChem(1);
	if (self.chem == 5)//jet
		UseBoostingChem(2);
	if (self.chem == 6)//psycho
		UseBoostingChem(3);
	if (self.chem == 7)//mentats
		UseBoostingChem(3);

	if (self.chemcount == 0)
		self.chem = 0;

	self.rtime = 100;
};

void() Bandage =
{
	if (self.bandages <= 0)
	{
		self.bandages = 0;
		sound (self, CHAN_BODY, "misc/menu3.wav", 1, ATTN_IDLE);
		sprint(self, 2, "you are out of bandages!\n");
		return;
	}

	UseHealingChem(1);

	self.chemcount = self.chemcount - 1;
	if (self.chemcount == 0)
		self.chem = 0;
};

void () DisplayMenu =
{
	local entity spot;
	local string menu;
	local entity te;
	local string qq;

	if (self.currentmenu == "none")
		return;

	if (self.currentmenu == "shop_list")
	{
		menu = ShopString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "shop_trait")
	{
		menu = TraitString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "shop_perks")
	{
		menu = PerkString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "shop_armor")
	{
		menu = ArmorString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "shop_protect")
	{
		menu = ProtectString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "shop_melee")
	{
		menu = MeleeString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "shop_thrown")
	{
		menu = ThrownString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "shop_pistols")
	{
		menu = PistolString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "shop_shotguns")
	{
		menu = ShotgunString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "shop_rifles")
	{
		menu = RifleString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "shop_chems")
	{
		menu = ChemString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "shop_other")
	{
		menu = OtherString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "shop_equipment")
	{
		menu = EquipmentString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "shop_weapons")
	{
		menu = WeaponString ();
		centerprint (self, menu);
	}
	if (((self.currentmenu == "select_skill") && (self.team == 1)))
	{
		centerprint (self, "êCHOOSE SKILL SETë\n\nê1ë Medic    \nê2ë Assassin \nê3ë Soldier  \nê4ë Scientist\n");
	}
	if (((self.currentmenu == "select_skill") && (self.team == 2)))
	{
		centerprint (self, "êCHOOSE SKILL SETë\n\nê1ë Medic    \nê2ë Assassin \nê3ë Soldier  \nê4ë Scientist\n");
	}
	if ((self.currentmenu == "select_team"))
	{
		if (self.class == 0)
			centerprint (self, "êCHOOSE YOUR TEAMë\n\nê1ë Rangers     (good)\nê2ë Raiders     (evil)\nê3ë Auto-Assign       \n");
		if (self.class >= 0 && self.oldteam == 0)
			centerprint (self, "êCHOOSE YOUR TEAMë\n\nê1ë Rangers     (good)\nê2ë Raiders     (evil)\nê3ë Auto-Assign       \n");
		if (self.class > 0 && self.oldteam > 0)
			centerprint (self, "êCHOOSE YOUR TEAMë\n\nê1ë Rangers     (good)\nê2ë Raiders     (evil)\nê3ë Auto-Assign       \nê4ë Keep Previous     \n");
	}
	if (self.currentmenu == "confirm_team")
	{
		sound (self, CHAN_BODY, "player/yourturn.wav", 1, ATTN_NORM);

		if (self.team == 1)
			centerprint (self, "êyou will respawn asë\n\nRanger - OK?\nê1ë Yes     \nê2ë No      \n");
		if (self.team == 2)
			centerprint (self, "êyou will respawn asë\n\nRaider - OK?\nê1ë Yes     \nê2ë No      \n");
	}
	if (self.currentmenu == "confirm_skill")
	{
		sound (self, CHAN_BODY, "player/yourturn.wav", 1, ATTN_NORM);

		if (self.class == 1)
			centerprint (self, "êyour class will beë\n\nMedic - OK?\nê1ë Yes     \nê2ë No      \n");
		if (self.class == 2)
			centerprint (self, "êyour class will beë\n\nAssassin - OK?\nê1ë Yes     \nê2ë No      \n");
		if (self.class == 3)
			centerprint (self, "êyour class will beë\n\nSoldiier - OK?\nê1ë Yes     \nê2ë No      \n");
		if (self.class == 4)
			centerprint (self, "êyour class will beë\n\nScientist - OK?\nê1ë Yes     \nê2ë No      \n");
	}

	if (self.currentmenu == "menu_build")
	{
		menu = BuildString ();
		centerprint (self, menu);
	}
};


void () Special =
{
	if (self.class == 1)
		Bandage ();
	if (self.class == 2)
		Sneak ();
	if (self.class == 4)
		self.currentmenu = "menu_build";
};

void () hos_run1;
void () hos_stand1;
void (vector org, entity guy, float input, float cost) spawn_station;

void () ExitScreen =
{
	local float car;
	local float q;
	local entity te;
	local string x;


	if (self.class == 0)
		return;

	if (self.ghost == 1)
		return;

	if (self.team == 0)
		return;

	if (self.attack_finished > time)
		return;

	if (trace_ent.classname == "hostage" && trace_ent.health > 0 && trace_fraction < 1)
	{
		if (self.team != 1)
			return;

		if (self.currentmenu == "menu_build")
			return;

		if (trace_ent.cnt == 0)
		{
			sprint (self, 2, "hostage is now following you.\n");
			trace_ent.nextthink = (time + 0.1);
			trace_ent.think = hos_run1;
			trace_ent.cnt = 1;
			trace_ent.friend = self;
			return;
		}
		else
		{
			if (trace_ent.cnt == 1)
			{
				sprint (self, 2, "hostage stopped following you.\n");
				trace_ent.nextthink = (time + 0.1);
				trace_ent.think = hos_stand1;
				trace_ent.cnt = 0;
				trace_ent.friend = trace_ent;
				return;
			}
		}
		return;
	}

	if ((self.currentmenu != "none"))
	{
		centerprint (self, "\n");
		self.currentmenu = "none";
	}
};

void () Sneak =
{
	local float w;

	w = weightx();

	if (self.sneak >= 1)
	{
		sound (self, CHAN_BODY, "items/inv1.wav", 1, ATTN_NORM);
		centerprint (self, "ç Uncloaked ç\n");
		setmodel (self, "progs/guy.mdl");
		self.sneak = 0;
		return;
	}
	if (self.ammo_cells < 20)
	{
		sprint (self, PRINT_HIGH, "wait for stealth-boy to recharge.\n");
		return;
	}
	if (self.sneak == 0 && self.class == 2)
	{
		sound (self, CHAN_BODY, "items/inv1.wav", 1, ATTN_NORM);
		centerprint (self, "ç cloaked and sneaking ç\n(1% detection chance)");
		self.sneak = 1;
		return;
	}
	else if (self.sneak == 0 && self.class != 2)
	{
		sound (self, CHAN_BODY, "items/inv1.wav", 1, ATTN_NORM);
		centerprint (self, "ç cloaked ç\n(15% detection chance)");
		self.sneak = 3;
		return;
	}
	else if (self.sneak == 0 && w > 20)
	{
		sound (self, CHAN_BODY, "items/inv1.wav", 1, ATTN_NORM);
		centerprint (self, "ç too much gear ç\n");
		setmodel (self, "progs/guy.mdl");
		self.sneak = 0;
		return;
	}
};

void () Shield =
{
	if (self.class != 6)
	{
		centerprint (self, "You can't shield yourself!\n");
		return;
	}
	if (self.sneak == 2)
	{
		centerprint (self, "ççç Unshielded ççç\n");
		self.sneak = 0;
		sound (self, CHAN_BODY, "items/protect2.wav", 1, ATTN_NORM);
		return;
	}
	if (self.ammo_cells < 10)
	{
		centerprint (self, "wait for your shield to recharge.\n");
		return;
	}
	if (self.sneak == 0)
	{
		centerprint (self, "ççç Energy Shield ççç\n");
		self.sneak = 2;
		sound (self, CHAN_BODY, "items/protect.wav", 1, ATTN_NORM);
		return;
	}

};

void () station_die =
{
	if ((self.buildtype == 1))
	{
		if ((self.team == 1))
		{
			blue_weapon = 0;
		}
		else
		{
			if ((self.team == 2))
			{
				red_weapon = 0;
			}
		}
	}
	if ((self.buildtype == 2))
	{
		if ((self.team == 1))
		{
			blue_armor = 0;
		}
		else
		{
			if ((self.team == 2))
			{
				red_armor = 0;
			}
		}
	}
	if ((self.buildtype == AS_MELEE))
	{
		if ((self.team == 1))
		{
			blue_gadget = 0;
		}
		else
		{
			if ((self.team == 2))
			{
				red_gadget = 0;
			}
		}
	}
	Explosion (2);
};

void () station_think =
{
	local entity dog;
	local entity te;
	local string qq;
	local float zz, x;

	self.nextthink = time + 2;
	self.frame = self.buildtype;

	if (self.track.team != self.team)
	{
		station_die ();
		return;
	}

	if (self.chemcount <= 0)
	{
		station_die ();
		return;
	}

	if (self.buildtype == 1)//barricade
	{
		sound (self, CHAN_BODY, "items/protect2.wav", 1, ATTN_NORM);

		te = findradius (self.origin, 256);
		while (te)
		{
			if (te.classname == "player" && te.team == self.team)
			{
				if (self.chemcount <= 0)
				{
					sound (self, CHAN_BODY, "misc/menu2.wav", TRUE, ATTN_NORM);
					sprint (te, 2, "the shield generator is out of power.\n");
					return;
				}
				if (te.classname == "player")
				{
					te.protect = 3;
					self.chemcount = self.chemcount - 1;
					stuffcmd(te, "v_cshift 0 100 100 100\n");
				}
			}
			te = te.chain;
		}
	}
	if (self.buildtype == 2)//autodoc
	{
		te = findradius (self.origin, 70);
		while (te)
		{
			if (te.classname == "player" && te.team == self.team)
			{
				if (self.chemcount <= 0)
				{
					sound (self, CHAN_BODY, "misc/item1.wav", TRUE, ATTN_NORM);
					sprint (te, 2, "the autodoc is out of medical supplies.\n");
					return;
				}
				if (te.health < te.max_health)
				{
					sound (self, CHAN_BODY, "items/r_item2.wav", 1, ATTN_NORM);
					sprint (te, 2, "the auto-doc heals you for 3 health.\n");
					te.health = te.health + 3;
					self.chemcount = self.chemcount - 1;
					if (te.health > te.max_health)
						te.health = te.max_health;
				}
			}
			te = te.chain;
		}
	}

	if (self.buildtype == 0)//mr. ammo
	{
		te = findradius (self.origin, 60);
		while (te)
		{
			if (te.classname == "player" && te.team == self.team)
			{
				if (self.chemcount <= 0)
				{
					sound (self, CHAN_BODY, "misc/item1.wav", TRUE, ATTN_NORM);
					sprint (te, 2, "this mr.ammo is out of ammunition.\n");
					return;
				}

				x = 300;

				if (te.current_slot == 1 && te.ammo2 < (x))
				{
					sound (self, CHAN_BODY, "misc/item1.wav", TRUE, ATTN_NORM);
					zz = (te.maxmag1 / 2);
					zz = ceil (zz);
					qq = ftos (zz);
					sprint (te, 2, qq);
					sprint (te, 2, " ammo was received from the mr.ammo.\n");
					te.ammo1 = (te.ammo1 + zz);
					te.ammo1 = ceil (te.ammo1);
					self.chemcount = self.chemcount - 1;
				}
				else
				{
					if (te.current_slot == 2 && te.ammo2 < (x))
					{
						sound (self, CHAN_BODY, "misc/item1.wav", TRUE, ATTN_NORM);
						zz = (te.maxmag2 / 2);
						zz = ceil (zz);
						qq = ftos (zz);
						sprint (te, 2, qq);
						sprint (te, 2, " ammo was received from the mr.ammo.\n");
						te.ammo2 = (te.ammo2 + zz);
						te.ammo2 = ceil (te.ammo2);
						self.chemcount = self.chemcount - 1;
					}
				}
			}
			te = te.chain;
		}
	}
};

void () Bar_Think =
{
	local float var;
	local float dot1;

	if ((self.owner.health < WEAPON_SHOTGUN))
	{
		remove (self);
		return;
	}
	if ((self.owner.health >= self.owner.max_health))
	{
		remove (self);
		return;
	}
	self.flags = self.flags;

	if ((self.owner.position == WEAPON_SHOTGUN))
	{
		dot1 = WEAPON_SHOTGUN;
	}
	if ((dot1 == WEAPON_SHOTGUN))
	{
		self.frame = MULTICAST_ALL;
		setmodel (self, "");
		self.nextthink = (time + 0.01);
		return;
	}
	if (((self.owner.health >= WEAPON_SHOTGUN) && (dot1 == MULTICAST_ALL)))
	{
		self.frame = floor (((self.owner.health / self.owner.max_health) * TE_WIZSPIKE));
		setorigin (self, (self.owner.origin + '0 0 48'));
		self.nextthink = (time + 0.01);
		setmodel (self, "progs/hbar.spr");
	}
};

void (entity guy) spawn_dot =
{
	local entity hologram;

	hologram = spawn ();
	hologram.movetype = MOVETYPE_NONE;
	hologram.solid = SOLID_NOT;
	hologram.owner = self;
	setmodel (hologram, "progs/hbar.spr");
	hologram.skin = self.skin;
	setorigin (hologram, self.origin);
	setsize (hologram, VEC_ORIGIN, VEC_ORIGIN);
	hologram.angles = self.angles;
	hologram.colormap = self.colormap;
	hologram.cnt = MULTICAST_ALL;
	hologram.think = Bar_Think;
	hologram.nextthink = (time + 0.01);
};


void (vector org, entity guy, float input, float cost) spawn_station =
{
	local entity dog;
	local entity te;

	if (input == 3)
	{
		te = find (world, classname, "robofang");
		while (te)
		{
			if (te.track == self && te.buildtype == 3)
			{
				makevectors (self.v_angle);
				setorigin(te, self.origin + v_forward*32);
				return;
			}
			te = find (te, classname, "robofang");
		}
	}

	if (self.scraps < cost)
	{
		sprint (self, 2, "not enough metal.\n");
		return;
	}

	if (input == 4)
	{
		if (self.handgrenade == 0)
		{
			sprint (self, 2, "need a grenade.\n");
			return;
		}
		if (self.handgrenade == 1)
		{
			self.grenadetype = 0;
			self.handgrenade = 0;
		}

	}

	self = guy;
	te = find (world, classname, "station");
	while (te)
	{
		if (te.track == self && te.buildtype == input)
		{
			sprint (self, 2, "already have one.\n");
			return;
		}
		te = find (te, classname, "station");
	}
	if (input == 3)
	{
			te = findradius (self.origin, 128);
			while (te)
			{
				if (te != self && te.classname == "player" && te.health > 0)
				{
					sprint(self, PRINT_HIGH, "not with other players nearby.\n");
					return;
				}
				te = te.chain;
			}
	}

	makevectors (self.v_angle);
	org = ((org + (v_forward * IT_LIGHTNING)) + (v_up * EF_FLAG2));
	if ((pointcontents ((org + '0 20 0')) != CONTENT_EMPTY))
	{
		sprint (self, 2, "can't build there.\n");
		return;
	}
	if ((pointcontents ((org + '0 -20 0')) != CONTENT_EMPTY))
	{
		sprint (self, 2, "can't build there.\n");
		return;
	}
	if ((pointcontents ((org + '20 0 0')) != CONTENT_EMPTY))
	{
		sprint (self, 2, "can't build there.\n");
		return;
	}
	if ((pointcontents ((org + '-20 0 0')) != CONTENT_EMPTY))
	{
		sprint (self, 2, "can't build there.\n");
		return;
	}
	if ((pointcontents ((org + '0 0 50')) != CONTENT_EMPTY))
	{
		sprint (self, 2, "can't build there.\n");
		return;
	}
	if ((pointcontents ((org + '0 0 -10')) != CONTENT_EMPTY))
	{
		sprint (self, 2, "can't build there.\n");
		return;
	}
	self.impulse = 0;
	te = findradius (org, 40);
	while (te)
	{
		if (te.classname == "spawn1")
		{
			sprint (self, 2, "can't build at spawn.\n");
			return;
		}
		if (te.classname == "spawn2")
		{
			sprint (self, 2, "can't build at spawn.\n");
			return;
		}
		if (te.classname == "ghoul")
		{
			sprint (self, 2, "somethings in the way.\n");
			return;
		}
		if (((te.classname == "player") && (te.health > 0)))
		{
			sprint (self, 2, "can't build on players.\n");
			return;
		}
		if (((te.classname == "station") && (te.health > 0)))
		{
			sprint (self, 2, "can't build on other stations.\n");
			return;
		}
		te = te.chain;
	}
	self.scraps = (self.scraps - cost);
	dog = spawn ();
	dog.team = self.team;
	dog.track = self;
	self = dog;
	spawn_dot (dog);
	self.origin = org;
	self.takedamage = DAMAGE_YES;
	self.solid = SOLID_SLIDEBOX;
	self.movetype = MOVETYPE_TOSS;
	setsize (self, '-16 -16 0', '16 16 40');
	self.health = SVC_INTERMISSION;
	self.max_health = (300 + (input * 50));
	self.th_die = station_die;
	setmodel (self, "progs/station.mdl");
	self.classname = "station";
	self.think = station_think;
	self.helmet = 2;
	self.buildtype = input;
	if (self.buildtype == 0)
		self.netname = "mr. ammo";
	if (self.buildtype == 1)
		self.netname = "shield generator";
	if (self.buildtype == 2)
		self.netname = "autodoc";
	if (self.buildtype == 3)
	{
		setsize(self, '-24 -24 -24', '24 24 24');
		self.netname = "robofang";
		setmodel (self, "progs/dog.mdl");
	}
	if (self.buildtype == 4)
	{
		setsize(self, '-4 -4 -4', '4 4 4');
		self.netname = "grenade";
		setmodel (self, "progs/grenade2.mdl");
	}
	self.frame = 4;
};

void () BuyMenu =
{
	local float is_shop;
	local entity te;
	local entity shop;
	local entity spot;
	local entity oldself;
	local vector spot1;
	local vector spot2;
	local float r;
	local float cyc;
	local string menu;

	if (self.currentmenu == "777")
		return;

	is_shop = 0;

	if (coop == 0)
	{
		te = findradius (self.origin, 250);
		while (te)
		{
			if (te.classname == "buyzone1" && self.team == 1)
				is_shop = 1;
			if (te.classname == "buyzone2" && self.team == 2)
				is_shop = 1;

			te = te.chain;
		}
	}

	if (coop == 1)
	{

		te = findradius (self.origin, 80);
		while (te)
		{
			if (te.classname == "buyzone")
				is_shop = 1;
			if (te.classname == "merchant")
				is_shop = 1;

			te = te.chain;
		}
	}

	if ((is_shop == 0))
	{
		centerprint (self, "nothing but the afterglow, here.\n(find a trader)");
		return;
	}
	if ((is_shop == 1))
	{
		menu = ShopString ();
		centerprint (self, menu);
		self.currentmenu = "shop_list";

		if (self.current_slot != 0 && self.current_slot != 2)
			self.current_slot = 1;

		return;
	}
};



void () CharacterSheet =
{
	local string x;
	local float qq;
	local float ratio;
	local float r1;
	local float r2;
	local float var;

	var = weightx ();
	stuffcmd (self, "toggleconsole\n");
	sprint (self, PRINT_HIGH, "\n\n\n\n ** INFO ** \n");
	sprint (self, PRINT_HIGH, "êClass     ë ");
		if (self.class == 1)
			sprint (self, 2, "Medic");
		if (self.class == 2)
			sprint (self, 2, "Assassin");
		if (self.class == 3)
			sprint (self, 2, "Soldier");
		if (self.class == 4)
			sprint (self, 2, "Scientist");

	sprint (self, PRINT_HIGH, "\nêTeam      ë ");

	if (self.team == 1)
		sprint (self, 2, "Rangers\n");
	if (self.team == 2)
		sprint (self, 2, "Raiders\n");

	sprint (self, PRINT_HIGH, "êScore     ë ");
	r1 = (self.dead);
	r2 = (self.kills);
	if (r1 == 0)
		r1 = 1;

	ratio = (r2 / r1);
	x = ftos (ratio);
	sprint (self, 2, x);
	sprint (self, 2, " (");
	x = ftos (self.kills);
	sprint (self, 2, x);
	sprint (self, PRINT_HIGH, "/");
	x = ftos (self.dead);
	sprint (self, 2, x);
	sprint (self, 2, ")   ");
	sprint (self, PRINT_HIGH, "\nêSpeed     ë ");
	x = ftos (self.maxspeed);
	sprint (self, 2, x);
	sprint (self, PRINT_HIGH, "/300\n");
	sprint (self, PRINT_HIGH, "êMoney     ë ");

	x = ftos (self.ammo_shells);
	sprint (self, 2, x);
	sprint (self, PRINT_HIGH, "$\n");
	if (self.class == 2)
	{
		sprint (self, PRINT_HIGH, "êBandages  ë ");
		x = ftos (self.bandages);
		sprint (self, 2, x);
		sprint (self, PRINT_HIGH, "\n");
	}
	if (self.class == 6)
	{
		sprint (self, PRINT_HIGH, "êScraps    ë ");
		x = ftos (self.scraps);
		sprint (self, 2, x);
		sprint (self, PRINT_HIGH, "\n");
	}

	sprint (self, PRINT_HIGH, "\nêArmor     ë ");
	x = GetArmorName();
	sprint (self, 2, x);
	sprint (self, 2, "  (");
	x = ftos (self.armor_weight);
	sprint (self, 2, x);
	sprint (self, 2, ")\n");

	sprint (self, PRINT_HIGH, "êProtectiveë ");
	x = GetProtectName();
	sprint (self, 2, x);
	sprint (self, 2, "\n");
	sprint (self, PRINT_HIGH, "êChem      ë ");
	x = GetChemName();
	sprint (self, 2, x);
	x = ftos (self.chemcount);
	sprint (self, 2, "[");
	sprint (self, 2, x);
	sprint (self, 2, "]");
	sprint (self, 2, "\n");
	sprint (self, PRINT_HIGH, "êGadget    ë ");

	x = GetEquipmentName();
	sprint (self, 2, x);
	sprint (self, 2, "\n");
	sprint (self, PRINT_HIGH, "êPerk      ë ");
	x = GetPerkName();
	sprint (self, 2, x);
	sprint (self, 2, "\n");
	sprint (self, PRINT_HIGH, "êTrait     ë ");
	x = GetTraitName();
	sprint (self, 2, x);
	sprint (self, 2, "\n");
	sprint (self, PRINT_HIGH, "êWeapon 1  ë ");
	x = GetWeaponName (self, self.slot1);
	sprint (self, 2, x);
	sprint (self, 2, "  (");
	x = ftos (self.slot1_weight);
	sprint (self, 2, x);
	sprint (self, 2, ") (");
	sprint (self, 2, self.ammotype1);
	sprint (self, 2, ")\n");

	sprint (self, PRINT_HIGH, "êWeapon 2  ë ");
	x = GetWeaponName (self, self.slot2);
	sprint (self, 2, x);
	sprint (self, 2, "  (");
	x = ftos (self.slot2_weight);
	sprint (self, 2, x);
	sprint (self, 2, ") (");
	sprint (self, 2, self.ammotype2);
	sprint (self, 2, ")\n\n");
	qq = weightx ();
	sprint (self, PRINT_HIGH, "êWeight    ë ");
	x = ftos (qq);
	sprint (self, 2, x);
	qq = self.max_weight;
	sprint (self, PRINT_HIGH, "/");
	x = ftos (qq);
	sprint (self, 2, x);
	sprint (self, PRINT_HIGH, "\n");
	return;
};

void () UseEquipment =
{

	if (self.equipment == 0)
	{
		centerprint(self, "êë no extra equipment êë\n");
		return;
	}
	if (self.equipment == 1)
	{
		centerprint(self, "êë medic's bag êë\nlets you carry more stimpacks\n");
		return;
	}
	if (self.equipment == 4)
	{
		centerprint(self, "êë belt pouch êë\ngives you room for two extra grenades\n");
		return;
	}
	if (self.equipment == 5)
	{
		centerprint(self, "êë backpack êë\nlets you carry more ammunition\n");
		return;
	}
	if (self.equipment == 6)
	{
		centerprint(self, "êë toolkit mark ii êë\nbuild, defuse and open doors faster\n");
		return;
	}
	if (self.equipment == 7 && self.equipment_state == 0)
	{
		sprint (self, PRINT_HIGH, "climbing gear in place.\n");
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
		self.maxspeed = 100;
		self.velocity_z = 0;
		self.equipment_state = 1;
	}
	if (self.equipment == 7 && self.equipment_state == 1)
	{
		sprint (self, PRINT_HIGH, "climbing gear retrieved.\n");
		sound (self, CHAN_BODY, "misc/item2.wav", 1, ATTN_NORM);
		self.grab = 0;
		self.equipment_state = 0;
		return;
	}

	if (self.equipment == 8)
	{
		centerprint(self, "êë enhanced battery êë\nallows longer cloaking\n");
		return;
	}
	if (self.equipment == 9)
	{
		Sneak();
		return;
	}

};

void (vector s_aim, float dam, float var, float ran) W_FireBuckshotSpread1 =
{
	local vector source;
	local vector targ;
	local vector org;
	local float r;
	local float srange;
	local float zdif;
	local vector src;
	local vector pos;
	local float ydif;
	local float xdif;
	local float true;

	makevectors (self.v_angle);
	source = (self.origin + '0 0 20');
	targ = ((((((s_aim + ((v_right * random ()) * var)) - ((v_right * random ()) * var)) + ((v_up * random ()) * var)) - ((v_up * random ()) * var)) + (((v_up * random ()) * var) * 0.5)) + (v_forward * ran));
	traceline (source, targ, FALSE, self);
	if ((trace_fraction == 1))
	{
		return;
	}
	org = (trace_endpos - (v_forward * 2));
	if (trace_ent.takedamage)
	{
		dam = ((random () * dam) + dam);
		dam = (dam * (1 - trace_fraction));

		if (trace_ent.solid != SOLID_BSP)
			SpawnBlood (org, 1);
		if (trace_ent.solid == SOLID_BSP)
			SpawnWood (trace_ent, org, 1);

		zdif = (org_z - trace_ent.origin_z);
		ydif = (org_y - trace_ent.origin_y);
		xdif = (org_x - trace_ent.origin_x);
		true = 0;
		if (((ydif >= CONTENT_SLIME) && (ydif <= WEAPON_SPIKES)))
		{
			true = 1;
		}
		if (((xdif >= CONTENT_SLIME) && (xdif <= WEAPON_SPIKES)))
		{
			true = 1;
		}
		if (((true == 1) && (zdif >= ((trace_ent.size_z / 2) * 0.8))))
		{
			if (self.attack > 2)
				dam = (dam * 0.4);

			self.critical = 3;
		}
		T_Damage (trace_ent, self, self, dam);
	}
	else
	{
		bullet_hole (org);
	}
};

void (float rec, float number, float dam, float var, float ran, float auto) W_FireShotgun =
{
	local vector dir;
	local vector p_aim;
	local float var1;
	local float var2;
	local float var3;
	local float var4;
	local float weap;
	local vector adjust;

	if (self.current_slot == 1)
		weap = self.slot1;
	else
		weap = self.slot2;

	if (self.velocity != '0 0 0')
	{
		var1 = ((random () * 48) * (6 + self.recoil * 1.5));
		var2 = ((random () * 48) * (6 + self.recoil * 1.5));
		var3 = ((random () * 48) * (6 + self.recoil * 1.5));
		var4 = ((random () * 48) * (6 + self.recoil * 1.5));
	}
	if (self.position == 2)
	{
		var1 = ((random () * 12) * (6 + self.recoil * 1.5));
		var2 = ((random () * 12) * (6 + self.recoil * 1.5));
		var3 = ((random () * 12) * (6 + self.recoil * 1.5));
		var4 = ((random () * 12) * (6 + self.recoil * 1.5));
	}
	if (self.position == 1)
	{
		var1 = ((random () * IDLE3A) * (6 + self.recoil * 1.5));
		var2 = ((random () * IDLE3A) * (6 + self.recoil * 1.5));
		var3 = ((random () * IDLE3A) * (6 + self.recoil * 1.5));
		var4 = ((random () * IDLE3A) * (6 + self.recoil * 1.5));
	}
	if (self.position == 0)
	{
		var1 = ((random () * IDLE10A) * (6 + (self.recoil * 3)));
		var2 = ((random () * IDLE10A) * (6 + (self.recoil * 3)));
		var3 = ((random () * IDLE10A) * (6 + (self.recoil * 3)));
		var4 = ((random () * IDLE10A) * (6 + (self.recoil * 3)));
	}

	if (self.position == 0)
		player_single1 ();
	if (self.position == 2)
		player_single1 ();
	if (self.position == 1)
		player_single1_s ();

	stuffcmd (self, "+lookup\n");
	stuffcmd (self, "wait\n");
	stuffcmd (self, "-lookup\n");

	sound (self, CHAN_WEAPON, "weapons/shotgun1.wav", 1.5, ATTN_NORM);

	
	if (self.position == 0)
		p_aim = (((((self.origin + '0 0 20' + adjust) + (v_right * var1)) - (v_right * var2)) + (v_up * var3)) - (v_up * var4));
	else if (self.position == 1)
		p_aim = (((((self.origin + '0 0 4' + adjust) + (v_right * var1)) - (v_right * var2)) + (v_up * var3)) - (v_up * var4));
	else if (self.position == 2)
		p_aim = (((((self.origin + '0 0 -12' + adjust) + (v_right * var1)) - (v_right * var2)) + (v_up * var3)) - (v_up * var4));

	msg_entity = self;
	WriteByte (MSG_ONE, SVC_BIGKICK);
	DropAmmo ();
	self.attack = (self.attack + 1);
	self.recoil = (self.recoil + 8);
	Crosshair();

	if ((self.recoil >= 30))
	{
		self.recoil = 30;
	}
	if ((auto == 0))
	{
		self.attack_finished = (time + 0.5);
	}
	if ((auto == 1))
	{
		self.attack_finished = (time + 0.2);
	}
	if ((weap == DRAW4))
	{
		self.attack_finished = (time + 0.75);
	}
	if ((weap == IDLE2A))
	{
		self.attack_finished = (time + 0.4);
	}
	dir = aim (self, 10000);
	var = SVC_INTERMISSION;
	if ((number == MULTICAST_PVS_R))
	{
		W_FireBuckshotSpread1 (p_aim, dam, var, ran);
		W_FireBuckshotSpread1 ((p_aim + (v_right * 80)), dam, var, ran);
		W_FireBuckshotSpread1 ((p_aim - (v_right * 80)), dam, var, ran);
		W_FireBuckshotSpread1 ((p_aim + (v_up * 80)), dam, var, ran);
		W_FireBuckshotSpread1 ((p_aim - (v_up * 80)), dam, var, ran);
	}
	else
	{
		if ((number == AS_MELEE))
		{
			var = 80;
			W_FireBuckshotSpread1 (p_aim, dam, var, ran);
			W_FireBuckshotSpread1 (p_aim, dam, var, ran);
			W_FireBuckshotSpread1 (p_aim, dam, var, ran);
		}
	}
};

