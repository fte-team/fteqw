#define mag1 currentammo
#define maxmag1 cnt
.entity objective;

void () LobAGrenade;
void (vector jojo) spawn_civilian;
void (entity jeb, float time) spawn_excla;
void(float db) monster_noise;


void () find_hide;
void () army_load1;
void () army_radio1;
void () Footstep;
void () monster_commander;

void () DeathThroes =
{
	local entity te;

	te = findradius (self.origin, 300);
	while (te)
	{
		if (te.classname == "monster" && te.health > 0 && te != self)
			te.enemy = self;
		
		te = te.chain;
	}
};

//RADIO
void() fire_radio =
{
	if (world.map_obj == OBJ_SHADOW && self.has_radio == 1)
	{
		spawn_excla(self, 8);
		bprint(2, self.netname);
		bprint(2, " is calling for help! silence him!\n");
		self.rtime = 0;
		army_radio1();
		return;
	}
};


//PISTOL
void (float tmp, float dam) army_fire =
{
	local vector src;
	local vector dir;
	local vector direction;
	local entity en;
	local vector org;

	if (self.enemy.sneak == 1)
		tmp = tmp * 2;

	//soldiers call for help on shadow ops
	if (world.map_obj == OBJ_SHADOW && self.has_radio == 1)
	{
		spawn_excla(self, 8);
		bprint(2, self.netname);
		bprint(2, " is calling for help! silence him!\n");
		self.rtime = 0;
		army_radio1();
		return;
	}

	if (self.mag1 <= 0)
	{
		self.mag1 = 12;
		sound (self, CHAN_WEAPON, "misc/greload.wav", PLAT_LOW_TRIGGER, ATTN_NORM);
		army_load1();
		return;
	}

	self.mag1 = self.mag1 - 1;


	makevectors (self.angles);

	sound (self, CHAN_WEAPON, "weapons/usp.wav", PLAT_LOW_TRIGGER, ATTN_NORM);

	src = self.origin + v_forward*10;
	src_z = self.absmin_z + self.size_z * 0.7;

	en = self.enemy;
	
	dir = en.origin - en.velocity*0.2;
	if (en.position == 1)
		dir = dir - '0 0 8';
	if (en.position == 2)
		dir = dir - '0 0 16';

	dir = normalize (dir - self.origin);

	direction = dir;

	traceline (src, src + direction*2048 + v_right*crandom()*tmp + v_up*crandom()*tmp, FALSE, self);

	if (trace_fraction == 1)
		return;

	if (trace_ent.takedamage)
	{
		dam = 1 + random()*dam + random()*dam;
                dam = dam - (1 - (trace_fraction/2));
		SpawnBlood (org, PLAT_LOW_TRIGGER);
		T_Damage (trace_ent, self, self, dam);
	}
	else
		bullet_hole (trace_endpos);
};


//RIFLE
void (float tmp, float dam) army_fire1 =
{
	local vector src;
	local vector dir;
	local vector direction;
	local entity en;
	local vector org;

	if (self.enemy.sneak == 1)
		tmp = tmp * 2;

	//soldiers call for help on shadow ops
	if (world.map_obj == OBJ_SHADOW && self.has_radio == 1)
	{
		spawn_excla(self, 8);
		bprint(2, self.netname);
		bprint(2, " is calling for help! silence him!\n");
		self.rtime = 0;
		army_radio1();
		return;
	}

	monster_noise(60);

	if (self.mag1 <= 0)
	{
		self.mag1 = 1;
		sound (self, CHAN_WEAPON, "misc/greload.wav", PLAT_LOW_TRIGGER, ATTN_NORM);
		army_load1();
		return;
	}

	self.mag1 = self.mag1 - 1;

	makevectors (self.angles);

	sound (self, CHAN_WEAPON, "weapons/rangem.wav", PLAT_LOW_TRIGGER, ATTN_NORM);

	src = self.origin + v_forward*10;
	src_z = self.absmin_z + self.size_z * 0.7;

	en = self.enemy;
	
	dir = en.origin - en.velocity*0.05;
	if (en.position == 1)
		dir = dir - '0 0 8';
	if (en.position == 2)
		dir = dir - '0 0 16';

	dir = normalize (dir - self.origin);

	direction = dir;

	traceline (src, src + direction*2048 + v_right*crandom()*150 + v_up*crandom()*150, FALSE, self);

	if (trace_fraction == 1)
		return;

	if (trace_ent.takedamage)
	{
		SpawnBlood (org, PLAT_LOW_TRIGGER);
		dam = 10 + random()*dam + random()*dam;
		dam = dam * (1 - (trace_fraction/2));
		X_Damage (trace_ent, self, self, dam);
	}
	else
		bullet_hole (trace_endpos);
};


