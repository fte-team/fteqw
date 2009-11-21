void () bubble_bob;
void () make_bubbles;
void () bubble_remove;
void () PlayerDead;
void () player_diea1;
void () player_walk;
void () player_slow;

$cd /raid/quake/id1/models/enforcer
$origin 0 -6 24
$base base		
$skin skin

void() corpse_gib =
{
	self.solid = SOLID_NOT;
};


//
// running
//
$frame axrun1 axrun2 axrun3 axrun4 axrun5 axrun6

$frame rockrun1 rockrun2 rockrun3 rockrun4 rockrun5 rockrun6

//
// standing
//
$frame stand1 stand2 stand3 stand4 stand5

$frame axstnd1 axstnd2 axstnd3 axstnd4 axstnd5 axstnd6
$frame axstnd7 axstnd8 axstnd9 axstnd10 axstnd11 axstnd12


//
// pain
//
$frame axpain1 axpain2 axpain3 axpain4 axpain5 axpain6

$frame pain1 pain2 pain3 pain4 pain5 pain6


//
// death
//

$frame axdeth1 axdeth2 axdeth3 axdeth4 axdeth5 axdeth6
$frame axdeth7 axdeth8 axdeth9

$frame deatha1 deatha2 deatha3 deatha4 deatha5 deatha6 deatha7 deatha8
$frame deatha9 deatha10 deatha11

$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8
$frame deathb9

$frame deathc1 deathc2 deathc3 deathc4 deathc5 deathc6 deathc7 deathc8
$frame deathc9 deathc10 deathc11 deathc12 deathc13 deathc14 deathc15

$frame deathd1 deathd2 deathd3 deathd4 deathd5 deathd6 deathd7
$frame deathd8 deathd9

$frame deathe1 deathe2 deathe3 deathe4 deathe5 deathe6 deathe7
$frame deathe8 deathe9

//
// attacks
//
$frame nailatt1 nailatt2

$frame light1 light2

$frame rockatt1 rockatt2 rockatt3 rockatt4 rockatt5 rockatt6

$frame shotatt1 shotatt2 shotatt3 shotatt4 shotatt5 shotatt6

$frame axatt1 axatt2 axatt3 axatt4 axatt5 axatt6

$frame axattb1 axattb2 axattb3 axattb4 axattb5 axattb6

$frame axattc1 axattc2 axattc3 axattc4 axattc5 axattc6

$frame axattd1 axattd2 axattd3 axattd4 axattd5 axattd6


void () player_duck = [ 45, player_run ]
{
};


void () CrawlSound =
{
	local float rand;
	local float r;




	rand = random ();


	if (self.velocity == '0 0 0')
	{
		self.crawltime = 0;
		return;
	}


	self.attack_finished = time + 1;
	self.rtime = time + 1;


	r = 1 - (self.skill_sneak/40);

	if ((rand < 0.25))
		sound (self, CHAN_ITEM, "player/prone1.wav", r, ATTN_NORM);
	else if ((rand < 0.5))
		sound (self, CHAN_ITEM, "player/prone2.wav", r, ATTN_NORM);
	else if ((rand < 0.75))
		sound (self, CHAN_ITEM, "player/prone3.wav", r, ATTN_NORM);
	else
		sound (self, CHAN_ITEM, "player/prone4.wav", r, ATTN_NORM);


};


void () player_lay = [ 45, player_run ]
{
	if (self.velocity != '0 0 0')
		self.frame = 8 - self.crawltime;
	else if (self.velocity == '0 0 0')
	{
		self.frame = 0;
		return;
	}

	self.crawltime = self.crawltime - 1;

	if (self.crawltime <= 0)
	{
		CrawlSound();
		self.crawltime = 8;
	}


};

void () player_single_shot       = [ 89, player_single_shot2       ]{};
void () player_single_shot2      = [ 90, player_run                ]{};
void () player_single_shot_duck  = [ 183, player_single_shot_duck2 ]{};
void () player_single_shot_duck2 = [ 184, player_run               ]{};


void() idle_frames =
{
	local float weap;
	local string x;

	self.weaponframe = 0;
	return;

};



void() army_radio1;
void (vector org) spawn_tracer;

void (float db) loud_noise =
{
	//db is loudness factor
	//10 is low and 50 is high
	//10 would be a footstep
	//50 would be a gunshot

	local vector dir, start, org;
	local entity te;
	local string z, dt;
	local float x, y, d, t, v, md;

	if (db >= 50)//big rifles and guns can be heard through thick walls
		md = 3;
	else if (db >= 30)//things like SMGs and pistols, a little less
		md = 2;
	else if (db >= 15)//miscellaneous, richochets, silenced guns
		md = 1;
	else//very quiet things cannot be heard through doors
		md = 0;

	te = findradius(self.origin, db*40);

	while (te)
	{
		if (te.classname == "monster" && te.enemy.classname != "player")
		{
			dir = normalize(te.origin - self.origin);
			v = vlen(te.origin - self.origin);
			t = 32;
			d = 0;

			while (t < v)
			{
				start = (self.origin + (dir*t));

				if (pointcontents (start) == CONTENT_SOLID)
					d = d + 1;

				//spawn_tracer(start);
				t = t + 32;
			}

			if (x < db*20 && d <= md && (te.friend != self))
			{
				te.enemy = self;
				te.think = HuntTarget;
				te.nextthink = time + 0.25;
				te.alert = 20;

				if (world.map_obj == OBJ_SHADOW && self.has_radio == 1)
				{
					spawn_excla(self, 8);
					bprint(2, self.netname);
					bprint(2, " is calling for help! silence him!\n");
					self.rtime = 0;
					army_radio1();
					return;
				}
			}
			else
				te.alert = 20;
		}

		te = te.chain;
	}

	traceline (self.origin, te.origin, FALSE, self);
	if (trace_fraction == 1.0)
		return;

};

