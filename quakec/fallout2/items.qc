void() W_SetCurrentAmmo;
/* ALL LIGHTS SHOULD BE 0 1 0 IN COLOR ALL OTHER ITEMS SHOULD
BE .8 .3 .4 IN COLOR */

void(entity to, float iid, float count) AddStackable;
void (entity jeb, float time) spawn_excla;
void() AmmoTouch;
void() shop_face;

void() SUB_regen =
{
	self.model = self.mdl;          // restore original model
	self.solid = SOLID_TRIGGER;     // allow it to be touched again
	sound (self, CHAN_VOICE, "items/itembk2.wav", 1, ATTN_NORM);    // play respawn sound
	setorigin (self, self.origin);
};

float() crandom;

void () Levelshake =
{
	local entity te;

	te = findradius (self.origin, 1400);
	while (te)
	{
		if (te.classname == "player")
			stuffcmd (te, "v_idlescale 4\n");

		if (te.classname == "monster")
			te.alert = 120;

		te = te.chain;
	}
};

void(vector pos) throw_debris =
{
	local entity tank, oself;
	local float oldz;

	oself = self;
	tank = spawn();
	self = tank;

	self.origin = pos;
	
	self.solid = SOLID_NOT;
	self.movetype = MOVETYPE_BOUNCE;
	self.takedamage = DAMAGE_NO;

	if (random()<0.5)
		setmodel (self, "progs/junk1.mdl");
	else
		setmodel (self, "progs/junk2.mdl");

	setsize (self, '0 0 0', '0 0 0');

	self.velocity_y = random()*900 - random()*900;
	self.velocity_x = random()*900 - random()*900;
	self.velocity_z = 400 + random()*400;

	self.avelocity_x = random()*600;
	self.avelocity_y = random()*600;
	self.avelocity_z = random()*600;

	self.think = SUB_Remove;
	self.nextthink = time + 2+random()*2;

	self = oself;

};

void() fueltank_explode =
{
	throw_debris(self.origin);
	throw_debris(self.origin);
	throw_debris(self.origin);
	throw_debris(self.origin);
	throw_debris(self.origin);

	bprint(2, "an enemy fuel tank has been destroyed!\n");
	self.takedamage = DAMAGE_NO;
	self.classname = "explo_box";
	Levelshake();
	T_RadiusDamage2 (self, self.owner, 50+random()*100, other, 200);
	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_EXPLOSION);
	WriteCoord (MSG_MULTICAST, self.origin_x);
	WriteCoord (MSG_MULTICAST, self.origin_y);
	WriteCoord (MSG_MULTICAST, self.origin_z+32);
	multicast (self.origin, MULTICAST_PHS);
	supplies = supplies - 1;
	remove (self);
};

void(vector pos) FuelTankSmall =
{
	local entity item, oself;
	precache_model ("maps/b_exbox2.bsp");

	item = spawn();
	oself = self;
	self = item;
	setorigin (self, pos);
	setmodel (self, "maps/b_exbox2.bsp");
	supplies += 1;
	self.rtime = 1;
	self.solid = SOLID_SLIDEBOX;
	setsize (self, '-16 -16 -16', '16 16 24');
	self.health = 50;
	self.th_die = fueltank_explode;
	self.takedamage = DAMAGE_AIM;
	self.classname = "fueltank";
	self.netname = "fuel tank";
	self.movetype = MOVETYPE_STEP;
	self = oself;
};

void() FuelTankLarge =
{
	local entity item, oself;
	precache_model ("maps/b_explob.bsp");

	item = spawn();
	oself = self;
	self = item;
	setmodel (self, "maps/b_explob.bsp");
	supplies += 1;
	self.rtime = 1;
	self.solid = SOLID_SLIDEBOX;
	setsize (self, '-16 -16 -16', '16 16 48');
	self.health = 50;
	self.th_die = fueltank_explode;
	self.takedamage = DAMAGE_AIM;
	self.classname = "fueltank";
	self.netname = "fuel tank";
	self.movetype = MOVETYPE_STEP;
	self = oself;
};


void(vector pos) obj_fueltank =
{
	FuelTankLarge();

	if (random()<0.25)
		FuelTankSmall(pos + '0 64 0');
	if (random()<0.25)
		FuelTankSmall(pos + '0 -64 0');
	if (random()<0.25)
		FuelTankSmall(pos + '64 0 0');
	if (random()<0.25)
		FuelTankSmall(pos + '-64 0 0');
};


void () food_touch =
{
	if (other.classname != "player")
		return;

	if (self.rtime > 0)
	{
		self.rtime = 0;
		bprint(2, "food crate has been secured.\n");

		other.score = other.score + 10;

		if (random()<0.25)
			sound (self, CHAN_BODY, "effects/radio1.wav", 1, ATTN_NONE);
		else if (random()<0.25)
			sound (self, CHAN_BODY, "effects/radio2.wav", 1, ATTN_NONE);
		else if (random()<0.25)
			sound (self, CHAN_BODY, "effects/radio3.wav", 1, ATTN_NONE);
		else
			sound (self, CHAN_BODY, "effects/radio4.wav", 1, ATTN_NONE);

		supplies = supplies - 1;
		spawn_excla(self, 3000);
	}

};

void() FoodCrate =
{
	setmodel (self, "maps/crate.bsp");
	self.touch = food_touch;
	supplies += 1;
	self.rtime = 1;
	self.solid = SOLID_SLIDEBOX;
	setsize (self, '-16 -16 0', '16 16 16');
	self.takedamage = DAMAGE_NO;
	self.touch = food_touch;
	self.movetype = MOVETYPE_STEP;
	self.classname = "foodcrate";
};

void() treasure_touch =
{
	if (other.classname != "player")
		return;
	if (self.picking > 0)
		return;

	if (self.attack_finished < time)
	{
		sound (self, CHAN_BODY, "items/locked.wav", 1, ATTN_NORM);
		sprint(other, 2, "this metal box is locked.\n");
		if (other.class == 2 || other.class == 4)
		{
			sprint(other, 2, "you may attempt to pick this\n");
			sprint(other, 2, "by pressing your 'action' key\n");
		}
	}
	self.attack_finished = time + 2;
	return;
};


void() bomb_touch =
{
	if (other.classname != "player")
		return;
	if (self.picking > 0)
		return;

	if (self.attack_finished < time)
	{
		sound (self, CHAN_BODY, "items/locked.wav", 1, ATTN_NORM);
		sprint(other, 2, "this is a high-tech bomb.\n");
		if (other.class == 4)
		{
			sprint(other, 2, "you may attempt to deactivate this\n");
			sprint(other, 2, "bomb by pressing your 'action' key\n");
		}
	}
	self.attack_finished = time + 2;
	return;
};

void() Explosive =
{
	if (random()*100<=25)
	{
		remove(self);
		return;
	}

	setmodel (self, "progs/bomb.mdl");
	self.touch = bomb_touch;
	self.solid = SOLID_SLIDEBOX;
	setsize (self, '-16 -16 0', '16 16 16');
	self.takedamage = DAMAGE_NO;
	self.movetype = MOVETYPE_STEP;
	self.classname = "bomb";
	droptofloor();
};