//SHOTGUN
void (float tmp, float dam) army_fire2 =
{
	local vector src;
	local vector dir;
	local vector direction;
	local entity en;
	local vector org;
	local float var_u, var_r, var_o;
	local float shot;

	if (self.enemy.sneak == 1)
		tmp = tmp * 2;

	if (self.mag1 <= 0)
	{
		self.mag1 = 6;
		sound (self, CHAN_WEAPON, "misc/greload.wav", PLAT_LOW_TRIGGER, ATTN_NORM);
		army_load1();
		return;
	}

	//soldiers call for help on shadow ops
	if (world.map_obj == OBJ_SHADOW && self.has_radio == 1)
	{
		spawn_excla(self, 8);
		bprint(2, self.netname);
		bprint(2, " is calling for help! silence him!\n");
		self.rtime = 0;
		army_radio1();
		return;
	}

	self.mag1 = self.mag1 - 1;

	makevectors (self.angles);

	sound (self, CHAN_WEAPON, "weapons/shotgun1.wav", PLAT_LOW_TRIGGER, ATTN_NORM);

	monster_noise(50);

	src = self.origin + v_forward*10;
	src_z = self.absmin_z + self.size_z * 0.7;

	en = self.enemy;
	
	dir = en.origin - en.velocity*0.2;
	if (en.position == 1)
		dir = dir - '0 0 8';
	if (en.position == 2)
		dir = dir - '0 0 16';

	dir = normalize (dir - self.origin);

	direction = dir;

	shot = 5;
	var_o = crandom()*50;

	while (shot > 0)
	{
			if (shot == 5)
			{
				var_r = 30;
				var_u = 30;
			}
			if (shot == 4)
			{
				var_r = -30;
				var_u = 30;
			}
			if (shot == 3)
			{
				var_r = 30;
				var_u = -30;
			}
			if (shot == 2)
			{
				var_r = -30;
				var_u = -30;
			}
			if (shot == 1)
			{
				var_r = 0;
				var_u = 0;
			}

			traceline (src, src + direction*2048 + v_right*(var_o+var_r) + v_up*(var_o+var_u), FALSE, self);

			shot = (shot - 1);

			if (trace_fraction == 1)
				return;

			if (trace_ent.takedamage)
			{
				SpawnBlood (org, PLAT_LOW_TRIGGER);
				dam = 1 + random()*dam + random()*dam;
				dam = dam * (1 - (trace_fraction/2));
				T_Damage (trace_ent, self, self, dam);
			}
			else
		bullet_hole (trace_endpos);
	}
};

//SMG
void (float tmp, float dam) army_fire3 =
{
	local vector src;
	local vector dir;
	local vector direction;
	local entity en;
	local vector org;

	if (self.mag1 <= 0)
	{
		self.mag1 = 30;
		sound (self, CHAN_WEAPON, "misc/greload.wav", 1, ATTN_NORM);
		army_load1();
		return;
	}

	//soldiers call for help on shadow ops
	if (world.map_obj == OBJ_SHADOW && self.has_radio == 1)
	{
		spawn_excla(self, 8);
		bprint(2, self.netname);
		bprint(2, " is calling for help! silence him!\n");
		self.rtime = 0;
		army_radio1();
		return;
	}

	monster_noise(40);

	if (self.enemy.sneak == 1)
		tmp = tmp * 2;

	sound (self, CHAN_WEAPON, "weapons/mp5.wav", PLAT_LOW_TRIGGER, ATTN_NORM);

	self.mag1 = self.mag1 - 1;

	makevectors (self.angles);

	src = self.origin + v_forward*10;
	src_z = self.absmin_z + self.size_z * 0.7;

	en = self.enemy;
	
	dir = en.origin - en.velocity*0.2;
	if (en.position == 1)
		dir = dir - '0 0 8';
	if (en.position == 2)
		dir = dir - '0 0 16';

	dir = normalize (dir - self.origin);

	direction = dir;

	traceline (src, src + direction*1024 + v_right*crandom()*tmp + v_up*crandom()*tmp, FALSE, self);

	if (trace_fraction == 1)
		return;

	if (trace_ent.takedamage)
	{
		SpawnBlood (org, PLAT_LOW_TRIGGER);
		dam = 1 + random()*dam + random()*dam;
		dam = dam * (1 - (trace_fraction/2));
		T_Damage (trace_ent, self, self, dam);
	}
	else
		bullet_hole (trace_endpos);

};

//Assault Rifle
void (float tmp, float dam) army_fire4 =
{
	local vector src;
	local vector dir;
	local vector direction;
	local entity en;
	local vector org;

	if (self.mag1 <= 0)
	{
		self.mag1 = 24;
		sound (self, CHAN_WEAPON, "misc/greload.wav", PLAT_LOW_TRIGGER, ATTN_NORM);
		army_load1();
		return;
	}


	self.mag1 = self.mag1 - 1;

	makevectors (self.angles);

	sound (self, CHAN_WEAPON, "weapons/ak47.wav", PLAT_LOW_TRIGGER, ATTN_NORM);

	src = self.origin + v_forward*10;
	src_z = self.absmin_z + self.size_z * 0.7;

	en = self.enemy;
	
	dir = en.origin - en.velocity*0.2;
	if (en.position == 1)
		dir = dir - '0 0 8';
	if (en.position == 2)
		dir = dir - '0 0 16';

	dir = normalize (dir - self.origin);

	direction = dir;

	traceline (src, src + direction*2048 + v_right*crandom()*tmp + v_up*crandom()*tmp, FALSE, self);

	monster_noise(60);

	if (trace_fraction == 1)
		return;

	if (trace_ent.takedamage)
	{
		SpawnBlood (org, PLAT_LOW_TRIGGER);
		dam = 1 + random()*dam + random()*dam;
		dam = dam * (1 - (trace_fraction/2));
		T_Damage (trace_ent, self, self, dam);
	}
	else
	{
		if (en.classname == "player" && random()*20 <= 5)
			stuffcmd(en, "play effects/miss\n");

		bullet_hole (trace_endpos);
	}
};

void () army_run1;

void () army_radio1 = [ 29, army_radio2 ]
{
	if (self.health <= 0)
	{
		self.think = SUB_Null;
		return;
	}

	self.rtime = self.rtime + 1;
	ai_stand ();
};

void () army_radio2 = [ 29, army_radio3 ]
{
	ai_stand ();
};