void (float db) monster_noise =
{
	//db is loudness factor
	//10 is low and 50 is high
	//10 would be a footstep
	//50 would be a gunshot

	local vector dir, start, org;
	local entity te;
	local string z, dt;
	local float x, y, d, t, v, md;

	if (db >= 50)
		md = 3;
	else if (db >= 30)
		md = 2;
	else if (db >= 15)
		md = 1;
	else
		md = 0;

	te = findradius(self.origin, db*40);

	while (te)
	{
		if (te.classname == "monster" && te.enemy.classname == "world")
		{
			dir = normalize(te.origin - self.origin);
			v = vlen(te.origin - self.origin);
			t = 32;
			d = 0;

			while (t < v)
			{
				start = (self.origin + (dir*t));

				if (pointcontents (start) == CONTENT_SOLID)
					d = d + 1;

				t = t + 32;
			}
		}

		te = te.chain;
	}

	traceline (self.origin, te.origin, FALSE, self);
	if (trace_fraction == 1.0)
		return;

};


void () Footstep =
{
	local float rand;
	local float r;

	rand = random ();

	if (self.sneak != 0)
		return;


	r = 1 - (self.skill_sneak/40);

	if (self.classname == "player")
	{

		loud_noise(20 - self.skill_sneak);
	}

	if (world.worldtype == 1 || world.worldtype == 2)
	{
		if ((rand < 0.25))
			sound (self, CHAN_ITEM, "player/step1.wav", r, ATTN_NORM);
		else if ((rand < 0.5))
			sound (self, CHAN_ITEM, "player/step2.wav", r, ATTN_NORM);
		else if ((rand < 0.75))
			sound (self, CHAN_ITEM, "player/step3.wav", r, ATTN_NORM);
		else
			sound (self, CHAN_ITEM, "player/step4.wav", r, ATTN_NORM);
	}
	if (world.worldtype == 0)
	{
		if ((rand < 0.25))
			sound (self, CHAN_ITEM, "player/step1a.wav", r, ATTN_NORM);
		else if ((rand < 0.5))
			sound (self, CHAN_ITEM, "player/step2a.wav", r, ATTN_NORM);
		else if ((rand < 0.75))
			sound (self, CHAN_ITEM, "player/step3a.wav", r, ATTN_NORM);
		else
			sound (self, CHAN_ITEM, "player/step4a.wav", r, ATTN_NORM);
	}
};

float (entity guy) holding_melee =
{
	local float iid;
	iid = ToIID(ItemInSlot(guy, guy.current_slot));

	if (iid == IID_NONE ||
	    iid == IID_WP_KNIFE ||
	    iid == IID_WP_SLEDGE ||
	    iid == IID_WP_SPEAR ||
	    iid == IID_WP_WRENCH)
		return true;
	return false;
};
void () player_crouch;

void () player_stand1 = [ 149, player_stand1 ]
{
	if (self.rtime < time && self.attack == 0)
		idle_frames();


	if (self.position == 1)
	{
		player_crouch ();
		return;
	}
	else
	{
		if (self.position == 2)
		{
			player_lay ();
			return;
		}
	}
	if (self.velocity_x || self.velocity_y)
	{
		self.walkframe = 0;
		player_run ();
		return;
	}
	if (self.walkframe >= 4)
		self.walkframe = 0;

	self.frame = 149 + self.walkframe;
	self.walkframe = self.walkframe + 1;
};

void () player_crouch = [ 45, player_run ]
{
	if (self.rtime < time && self.attack == 0)
		self.weaponframe = 0;

	if (!self.velocity_x && !self.velocity_y)
	{
		self.frame = 45;
		return;
	}
	else
	{
		if (self.position == 2)
		{
			player_lay ();
			return;
		}
	}

	idle_frames();

	self.frame = (35 + self.walkframe);

	if (self.walkframe == 11)
		self.walkframe = 0;

	self.walkframe = (self.walkframe + 1);
};

void () player_climb = [ 23, player_run ]
{
	self.weaponframe = 0;
	if (self.velocity == '0 0 0')
	{
		self.frame = 25;
		return;
	}
	self.frame = (23 + self.walkframe);
	if ((self.walkframe >= 10))
	{
		self.walkframe = 0;
	}
	self.walkframe = (self.walkframe + 1);
};

void () player_run = [ 137, player_run ]
{
	if (self.rtime <= time)
		idle_frames();

	if (self.equipment_slot)
		if ((ToIID(ItemInSlot(self, self.equipment_slot))) == IID_EQUIP_CLIMBINGGEAR)
		{
			if (self.gravity == 0)
			{
				player_climb();
				return;
			}
		}

	if ((!self.velocity_x && !self.velocity_y))
	{
		player_stand1 ();
		return;
	}
	if ((self.position == 1))
	{
		player_crouch ();
		return;
	}
	else
	{
		if ((self.position == 2))
		{
			player_lay ();
			return;
		}
	}

	if (self.speedmulti <= 0.60)
	{
		player_slow ();
		return;
	}
	else if (self.speedmulti <= 0.80)
	{
		player_walk ();
		return;
	}

	if (self.walkframe == 2 && self.ghost == 0 && self.position == 0 && self.velocity_z == 0)
		Footstep ();
	if (self.walkframe == 5 && self.ghost == 0 && self.position == 0 && self.velocity_z == 0)
		Footstep ();

	idle_frames();

	self.frame = (137 + self.walkframe*2);

	self.walkframe = self.walkframe + 1;

	if (self.walkframe >= 6)
		self.walkframe = 0;
};