void() TreasureChest =
{
	setorigin(self, self.origin + '0 0 64');
	self.movetype = MOVETYPE_BOUNCE;
	self.velocity_x = crandom()*200;
	self.velocity_y = crandom()*200;
	self.velocity_z = -200;

	self.avelocity_y = crandom()*200;

	setmodel (self, "progs/ammobox2.mdl");
	self.touch = treasure_touch;
	self.solid = SOLID_BBOX;
	setsize (self, '-12 -12 0', '12 12 12');
	self.takedamage = DAMAGE_NO;
	self.classname = "treasure_chest";
};

void() car_rock =
{
	self.frame = self.frame + 1;

	if (self.frame >= 11)
		self.frame = 0;

	self.nextthink = time + 0.1 + (self.cnt*0.025);
	self.think = car_rock;
	self.cnt = self.cnt + 1;
};

void() car_rock2 =
{
	if (random()<0.5)
		sound (self, CHAN_BODY, "effects/metal-1.wav", 1, ATTN_NORM);
	else
		sound (self, CHAN_BODY, "effects/metal-2.wav", 1, ATTN_NORM);
};

void() car_touch =
{
	if (other.classname != "player")
		return;

	if (self.attack_finished < time)
	{
		self.think = car_rock;
		self.nextthink = time + 0.1;
		self.cnt = 1;
	}
	self.attack_finished = time + 4;
	return;
};

void() BlownUpCar =
{
	setmodel (self, "progs/BlownUp.mdl");
	self.origin = self.origin + '0 0 32';
	self.touch = car_touch;
	self.solid = SOLID_SLIDEBOX;
	self.helmet = 2;
	setsize (self, '-32 -32 0', '32 32 32');
      self.islot3 = SlotVal(IID_ARM_METAL, 1);
	self.takedamage = DAMAGE_AIM;
	self.health = 90000;
	self.th_pain = car_rock2;
	self.movetype = MOVETYPE_BOUNCE;
	self.classname = "car";
};

/*QUAKED noclass (0 0 0) (-8 -8 -8) (8 8 8)
prints a warning message when spawned
376 5857
354 8501
*/
void() noclass =
{
	dprint ("noclass spawned at");
	dprint (vtos(self.origin));
	dprint ("\n");
	remove (self);
};

void() q_touch;

void() q_touch =
{
local string    s;

	if (other.classname != "player")
		return;
	if (other.health <= 0)
		return;

	self.mdl = self.model;

	sound (other, CHAN_VOICE, self.noise, 1, ATTN_NORM);
	stuffcmd (other, "bf\n");
	self.solid = SOLID_NOT;
	other.items = other.items | IT_QUAD;
	self.model = string_null;
		if (deathmatch == 4)
		{
			other.armortype = 0;
			other.armorvalue = 0 * 0.01;
			other.ammo_cells = 0;
		}

// do the apropriate action
	other.super_time = 1;
	other.super_damage_finished = self.cnt;

	s=ftos(rint(other.super_damage_finished - time));

	bprint (PRINT_LOW, other.netname);
	if (deathmatch == 4)
		bprint (PRINT_LOW, " recovered an OctaPower with ");
	else 
		bprint (PRINT_LOW, " recovered a Quad with ");
	bprint (PRINT_LOW, s);
	bprint (PRINT_LOW, " seconds remaining!\n");

	activator = other;
	SUB_UseTargets();                               // fire all targets / killtargets
};


void(float timeleft) DropQuad =
{
	local entity    item;

	item = spawn();
	item.origin = self.origin;
	
	item.velocity_z = 300;
	item.velocity_x = -100 + (random() * 200);
	item.velocity_y = -100 + (random() * 200);
	
	item.flags = FL_ITEM;
	item.solid = SOLID_TRIGGER;
	item.movetype = MOVETYPE_TOSS;
	item.noise = "items/damage.wav";
	setmodel (item, "progs/quaddama.mdl");
	setsize (item, '-16 -16 -24', '16 16 32');
	item.cnt = time + timeleft;
	item.touch = q_touch;
	item.nextthink = time + timeleft;    // remove it with the time left on it
	item.think = SUB_Remove;
};


void() r_touch;

void () BombBeep =
{
	local float r;

	r = range (self.enemy);

	if (r != RANGE_MELEE || self.enemy.health <= 0)
	{
		self.owner.picking = 0;
		self.enemy.picking = 0;
		self.enemy.currentmenu = "none";
		remove(self.tumbler1);
		remove(self.tumbler2);
		remove(self);
		return;
	}

	self.inplace = 0;
	self.tumbler1.inplace = 0;
	self.tumbler2.inplace = 0;
	self.model = "";
	self.tumbler1.model = "";
	self.tumbler2.model = "";

	if (random() < 0.75)
	{
		self.inplace = 1;
		self.model = "progs/radio.spr";
	}
	if (random() < 0.75)
	{
		self.tumbler1.inplace = 1;
		self.tumbler1.model = "progs/radio.spr";
	}
	if (random() < 0.75)
	{
		self.tumbler2.inplace = 1;
		self.tumbler2.model = "progs/radio.spr";
	}


	self.think = BombBeep;
	self.nextthink = time + 0.3+random()*0.6;
	self.enemy.chest = self;
	self.enemy.currentmenu = "menu_defuse";

	if (random() < 0.5)
		sound (self, CHAN_BODY, "items/lockpick1.wav", 1, ATTN_NORM);
	else
		sound (self, CHAN_BODY, "items/lockpick2.wav", 1, ATTN_NORM);

};


void (entity portal, entity toucher) SpawnBomb =
{
	local entity open, t1, t2;

	makevectors (toucher.v_angle);

	open = spawn();
	setmodel(open, "progs/radio.spr");
	setorigin (open, toucher.origin + '0 0 48' + v_forward*64);
	setsize (open, VEC_ORIGIN, VEC_ORIGIN);
	open.think = BombBeep;
	open.nextthink = time + random()*0.2;
	open.enemy = toucher;
	open.owner = portal;
	open.owner.rtime = 3;

	t1 = spawn();
	setmodel(t1, "progs/radio.spr");
	setorigin (t1, toucher.origin + '0 0 48' + v_forward*64 + v_right*32);
	setsize (t1, VEC_ORIGIN, VEC_ORIGIN);
	t1.enemy = toucher;
	t1.owner = portal;

	t2 = spawn();
	setmodel(t2, "progs/radio.spr");
	setorigin (t2, toucher.origin + '0 0 48' + v_forward*64 - v_right*32);
	setsize (t2, VEC_ORIGIN, VEC_ORIGIN);
	t2.enemy = toucher;
	t2.owner = portal;

	open.tumbler1 = t1;
	open.tumbler2 = t2;
};