void () army_radio3 = [ 29, army_radio1 ]
{
	if (self.rtime > 15)
	{
		if (random()<0.25)
			sound (self, CHAN_VOICE, "effects/radio1.wav", 1, ATTN_NONE);
		else if (random()<0.25)
			sound (self, CHAN_VOICE, "effects/radio2.wav", 1, ATTN_NONE);
		else if (random()<0.25)
			sound (self, CHAN_VOICE, "effects/radio3.wav", 1, ATTN_NONE);
		else
			sound (self, CHAN_VOICE, "effects/radio4.wav", 1, ATTN_NONE);

		called_in += 1;
		self.has_radio = 0;
		army_run1();
		bprint(2, "soldier reported suspicious activity!\n");
		if (called_in == 1)
			bprint(2, "threat level: low\n");
		if (called_in == 2)
			bprint(2, "threat level: rising\n");
		if (called_in == 3)
			bprint(2, "threat level: alert\n");
		if (called_in == 4)
			bprint(2, "threat level: paranoid\n");
		if (called_in >= 5)
			bprint(2, "threat level: discovered\n");

	}
	else
	{
		if (random()<0.5)
			sound (self, CHAN_VOICE, "effects/blip1.wav", 1, ATTN_IDLE);
		else
			sound (self, CHAN_VOICE, "effects/blip2.wav", 1, ATTN_IDLE);
	}
	ai_stand ();
};




void () army_load1 = [ 29, army_load2 ]
{
	ai_stand ();
};
void () army_load2 = [ 29, army_load3 ]
{
	ai_stand ();
};
void () army_load3 = [ 30, army_load4 ]
{
	ai_stand ();
};
void () army_load4 = [ 30, army_load5 ]
{
	ai_stand ();
};
void () army_load5 = [ 31, army_load6 ]
{
	ai_stand ();
};
void () army_load6 = [ 31, army_load7 ]
{
	ai_stand ();
};
void () army_load7 = [ 32, army_load8 ]
{
	ai_stand ();
};
void () army_load8 = [ 32, army_load9 ]
{
	ai_stand ();
};
void () army_load9 = [ 33, army_load10 ]
{
	ai_stand ();
};
void () army_load10 = [ 33, army_load11 ]
{
	ai_stand ();
};
void () army_load11 = [ 34, army_load12 ]
{
	ai_stand ();
};

void () army_load12 = [ 34, army_load13 ]
{
	ai_stand ();
};
void () army_load13 = [ 35, army_load14 ]
{
	ai_stand ();
};
void () army_load14 = [ 35, army_load15 ]
{
	ai_stand ();
};
void () army_load15 = [ 36, army_load16 ]
{
	ai_stand ();
};
void () army_load16 = [ 36, army_load17 ]
{
	ai_stand ();
};
void () army_load17 = [ 37, army_load18 ]
{
	ai_stand ();
};
void () army_load18 = [ 37, army_load19 ]
{
	ai_stand ();
};
void () army_load19 = [ 38, army_load20 ]
{
	ai_stand ();
};
void () army_load20 = [ 38, army_load21 ]
{
	ai_stand ();
};
void () army_load21 = [ 39, army_load22 ]
{
	ai_stand ();
};
void () army_load22 = [ 39, army_run1 ]
{
	ai_stand ();
};

void () army_stand1 = [ 0, army_stand2 ]
{
	ai_stand ();
};

void () army_stand2 = [ 1, army_stand3 ]
{
	ai_stand ();
};

void () army_stand3 = [ 2, army_stand4 ]
{
	ai_stand ();
};

void () army_stand4 = [ 3, army_stand5 ]
{
	ai_stand ();
};

void () army_stand5 = [ 4, army_stand6 ]
{
	ai_stand ();
};

void () army_stand6 = [ 5, army_stand7 ]
{
	ai_stand ();
};

void () army_stand7 = [ 6, army_stand8 ]
{
	ai_stand ();
};

void () army_stand8 = [ 7, army_stand1 ]
{
	ai_stand ();
};

void () army_walk1 = [ 73, army_walk2 ]
{
	ai_walk (TE_TELEPORT);
};

void () army_walk2 = [ 74, army_walk3 ]
{
	ai_walk (15);
};

void () army_walk3 = [ 75, army_walk4 ]
{
	ai_walk (TE_LAVASPLASH);
};

void () army_walk4 = [ 76, army_walk5 ]
{
	ai_walk (TE_LAVASPLASH);
};

void () army_walk5 = [ 77, army_walk6 ]
{
	ai_walk (SECRET_NO_SHOOT);
};

void () army_walk6 = [ 78, army_walk7 ]
{
	ai_walk (15);
};

void () army_walk7 = [ 79, army_walk8 ]
{
	ai_walk (TE_LAVASPLASH);
};

void () army_walk8 = [ 80, army_walk1 ]
{
	ai_walk (SECRET_NO_SHOOT);
};

void () army_run1 = [ 73, army_run2 ]
{

	if ((random()<0.25 || self.health <= 25) && self.charmed != 1)
		find_hide();

	ai_run (TE_TELEPORT);
};

void () army_run2 = [ 74, army_run3 ]
{
	ai_run (15);
};

void () army_run3 = [ 75, army_run4 ]
{
	if (world.map_obj == OBJ_SHADOW && self.has_radio == 1)
	{
		spawn_excla(self, 8);
		bprint(2, self.netname);
		bprint(2, " is calling for help! silence him!\n");
		self.rtime = 0;
		army_radio1();
		return;
	}

	ai_run (TE_LAVASPLASH);
};

void () army_run4 = [ 76, army_run5 ]
{
	ai_run (TE_LAVASPLASH);
};

void () army_run5 = [ 77, army_run6 ]
{
	ai_run (SECRET_NO_SHOOT);
};

void () army_run6 = [ 78, army_run7 ]
{
	ai_run (15);
};

void () army_run7 = [ 79, army_run8 ]
{
	ai_run (TE_LAVASPLASH);
};

void () army_run8 = [ 80, army_run1 ]
{
	ai_run (SECRET_NO_SHOOT);
};

void () army_atk1 = [ 81, army_atk2 ]
{
	if (self.grenadetoggle > 0 && random()*4 <= 1)
	{
		LobAGrenade();
		self.grenadetoggle = self.grenadetoggle - 1;
	}

	ai_face ();
};

void () army_atk2 = [ 82, army_atk3 ]
{
	ai_face ();
};

void () army_atk3 = [ 83, army_atk4 ]
{
	ai_face ();
};