void () player_walk = [ 170, player_walk ]
{
	if (self.rtime <= time)
		self.weaponframe = 0;

	if (self.equipment_slot)
		if ((ToIID(ItemInSlot(self, self.equipment_slot))) == IID_EQUIP_CLIMBINGGEAR)
		{
			if (self.gravity == 0)
			{
				player_climb();
				return;
			}
		}

	if (!self.velocity_x && !self.velocity_y)
	{
		player_stand1 ();
		return;
	}
	if (self.position == 1)
	{
		player_crouch ();
		return;
	}
	else
	{
		if (self.position == 2)
		{
			player_lay ();
			return;
		}
	}

	if (self.speedmulti > 0.80)
	{
		player_run ();
		return;
	}
	else if (self.speedmulti <= 0.60)
	{
		player_slow ();
		return;
	}

	if (self.walkframe == 3 && self.ghost == 0 && self.position == 0 && self.velocity_z == 0)
		Footstep ();

	if (self.walkframe == 8 && self.ghost == 0 && self.position == 0 && self.velocity_z == 0)
		Footstep ();

	idle_frames();

	self.frame = (170 + self.walkframe);

	if (self.walkframe >= 11)
		self.walkframe = 0;

	self.walkframe = (self.walkframe + 1);
};

void () player_slow = [ 170, player_slow ]
{
	if (self.rtime <= time)
		self.weaponframe = 0;

	if (self.equipment_slot)
		if ((ToIID(ItemInSlot(self, self.equipment_slot))) == IID_EQUIP_CLIMBINGGEAR)
		{
			if (self.gravity == 0)
			{
				player_climb();
				return;
			}
		}

	if (!self.velocity_x && !self.velocity_y)
	{
		player_stand1 ();
		return;
	}
	if (self.position == 1)
	{
		player_crouch ();
		return;
	}
	else
	{
		if (self.position == 2)
		{
			player_lay ();
			return;
		}
	}

	if (self.speedmulti > 0.80)
	{
		player_run ();
		return;
	}
	else if (self.speedmulti > 0.60)
	{
		player_walk ();
		return;
	}

	if (self.walkframe == 6 && self.ghost == 0 && self.position == 0 && self.velocity_z == 0)
		Footstep ();

	if (self.walkframe == 18 && self.ghost == 0 && self.position == 0 && self.velocity_z == 0)
		Footstep ();

	idle_frames();

	self.frame = (170 + floor(self.walkframe/2));

	if (self.walkframe >= 22)
		self.walkframe = 0;

	self.walkframe = (self.walkframe + 1);
};

void () player_reload1 = [ 123, player_reload2 ]
{
};

void () player_reload2 = [ 124, player_reload3 ]
{
};

void () player_reload3 = [ 125, player_reload4 ]
{
};

void () player_reload4 = [ 126, player_reload5 ]
{
};

void () player_reload5 = [ 127, player_reload6 ]
{
};

void () player_reload6 = [ 128, player_reload7 ]
{
};

void () player_reload7 = [ 129, player_reload8 ]
{
};

void () player_reload8 = [ 130, player_reload9 ]
{
};

void () player_reload9 = [ 131, player_reload10 ]
{
};

void () player_reload10 = [ 132, player_reload11 ]
{
};

void () player_reload11 = [ 133, player_reload12 ]
{
};

void () player_reload12 = [ 134, player_reload13 ]
{
};

void () player_reload13 = [ 135, player_reload14 ]
{
};

void () player_reload14 = [ 136, player_run ]
{
};

void () player_creload1 = [ 74, player_creload2 ]
{
};

void () player_creload2 = [ 75, player_creload3 ]
{
};

void () player_creload3 = [ 76, player_creload4 ]
{
};

void () player_creload4 = [ 77, player_creload5 ]
{
};

void () player_creload5 = [ 78, player_creload6 ]
{
};

void () player_creload6 = [ 79, player_creload7 ]
{
};

void () player_creload7 = [ 80, player_creload8 ]
{
};

void () player_creload8 = [ 81, player_creload9 ]
{
};

void () player_creload9 = [ 82, player_creload10 ]
{
};

void () player_creload10 = [ 83, player_creload11 ]
{
};

void () player_creload11 = [ 84, player_creload12 ]
{
};

void () player_creload12 = [ 85, player_creload13 ]
{
};

void () player_creload13 = [ 86, player_creload14 ]
{
};

void () player_creload14 = [ 87, player_run ]
{
};

void () player_use1 = [ 155, player_use2 ]
{
};

void () player_use2 = [ 156, player_use3 ]
{
};

void () player_use3 = [ 157, player_use4 ]
{
};

void () player_use4 = [ 158, player_use5 ]
{
};

void () player_use5 = [ 159, player_use6 ]
{
};

void () player_use6 = [ 160, player_use7 ]
{
};

void () player_use7 = [ 161, player_use8 ]
{
};

void () player_use8 = [ 162, player_use9 ]
{
};

void () player_use9 = [ 163, player_use10 ]
{
};

void () player_use10 = [ 164, player_use11 ]
{
};

void () player_use11 = [ 165, player_use12 ]
{
};

void () player_use12 = [ 166, player_use13 ]
{
};

void () player_use13 = [ 167, player_use14 ]
{
};

void () player_use14 = [ 168, player_use15 ]
{
};

void () player_use15 = [ 169, player_use16 ]
{
};

void () player_use16 = [ 170, player_run ]
{
};

void () player_holster1 = [ 107, player_holster2 ]
{
	self.attack_finished = (time + 0.25);
};

void () player_holster2 = [ 109, player_holster3 ]
{
};

void () player_holster3 = [ 111, player_holster4 ]
{
};

void () player_holster4 = [ 112, player_holster5 ]
{
};

void () player_holster5 = [ 113, player_holster6 ]
{
};

