
void() T_MissileTouch;
void() info_player_start;
void(entity targ, entity attacker) ClientObituary;
void(entity inflictor, entity attacker, float damage, entity ignore, string dtype) T_RadiusDamage;
float (float int) getperk;
void() monster_death_use;

//============================================================================

/*
============
CanDamage

Returns true if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
float(entity targ, entity inflictor) CanDamage =
{
// bmodels need special checking because their origin is 0,0,0
	if (targ.movetype == MOVETYPE_PUSH)
	{
		traceline(inflictor.origin, 0.5 * (targ.absmin + targ.absmax), TRUE, self);
		if (trace_fraction == 1)
			return TRUE;
		if (trace_ent == targ)
			return TRUE;
		return FALSE;
	}
	
	traceline(inflictor.origin, targ.origin, TRUE, self);
	if (trace_fraction == 1)
		return TRUE;
	traceline(inflictor.origin, targ.origin + '15 15 0', TRUE, self);
	if (trace_fraction == 1)
		return TRUE;
	traceline(inflictor.origin, targ.origin + '-15 -15 0', TRUE, self);
	if (trace_fraction == 1)
		return TRUE;
	traceline(inflictor.origin, targ.origin + '-15 15 0', TRUE, self);
	if (trace_fraction == 1)
		return TRUE;
	traceline(inflictor.origin, targ.origin + '15 -15 0', TRUE, self);
	if (trace_fraction == 1)
		return TRUE;

	return FALSE;
};


/*
============
Killed
============
*/
void(entity targ, entity attacker) Killed =
{
	local entity oself;

	oself = self;
	self = targ;
	
	if (self.health < -99)
		self.health = -99;              // don't let sbar look bad if a player

	if (self.movetype == MOVETYPE_PUSH || self.movetype == MOVETYPE_NONE)
	{       // doors, triggers, etc
		self.th_die ();
		self = oself;
		return;
	}

	self.enemy = attacker;

	targ.grab = 0;
	targ.equipment_slot = 0;         //turn off stealth-boys, climbing gear, etc

// bump the monster counter
	if (self.flags & FL_MONSTER)
	{
		killed_monsters = killed_monsters + 1;
		WriteByte (MSG_ALL, SVC_KILLEDMONSTER);
	}

	ClientObituary(self, attacker);
	
	if (self.classname == "player")  //so dead players can spectate
	{
		self.ghost = 1;
		self.flags = self.flags | FL_FINDABLE_NONSOLID;	//so tracelines can find them
	}

	self.takedamage = DAMAGE_NO;
	self.touch = SUB_Null;
	self.effects = 0;

	monster_death_use();

	self.th_die ();
	
	self = oself;
};


void(entity targ, entity attacker) ArmorNoise =
{
	local float r;

	if (targ.classname != "player" && targ.classname != "monster")
		return;

	if (attacker.classname != "player" && attacker.classname != "monster")
		return;

	r = random();

	if (r <= 0.33)
		sound (targ, CHAN_WEAPON, "contact/armor1.wav", 1, ATTN_NORM);
	else if (r <= 0.67)
		sound (targ, CHAN_WEAPON, "contact/armor2.wav", 1, ATTN_NORM);
	else
		sound (targ, CHAN_WEAPON, "contact/armor3.wav", 1, ATTN_NORM);
};


/*
============
T_Damage

The damage is coming from inflictor, but get mad at attacker
This should be the only function that ever reduces health.
============
*/