void () army_atk4 = [ 84, army_atk5 ]
{
	ai_face ();
};

void () army_atk5 = [ 85, army_atk6 ]
{
	ai_face ();
};

void () army_atk6 = [ 86, army_atk7 ]
{
	ai_face ();
};

void () army_atk7 = [ 87, army_atk8 ]
{
	ai_face ();
	army_fire (170, 19);
};

void () army_atk8 = [ 88, army_atk9 ]
{
	ai_face ();
};

void () army_atk9 = [ 89, army_run1 ]
{
	ai_face ();
};

void () army_atka1 = [ 81, army_atka2 ]
{
	if (self.grenadetoggle > 0 && self.health < 70)
	{
		LobAGrenade();
		self.grenadetoggle = self.grenadetoggle - 1;
	}

	ai_face ();
};

void () army_atka2 = [ 82, army_atka3 ]
{
	ai_face ();
};

void () army_atka3 = [ 83, army_atka4 ]
{
	ai_face ();
};

void () army_atka4 = [ 84, army_atka5 ]
{
	ai_face ();
};

void () army_atka5 = [ 85, army_atka6 ]
{
	ai_face ();
	army_fire2 (250, 7);
};

void () army_atka6 = [ 86, army_atka7 ]
{
	ai_face ();
};

void () army_atka7 = [ 87, army_atka8 ]
{
	ai_face ();
};

void () army_atka8 = [ 88, army_atka9 ]
{
	ai_face ();
};

void () army_atka9 = [ 89, army_run1 ]
{
	ai_face ();
};

void () army_atkb1 = [ 81, army_atkb2 ]
{
	if (self.grenadetoggle > 0 && self.health < 70)
	{
		LobAGrenade();
		self.grenadetoggle = self.grenadetoggle - 1;
	}

	ai_face ();
};

void () army_atkb2 = [ 82, army_atkb4 ]
{
	ai_face ();
};

void () army_atkb3 = [ 83, army_atkb4 ]
{
	ai_face ();
};

void () army_atkb4 = [ 84, army_atkb5 ]
{
	ai_face ();
};

void () army_atkb5 = [ 85, army_atkb6 ]
{
	local float r;

	ai_face ();
	r = range (self.enemy);

	self.recoil = 0;

	if (r == RANGE_NEAR || r == RANGE_MELEE)
		self.recoil = 1;

	army_fire3 (120, 15);
};

void () army_atkb6 = [ 86, army_atkb7 ]
{
	ai_face ();
	if (self.recoil == 1)
	{
		army_fire3 (160, 14);
		army_fire3 (160, 14);
	}
};

void () army_atkb7 = [ 87, army_atkb8 ]
{
	ai_face ();
	if (self.recoil == 1)
		army_fire3 (160, 14);
};

void () army_atkb8 = [ 88, army_atkb9 ]
{
	ai_face ();
	if (self.recoil == 1)
	{
		army_fire3 (160, 14);
		army_fire3 (160, 14);
	}
};

void () army_atkb9 = [ 89, army_run1 ]
{
	ai_face ();
	if (self.recoil == 1)
	{
		army_fire3 (170, 14);
		army_fire3 (170, 14);
	}
};

void () army_atkcs1 = [ 81, army_atkcs2 ]
{
	if (self.grenadetoggle > 0 && self.health < 70)
	{
		LobAGrenade();
		self.grenadetoggle = self.grenadetoggle - 1;
	}

	ai_face ();
};

void () army_atkcs2 = [ 82, army_atkcs3 ]
{
	ai_face ();
};

void () army_atkcs3 = [ 83, army_atkcs4 ]
{
	ai_face ();
};

void () army_atkcs4 = [ 84, army_atkcs5 ]
{
	ai_face ();
};

void () army_atkcs5 = [ 85, army_atkcs6 ]
{
	ai_face ();
	sound (self, CHAN_WEAPON, "weapons/ak47.wav", PLAT_LOW_TRIGGER, ATTN_NORM);
	army_fire4 (100, 20);
};

void () army_atkcs6 = [ 85, army_atkcs7 ]
{

	ai_face ();
};

void () army_atkcs7 = [ 87, army_atkcs8 ]
{

	ai_face ();
};

void () army_atkcs8 = [ 88, army_atkcs9 ]
{

	ai_face ();
};

void () army_atkcs9 = [ 89, army_run1 ]
{
	ai_face ();
};

void () army_atkc1 = [ 81, army_atkc2 ]
{
	ai_face ();
};

void () army_atkc2 = [ 82, army_atkc3 ]
{
	ai_face ();
};

void () army_atkc3 = [ 83, army_atkc4 ]
{
	ai_face ();
};

void () army_atkc4 = [ 84, army_atkc5 ]
{
	ai_face ();
};

void () army_assault_rifle =
{
	local float r;

	r = range (self.enemy);

	if (r == RANGE_FAR)//single shot at range
		army_atkcs1();
	else//full-auto
		army_atkc1();
};

void () army_atkc5 = [ 85, army_atkc6 ]
{
	ai_face ();

	sound (self, CHAN_WEAPON, "weapons/auto.wav", PLAT_LOW_TRIGGER, ATTN_NORM);
	army_fire4 (100, 16);
};

void () army_atkc6 = [ 84, army_atkc7 ]
{
	ai_face ();
	army_fire4 (140, 16);
};

void () army_atkc7 = [ 85, army_atkc8 ]
{
	ai_face ();
	army_fire4 (180, 16);
};

void () army_atkc8 = [ 84, army_atkc9 ]
{
	ai_face ();
	army_fire4 (220, 16);
};

void () army_atkc9 = [ 85, army_atkc10 ]
{
	ai_face ();
	army_fire4 (260, 16);
};

void () army_atkc10 = [ 84, army_atkc11 ]
{
	ai_face ();
	army_fire4 (300, 16);
};

void () army_atkc11 = [ 85, army_atkc12 ]
{
	ai_face ();
	army_fire4 (340, 16);
};