void () player_holster6 = [ 114, player_holster7 ]
{
};

void () player_holster7 = [ 115, player_holster8 ]
{
};

void () player_holster8 = [ 116, player_holster9 ]
{
};

void () player_holster9 = [ 117, player_holster10 ]
{
};

void () player_holster10 = [ 119, player_holster11 ]
{
};

void () player_holster11 = [ 121, player_holster12 ]
{
};

void () player_holster12 = [ 122, player_run ]
{
};

void () player_jump1 = [ 48, player_jump2 ]
{
};

void () player_jump2 = [ 49, player_jump3 ]
{
};

void () player_jump3 = [ 50, player_jump4 ]
{
};

void () player_jump4 = [ 52, player_jump5 ]
{
};

void () player_jump5 = [ 54, player_jump6 ]
{
};

void () player_jump6 = [ 53, player_jump7 ]
{
};

void () player_jump7 = [ 51, player_jump8 ]
{
};

void () player_jump8 = [ 49, player_jump9 ]
{
};

void () player_jump9 = [ 48, player_run ]
{
};
void()  player_singlex =  [89, player_singlex2       ] {muzzleflash();};
void()  player_singlex2 = [89, player_run            ] {muzzleflash();};

void()  player_singley =  [90, player_singley2       ] {muzzleflash();};
void()  player_singley2 = [90, player_run            ] {muzzleflash();};

void()  player_auto_duck1 = [182, player_auto_duck2  ] {muzzleflash();};
void()  player_auto_duck2 = [182, player_run         ] {muzzleflash();};

void()  player_auto_ducka1 = [183, player_auto_ducka2    ] {muzzleflash();};
void()  player_auto_ducka2 = [183, player_run            ] {muzzleflash();};

void () player_single1_left = [ 88, player_single2_left ]
{
	self.weaponframe = 4;
	muzzleflash ();
};

void () player_single2_left = [ 89, player_run ]
{
	self.weaponframe = 5;
};

void () player_single1s_left = [ 183, player_single2_s ]
{
	self.weaponframe = 4;
	muzzleflash ();
};

void () player_single2s_left = [ 184, player_run ]
{
	self.weaponframe = 5;
};

void () player_singlea = [ 88, player_run ]
{
	self.weaponframe = WEAPON_SHOTGUN;
	muzzleflash ();
};

void () player_singleaz = [ 183, player_run ]
{
	self.weaponframe = WEAPON_SHOTGUN;
	muzzleflash ();
};

void () player_singlea2 = [ 89, player_run ]
{
	self.weaponframe = WEAPON_SHOTGUN;
	muzzleflash ();
};

void () player_singleb = [ 89, player_run ]
{
	self.weaponframe = WEAPON_ROCKET;
	muzzleflash ();
};

void () player_singleb2 = [ 89, player_run ]
{
	self.weaponframe = WEAPON_ROCKET;
	muzzleflash ();
};

void () player_singlebz = [ 184, player_run ]
{
	self.weaponframe = WEAPON_ROCKET;
	muzzleflash ();
};

void () player_shotty1 = [ 88, player_shotty2 ]
{
	self.weaponframe = WEAPON_SHOTGUN;
};

void () player_shotty2 = [ 89, player_shotty3 ]
{
	self.weaponframe = WEAPON_ROCKET;
};

void () player_shotty3 = [ 90, player_shotty4 ]
{
	self.weaponframe = AS_MELEE;
};

void () player_shotty4 = [ 91, player_run ]
{
	self.weaponframe = WEAPON_ROCKET;
};

void () player_pull1 = [ 155, player_pull2 ]
{
	self.weaponframe = 1;
};

void () player_pull2 = [ 156, player_pull3 ]
{
	self.weaponframe = 2;
	sound (self, CHAN_WEAPON, "weapons/gpull.wav", WEAPON_SHOTGUN, ATTN_IDLE);
};

void () player_pull3 = [ 157, player_pull4 ]
{
	self.weaponframe = AS_MELEE;
};

void () player_pull4 = [ 158, player_pull5 ]
{
	self.weaponframe = WEAPON_SPIKES;
};

void () player_pull5 = [ 157, player_pull6 ]
{
	self.weaponframe = MULTICAST_PVS_R;
};

void () player_pull6 = [ 156, player_pull7 ]
{
	self.weaponframe = TE_LIGHTNING2;
};

void () player_pull7 = [ 155, player_pull8 ]
{
	self.weaponframe = TE_WIZSPIKE;
};

void () player_pull8 = [ 155, player_pull9 ]
{
	self.weaponframe = 8;
	self.grenadetoggle = 1;
};

void () player_pull9 = [ 155, player_pull10 ]
{
	self.weaponframe = 9;
};

void () player_pull10 = [ 155, player_pull11 ]
{
	self.weaponframe = 10;
};

void () player_pull11 = [ 155, player_throw1 ]
{
	self.weaponframe = 11;
};

void () player_throw1 = [ 155, player_throw2 ]
{
	if (self.attack_finished > time)
		return;

	self.attack_finished = time + 1;

	self.weaponframe = 13;
	if (((random () * WEAPON_BIG) <= WEAPON_SPIKES))
		sound (self, CHAN_VOICE, "radio/grenade.wav", 0.7, ATTN_NORM);
	else
		sound (self, CHAN_VOICE, "radio/lookout.wav", 0.7, ATTN_NORM);
};

void () player_throw2 = [ 156, player_run ]
{
	self.weaponframe = 14;
	if (self.grenslot == 0)
		self.grenslot = self.current_slot;

	FireHandGrenade ();
	self.attack_finished = time + 1;
	W_SetCurrentAmmo();
};

void () player_shotty1b = [ 183, player_shotty2b ]
{
	self.weaponframe = WEAPON_SHOTGUN;
};