void(entity targ, entity inflictor, entity attacker, float damage) T_Damage =
{
	local   entity  oldself;
	local   float   save, dm, radpenalty;
	local   float   take, severity, helm;
	local   string  attackerteam, targteam;
	local string info1, info2;

	if (!targ.takedamage)
		return;


	damage = floor(damage);
	targ.health = floor(targ.health);

	if (total_players == 1)
		damage = damage * 0.70;
	else if (total_players == 2)
		damage = damage * 0.80;
	else if (total_players == 3)
		damage = damage * 0.90;
	else
		damage = damage * 1.00;


	if (challenge_rating >= 20 && attacker.classname == "monster")
		damage = damage * 1.30;
	else if (challenge_rating <= 12 && attacker.classname == "monster")
		damage = damage * 1.20;
	else if (challenge_rating <= 9 && attacker.classname == "monster")
		damage = damage * 1.10;

	if (targ.radiation > 0)
	{
		radpenalty = 1 + (targ.radiation/200);
		damage = damage * radpenalty;
	}

	if (attacker.radiation > 0)
	{
		radpenalty = 1 - (targ.radiation/200);
		damage = damage * radpenalty;
	}

	if (targ.classname == "player" && attacker.classname == "monster")
		targ.superenemy = attacker;

//flanking monsters awards points
	if (attacker.classname == "player" && targ.classname == "monster" && targ.enemy != attacker)
		attacker.score = attacker.score + 1;

//aimed shots awards points
	if (attacker.classname == "player" && targ.classname == "monster" && attacker.recoil <= 5)
		attacker.score = attacker.score + 1;

//attacking a monster from behind awards points

	local vector vec;
	local float dot;

	if (attacker.classname == "player" && targ.classname == "monster")
	{
		makevectors (targ.angles);
		vec = normalize (attacker.origin - targ.origin);
		dot = (vec * v_forward);

		if (dot < 0.3)
			attacker.score = attacker.score + 1;

		if (attacker.perk1 == 15 || attacker.perk2 == 15)
		{
			bprint(2, "SNEAK ATTACK\n");
			damage = damage * (1 + (attacker.skill_sneak/15));
		}
	}

//attacking a team-mate is a HUGE penalty
	if (attacker.classname == "player" && targ.classname == "player")
	{
		attacker.score = attacker.score - 2;
		sprint(attacker, 2, "you just shot a teammate! be careful!\n");
	}

//attacking a hostage is a HUGE penalty
	if (attacker.classname == "player" && targ.classname == "hostage")
	{
		attacker.score = attacker.score - 2;
		sprint(attacker, 2, "you just shot a hostage! be careful!\n");
	}

//using cover awards points
	if (targ.classname == "monster" && attacker.classname == "player" && attacker.rage == 777)
		attacker.score = attacker.score + 1;


//charmed monsters will protect their master
	if (targ.classname == "player" && targ.friend.classname == "monster")
	{
		if (targ.friend.enemy.classname != "monster")
			targ.friend.enemy = attacker;
	}
//charmed monsters will assist their master
	if (attacker.classname == "player" && attacker.friend.classname == "monster")
	{
		if (attacker.friend.enemy.classname != "monster")
			attacker.friend.enemy = targ;
	}
//dodge chance

	if (targ.position == 0 && random()<0.05)
	{
		sound (targ, CHAN_BODY, "effects/miss.wav", 1, ATTN_NORM);
		return;
	}
	else if (targ.position == 1 && random()<0.10)
	{
		sound (targ, CHAN_BODY, "effects/miss.wav", 1, ATTN_NORM);
		return;
	}
	else if (targ.position == 2 && random()<0.20)
	{
		sound (targ, CHAN_BODY, "effects/miss.wav", 1, ATTN_NORM);
		return;
	}



//stay low: if taking cover, 33% miss chance (others get 10% bonus)
	if (targ.rage == 777)
	{
		if (targ.perk1 == 4 || targ.perk2 == 4)
		{
			if (random()<(0.33+(self.skill_speech*0.01)))
			{
				sound (targ, CHAN_BODY, "effects/miss.wav", 1, ATTN_NORM);
				return;
			}
		}
		else
		{
			if (random()<0.10)
			{
				sound (targ, CHAN_BODY, "effects/miss.wav", 1, ATTN_NORM);
				return;
			}
		}
	}

	//master blaster: the more you burst, the higher the damage bonus
	if (attacker.perk1 == 14 || attacker.perk2 == 14)
	{
		if (attacker.recoil > 5)
		{
			damage = damage * (1 + (attacker.recoil * (0.03+(self.skill_speech*0.002))));
		}
	}



	//sneak attack: if you attack a creature who isn't fighting you, bonus damage
	if (attacker.skill_sneak > 0 && attacker.sneak > 0)
	{
		if (targ.enemy != attacker)
		{
			bprint(2, "SNEAK ATTACK\n");
			damage = damage * (1 + (attacker.skill_sneak/15));
		}
	}




// used by buttons and triggers to set activator for target firing
	damage_attacker = attacker;

	if (attacker.classname == "monster")
	{
		if (random()*100>= 95)
			attacker.critical = 3;
		else
			attacker.critical = 0;
	}

	if (attacker.critical == 3)//attacker scored a headshot/critical
	{
		if (attacker.critical == 3)
		{

			damage = (damage * 2);

			if (targ.classname == "monster")
				attacker.score = attacker.score + 1;

			sound (targ, CHAN_VOICE, "player/headshot.wav", 1, ATTN_NORM);
	

			makevectors (targ.v_angle);
		}
	}




	
// add to the damage total for clients, which will be sent as a single
// message at the end of the frame
// FIXME: remove after combining shotgun blasts?
	if (targ.flags & FL_CLIENT)
	{
		targ.dmg_take = targ.dmg_take + take;
		targ.dmg_save = targ.dmg_save + save;
		targ.dmg_inflictor = inflictor;
	}

	damage_inflictor = inflictor;        

/*
// figure momentum add
	if ( (inflictor != world) && (targ.movetype == MOVETYPE_WALK) )
	{
		dir = targ.origin - (inflictor.absmin + inflictor.absmax) * 0.5;
		dir = normalize(dir);
		// Set kickback for smaller weapons
//Zoid -- use normal NQ kickback
//		// Read: only if it's not yourself doing the damage
//		if ( (damage < 60) & ((attacker.classname == "player") & (targ.classname == "player")) & ( attacker.netname != targ.netname)) 
//			targ.velocity = targ.velocity + dir * damage * 11;
//		else                        
		// Otherwise, these rules apply to rockets and grenades                        
		// for blast velocity
			targ.velocity = targ.velocity + dir * damage * 8;
		
		// Rocket Jump modifiers
		if ( (rj > 1) & ((attacker.classname == "player") & (targ.classname == "player")) & ( attacker.netname == targ.netname)) 
			targ.velocity = targ.velocity + dir * damage * rj;

	}*/



// check for godmode or invincibility
	if (targ.flags & FL_GODMODE)
		return;
	if (targ.invincible_finished >= time)
	{
		if (self.invincible_sound < time)
		{
			sound (targ, CHAN_ITEM, "items/protect3.wav", 1, ATTN_NORM);
			self.invincible_sound = time + 2;
		}
		return;
	}

// team play damage avoidance
//ZOID 12-13-96: self.team doesn't work in QW.  Use keys
	attackerteam = infokey(attacker, "team");
	targteam = infokey(targ, "team");

	if ((teamplay == 1) && (targteam == attackerteam) &&
		(attacker.classname == "player") && (attackerteam != "") &&
		inflictor.classname !="door")
		return;

	if ((teamplay == 3) && (targteam == attackerteam) &&
		(attacker.classname == "player") && (attackerteam != "") &&
		(targ != attacker)&& inflictor.classname !="door")
		return;
		

take = damage;

// do the damage
	//different sorts of armour simply subtract different amounts
      //this makes armor like the force armor good against many small rounds
      //(SMG, shotguns, swords, etc) but useless against big damage (grenades)
      //power armor, which has the best of both worlds, is also the heaviest :p

	//fix: soldiers inherent armor bonus applies to armor absorb
	//fix: psycho drug gives similar benefits to a soldiers bonus

	if (self.rage == IID_CHEM_PSYCHO)
		take -= 3;

	switch(ToIID(targ.islot3))
	{
	case IID_ARM_DESERT:
		take -= 1;
		break;
	case IID_ARM_LEATHER:
		take -= 2;
		break;
	case IID_ARM_VANDAL:
		take -= 3;
		break;
	case IID_ARM_METAL:
		take -= 1;
		break;
	case IID_ARM_TESLA:
		take -= 5;
		break;
	case IID_ARM_COMBAT:
		take -= 6;
		break;
	case IID_ARM_SEVA:
		take -= 5;
		break;
	case IID_ARM_FORCE:
		take -= 9;
		break;
	case IID_ARM_LPOWER:
		take -= 8;
		break;
	default:
		break;
	}

	switch(ToIID(targ.islot3))
	{
	case IID_ARM_LEATHER:
		take = take * 0.80;
		break;
	case IID_ARM_VANDAL:
		take = take * 0.65;
		break;
	case IID_ARM_METAL:
		take = take * 0.40;
		break;
	case IID_ARM_TESLA:
		take = take * 0.60;
		break;
	case IID_ARM_COMBAT:
		take = take * 0.60;
		break;
	case IID_ARM_SEVA:
		take = take * 0.60;
		break;
	case IID_ARM_FORCE:
		take = take * 0.90;
		break;
	case IID_ARM_LPOWER:
		take = take * 0.40;
		break;
	default:
		break;
	}

	if (targ.classname == "player" && targ.zoom > 0)
	{
		targ.maxspeed = targ.maxspeed * 0.33;
		stuffcmd(targ, "v_idlescale 10\n");
	}
	else if (take > 20 && targ.classname == "player")
	{
		targ.maxspeed = targ.maxspeed * 0.25;
		stuffcmd(targ, "v_cshift 100 0 0 100\n");
		stuffcmd(targ, "v_idlescale 30\n");
	}
	else if (take > 0)
	{
		targ.maxspeed = targ.maxspeed * 0.50;
		stuffcmd(targ, "v_cshift 100 0 0 50\n");
	}


	ArmorNoise(targ, attacker);

	if (take <= 0)
	{
		take = 0;
		return;
	}

	if (targ.hold <= 0)
		targ.health = targ.health - take;

	if (targ.classname == "monster" && targ.enemy.classname != "player" && attacker.classname == "player")
	{
		targ.enemy = attacker;
		targ.think = HuntTarget;
		targ.nextthink = time + 0.1;
	}

	local string lm;

	if (targ.health <= 0)
	{

//vanquishing a monster earns points for all players
	if (attacker.classname == "player" && targ.classname == "monster")
	{
		local entity te;
		te = findradius(attacker.origin, 1000);
		while (te)
		{
			if (te.classname == "player")
				te.score = te.score + 2;

			te = te.chain;
		}
	}

		Killed (targ, attacker);

		return;
	}

// react to the damage
	oldself = self;
	self = targ;

//SERVER

	if ( (self.flags & FL_MONSTER) && attacker != world)
	{
	// get mad unless of the same class (except for soldiers)
		if (self != attacker && attacker != self.enemy)
		{
			if ((self.classname != attacker.classname))
			{
				if (self.enemy.classname == "player")
					self.oldenemy = self.enemy;
				self.enemy = attacker;
				FoundTarget ();
			}
		}
	}

	if (self.th_pain)
	{
		self.th_pain (attacker, take);
	}

	self = oldself;
};