void () army_atkc12 = [ 84, army_atkc13 ]
{
	ai_face ();
	army_fire4 (380, 16);
};

void () army_atkc13 = [ 86, army_atkc14 ]
{
	ai_face ();
};

void () army_atkc14 = [ 87, army_atkc15 ]
{
	ai_face ();
};

void () army_atkc15 = [ 88, army_atkc16 ]
{
	ai_face ();
};

void () army_atkc16 = [ 89, army_run1 ]
{
	ai_face ();
};

void () army_atkp1 = [ 81, army_atkp2 ]
{
	if (self.grenadetoggle > 0 && self.health < 70)
	{
		LobAGrenade();
		self.grenadetoggle = self.grenadetoggle - 1;
	}

	ai_face ();
};

void () army_atkp2 = [ 82, army_atkp3 ]
{
	ai_face ();
};

void () army_atkp3 = [ 83, army_atkp4 ]
{
	ai_face ();
};

void () army_atkp4 = [ 84, army_atkp5 ]
{
	ai_face ();
};

void () army_atkp5 = [ 85, army_atkp6 ]
{
	ai_face ();
	army_fire1 (300, 22);
};

void () army_atkp6 = [ 86, army_atkp7 ]
{
	ai_face ();
};

void () army_atkp7 = [ 87, army_atkp8 ]
{
	ai_face ();
};

void () army_atkp8 = [ 88, army_atkp9 ]
{
	ai_face ();
};

void () army_atkp9 = [ 89, army_run1 ]
{
	ai_face ();
};

void () army_pain1 = [ 40, army_pain2 ]
{
};

void () army_pain2 = [ 41, army_pain3 ]
{
};

void () army_pain3 = [ 42, army_pain4 ]
{
};

void () army_pain4 = [ 43, army_pain5 ]
{
};

void () army_pain5 = [ 44, army_pain6 ]
{
};

void () army_pain6 = [ 45, army_run1 ]
{
	ai_pain (PLAT_LOW_TRIGGER);
};

void () army_painb1 = [ 46, army_painb2 ]
{
};

void () army_painb2 = [ 47, army_painb3 ]
{
	ai_painforward (TE_LIGHTNINGBLOOD);
};

void () army_painb3 = [ 48, army_painb4 ]
{
	ai_painforward (TE_LIGHTNING3);
};

void () army_painb4 = [ 49, army_painb5 ]
{
};

void () army_painb5 = [ 50, army_painb6 ]
{
};

void () army_painb6 = [ 51, army_painb7 ]
{
};

void () army_painb7 = [ 52, army_painb8 ]
{
};

void () army_painb8 = [ 53, army_painb9 ]
{
};

void () army_painb9 = [ 54, army_painb10 ]
{
};

void () army_painb10 = [ 55, army_painb11 ]
{
};

void () army_painb11 = [ 56, army_painb12 ]
{
};

void () army_painb12 = [ 57, army_painb13 ]
{
	ai_pain (SILENT);
};

void () army_painb13 = [ 58, army_painb14 ]
{
};

void () army_painb14 = [ 59, army_run1 ]
{
};

void () army_painc1 = [ 60, army_painc2 ]
{
};

void () army_painc2 = [ 61, army_painc3 ]
{
	ai_pain (PLAT_LOW_TRIGGER);
};

void () army_painc3 = [ 62, army_painc4 ]
{
};

void () army_painc4 = [ 63, army_painc5 ]
{
};

void () army_painc5 = [ 64, army_painc6 ]
{
	ai_painforward (PLAT_LOW_TRIGGER);
};

void () army_painc6 = [ 65, army_painc7 ]
{
	ai_painforward (PLAT_LOW_TRIGGER);
};

void () army_painc7 = [ 66, army_painc8 ]
{
};

void () army_painc8 = [ 67, army_painc9 ]
{
	ai_pain (PLAT_LOW_TRIGGER);
};

void () army_painc9 = [ 68, army_painc10 ]
{
	ai_painforward (SECRET_1ST_DOWN);
};

void () army_painc10 = [ 69, army_painc11 ]
{
	ai_painforward (AS_MELEE);
};

void () army_painc11 = [ 70, army_painc12 ]
{
	ai_painforward (TE_LIGHTNING2);
};

void () army_painc12 = [ 71, army_painc13 ]
{
	ai_painforward (SECRET_NO_SHOOT);
};

void () army_painc13 = [ 72, army_run1 ]
{
};

void (entity attacker, float damage) army_pain =
{
	local float r;

	if ((self.pain_finished > time))
	{
		return;
	}
	r = random ();
	if ((r < 0.05))
	{
		self.pain_finished = (time + 0.6);
		army_pain1 ();
		sound (self, CHAN_VOICE, "player/paina.wav", PLAT_LOW_TRIGGER, ATTN_NORM);
	}
	else
	{
		if ((r < 0.1))
		{
			self.pain_finished = (time + 1.1);
			army_painb1 ();
			sound (self, CHAN_VOICE, "player/painb.wav", PLAT_LOW_TRIGGER, ATTN_NORM);
		}
	}
};

void () army_die1 = [ 8, army_die2 ]
{

	if (random()*4 <= 2)
		sound (self, CHAN_VOICE, "player/agdie2.wav", 1, ATTN_NORM);
	else
		sound (self, CHAN_VOICE, "player/agdie3.wav", 1, ATTN_NORM);

};

void () army_die2 = [ 9, army_die3 ]
{
	self.rtime = 1;
	self.velocity_z = 50+random()*50;
};

void () army_die3 = [ 10, army_die4 ]
{
};

void () army_die4 = [ 11, army_die5 ]
{
	setsize (self, '-12 -12 -24', '12 12 -16');
};

void () army_die5 = [ 12, army_die6 ]
{
};

void () army_die6 = [ 13, army_die7 ]
{
};