void () player_shotty2b = [ 183, player_shotty3b ]
{
	self.weaponframe = WEAPON_ROCKET;
};

void () player_shotty3b = [ 184, player_shotty4b ]
{
	self.weaponframe = AS_MELEE;
};

void () player_shotty4b = [ 184, player_run ]
{
	self.weaponframe = WEAPON_ROCKET;
};

void () player_punch1 = [ 155, player_run ]
{
	self.weaponframe = 1;
	W_FireMelee ();
};
void () player_punch2 = [ 156, player_run ]
{
	self.weaponframe = 2;
	W_FireMelee ();
};


void () player_swing1 = [ 155, player_run ]
{
	self.attack_finished = time + 0.30;
	self.weaponframe = 1;
	W_FireMelee ();
};

void () player_swing1a = [ 155, player_run ]
{
	self.attack_finished = time + 0.30;
	self.weaponframe = 2;
	W_FireMelee ();
};


void () player_kick1 = [ 155, player_kick2 ]
{
	self.attack_finished = time + 0.40;
	self.weaponframe = 7;
};
void () player_kick2 = [ 156, player_kick3 ]
{
	self.weaponframe = 8;
	W_FireMelee ();
	W_FireMelee ();
};
void () player_kick3 = [ 157, player_run ]
{
	self.weaponframe = 9;
};




void () player_kick1a = [ 155, player_kick2a ]
{
	self.attack_finished = time + 0.40;
	self.weaponframe = 10;
};
void () player_kick2a = [ 156, player_kick3a ]
{
	self.weaponframe = 11;
	W_FireMelee ();
	W_FireMelee ();
};
void () player_kick3a = [ 157, player_run ]
{
	self.weaponframe = 12;
};

void () player_kick1b = [ 155, player_kick2b ]
{
	self.attack_finished = time + 1;
	self.weaponframe = 10;
};
void () player_kick2b = [ 156, player_kick3b ]
{
	self.weaponframe = 10;

};
void () player_kick3b = [ 155, player_kick4b ]
{
	self.weaponframe = 11;
	W_FireMelee ();
	W_FireMelee ();
};
void () player_kick4b = [ 156, player_kick5b ]
{
	self.weaponframe = 11;
	W_FireMelee ();
	W_FireMelee ();
};
void () player_kick5b = [ 155, player_kick6b ]
{
	self.weaponframe = 12;
	W_FireMelee ();
	W_FireMelee ();
};
void () player_kick6b = [ 156, player_run ]
{
	W_FireMelee ();
	W_FireMelee ();
	self.attack_finished = time + 1;
	self.weaponframe = 12;
};

void () player_dragon_kick =
{
	self.velocity = self.velocity * 1.50;
	self.velocity_x = self.velocity_x * 1.50;
	self.velocity_y = self.velocity_y * 1.50;

	self.recoil = 30;
	player_kick1b();
};

void () player_knife1 = [ 91, player_knife2 ]
{
	self.weaponframe = 5;
};

void () player_knife2 = [ 92, player_knife3]
{
	self.weaponframe = 6;
};

void () player_knife3 = [ 93, player_run ]
{
	self.weaponframe = 7;
	W_FireMelee ();
};

void () player_spear1 = [ 89, player_spear2 ]
{
	self.weaponframe = 5;
};

void () player_spear2 = [ 90, player_spear3 ]
{
	self.weaponframe = 6;
};

void () player_spear3 = [ 89, player_run ]
{
	self.weaponframe = 7;
	W_FireMelee ();
};

void () player_spear1a = [ 89, player_spear2a ]
{
	self.weaponframe = 5;
};
void () player_spear2a = [ 90, player_spear3a ]
{
	self.weaponframe = 6;
};
void () player_spear3a = [ 89, player_run ]
{
	self.weaponframe = 7;
	W_FireMelee ();
};

void () player_knifea = [ 91, player_knifeb ]
{
	self.weaponframe = 8;
};

void () player_knifeb = [ 92, player_knifec ]
{
	self.weaponframe = 9;
};

void () player_knifec = [ 93, player_run ]
{
	self.weaponframe = 10;
	W_FireMelee ();
};

void () player_axe1 = [ 89, player_axe2 ]
{
	self.weaponframe = 1;
};
void () player_axe2 = [ 89, player_axe3 ]
{
	self.weaponframe = 1;
};
void () player_axe3 = [ 90, player_axe4 ]
{
	sound (self, CHAN_WEAPON, "effects/axe_swing.wav", TRUE, ATTN_NORM);
	self.weaponframe = 2;
};
void () player_axe4 = [ 90, player_axe5 ]
{
	self.weaponframe = 3;
	W_FireMelee ();
};
void () player_axe5 = [ 89, player_run ]
{
	self.weaponframe = 7;
};

void () player_axeb1 = [ 89, player_axeb2 ]
{
	self.weaponframe = 4;
};
void () player_axeb2 = [ 89, player_axeb3 ]
{
	self.weaponframe = 4;
};
void () player_axeb3 = [ 90, player_axeb4 ]
{
	sound (self, CHAN_WEAPON, "effects/axe_swing.wav", TRUE, ATTN_NORM);
	self.weaponframe = 5;
};
void () player_axeb4 = [ 90, player_axeb5 ]
{
	self.weaponframe = 6;
	W_FireMelee ();
};
void () player_axeb5 = [ 89, player_run ]
{
	self.weaponframe = 8;
};

void () player_nail1 = [ 88, player_nail2 ]
{
	muzzleflash ();
	if (((!self.button0 || intermission_running) || self.impulse))
	{
		player_run ();
		return;
	}
	self.weaponframe = (self.weaponframe + WEAPON_SHOTGUN);
	if ((self.weaponframe == AS_MELEE))
	{
		self.weaponframe = WEAPON_SHOTGUN;
	}
};