void () OpenChestBeep =
{
	local float r;

	r = range (self.enemy);

	if (r != RANGE_MELEE || self.enemy.health <= 0)
	{
		self.owner.picking = 0;
		self.enemy.picking = 0;
		self.enemy.currentmenu = "none";
		remove(self.tumbler1);
		remove(self.tumbler2);
		remove(self);
		return;
	}

	self.inplace = 0;
	self.tumbler1.inplace = 0;
	self.tumbler2.inplace = 0;
	self.model = "";
	self.tumbler1.model = "";
	self.tumbler2.model = "";

	if (random() < 0.5)
	{
		self.inplace = 1;
		self.model = "progs/radio.spr";
	}
	if (random() < 0.5)
	{
		self.tumbler1.inplace = 1;
		self.tumbler1.model = "progs/radio.spr";
	}
	if (random() < 0.5)
	{
		self.tumbler2.inplace = 1;
		self.tumbler2.model = "progs/radio.spr";
	}


	self.think = OpenChestBeep;
	self.nextthink = time + 0.2+random()*0.4;
	self.enemy.chest = self;
	self.enemy.currentmenu = "menu_lockpick";

	if (random() < 0.5)
		sound (self, CHAN_BODY, "items/lockpick1.wav", 1, ATTN_NORM);
	else
		sound (self, CHAN_BODY, "items/lockpick2.wav", 1, ATTN_NORM);

};


void (entity portal, entity toucher) SpawnOpenChest =
{
	local entity open, t1, t2;

	makevectors (toucher.v_angle);

	open = spawn();
	setmodel(open, "progs/radio.spr");
	setorigin (open, toucher.origin + '0 0 48' + v_forward*64);
	setsize (open, VEC_ORIGIN, VEC_ORIGIN);
	open.think = OpenChestBeep;
	open.nextthink = time + random()*0.2;
	open.enemy = toucher;
	open.owner = portal;

	t1 = spawn();
	setmodel(t1, "progs/radio.spr");
	setorigin (t1, toucher.origin + '0 0 48' + v_forward*64 + v_right*32);
	setsize (t1, VEC_ORIGIN, VEC_ORIGIN);
	t1.enemy = toucher;
	t1.owner = portal;

	t2 = spawn();
	setmodel(t2, "progs/radio.spr");
	setorigin (t2, toucher.origin + '0 0 48' + v_forward*64 - v_right*32);
	setsize (t2, VEC_ORIGIN, VEC_ORIGIN);
	t2.enemy = toucher;
	t2.owner = portal;

	open.tumbler1 = t1;
	open.tumbler2 = t2;
};


void() r_touch =
{
local string    s;

	if (other.classname != "player")
		return;
	if (other.health <= 0)
		return;

	self.mdl = self.model;

	sound (other, CHAN_VOICE, self.noise, 1, ATTN_NORM);
	stuffcmd (other, "bf\n");
	self.solid = SOLID_NOT;
	other.items = other.items | IT_INVISIBILITY;
	self.model = string_null;

// do the apropriate action
	other.invisible_time = 1;
	other.invisible_finished = self.cnt;
	s=ftos(rint(other.invisible_finished - time));
	bprint (PRINT_LOW, other.netname);
	bprint (PRINT_LOW, " recovered a Ring with ");
	bprint (PRINT_LOW, s);
	bprint (PRINT_LOW, " seconds remaining!\n");
      

	activator = other;
	SUB_UseTargets();                               // fire all targets / killtargets
};


void(float timeleft) DropRing =
{
	local entity    item;

	item = spawn();
	item.origin = self.origin;
	
	item.velocity_z = 300;
	item.velocity_x = -100 + (random() * 200);
	item.velocity_y = -100 + (random() * 200);
	
	item.flags = FL_ITEM;
	item.solid = SOLID_TRIGGER;
	item.movetype = MOVETYPE_TOSS;
	item.noise = "items/inv1.wav";
	setmodel (item, "progs/invisibl.mdl");
	setsize (item, '-16 -16 -24', '16 16 32');
	item.cnt = time + timeleft;
	item.touch = r_touch;
	item.nextthink = time + timeleft;    // remove after 30 seconds
	item.think = SUB_Remove;
};

/*
============
PlaceItem

plants the object on the floor
============
*/
void() PlaceItem =
{
	local float     oldz;

	self.mdl = self.model;          // so it can be restored on respawn
	self.flags = FL_ITEM;           // make extra wide
	self.solid = SOLID_TRIGGER;
	self.movetype = MOVETYPE_TOSS;  
	self.velocity = '0 0 0';
	self.origin_z = self.origin_z + 6;
	oldz = self.origin_z;
	if (!droptofloor())
	{
		dprint ("Bonus item fell out of level at ");
		dprint (vtos(self.origin));
		dprint ("\n");
		remove(self);
		return;
	}
};

/*
============
StartItem

Sets the clipping size and plants the object on the floor
============
*/
void() StartItem =
{
	self.nextthink = time + 0.2;    // items start after other solids
	self.think = PlaceItem;
};

/*
=========================================================================

HEALTH BOX

=========================================================================
*/
//
// T_Heal: add health to an entity, limiting health to max_health
// "ignore" will ignore max_health limit
//
float (entity e, float healamount, float ignore) T_Heal =
{
	if (e.health <= 0)
		return 0;
	if ((!ignore) && (e.health >= other.max_health))
		return 0;
	healamount = ceil(healamount);

	e.health = e.health + healamount;
	if ((!ignore) && (e.health >= other.max_health))
		e.health = other.max_health;
		
	if (e.health > 250)
		e.health = 250;
	return 1;
};

/*QUAKED item_health (.3 .3 1) (0 0 0) (32 32 32) rotten megahealth
Health box. Normally gives 25 points.
Rotten box heals 5-10 points,
megahealth will add 100 health, then 
rot you down to your maximum health limit, 
one point per second.
*/

float   H_ROTTEN = 1;
float   H_MEGA = 2;
.float  healamount, healtype;
void() health_touch;
void() item_megahealth_rot;

void() item_health =
{       

	if (self.spawnflags & H_MEGA)
		TreasureChest();
	else
	{
		if (random()*100<=75)
		{
			remove(self);
			return;
		}

		self.touch = health_touch;
		precache_model("maps/b_bh25.bsp");
		precache_sound("items/health1.wav");
		setmodel(self, "maps/b_bh25.bsp");
		self.noise = "items/health1.wav";
		self.healamount = 25;
		self.healtype = 1;
		setsize (self, '0 0 0', '32 32 56');
		StartItem ();
	}
};


float (entity to, float iid, float quant) TryGiveStackable =
{
	local float slot;
	local float item;
	local float x;

	x = 300;

	if (ToIID(self.islot4) == IID_EQUIP_BACKPACK)
		x = 600;

	slot = SlotOfItem(to, iid);
	if (slot > 4)
	{
		item = ItemInSlot(to, slot);
		if (ToStatus(item) + quant > x)	//300 is our maxstack value here
		{
			quant = quant + ToStatus(item) - x;
			SetItemSlot(to, slot, SlotVal(iid, x));
			slot = 0;
			return false;
		}
		else
			quant = quant + ToStatus(item);
	}
	if (quant == 0)
		return true;
	if (slot == 0)
	{
		slot = FindSuitableEmptySlot(to, iid);
		if (slot == 0)
			return false;
	}

	SetItemSlot(to, slot, SlotVal(iid, quant));

	return true;
};