void () army_die7 = [ 14, army_die8 ]
{
};

void () army_die8 = [ 15, army_die9 ]
{
};

void () army_die9 = [ 16, army_die10 ]
{
};

void () army_die10 = [ 17, army_die10 ]
{
};

void () army_cdie1 = [ 18, army_cdie2 ]
{
	if (random()*4 <= 2)
		sound (self, CHAN_VOICE, "player/agdie2.wav", 1, ATTN_NORM);
	else
		sound (self, CHAN_VOICE, "player/agdie3.wav", 1, ATTN_NORM);
};

void () army_cdie2 = [ 19, army_cdie3 ]
{
	self.rtime = 1;
	self.velocity_z = 50+random()*50;
	ai_back (MULTICAST_PVS_R);
};

void () army_cdie3 = [ 20, army_cdie4 ]
{
	ai_back (SECRET_1ST_DOWN);
};

void () army_cdie4 = [ 21, army_cdie5 ]
{
	setsize (self, '-12 -12 -24', '12 12 -16');
	ai_back (TE_LIGHTNINGBLOOD);
};

void () army_cdie5 = [ 22, army_cdie6 ]
{
	ai_back (AS_MELEE);
};

void () army_cdie6 = [ 23, army_cdie7 ]
{
	ai_back (SECRET_1ST_DOWN);
};

void () army_cdie7 = [ 24, army_cdie8 ]
{
};

void () army_cdie8 = [ 25, army_cdie9 ]
{
};

void () army_cdie9 = [ 26, army_cdie10 ]
{
};

void () army_cdie10 = [ 27, army_cdie11 ]
{
};

void () army_cdie11 = [ 28, army_cdie11 ]
{
};


void () grunt_pain =
{
	local vector dir;

	if (self.rtime > 0)
	{
		if (random()*100<10)
		{
			remove(self);
			return;
		}
		if (random()*100<50)
			sound (self, CHAN_VOICE, "player/headshot.wav", 0.75, ATTN_NORM);
		else
			sound (self, CHAN_VOICE, "misc/thud.wav", 1, ATTN_NORM);

		SpawnBlood (self.origin + '0 0 8', 4);

		self.health = 180;
		return;
	}

	dir = normalize(self.origin - self.enemy.origin);
	self.velocity = dir * 120;

	if (self.frame == 41)
	{
		self.health = 180;
		self.frame = 42;
		self.think = army_die1;
		self.nextthink = time + 0.12;
	}
	else
	{
		self.health = 180;
		ThrowGib ("progs/zom_gib.mdl", -40);
		self.frame = 41;
		self.think = army_cdie1;
		self.nextthink = time + 0.12;
	}

	self.attack = self.attack + 1;

	if (self.attack >= 16)
	{
		self.solid = SOLID_NOT;

		army_cdie1();
		ThrowGib ("progs/zom_gib.mdl", -35);
		ThrowGib ("progs/zom_gib.mdl", -35);
		ThrowGib ("progs/gib1.mdl", -35);
	}
};

void (vector stuff, vector ang) spawn_live_grunt =
{
	local entity grunt;

	grunt = spawn ();
	grunt.origin = stuff;
	grunt.enemy = self.enemy;
	grunt.attack_finished = time + 10;
	grunt.solid = SOLID_SLIDEBOX;
	grunt.movetype = MOVETYPE_STEP;
	grunt.takedamage = DAMAGE_YES;
	setmodel (grunt, "progs/soldier.mdl");
	setsize (grunt, '-12 -12 -24', '12 12 28');
	grunt.classname = "body";
	grunt.netname = "dead";
	grunt.health = 180;
	grunt.angles = ang;
	grunt.max_health = grunt.health;
	grunt.th_pain = grunt_pain;
	grunt.think = grunt_pain;
	grunt.th_die = corpse_gib;
	grunt.nextthink = time + 0.05;
};

void () army_die =
{
	local float r;

	r = random();

	if (random()<0.75)
		DropMoney();



	DeathThroes();

	sound (self, CHAN_BODY, "", 1, ATTN_NORM);

	if (self.weapon == 1)
		DropFromChest(self, IID_WP_PIPERIFLE, 1);
	if (self.weapon == 2)
		DropFromChest(self, IID_WP_GLOCK, 6);
	if (self.weapon == 3)
		DropFromChest(self, IID_WP_WINCHESTER, 1);
	if (self.weapon == 4)
		DropFromChest(self, IID_WP_GREASEGUN, 15);
	if (self.weapon == 5)
		DropFromChest(self, IID_WP_AK47, 12);

	if (self.weapon == 1)
		DropFromChest(self, IID_AM_44MAGNUM, 10);
	if (self.weapon == 2)
		DropFromChest(self, IID_AM_10MM, 12);
	if (self.weapon == 3)
		DropFromChest(self, IID_AM_12GAUGESHELLS, 8);
	if (self.weapon == 4)
		DropFromChest(self, IID_AM_45ACP, 30);
	if (self.weapon == 5)
		DropFromChest(self, IID_AM_5MMHIGHVEL, 24);

	self.solid = SOLID_NOT;
	spawn_live_grunt(self.origin, self.angles);
	remove(self);
};