void () player_nail2 = [ 89, player_nail1 ]
{
	muzzleflash ();
	if (((!self.button0 || intermission_running) || self.impulse))
	{
		player_run ();
		return;
	}
	self.weaponframe = (self.weaponframe + WEAPON_SHOTGUN);
	if ((self.weaponframe == AS_MELEE))
	{
		self.weaponframe = WEAPON_SHOTGUN;
	}
};

void () player_auto1 = [ 88, player_auto2 ]
{
	muzzleflash ();
	if (((!self.button0 || intermission_running) || self.impulse))
	{
		player_run ();
		return;
	}
	self.weaponframe = (self.weaponframe + WEAPON_SHOTGUN);
	if ((self.weaponframe == WEAPON_ROCKET))
	{
		self.weaponframe = WEAPON_SHOTGUN;
	}
};

void () player_auto2 = [ 89, player_auto1 ]
{
	muzzleflash ();
	if (((!self.button0 || intermission_running) || self.impulse))
	{
		player_run ();
		return;
	}
	self.weaponframe = (self.weaponframe + WEAPON_SHOTGUN);
	if ((self.weaponframe == WEAPON_ROCKET))
	{
		self.weaponframe = WEAPON_SHOTGUN;
	}
};

void () player_auto3 = [ 88, player_auto4 ]
{
	muzzleflash ();
	if (((!self.button0 || intermission_running) || self.impulse))
	{
		player_run ();
		return;
	}
	self.weaponframe = WEAPON_SHOTGUN;
};

void () player_auto4 = [ 89, player_auto3 ]
{
	muzzleflash ();
	if (((!self.button0 || intermission_running) || self.impulse))
	{
		player_run ();
		return;
	}
	self.weaponframe = MULTICAST_ALL;
};
void (float num_bubbles) DeathBubbles;

void () PainSound =
{
	if ((self.equipment == 8))
		return;

	if ((self.health <= 0))
		return;

	if (self.air_finished < time)
	{
		sound (self, CHAN_VOICE, "player/drown2.wav", 1, ATTN_NORM);
		return;
	}

	if ((damage_attacker.classname == "teledeath"))
	{
		sound (self, CHAN_VOICE, "player/teledth1.wav", WEAPON_SHOTGUN, ATTN_NONE);
		return;
	}
	if (((self.watertype == CONTENT_WATER) && (self.waterlevel == AS_MELEE)))
	{
		DeathBubbles (WEAPON_SHOTGUN);
		sound (self, CHAN_VOICE, "player/pain1.wav", WEAPON_SHOTGUN, ATTN_NORM);
		return;
	}
	if ((self.watertype == CONTENT_SLIME))
	{
		if ((random () > 0.5))
			sound (self, CHAN_VOICE, "player/slimbrn2.wav", WEAPON_SHOTGUN, ATTN_NORM);
		else
			sound (self, CHAN_VOICE, "player/lburn2.wav", WEAPON_SHOTGUN, ATTN_NORM);
		return;
	}
	if ((self.watertype == CONTENT_LAVA))
	{
		if ((random () > 0.5))
			sound (self, CHAN_VOICE, "player/lburn1.wav", WEAPON_SHOTGUN, ATTN_NORM);
		else
			sound (self, CHAN_VOICE, "player/lburn2.wav", WEAPON_SHOTGUN, ATTN_NORM);
		return;
	}
	if ((self.pain_finished > time))
	{
		self.axhitme = MULTICAST_ALL;
		return;
	}
	self.pain_finished = (time + 1.5);
	if ((self.axhitme == WEAPON_SHOTGUN))
	{
		self.axhitme = 0;
		sound (self, CHAN_VOICE, "player/pain1.wav", WEAPON_SHOTGUN, ATTN_NORM);
		return;
	}
	self.noise = "player/pain2.wav";
	sound (self, CHAN_VOICE, self.noise, WEAPON_SHOTGUN, ATTN_NORM);
	return;
};

void () player_pain1 = [ 14, player_pain2 ]
{
};

void () player_pain2 = [ 15, player_pain3 ]
{
};

void () player_pain3 = [ 16, player_pain4 ]
{
};

void () player_pain4 = [ 17, player_pain5 ]
{
};

void () player_pain5 = [ 18, player_pain6 ]
{
};

void () player_pain6 = [ 19, player_run ]
{
};

void () player_pain =
{
	self.pain_finished = time + 0.1;
	self.missionbrief = 6;

	if (self.pain_finished > time)
	{
		sound (self, CHAN_VOICE, self.armornoise, 1, ATTN_NORM);
		return;
	}

	if ((self.health <= 40) && (getperk(9)) && (random()<=0.10))
	{
		sprint(self, 2, "THE ZONE!\n");
		self.rage = 255;
		self.ragetime = 10;
	}


	self.pain_finished = time + 0.1;

	if (random () * WEAPON_BIG < WEAPON_SPIKES)
		sound (self, CHAN_VOICE, "player/paina.wav", WEAPON_SHOTGUN, ATTN_NORM);
	else
		sound (self, CHAN_VOICE, "player/painb.wav", WEAPON_SHOTGUN, ATTN_NORM);
	if (random () * WEAPON_BIG < TE_LIGHTNING2)
		sound (self, CHAN_BODY, "player/hit1.wav", WEAPON_SHOTGUN, ATTN_NORM);
	if (self.invisible_finished > time)
		return;

	if (getperk(1))
		self.maxspeed = (self.maxspeed * 0.9);
	else
		self.maxspeed = (self.maxspeed * 0.6);

	player_pain1 ();
};

void () player_dieb1;
void () player_diec1;

