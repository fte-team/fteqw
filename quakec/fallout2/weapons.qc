/*
*/
void() show_light_level;
void() UseStimpack;
void() UseSuperStim;
void() UseMedicalBag;
void (entity chest, float iid, float num) DropFromChest;
void() AddClone;
void() SpawnGModel;
void (string snd, float type) function_radio;
void () MapInfo;
void (entity targ, entity inflictor, entity attacker, float damage) T_Damage;
void () player_run;
void () walkmonster_start;
void(entity bomb, entity attacker, float rad, entity ignore, string dtype) T_RadiusDamage;
void(entity bomb, entity attacker, float rad, entity ignore, float xrange) T_RadiusDamage2;
void() ZoomIn;
void (float dam, float xrecoil, string snd, float rng, float rate) FireMinigun;
void(float db) loud_noise;
void() player_auto_duck1;
void() player_auto_ducka1;
void(vector org, float damage) SpawnBlood;
void(vector org, float damage) SpawnNonBlood;
void() SuperDamageSound;
void (float rec, float speed, float dam, float type) FirePulseRifle;
void (float rec, float number, float dam, float var, float ran, float auto, string snd) W_FireShotgun;
void (float dam, float rec, string snd, float rng, float rate) FireAssaultRifle;
void (float dam, float rec, string snd, float rng, float rate) FirePistol;
void (float dam, float rec, string snd, float rng, float rate) FireSMG;
float() FireToolkit;
void () HostageReport;
void (entity jeb, float time) spawn_excla;
void (entity jeb, float time) spawn_stop;
void () FireAlienBlaster;
void () W_PlayerMenu;
void () player_single_shot;
void () player_single_shot_duck;
//void () UseChem;
void () Special;
void () BuyMenu;
void() Sneak;
//void() Bandage;
void() Shield;
void () player_pull1;
void() player_dragon_kick;
void() player_kick1;
void() player_kick1a;
void() player_swing1;
void() player_swing1a;
void() player_punch1;
void() player_punch2;
void() player_knife1;
void() player_knifea;
void() player_spear1;
void() player_spear1a;
void() player_axe1;
void() player_axeb1;
void() ExitScreen;
void() CharacterSheet;
void() UseEquipment;
void (float slot, float snd, float drop) DropFromSlot;
void() PositionControl;
void() autofire;
void() autofire_s;
float(float iid) UseBoostingChem;
float(float iid) UseHealingChem;
float(float slotno) DecreaseDestroySlot;
void () DropAmmo;
void()  player_singlex;
void()  player_singley;
void(string arg1) Cmd_InvUse;

#define weightx() (self.weight)



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



void(float slotno) UsePerk =
{
	local float x, mcount;
	local string y, timeleft;
	local entity te;

	if (slotno == 1 && self.perk1timer > 0)
	{
		timeleft = ftos(self.perk1timer);
		sprint(self, 2, "ability is not ready yet. [");
		sprint(self, 2, timeleft);
		sprint(self, 2, "]\n");
		sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
		return;
	}

	if (slotno == 2 && self.perk2timer > 0)
	{
		timeleft = ftos(self.perk2timer);
		sprint(self, 2, "ability is not ready yet. [");
		sprint(self, 2, timeleft);
		sprint(self, 2, "]\n");
		sound (self, CHAN_BODY, "misc/beep1.wav", 1, ATTN_NORM);
		return;
	}

	if ((self.perk1 == 7 && slotno == 1) || (self.perk2 == 7 && slotno == 2))
	{
		if (slotno == 1)
			self.perk1timer = 2000;
		if (slotno == 2)
			self.perk2timer = 2000;

		sound (self, CHAN_BODY, "misc/r_tele4.wav", 1, ATTN_NORM);

		te = findradius (self.origin, 6000);
		while (te)
		{
			if (te.classname == "monster")
			{
				te.hold = 160;
				spawn_stop(te, 10);
				mcount = mcount + 1;
			}
			te = te.chain;
		}
	}
};


void() muzzleflash =
{
	WriteByte (MSG_MULTICAST, SVC_MUZZLEFLASH);
	WriteEntity (MSG_MULTICAST, self);
	multicast (self.origin, MULTICAST_PVS);
};


float (entity healer, entity saved) RevivePlayer =
{
	local entity oself;
	oself = self;

	self = saved;
/*	if (!walkmove(0, 0))
	{
		//too close?
		return FALSE;
	}
*/
	saved.deadflag = DEAD_NO;
	saved.takedamage = DAMAGE_AIM;
	saved.movetype = MOVETYPE_WALK;
//	saved.solid = SOLID_NOT;

	self = oself;

//	saved.materialize = 200;
	saved.ghost = 0;
	saved.health = 20 + (healer.skill_doctor);
	saved.air_finished = time + 10;
	saved.view_ofs = '0 0 22';
	self = saved;
	player_run();
	self = oself;
	stuffcmd(saved, "impulse 1\n");
	if (healer.classname == "player")
	{
		sprint (healer, PRINT_HIGH, "you revive ");
		sprint (healer, PRINT_HIGH, trace_ent.netname);
		sprint (healer, PRINT_HIGH, ".\n ");
	}
	sprint (saved, PRINT_HIGH, healer.netname);
	sprint (saved, PRINT_HIGH, " saves you from death.\n");
	saved.view2 = world;

	saved.flags (-) FL_FINDABLE_NONSOLID;

	return TRUE;
};

/*
================
W_FireMelee
================
*/
void(float damage, float dist, float rate) FireMelee =
{
	local vector source, org, vec, dir;
	local float weap, dot, tdam;
	local string sdam;

	if (getperk(5))//living anatomy
	{
		if (self.steadyaim == 0)
			damage = damage + self.skill_doctor;
	}

	self.recoil = self.recoil + 5;

	weap = ToIID(self.(SlotField(self.current_slot)));

	makevectors (self.v_angle);
	source = self.origin + '0 0 16';
	traceline (source, source + v_forward*dist, FALSE, self);
	if (trace_fraction == 1.0)
		return;
	
	org = trace_endpos - v_forward*4;

	if (self.sneak > 0 && weap == IID_WP_KNIFE)
	{
			makevectors (trace_ent.angles);
			vec = normalize ((self.origin - trace_ent.origin));
			dot = (vec * v_forward);

			if (dot < 0.5 && trace_ent.takedamage)
			{
				tdam = 20 + (20*self.skill_sneak);

				if (getperk(9))
					tdam = 900;

				if (self.steadyaim == 0)
					tdam = tdam + 10;

			if (trace_ent.solid != SOLID_BSP && trace_ent.classname != "car")
				SpawnBlood (org, 1);
			else
				SpawnNonBlood (org, 1);

				T_Damage (trace_ent, self, self, tdam);
				self.attack_finished = time + 2;

				if (trace_ent.health > 0)//victim survived the attack
				{
						bprint(2, self.netname);
						bprint(2, " ambushes ");
						bprint(2, trace_ent.netname);
						bprint(2, "!\n");

						if (random()*4<2)
							sound (trace_ent, CHAN_WEAPON, "player/headshot.wav", 1, ATTN_NONE);
						else
							sound (trace_ent, CHAN_WEAPON, "player/udeath.wav", 1, ATTN_NONE);

						loud_noise(15);
						self.equipment_slot = 0;
						sprint(self, PRINT_HIGH, "ç Uncloaked ç\n");
						setmodel (self, "progs/guy.mdl");
						self.sneak = 0;
						spawn_excla(self, 3);
				}
				else
				{
						bprint(2, self.netname);
						bprint(2, " assassinates ");
						bprint(2, trace_ent.netname);
						bprint(2, "!\n");
						self.score = self.score + 5;

						if (random()*4<2)
							sound (trace_ent, CHAN_WEAPON, "player/headshot.wav", 0.25, ATTN_IDLE);
						else
							sound (trace_ent, CHAN_WEAPON, "player/udeath.wav", 0.25, ATTN_IDLE);
				}
				return;
			}
			else if (trace_ent.takedamage)
			{
				loud_noise(15);
				tdam = (30*(random()*30));
				sdam = ftos(tdam);
				if (trace_ent.solid != SOLID_BSP && trace_ent.classname != "car" && (trace_ent.islot3 == 0 || self.critical == 3))
					SpawnBlood (org, 1);
				else 
					SpawnNonBlood (org, 1);				T_Damage (trace_ent, self, self, tdam);
				self.equipment_slot = 0;
				sprint(self, PRINT_HIGH, "ç Uncloaked ç\n");
				setmodel (self, "progs/guy.mdl");
				self.sneak = 0;
				spawn_excla(self, 3);
				return;
			}
	}
	else if (trace_ent.takedamage)
	{
		//soldiers and assassins do more melee damage
		if (self.class == 2 || self.class == 3)
			damage = damage * 1.25;

		if (weap == IID_WP_SLEDGE || weap == IID_WP_SPEAR || weap == IID_WP_KNIFE)
			SpawnBlood (org, 1);


		trace_ent.axhitme = 1;
		if (trace_ent.classname != "car")
		{
			if (weap == IID_NONE)
			{
				if (random()<0.5)
					sound (self, CHAN_BODY, "weapons/brawl-1.wav", 1, ATTN_NORM);
				else
					sound (self, CHAN_BODY, "weapons/brawl-2.wav", 1, ATTN_NORM);
			}
			else
				sound (trace_ent, CHAN_VOICE, "player/headshot.wav", 0.5, ATTN_IDLE);
		}
		T_Damage (trace_ent, self, self, damage+random()*damage);
	}

	else if (weap == IID_NONE)
	{
		if (random()<0.5)
			sound (self, CHAN_BODY, "weapons/brawl-1.wav", 1, ATTN_NORM);
		else
			sound (self, CHAN_BODY, "weapons/brawl-2.wav", 1, ATTN_NORM);
	}
	else if (weap == IID_WP_SLEDGE || weap == IID_WP_WRENCH)
	{
		if (random()<0.5)
			sound (self, CHAN_BODY, "effects/axe_hit1.wav", 1, ATTN_NORM);
		else
			sound (self, CHAN_BODY, "effects/axe_hit2.wav", 1, ATTN_NORM);
	}
	else if (weap == IID_WP_SPEAR)
	{
		if (random()<0.5)
			sound (self, CHAN_BODY, "effects/spear_hit1.wav", 1, ATTN_NORM);
		else
			sound (self, CHAN_BODY, "effects/spear_hit2.wav", 1, ATTN_NORM);
	}
	else
		sound (self, CHAN_WEAPON, "weapons/knifehit.wav", 1, ATTN_NORM);
};

void() W_FireMelee =
{
	local float iid;
	iid = ToIID(self.(SlotField(self.current_slot)));
	if (iid == IID_NONE)
		FireMelee(5+random()*10, 64, 0.6);
	else if (iid == IID_WP_WRENCH)
		FireMelee(5+random()*30, 64, 1.2);
	else if (iid == IID_WP_KNIFE)
		FireMelee(5+random()*20, 64, 0.4);
	else if (iid == IID_WP_SLEDGE)
		FireMelee(5+random()*80, 96, 1);
	else if (iid == IID_WP_SPEAR)	
		FireMelee(5+random()*60, 160, 0.8);

	else if (iid == IID_CHEM_STIMPACK ||
		   iid == IID_CHEM_MEDICALBAG ||
		   iid == IID_CHEM_SUPERSTIM ||
               iid == IID_MISC_NUKACOLA)
	{
		if (UseHealingChem(iid))
			DecreaseDestroySlot(self.current_slot);
	}
	else if (iid == IID_CHEM_ADRENALINE ||
		   iid == IID_CHEM_PSYCHO ||
               iid == IID_CHEM_RADX ||
		   iid == IID_CHEM_BESERK)
	{
		if (UseBoostingChem(iid))
			DecreaseDestroySlot(self.current_slot);
	}
	else
		dprint(strcat("W_FireMelee - ", ftos(iid), "not implemented\n"));
};

void () player_single1 = [ 89, player_single2 ]
{
	self.weaponframe = 1;
	muzzleflash ();
};

void () player_single2 = [ 90, player_run ]
{
	self.weaponframe = 2;
};

void () player_single1_s = [ 182, player_single2_s ]
{
	self.weaponframe = 1;
	muzzleflash ();
};

void () player_single2_s = [ 183, player_run ]
{
	self.weaponframe = 2;
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

void(vector org, float damage) SpawnNonBlood =
{
	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_KNIGHTSPIKE);
	WriteCoord (MSG_MULTICAST, org_x);
	WriteCoord (MSG_MULTICAST, org_y);
	WriteCoord (MSG_MULTICAST, org_z);
	multicast (org, MULTICAST_PVS);
};

void(vector org, float damage) MassBlood =
{
	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_GUNSHOT);
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

	if (pointcontents(self.origin) == CONTENT_SKY)
	{
		remove(self);
		return;
	}


	T_RadiusDamage2 (self, self.owner, 100+random()*100, world, 600);

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
W_FirePlasma
================
*/
void () PlasmaBolt =
{
	local float dam;
	local float zdif;
	local float ydif;
	local float xdif;
	local float tru;

	if (other.solid == SOLID_TRIGGER)
		return;	//they're not really solid

	if (pointcontents (self.origin) == CONTENT_SKY)
	{
		remove (self);
		return;
	}

	if (other.takedamage)
	{
		dam = 30 + (random () * 30);

		T_Damage (other, self, self.owner, dam);
		WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
		WriteByte (MSG_MULTICAST, TE_LIGHTNINGBLOOD);
		WriteCoord (MSG_MULTICAST, self.origin_x);
		WriteCoord (MSG_MULTICAST, self.origin_y);
		WriteCoord (MSG_MULTICAST, self.origin_z);
		multicast (self.origin, MULTICAST_PHS);
		sound (self, CHAN_WEAPON, "enforcer/enfstop.wav", 1, ATTN_NORM);
	}
	else
	{
		WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
		WriteByte (MSG_MULTICAST, TE_LIGHTNINGBLOOD);
		WriteCoord (MSG_MULTICAST, self.origin_x);
		WriteCoord (MSG_MULTICAST, self.origin_y);
		WriteCoord (MSG_MULTICAST, self.origin_z);
		multicast (self.origin, MULTICAST_PHS);
		sound (self, CHAN_WEAPON, "enforcer/enfstop.wav", 1, ATTN_NORM);
	}
	remove (self);
};

void() FirePlasmaBolt =
{
	local float tmp;
	local vector adjust;

	if (self.attack >= 3)
		return;

	DropAmmo ();
	self.attack_finished = time + 0.1;
	
	sound (self, CHAN_WEAPON, "weapons/energy2.wav", 1, ATTN_NORM);

	msg_entity = self;
	WriteByte (MSG_ONE, SVC_SMALLKICK);

	self.recoil = self.recoil + 4;
	self.attack = self.attack + 1;

	if (random()*3<=2)
		self.driftx = self.driftx + 72;
	else
		self.driftx = self.driftx - 72;

	if (random()*2<=1)
		self.drifty = self.drifty + 72;
	else
		self.drifty = self.drifty - 72;

	newmis = spawn ();
	newmis.owner = self;
	newmis.movetype = MOVETYPE_FLYMISSILE;
	newmis.solid = SOLID_BBOX;
		
	makevectors (self.v_angle);
	newmis.velocity = aim(self, 2000);
	newmis.velocity = newmis.velocity * 2000;
	newmis.angles = vectoangles(newmis.velocity);
	
	newmis.touch = PlasmaBolt;
	newmis.voided = 0;
	
	newmis.nextthink = time + 5;
	newmis.think = SUB_Remove;
	newmis.classname = "rocket";

	if (self.position == 0)
		player_single1();
	if (self.position == 1)
		player_single1_s();
	if (self.position == 2)
		player_single1_s();


	if (self.position == 0)
		adjust = '0 0 0';
	if (self.position == 1)
		adjust = '0 0 -16';
	if (self.position == 2)
		adjust = '0 0 -32';

	newmis.effects = EF_BLUE;
	setmodel (newmis, "progs/plasma.mdl");
	setsize (newmis, '0 0 0', '0 0 0');             
	setorigin (newmis, self.origin + v_forward*8 + '0 0 16' + adjust);

	tmp = 60;
	tmp = tmp + self.velocity_y + self.velocity_x;

	newmis.velocity = newmis.velocity + (v_right * crandom () * tmp) + (v_up * crandom () * tmp);

};