void (vector jojo, entity friend) spawn_soldier =
{
	local entity soldier;
	local entity te;


	soldier = spawn ();
	self = soldier;
	self.solid = SOLID_SLIDEBOX;
	self.movetype = MOVETYPE_STEP;
	setmodel (self, "progs/soldier.mdl");
	setsize (self, '-12 -12 -24', '12 12 28');
	self.netname = "raider";
	self.max_health = 80;
	self.health = self.max_health;
	self.armornoise = "misc/thud.wav";
	self.th_stand = army_stand1;
	self.th_walk = army_walk1;
	self.th_run = army_run1;
	if (random()<0.3)
		self.has_radio = 1;
	if (self.has_radio == 0)
		self.th_missile = army_atk1;
	if (self.has_radio == 1)
		self.th_missile = army_radio1;
	self.th_pain = army_pain;
	self.th_die = army_die;

	self.origin = jojo + '0 64 0';
	self.origin_z = self.origin_z + 1;
	droptofloor();

	if (!walkmove(0, 0))
	{
		self.origin = jojo + '0 -64 0';
		self.origin_z = self.origin_z + 1;
		droptofloor();
		if (!walkmove(0, 0))
		{
			self.origin = jojo + '64 0 0';
			self.origin_z = self.origin_z + 1;
			droptofloor();
			if (!walkmove(0, 0))
			{
				self.origin = jojo + '-64 0 0';
				self.origin_z = self.origin_z + 1;
				droptofloor();
				if (!walkmove(0, 0))
				{
					remove(self);
					return;
				}
			}
		}
	}

	te = findradius (self.origin, 30);
	while (te)
	{
		if (te.classname == "player" || te.classname == "monster" && te.health > 0)
		{
			remove(self);
			return;
		}


		te = te.chain;
	}


//soldiers have either pistol, smg, shotgun or rifle
	self.weapon = ceil(random()*4);

	if (self.weapon == 1)
		self.th_missile = army_atkp1; //rifle
	if (self.weapon == 2)
		self.th_missile = army_atk1; //pistol
	if (self.weapon == 3)
		self.th_missile = army_atka1; //shotgun
	if (self.weapon == 4)
		self.th_missile = army_atkb1; //smg

		self.mag1 = 8;

	if (self.weapon == 1)
		self.mag1 = 6+random()*6;
	if (self.weapon == 2)
		self.mag1 = 12+random()*12;
	if (self.weapon == 3)
		self.mag1 = 3+random()*3;
	if (self.weapon == 4)
		self.mag1 = 15+random()*15;

	self.maxmag1 = self.mag1;

	self.classname = "monster";
	walkmonster_start_go ();
	self.angles_y = friend.angles_y;
};

void () monster_army =
{
	local float com;
	local entity te;

	if (self.zone == 0)
	{
		load_monster();
		return;
	}

	local float x;
	local entity te;

	x = total_players;

	te = find(world, classname, "monster");
	while (te)
	{
		if (te.netname == "commander")
			com = com + 1;

		te = find(te, classname, "monster");
	}

	if (com == 0 && random()<0.50 && world.map_obj == OBJ_SHADOW)
	{
			monster_commander();
			return;
	}

	else if (com == 1 && random()<0.25 && world.map_obj == OBJ_SHADOW)
	{
			monster_commander();
			return;
	}

	if (random()*10 <= x*0.5)
	{	
		precache_model2 ("progs/enforcer.mdl");
		precache_model2 ("progs/h_mega.mdl");
		precache_model2 ("progs/laser.mdl");

		precache_sound2 ("enforcer/death1.wav");
		precache_sound2 ("enforcer/enfire.wav");
		precache_sound2 ("enforcer/enfstop.wav");
		precache_sound2 ("enforcer/idle1.wav");
		precache_sound2 ("enforcer/pain1.wav");
		precache_sound2 ("enforcer/pain2.wav");
		precache_sound2 ("enforcer/sight1.wav");
		precache_sound2 ("enforcer/sight2.wav");
		precache_sound2 ("enforcer/sight3.wav");
		precache_sound2 ("enforcer/sight4.wav");
	
		self.solid = SOLID_SLIDEBOX;
		self.movetype = MOVETYPE_STEP;
		self.team = 3;

		setmodel (self, "progs/enforcer.mdl");
		self.classname = "monster";
		self.netname = "enforcer";

		setsize (self, '-12 -12 -24', '12 12 28');
		self.max_health = 150;
		self.health = self.max_health;
          	  self.islot3 = SlotVal(IID_ARM_COMBAT, 1);
		self.armortype = 35;
		self.helmet = 1;
		self.th_stand = enf_stand1;
		self.th_walk = enf_walk1;
		self.th_run = enf_run1;
		self.th_pain = enf_pain;
		self.th_die = enf_die;
		self.th_missile = enf_atk1;
		self.armornoise = "misc/thud.wav";

		walkmonster_start();
		if (random()*4 <= 2)
			self.weapon = 5;
		else
			self.weapon = 6;
		self.th_missile = enf_atk1;
		self.mag1 = 24;
		self.maxmag1 = self.mag1;
		return;
	}

	precache_model ("progs/soldier.mdl");
	precache_model ("progs/h_guard.mdl");
	precache_model ("progs/gib1.mdl");
	precache_model ("progs/gib2.mdl");
	precache_model ("progs/gib3.mdl");
	precache_sound ("soldier/death1.wav");
	precache_sound ("soldier/idle.wav");
	precache_sound ("soldier/pain1.wav");
	precache_sound ("soldier/pain2.wav");
	precache_sound ("soldier/sattck1.wav");
	precache_sound ("soldier/sight1.wav");
	precache_sound ("player/udeath.wav");

	self.ammo_shells = 1;
	self.solid = SOLID_SLIDEBOX;
	self.classname = "monster";
	self.netname = "raider";
	self.movetype = MOVETYPE_STEP;
	setmodel (self, "progs/soldier.mdl");
	setsize (self, '-12 -12 -24', '12 12 28');
	self.max_health = 80;
	self.health = self.max_health;
	self.team = 3;
      self.islot3 = SlotVal(IID_ARM_LEATHER, 1);
	self.armortype = 0.2;
	self.armornoise = "misc/thud.wav";
	self.th_stand = army_stand1;
	self.th_walk = army_walk1;
	self.th_run = army_run1;
	if (random()<0.3)
		self.has_radio = 1;
	if (self.has_radio == 0)
		self.th_missile = army_atk1;
	if (self.has_radio == 1)
		self.th_missile = army_radio1;

	self.th_pain = army_pain;
	self.th_die = army_die;

	te = find (world, classname, "rhostage");
	self.movetarget = te;

	walkmonster_start ();
	
//soldiers have either 0, 1 or 2 grenades
	self.grenadetoggle = floor(random()*2);

//soldiers have either pistol, smg, shotgun or rifle

	if (random()*100 >= 80)
		self.weapon = ceil(random()*4);
	else
		self.weapon = 2;

	if (self.weapon == 1)
		self.th_missile = army_atkp1; //pipe rifle
	if (self.weapon == 2)
		self.th_missile = army_atk1; //pistol
	if (self.weapon == 3)
		self.th_missile = army_atka1; //shotgun
	if (self.weapon == 4)
		self.th_missile = army_atkb1; //smg

		self.mag1 = 8;

	if (self.weapon == 1)
		self.mag1 = 1;
	if (self.weapon == 2)
		self.mag1 = 6+random()*6;
	if (self.weapon == 3)
		self.mag1 = 3+random()*3;
	if (self.weapon == 4)
		self.mag1 = 15+random()*15;

//soldiers on shadow missions have full combat armor & assault rifles
	if (world.map_obj == OBJ_SHADOW)
	{
		if (random()<0.5)
     			self.islot3 = SlotVal(IID_ARM_LEATHER, 1);
		else if (random()<0.5)
     			self.islot3 = SlotVal(IID_ARM_METAL, 1);
		else
     			self.islot3 = SlotVal(IID_ARM_COMBAT, 1);

		if (random()<0.25)
			self.weapon = 3;
		else if (random()<0.5)
			self.weapon = 4;
		else
			self.weapon = 5;

	if (self.weapon == 3)
		self.th_missile = army_atka1; //shotgun
	if (self.weapon == 4)
		self.th_missile = army_atkb1; //smg
	if (self.weapon == 5)
		self.th_missile = army_assault_rifle; //ak-112 assault rifle

		self.mag1 = 24;
	}

	self.maxmag1 = self.mag1;

	if (random()<0.25 && (world.map_obj != OBJ_SHADOW))
		spawn_soldier(self.origin, self);
	if (random()<0.25)
		spawn_civilian(self.origin);
};