void () DeathBubblesSpawn =
{
	local entity bubble;

	if ((self.owner.waterlevel != AS_MELEE))
	{
		return;
	}
	bubble = spawn ();
	setmodel (bubble, "progs/s_bubble.spr");
	setorigin (bubble, (self.owner.origin + '0 0 24'));
	bubble.movetype = MOVETYPE_NOCLIP;
	bubble.solid = SOLID_NOT;
	bubble.velocity = '0 0 15';
	bubble.nextthink = (time + 0.5);
	bubble.think = bubble_bob;
	bubble.classname = "bubble";
	bubble.frame = MULTICAST_ALL;
	bubble.cnt = MULTICAST_ALL;
	setsize (bubble, '-8 -8 -8', '8 8 8');
	self.nextthink = (time + 0.1);
	self.think = DeathBubblesSpawn;
	self.air_finished = (self.air_finished + 1);
	if ((self.air_finished >= self.bubble_count))
	{
		remove (self);
	}
};

void (float num_bubbles) DeathBubbles =
{
	local entity bubble_spawner;

	bubble_spawner = spawn ();
	setorigin (bubble_spawner, self.origin);
	bubble_spawner.movetype = MOVETYPE_NONE;
	bubble_spawner.solid = SOLID_NOT;
	bubble_spawner.nextthink = (time + 0.1);
	bubble_spawner.think = DeathBubblesSpawn;
	bubble_spawner.air_finished = MULTICAST_ALL;
	bubble_spawner.owner = self;
	bubble_spawner.bubble_count = num_bubbles;
	return;
};

void () DeathSound =
{
	local float r;

	if ((self.equipment == 8))
	{
		return;
	}
	if ((self.waterlevel == AS_MELEE))
	{
		sound (self, CHAN_VOICE, "player/drown2.wav", WEAPON_SHOTGUN, ATTN_NONE);
		return;
	}
	r = random ();
	self.noise = "player/agdie4.wav";
	sound (self, CHAN_VOICE, self.noise, 0.9, ATTN_NORM);
	return;
};

void () PlayerDead =
{
	setmodel(self, "progs/dead.mdl");
	setsize(self, '-16 -16 -24', '16 16 0');
	self.nextthink = CONTENT_EMPTY;
	self.deadflag = DEAD_DEAD;
};

vector(float dm) VelocityForDamage =
{
	local vector v;

	v_x = 100 * crandom();
	v_y = 100 * crandom();
	v_z = 200 + 100 * random();

	if (dm > -50)
	{
//		dprint ("level 1\n");
		v = v * 0.7;
	}
	else if (dm > -200)
	{
//		dprint ("level 3\n");
		v = v * 2;
	}
	else
		v = v * 10;

	return v;
};

void(string gibname, float dm) ThrowGib =
{
	local	entity new;

	new = spawn();
	new.origin = self.origin;
	setmodel (new, gibname);
	setsize (new, '0 0 0', '0 0 0');
	new.velocity = VelocityForDamage (dm);
	new.movetype = MOVETYPE_BOUNCE;
	new.solid = SOLID_NOT;
//	new.avelocity_x = random()*600;
	new.avelocity_y = random()*600;
//	new.avelocity_z = random()*600;
	new.think = SUB_Remove;
	new.ltime = time;
	new.nextthink = time + 10 + random()*10;
	new.frame = 0;
	new.flags = 0;
};

void(string gibname, float dm) ThrowHead =
{
	setmodel (self, gibname);
	self.frame = 0;
	self.nextthink = -1;
	self.movetype = MOVETYPE_BOUNCE;
	self.takedamage = DAMAGE_NO;
	self.solid = SOLID_NOT;
	self.view_ofs = '0 0 8';
	setsize (self, '-16 -16 0', '16 16 56');
	self.velocity = VelocityForDamage (dm);
	self.origin_z = self.origin_z - 24;
	self.flags = self.flags - (self.flags & FL_ONGROUND);
	self.avelocity = crandom() * '0 600 0';
};

void (string gibname, float dm) ThrowGib2 =
{
	local	entity new;

	new = spawn();
	new.origin = self.origin;
	setmodel (new, gibname);
	setsize (new, '0 0 0', '0 0 0');
	new.velocity = VelocityForDamage (dm);
	new.velocity_z = new.velocity_z + 40;
	new.movetype = MOVETYPE_BOUNCE;
	new.solid = SOLID_NOT;
//	new.avelocity_x = random()*600;
	new.avelocity_y = random()*600;
//	new.avelocity_z = random()*600;
	new.think = SUB_Remove;
	new.ltime = time;
	new.nextthink = time + 10 + random()*10;
	new.frame = 0;
	new.flags = 0;
};


void () SmokeBob2 =
{
	local float rnd1;
	local float rnd2;
	local float rnd3;

	self.cnt = (self.cnt + WEAPON_SHOTGUN);
	if ((self.cnt >= (WEAPON_ROCKET + (random () * AS_MELEE))))
	{
		remove (self);
	}
	rnd1 = (self.velocity_x + (-10 + (random () * IDLE8A)));
	rnd2 = (self.velocity_y + (-10 + (random () * IDLE8A)));
	rnd3 = ((self.velocity_z + TE_LAVASPLASH) + (random () * TE_LAVASPLASH));
	if ((rnd1 > MULTICAST_PVS_R))
	{
		rnd1 = MULTICAST_PVS_R;
	}
	if ((rnd1 < CONTENT_LAVA))
	{
		rnd1 = CONTENT_LAVA;
	}
	if ((rnd2 > MULTICAST_PVS_R))
	{
		rnd2 = MULTICAST_PVS_R;
	}
	if ((rnd2 < CONTENT_LAVA))
	{
		rnd2 = CONTENT_LAVA;
	}
	if ((rnd3 < TE_LAVASPLASH))
	{
		rnd3 = IDLE3A;
	}
	if ((rnd3 > SVC_INTERMISSION))
	{
		rnd3 = DRAW3;
	}
	self.velocity_x = rnd1;
	self.velocity_y = rnd2;
	self.velocity_z = rnd3;
	self.nextthink = (time + 0.5);
	self.think = SmokeBob2;
};