/*
============
T_RadiusDamage
============
*/
void(entity inflictor, entity attacker, float damage, entity ignore, string dtype) T_RadiusDamage =
{
	local   float   points, srange, brange;
	local   entity  head;
	local   vector  org;

	head = findradius(inflictor.origin, damage+40);
	
	while (head)
	{
		//bprint (PRINT_HIGH, head.classname);
		//bprint (PRINT_HIGH, " | ");
		//bprint (PRINT_HIGH, head.netname);
		//bprint (PRINT_HIGH, "\n");
	
		if (head != ignore)
		{
			if (head.takedamage)
			{
				org = head.origin + (head.mins + head.maxs)*0.5;
				points = vlen(inflictor.origin - org);
				points = points + damage*0.50;
				if (points < 0)
					points = 0;
				points = damage - points;
				
				if (points > 0)
				{
					if (CanDamage (head, inflictor))
					{
						head.deathtype = dtype;
						T_Damage (head, inflictor, attacker, points);
					}
				}
			}
		}
		head = head.chain;
	}
};


void (entity inflictor, entity attacker, float damage, entity ignore, float xrange) T_RadiusDamage2 =
{
	local float points;
	local entity head;
	local vector org;
	local float srange;
	local float sub;
	local float damaget;

	head = findradius (inflictor.origin, xrange);
	while (head)
	{
		if (head != ignore)
		{
			if (head.takedamage)
			{
				org = head.origin;
				srange = vlen ((inflictor.origin - org));
				points = (xrange / srange);
				sub = (1 / points);
				damaget = (damage - (damage * sub));
				if (damaget > 0)
					if (CanDamage (head, inflictor))
						T_Damage (head, inflictor, attacker, damaget);
			}
		}
		head = head.chain;
	}
};

/*
============
T_BeamDamage
============
*/
void(entity attacker, float damage) T_BeamDamage =
{
	local   float   points;
	local   entity  head;
	
	head = findradius(attacker.origin, damage+40);
	
	while (head)
	{
		if (head.takedamage)
		{
			points = 0.5*vlen (attacker.origin - head.origin);
			if (points < 0)
				points = 0;
			points = damage - points;
			if (head == attacker)
				points = points * 0.5;
			if (points > 0)
			{
				if (CanDamage (head, attacker))
					T_Damage (head, attacker, attacker, points);
			}
		}
		head = head.chain;
	}
};