void() WalkTo_Think =
{
        local entity ent;

        ent = nextent(world);

        while(ent)
        {
                if((ent.goalentity == self) || (ent.objective == self))
                {
                        self.nextthink = time + 2;
                        return;
                }
                ent = nextent(ent);
        }

	  remove(self);

};


float(vector vec) hide_search =
{
	local float flo;
	local vector vic;
	local entity ent;

	flo = random() * 1000;

	traceline(self.origin, self.origin + vec * flo, TRUE, self);
	vic = trace_endpos - normalize(trace_endpos - self.origin) * 32;
	traceline(vic, self.enemy.origin, TRUE, self);

	if (vlen(trace_endpos - self.enemy.origin) > 50)
	{
		makevectors(vec);

		ent = spawn();
//		setmodel(ent, "progs/lavaball.mdl");
		setorigin(ent, vic - v_forward * 8);
	      ent.think = WalkTo_Think;
	      ent.nextthink = time + 8;
		ent.classname = "runner";
		
//		self.objective = ent;
		self.angles_y = vectoyaw(ent.origin - self.origin);
		self.goalentity = ent;
//		self.think = self.th_run;
		return TRUE;
	}
	
	return FALSE;
};


void() find_hide =
{
	local float flo;
	local vector vec, vac;

	flo = 5;
	makevectors(self.angles);
	while(flo)
	{
		vec = crandom() * v_right;
		vac = crandom() * v_forward;
		vec = vec + vac;
		if (hide_search(vec))
			return;
		flo = flo - 1;
	}
};


void () monster_commander =
{
	if (self.zone == 0)
	{
		load_monster();
		return;
	}

	precache_model2 ("progs/enforcer.mdl");
	precache_model2 ("progs/h_mega.mdl");
	precache_model2 ("progs/laser.mdl");

	precache_sound2 ("enforcer/death1.wav");
	precache_sound2 ("enforcer/enfire.wav");
	precache_sound2 ("enforcer/enfstop.wav");
	precache_sound2 ("enforcer/idle1.wav");
	precache_sound2 ("enforcer/pain1.wav");
	precache_sound2 ("enforcer/pain2.wav");
	precache_sound2 ("enforcer/sight1.wav");
	precache_sound2 ("enforcer/sight2.wav");
	precache_sound2 ("enforcer/sight3.wav");
	precache_sound2 ("enforcer/sight4.wav");
	
	self.solid = SOLID_SLIDEBOX;
	self.movetype = MOVETYPE_STEP;
	self.team = 3;

	setmodel (self, "progs/enforcer.mdl");
	self.classname = "monster";
	self.netname = "commander";
	commanders = commanders + 1;

	setsize (self, '-12 -12 -24', '12 12 32');
	self.max_health = 200;
	self.health = self.max_health;
      self.islot3 = SlotVal(IID_ARM_COMBAT, 1);
	self.armortype = 0.5;
	self.helmet = 1;
	self.th_stand = enf_stand1;
	self.th_walk = enf_walk1;
	self.th_run = enf_run1;
	self.th_pain = enf_pain;
	self.th_die = enf_die;
	self.th_missile = enf_atk1;

	if (random()<0.9)
		self.weapon = 5;
	else if (random()<0.1)
		self.weapon = 3;
	else if (random()<0.1)
		self.weapon = 4;

	else if (random()<0.1)
		self.weapon = 6;
	else
		self.weapon = 5;

	self.th_missile = enf_atk1;
	self.mag1 = 24;
	self.maxmag1 = self.mag1;

	walkmonster_start();

	spawn_soldier(self.origin, self);
	spawn_soldier(self.origin, self);
	spawn_soldier(self.origin, self);
	spawn_soldier(self.origin, self);

	return;

};


#undef mag1
#undef maxmag1