void (string gibname, float dm) ThrowPlayerHead =
{
	setmodel (self, gibname);
	self.frame = MULTICAST_ALL;
	if (self.team == 1)
		self.skin = 0;
	if (self.team == 2)
		self.skin = 1;
	self.nextthink = (time + WEAPON_SHOTGUN);
	self.think = SUB_Null;
	self.movetype = MOVETYPE_BOUNCE;
	self.takedamage = DAMAGE_NO;
	self.view_ofs = '0 0 8';
	setsize (self, '-8 -8 -8', '8 8 8');
	self.velocity = VelocityForDamage (dm);
	self.origin_z = (self.origin_z + DRAW2);
	self.flags = (self.flags - (self.flags & FL_ONGROUND));
	self.avelocity = (crandom () * '0 600 0');
};

void () GibPlayer =
{
	self.solid = SOLID_NOT;
	self.deadflag = DEAD_DYING;
	setmodel (self, "");
	self.skin = self.team;
	ThrowGib ("progs/gib1.mdl", self.health);
	ThrowGib ("progs/gib1.mdl", self.health);
	ThrowGib ("progs/gib3.mdl", self.health);
	SpawnMeatSpray (self.origin, (self.origin + VEC_ORIGIN));
	SpawnMeatSpray (self.origin, (self.origin + VEC_ORIGIN));
	SpawnMeatSpray (self.origin, (self.origin + VEC_ORIGIN));
	self.deadflag = DEAD_DEAD;
};

void () PlayerDie =
{
	local float i;
	local float r;

	r = random ();
	self.noise = "player/agdie2.wav";

	sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
	self.items = (self.items - (self.items & IT_INVISIBILITY));
	self.invisible_finished = 0;
	self.invincible_finished = 0;
	self.super_damage_finished = 0;
	self.radsuit_finished = 0;
	self.modelindex = modelindex_player;
	self.weaponmodel = "";
	self.view_ofs = '0 0 -8';
	self.deadflag = DEAD_DYING;
	self.flags = (self.flags - (self.flags & FL_ONGROUND));
	self.movetype = MOVETYPE_WALK;

	self.angles_z = 0;
	if (self.position == 1 || self.position == 2)
		player_diec1 ();
	else
	{
		if (self.health <= -40)
			player_dieb1 ();
		else
			player_diea1 ();
	}
};

void () set_suicide_frame =
{
	self.frame = TE_LAVASPLASH;
	self.solid = SOLID_NOT;
	self.movetype = MOVETYPE_TOSS;
	self.deadflag = DEAD_DEAD;
	self.nextthink = CONTENT_EMPTY;
};

void () player_diea1 = [ 1, player_diea2 ]
{
};

void () player_diea2 = [ 2, player_diea3 ]
{
};

void () player_diea3 = [ 3, player_diea4 ]
{
};

void () player_diea4 = [ 4, player_diea5 ]
{
};

void () player_diea5 = [ 5, player_diea6 ]
{
};

void () player_diea6 = [ 6, player_diea7 ]
{
};

void () player_diea7 = [ 7, player_diea8 ]
{
};

void () player_diea8 = [ 8, player_diea9 ]
{
};

void () player_diea9 = [ 9, player_diea10 ]
{
};

void () player_diea10 = [ 10, player_diea11 ]
{
};

void () player_diea11 = [ 11, player_diea12 ]
{
};

void () player_diea12 = [ 12, player_diea13 ]
{
};

void () player_diea13 = [ 13, player_diea13 ]
{
	PlayerDead ();
};

void () player_dieb1 = [ 94, player_dieb2 ]
{
};

void () player_dieb2 = [ 95, player_dieb3 ]
{
};

void () player_dieb3 = [ 96, player_dieb4 ]
{
};

void () player_dieb4 = [ 97, player_dieb5 ]
{
};

void () player_dieb5 = [ 98, player_dieb6 ]
{
};

void () player_dieb6 = [ 99, player_dieb7 ]
{
};

void () player_dieb7 = [ 100, player_dieb8 ]
{
};

void () player_dieb8 = [ 101, player_dieb9 ]
{
};

void () player_dieb9 = [ 102, player_dieb10 ]
{
};

void () player_dieb10 = [ 103, player_dieb11 ]
{
};

void () player_dieb11 = [ 104, player_dieb12 ]
{
};

void () player_dieb12 = [ 105, player_dieb13 ]
{
};

void () player_dieb13 = [ 106, player_dieb13 ]
{
	PlayerDead ();
};

void () player_diec1 = [ 55, player_diec2 ]
{
};

void () player_diec2 = [ 56, player_diec3 ]
{
};

void () player_diec3 = [ 57, player_diec4 ]
{
};

void () player_diec4 = [ 58, player_diec5 ]
{
};

void () player_diec5 = [ 59, player_diec6 ]
{
};

void () player_diec6 = [ 60, player_diec7 ]
{
};

void () player_diec7 = [ 61, player_diec8 ]
{
};

void () player_diec8 = [ 62, player_diec9 ]
{
};

void () player_diec9 = [ 63, player_diec10 ]
{
};

void () player_diec10 = [ 64, player_diec11 ]
{
};

void () player_diec11 = [ 65, player_diec12 ]
{
};

void () player_diec12 = [ 66, player_diec13 ]
{
};

void () player_diec13 = [ 67, player_diec13 ]
{
	PlayerDead ();
};