void () health_touch =
{
	if (!triggercantouch(self, other))
		return;

	if (other.classname != "player" || other.ghost != 0)
		return;

	if (other.classname == "player")
	{
		if (self.healtype == H_MEGA)
			return;

		if (other.ghost != 0)
			return;

		if (!TryGiveStackable(other, IID_CHEM_STIMPACK, 1))	
			return;

		sprint (other, PRINT_HIGH, "picked up a stimpack.\n");

		if (random()*4 <= 2)
			sound (other, CHAN_ITEM, "misc/item1.wav", 1, ATTN_NORM);
		else
			sound (other, CHAN_ITEM, "misc/item2.wav", 1, ATTN_NORM);

		self.model = string_null;
		self.solid = SOLID_NOT;
		return;
	}

	activator = other;
	SUB_UseTargets ();
};

void() item_megahealth_rot =
{
	other = self.owner;
	
	if (other.health > other.max_health)
	{
		other.health = other.health - 1;
		self.nextthink = time + 1;
		return;
	}

// it is possible for a player to die and respawn between rots, so don't
// just blindly subtract the flag off
	other.items = other.items - (other.items & IT_SUPERHEALTH);
	
	if (deathmatch != 2)    // deathmatch 2 is silly old rules
	{
		self.nextthink = time + 20;
		self.think = SUB_regen;
	}
};

/*
===============================================================================

ARMOR

===============================================================================
*/

void() armor_touch;