/*
================
W_FireRocket
================
*/
void() W_FireRocket =
{
	DropAmmo ();
	self.attack_finished = time + 0.7;
	
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
	if (trace_ent.classname == "player")
		trace_ent.hold = 120;
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
/*
		if (self.classname == "player")
		{
			if (other.classname == "player")
				trace_ent.velocity_z = trace_ent.velocity_z + 400;
		}
*/
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



//.float hit_z;
void() spike_touch =
{
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
		{
			T_Damage (other, self, self.owner, 9+random()*18);
			WriteByte (MSG_MULTICAST, TE_WIZSPIKE);
		}
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

void() SetWeaponModel =
{
	self.weaponmodel = GetItemVModel(ToIID(ItemInSlot(self, self.current_slot)));
};

void() W_SetCurrentAmmo =
{
	local string x;

	player_run ();          // get out of any weapon firing states

	self.items = self.items - ( self.items & (IT_SHELLS | IT_NAILS | IT_ROCKETS | IT_CELLS) );
	

/*SPIKEREMOVED
	if (self.current_slot == 1)
		self.currentammo = ToStatus(self.islot1);
	else if (self.current_slot == 2)
		self.currentammo = ToStatus(self.islot2);
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
*/
//	else
		sound (self, CHAN_WEAPON, "misc/menu3.wav", 1, ATTN_NORM);

		if (self.current_slot == 1)
			x = GetItemName(ToIID(self.islot1));
		if (self.current_slot == 2)
			x = GetItemName(ToIID(self.islot2));
/*SPIKEREMOVED
		if (self.current_slot == 3)
			x = "grenade";
		if (self.current_slot == 4)
			x = "tools";
*/

		sprint(self, 2, x);
		sprint (self, PRINT_HIGH, " selected.\n");
		sound (self, CHAN_WEAPON, "misc/weapon.wav", 1, ATTN_NORM);
		SetWeaponModel();

	self.currentammo = ToStatus(ItemInSlot(self, self.current_slot));
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

void() player_reload1;
void() player_creload1;

void (float slotno) ReloadWeapon =
{
	local float at;
	local float as;
	local float asv;
	local float ac;
	local float x;
	local float wid;
	local float r;

	local .float wslot;

	if (self.rtime > time || self.attack_finished > time)
		return;

	if (self.zoom > 0)
		ZoomIn();

	wslot = SlotField(slotno);

	if (self.wslot == 0)
		return;	//already empty..

	wid = ToIID(self.wslot);
	at = WeaponAmmoType(wid);
	as = SlotOfItem(self, at);
	if (as == 0)
	{
		sprint(self, 2, "no suitable ammo\n");
		return;
	}
	asv = ItemInSlot(self, as);
	ac = ToStatus(asv);

	x = WeaponMagQuant(wid);	//get the max ammo capacity

	if (ToStatus(self.wslot) == x) //weapon full already
		return;

	x = x - ToStatus(self.wslot);	//take away the ammo currently in the weapon
	if (x > ac)	//make sure there's enough ammo
		x = ac;
	ac -= x;

	if (ac)
		asv = SlotVal(at, ac);
	else
		asv = 0;	//totally used up.

	SetItemSlot(self, as, asv);

	self.wslot = SlotVal(wid, x + ToStatus(self.wslot));

	r = 3;

	if (wid == IID_WP_ROCKETLAUNCHER || wid == IID_WP_WINCHESTER || wid == IID_WP_MOSSBERG)
	{
		sound (self, CHAN_VOICE, "weapons/shell.wav", TRUE, ATTN_NORM);
		r = 1;
		if (self.current_slot == slotno)
			self.currentammo = ToStatus(ItemInSlot(self, slotno));
		self.attack_finished = time + r;
		self.rtime = time + r;

		return;
	}
	else if (wid == IID_WP_PIPERIFLE_R || wid == IID_WP_PIPERIFLE_S_R || wid == IID_WP_PIPERIFLE_SCOPE_R || wid == IID_WP_PIPERIFLE_SCOPE_S_R)
	{
		sound (self, CHAN_VOICE, "weapons/shell.wav", TRUE, ATTN_NORM);
		r = 0.5;
		if (self.current_slot == slotno)
			self.currentammo = ToStatus(ItemInSlot(self, slotno));
		self.attack_finished = time + r;
		self.rtime = time + r;

		return;
	}
	else if (wid >= IID_WP_PIPERIFLE && wid <= IID_WP_PIPERIFLE_S_R)
	{
		sound (self, CHAN_VOICE, "weapons/shell.wav", TRUE, ATTN_NORM);
		r = 1;
		if (self.current_slot == slotno)
			self.currentammo = ToStatus(ItemInSlot(self, slotno));
		self.attack_finished = time + r;
		self.rtime = time + r;

		return;
	}
	else
		sound (self, CHAN_WEAPON, "weapons/reload.wav", TRUE, ATTN_NORM);

	sprint(self, 2, "reloading...\n");

	if (self.position == 0)
		player_reload1();
	else if (self.position == 1)
		player_creload1();



		self.attack_finished = time + r;
		self.rtime = time + r;

	if (self.current_slot == slotno)
		self.currentammo = ToStatus(ItemInSlot(self, slotno));

	player_run();
};


float() W_CheckNoAmmo =
{
	local float at;
	local .float slotfield;
	slotfield = SlotField(self.current_slot);

	if (ToIID(self.slotfield) == IID_NONE)
		return FALSE;

	if (ToStatus(self.slotfield) < 1)
	{
		at = WeaponAmmoType(ToIID(self.slotfield));
		if (at == 0)
			return FALSE;

		at = SlotOfItem(self, at);
		if (at == 0)
		{
			self.attack_finished = (time + 0.2);
			stuffcmd (self, "-attack\n");
			sound (self, CHAN_WEAPON, "weapons/click.wav", TRUE, ATTN_NORM);
			return TRUE;
		}
		else
		{
			stuffcmd (self, "-attack\n");
			ReloadWeapon (self.current_slot);
			return TRUE;
		}
	}

	return FALSE;
};

void() ToggleFlare =
{
		if (self.flare == 0)
		{
			sound (self, CHAN_BODY, "weapons/flare.wav", 1, ATTN_NORM);
			self.flare = 1;
			self.effects = EF_BLUE;
		}
		else if (self.flare == 1)
		{
			sound (self, CHAN_BODY, "weapons/gpull.wav", 1, ATTN_NORM);
			self.flare = 0;
			self.effects = self.effects - EF_BLUE;
		}
};

/*
============
W_Attack

An attack impulse can be triggered now
============
*/

void() W_Attack =
{
	local float weap;

	if (self.hold > 0)
		return;

	if (self.popup > 0)
	{
		self.popup = 0;
		self.attack_finished = time + 5;
		stuffcmd(self, "-attack\n");
		return;
	}

	makevectors     (self.v_angle);                 // calculate forward angle for velocity
	self.show_hostile = time + 1;   // wake monsters up

	if (self.rtime > time)
		return;
	if (self.class == 0)
		return;
	if (self.team == 0)
		return;
	if (self.attack_finished > time)
		return;

	if (W_CheckNoAmmo())
		return;

	weap = ToIID(self.(SlotField(self.current_slot)));

	self.safezone = 0;

	if (weap == IID_NONE)
	{
		if (getperk(15))
		{
			if ((self.velocity_z > 0) && self.recoil == 0)
				player_dragon_kick ();
			else
			{
				if (random()<0.25)
					player_swing1 ();
				else if (random()<0.25)
					player_swing1a ();
				else if (random()<0.25)
					player_kick1 ();
				else if (random()<0.25)
					player_kick1a ();
			}

			sound (self, CHAN_WEAPON, "effects/axe_swing.wav", TRUE, ATTN_IDLE);
		}
		else
		{
			self.attack_finished = time + 0.50;
			if (random()<0.5)
				player_punch1 ();
			else
				player_punch2 ();

			sound (self, CHAN_WEAPON, "weapons/ax1.wav", TRUE, ATTN_IDLE);
		}
	}
	else if (weap == IID_WP_KNIFE)
	{
		self.attack_finished = time + 0.50;
		if (random()<0.5)
			player_knife1 ();
		else
			player_knifea ();

		sound (self, CHAN_WEAPON, "weapons/ax1.wav", TRUE, ATTN_NORM);

	}
	else if (weap == IID_WP_SPEAR)
	{
		self.attack_finished = time + 0.80;

		if (random()<0.5)
			player_spear1 ();
		else
			player_spear1a ();

		sound (self, CHAN_WEAPON, "effects/miss.wav", TRUE, ATTN_NORM);

	}
	else if (weap == IID_WP_SLEDGE || weap == IID_WP_WRENCH)
	{
		if (random()<0.75)
		{
			player_axeb1 ();
			self.attack_finished = time + 1;
		}
		else
		{
			player_axe1 ();
			self.attack_finished = time + 1.25;
		}
	}
	else if (weap == IID_WP_USP)
		FirePistol(14, 6, "weapons/usp.wav", 2000, 0.1);
	else if (weap == IID_WP_USP_S)
		FirePistol(12, 6, "weapons/smg_s.wav", 1800, 0.1);
	else if (weap == IID_WP_DEAGLE)
		FirePistol(18, 9, "weapons/deagle.wav", 3000, 0.1);
	else if (weap == IID_WP_DEAGLE_M)
		FirePistol(18, 9, "weapons/deagle.wav", 3000, 0.1);
	else if (weap == IID_WP_NEEDLER)
		FirePistol(10+self.skill_science, 2, "weapons/needler.wav", 2000, 0.1);
	else if (weap == IID_WP_GLOCK)
		FirePistol(12, 6, "weapons/glock.wav", 2000, 0.1);
	else if (weap == IID_WP_GLOCK_S)
		FirePistol(10, 6, "weapons/smg_s.wav", 2000, 0.1);
	else if (weap == IID_WP_ALIENBLASTER)
		FirePulseRifle(16, 0.1, 10+self.skill_science*4, 2);
	else if (weap == IID_WP_PIPERIFLE)
		FireAssaultRifle(15+self.skill_science, 5, "weapons/rangem.wav", 5000, 0.1);
	else if (weap == IID_WP_PIPERIFLE_S)
		FireAssaultRifle(15+self.skill_science, 5, "weapons/needler.wav", 5000, 0.1);
	else if (weap == IID_WP_PIPERIFLE_S_R)
		FireAssaultRifle(15+self.skill_science, 5, "weapons/needler.wav", 5000, 0.1);
	else if (weap == IID_WP_PIPERIFLE_SCOPE_S_R)
		FireAssaultRifle(15+self.skill_science, 5, "weapons/needler.wav", 5000, 0.1);
	else if (weap == IID_WP_PIPERIFLE_SCOPE_S)
		FireAssaultRifle(15+self.skill_science, 5, "weapons/needler.wav", 5000, 0.1);
	else if (weap == IID_WP_PIPERIFLE_R)
		FireAssaultRifle(15+self.skill_science, 5, "weapons/rangem.wav", 5000, 0.1);
	else if (weap == IID_WP_PIPERIFLE_SCOPE)
		FireAssaultRifle(15+self.skill_science, 5, "weapons/rangem.wav", 5000, 0.1);
	else if (weap == IID_WP_PIPERIFLE_SCOPE_R)
		FireAssaultRifle(15+self.skill_science, 5, "weapons/rangem.wav", 5000, 0.1);
	else if (weap == IID_WP_DKS1_S)
		FireAssaultRifle(35, 3, "weapons/moonlight1.wav", 5000, 0.25);
	else if (weap == IID_WP_WINCHESTER)
		W_FireShotgun (2, 5, 7, 50, 1500, 0, "weapons/shotgun1.wav");
	else if (weap == IID_WP_MOSSBERG)
		W_FireShotgun (2, 5, 6, 20, 2000, 1, "weapons/citykiller.wav");
	else if (weap == IID_WP_JACKHAMMER)
		W_FireShotgun (1, 5, 6, 30, 1750, 2, "weapons/citykiller.wav");
	else if (weap == IID_WP_GREASEGUN)
		FireSMG(14, 0, "weapons/mp5.wav", 1500, 0.10);
	else if (weap == IID_WP_MP9)
		FireSMG(12, 0, "ogre/ak74.wav", 1500, 0.08);
	else if (weap == IID_WP_GREASEGUN_S)
		FireSMG(12, 0, "weapons/smg_s.wav", 1500, 0.10);
	else if (weap == IID_WP_MP9_S)
		FireSMG(10, 0, "weapons/smg_s.wav", 1500, 0.08);
	else if (weap == IID_WP_RANGEMASTER)
		FireAssaultRifle(18, 4, "weapons/rangem.wav", 4000, 0.1);
	else if (weap == IID_WP_AK112)
		FireAssaultRifle(16, 10, "weapons/ak112.wav", 4000, 0.095);
	else if (weap == IID_WP_AK112_M)
		FireAssaultRifle(17, 10, "weapons/lsw.wav", 5000, 0.10);
	else if (weap == IID_WP_ACR)
		FireAssaultRifle(35, 7, "weapons/bozar.wav", 9000, 0.1);
	else if (weap == IID_WP_AK74)
		FireAssaultRifle(45, 12, "ogre/ak47.wav", 3500, 0.090);
	else if (weap == IID_WP_DKS1)
		FireAssaultRifle(35, 3, "weapons/dks-1.wav", 8000, 0.2);
	else if (weap == IID_WP_MOONLIGHT && self.zoom == 0)
		FireAssaultRifle(16, 10, "weapons/m4-nw.wav", 4000, 0.09);
	else if (weap == IID_WP_MOONLIGHT && self.zoom == 1)
		FireAssaultRifle(16, 5, "weapons/m4-nw.wav", 4000, 0.20);
	else if (weap == IID_WP_G11 && self.zoom == 0)
		FireAssaultRifle(14, 7, "weapons/g11.wav", 4000, 0.11);
	else if (weap == IID_WP_G11 && self.zoom == 1)
		FireAssaultRifle(14, 7, "weapons/g11.wav", 4000, 0.02);
	else if (weap == IID_WP_FNFAL)
		FireAssaultRifle(30, 6, "weapons/fnfal.wav", 7000, 0.11);
	else if (weap == IID_WP_PULSERIFLE)
		FirePulseRifle(12, 0.8, 36, 2);
	else if (weap == IID_WP_TURBOPLASMA)
		FirePulseRifle(8, 0.4, 30, 2);
	else if (weap == IID_WP_PLASMACARBINE)
		FirePlasmaBolt();
	else if (weap == IID_WP_GAUSERIFLE)
		FireAssaultRifle(30, 80, "weapons/gauss.wav", 9000, 0.2);
	else if (weap == IID_WP_LASERGATLING)
		FireMinigun(16, 20, "weapons/ak112.wav", 4000, 0.05);

		//FirePulseRifle(2, 0.1, 30, 3);


//float IID_WP_GAUSERIFLE = 421;
//float IID_WP_PULSERIFLE = 422;

	else if (weap == IID_GREN_FRAG)
		player_pull1();
	else if (weap == IID_GREN_EMP)
		player_pull1();
	else if (weap == IID_GREN_SMOKE)
		player_pull1();
	else if (weap == IID_GREN_FLASH)
		player_pull1();

	else if (weap == IID_WP_ROCKETLAUNCHER)
		W_FireRocket();

	else if (weap == IID_WP_WRENCH)
	{
		if (!FireToolkit())
		{
			self.attack_finished = time + 1;
			if (random()<0.5)
				player_knife1 ();
			else
				player_knifea ();
		}
	}
	else if (IsChem(weap))
	{
		self.attack_finished = time + 0.25;
		player_knife1 ();
	}
	else
		centerprint(self, "Not implemented (", ftos(weap), ")");
};

/*
============
W_ChangeWeapon

============
*/
void() W_ChangeWeapon =
{
	local   float   it, am, fl;
	
	it = self.items;
	am = 0;


	if (self.impulse == 1 && self.current_slot != 1)
	{
		if (!getperk(10))
		{
			self.weaponframe = 4;
			self.attack_finished = time + 1;
			self.rtime = time + 1;
		}
		fl = IT_NAILGUN;
		self.current_slot = 1;	
		if (IsMelee(ToIID(self.islot1)))
			SpawnGModel();
	}
	else if (self.impulse == 2 && self.current_slot != 2)
	{
		if (!getperk(10))
		{
			self.weaponframe = 4;
			self.attack_finished = time + 1;
			self.rtime = time + 1;
		}
		fl = IT_SUPER_NAILGUN;
		self.current_slot = 2;
		if (IsMelee(ToIID(self.islot2)))
			SpawnGModel();
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
	local float x;
	local string y;
	local entity te;

	self.ammo_shells = 900;

	te = findradius (self.origin, 6000);
	while (te)
	{
		if (te.classname == "monster")
			te.hold = 100;

		te = te.chain;
	}
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
	setsize(self, '-16 -16 -24', '16 16 32');

	//if (!walkmove(0, 0) && self.waterlevel == 0)
	if (pointcontents (self.origin + '0 0 32') == CONTENT_SOLID) //not enough room to stand up
	{
		sprint (self, 2, "not enough room to stand up.\n");
		setsize(self, '-16 -16 -24', '16 16 0');
		return;
	}

	sprint (self, 2, "position: stand.\n");
	if (getperk(10))
	{
		self.attack_finished = time + 0.5;
		self.rtime = time + 0.5;
	}
	else
	{
		self.attack_finished = time + 1.5;
		self.rtime = time + 1.5;
	}
	self.position = 0;
	PositionControl();
	player_run ();
};

void () ProneOn =
{
	if (self.waterlevel > 0)
	{
		sprint (self, 2, "position: stand.\n");
		self.position = 0;
		PositionControl();
		player_run ();
		return;
	}

	if (!getperk(10) && self.velocity_z != 0)
		return;

	if (self.position == 2)
	{
		ProneOff();
		return;
	}

	self.walk = 0;
	self.speedmulti = 1.00;
	self.maxspeed = (self.maxspeed * 0.25);
	self.position = 2;
	self.view_ofs = '0 0 -10';
	sprint (self, 2, "position: prone.\n");
	if (!getperk(10))
	{
		self.attack_finished = time + 1.5;
		self.rtime = time + 1.5;
	}
	PositionControl();
};


void () DuckOff =
{
	setsize(self, '-16 -16 -24', '16 16 32');

	//if (!walkmove(0, 0) && self.waterlevel == 0)
	if (pointcontents (self.origin + '0 0 16') == CONTENT_SOLID) //not enough room to stand up
	{
		sprint (self, 2, "not enough room to stand up.\n");
		setsize(self, '-16 -16 -24', '16 16 16');
		return;
	}
	sprint (self, 2, "position: stand.\n");
	if (!getperk(10))
	{
		self.attack_finished = time + 0.2;
		self.rtime = time + 0.2;
	}
	self.view_ofs = '0 0 22';
	self.position = 0;
	PositionControl();
	player_run ();
};


void () DuckOn =
{
	if (self.waterlevel > 0)
	{
		if (!getperk(10))
		{
			self.attack_finished = time + 0.5;
			self.rtime = time + 0.5;
		}
		sprint (self, 2, "position: stand.\n");
		self.view_ofs = '0 0 22';
		self.position = 0;
		PositionControl();
		player_run ();
		self.position = 0;
		return;
	}

	if (self.velocity_z != 0)
		return;
	
	if (self.position == 1)
	{
		DuckOff();
		return;
	}

	if (!getperk(10))
	{
		self.attack_finished = time + 0.5;
		self.rtime = time + 0.5;
	}
	self.walk = 0;
	self.speedmulti = 1.00;
	self.maxspeed = (self.maxspeed * 0.50);
	self.position = 1;
	sprint (self, 2, "position: duck.\n");
	PositionControl();
};

void () WalkOff =
{
	sprint (self, 2, "position: run.\n");
	self.walk = 0;
	self.speedmulti = 1.00;
};


void () WalkOn =
{
	sprint (self, 2, "position: walk.\n");
	self.walk = 1;
	self.speedmulti = 0.50;
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
	if (self.hold > 0)
		return;

	if (self.impulse >= 1 && self.impulse <= 2 && self.currentmenu == "none")
		W_ChangeWeapon ();

	if (self.impulse == 3 && self.currentmenu == "none")
		Cmd_InvUse("5");
	if (self.impulse == 4 && self.currentmenu == "none")
		Cmd_InvUse("6");


	if (self.impulse >= 1 && self.impulse <= 10 && self.currentmenu != "none")
	{
		W_PlayerMenu ();
		self.impulse = 0;
		return;
	}


	if (self.impulse == 11)
		ServerflagsCommand ();

	if (self.impulse == 200)
		DuckOn ();
	if (self.impulse == 201)
		ProneOn ();

	if (self.impulse == 240)
		AddClone ();

	if (self.impulse == 50)
		ReloadWeapon (self.current_slot);
//	if (self.impulse == 51)
//		UseChem ();
	if (self.impulse == 52)
		BuyMenu ();
	if (self.impulse == 53)
		Special ();
	if (self.impulse == 100)
		ExitScreen ();
	if (self.impulse == 55)
		DropFromSlot (self.current_slot, 1, 0);
	if (self.impulse == 56)
		CharacterSheet ();
	if (self.impulse == 57)
		UseEquipment ();
	if (self.impulse == 60)
		ZoomIn ();
	if (self.impulse == 61)
		UsePerk (1);
	if (self.impulse == 62)
		UsePerk (2);

	if (self.impulse == 65)
		HostageReport ();
	if (self.impulse == 66)
		WalkOn ();
	if (self.impulse == 67)
		WalkOff ();

	if (self.impulse == 212)
		function_radio ("radio/1_yes.wav", 1);
	if (self.impulse == 213)
		function_radio ("radio/2_no.wav", 2);
	if (self.impulse == 214)
		function_radio ("radio/3_letsgo.wav", 3);
	if (self.impulse == 215)
		function_radio ("radio/4_cover.wav", 4);
	if (self.impulse == 216)
		function_radio ("radio/5_recon.wav", 5);
	if (self.impulse == 217)
		function_radio ("radio/6_backup.wav", 6);
	if (self.impulse == 218)
		function_radio ("radio/7_hold.wav", 7);
	if (self.impulse == 219)
		function_radio ("radio/8_behind.wav", 8);
	if (self.impulse == 220)
		function_radio ("radio/9_secure.wav", 9);
	if (self.impulse == 221)
		function_radio ("radio/10_suppressing.wav", 10);
	if (self.impulse == 222)
		function_radio ("radio/11_quiet.wav", 11);
	if (self.impulse == 223)
		CheatCommand();
	if (self.impulse == 224)
		show_light_level();
	if (self.impulse == 225)
		UseStimpack();
	if (self.impulse == 226)
		UseSuperStim();
	if (self.impulse == 227)
		UseMedicalBag();

	if (self.impulse == 255)
		self.ammo_shells = 200;

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
	local float weap;

	if (time < self.attack_finished)
		return;

	ImpulseCommands ();
	
// check for attack
	if (self.button0)
	{
		SuperDamageSound ();
		W_Attack ();
	}
	if (!self.button0)
	{

		if (self.attack > 0)
			player_run ();

		self.attack = 0;
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
	local .float wslot;

	wslot = SlotField(self.current_slot);

	self.currentammo = (ToStatus(self.wslot) )- 1;
	self.wslot = SlotVal(ToIID(self.wslot), self.currentammo);
	self.currentammo = ToStatus(self.wslot);
};


void() autofire =
{
	if (self.attack == 0)
		self.cnt = 0;

	if (self.weaponframe == 1)
		self.weaponframe = 2;
	else
		self.weaponframe = 1;

	if (self.cnt == 0)
		player_singlex();
	else if (self.cnt == 1)
		player_singley();

	self.cnt = self.cnt + 1;

	if (self.cnt == 2)
		self.cnt = 0;
};

/*void () autofire =
{
	if (self.frame == 88)
		self.frame = 89;
	else
		self.frame = 88;


	if (self.weaponframe == 1)
		self.weaponframe = 2;
	else
		self.weaponframe = 1;

	muzzleflash ();
};*/

void() autofire_s =
{
	if (self.attack == 0)
		self.cnt = 0;

	if (self.weaponframe == 1)
		self.weaponframe = 2;
	else
		self.weaponframe = 1;

	if (self.cnt == 0)
		player_auto_duck1();
	else if (self.cnt == 1)
		player_auto_ducka1();

	self.cnt = self.cnt + 1;

	if (self.cnt == 2)
		self.cnt = 0;
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
	local vector start;
	local vector end;
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

	te = findradius (self.origin, 600);
	while (te)
	{
		if ((((te.classname == "camera") || (te.classname == "alarm")) && (te.owner.pcamera == 0)))
		{
			te.owner.pcamera = 0;
			te.owner.pcamera2 = 0;
			te.owner.equipment_slot = 0;
			sprint (self.owner, 2, te.owner.netname);
			sprint (self.owner, 2, "'s ");
			sprint (self.owner, 2, te.classname);
			sprint (self.owner, 2, " was wiped out!\n");
			remove (te);
		}
		if (te.has_radio == 1)
		{
			te.has_radio = 0;
			sprint (self.owner, 2, te.netname);
			sprint (self.owner, 2, "'s radio was wiped out!\n");
		}

		te = te.chain;
	}
	T_RadiusDamage (self, self.owner, 30+random()*30, other, "");
	remove (self);
};

void (vector org) CreateSmoke =
{
	newmis = spawn ();
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
	self.cnt = (self.cnt + 1);

	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_GUNSHOT);
	WriteByte (MSG_MULTICAST, 2);
	WriteCoord (MSG_MULTICAST, self.origin_x);
	WriteCoord (MSG_MULTICAST, self.origin_y);
	WriteCoord (MSG_MULTICAST, self.origin_z);
	multicast (self.origin, MULTICAST_PVS);

	if (self.cnt == 1)
		sound (self, CHAN_VOICE, "weapons/smoke.wav", 1, ATTN_NORM);

	self.nextthink = (time + 0.33);
	if (self.cnt >= 90)
		remove (self);
};

void () Screenshake =
{
	local entity te;

	te = findradius (self.origin, 700);
	while (te)
	{
		if (te.classname == "player")
			stuffcmd (te, "v_idlescale 5\n");
		te = te.chain;
	}
};


void () ExplosionFrames =
{
	self.avelocity = '300 300 250';
	self.nextthink = (time + 0.02);
	self.frame = self.frame + 1;
	if (self.frame == 60)
		remove (self);

	self.nextthink = (time + 0.02);
};

void () Explosion =
{
	local float r;

	//self.effects = EF_DIMLIGHT;
	//self.touch = SUB_Null;

	r = random ();
	if (r < 0.33)
		sound (self, CHAN_BODY, "misc/exp1.wav", 1, ATTN_NORM);
	if (r < 0.66)
		sound (self, CHAN_BODY, "misc/exp2.wav", 1, ATTN_NORM);
	else
		sound (self, CHAN_BODY, "misc/exp3.wav", 1, ATTN_NORM);

	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_EXPLOSION);
	WriteCoord (MSG_BROADCAST, self.origin_x);
	WriteCoord (MSG_BROADCAST, self.origin_y);
	WriteCoord (MSG_BROADCAST, self.origin_z);
	//Screenshake ();
	//self.frame = 3;
	//self.velocity = VEC_ORIGIN;
	//self.avelocity = '300 300 250';
	//self.think = ExplosionFrames;
	//self.nextthink = (time + 0.02);
};

void (float skill) FragExplode =
{
	local float r, d;
	local entity te;
	local vector dir;

	sound (self, CHAN_VOICE, "ambience/gunfire7.wav", 1, ATTN_NONE);
	self.origin = (self.origin + '0 0 16');
	setmodel (self, "progs/blast.mdl");
	setorigin (self, self.origin);
	setsize (self, '0 0 0', '0 0 0');

	Explosion();

	te = findradius(self.origin, 400);
	while (te)//bodies fly
	{
		if (te.classname == "body")
		{
			dir = normalize(te.origin - self.origin);
			te.velocity = dir * 100;
			te.velocity_z = 100;
			te.avelocity_y = (random()* 300);
		}

		te = te.chain;
	}

	r = 200 + skill*20;
	d = 10 + skill*10;

	T_RadiusDamage2 (self, self.owner, d+random()*40, other, r);
	remove (self);
};


void () PlasmaExplode =
{
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
	local float dot, effect;
	local vector vec;

	self.velocity = VEC_ORIGIN;
	setmodel (self, "progs/blast.mdl");
	WriteByte (0, SVC_TEMPENTITY);
	WriteByte (0, WEAPON_SPIKES);
	WriteCoord (0, self.origin_x);
	WriteCoord (0, self.origin_y);
	WriteCoord (0, self.origin_z);
	sound (self, CHAN_BODY, "misc/flash.wav", 1, ATTN_NORM);

	effect = 5 + (self.skill_science/3);

	te = findradius (self.origin, 200);
	while (te)
	{
		if (te.flags & FL_MONSTER)
		{
			if (CanDamage (self, te))
			{
				te.flash = effect;
				te.attack_finished = time + effect;
				spawn_excla(te, 12);
			}

		}
		if ((te.classname == "player"))
		{
			if (CanDamage (self, te))
			{
				te.missionbrief = 5;
				stuffcmd (te, "v_cshift 255 255 255 255\n");
				stuffcmd (te, "v_idlescale 3\n");
				te.flash = time + effect;

				spawn_excla(te, effect);
			}
		}
		te = te.chain;
	}

	te = findradius (self.origin, 800);
	while (te)
	{
		if (te.flags & FL_MONSTER)
		{
			makevectors (te.angles);
			vec = normalize ((self.origin - te.origin));
			dot = (vec * v_forward);

			if (dot > 0.3 && CanDamage (self, te))
			{
				te.flash = effect;
				te.attack_finished = time + effect;
				spawn_excla(te, effect);
			}

		}
		if ((te.classname == "player"))
		{
			makevectors (te.angles);
			vec = normalize ((self.origin - te.origin));
			dot = (vec * v_forward);

			if (dot > 0.3 && CanDamage (self, te))
			{
				stuffcmd (te, "v_cshift 255 255 255 255\n");
				stuffcmd (te, "v_idlescale 3\n");
				te.flash = time + effect;

				spawn_excla(te, effect);
			}
		}
		te = te.chain;
	}
	remove (self);
};


void () StunExplode =
{
	local entity te;
	local float dot, effect;
	local vector vec;

	self.velocity = VEC_ORIGIN;

	WriteByte (0, SVC_TEMPENTITY);
	WriteByte (0, WEAPON_SPIKES);
	WriteCoord (0, self.origin_x);
	WriteCoord (0, self.origin_y);
	WriteCoord (0, self.origin_z);

	te = findradius(self.origin, 400);

	while (te)
	{
		if (te.flags & FL_MONSTER)
		{
			te.hold = effect;

		}
		if (te.classname == "player")
			te.hold = effect;

		te = te.chain;
	}
	sound (self, CHAN_BODY, "misc/flash.wav", 1, ATTN_NORM);

	remove(self);
};

/*
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
				sound (self, CHAN_VOICE, "weapons/smoke.wav", 1, ATTN_NORM);
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
*/

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
	local float item, iid;
	local float amcount;
	local float w, x;

	item = ItemInSlot(self, self.grenslot);
	iid = ToIID(item);
	amcount = ToStatus(item);
	if (iid == 0)
		return;
	
	w = SlotOfItem(self, iid);

	DecreaseDestroySlot(w);

	//SetItemSlot(self, self.current_slot, w);

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

	if (iid == IID_GREN_FRAG)
		newmis.think = FragExplode;
	else if (iid == IID_GREN_EMP)
		newmis.think = EMPExplode;
	else if (iid == IID_GREN_STUN)
		newmis.think = StunExplode;
	else if (iid == IID_GREN_SMOKE)
		newmis.think = SmokeThink;
	else //if (iid == IID_GREN_FLASH)
		newmis.think = FlashExplode;

	newmis.frame = 1;
	setmodel (newmis, "progs/handgren.mdl");
	setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);
	setorigin (newmis, self.origin);
	self.grenslot = 0;
};



void(float slot, float magazine) GiveAmmo =
{
	local float wi;
	local float wt;
	local float at;
	local float as;

	//give ammo to the weapon in a slot
	wi = ItemInSlot(self, slot);
	wt = ToIID(wi);
	SetItemSlot(self, slot, SlotVal(wt, magazine));

	magazine = magazine*4;
	if (magazine < 30)
		magazine = 30;	//be generous.

	at = WeaponAmmoType(wt);
	//make sure we have enough ammo
	as = SlotOfItem(self, at);
	if (as == 0)
		as = SlotOfItem(self, IID_NONE);
	if (as == 0)
		return;	//no free slots, so we can't give them ammo
	if (ToStatus(ItemInSlot(self, as)) < magazine)
		SetItemSlot(self, as, SlotVal(at, magazine));
};




void (float dam, float ap, string snd, float rng, float rate) FirePistol =
{
      local float weap, tmp, zdif, xdif, ydif, is_headshot;
      local vector dir, source, targ, org, org2, org3, adjust;

	weap = ToIID(self.(SlotField(self.current_slot)));

	stuffcmd(self, "-attack\n");
	self.attack_finished = (time + rate);

	if (getperk(5))
	{
		if (self.steadyaim == 0)
			dam = dam + self.skill_doctor;
	}

	if (self.extender == weap && self.silencer != weap)
		dam = dam * 1.10;


	sound (self, CHAN_WEAPON, snd, 1, ATTN_NORM);

	if (weap == IID_WP_NEEDLER)
		loud_noise(5);
	else if (weap == IID_WP_USP)
		loud_noise(40);
	else if (weap == IID_WP_GLOCK)
		loud_noise(40);
	else if (weap == IID_WP_DEAGLE)
		loud_noise(50);
	else if (weap == IID_WP_USP_S)
		loud_noise(10);
	else if (weap == IID_WP_GLOCK_S)
		loud_noise(10);

		//only silenced weapons do not break stealth
			if (weap != IID_WP_NEEDLER && weap != IID_WP_USP_S && weap != IID_WP_GLOCK_S && self.sneak > 0)
			{
				self.equipment_slot = 0;
				sprint(self, PRINT_HIGH, "ç Uncloaked ç\n");
				setmodel (self, "progs/guy.mdl");
				self.sneak = 0;
				sound (self, CHAN_BODY, "misc/r_tele4.wav", 1, ATTN_NORM);
			}	

	if (self.position == 0)
		player_single1();
	if (self.position == 1)
		player_single1_s();
	if (self.position == 2)
		player_single1_s();


	if (self.position == 0)
		adjust = '0 0 0';
	if (self.position == 1)
		adjust = '0 0 -16';
	if (self.position == 2)
		adjust = '0 0 -32';


	DropAmmo ();
	makevectors (self.v_angle);
	if (self.recoil >= 10)
		self.recoil = 10;



	tmp = 50;

	if (self.velocity == '0 0 0')
		tmp = tmp + (20 * self.recoil);
	else
		tmp = tmp + (40 * self.recoil);

	if (self.steadyaim == 0)
		tmp = tmp * 0.50;

	self.attack = self.attack + 1;
	self.recoil = self.recoil + ap;

	source = self.origin + '0 0 22';

	targ = self.origin + '0 0 22' + v_right*crandom()* tmp + v_up*crandom()*tmp;

	traceline (source+adjust, targ+adjust+v_forward*4000, FALSE, self);
	if (trace_fraction == 1)
		return;

	org3 = self.origin + v_forward*32 + '0 0 22';

	org = trace_endpos - v_forward * 1;
	org2 = trace_endpos + (v_forward * ((trace_ent.size_y / 2) + (trace_ent.size_x / 2)));
	if (trace_ent.takedamage)
	{
		org2 = (trace_endpos + (v_forward * ((trace_ent.size_y / 2) + (trace_ent.size_x / 2))));
		zdif = org_z - trace_ent.origin_z;
		ydif = org2_y - trace_ent.origin_y;
		xdif = org2_x - trace_ent.origin_x;


		is_headshot = 0;

		if (((ydif >= -6) && (ydif <= 6)))
			is_headshot = 1;
		if (((xdif >= -6) && (xdif <= 6)))
			is_headshot = 1;
		if (self.attack <= 5 && is_headshot == 1 && zdif >= (trace_ent.size_z / 2 * 0.8))
			self.critical = 3;

		dam = (dam * (1 - trace_fraction));
		if (trace_ent.solid != SOLID_BSP && trace_ent.classname != "car" && (trace_ent.islot3 == 0 || self.critical == 3))
			SpawnBlood (org, 1);
		else if (trace_ent.solid != SOLID_BSP && trace_ent.classname != "car" && trace_ent.islot3 > 0)
			SpawnNonBlood (org, 1);
		else if (trace_ent.solid == SOLID_BSP || trace_ent.classname == "car")
			SpawnWood (trace_ent, org, 1);

		dam = dam + random()*dam;

		if (weap == IID_WP_NEEDLER)
			X_Damage (trace_ent, self, self, dam);
		else if (weap == IID_WP_USP)
			J_Damage (trace_ent, self, self, dam);
		else
			T_Damage (trace_ent, self, self, dam);

		loud_noise(5);//hit flesh

		//victim survived the ambush
		if (random()*10 < 8 && trace_ent.health > 0 && self.sneak > 0)
		{
				self.equipment_slot = 0;
				sprint(self, PRINT_HIGH, "ç Uncloaked ç\n");
				setmodel (self, "progs/guy.mdl");
				self.sneak = 0;
				spawn_excla(self, 3);
		}

		self.critical = 0;
	}
	else
	{
		te_smallflash(org);
		bullet_hole (org);
		dir = vectoangles (source - targ);
		return;
	}
};

void (float dam, float ap, string snd, float rng, float rate) FireSMG =
{
      local float weap, tmp, zdif, xdif, ydif, is_headshot;
      local vector dir, source, targ, org, org2, adjust;

	weap = ToIID(self.(SlotField(self.current_slot)));

	if (getperk(5))
	{
		if (self.steadyaim == 0)
			dam = dam + self.skill_doctor;
	}

	sound (self, CHAN_WEAPON, snd, 1, ATTN_NORM);
	if (weap == IID_WP_GREASEGUN)
		loud_noise(50);
	else if (weap == IID_WP_MP9)
		loud_noise(40);
	else if (weap == IID_WP_GREASEGUN_S)
		loud_noise(5);
	else if (weap == IID_WP_MP9_S)
		loud_noise(5);

	//only silenced weapons do not break stealth
	if (weap != IID_WP_GREASEGUN_S && weap != IID_WP_MP9_S && self.sneak > 0)
	{
		self.equipment_slot = 0;
		sprint(self, PRINT_HIGH, "ç Uncloaked ç\n");
		setmodel (self, "progs/guy.mdl");
		self.sneak = 0;
		sound (self, CHAN_BODY, "misc/r_tele4.wav", 1, ATTN_NORM);
	}

	self.attack_finished = (time + rate);

	if (self.position == 0)
		autofire ();
	if (self.position >= 1)
		autofire_s ();

	if (self.position == 0)
		adjust = '0 0 0';
	if (self.position == 1)
		adjust = '0 0 -16';
	if (self.position == 2)
		adjust = '0 0 -32';


	if (self.attack > 0)
	{
		if (random()*3<=2)
			self.driftx = self.driftx + 90;
		else
			self.driftx = self.driftx - 90;
	
		if (random()*2<=1)
			self.drifty = self.drifty + 90;
		else
			self.drifty = self.drifty - 90;
	}

	DropAmmo ();
	makevectors (self.v_angle);

	if (self.recoil >= 10)
		self.recoil = 10;

	tmp = 150;

	if (self.velocity == '0 0 0')
		tmp = tmp + (15*self.recoil);
	else
		tmp = tmp + (25*self.recoil);

	self.attack = self.attack + 1;
	self.recoil = self.recoil + 4;

	source = self.origin + '0 0 22';

	tmp = tmp * 0.50;

	if (self.steadyaim == 0)
		tmp = tmp * 0.50;

	targ = self.origin + '0 0 22' + v_right*self.driftx + v_up*self.drifty + v_right*crandom()* tmp + v_up*random()*(tmp*2);

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
		is_headshot = 0;
		if (((ydif >= -5) && (ydif <= 5)))
			is_headshot = 1;
		if (((xdif >= -5) && (xdif <= 5)))
			is_headshot = 1;
		if (self.attack == 1 && is_headshot == 1 && zdif >= (trace_ent.size_z / 2 * 0.8))
			self.critical = 3;

		dam = (dam * (1 - trace_fraction));
		if (trace_ent.solid != SOLID_BSP && trace_ent.classname != "car" && (trace_ent.islot3 == 0 || self.critical == 3))
			SpawnBlood (org, 1);
		else if (trace_ent.solid != SOLID_BSP && trace_ent.classname != "car" && trace_ent.islot3 > 0)
			SpawnNonBlood (org, 1);
		if (trace_ent.solid == SOLID_BSP || trace_ent.classname == "car")
			SpawnWood (trace_ent, org, 1);


		dam = dam + random()*dam;

	if (weap == IID_WP_GREASEGUN)
		J_Damage (trace_ent, self, self, dam);
	else if (weap == IID_WP_MP9)
		T_Damage (trace_ent, self, self, dam);
	else if (weap == IID_WP_GREASEGUN_S)
		J_Damage (trace_ent, self, self, dam);
	else if (weap == IID_WP_MP9_S)
		T_Damage (trace_ent, self, self, dam);

		T_Damage (trace_ent, self, self, dam);
		self.critical = 0;

		if (trace_ent.solid == SOLID_BSP || trace_ent.classname == "car")
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

void (float rec, float speed, float dam, float type) FirePulseRifle =
{
      local float dam, weap, tmp, zdif, xdif, ydif, is_headshot;
      local vector dir, source, targ, org, org2, adjust;
	local entity zap;

	weap = ToIID(self.(SlotField(self.current_slot)));

	if (weap != IID_WP_ALIENBLASTER)
	{
		if (self.recoil >= 15)
		{
			loud_noise(60);
			sound (self, CHAN_WEAPON, "weapons/blowoff-1.wav", 1, ATTN_NORM);
			self.attack_finished = (time + (1 + random()*1.5));
			return;
		}
		else if (weap != IID_WP_LASERGATLING)
		{
			loud_noise(60);
			sound (self, CHAN_AUTO, "weapons/blowoff-2.wav", 0.5, ATTN_IDLE);
		}
	}


	if (weap == IID_WP_ALIENBLASTER)
		stuffcmd (self, "-attack\n");

	if (weap != IID_WP_LASERGATLING)
	{
		loud_noise(20);

		if (random()<0.5)
			sound (self, CHAN_WEAPON, "weapons/plasma-1.wav", 1, ATTN_NORM);
		else
			sound (self, CHAN_WEAPON, "weapons/plasma-2.wav", 1, ATTN_NORM);
	}
	else if (weap == IID_WP_LASERGATLING && self.attack == 1)
	{
			loud_noise(40);
			sound (self, CHAN_WEAPON, "weapons/laserburst1.wav", 1, ATTN_NORM);
	}

	if (self.attack >= 6)
		self.attack = 0;

	self.attack_finished = time + (speed*0.5) + (random()*speed*0.5);


//only silenced weapons do not break stealth
	if (self.sneak > 0)
	{
		self.equipment_slot = 0;
		sprint(self, PRINT_HIGH, "ç Uncloaked ç\n");
		setmodel (self, "progs/guy.mdl");
		self.sneak = 0;
		sound (self, CHAN_BODY, "misc/r_tele4.wav", 1, ATTN_NORM);
	}

	if (self.position == 0)
		autofire ();
	if (self.position >= 1)
		autofire_s ();

	if (self.position == 0)
		adjust = '0 0 -4';
	if (self.position == 1)
		adjust = '0 0 -20';
	if (self.position == 2)
		adjust = '0 0 -36';


	DropAmmo ();
	makevectors (self.v_angle);

	if (self.recoil >= 20)
		self.recoil = 20;

	tmp = 50;

	if (self.velocity != '0 0 0')
		tmp = 100;

	tmp = tmp + (5*self.recoil);

	if (self.recoil <= 1 && self.velocity_z == 0)
		tmp = (tmp * 0.5);

	self.attack = self.attack + 1;
	self.recoil = self.recoil + rec;


	source = self.origin + '0 0 22' + v_right*6 + v_forward*14;
	source = source + adjust;

	tmp = tmp * 0.60;

	if (self.position == 2)
		tmp = tmp * 0.60;

	targ = source + v_right*crandom()* tmp + v_up*crandom()*(tmp*2);

	traceline (source, targ+adjust+v_forward*2000, FALSE, self);
	if (trace_fraction == 1)
		return;

	org = trace_endpos - v_forward * 2;
	org2 = trace_endpos + (v_forward * ((trace_ent.size_y / 2) + (trace_ent.size_x / 2)));

	zap = spawn();
	zap.origin = self.origin + v_right*16 + adjust;
	zap.velocity = self.velocity;
	zap.angles = self.angles;
	zap.v_angle = self.v_angle;

	if (type == 1)
	{
		WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
		WriteByte (MSG_MULTICAST, TE_LIGHTNING1);
		WriteEntity (MSG_MULTICAST, zap);
		WriteCoord (MSG_MULTICAST, source_x);
		WriteCoord (MSG_MULTICAST, source_y);
		WriteCoord (MSG_MULTICAST, source_z);
		WriteCoord (MSG_MULTICAST, trace_endpos_x);
		WriteCoord (MSG_MULTICAST, trace_endpos_y);
		WriteCoord (MSG_MULTICAST, trace_endpos_z);
		multicast (org, MULTICAST_PHS);
	}
	if (type == 2)
	{
		WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
		WriteByte (MSG_MULTICAST, TE_LIGHTNING2);
		WriteEntity (MSG_MULTICAST, zap);
		WriteCoord (MSG_MULTICAST, source_x);
		WriteCoord (MSG_MULTICAST, source_y);
		WriteCoord (MSG_MULTICAST, source_z);
		WriteCoord (MSG_MULTICAST, trace_endpos_x);
		WriteCoord (MSG_MULTICAST, trace_endpos_y);
		WriteCoord (MSG_MULTICAST, trace_endpos_z);
		multicast (org, MULTICAST_PHS);
	}
	if (type == 3)
	{
		WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
		WriteByte (MSG_MULTICAST, TE_LIGHTNING3);
		WriteEntity (MSG_MULTICAST, zap);
		WriteCoord (MSG_MULTICAST, source_x);
		WriteCoord (MSG_MULTICAST, source_y);
		WriteCoord (MSG_MULTICAST, source_z);
		WriteCoord (MSG_MULTICAST, trace_endpos_x);
		WriteCoord (MSG_MULTICAST, trace_endpos_y);
		WriteCoord (MSG_MULTICAST, trace_endpos_z);
		multicast (org, MULTICAST_PHS);
	}
	remove(zap);

	if (trace_ent.takedamage)
	{

		dam = (dam * (1 - trace_fraction));
		if (trace_ent.solid != SOLID_BSP && trace_ent.classname != "car" && (trace_ent.islot3 == 0 || self.critical == 3))
			SpawnBlood (org, 1);
		else if (trace_ent.solid != SOLID_BSP && trace_ent.classname != "car" && trace_ent.islot3 > 0)
			SpawnNonBlood (org, 1);
		else if (trace_ent.solid == SOLID_BSP || trace_ent.classname == "car")
			SpawnWood (trace_ent, org, 1);


		dam = dam + random()*dam;

		T_Damage (trace_ent, self, self, dam);
		self.critical = 0;

		if (trace_ent.solid == SOLID_BSP || trace_ent.classname == "car")
			penetrate (org, (dam / 2), (dam / 2));
	}
};

void (float dam, float accuracy, string snd, float rng, float rate) FireAssaultRifle =
{
      local float weap, hs, tmp, zdif, is_headshot, z, y;
      local vector dir, source, targ, org, adjust, headshot_check;
	local float xrecoil;

	//xrecoil: 8 is average, 12 high, 6 low

	weap = ToIID(self.(SlotField(self.current_slot)));

	if (getperk(5))
	{
		if (self.steadyaim == 0)
			dam = dam + self.skill_doctor;
	}

	if (weap == IID_WP_MOONLIGHT)
	{
		if (random()<0.5)
			sound (self, CHAN_WEAPON, "weapons/moonlight-1.wav", 1, ATTN_NORM);
		else
			sound (self, CHAN_WEAPON, "weapons/moonlight-2.wav", 1, ATTN_NORM);
	}
	else
	{

		if (weap == IID_WP_DKS1_S)
			sound (self, CHAN_WEAPON, "ogre/ak74.wav", 1, ATTN_NORM);
		else
			sound (self, CHAN_WEAPON, snd, 1, ATTN_NORM);

		if (weap == IID_WP_DKS1)
			loud_noise(100);
		else if (weap == IID_WP_ACR)
			loud_noise(110);
		else if (weap == IID_WP_MOONLIGHT)
			loud_noise(5);
		else if (weap == IID_WP_DKS1_S)
			loud_noise(15);
		else if (weap == IID_WP_PIPERIFLE_S)
			loud_noise(5);
		else if (weap == IID_WP_PIPERIFLE_S_R)
			loud_noise(5);
		else if (weap == IID_WP_PIPERIFLE_SCOPE_S)
			loud_noise(5);
		else if (weap == IID_WP_PIPERIFLE_SCOPE_S_R)
			loud_noise(5);
		else
			loud_noise(80);

		if (weap == IID_WP_DKS1)
			stuffcmd(self, "v_idlescale 10\n");
		else if (weap == IID_WP_FNFAL)
			stuffcmd(self, "v_idlescale 10\n");
		else if (weap == IID_WP_DKS1_S)
			stuffcmd(self, "v_idlescale 10\n");

		//only silenced weapons do not break stealth
		if (weap != IID_WP_MOONLIGHT && weap != IID_WP_DKS1_S && self.sneak > 0)
		{
			self.equipment_slot = 0;
			sprint(self, PRINT_HIGH, "ç Uncloaked ç\n");
			setmodel (self, "progs/guy.mdl");
			self.sneak = 0;
			sound (self, CHAN_BODY, "misc/r_tele4.wav", 1, ATTN_NORM);
		}
	}

	self.attack_finished = (time + rate);

	if (self.position == 0)
		adjust = '0 0 0';
	if (self.position == 1)
		adjust = '0 0 -16';
	if (self.position == 2)
		adjust = '0 0 -32';

	DropAmmo ();
	makevectors (self.v_angle);


	if (self.attack == 0)
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

	tmp = accuracy * 10;
	xrecoil = dam / 2;


	//for the snipers
	if (self.position == 2 && self.recoil == 0)
		tmp = tmp * 0.75;

	if (self.steadyaim == 0)
		tmp = tmp * 0.25;

	self.attack = self.attack + 1;
	self.recoil = self.recoil + xrecoil;
	if (self.recoil >= 10)
		self.recoil = 10;

	if (self.zoom > 0 && weap == IID_WP_G11 && self.attack == 3)
	{
		self.recoil = self.recoil + 10;
		self.attack_finished = time + 1;
	}

	if (tmp > 500)
		tmp = 500;

	tmp = tmp * 0.75;

	source = self.origin + '0 0 22';

	targ = self.origin + '0 0 22' + v_right*self.driftx + v_up*self.drifty + v_right*crandom()* tmp + v_up*random()*tmp + v_up*random()*tmp;

	y = xrecoil * 12;

	if (self.attack == 0)
		y = y * 0.90;
	if (self.velocity == '0 0 0')
		y = y * 0.90;
	if (self.position == 2)
		y = y * 0.80;
	if (self.position == 1)
		y = y * 0.90;

	if (self.position == 2 && weap == IID_WP_AK112_M)
	{
		if (self.recoil >= 8)
			self.recoil = 8;

		y = y * 0.25;
		tmp = tmp * 0.80;
	}
	if (random()*3<=2)
		self.driftx = self.driftx + y;
	else
		self.driftx = self.driftx - y;

	if (random()*2<=1)
		self.drifty = self.drifty + y;
	else
		self.drifty = self.drifty - y;

	traceline (source+adjust, targ+adjust+v_forward*4000, FALSE, self);
	if (trace_fraction == 1)
		return;

	org = trace_endpos - v_forward * 2;
	headshot_check = trace_endpos + v_forward * (trace_ent.size_x / 2);

	if (weap == IID_WP_PIPERIFLE)
		stuffcmd (self, "-attack\n");
	if (weap == IID_WP_RANGEMASTER)
		stuffcmd (self, "-attack\n");
	if (weap == IID_WP_DKS1)
		stuffcmd (self, "-attack\n");
	if (weap == IID_WP_FNFAL && self.zoom == 1)
		stuffcmd (self, "-attack\n");
	if (self.zoom > 0 && weap != IID_WP_G11 && weap != IID_WP_MOONLIGHT && weap != IID_WP_AK112_M)
		stuffcmd (self, "-attack\n");
	if (self.zoom > 0 && weap == IID_WP_G11 && self.attack >= 3 && weap != IID_WP_MOONLIGHT)
		stuffcmd (self, "-attack\n");
	if (weap == IID_WP_GAUSERIFLE)
		stuffcmd (self, "-attack\n");
	if (weap == IID_WP_ACR)
		stuffcmd (self, "-attack\n");

	if (trace_ent.takedamage)
	{


		if (self.attack <= 3)
		{
			is_headshot = 0;
			hs = headshot_check_x - trace_ent.origin_x;
			hs = hs + headshot_check_y - trace_ent.origin_y;
	
			if (hs < 0)
				hs = hs * -1;

			if (hs <= 8)
				is_headshot = 1;

			zdif = trace_endpos_z - trace_ent.origin_z;
	
			if (is_headshot == 1 && zdif >= (trace_ent.size_z / 2 * 0.8))
				self.critical = 3;
		}


		dam = (dam * (1 - trace_fraction));

		if (trace_ent.solid != SOLID_BSP && trace_ent.classname != "car" && (trace_ent.islot3 == 0 || self.critical == 3))
			SpawnBlood (org, 1);
		else if (trace_ent.solid != SOLID_BSP && trace_ent.classname != "car" && trace_ent.islot3 > 0)
			SpawnNonBlood (org, 1);
		else if (trace_ent.solid == SOLID_BSP || trace_ent.classname == "car")
			SpawnWood (trace_ent, org, 1);

		dam = dam + random()*dam;

		if (weap == IID_WP_DKS1 || weap == IID_WP_FNFAL || weap == IID_WP_ACR)
			X_Damage (trace_ent, self, self, dam);
		else if (weap == IID_WP_AK112)
			J_Damage (trace_ent, self, self, dam);
		else
			T_Damage (trace_ent, self, self, dam);

		loud_noise(5);//hit flesh

		//victim survived the ambush
		if (random()*10 < 7.5 && trace_ent.health > 0 && self.sneak > 0)
		{
				self.equipment_slot = 0;
				sprint(self, PRINT_HIGH, "ç Uncloaked ç\n");
				setmodel (self, "progs/guy.mdl");
				self.sneak = 0;
				spawn_excla(self, 3);
		}


		self.critical = 0;

		if (trace_ent.solid == SOLID_BSP || trace_ent.classname == "car")
			penetrate (org, (dam / 2), (dam / 2));
	}
	else
	{
		loud_noise(15);
		bullet_hole (org);
		dir = vectoangles (source - targ);
		penetrate (org, (dam / 2), (dam / 2));
		return;
	}
};



void (float dam, float xrecoil, string snd, float rng, float rate) FireMinigun =
{
      local float weap, hs, tmp, zdif, is_headshot, z;
      local vector dir, source, targ, org, adjust, headshot_check;

	//xrecoil: 40 is average, 60 high, 30 low

	weap = ToIID(self.(SlotField(self.current_slot)));

	if (self.attack <= 3)
	{
		self.weaponframe = self.attack;
		self.attack_finished = time + 0.10;
		self.attack = self.attack + 1;
		return;
	}

	if (self.attack == 4)
		self.weaponframe = 5;
	if (self.attack == 5)
		self.weaponframe = 6;
	if (self.attack == 6)
		self.weaponframe = 7;
	if (self.attack == 7)
		self.weaponframe = 8;
	if (self.attack == 8)
	{
		self.attack = 4;
		self.weaponframe = 9;
	}

	if (getperk(5))
	{
		if (self.steadyaim == 0)
			dam = dam + self.skill_doctor;
	}

	loud_noise(50);

	sound (self, CHAN_WEAPON, "weapons/ak112.wav", 0.8, ATTN_NORM);

	self.attack_finished = (time + rate);

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

	tmp = 50;

	if (self.velocity_y < 0)
		z = z + (self.velocity_y*-1);
	else if (self.velocity_y > 0)
		z = z + (self.velocity_y);

	if (self.velocity_x < 0)
		z = z + (self.velocity_x*-1);
	else if (self.velocity_x > 0)
		z = z + (self.velocity_x);

	tmp = tmp + z;
	tmp = tmp + (xrecoil * self.recoil);

	if (self.attack <= 3 && self.position == 1 && self.velocity_z == 0)
		tmp = (tmp * 0.75);

	if (self.attack <= 3 && self.position == 2 && self.velocity_z == 0)
		tmp = (tmp * 0.5);

	self.attack = self.attack + 1;
	self.recoil = self.recoil + 5;

	source = self.origin + '0 0 22';

	targ = self.origin + '0 0 22' + v_right*crandom()* tmp + v_up*random()*tmp;

	traceline (source+adjust, targ+adjust+v_forward*4000, FALSE, self);
	if (trace_fraction == 1)
		return;

	org = trace_endpos - v_forward * 2;
	headshot_check = trace_endpos + v_forward * (trace_ent.size_x / 2);

	if (weap == IID_WP_PIPERIFLE)
		stuffcmd (self, "-attack\n");
	if (weap == IID_WP_RANGEMASTER)
		stuffcmd (self, "-attack\n");
	if (weap == IID_WP_DKS1)
		stuffcmd (self, "-attack\n");
	if (weap == IID_WP_FNFAL && self.zoom == 1)
		stuffcmd (self, "-attack\n");
	if (self.zoom > 0 && weap != IID_WP_G11 && weap != IID_WP_MOONLIGHT)
		stuffcmd (self, "-attack\n");
	if (self.zoom > 0 && weap == IID_WP_G11 && self.attack >= 3 && weap != IID_WP_MOONLIGHT)
		stuffcmd (self, "-attack\n");
	if (weap == IID_WP_GAUSERIFLE)
		stuffcmd (self, "-attack\n");

	if (trace_ent.takedamage)
	{


		is_headshot = 0;
		hs = headshot_check_x - trace_ent.origin_x;
		hs = hs + headshot_check_y - trace_ent.origin_y;

		if (hs < 0)
			hs = hs * -1;

		if (hs <= 4)
			is_headshot = 1;

		zdif = trace_endpos_z - trace_ent.origin_z;

		if (is_headshot == 1 && zdif >= (trace_ent.size_z / 2 * 0.8))
			self.critical = 3;



		dam = (dam * (1 - trace_fraction));

		if (trace_ent.solid != SOLID_BSP && trace_ent.classname != "car" && (trace_ent.islot3 == 0 || self.critical == 3))
			SpawnBlood (org, 1);
		else if (trace_ent.solid != SOLID_BSP && trace_ent.classname != "car" && trace_ent.islot3 > 0)
			SpawnNonBlood (org, 1);
		else if (trace_ent.solid == SOLID_BSP || trace_ent.classname == "car")
			SpawnWood (trace_ent, org, 1);

		dam = dam + random()*dam;

		if (weap == IID_WP_DKS1)
			X_Damage (trace_ent, self, self, dam);
		else if (weap == IID_WP_FNFAL)
			X_Damage (trace_ent, self, self, dam);
		else if (weap == IID_WP_DKS1_S)
			X_Damage (trace_ent, self, self, dam);
		else if (weap == IID_WP_AK112)
			X_Damage (trace_ent, self, self, dam);
		else if (weap == IID_WP_AK112_M)
			X_Damage (trace_ent, self, self, dam);
		else
			T_Damage (trace_ent, self, self, dam);

		loud_noise(5);//hit flesh

		//victim survived the ambush
		if (random()*10 < 7.5 && trace_ent.health > 0 && self.sneak > 0)
		{
				self.equipment_slot = 0;
				sprint(self, PRINT_HIGH, "ç Uncloaked ç\n");
				setmodel (self, "progs/guy.mdl");
				self.sneak = 0;
				spawn_excla(self, 3);
		}


		self.critical = 0;

		if (trace_ent.solid == SOLID_BSP || trace_ent.classname == "car")
			penetrate (org, (dam / 2), (dam / 2));
	}
	else
	{
		loud_noise(15);
		bullet_hole (org);
		dir = vectoangles (source - targ);
		penetrate (org, (dam / 2), (dam / 2));
		return;
	}
};


void () WeaponTouch =
{
	local float slotnum, x;
	if (other.classname != "player")
		return;
	if (other.ghost != 0)
		return;
	if (other.accept == 0)
		return;

	other.accept = 0;

	x = (ToIID(self.islot1));

	if (ItemInSlot(other, other.current_slot) == 0 && FitsInSlot(other.current_slot, ToIID(self.islot1)))
		slotnum = other.current_slot;
	if (!slotnum)
		slotnum = FindSuitableEmptySlot(other, ToIID(self.islot1));
	if (!slotnum)
		return;	//can't get it.

	sound (other, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);

	SetItemSlot(other, slotnum, self.islot1);

	sprint(other, PRINT_MEDIUM, "You pick up the ");
	sprint(other, PRINT_MEDIUM, GetItemName(ToIID(self.islot1)));
	sprint(other, PRINT_MEDIUM, "\n");
	remove (self);

	self = other;
	if (self.current_slot == slotnum)
		W_SetCurrentAmmo ();
};

void () AmmoTouch =
{
	local float slotnum,x,y,z;

	if (other.classname != "player")
		return;

	if (other.ghost != 0)
		return;

	x = ToIID(other.islot1);
	y = ToIID(other.islot2);
	z = ToIID(self.islot1);

	x = WeaponAmmoType(x);
	y = WeaponAmmoType(y);


	if ((x != z) && (y != z) && (other.accept == 0))
		return;

	other.accept = 0;

	if (!TryGiveStackable(other, ToIID(self.islot1), self.flash))
	{	
		sprint(other, PRINT_MEDIUM, "inventory full.\n");
		return;
	}

	sound (other, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);

	sprint(other, PRINT_MEDIUM, "found ");
	sprint(other, PRINT_MEDIUM, GetItemName(ToIID(self.islot1)));
	sprint(other, PRINT_MEDIUM, ".\n");

	remove (self);

	self = other;
	if (self.current_slot == slotnum)
		W_SetCurrentAmmo ();
};

void () MultiTouch =
{
	local float slotnum,x,y,z;

	if (other.classname != "player")
		return;

	if (other.ghost != 0)
		return;

	if (other.accept == 0)
		return;

	other.accept = 0;

	if (!TryGiveStackable(other, ToIID(self.islot1), self.flash))
	{	
		sprint(other, PRINT_MEDIUM, "inventory full.\n");
		return;
	}

	sound (other, CHAN_BODY, "misc/item1.wav", 1, ATTN_NORM);

	sprint(other, PRINT_MEDIUM, "found ");
	sprint(other, PRINT_MEDIUM, GetItemName(ToIID(self.islot1)));
	sprint(other, PRINT_MEDIUM, ".\n");

	remove (self);

	self = other;
	if (self.current_slot == slotnum)
		W_SetCurrentAmmo ();
};

void (float slotnum, float snd, float force) DropFromSlot =
{
	local float weap;
	local string mdel;

	if (self.attack_finished > time && !force)
		return;

	if (slotnum == 0)
		return;

	weap = ItemInSlot(self, slotnum);
	if (weap == 0)
	{
		sprint(self, PRINT_HIGH, "Can't drop nothing\n");
		return;
	}

	if (snd == 1)
		sound (self, CHAN_WEAPON, "weapons/lock4.wav", 1, ATTN_NORM);

	makevectors (self.v_angle);
	newmis = spawn ();
	newmis.owner = self;
	newmis.classname = "dropped_weapon";
	newmis.movetype = MOVETYPE_TOSS;
	newmis.solid = SOLID_TRIGGER;
	newmis.flags = FL_ITEM;	//item makes it bigger to player touches.
	newmis.velocity = aim (self, 500);
	newmis.velocity = (newmis.velocity * 500);
	newmis.angles_y = (random () * 360);

	mdel = GetItemWModel(ToIID(weap));

	setmodel (newmis, mdel);
	setsize (newmis, '-2 -2 0', '2 2 1');
	makevectors (self.v_angle);
	traceline (self.origin, ((self.origin + (v_forward * IT_LIGHTNING)) + '0 0 32'), FALSE, self);
	trace_endpos = (trace_endpos - (v_forward * WEAPON_SPIKES));
	setorigin (newmis, trace_endpos);
	newmis.origin_z = self.origin_z;
	newmis.nextthink = (time + 180);
	newmis.think = SUB_Remove;
	newmis.islot1 = weap;

	if (ToIID(weap) >= IID_AM_NEEDLER && ToIID(weap) <= IID_AM_WARSAW)
	{
		newmis.flash = ToStatus(ItemInSlot(self, slotnum));
		newmis.touch = AmmoTouch;
		newmis.skin = 1;
	}
	else if (ToIID(weap) >= IID_MISC_JUNK && ToIID(weap) <= IID_MISC_XRAYTUBE)
	{
		newmis.flash = ToStatus(ItemInSlot(self, slotnum));
		newmis.touch = MultiTouch;
	}
	else if (ToIID(weap) >= IID_CHEM_STIMPACK && ToIID(weap) <= IID_CHEM_RADX)
	{
		newmis.flash = ToStatus(ItemInSlot(self, slotnum));
		newmis.touch = MultiTouch;
	}
	else
		newmis.touch = WeaponTouch;


	SetItemSlot(self, slotnum, 0);

	if (self.attack_finished < time+0.6)
		self.attack_finished = time+0.6;

	if (self.current_slot == slotnum)
		SetWeaponModel ();

	if (self.equipment_slot == slotnum)
		self.equipment_slot = 0;
		
};

void (entity chest, float iid, float num) DropFromChest =
{
	local float weap;
	local string mdel;

	makevectors (self.v_angle);
	newmis = spawn ();
	newmis.owner = self;
	newmis.classname = "dropped_weapon";
	newmis.movetype = MOVETYPE_TOSS;
	newmis.solid = SOLID_TRIGGER;
	newmis.flags = FL_ITEM;	//item makes it bigger to player touches.
	newmis.velocity = aim (self, 500);
	newmis.velocity = (newmis.velocity * 500);
	newmis.angles_y = (random () * 360);
	newmis.angles_x = (random () * 360);
	newmis.velocity_x = (random () * 360);
	newmis.velocity_x = (random () * 360);


	mdel = GetItemWModel(iid);

	setmodel (newmis, mdel);
	setsize (newmis, '-2 -2 0', '2 2 1');
	makevectors (self.v_angle);
	setorigin (newmis, self.origin + '0 0 48');
	newmis.origin_z = self.origin_z;
	newmis.nextthink = (time + 180);
	newmis.think = SUB_Remove;
	newmis.islot1 = iid*512;

	if (iid >= IID_AM_NEEDLER && iid <= IID_AM_WARSAW)
	{
		newmis.flash = num;
		newmis.touch = AmmoTouch;
		newmis.skin = ToIID(weap) - 506;
	}
	else if (iid >= IID_MISC_JUNK && iid <= IID_MISC_XRAYTUBE)
	{
		newmis.flash = num;
		newmis.touch = MultiTouch;
	}
	else if (iid >= IID_CHEM_STIMPACK && iid <= IID_CHEM_RADX)
	{
		newmis.flash = num;
		newmis.touch = MultiTouch;
	}
	else if (iid >= IID_ARM_LEATHER && iid <= IID_ARM_LPOWER)
	{
		newmis.flash = num;
		newmis.touch = MultiTouch;
	}
	else
	{
		newmis.islot1 = (iid*512) + num;
		newmis.touch = WeaponTouch;
	}		
};

void (float slot) WeaponAmmo =
{
	local float weap, amount;

	weap = ToIID(ItemInSlot(self, slot));

	amount = WeaponMagQuant(weap);
	GiveAmmo (slot, amount);
};


float(float iid) UseBoostingChem =
{
	local vector source;
	local string x;
	local float duration;

	if (self.attack_finished > time)
		return false;

	x = GetItemName (iid);
	if (iid == IID_CHEM_ADRENALINE)
		duration = 30;
	else if (iid == IID_CHEM_PSYCHO)
		duration = 30;
	else if (iid == IID_CHEM_BESERK)
		duration = 60;
	else if (iid == IID_CHEM_RADX)
		duration = 120;
	else
	{
		sprint (self, PRINT_HIGH, "Not a boosting chem\n");
		return false;
	}


	duration = duration * (1 + (self.skill_doctor / 10));

	self.attack_finished = time + 1;

	makevectors (self.v_angle);
	source = self.origin + '0 0 0';
	traceline (source, source + v_forward*64, FALSE, self);
	if (trace_ent.classname == "player" && trace_ent.team == self.team)
	{
		if (trace_ent.health <= 0)
			return false;

		if (trace_ent.rage >= 1)
		{
			sprint (self, 2, trace_ent.netname);
			sprint (self, PRINT_HIGH, " is already affected.\n");
			return false;
		}
		sprint (trace_ent, 2, self.netname);
		sprint (trace_ent, PRINT_HIGH, " used a ");
		sprint (trace_ent, PRINT_HIGH, x);
		sprint (trace_ent, PRINT_HIGH, " on you.\n");
		if (iid != IID_CHEM_RADX)
			sound (trace_ent, CHAN_BODY, "player/berserk.wav", 1, ATTN_NORM);
		if (iid == IID_CHEM_RADX)
			sound (trace_ent, CHAN_BODY, "items/r_item2.wav", 1, ATTN_NORM);
		stuffcmd(trace_ent, "v_cshift 200 0 0 200\n");
		trace_ent.rage = iid;
		trace_ent.ragetime = duration;
		return true;
	}
	if (self.health < self.max_health && self.rage == 0)
	{
		stuffcmd(self, "v_cshift 200 0 0 200\n");
		if (iid != IID_CHEM_RADX)
			sound (self, CHAN_BODY, "player/berserk.wav", 1, ATTN_NORM);
		if (iid == IID_CHEM_RADX)
			sound (self, CHAN_BODY, "items/r_item2.wav", 1, ATTN_NORM);
		self.rage = iid;
		self.ragetime = duration;
		return true;
	}
	return false;
};

float(float iid) UseHealingChem =
{
	local vector source;
	local float heal;
	local string x;
	local float friendly;


	if (self.attack_finished > time)
		return false;

	x = GetItemName (iid);
	if (iid == IID_CHEM_STIMPACK)
		heal = 10;
	else if (iid == IID_CHEM_MEDICALBAG)
	{
		heal = 5;
		x = "medkit";
	}
	else if (iid == IID_CHEM_SUPERSTIM)
		heal = 50;
	else
	{
		sprint (self, PRINT_HIGH, "Not a healing chem\n");
		return false;
	}
	

	self.attack_finished = time + 1;
	self.rtime = time + 0.9;

	makevectors (self.v_angle);
	source = self.origin + '0 0 0';
	traceline (source, source + v_forward*64, FALSE, self);
	if (trace_ent.classname == "player")
	{
		if (coop)
			friendly = true;	//all players are friendly in coop
		if (teamplay)
			friendly = trace_ent.team == self.team;
		if (trace_ent.health <= 0 && friendly)
		{
			return RevivePlayer(self, trace_ent);
		}
		if (!friendly)
			return false;

		if (trace_ent.regen >= 1 && trace_ent.health > 0)
		{
			sprint (self, PRINT_HIGH, trace_ent.netname);
			sprint (self, PRINT_HIGH, " is already healing.\n");
			return false;
		}
		if (trace_ent.health >= trace_ent.max_health)
		{
			sprint(self, PRINT_HIGH, trace_ent.netname);
			sprint(self, PRINT_HIGH, " isn't injured.\n");
			return false;
		}

		sprint (trace_ent, PRINT_HIGH, self.netname);
		sprint (trace_ent, PRINT_HIGH, " used a ");
		sprint (trace_ent, PRINT_HIGH, x);
		sprint (trace_ent, PRINT_HIGH, " to heal you.\n");

		trace_ent.regen = heal;
		trace_ent.health = trace_ent.health + heal;
		stuffcmd (trace_ent, "v_cshift 0 0 0 0\n");
		sound (self, CHAN_BODY, "items/r_item2.wav", 1, ATTN_NORM);
		return true;
	}
	else if (self.health < self.max_health && self.regen == 0)
	{
		sprint (self, PRINT_HIGH, "you heal yourself with a ");
		sprint (self, PRINT_HIGH, x);
		sprint (self, PRINT_HIGH, ".\n");

		sound (self, CHAN_BODY, "items/r_item2.wav", 1, ATTN_NORM);
		self.health = self.health + heal;
		self.regen = heal;
		return true;
	}

	return false;
};

float(entity x, float slotno) DecreaseDestroySlotOther;

void () UseStimpack =
{
	local float x, found;
	local vector source;
	local entity thing, te;


	if (self.attack_finished > time)
		return;

	te = findradius (self.origin, 100);
	while (te)
	{

		if (found == 0 && te.classname == "player" && te.deadflag)
		{

			x = SlotOfItem(self, IID_CHEM_STIMPACK);
			if (x == 0)
			{
				sprint(self, PRINT_MEDIUM, "you don't have a stimpack\n");
				return;
			}

			DecreaseDestroySlot(x);//take away a stimpack
			sound (trace_ent, CHAN_BODY, "items/r_item2.wav", 1, ATTN_NORM);
			RevivePlayer(self, te);
			found = 1;
		}

		te = te.chain;
	}

	if (found == 1)
		return;

	self.attack_finished = time + 1;
	self.rtime = time + 1;

	makevectors (self.v_angle);
	source = self.origin + '0 0 0';
	traceline (source, source + v_forward*64, FALSE, self);

	if (trace_fraction == 1.0)//nobody in front of user, so heal self
	{
		x = SlotOfItem(self, IID_CHEM_STIMPACK);
		if (x == 0)
		{
			sprint(self, PRINT_MEDIUM, "you don't have a stimpack\n");
			return;
		}

		if (self.regen > 0)
			sprint(self, PRINT_MEDIUM, "you are already affected by a stimpack\n");
		else if (self.health >= self.max_health)
			sprint(self, PRINT_MEDIUM, "you are not injured.\n");
		else if (self.health < self.max_health)
		{
			DecreaseDestroySlot(x);//take away a stimpack

			sound (self, CHAN_BODY, "items/r_item2.wav", 1, ATTN_NORM);
			sprint(self, PRINT_MEDIUM, "you heal yourself with a stimpack\n");

			thing = findradius(self.origin, 400);
			while (thing)
			{
				if (thing.classname == "player" && thing.health < self.health)
					self.score = self.score - 50;        
              
				thing = thing.chain;      
			}

			self.health = self.health + 30 + (self.skill_doctor);
			self.regen = self.regen + (5 + (self.skill_doctor/2));
		}
	}
	else if (trace_ent.classname == "player")//healing another team-mate
	{
		if (trace_ent.regen >= 1 && trace_ent.health > 0)
		{
			sprint(self, PRINT_MEDIUM, trace_ent.netname);
			sprint(self, PRINT_MEDIUM, " is already healing from a stimpack.\n");
		}
		else if (trace_ent.health >= trace_ent.max_health)
		{
			sprint(self, PRINT_MEDIUM, trace_ent.netname);
			sprint(self, PRINT_MEDIUM, " is not injured.\n");
		}
		else if (trace_ent.health < trace_ent.max_health)
		{
			x = SlotOfItem(self, IID_CHEM_STIMPACK);
			if (x > 0)
				DecreaseDestroySlot(x);//take away a stimpack
			else if (x == 0)
			{
				sprint(self, PRINT_MEDIUM, "no stims! searching ");
				sprint(self, PRINT_MEDIUM, trace_ent.netname);
				sprint(self, PRINT_MEDIUM, "'s body...\n");

				x = SlotOfItem(trace_ent, IID_CHEM_STIMPACK);
				if (x == 0)
				{
					sprint(self, PRINT_MEDIUM, trace_ent.netname);
					sprint(self, PRINT_MEDIUM, " had no stims!\n");
				}
				if (x >= 1)
				{
					self.score = self.score + 5;
					sprint(self, PRINT_MEDIUM, trace_ent.netname);
					sprint(self, PRINT_MEDIUM, " had a stim. using it to heal.\n");
					DecreaseDestroySlotOther(trace_ent, x);//take away a stimpack
					return;
				}
				x = SlotOfItem(trace_ent, IID_CHEM_MEDICALBAG);
				if (x == 0)
				{
					sprint(self, PRINT_MEDIUM, trace_ent.netname);
					sprint(self, PRINT_MEDIUM, " had no medical bag!\n");
				}
				if (x >= 1)
				{
					self.score = self.score + 5;
					sprint(self, PRINT_MEDIUM, trace_ent.netname);
					sprint(self, PRINT_MEDIUM, " had a stim. using it to heal.\n");
					DecreaseDestroySlotOther(trace_ent, x);//take away a stimpack
					return;
				}
			}

			if (trace_ent.deadflag)
			{
				sound (trace_ent, CHAN_BODY, "items/r_item2.wav", 1, ATTN_NORM);
				RevivePlayer(self, trace_ent);
			}
			else
			{
				sound (trace_ent, CHAN_BODY, "items/r_item1.wav", 1, ATTN_NORM);
				self.score = self.score + 50;
				sprint(trace_ent, PRINT_MEDIUM, self.netname);
				sprint(trace_ent, PRINT_MEDIUM, " heals you with a stimpack\n");
				sprint(self, PRINT_MEDIUM, "you heal ");
				sprint(self, PRINT_MEDIUM, trace_ent.netname);
				sprint(self, PRINT_MEDIUM, " with a stimpack\n");

				trace_ent.health = trace_ent.health + 30 + (self.skill_doctor);
				trace_ent.regen = trace_ent.regen + (3 + (self.skill_doctor/2));
			}
		}
	}
};

void () UseSuperStim =
{
	local float x;
	local vector source;


	if (self.attack_finished > time)
		return;

	self.attack_finished = time + 1;
	self.rtime = time + 1;

	makevectors (self.v_angle);
	source = self.origin + '0 0 16';
	traceline (source, source + v_forward*64, FALSE, self);

	if (trace_fraction == 1.0)//nobody in front of user, so heal self
	{
		x = SlotOfItem(self, IID_CHEM_SUPERSTIM);
		if (x == 0)
		{
			sprint(self, PRINT_MEDIUM, "you don't have a superstim\n");
			return;
		}

		if (self.regen > 0)
			sprint(self, PRINT_MEDIUM, "you are already affected by a healing chem\n");
		else if (self.health >= self.max_health)
			sprint(self, PRINT_MEDIUM, "you are not injured.\n");
		else if (self.health < self.max_health)
		{
			local entity thing;
			thing = findradius(self.origin, 400);
			while (thing)
			{
				if (thing.classname == "player" && (thing.health+20) < self.health)
					self.score = self.score - 50;        
              
				thing = thing.chain;      
			}

			DecreaseDestroySlot(x);//take away a stimpack
			sound (self, CHAN_BODY, "items/r_item2.wav", 1, ATTN_NORM);
			sprint(self, PRINT_MEDIUM, "you heal yourself with a superstim\n");
			self.xregen = 100;
		}
	}
	else if (trace_ent.classname == "player")//healing another team-mate
	{
		if (trace_ent.regen >= 1 && trace_ent.health > 0)
		{
			sprint(self, PRINT_MEDIUM, trace_ent.netname);
			sprint(self, PRINT_MEDIUM, " is already healing from a healing chem.\n");
		}
		else if (trace_ent.health >= trace_ent.max_health)
		{
			sprint(self, PRINT_MEDIUM, trace_ent.netname);
			sprint(self, PRINT_MEDIUM, " is not injured.\n");
		}
		else if (trace_ent.health < trace_ent.max_health)
		{
			x = SlotOfItem(self, IID_CHEM_STIMPACK);
			if (x > 0)
				DecreaseDestroySlot(x);//take away a stimpack
			else if (x == 0)
			{
				sprint(self, PRINT_MEDIUM, "no stims! searching ");
				sprint(self, PRINT_MEDIUM, trace_ent.netname);
				sprint(self, PRINT_MEDIUM, "'s body...\n");

				x = SlotOfItem(trace_ent, IID_CHEM_SUPERSTIM);
				if (x == 0)
				{
					sprint(self, PRINT_MEDIUM, trace_ent.netname);
					sprint(self, PRINT_MEDIUM, " had no stims!\n");
					return;
				}
				else
				{
					sprint(self, PRINT_MEDIUM, trace_ent.netname);
					sprint(self, PRINT_MEDIUM, " had a superstim. using it to heal.\n");
					DecreaseDestroySlotOther(trace_ent, x);//take away a stimpack
				}
			}

			if (trace_ent.deadflag)
			{
				sound (trace_ent, CHAN_BODY, "items/r_item2.wav", 1, ATTN_NORM);
				RevivePlayer(self, trace_ent);
			}
			else
			{
				sound (trace_ent, CHAN_BODY, "items/r_item1.wav", 1, ATTN_NORM);

				self.score = self.score + 50;
				sprint(trace_ent, PRINT_MEDIUM, self.netname);
				sprint(trace_ent, PRINT_MEDIUM, " heals you with a superstim!\n");
				sprint(self, PRINT_MEDIUM, "you heal ");
				sprint(self, PRINT_MEDIUM, trace_ent.netname);
				sprint(self, PRINT_MEDIUM, " with a superstim\n");

				trace_ent.xregen = 100;
			}
		}
	}
};

void () UseMedicalBag =
{
	local float x;
	local vector source;


	if (self.attack_finished > time)
		return;

	self.attack_finished = time + 1;
	self.rtime = time + 1;

	makevectors (self.v_angle);
	source = self.origin + '0 0 0';
	traceline (source, source + v_forward*64, FALSE, self);

	if (trace_fraction == 1.0)//nobody in front of user, so heal self
	{
		x = SlotOfItem(self, IID_CHEM_MEDICALBAG);
		if (x == 0)
		{
			sprint(self, PRINT_MEDIUM, "you don't have a medkit\n");
			return;
		}

		if (self.regen > 0)
			sprint(self, PRINT_MEDIUM, "you are already affected by a healing chem\n");
		else if (self.health >= self.max_health)
			sprint(self, PRINT_MEDIUM, "you are not injured.\n");
		else if (self.health < self.max_health)
		{
			DecreaseDestroySlot(x);//take away a medkit

			local entity thing;
			thing = findradius(self.origin, 400);
			while (thing)
			{
				if (thing.classname == "player" && (thing.health+20) < self.health)
					self.score = self.score - 25;        
              
				thing = thing.chain;      
			}

			sound (self, CHAN_BODY, "items/r_item1.wav", 1, ATTN_NORM);
			self.health = self.health + 5 + (self.skill_doctor/2);
			self.regen = self.regen + 3 + (self.skill_doctor/2);
		}
	}
	else if (trace_ent.classname == "player")//healing another team-mate
	{
		if (trace_ent.regen >= 1 && trace_ent.health > 0)
		{
			sprint(self, PRINT_MEDIUM, trace_ent.netname);
			sprint(self, PRINT_MEDIUM, " is already healing from a healing chem.\n");
		}
		else if (trace_ent.health >= trace_ent.max_health)
		{
			sprint(self, PRINT_MEDIUM, trace_ent.netname);
			sprint(self, PRINT_MEDIUM, " is not injured.\n");
		}
		else if (trace_ent.health < trace_ent.max_health)
		{
			x = SlotOfItem(self, IID_CHEM_MEDICALBAG);
			if (x > 0)
				DecreaseDestroySlot(x);//take away a medkit
			else if (x == 0)
			{
				sprint(self, PRINT_MEDIUM, "no stims! searching ");
				sprint(self, PRINT_MEDIUM, trace_ent.netname);
				sprint(self, PRINT_MEDIUM, "'s body...\n");

				x = SlotOfItem(trace_ent, IID_CHEM_MEDICALBAG);
				if (x == 0)
				{
					sprint(self, PRINT_MEDIUM, trace_ent.netname);
					sprint(self, PRINT_MEDIUM, " had no medkit!\n");
					return;
				}
				else
				{
					sprint(self, PRINT_MEDIUM, trace_ent.netname);
					sprint(self, PRINT_MEDIUM, " had a medkit. using it to heal.\n");
					DecreaseDestroySlotOther(trace_ent, x);//take away a medkit
				}
			}

			if (trace_ent.deadflag)
			{
				sound (trace_ent, CHAN_BODY, "items/r_item2.wav", 1, ATTN_NORM);
				RevivePlayer(self, trace_ent);
			}
			else
			{
				sound (trace_ent, CHAN_BODY, "items/r_item1.wav", 1, ATTN_NORM);
				self.score = self.score + 1;
				trace_ent.health = trace_ent.health + 5 + (self.skill_doctor/2);
				trace_ent.regen = trace_ent.regen + 3 + (self.skill_doctor/2);
			}
		}
	}
};

void () DisplayMenu =
{
	local string menu;
	local entity ze;
	local float gotm, gotc, gots, gote;

	if (self.currentmenu == "none")
		return;

	if (self.currentmenu == "shop_list")
	{
		self.missionbrief = 4;
		menu = ShopString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "shop_trait")
	{
		menu = TraitString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "select_mission")
	{
		menu = MissionString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "confirm_depart")
	{
		menu = DepartString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "shop_perk1")
	{
		menu = PerkString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "shop_perk2")
	{
		menu = PerkString ();
		centerprint (self, menu);
	}

	if (self.currentmenu == "shop_armor")
	{
		menu = ArmorString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "shop_ammo")
	{
		if (world.map_obj != 4)
			menu = AmmoString ();
		else
			menu = AmmoString2 ();
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
	if (self.currentmenu == "shop_heavy")
	{
		menu = HeavyString ();
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
	if (self.currentmenu == "menu_lockpick")
	{
		menu = LockpickString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "menu_defuse")
	{
		menu = DefuseString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "display_enter_screen")
	{
		menu = EnterString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "display_wait_screen")
	{
		menu = WaitString ();
		centerprint (self, menu);
	}
	if (self.currentmenu == "shop_weapons")
	{
		menu = WeaponString ();
		centerprint (self, menu);
	}

	if (self.currentmenu == "gain_skill")
	{

		centerprint (self, "êchoose skill to gainë\n\nê1ë doctor     \nê2ë sneak      \nê3ë combat     \nê4ë science    \n");
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
		if (self.team == 1)
			centerprint (self, "êyou will respawn asë\n\nRanger - OK?\nê1ë Yes     \nê2ë No      \n");
		if (self.team == 2)
			centerprint (self, "êyou will respawn asë\n\nRaider - OK?\nê1ë Yes     \nê2ë No      \n");
	}
	if (self.currentmenu == "confirm_skill")
	{
		if (self.tclass == 1)
			centerprint (self, "êyour skill-set will beë\n\nfirst aid - OK?\nê1ë Yes     \nê2ë No      \n\nprimary:   first aid\nsecondary: buff allies\n");
		if (self.tclass == 2)
			centerprint (self, "êyour skill-set will beë\n\nstealth - OK?\nê1ë Yes     \nê2ë No      \n\nprimary:   stealth\nsecondary:  damage");
		if (self.tclass == 3)
			centerprint (self, "êyour skill-set will beë\n\ncombat - OK?\nê1ë Yes     \nê2ë No      \n\nprimary:   survival\nsecondary:  damage");
		if (self.tclass == 4)
			centerprint (self, "êyour skill-set will beë\n\nscience - OK?\nê1ë Yes     \nê2ë No      \n\nprimary:   create\nsecondary: bypass");
	}

	if (self.currentmenu == "menu_build")
	{
		menu = BuildString ();
		centerprint (self, menu);
	}
};


void () Special =
{

		Sneak();
};

void () hos_run1;
void () hos_stand1;
float (float iid) spawn_station;

.float gravity;


void () pickup_body =
{
	local float x, z, forward_okay, down_okay, fp;
	local vector dir, org;
	local entity te;

	makevectors(self.enemy.v_angle);
	traceline (self.enemy.origin, self.enemy.origin + v_forward*64, TRUE, self);
	org = trace_endpos - v_forward*24;

	self.movetype = MOVETYPE_TOSS;

	if (self.enemy.pickup == 1)
	{
		self.flags = self.flags - (self.flags & FL_ONGROUND);
		self.gravity = 0;
		dir = normalize(self.origin - (self.enemy.origin+'0 0 48'));
		self.angles_y = dir;
		self.velocity_z += 30;
		self.goalentity = self.enemy;
		self.velocity = dir  * -100;
		self.enemy.maxspeed = 100;
		self.nextthink = time + 0.01;
		return;
	}
	else if (self.enemy.pickup == 0)
	{
		self.velocity = '0 0 0';
		self.velocity_z = -250;
		self.gravity = 800;
		self.enemy = world;
		return;
	}
};

void () pickup_body2 =
{
	local float x, z;
	local vector dir;

	x = vlen(self.origin - self.enemy.origin);
	dir = normalize(self.origin - self.enemy.origin);

	self.movetype = MOVETYPE_STEP;

	z = self.origin_z - self.enemy.origin_z;

	if ((x >= 32 && x <= 96) && self.enemy.pickup == 1)
	{
		self.velocity = (dir * (-30));
		self.velocity_z = 1;
		self.enemy.maxspeed = 50;
		self.angles = dir;
		self.nextthink = time + 0.01;
	}
	else if (x > 96 && self.enemy.pickup == 1)
	{
		self.velocity = '0 0 0';
		self.enemy.pickup = 0;
		sprint(self.enemy, 2, "dropped body.\n");
	}
};

void () hos_face =
{
	local entity te;

	te = findradius (self.origin, 250);
	while (te)
	{
		if (te.classname == "player")
		{
			self.enemy = te;
			te.safezone = 1;
		}
		te = te.chain;
	}
			
	self.angles_y = vectoyaw(self.enemy.origin - self.origin);

	self.nextthink = time + 0.1;

	if (vlen(self.origin - self.enemy.origin) > 250)
		self.enemy = world;
};

void () ExitScreen =
{
	local vector vec, dir;
	local string ammoname, ammocountftos, rep;
	local float best, ammotype, ammocount, dot;
	local entity save;

	self.impulse = 0;

	if (self.class == 0)
		return;

	if (self.ghost == 1)
		return;

	if (self.team == 0)
		return;

	if (self.attack_finished > time)
		return;

	self.accept = 1;

	if (self.currentmenu != "menu_none")
		self.missionbrief = 0;

	makevectors (self.v_angle);
	traceline (self.origin, self.origin + v_forward*128, FALSE, self);

	if (self.pickup == 1)
	{
		self.pickup = 0;
		return;
	}
	if (trace_ent.classname == "body" && self.pickup == 0)
	{

			trace_ent.movetype = MOVETYPE_BOUNCE;
			dir = normalize(trace_ent.origin - self.origin);
			trace_ent.velocity = dir * 100;
			trace_ent.velocity_z = 100;
			trace_ent.avelocity_y = (random()* 30);
			trace_ent.think = pickup_body;
			trace_ent.enemy = self;
			trace_ent.nextthink = time + 0.01;
			self.pickup = 1;
			sound (self, CHAN_BODY, "player/land.wav", 1, ATTN_IDLE);
			sprint(self, 2, "grabbed body.\n");
			return;
	}
	else if (self.pickup == 1)
		self.pickup = 0;

	if (trace_ent.classname == "bomb")
	{
		if (trace_ent.picking == 1)
		{
			sprint(self, 2, "already being defused.\n");
			return;
		}
		if (trace_ent.picking == 2)
		{
			sprint(self, 2, "that bomb is disarmed\n");
			return;
		}
		if (trace_ent.picking == 3)
		{
			sprint(self, 2, "circuits are jammed.\n");
			return;
		}

		self.picking = 1;
		trace_ent.picking = 1;
		self.currentmenu = "menu_defuse";
		SpawnBomb(trace_ent, self);
		return;
	}

	if (trace_ent.classname == "brotherhood_guard")
	{

		if (random()<0.25)
			sound (self, CHAN_BODY, "enforcer/sight2.wav", 1, ATTN_NORM);
		else if (random()<0.25)
			sound (self, CHAN_BODY, "enforcer/sight2.wav", 1, ATTN_NORM);
		else if (random()<0.25)
			sound (self, CHAN_BODY, "enforcer/sight3.wav", 1, ATTN_NORM);
		else
			sound (self, CHAN_BODY, "enforcer/sight4.wav", 1, ATTN_NORM);

		sprint(self, 2, "guard: back off, grunts ain't allowed in here.\n");
		return;
	}

	if (trace_ent.classname == "brotherhood_doctor")
	{
		sprint(self, 2, "doctor: if you are hurt, lie down and rest\n");
		return;
	}

	if (trace_ent.classname == "brotherhood_soldier")
	{
		if (random()<0.12)
		{
			sprint(self, 2, "\n\n\n");
			sprint(self, 2, "make sure you buy ammo before going out\n");
			sprint(self, 2, "into the wastes, you'll use alot of it!\n");
		}
		else if (random()<0.12)
		{
			sprint(self, 2, "\n\n\n");
			sprint(self, 2, "have team mates whose skills cover up your\n");
			sprint(self, 2, "weaknesses. first-aid and science-oriented\n");
			sprint(self, 2, "team members are crucial to a team.\n");
		}
		else if (random()<0.12)
		{
			sprint(self, 2, "\n\n\n");
			sprint(self, 2, "those skilled in science & repair can make \n");
			sprint(self, 2, "make useful objects out of junk. this helps\n");
			sprint(self, 2, "improve your squad as a whole.\n");
		}
		else if (random()<0.12)
		{
			sprint(self, 2, "\n\n\n");
			sprint(self, 2, "jhp stands for jacketed hollow-point, which\n");
			sprint(self, 2, "means the ammo spreads for a great deal of \n");
			sprint(self, 2, "stopping power against unarmored foes.\n");
		}
		else if (random()<0.12)
		{
			sprint(self, 2, "\n\n\n");
			sprint(self, 2, "ap stands for armor-piercing, which means the \n");
			sprint(self, 2, "rounds will tear through armor like paper. use\n");
			sprint(self, 2, "such rounds when fighting heavily-armored foes\n");
		}
		else if (random()<0.12)
		{
			sprint(self, 2, "\n\n\n");
			sprint(self, 2, "you can improve your stealth level by being\n");
			sprint(self, 2, "quiet, hugging walls, ducking, silencers, &\n");
			sprint(self, 2, "wearing light armor.                       \n");
		}
		else if (random()<0.12)
		{
			sprint(self, 2, "\n\n\n");
			sprint(self, 2, "perks are useful abilities. make sure you have\n");
			sprint(self, 2, "perks that complement your skills and playing \n");
			sprint(self, 2, "style. some perks even go really well together\n");
		}
		else if (random()<0.12)
		{
			sprint(self, 2, "\n\n\n");
			sprint(self, 2, "proper positioning of all squad members is key\n");
			sprint(self, 2, "to success in the battles you will face. make \n");
			sprint(self, 2, "sure to duck, prone and sneak when necessary  \n");
		}
		else
		{
			sprint(self, 2, "\n\n\n");
			sprint(self, 2, "if you are trained in first aid, do not forget\n");
			sprint(self, 2, "about your chems. psycho can provide temporary\n");
			sprint(self, 2, "armor bonus, and adrenaline lets you run fast.\n");
		}
		return;
	}

	if (trace_ent.classname == "brotherhood_sergeant")
	{
		sprint(self, 2, "sergeant: ready for your next mission?\n");
		self.currentmenu = "select_mission";
		return;
	}
	if (trace_ent.classname == "func_tank")
	{
		sprint(self, 2, "get ready to gear up!\n");
		self.currentmenu = "confirm_depart";
		return;
	}
	if (trace_ent.classname == "treasure_chest")
	{
		if (trace_ent.picking == 1)
		{
			sprint(self, 2, "already being picked.\n");
			return;
		}
		if (trace_ent.picking == 2)
		{
			sprint(self, 2, "that chest is empty!\n");
			return;
		}
		if (trace_ent.picking == 3)
		{
			sprint(self, 2, "lock is jammed.\n");
			return;
		}
		if (self.skill_sneak == 0)
		{
			sound (self, CHAN_BODY, "items/locked.wav", 1, ATTN_NORM);
			sprint(self, 2, "locked!\n");
			return;
		}

		self.attack_finished = time + 0.5;
		self.rtime = time + 4;
		self.picking = 1;
		trace_ent.picking = 1;
		self.currentmenu = "menu_lockpick";
		SpawnOpenChest(trace_ent, self);
		return;
	}

	if (trace_ent.classname == "rhostage" && trace_ent.health > 0 && trace_ent.rescued == 0)
	{
		if (self.team != 1)
			return;

		if (self.currentmenu != "none")
			return;

		bprint (2, "civilian has been located.\n");
		sound (trace_ent, CHAN_BODY, "misc/rescued.wav", 1, ATTN_NONE);
		rescue = rescue + 1;
		//remove(trace_ent);
		spawn_excla(trace_ent, 3000);
		trace_ent.rescued = 1;
		trace_ent.think = hos_face;
		trace_ent.nextthink = time + 1;

		return;
	}
	if (trace_ent.classname == "monster" && trace_ent.health > 0)
	{
		if (self.sneak == 0)
			return;

		if (self.class != 2)
			return;

		if (self.currentmenu != "none")
			return;


		if (random()*100 <= 75)
		{
			if (trace_ent.ammo_shells == 0)
			{
				sprint(self, 2, "nothing to steal!\n");
				return;
			}
			sprint (self, 2, "success! stole ");

			ammotype = GetRandomAmmo();
			ammocount = GetAmmoCount(ammotype);
			ammocount = ceil(ammocount);
			ammoname = GetItemName(ammotype);
			ammocountftos = ftos(ammocount);
			sprint (self, 2, ammoname);
			sprint (self, 2, "(");
			sprint (self, 2, ammocountftos);
			sprint (self, 2, ") ");
			TryGiveStackable(self, ammotype, ammocount);

			sound (self, CHAN_WEAPON, "weapons/ax1", 1, ATTN_NORM);
			trace_ent.ammo_shells = 0;
			return;
		}
		else
		{
			sound (self, CHAN_WEAPON, "weapons/ax1", 1, ATTN_NORM);
			sprint (self, 2, "pilfer failed...\n");
			return;
		}
	}
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

void () disappear =
{
	local entity te;
	local float nearby;
	//this allows an assassin to vanish, making monsters aggro
	//those who are visible

	if (!getperk(3))
		return;

	te = findradius(self.origin, 500);
	while (te)
	{
		if (te.classname == "player" && te.sneak == 0 && te != self)
			nearby = 1;
			
		te = te.chain;
	}

	if (nearby == 1)
	{
		te = findradius(self.origin, 1000);
		while (te)
		{
			if (te.enemy == self)
				te.enemy = world;
			
			te = te.chain;
		}
	}
};

void () Sneak =
{
	local float w;

	w = weightx();

	if (self.sneak >= 1)
	{
		sound (self, CHAN_BODY, "items/r_tele4.wav", 1, ATTN_NORM);
		setmodel (self, "progs/guy.mdl");
		self.sneak = 0;
		return;
	}
	if (self.sneak == 0)
	{
		disappear();
		sound (self, CHAN_BODY, "items/inv1.wav", 1, ATTN_NORM);
		self.sneak = 1;
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
	if ((self.buildtype == IID_BUILD_MRAMMO))
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
	if ((self.buildtype == IID_BUILD_SHIELDGEN))
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
	if ((self.buildtype == IID_BUILD_AUTODOC))
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
#define chemcount attack_finished
	local entity te;
	local float r;

	self.nextthink = time + 2;

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

	if (self.buildtype == IID_BUILD_SHIELDGEN)//barricade
	{
		sound (self, CHAN_BODY, "items/protect2.wav", 1, ATTN_NORM);

		te = findradius (self.origin, 256);
		while (te)
		{
			if (te.classname == "player" && te.team == self.team && !te.deadflag)
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
	if (self.buildtype == IID_BUILD_AUTODOC)//autodoc
	{
		te = findradius (self.origin, 70);
		while (te)
		{
			if (te.classname == "player" && te.team == self.team && !te.deadflag)
			{
				if (self.chemcount <= 0)
				{
					sound (self, CHAN_BODY, "misc/item1.wav", TRUE, ATTN_NORM);
					sprint (te, 2, "the autodoc is out of medical supplies.\n");
					return;
				}
				if (te.deadflag)
					RevivePlayer(self, te);
				else if (te.health < te.max_health)
				{
					sound (self, CHAN_BODY, "items/r_item2.wav", 1, ATTN_NORM);
					sprint (te, PRINT_MEDIUM, "the auto-doc heals you for 3 health.\n");
					te.health = te.health + 3;
					self.chemcount = self.chemcount - 1;
					if (te.health > te.max_health)
						te.health = te.max_health;
				}
				if (random() < 0.001)
				{
					r = random()*6;
					if (r < 1)
						sprint(te, PRINT_HIGH, "the auto-doc heals you of AIDS\n");
					else if (r < 2)
						sprint(te, PRINT_HIGH, "the auto-doc heals you of Genital Herpes\n");
					else if (r < 3)
						sprint(te, PRINT_HIGH, "the auto-doc heals you of Gonorrhea\n");
					else if (r < 4)
						sprint(te, PRINT_HIGH, "the auto-doc heals you of Syphillis\n");
					else if (r < 5)
						sprint(te, PRINT_HIGH, "the auto-doc rids you of Crabs\n");
					else if (r < 6)
						sprint(te, PRINT_HIGH, "the auto-doc heals you of Chlamydia\n");
				}
			}
			te = te.chain;
		}
	}

	if (self.buildtype == IID_BUILD_MRAMMO)//mr. ammo
	{
		te = findradius (self.origin, 60);
		while (te)
		{
			if (te.classname == "player" && te.team == self.team && !te.deadflag)
			{
				if (self.chemcount <= 0)
				{
					sound (self, CHAN_BODY, "misc/item1.wav", TRUE, ATTN_NORM);
					sprint (te, 2, "this mr.ammo is out of ammunition.\n");
					return;
				}

				local float x;

				x = 300;

				local float curweap;
				local float ammotype;
				local float ammocount;
				local float ammoslot;
				curweap = ToIID(ItemInSlot(te, te.current_slot));
				ammotype = WeaponAmmoType(curweap);
				if (ammotype)
				{
					ammocount = TotalQuantity(te, ammotype);
					x = x - ammocount;
				}
				else
					x = 0;
				if (x > 0)
				{
					ammocount = WeaponMagQuant(curweap);
					if (x > ammocount/2)
						x = ammocount/2;

					ammoslot = SlotOfItem(te, ammotype);
					if (!ammoslot)
						ammoslot = FindEmptySlot(te);
					if (ammoslot)
						SetItemSlot(te, ammoslot, SlotVal(ammotype, x + ToStatus(ItemInSlot(te, ammoslot))));

					sprint (te, PRINT_MEDIUM, ftos(ceil(x)), " ammo was received from the mr.ammo.\n");
/*
					else if (r < 2)
						sprint (te, PRINT_MEDIUM, ftos(ceil(x)), " potatos were received from the mr.ammo.\n");
					else if (r < 3)
						sprint (te, PRINT_MEDIUM, ftos(ceil(x)), " ammo was given by the mr.ammo.\n");
					else if (r < 4)
						sprint (te, PRINT_MEDIUM, ftos(ceil(x)), " ammo was stolen from the mr.ammo.\n");
					else if (r < 5)
						sprint (te, PRINT_MEDIUM, ftos(ceil(x)), " ammo was extracted from the mr.ammo.\n");
					else
						sprint (te, PRINT_MEDIUM, ftos(ceil(x)), " ammo was given by the mr.ammo, along with an std\n");*/

					self.chemcount = self.chemcount - 1;
				}
			}
			te = te.chain;
		}
	}
};

void () Bar_Think =
{
	local float dot1;

	self.flags = self.flags;

	self.frame = floor (((self.owner.health / self.owner.max_health) * TE_WIZSPIKE));

	if (self.owner.position == 0)
		setorigin (self, (self.owner.origin + '0 0 40'));
	else if (self.owner.position == 1)
		setorigin (self, (self.owner.origin + '0 0 32'));
	else
		setorigin (self, (self.owner.origin + '0 0 24'));

	self.nextthink = (time + 0.01);
	setmodel (self, "progs/hbar.spr");
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
	hologram.scale = 0.5;
};

void() FinishTurret;
float (float iid) spawn_station =
{
	local entity oself;
	local entity te;
	local vector org;

	if (iid == IID_BUILD_ROBOFANG)
	{
		te = find (world, classname, "robofang");
		while (te)
		{
			if (te.track == self && te.buildtype == IID_BUILD_ROBOFANG)
			{
				makevectors (self.v_angle);
				setorigin(te, self.origin + v_forward*32);
				return false;
			}
			te = find (te, classname, "robofang");
		}
	}

	te = find (world, classname, "station");
	while (te)
	{
		if (te.track == self && te.buildtype == iid)
		{
			sprint (self, 2, "already have one.\n");
			return false;
		}
		te = find (te, classname, "station");
	}
	if (iid == IID_BUILD_ROBOFANG)
	{
			te = findradius (self.origin, 128);
			while (te)
			{
				if (te != self && te.classname == "player" && te.health > 0)
				{
					sprint(self, PRINT_HIGH, "not with other players nearby.\n");
					return false;
				}
				te = te.chain;
			}
	}

	makevectors (self.v_angle);
	org = ((self.origin + (v_forward * IT_LIGHTNING)) + (v_up * EF_FLAG2));
	if ((pointcontents ((org + '0 20 0')) != CONTENT_EMPTY))
	{
		sprint (self, 2, "can't build there.\n");
		return false;
	}
	if ((pointcontents ((org + '0 -20 0')) != CONTENT_EMPTY))
	{
		sprint (self, 2, "can't build there.\n");
		return false;
	}
	if ((pointcontents ((org + '20 0 0')) != CONTENT_EMPTY))
	{
		sprint (self, 2, "can't build there.\n");
		return false;
	}
	if ((pointcontents ((org + '-20 0 0')) != CONTENT_EMPTY))
	{
		sprint (self, 2, "can't build there.\n");
		return false;
	}
	if ((pointcontents ((org + '0 0 50')) != CONTENT_EMPTY))
	{
		sprint (self, 2, "can't build there.\n");
		return false;
	}
	if ((pointcontents ((org + '0 0 -10')) != CONTENT_EMPTY))
	{
		sprint (self, 2, "can't build there.\n");
		return false;
	}
	self.impulse = 0;
	te = findradius (org, 40);
	while (te)
	{
		if (te.classname == "spawn1")
		{
			sprint (self, 2, "can't build at spawn.\n");
			return false;
		}
		if (te.classname == "spawn2")
		{
			sprint (self, 2, "can't build at spawn.\n");
			return false;
		}
		if (te.classname == "ghoul")
		{
			sprint (self, 2, "somethings in the way.\n");
			return false;
		}
		if (((te.classname == "player") && (te.health > 0)))
		{
			sprint (self, 2, "can't build on players.\n");
			return false;
		}
		if (((te.classname == "station") && (te.health > 0)))
		{
			sprint (self, 2, "can't build on other stations.\n");
			return false;
		}
		te = te.chain;
	}

	oself = self;
	self = spawn ();
	self.team = oself.team;
	self.track = oself;

	spawn_dot (self);
	self.origin = org;
	self.takedamage = DAMAGE_YES;
	self.solid = SOLID_SLIDEBOX;
	self.movetype = MOVETYPE_TOSS;
	setsize (self, '-16 -16 0', '16 16 40');
	self.health = 30;
	self.max_health = 300;
	self.th_die = station_die;
	setmodel (self, "progs/station.mdl");
	self.classname = "station";
	self.think = station_think;
	self.helmet = 2;
	self.buildtype = iid;
	self.aflag = true;

	self.frame = 4;

	self.chemcount = 75;

	if (self.buildtype == IID_BUILD_MRAMMO)
		self.netname = "mr. ammo";
	if (self.buildtype == IID_BUILD_SHIELDGEN)
		self.netname = "shield generator";
	if (self.buildtype == IID_BUILD_AUTODOC)
		self.netname = "autodoc";
	if (self.buildtype == IID_BUILD_ROBOFANG)
	{
		setsize(self, '-24 -24 -24', '24 24 24');
		self.netname = "robofang";
		setmodel (self, "progs/dog.mdl");
	}
	if (self.buildtype == IID_BUILD_TTURRET || self.buildtype == IID_BUILD_RTURRET || self.buildtype == IID_BUILD_GTURRET)
	{
		FinishTurret();
	}

	self = oself;

	return true;
};

float() FireToolkit =
{
	local float rr;

	makevectors (self.v_angle);
	traceline (self.origin, self.origin + v_forward*64, FALSE, self);

	//track is used for the owning player (without using the .owner field)
	if (trace_ent.track == self && trace_ent.classname == "station" && trace_ent.health > 0)
	{
		if (trace_ent.health >= trace_ent.max_health)	//already built - don't whack it!
			return true;

		//play a random build sound
		rr = random ();
		if (rr < 0.5)
			sound (self, CHAN_WEAPON, "items/lockpick1.wav", TRUE, ATTN_NORM);
		else
			sound (self, CHAN_WEAPON, "items/lockpick2.wav", TRUE, ATTN_NORM);


		if (!trace_ent.nextthink)
			if (trace_ent.health > 200)
				trace_ent.frame = 5;
		self.attack_finished = time + 0.1 + (random () * 0.1);
		trace_ent.health = trace_ent.health + 6;
		

		if (trace_ent.health >= trace_ent.max_health)
		{	//it's built!
			trace_ent.health = trace_ent.max_health;
			if (!trace_ent.aflag)
				return true;	//already complete
			trace_ent.aflag = false;

			trace_ent.nextthink = time + 1;
			sprint (self, PRINT_MEDIUM, "BUILDING COMPLETED.\n");


			if (trace_ent.buildtype == IID_BUILD_ROBOFANG)
			{
				trace_ent.solid = SOLID_NOT;
				trace_ent.think = SUB_Remove;
				trace_ent.nextthink = time;
				makevectors (self.v_angle);
sprint(self, PRINT_HIGH, "Robofang appears to have malfunctioned\n");
			}
			if (trace_ent.buildtype == IID_BUILD_AUTODOC)
				trace_ent.frame = 1;
			if (trace_ent.buildtype == IID_BUILD_MRAMMO)
				trace_ent.frame = 0;
			if (trace_ent.buildtype == IID_BUILD_SHIELDGEN)
				trace_ent.frame = 2;
		}
		return true;
	}
	return false;
};


void () shop_face =
{
	local entity te;

	te = findradius (self.origin, 250);
	while (te)
	{
		if (te.classname == "player")
		{
			self.enemy = te;
			te.safezone = 1;
			te.xslot1 = self.xslot1;
			te.xslot2 = self.xslot2;
			te.xslot3 = self.xslot3;
			te.xslot4 = self.xslot4;
			te.xslot5 = self.xslot5;
			te.xslot6 = self.xslot6;
			te.xslot7 = self.xslot7;
			te.xslot8 = self.xslot8;
			te.xslot9 = self.xslot9;
			te.xslot10 = self.xslot10;
			te.xslot11 = self.xslot11;
			te.xslot12 = self.xslot12;
			te.xslot13 = self.xslot13;
			te.xslot14 = self.xslot14;
			te.xslot15 = self.xslot15;
			te.xslot16 = self.xslot16;
			te.xslot17 = self.xslot17;
			te.xslot18 = self.xslot18;
			te.xslot19 = self.xslot19;
			te.xslot20 = self.xslot20;
		}
		te = te.chain;
	}
			
	self.angles_y = vectoyaw(self.enemy.origin - self.origin);

	self.nextthink = time + 0.1;

	if (vlen(self.origin - self.enemy.origin) > 500)
		self.enemy = world;
};

void () BuyMenu =
{
	local float is_shop;
	local entity te;
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
			{
				te.enemy = self;
				te.think = shop_face;
				te.nextthink = time + 0.5;
				is_shop = 1;
			}
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
		self.missionbrief = 4;
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
	r1 = (self.score);

	x = ftos (r1);
	sprint (self, 2, x);

	sprint (self, PRINT_HIGH, "\nêSpeed     ë ");
	x = ftos (self.maxspeed);
	sprint (self, 2, x);
	sprint (self, PRINT_HIGH, "/300\n");
	sprint (self, PRINT_HIGH, "êMoney     ë ");

	x = ftos (self.ammo_shells);
	sprint (self, 2, x);
	sprint (self, PRINT_HIGH, "$\n");

/*
	sprint (self, PRINT_HIGH, "\nêArmor     ë ");
	x = GetArmorName();
	sprint (self, 2, x);
	sprint (self, 2, "  (");
	x = ftos (self.armor_weight);
	sprint (self, 2, x);
	sprint (self, 2, ")\n");
*/
	sprint (self, PRINT_HIGH, "êProtectiveë ");
	x = GetProtectName();
	sprint (self, 2, x);
	sprint (self, 2, "\n");
/*
	sprint (self, PRINT_HIGH, "êChem      ë ");
	x = GetChemName();
	sprint (self, 2, x);
	x = ftos (self.chemcount);
	sprint (self, 2, "[");
	sprint (self, 2, x);
	sprint (self, 2, "]");
	sprint (self, 2, "\n");
*/
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
/*
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
*/
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
/*
	if (self.equipment == 7 && self.equipment_slot == 0)
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
*/
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

void (vector s_aim, float dam, float tmp, float ran) W_FireBuckshotSpread1 =
{
	local vector source;
	local vector targ;
	local vector org;
	local float zdif;
	local float ydif;
	local float xdif;
	local float is_headshot;

	makevectors (self.v_angle);

	if (self.position == 0)
		source = (self.origin + '0 0 20');
	if (self.position == 1)
		source = (self.origin + '0 0 4');
	if (self.position == 2)
		source = (self.origin + '0 0 -12');

	targ = ((((((s_aim + ((v_right * random ()) * tmp)) - ((v_right * random ()) * tmp)) + ((v_up * random ()) * tmp)) - ((v_up * random ()) * tmp)) + (((v_up * random ()) * tmp) * 0.5)) + (v_forward * ran));

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

		if (trace_ent.solid != SOLID_BSP && trace_ent.classname != "car" && (trace_ent.islot3 == 0 || self.critical == 3))
			SpawnBlood (org, 1);
		else if (trace_ent.solid != SOLID_BSP && trace_ent.classname != "car" && trace_ent.islot3 > 0)
			SpawnNonBlood (org, 1);
		else if (trace_ent.solid == SOLID_BSP || trace_ent.classname == "car")
			SpawnWood (trace_ent, org, 1);

		zdif = (org_z - trace_ent.origin_z);
		ydif = (org_y - trace_ent.origin_y);
		xdif = (org_x - trace_ent.origin_x);
		is_headshot = 0;
		if (((ydif >= CONTENT_SLIME) && (ydif <= WEAPON_SPIKES)))
		{
			is_headshot = 1;
		}
		if (((xdif >= CONTENT_SLIME) && (xdif <= WEAPON_SPIKES)))
		{
			is_headshot = 1;
		}
		if (((is_headshot == 1) && (zdif >= ((trace_ent.size_z / 2) * 0.8))))
		{
			if (self.attack > 2)
				dam = (dam * 0.4);

			self.critical = 3;
		}

		dam = dam + random()*dam;
		T_Damage (trace_ent, self, self, dam);
		loud_noise(5);//hit flesh
		self.critical = 0;
	}
	else
	{
		loud_noise(15);//richochet
		bullet_hole (org);
	}
};

void (float rec, float number, float dam, float spread, float ran, float auto, string snd) W_FireShotgun =
{
	local vector dir;
	local vector p_aim;
	local float var1;
	local float var2;
	local float var3;
	local float var4;
	local float weap;

	if (getperk(5))
	{
		if (self.steadyaim == 0)
			dam = dam + self.skill_doctor/3;
	}

	weap = ToIID(self.(SlotField(self.current_slot)));

	var1 = ((random () * 12) * (self.recoil));
	var2 = ((random () * 12) * (self.recoil));
	var3 = ((random () * 12) * (self.recoil));
	var4 = ((random () * 12) * (self.recoil));

	if (self.position == 0)
		player_single1 ();
	else
		player_single1_s ();

	if (self.steadyaim == 0)
	{
		var1 = (8 + (random () * 6) * (self.recoil));
		var2 = (8 + (random () * 6) * (self.recoil));
		var3 = (8 + (random () * 6) * (self.recoil));
		var4 = (8 + (random () * 6) * (self.recoil));
	}

	stuffcmd (self, "+lookup\n");
	stuffcmd (self, "wait\n");
	stuffcmd (self, "-lookup\n");

	sound (self, CHAN_WEAPON, snd, 1.5, ATTN_NORM);

	//only silenced weapons do not break stealth
	if (self.sneak > 0)
	{
		self.equipment_slot = 0;
		sprint(self, PRINT_HIGH, "ç Uncloaked ç\n");
		setmodel (self, "progs/guy.mdl");
		self.sneak = 0;
		sound (self, CHAN_BODY, "items/inv1.wav", 1, ATTN_NORM);
	}


	loud_noise(40);

	p_aim = (((((self.origin + '0 0 20') + (v_right * var1)) - (v_right * var2)) + (v_up * var3)) - (v_up * var4));

	msg_entity = self;
	WriteByte (MSG_ONE, SVC_BIGKICK);
	DropAmmo ();
	self.attack = (self.attack + 1);
	self.recoil = (self.recoil + 8*rec);

	if (self.recoil >= 30)
		self.recoil = 30;

	if (auto == 0)
	{
		self.attack_finished = (time + 0.5);
		stuffcmd (self, "-attack\n");
	}
	if (auto == 1)
	{
		self.attack_finished = (time + 0.25);
		stuffcmd (self, "-attack\n");
	}
	if (auto == 2)
		self.attack_finished = (time + 0.2);

	dir = aim (self, 10000);
	if ((number == MULTICAST_PVS_R))
	{
		W_FireBuckshotSpread1 (p_aim, dam, 30, ran);
		W_FireBuckshotSpread1 ((p_aim + (v_right * spread)), dam, 30, ran);
		W_FireBuckshotSpread1 ((p_aim - (v_right * spread)), dam, 30, ran);
		W_FireBuckshotSpread1 ((p_aim + (v_up * spread)), dam, 30, ran);
		W_FireBuckshotSpread1 ((p_aim - (v_up * spread)), dam, 30, ran);
	}
	else
	{
		if ((number == AS_MELEE))
		{
			W_FireBuckshotSpread1 (p_aim, dam, spread, ran);
			W_FireBuckshotSpread1 (p_aim, dam, spread, ran);
			W_FireBuckshotSpread1 (p_aim, dam, spread, ran);
		}
	}
};




void (vector org) spawn_tracer =
{
	local entity tracer;
	local float x;
	local string y;

	x = 0;

	tracer = spawn();
	setmodel(tracer, "progs/handgren.mdl");
	setorigin(tracer, org);
	setsize(tracer, '0 0 0', '0 0 0');
	tracer.think = SUB_Remove;
	tracer.nextthink = time + 2;

	if (pointcontents (org) == CONTENT_SOLID)
		x = x + 1;
	
	y = ftos(x);
	bprint(2, y);
	bprint(2, "\n");
};


//the alien blaster
void () FireAlienBlaster =
{
	local float tmp;

	DropAmmo ();
	self.recoil = self.recoil + 8;

	msg_entity = self;
	WriteByte (MSG_ONE, SVC_SMALLKICK);

	newmis = spawn ();
	newmis.owner = self;
	newmis.movetype = MOVETYPE_FLYMISSILE;
	newmis.solid = SOLID_BBOX;
	newmis.effects = EF_DIMLIGHT;
	makevectors (self.v_angle);
	newmis.velocity = aim (self, 3000);
	newmis.velocity = (newmis.velocity * 3000);
	newmis.angles = vectoangles (newmis.velocity);

	newmis.nextthink = (time + IDLE3A);
	newmis.think = SUB_Remove;
	setmodel (newmis, "progs/plasma.mdl");
	tmp = ((30 + self.velocity_y) + self.velocity_x);


	{
		newmis.velocity = aim (self, 1700);
		newmis.velocity = newmis.velocity * 1700;
		newmis.angles = vectoangles (newmis.velocity);
		newmis.nextthink = time + 1.4;
		sound (self, CHAN_WEAPON, "weapons/blaster.wav", WEAPON_SHOTGUN, ATTN_NORM);
		newmis.touch = PlasmaBolt;
		setmodel (newmis, "progs/ray.mdl");
		tmp = 90 + self.velocity_y + self.velocity_x;
	}

	setsize (newmis, '0 0 0', '0 0 0');
	setorigin (newmis, self.origin + (v_right * WEAPON_BIG) + (v_forward * WEAPON_ROCKET) + '0 0 20');

	newmis.velocity = newmis.velocity + (v_right * random () * tmp) - (v_right * random () * tmp) + (v_up * random () * tmp) - (v_up * random () * tmp);

	self.attack_finished = time + 0.5;
};



void () radio_think =
{
	self.cnt = self.cnt - 1;
	setorigin (self, (self.owner.origin + '0 0 48'));
	self.nextthink = (time + 0.01);
	if (self.cnt <= 0)
		remove (self);
};

void (entity jeb, float length) spawn_excla =
{
	local entity hologram;

	hologram = spawn ();
	hologram.movetype = MOVETYPE_NONE;
	hologram.solid = SOLID_NOT;
	hologram.owner = jeb;
	setmodel (hologram, "progs/radio.spr");
	setorigin (hologram, jeb.origin + '0 0 48');
	setsize (hologram, VEC_ORIGIN, VEC_ORIGIN);
	hologram.angles = jeb.angles;
	hologram.colormap = jeb.colormap;
	hologram.cnt = 100*length;
	hologram.think = radio_think;
	hologram.nextthink = (time + 0.01);
};

void (entity jeb, float length) spawn_stop =
{
	local entity hologram;

	hologram = spawn ();
	hologram.movetype = MOVETYPE_NONE;
	hologram.solid = SOLID_NOT;
	hologram.owner = jeb;
	setmodel (hologram, "progs/stop.spr");
	setorigin (hologram, jeb.origin + '0 0 48');
	setsize (hologram, VEC_ORIGIN, VEC_ORIGIN);
	hologram.angles = jeb.angles;
	hologram.colormap = jeb.colormap;
	hologram.cnt = 100*length;
	hologram.think = radio_think;
	hologram.nextthink = (time + 0.01);
};

void (string color) ZoomInLarge =
{
	if (self.attack_finished > time)
		return;

	self.weaponmodel = "progs/v_scope1.mdl";
	self.zoom = 1;
	stuffcmd (self, "fov 30\n");

	if (color == "red")
		stuffcmd (self, "v_cshift 100 0 0 50\n");
	if (color == "pink")
		stuffcmd (self, "v_cshift 100 0 25 50\n");
	if (color == "blue")
		stuffcmd (self, "v_cshift 0 0 100 75\n");
	if (color == "green")
		stuffcmd (self, "v_cshift 0 100 0 100\n");
	if (color == "yellow")
		stuffcmd (self, "v_cshift 100 100 0 50\n");
	if (color == "purple")
		stuffcmd (self, "v_cshift 100 0 100 100\n");
};

void (string color) ZoomInSmall =
{
	if (self.attack_finished > time)
		return;

	self.weaponmodel = "progs/v_scope2.mdl";
	self.zoom = 1;
	stuffcmd (self, "fov 60\n");

	if (color == "red")
		stuffcmd (self, "v_cshift 100 0 0 50\n");
	if (color == "pink")
		stuffcmd (self, "v_cshift 100 0 25 50\n");
	if (color == "blue")
		stuffcmd (self, "v_cshift 0 0 100 75\n");
	if (color == "green")
		stuffcmd (self, "v_cshift 0 100 0 100\n");
	if (color == "yellow")
		stuffcmd (self, "v_cshift 100 100 0 50\n");
	if (color == "purple")
		stuffcmd (self, "v_cshift 100 0 100 100\n");
};

void () ZoomOut =
{
	SetWeaponModel();
	self.zoom = 0;
	stuffcmd (self, "fov 90\n");
	stuffcmd (self, "v_cshift 0 0 0 0\n");
};

void () ZoomIn =
{
	local float weap;

	weap = ToIID(self.(SlotField(self.current_slot)));

	if (self.zoom > 0)
	{
		ZoomOut ();
		return;
	}

	if (weap == IID_WP_DKS1)
		ZoomInLarge("purple");
	if (weap == IID_WP_ACR)
		ZoomInLarge("pink");
	if (weap == IID_WP_AK112_M)
		ZoomInSmall("pink");
	if (weap == IID_WP_DKS1_S)
		ZoomInLarge("purple");
	if (weap == IID_WP_FNFAL)
		ZoomInSmall("red");
	if (weap == IID_WP_G11)
		ZoomInSmall("blue");
	if (weap == IID_WP_MOONLIGHT)
		ZoomInSmall("green");

	if (weap == IID_WP_PIPERIFLE_SCOPE)
		ZoomInLarge("green");
	if (weap == IID_WP_PIPERIFLE_SCOPE_R)
		ZoomInLarge("green");
	if (weap == IID_WP_PIPERIFLE_SCOPE_S)
		ZoomInLarge("green");
	if (weap == IID_WP_PIPERIFLE_SCOPE_S_R)
		ZoomInLarge("green");
};


void () HostageReport =
{
	local float acount, dcount;
	local entity te;
	local string ah, dh, rh;
	local vector dir, org;

	hostages = 0;
	dead_hostage = 0;

	org = self.origin;

	te = find (world, classname, "rhostage");
	while (te)
	{
		if (te.health > 0)
			acount = acount + 1;

		if (te.health <= 0)
			dcount = dcount + 1;

		te = find (te, classname, "rhostage");
	}

	te = find (world, classname, "rhostage");
	while (te)
	{
		if (te.health > 0)
		{
			dir = normalize(te.origin - self.origin);
			traceline (self.origin, self.origin+dir*2000, FALSE, self);

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
		}
		te = find (te, classname, "rhostage");
	}


	hostages = acount;
	dead_hostage = dcount;

	ah = ftos(acount);
	dh = ftos(dcount);
	rh = ftos(rescue);

	sprint (self, PRINT_HIGH, "*** HOSTAGE REPORT ***\n");
	sprint (self, PRINT_HIGH, "rescued: ");
	sprint (self, PRINT_HIGH, rh);
	sprint (self, PRINT_HIGH, "\n");
	sprint (self, PRINT_HIGH, "alive:   ");
	sprint (self, PRINT_HIGH, ah);
	sprint (self, PRINT_HIGH, "\n");
	sprint (self, PRINT_HIGH, "dead:    ");
	sprint (self, PRINT_HIGH, dh);
	sprint (self, PRINT_HIGH, "\n");

	rh = ftos(supplies);
	sprint (self, PRINT_HIGH, "*** SUPPLY REPORT ***\n");
	sprint (self, PRINT_HIGH, "supplies: ");
	sprint (self, PRINT_HIGH, rh);
	sprint (self, PRINT_HIGH, "\n");
};


void () clone_think =
{
	self.model = self.owner.model;
	self.frame = self.owner.frame;
	self.nextthink = time + 0.01;
};

void () AddClone =
{
	local entity clone;

	clone = spawn ();
	setmodel(clone, "progs/guy.mdl");
	clone.owner = self;
	clone.movetype = MOVETYPE_BOUNCE;
	clone.solid = SOLID_NOT;
	clone.touch = SUB_Null;
	clone.owner = self;
	clone.think = clone_think;
	setsize(clone, '-16 -16 -24', '16 16 32');
	setorigin(clone, self.origin);
	clone.nextthink = time + 0.01;
	sprint(self, 2, "clone added.\n");
	self.health = 9000;
};

float (float x) find_percent =
{
	if (x == 1)
		return world.map_ent1_percent;
	if (x == 2)
		return world.map_ent2_percent;
	if (x == 3)
		return world.map_ent3_percent;
};

float(vector org) light_level =
{
	local vector light;
	local float brightness;

	light = getlight(self.origin);
	//brightness = light_x*0.3 + light_y*0.51 + light_z*0.2;

	brightness = light_x + light_y + light_z;

	return brightness;
};

void() show_light_level =
{
	local float x;
	local string y;

	x = light_level(self.origin);
	y = ftos(x);

	bprint(2, "light level: ");
	bprint(2, y);
	bprint(2, "\n");

	x = self.waterlevel;
	y = ftos(x);

	bprint(2, "water level: ");
	bprint(2, y);
	bprint(2, "\n");
};
	
void() AttachSilencer =
{
	local float weap, x, y;

	weap = ToIID(self.(SlotField(self.current_slot)));

	y = SlotOfItem(self, IID_EQUIP_SILENCER);

	if (weap == IID_WP_USP)
	{
		sprint(self, 2, "silencer attached.\n");
		DecreaseDestroySlot(y);

		if (self.current_slot == 1)
		{
			x = ToStatus(self.islot1);
			self.islot1 = 0;
			SetItemSlot(self, 1, SlotVal(IID_WP_USP_S, x));
			return;
		}
		if (self.current_slot == 2)
		{
			x = ToStatus(self.islot2);
			self.islot2 = 0;
			SetItemSlot(self, 2, SlotVal(IID_WP_USP_S, x));
			return;
		}
	}
	else if (weap == IID_WP_DKS1)
	{
		sprint(self, 2, "silencer attached.\n");
		DecreaseDestroySlot(y);

		if (self.current_slot == 1)
		{
			x = ToStatus(self.islot1);
			self.islot1 = 0;
			SetItemSlot(self, 1, SlotVal(IID_WP_DKS1_S, x));
			return;
		}
		if (self.current_slot == 2)
		{
			x = ToStatus(self.islot2);
			self.islot2 = 0;
			SetItemSlot(self, 2, SlotVal(IID_WP_DKS1_S, x));
			return;
		}
	}
	else if (weap == IID_WP_GLOCK)
	{
		sprint(self, 2, "silencer attached.\n");
		DecreaseDestroySlot(y);

		if (self.current_slot == 1)
		{
			x = ToStatus(self.islot1);
			self.islot1 = 0;
			SetItemSlot(self, 1, SlotVal(IID_WP_GLOCK_S, x));
			return;
		}
		if (self.current_slot == 2)
		{
			x = ToStatus(self.islot2);
			self.islot2 = 0;
			SetItemSlot(self, 2, SlotVal(IID_WP_GLOCK_S, x));
			return;
		}
	}
	else if (weap == IID_WP_MP9)
	{
		sprint(self, 2, "silencer attached.\n");
		DecreaseDestroySlot(y);

		if (self.current_slot == 1)
		{
			x = ToStatus(self.islot1);
			self.islot1 = 0;
			SetItemSlot(self, 1, SlotVal(IID_WP_MP9_S, x));
			return;
		}
		if (self.current_slot == 2)
		{
			x = ToStatus(self.islot2);
			self.islot2 = 0;
			SetItemSlot(self, 2, SlotVal(IID_WP_MP9_S, x));
			return;
		}
	}
	else if (weap == IID_WP_GREASEGUN)
	{
		sprint(self, 2, "silencer attached.\n");
		DecreaseDestroySlot(y);

		if (self.current_slot == 1)
		{
			x = ToStatus(self.islot1);
			self.islot1 = 0;
			SetItemSlot(self, 1, SlotVal(IID_WP_GREASEGUN_S, x));
			return;
		}
		if (self.current_slot == 2)
		{
			x = ToStatus(self.islot2);
			self.islot2 = 0;
			SetItemSlot(self, 2, SlotVal(IID_WP_GREASEGUN_S, x));
			return;
		}
	}
	else if (weap == IID_WP_PIPERIFLE)
	{
		sprint(self, 2, "silencer attached.\n");
		DecreaseDestroySlot(y);

		if (self.current_slot == 1)
		{
			x = ToStatus(self.islot1);
			self.islot1 = 0;
			SetItemSlot(self, 1, SlotVal(IID_WP_PIPERIFLE_S, x));
			return;
		}
		if (self.current_slot == 2)
		{
			x = ToStatus(self.islot2);
			self.islot2 = 0;
			SetItemSlot(self, 2, SlotVal(IID_WP_PIPERIFLE_S, x));
			return;
		}
	}
	else if (weap == IID_WP_PIPERIFLE_R)
	{
		sprint(self, 2, "silencer attached.\n");
		DecreaseDestroySlot(y);

		if (self.current_slot == 1)
		{
			x = ToStatus(self.islot1);
			self.islot1 = 0;
			SetItemSlot(self, 1, SlotVal(IID_WP_PIPERIFLE_S_R, x));
			return;
		}
		if (self.current_slot == 2)
		{
			x = ToStatus(self.islot2);
			self.islot2 = 0;
			SetItemSlot(self, 2, SlotVal(IID_WP_PIPERIFLE_S_R, x));
			return;
		}
	}
	else if (weap == IID_WP_PIPERIFLE_SCOPE)
	{
		sprint(self, 2, "silencer attached.\n");
		DecreaseDestroySlot(y);

		if (self.current_slot == 1)
		{
			x = ToStatus(self.islot1);
			self.islot1 = 0;
			SetItemSlot(self, 1, SlotVal(IID_WP_PIPERIFLE_SCOPE_S, x));
			return;
		}
		if (self.current_slot == 2)
		{
			x = ToStatus(self.islot2);
			self.islot2 = 0;
			SetItemSlot(self, 2, SlotVal(IID_WP_PIPERIFLE_SCOPE_S, x));
			return;
		}
	}
	else if (weap == IID_WP_PIPERIFLE_SCOPE_R)
	{
		sprint(self, 2, "silencer attached.\n");
		DecreaseDestroySlot(y);

		if (self.current_slot == 1)
		{
			x = ToStatus(self.islot1);
			self.islot1 = 0;
			SetItemSlot(self, 1, SlotVal(IID_WP_PIPERIFLE_SCOPE_S_R, x));
			return;
		}
		if (self.current_slot == 2)
		{
			x = ToStatus(self.islot2);
			self.islot2 = 0;
			SetItemSlot(self, 2, SlotVal(IID_WP_PIPERIFLE_SCOPE_S_R, x));
			return;
		}
	}
	else
	{
		sprint(self, 2, "cannot be silenced.\n");
		return;
	}

};


void() AttachExtender =
{
	local float weap;

	weap = ToIID(self.(SlotField(self.current_slot)));

	if (weap == IID_WP_NEEDLER || weap == IID_WP_MOONLIGHT)
	{
		sprint(self, 2, "that weapon already has a silencer attached\n");
		return;
	}
	if (weap == IID_WP_ALIENBLASTER)
	{
		sprint(self, 2, "how exactly do you extend that???\n");
		return;
	}
	if (self.silencer == weap)
	{
		sprint(self, 2, "that weapon already has a silencer attached\n");
		return;
	}
	else if (ToIID(self.islot4) == IID_EQUIP_EXTENDER && self.extender == 0)
	{
		self.extender = weap;
		sprint(self, 2, "extender attached.\n");
		self.islot4 = 0;
		self.attack_finished = time + 1;
		self.rtime = time + 1;
		return;
	}
/*
	else if (ToIID(self.islot4) == IID_EQUIP_EXTENDER && self.extender > 0)
	{
		self.extender = 0;
		sprint(self, 2, "removing extender...\n");
		self.attack_finished = time + 1;
		return;
	}*/
};

void (float iid) RemoveSilencer =
{
	if (ToIID(self.islot4) == 0)
	{
		SetItemSlot(self, 4, SlotVal(IID_EQUIP_SILENCER, 1));
		sprint(self, 2, "removing silencer...\n");
		self.attack_finished = time + 1;
		self.rtime = time + 1;
		self.silencer = 0;
	}
	else if (ToIID(self.islot4) > 0)
	{
		sprint(self, 2, "can't remove silencer, equipment slot is full\n");
		return;
	}
};

void (float iid) RemoveExtender =
{
	if (ToIID(self.islot4) == 0)
	{
		SetItemSlot(self, 4, SlotVal(IID_EQUIP_EXTENDER, 1));
		sprint(self, 2, "removing extender...\n");
		self.attack_finished = time + 1;
		self.rtime = time + 1;
		self.extender = 0;
	}
	else if (ToIID(self.islot4) > 0)
	{
		sprint(self, 2, "can't remove extender, equipment slot is full\n");
		return;
	}
};


void (string snd, float type) function_radio =
{
	local entity te;
	local vector dir;
	local vector vec;
	local float dot1;
	local float dot2;
	local float dot3;
	local float dot4;
	local float dot5;
	local float dot6;

	if (!self.attack_finished)
	{
		return;
	}
	self.attack_finished = (time + 2);
	spawn_excla (self, 2);
	te = find (world, classname, "player");
	while (te)
	{
		if (te.team == self.team)
		{
			sprint (te, 2, "radio");
			makevectors (te.angles);
			vec = normalize ((te.origin - self.origin));
			dot1 = ((vec * v_forward) * 1);
			dot2 = ((vec * v_right) * CONTENT_EMPTY);
			dot3 = ((vec * v_right) * 1);
			dot4 = ((vec * v_forward) * CONTENT_EMPTY);
			dot5 = ((vec * v_up) * 1);
			dot6 = ((vec * v_up) * CONTENT_EMPTY);
			if ((te == self))
			{
				sprint (te, 2, "(talk)");
			}
			else
			{
				if ((dot1 > 0.3))
				{
					sprint (te, 2, "(ahead)");
				}
				else
				{
					if ((dot2 > 0.3))
					{
						sprint (te, 2, "(left)");
					}
					else
					{
						if ((dot3 > 0.3))
						{
							sprint (te, 2, "(right)");
						}
						else
						{
							if ((dot4 > 0.3))
							{
								sprint (te, 2, "(behind)");
							}
							else
							{
								if ((dot5 > 0.3))
								{
									sprint (te, 2, "(above)");
								}
								else
								{
									if ((dot6 > 0.3))
									{
										sprint (te, 2, "(below)");
									}
									else
									{
										sprint (te, 2, "(???)");
									}
								}
							}
						}
					}
				}
			}
			sprint (te, 2, self.netname);
			if ((type == 1))
			{
				sprint (te, 2, ": affirmative");
			}
			else
			{
				if ((type == 2))
				{
					sprint (te, 2, ": negative");
				}
				else
				{
					if ((type == 3))
					{
						sprint (te, 2, ": lets go");
					}
					else
					{
						if ((type == 4))
						{
							sprint (te, 2, ": team, take cover");
						}
						else
						{
							if ((type == 5))
							{
								sprint (te, 2, ": team, get in formation!");
							}
							else
							{
								if ((type == 6))
								{
									sprint (te, 2, ": need backup!");
								}
								else
								{
									if ((type == 7))
									{
										sprint (te, 2, ": hold this position");
									}
									else
									{
										if ((type == 8))
										{
											sprint (te, 2, ": check your six");
										}
										else
										{
											if ((type == 9))
											{
												sprint (te, 2, ": secure this area");
											}
											else
											{
												if ((type == 10))
												{
													sprint (te, 2, ": squad, use suppressing fire!");
												}
												else
												{
													if ((type == 11))
													{
														sprint (te, 2, ": team, silence");
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
			sprint (te, 2, "\n");
			sound (self, CHAN_VOICE, snd, 0.55, ATTN_IDLE);
		}
		te = find (te, classname, "player");
	}
};


void () GModelThink =
{
	local float weap;

	weap = ToIID(self.(SlotField(self.owner.current_slot)));

	local float dot1;

	if (self.owner.health <= 0)
	{
		remove (self);
		return;
	}
	if (!IsMelee(weap))
	{
		setmodel(self, "");
		return;
	}
	if (self.owner.health >= 1)
	{
		self.frame = self.owner.weaponframe;
		self.nextthink = (time + 0.01);
	}

	self.nextthink = time + 0.01;

};

void() SpawnGModel =
{
	local entity hologram;
	local float weap;

	weap = ToIID(self.(SlotField(self.current_slot)));

	if (!self.wielded)
	{
		hologram = spawn ();
		hologram.movetype = MOVETYPE_NONE;
		hologram.solid = SOLID_NOT;
		hologram.owner = self;
		if (weap == IID_WP_KNIFE)
			setmodel (hologram, "progs/g_knife.mdl");
		if (weap == IID_WP_WRENCH)
			setmodel (hologram, "progs/g_wrench.mdl");
		if (weap == IID_WP_SLEDGE)
			setmodel (hologram, "progs/g_axe.mdl");
		if (weap == IID_WP_SPEAR)
			setmodel (hologram, "progs/g_spear.mdl");
	
		setsize (hologram, VEC_ORIGIN, VEC_ORIGIN);
	
		setattachment(hologram, self, "");
		hologram.nextthink = time + 0.01;
		hologram.think = GModelThink;
		self.wielded = hologram;
	}
	else
	{
		hologram = self.wielded;
		hologram.movetype = MOVETYPE_NONE;
		hologram.solid = SOLID_NOT;
		hologram.owner = self;
		if (weap == IID_WP_KNIFE)
			setmodel (hologram, "progs/g_knife.mdl");
		if (weap == IID_WP_WRENCH)
			setmodel (hologram, "progs/g_wrench.mdl");
		if (weap == IID_WP_SLEDGE)
			setmodel (hologram, "progs/g_axe.mdl");
		if (weap == IID_WP_SPEAR)
			setmodel (hologram, "progs/g_spear.mdl");
	
		setsize (hologram, VEC_ORIGIN, VEC_ORIGIN);
	
		setattachment(hologram, self, "");
		hologram.nextthink = time + 0.01;
		hologram.think = GModelThink;
	}
};