void() armor_touch =
{
	local   float   type, value, bit;

	if (!triggercantouch(self, other))
		return;
	
	if (other.health <= 0)
		return;
	if (other.classname != "player")
		return;

	if (deathmatch == 4)
		if (other.invincible_time > 0)
			return;

	if (self.classname == "item_armor1")
	{
		type = 0.3;
		value = 100;
		bit = IT_ARMOR1;
	}
	if (self.classname == "item_armor2")
	{
		type = 0.6;
		value = 150;
		bit = IT_ARMOR2;
	}
	if (self.classname == "item_armorInv")
	{
		type = 0.8;
		value = 200;
		bit = IT_ARMOR3;
	}
	if (other.armortype*other.armorvalue >= type*value)
		return;
		
	other.armortype = type;
	other.armorvalue = value;
	other.items = other.items - (other.items & (IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) + bit;

	self.solid = SOLID_NOT;
	self.model = string_null;
	if (deathmatch != 2)
		self.nextthink = time + 20;
	self.think = SUB_regen;

	sprint(other, PRINT_LOW, "You got armor\n");
// armor touch sound
	sound(other, CHAN_ITEM, "items/armor1.wav", 1, ATTN_NORM);
	stuffcmd (other, "bf\n");
	
	activator = other;
	SUB_UseTargets();                               // fire all targets / killtargets
};


/*QUAKED item_armor1 (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() buyzone1 =
{
	SUB_UseTargets ();


	self.touch = SUB_Null;
	self.solid = SOLID_BBOX;
	self.movetype = MOVETYPE_STEP;
	setmodel (self, "progs/enforcer.mdl");
	self.skin = 0;
	setsize (self, '-16 -16 -24', '16 16 32');
	setorigin(self, self.origin + '0 0 24');
	self.classname = "merchant";
};

void() buyzone2 =
{
	SUB_UseTargets ();


	self.touch = SUB_Null;
	self.solid = SOLID_BBOX;
	setmodel (self, "progs/enforcer.mdl");
	self.skin = 0;
	setsize (self, '-16 -16 -24', '16 16 32');
	setorigin(self, self.origin + '0 0 24');
	self.classname = "merchant";
};

void() item_armor1_finish =
{
	SUB_UseTargets ();


	self.touch = SUB_Null;
	self.solid = SOLID_BBOX;
	self.movetype = MOVETYPE_STEP;
	setmodel (self, "progs/enforcer.mdl");
	self.skin = 0;
	setsize (self, '-16 -16 -24', '16 16 32');
	setorigin(self, self.origin + '0 0 24');
	self.classname = "merchant";
	self.think = shop_face;
	self.nextthink = time + 0.5;
};
void() item_armor1 =
{
	if (random()<0.50)
	{
		precache_model ("progs/enforcer.mdl");
		self.think = item_armor1_finish;
		self.nextthink = time + 0.1;
	}
};

/*QUAKED item_armor2 (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() item_armor2 =
{
	if (random()<0.50)
	{
		precache_model ("progs/enforcer.mdl");
		self.touch = SUB_Null;
		self.solid = SOLID_BBOX;
		setmodel (self, "progs/enforcer.mdl");
		self.skin = 0;
		setsize (self, '-16 -16 -24', '16 16 32');
		setorigin(self, self.origin + '0 0 24');
		self.classname = "merchant";
		self.think = shop_face;
		self.nextthink = time + 0.5;
	}
};

/*QUAKED item_armorInv (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() item_armorInv =
{
	if (random()<0.50)
	{
		precache_model ("progs/enforcer.mdl");
		self.touch = SUB_Null;
		self.solid = SOLID_BBOX;
		setmodel (self, "progs/enforcer.mdl");
		self.skin = 0;
		setsize (self, '-16 -16 -24', '16 16 32');
		setorigin(self, self.origin + '0 0 24');
		self.classname = "merchant";
		self.think = shop_face;
		self.nextthink = time + 0.5;
	}
};

/*
===============================================================================

WEAPONS

===============================================================================
*/

void() bound_other_ammo =
{
         
};


float(float w) RankForWeapon =
{
	if (w == IT_LIGHTNING)
		return 1;
	if (w == IT_ROCKET_LAUNCHER)
		return 2;
	if (w == IT_SUPER_NAILGUN)
		return 3;
	if (w == IT_GRENADE_LAUNCHER)
		return 4;
	if (w == IT_SUPER_SHOTGUN)
		return 5;
	if (w == IT_NAILGUN)
		return 6;
	return 7;
};

float (float w) WeaponCode =
{
	if (w == IT_SUPER_SHOTGUN)
		return 3;
	if (w == IT_NAILGUN)
		return 4;
	if (w == IT_SUPER_NAILGUN)
		return 5;
	if (w == IT_GRENADE_LAUNCHER)
		return 6;
	if (w == IT_ROCKET_LAUNCHER)
		return 7;
	if (w == IT_LIGHTNING)
		return 8;
	return 1;
};

/*
=============
Deathmatch_Weapon

Deathmatch weapon change rules for picking up a weapon

.float          ammo_shells, ammo_nails, ammo_rockets, ammo_cells;
=============
*/
void(float old, float new) Deathmatch_Weapon =
{
	local float or, nr;

// change self.weapon if desired
	or = RankForWeapon (self.weapon);
	nr = RankForWeapon (new);
	if ( nr < or )
		self.weapon = new;
};

/*
=============
weapon_touch
=============
*/
float() W_BestWeapon;

void() weapon_touch =
{
	local   float   hadammo, best, new, old;
	local   entity  stemp;
	local   float   leave;

	// For client weapon_switch
	local   float   w_switch;

	if (!triggercantouch(self, other))
		return;

	if (!(other.flags & FL_CLIENT))
		return;

	if ((stof(infokey(other,"w_switch"))) == 0)
		w_switch = 8;
	else
		w_switch = stof(infokey(other,"w_switch"));
	
// if the player was using his best weapon, change up to the new one if better          
	stemp = self;
	self = other;
	best = W_BestWeapon();
	self = stemp;

	if (deathmatch == 2 || deathmatch == 3 || deathmatch == 5)
		leave = 1;
	else
		leave = 0;
	
	if (self.classname == "weapon_nailgun")
	{
		if (leave && (other.items & IT_NAILGUN) )
			return;
		hadammo = other.ammo_nails;                     
		new = IT_NAILGUN;
		other.ammo_nails = other.ammo_nails + 30;
	}
	else if (self.classname == "weapon_supernailgun")
	{
		if (leave && (other.items & IT_SUPER_NAILGUN) )
			return;
		hadammo = other.ammo_rockets;                   
		new = IT_SUPER_NAILGUN;
		other.ammo_nails = other.ammo_nails + 30;
	}
	else if (self.classname == "weapon_supershotgun")
	{
		if (leave && (other.items & IT_SUPER_SHOTGUN) )
			return;
		hadammo = other.ammo_rockets;                   
		new = IT_SUPER_SHOTGUN;
		other.ammo_shells = other.ammo_shells + 5;
	}
	else if (self.classname == "weapon_rocketlauncher")
	{
		if (leave && (other.items & IT_ROCKET_LAUNCHER) )
			return;
		hadammo = other.ammo_rockets;                   
		new = IT_ROCKET_LAUNCHER;
		other.ammo_rockets = other.ammo_rockets + 5;
	}
	else if (self.classname == "weapon_grenadelauncher")
	{
		if (leave && (other.items & IT_GRENADE_LAUNCHER) )
			return;
		hadammo = other.ammo_rockets;                   
		new = IT_GRENADE_LAUNCHER;
		other.ammo_rockets = other.ammo_rockets + 5;
	}
	else if (self.classname == "weapon_lightning")
	{
		if (leave && (other.items & IT_LIGHTNING))
			return;
		hadammo = other.ammo_rockets;                   
		new = IT_LIGHTNING;
		other.ammo_cells = other.ammo_cells + 15;
	}
	else
		objerror ("weapon_touch: unknown classname");

	sprint (other, PRINT_LOW, "You got the ");
	sprint (other, PRINT_LOW, self.netname);
	sprint (other, PRINT_LOW, "\n");
// weapon touch sound
	sound (other, CHAN_ITEM, "weapons/pkup.wav", 1, ATTN_NORM);
	stuffcmd (other, "bf\n");

	bound_other_ammo ();

// change to the weapon
	old = other.items;
	other.items = other.items | new;
	
	stemp = self;
	self = other;

	if ( WeaponCode(new) <= w_switch )
	{
		if (self.flags & FL_INWATER)
		{
			if (new != IT_LIGHTNING)
			{
				Deathmatch_Weapon (old, new);
			}
		}
		else
		{                
			Deathmatch_Weapon (old, new);
		}
	}

	W_SetCurrentAmmo();

	self = stemp;

	if (leave)
		return;

	if (deathmatch!=3 || deathmatch !=5)
	{
	// remove it in single player, or setup for respawning in deathmatch
		self.model = string_null;
		self.solid = SOLID_NOT;
		if (deathmatch != 2)
			self.nextthink = time + 30;
		self.think = SUB_regen;
	}
	activator = other;
	SUB_UseTargets();                               // fire all targets / killtargets
};


void(vector place) hostage_new;

/*QUAKED weapon_supershotgun (0 .5 .8) (-16 -16 0) (16 16 32)
*/



void() weapon_supershotgun =
{
	//weapon spawns are now either treasure chests
	//or hostages that need to be rescued

	if (random()<0.25)
		TreasureChest();
};

/*QUAKED weapon_nailgun (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() weapon_nailgun =
{
	if (world.map_obj == 2)
		FoodCrate();
	else if (world.map_obj == 5)
		obj_fueltank(self.origin);
	else if (world.map_obj == 6)
		Explosive();
	else
		TreasureChest();
};

/*QUAKED weapon_supernailgun (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() weapon_supernailgun =
{
	if (world.map_obj == 2)
		FoodCrate();
	else if (world.map_obj == 5)
		obj_fueltank(self.origin);
	else if (world.map_obj == 6)
		Explosive();
	else if (random()*100 <= 40)
		TreasureChest();
};

/*QUAKED weapon_grenadelauncher (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() weapon_grenadelauncher =
{

	if (world.map_obj == 2)
		FoodCrate();
	else if (world.map_obj == 5)
		obj_fueltank(self.origin);
	else if (world.map_obj == 6)
		Explosive();
	else if (random()*100 <= 40)
		TreasureChest();

};

/*QUAKED weapon_rocketlauncher (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() weapon_rocketlauncher =
{
	if (world.map_obj == 2)
		FoodCrate();
	else if (world.map_obj == 5)
		obj_fueltank(self.origin);
	else if (world.map_obj == 6)
		Explosive();
	else if (random()*100 <= 40)
		TreasureChest();

};


/*QUAKED weapon_lightning (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() weapon_lightning =
{

	if (world.map_obj == 2)
		FoodCrate();
	else if (world.map_obj == 5)
		obj_fueltank(self.origin);
	else if (world.map_obj == 6)
		Explosive();
	else if (random()*100 <= 40)
		TreasureChest();

};


/*
===============================================================================

AMMO

===============================================================================
*/

float() GetRandomAmmo =
{
	local float r;

	r = random()*42;

	if (r <= 5)
		return IID_AM_10MM;
	else if (r <= 10)
		return IID_AM_45ACP;
	else if (r <= 15)
		return IID_AM_12GAUGESHELLS;
	else if (r <= 20)
		return IID_AM_44MAGNUM;
	else if (r <= 24)
		return IID_AM_556MM;
	else if (r <= 28)
		return IID_AM_5MMHIGHVEL;
	else if (r <= 32)
		return IID_AM_762MM;
	else if (r <= 36)
		return IID_AM_NEEDLER;
	else if (r <= 38)
		return IID_AM_ENERGYCELL;
	else if (r <= 40)
		return IID_AM_2MMEC;
	else
		return IID_AM_CASELESS;
};

float(float ammotype) GetAmmoCount =
{
	local float ammocount;

	if (ammotype == IID_AM_NEEDLER)
		ammocount = 2 + random()*7;
	if (ammotype == IID_AM_10MM)
		ammocount = 5 + random()*5;
	if (ammotype == IID_AM_556MM)
		ammocount = 3 + random()*8;
	if (ammotype == IID_AM_5MMHIGHVEL)
		ammocount = 5 + random()*8;
	if (ammotype == IID_AM_12GAUGESHELLS)
		ammocount = 2 + random()*6;
	if (ammotype == IID_AM_ENERGYCELL)
		ammocount = 2 + random()*6;
	if (ammotype == IID_AM_2MMEC)
		ammocount = 1 + random()*2;
	if (ammotype == IID_AM_762MM)
		ammocount = 2 + random()*4;
	if (ammotype == IID_AM_44MAGNUM)
		ammocount = 2 + random()*4;
	if (ammotype == IID_AM_45ACP)
		ammocount = 5 + random()*10;

	return ammocount;
};

void() ammo_touch =
{
	local entity stemp;
	local string ammoname, ammocountftos;
	local float best, ammotype, ammocount;

	if (!triggercantouch(self, other))
		return;

	if (other.classname != "player")
		return;
	if (other.health <= 0)
		return;

// if the player was using his best weapon, change up to the new one if better          
	stemp = self;
	self = other;
	best = W_BestWeapon();
	self = stemp;

	sprint (other, 2, "a pre-war military ammo cache.\n");
	sprint (other, 2, "found ");

	ammotype = GetRandomAmmo();

	ammocount = GetAmmoCount(ammotype);
	ammocount = ceil(ammocount);

	if (coop == 1 && total_players == 1)
		ammocount = ammocount * 2;

	ammoname = GetItemName(ammotype);
	ammocountftos = ftos(ammocount);

		if (random()*4 <= 2)
			sound (other, CHAN_ITEM, "misc/item1.wav", 1, ATTN_NORM);
		else
			sound (other, CHAN_ITEM, "misc/item2.wav", 1, ATTN_NORM);

// shotgun
	if (self.weapon == 1)
	{
		sprint (other, 2, ammoname);
		sprint (other, 2, "(");
		sprint (other, 2, ammocountftos);
		sprint (other, 2, ") ");

		TryGiveStackable(other, ammotype, ammocount);

		if (random()*20 <= 10)
		{
			other.ammo_shells = other.ammo_shells + ceil(10+random()*10);
			sprint (other, 2, "& money.\n");
		}
		else
			sprint (other, 2, "\n");

	}

// spikes
	if (self.weapon == 2)
	{
		sprint (other, 2, ammoname);
		sprint (other, 2, "(");
		sprint (other, 2, ammocountftos);
		sprint (other, 2, ") ");

		TryGiveStackable(other, ammotype, ammocount);

		if (random()*20 <= 10)
		{
			other.ammo_shells = other.ammo_shells + ceil(10+random()*10);
			sprint (other, 2, " & money.\n");
		}
		else
			sprint (other, 2, "\n");
	}

//      rockets
	if (self.weapon == 3)
	{
		sprint (other, 2, ammoname);
		sprint (other, 2, "(");
		sprint (other, 2, ammocountftos);
		sprint (other, 2, ") ");

		TryGiveStackable(other, ammotype, ammocount);

		if (random()*20 <= 10)
		{
			other.ammo_shells = other.ammo_shells + ceil(5+random()*25);
			sprint (other, 2, " & money.\n");
		}
		else
			sprint (other, 2, "\n");

	}

//      cells
	if (self.weapon == 4)
	{
		sprint (other, 2, ammoname);
		sprint (other, 2, "(");
		sprint (other, 2, ammocountftos);
		sprint (other, 2, ") ");

		TryGiveStackable(other, ammotype, ammocount);

		if (random()*20 <= 10)
		{
			other.ammo_shells = other.ammo_shells + ceil(5+random()*25);
			sprint (other, 2, " & money.\n");
		}
		else
			sprint (other, 2, "\n");
	}

	bound_other_ammo ();
	


// change to a better weapon if appropriate
/*
	if ( other.weapon == best )
	{
		stemp = self;
		self = other;
		self.weapon = W_BestWeapon();
		W_SetCurrentAmmo ();
		self = stemp;
	}*/

// if changed current ammo, update it
	stemp = self;
	self = other;
	self = stemp;

// remove it in single player, or setup for respawning in deathmatch
	self.model = string_null;
	self.solid = SOLID_NOT;
	if (coop != 1)
		self.nextthink = time + 30;

// Xian -- If playing in DM 3.0 mode, halve the time ammo respawns        

	self.think = SUB_regen;

	activator = other;
	SUB_UseTargets();                               // fire all targets / killtargets
};




float WEAPON_BIG2 = 1;

/*QUAKED item_shells (0 .5 .8) (0 0 0) (32 32 32) big
*/

void() item_shells =
{
	if (deathmatch == 4)
		return;

	if (coop == 1 && random()*4 <= 2)
		return;

	self.islot1 = GetRandomAmmo();
	self.islot1 = self.islot1 * 512;
	self.flash = ceil(random()*30);
	self.touch = AmmoTouch;
	if (random()<0.5)
		self.islot2 = ceil(random()*20);



	if (self.spawnflags & WEAPON_BIG2)
	{
		precache_model ("maps/b_shell1.bsp");
		setmodel (self, "maps/b_shell1.bsp");
		self.aflag = 40;
	}
	else
	{
		precache_model ("maps/b_shell0.bsp");
		setmodel (self, "maps/b_shell0.bsp");
		self.aflag = 20;
	}
	self.weapon = 1;
	self.netname = "shells";
	setsize (self, '0 0 0', '32 32 56');
	StartItem ();
};

/*QUAKED item_spikes (0 .5 .8) (0 0 0) (32 32 32) big
*/

void() item_spikes =
{
	if (deathmatch == 4)
		return;

	if (coop == 1 && random()*4 <= 1)
		return;

	self.islot1 = GetRandomAmmo();
	self.islot1 = self.islot1 * 512;
	self.flash = ceil(random()*30);
	self.touch = AmmoTouch;
	if (random()<0.5)
		self.islot2 = ceil(random()*20);


	if (self.spawnflags & WEAPON_BIG2)
	{
		precache_model ("maps/b_nail1.bsp");
		setmodel (self, "maps/b_nail1.bsp");
		self.aflag = 50;
	}
	else
	{
		precache_model ("maps/b_nail0.bsp");
		setmodel (self, "maps/b_nail0.bsp");
		self.aflag = 25;
	}
	self.weapon = 2;
	self.netname = "nails";
	setsize (self, '0 0 0', '32 32 56');
	StartItem ();

};

/*QUAKED item_rockets (0 .5 .8) (0 0 0) (32 32 32) big
*/

void() item_rockets =
{
	if (deathmatch == 4)
		return;

	if (coop == 1 && random()*4 <= 3)
		return;

	self.islot1 = GetRandomAmmo();
	self.islot1 = self.islot1 * 512;
	self.flash = ceil(random()*30);
	self.touch = AmmoTouch;
	if (random()<0.5)
		self.islot2 = ceil(random()*20);


	if (self.spawnflags & WEAPON_BIG2)
	{
		precache_model ("maps/b_rock1.bsp");
		setmodel (self, "maps/b_rock1.bsp");
		self.aflag = 10;
	}
	else
	{
		precache_model ("maps/b_rock0.bsp");
		setmodel (self, "maps/b_rock0.bsp");
		self.aflag = 5;
	}
	self.weapon = 3;
	self.netname = "rockets";
	setsize (self, '0 0 0', '32 32 56');
	StartItem ();

};


/*QUAKED item_cells (0 .5 .8) (0 0 0) (32 32 32) big
*/

void() item_cells =
{
	if (deathmatch == 4)
		return;

	if (coop == 1 && random()*4 <= 2)
		return;

	self.islot1 = GetRandomAmmo();
	self.islot1 = self.islot1 * 512;
	self.flash = ceil(random()*30);
	self.touch = AmmoTouch;
	if (random()<0.5)
		self.islot2 = ceil(random()*20);


	if (self.spawnflags & WEAPON_BIG2)
	{
		precache_model ("maps/b_batt1.bsp");
		setmodel (self, "maps/b_batt1.bsp");
		self.aflag = 12;
	}
	else
	{
		precache_model ("maps/b_batt0.bsp");
		setmodel (self, "maps/b_batt0.bsp");
		self.aflag = 6;
	}
	self.weapon = 4;
	self.netname = "cells";
	setsize (self, '0 0 0', '32 32 56');
	StartItem ();

};


/*QUAKED item_weapon (0 .5 .8) (0 0 0) (32 32 32) shotgun rocket spikes big
DO NOT USE THIS!!!! IT WILL BE REMOVED!
*/

float WEAPON_SHOTGUN = 1;
float WEAPON_ROCKET = 2;
float WEAPON_SPIKES = 4;
float WEAPON_BIG = 8;
void() item_weapon =
{
	if (coop == 1 && random()*4 <= 3)
		return;

	self.islot1 = GetRandomAmmo();
	self.islot1 = self.islot1 * 512;
	self.flash = ceil(random()*30);
	self.touch = AmmoTouch;
	if (random()<0.5)
		self.islot2 = ceil(random()*20);

	precache_model ("progs/ammobox.mdl");
	setmodel (self, "progs/ammobox.mdl");
	self.skin = 2;
	
	setsize (self, '0 0 0', '32 32 56');
	StartItem ();
};


/*
===============================================================================

KEYS

===============================================================================
*/

void() key_touch =
{
	if (!triggercantouch(self, other))
		return;

	if (other.classname != "player")
		return;
	if (other.health <= 0)
		return;
	if (other.items & self.items)
		return;

	sprint (other, PRINT_LOW, "You got the ");
	sprint (other, PRINT_LOW, self.netname);
	sprint (other,PRINT_LOW, "\n");

	sound (other, CHAN_ITEM, self.noise, 1, ATTN_NORM);
	stuffcmd (other, "bf\n");
	other.items = other.items | self.items;

	self.solid = SOLID_NOT;
	self.model = string_null;

	activator = other;
	SUB_UseTargets();                               // fire all targets / killtargets
};


void() key_setsounds =
{
	if (world.worldtype == 0)
	{
		precache_sound ("misc/medkey.wav");
		self.noise = "misc/medkey.wav";
	}
	if (world.worldtype == 1)
	{
		precache_sound ("misc/runekey.wav");
		self.noise = "misc/runekey.wav";
	}
	if (world.worldtype == 2)
	{
		precache_sound2 ("misc/basekey.wav");
		self.noise = "misc/basekey.wav";
	}
};

/*QUAKED item_key1 (0 .5 .8) (-16 -16 -24) (16 16 32)
SILVER key
In order for keys to work
you MUST set your maps
worldtype to one of the
following:
0: medieval
1: metal
2: base
*/

void() item_key1 =
{
	if (world.worldtype == 0)
	{
		precache_model ("progs/w_s_key.mdl");
		setmodel (self, "progs/w_s_key.mdl");
		self.netname = "silver key";
	}
	else if (world.worldtype == 1)
	{
		precache_model ("progs/m_s_key.mdl");
		setmodel (self, "progs/m_s_key.mdl");
		self.netname = "silver runekey";
	}
	else if (world.worldtype == 2)
	{
		precache_model2 ("progs/b_s_key.mdl");
		setmodel (self, "progs/b_s_key.mdl");
		self.netname = "silver keycard";
	}
	key_setsounds();
	self.touch = key_touch;
	self.items = IT_KEY1;
	setsize (self, '-16 -16 -24', '16 16 32');
	StartItem ();
};

/*QUAKED item_key2 (0 .5 .8) (-16 -16 -24) (16 16 32)
GOLD key
In order for keys to work
you MUST set your maps
worldtype to one of the
following:
0: medieval
1: metal
2: base
*/

void() item_key2 =
{
	if (world.worldtype == 0)
	{
		precache_model ("progs/w_g_key.mdl");
		setmodel (self, "progs/w_g_key.mdl");
		self.netname = "gold key";
	}
	if (world.worldtype == 1)
	{
		precache_model ("progs/m_g_key.mdl");
		setmodel (self, "progs/m_g_key.mdl");
		self.netname = "gold runekey";
	}
	if (world.worldtype == 2)
	{
		precache_model2 ("progs/b_g_key.mdl");
		setmodel (self, "progs/b_g_key.mdl");
		self.netname = "gold keycard";
	}
	key_setsounds();
	self.touch = key_touch;
	self.items = IT_KEY2;
	setsize (self, '-16 -16 -24', '16 16 32');
	StartItem ();
};



/*
===============================================================================

END OF LEVEL RUNES

===============================================================================
*/

void() sigil_touch =
{
	if (!triggercantouch(self, other))
		return;

	if (other.classname != "player")
		return;
	if (other.health <= 0)
		return;

	centerprint (other, "You got the rune!");

	sound (other, CHAN_ITEM, self.noise, 1, ATTN_NORM);
	stuffcmd (other, "bf\n");
	self.solid = SOLID_NOT;
	self.model = string_null;
	serverflags = serverflags | (self.spawnflags & 15);
	self.classname = "";            // so rune doors won't find it
	
	activator = other;
	SUB_UseTargets();                               // fire all targets / killtargets
};


/*QUAKED item_sigil (0 .5 .8) (-16 -16 -24) (16 16 32) E1 E2 E3 E4
End of level sigil, pick up to end episode and return to jrstart.
*/

void() item_sigil =
{
	if (!self.spawnflags)
		objerror ("no spawnflags");

	precache_sound ("misc/runekey.wav");
	self.noise = "misc/runekey.wav";

	if (self.spawnflags & 1)
	{
		precache_model ("progs/end1.mdl");
		setmodel (self, "progs/end1.mdl");
	}
	if (self.spawnflags & 2)
	{
		precache_model2 ("progs/end2.mdl");
		setmodel (self, "progs/end2.mdl");
	}
	if (self.spawnflags & 4)
	{
		precache_model2 ("progs/end3.mdl");
		setmodel (self, "progs/end3.mdl");
	}
	if (self.spawnflags & 8)
	{
		precache_model2 ("progs/end4.mdl");
		setmodel (self, "progs/end4.mdl");
	}
	
	self.touch = sigil_touch;
	setsize (self, '-16 -16 -24', '16 16 32');
	StartItem ();
};

/*
===============================================================================

POWERUPS

===============================================================================
*/

void() powerup_touch;


void() powerup_touch =
{
	if (!triggercantouch(self, other))
		return;

	if (other.classname != "player")
		return;
	if (other.health <= 0)
		return;

	sprint (other, PRINT_LOW, "You got the ");
	sprint (other,PRINT_LOW,  self.netname);
	sprint (other,PRINT_LOW, "\n");

	self.mdl = self.model;
	
	if ((self.classname == "item_artifact_invulnerability") ||
		(self.classname == "item_artifact_invisibility"))
		self.nextthink = time + 60*5;
	else
		self.nextthink = time + 60;
	
	self.think = SUB_regen;

	sound (other, CHAN_VOICE, self.noise, 1, ATTN_NORM);
	stuffcmd (other, "bf\n");
	self.solid = SOLID_NOT;
	other.items = other.items | self.items;
	self.model = string_null;

// do the apropriate action
	if (self.classname == "item_artifact_envirosuit")
	{
		other.rad_time = 1;
		other.radsuit_finished = time + 30;
	}
	
	if (self.classname == "item_artifact_invulnerability")
	{
		other.invincible_time = 1;
		other.invincible_finished = time + 30;
	}
	
	if (self.classname == "item_artifact_invisibility")
	{
		other.invisible_time = 1;
		other.invisible_finished = time + 30;
	}

	if (self.classname == "item_artifact_super_damage")
	{
		if (deathmatch == 4)
		{
			other.armortype = 0;
			other.armorvalue = 0 * 0.01;
			other.ammo_cells = 0;
		}
		other.super_time = 1;
		other.super_damage_finished = time + 30;
	}       

	activator = other;
	SUB_UseTargets();                               // fire all targets / killtargets
};



/*QUAKED item_artifact_invulnerability (0 .5 .8) (-16 -16 -24) (16 16 32)
Player is invulnerable for 30 seconds
*/
void() item_artifact_invulnerability =
{
	self.touch = powerup_touch;

	precache_model ("progs/invulner.mdl");
	precache_sound ("items/protect.wav");
	precache_sound ("items/protect2.wav");
	precache_sound ("items/protect3.wav");
	self.noise = "items/protect.wav";
	setmodel (self, "progs/invulner.mdl");
	self.netname = "Pentagram of Protection";
	self.effects = self.effects | EF_RED;
	self.items = IT_INVULNERABILITY;
	setsize (self, '-16 -16 -24', '16 16 32');
	StartItem ();
};

/*QUAKED item_artifact_envirosuit (0 .5 .8) (-16 -16 -24) (16 16 32)
Player takes no damage from water or slime for 30 seconds
*/
void() item_artifact_envirosuit =
{
	remove(self);
	return;
};


/*QUAKED item_artifact_invisibility (0 .5 .8) (-16 -16 -24) (16 16 32)
Player is invisible for 30 seconds
*/
void() item_artifact_invisibility =
{
	remove(self);
	return;
};


/*QUAKED item_artifact_super_damage (0 .5 .8) (-16 -16 -24) (16 16 32)
The next attack from the player will do 4x damage
*/
void() item_artifact_super_damage =
{

	remove(self);
};



/*
===============================================================================

PLAYER BACKPACKS

===============================================================================
*/

void() BackpackTouch =
{
	local float ammotype, ammocount;
	local string ammoname;

	if (other.classname != "player")
		return;

	sprint(other, 2, "loot: found");

	if (self.weapon == 1)
	{
		TryGiveStackable(other, IID_AM_10MM, floor(9+random()*20));
		sprint(other, 2, " 10mm ammo");
	}
	else if (self.weapon == 2)
	{
		TryGiveStackable(other, IID_AM_556MM, floor(5+random()*15));
		sprint(other, 2, " 5mm ammo");
	}
	else if (self.weapon == 3)
	{
		TryGiveStackable(other, IID_AM_12GAUGESHELLS, floor(3+random()*6));
		sprint(other, 2, " 12 gauge ammo");
	}
	else if (self.weapon == 4)
	{
		TryGiveStackable(other, IID_AM_10MM, floor(9+random()*20));
		sprint(other, 2, " 10mm ammo");
	}
	else
	{
		ammotype = GetRandomAmmo();
		ammocount = GetAmmoCount(ammotype);
		ammoname = GetItemName(ammotype);
		sprint(other, 2, " ");
		sprint(other, 2, ammoname);
		TryGiveStackable(other, ammotype, ammocount);

	}


	if (random()*4 <= 2)
	{
		other.ammo_shells = other.ammo_shells + ceil(5+random()*20);
		sprint(other, 2, " & money.\n");
	}
	else
		sprint(other, 2, ".\n");

	remove(self);
	return;
};


/*
===============
DropMoney
===============
*/

void() MoneyTouch =
{
	if (other.classname != "player")
		return;

	if (other.health <= 0)
		return;

	sound (self, CHAN_VOICE, "items/item1.wav", 1, ATTN_IDLE);
	sprint(other, 2, "a small amount of wasteland currency.\n");
	other.ammo_shells += self.islot1;
	remove(self);
};

void() DropMoney =
{
	local entity    item;

	item = spawn();
	item.origin = self.origin - '0 0 24';
	
	item.weapon = self.weapon;

	item.velocity_z = 300;
	item.velocity_x = -100 + (random() * 200);
	item.velocity_y = -100 + (random() * 200);
	
	item.flags = FL_ITEM;
	item.solid = SOLID_TRIGGER;
	item.movetype = MOVETYPE_TOSS;
	setmodel (item, "progs/money.mdl");
	setsize (item, '-16 -16 0', '16 16 56');
	item.islot1 = 2 + random()*5;
	item.skin = ToIID(item.islot1) - 506;

	item.touch = MoneyTouch;

	item.nextthink = time + 120;    // remove after 2 minutes
	item.think = SUB_Remove;
};


/*
===============
DropBackpack
===============
*/
void() DropBackpack =
{
	local entity    item;


	item = spawn();
	item.origin = self.origin - '0 0 24';
	
	item.weapon = self.weapon;

	item.velocity_z = 300;
	item.velocity_x = -100 + (random() * 200);
	item.velocity_y = -100 + (random() * 200);
	
	item.flags = FL_ITEM;
	item.solid = SOLID_TRIGGER;
	item.movetype = MOVETYPE_TOSS;
	setmodel (item, "progs/ammobox.mdl");
	setsize (item, '-16 -16 0', '16 16 56');
	item.islot1 = GetRandomAmmo();
	item.islot1 = item.islot1 * 512;
	item.skin = ToIID(item.islot1) - 506;

	item.flash = ceil(random()*30);
	item.touch = AmmoTouch;

	item.nextthink = time + 120;    // remove after 2 minutes
	item.think = SUB_Remove;
};


