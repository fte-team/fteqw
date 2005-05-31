
// Effects code (EFF) --

// TE effects/muzzleflash/messages

// modular efs
var float ef_red  = 0;           // used for glow on pent item
var float ef_pent = EF_DIMLIGHT; // used for pent effect on player
var float ef_blue = 0;           // used for glow on quad
var float ef_quad = EF_DIMLIGHT; // used for quad effect on player

/*
================
SpawnBlood
================
*/
void(vector org, float damage) _SpawnBlood_TEBlood =
{
	te_blood(org, '0 0 0', damage*2);
};

void(vector org, float damage) _SpawnBlood =
{
#ifdef NETQUAKE
	particle (org, '0 0 0', 73, damage*2);
#else
	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_BLOOD);
	WriteByte (MSG_MULTICAST, damage);
	WriteCoord (MSG_MULTICAST, org_x);
	WriteCoord (MSG_MULTICAST, org_y);
	WriteCoord (MSG_MULTICAST, org_z);
	multicast (org, MULTICAST_PVS);
#endif
};

#ifdef NETQUAKE
void() s_explode6 = [5,		SUB_Remove] {};
void() s_explode5 = [4,		s_explode6] {};
void() s_explode4 = [3,		s_explode5] {};
void() s_explode3 = [2,		s_explode4] {};
void() s_explode2 = [1,		s_explode3] {};
void() s_explode1 = [0,		s_explode2] {};

void(vector org) CreateExplosion =
{
	local entity s;

	s = self; // save old self

	self = spawn();

	self.movetype = MOVETYPE_NONE;
	self.velocity = '0 0 0';
	self.touch = SUB_Null;
	setmodel (self, "progs/s_explod.spr");
	setorigin (self, org);
	self.solid = SOLID_NOT;
	s_explode1 ();

	self = s; // restore old self
};
#endif

void(vector org) _TE_explosion =
{
#ifdef NETQUAKE
	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_EXPLOSION);
	WriteCoord (MSG_BROADCAST, org_x);
	WriteCoord (MSG_BROADCAST, org_y);
	WriteCoord (MSG_BROADCAST, org_z);
	CreateExplosion(org);
#else
	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_EXPLOSION);
	WriteCoord (MSG_MULTICAST, org_x);
	WriteCoord (MSG_MULTICAST, org_y);
	WriteCoord (MSG_MULTICAST, org_z);
	multicast (org, MULTICAST_PHS);
#endif
};

void(vector org) _TE_gunshot =
{
#ifdef NETQUAKE
	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_GUNSHOT);
	WriteCoord (MSG_BROADCAST, org_x);
	WriteCoord (MSG_BROADCAST, org_y);
	WriteCoord (MSG_BROADCAST, org_z);
#else
	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_GUNSHOT);
	WriteByte (MSG_MULTICAST, 3);
	WriteCoord (MSG_MULTICAST, org_x);
	WriteCoord (MSG_MULTICAST, org_y);
	WriteCoord (MSG_MULTICAST, org_z);
	multicast (org, MULTICAST_PVS);
#endif
};

void(vector org) _TE_spike =
{
#ifdef NETQUAKE
	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_SPIKE);
	WriteCoord (MSG_BROADCAST, org_x);
	WriteCoord (MSG_BROADCAST, org_y);
	WriteCoord (MSG_BROADCAST, org_z);
#else
	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_SPIKE);
	WriteCoord (MSG_MULTICAST, org_x);
	WriteCoord (MSG_MULTICAST, org_y);
	WriteCoord (MSG_MULTICAST, org_z);
	multicast (org, MULTICAST_PHS);
#endif
};

void(vector org) _TE_knightspike =
{
#ifdef NETQUAKE
	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_KNIGHTSPIKE);
	WriteCoord (MSG_BROADCAST, org_x);
	WriteCoord (MSG_BROADCAST, org_y);
	WriteCoord (MSG_BROADCAST, org_z);
#else
	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_KNIGHTSPIKE);
	WriteCoord (MSG_MULTICAST, org_x);
	WriteCoord (MSG_MULTICAST, org_y);
	WriteCoord (MSG_MULTICAST, org_z);
	multicast (org, MULTICAST_PHS);
#endif
};

void(vector org) _TE_wizspike =
{
#ifdef NETQUAKE
	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_WIZSPIKE);
	WriteCoord (MSG_BROADCAST, org_x);
	WriteCoord (MSG_BROADCAST, org_y);
	WriteCoord (MSG_BROADCAST, org_z);
#else
	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_WIZSPIKE);
	WriteCoord (MSG_MULTICAST, org_x);
	WriteCoord (MSG_MULTICAST, org_y);
	WriteCoord (MSG_MULTICAST, org_z);
	multicast (org, MULTICAST_PHS);
#endif
};

void(vector org) _TE_superspike =
{
#ifdef NETQUAKE
	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_SUPERSPIKE);
	WriteCoord (MSG_BROADCAST, org_x);
	WriteCoord (MSG_BROADCAST, org_y);
	WriteCoord (MSG_BROADCAST, org_z);
#else
	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_SUPERSPIKE);
	WriteCoord (MSG_MULTICAST, org_x);
	WriteCoord (MSG_MULTICAST, org_y);
	WriteCoord (MSG_MULTICAST, org_z);
	multicast (org, MULTICAST_PHS);
#endif
};

void(vector org) _TE_teleport =
{
#ifdef NETQUAKE
	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_TELEPORT);
	WriteCoord (MSG_BROADCAST, org_x);
	WriteCoord (MSG_BROADCAST, org_y);
	WriteCoord (MSG_BROADCAST, org_z);
#else
	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_TELEPORT);
	WriteCoord (MSG_MULTICAST, org_x);
	WriteCoord (MSG_MULTICAST, org_y);
	WriteCoord (MSG_MULTICAST, org_z);
	multicast (org, MULTICAST_PHS);
#endif
};

void(entity ent, vector start, vector end) _TE_lightning2 =
{
#ifdef NETQUAKE
	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_LIGHTNING2);
	WriteEntity (MSG_BROADCAST, ent);
	WriteCoord (MSG_BROADCAST, start_x);
	WriteCoord (MSG_BROADCAST, start_y);
	WriteCoord (MSG_BROADCAST, start_z);
	WriteCoord (MSG_BROADCAST, end_x);
	WriteCoord (MSG_BROADCAST, end_y);
	WriteCoord (MSG_BROADCAST, end_z);
#else
	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_LIGHTNING2);
	WriteEntity (MSG_MULTICAST, ent);
	WriteCoord (MSG_MULTICAST, start_x);
	WriteCoord (MSG_MULTICAST, start_y);
	WriteCoord (MSG_MULTICAST, start_z);
	WriteCoord (MSG_MULTICAST, end_x);
	WriteCoord (MSG_MULTICAST, end_y);
	WriteCoord (MSG_MULTICAST, end_z);
	multicast (start, MULTICAST_PHS);
#endif
};

void(vector org) TE_lightningblood =
{
#ifdef NETQUAKE
	particle(org, '0 0 100', 225, 120);
#else
	WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
	WriteByte (MSG_MULTICAST, TE_LIGHTNINGBLOOD);
	WriteCoord (MSG_MULTICAST, org_x);
	WriteCoord (MSG_MULTICAST, org_y);
	WriteCoord (MSG_MULTICAST, org_z);
	multicast (org, MULTICAST_PVS);
#endif
};

// function pointers for TE calls
var void(vector org, float damage) SpawnBlood = _SpawnBlood;
var void(vector org) TE_explosion = _TE_explosion;
var void(vector org) TE_gunshot = _TE_gunshot;
var void(vector org) TE_spike = _TE_spike;
var void(vector org) TE_knightspike = _TE_knightspike;
var void(vector org) TE_wizspike = _TE_wizspike;
var void(vector org) TE_superspike = _TE_superspike;
var void(vector org) TE_teleport = _TE_teleport;
var void(entity ent, vector start, vector end) TE_lightning2 = _TE_lightning2;

// set effects function, takes function pointers and assigns them based on detected builtins
void() EFF_SetEffects =
{
	if (eng_support & ENG_EFRED)
	{
		ef_pent = 128;
		ef_red = 128;
	}

	if (eng_support & ENG_EFBLUE)
	{
		ef_blue = 64;
		ef_quad = 64;
	}

	if (eng_support & ENG_TEBUILTINS)
	{
		// use TE_ builtins instead
		TE_explosion = _te_explosion;
		TE_gunshot = _te_gunshot;
		TE_spike = _te_spike;
		TE_knightspike = _te_knightspike;
		TE_wizspike = _te_wizspike;
		TE_superspike = _te_superspike;
		TE_teleport = _te_teleport;
		TE_lightning2 = _te_lightning2;
	}

	if (eng_support & ENG_TEBLOOD)
		SpawnBlood = _SpawnBlood_TEBlood; // use TE_Blood builtin instead
};

// view kicks
void(entity ent) VK_smallkick =
{
#ifdef NETQUAKE
	self.punchangle_x = -2;
#else
	msg_entity = ent;
	WriteByte (MSG_ONE, SVC_SMALLKICK);
#endif
}

void(entity ent) VK_bigkick =
{
#ifdef NETQUAKE
	self.punchangle_x = -4;
#else
	msg_entity = ent;
	WriteByte (MSG_ONE, SVC_BIGKICK);
#endif
}

// muzzle flash
void() muzzleflash =
{
#ifdef NETQUAKE
	self.effects |= EF_MUZZLEFLASH;
#else
	WriteByte (MSG_MULTICAST, SVC_MUZZLEFLASH);
	WriteEntity (MSG_MULTICAST, self);
	multicast (self.origin, MULTICAST_PVS);
#endif
};

// touch blood functions
void(float damage) spawn_touchblood =
{
	local vector    vel;

	vel = normalize (self.velocity);
	vel = normalize(vel + v_up*(random()- 0.5) + v_right*(random()- 0.5));
	vel = vel + 2*trace_plane_normal;
	vel = vel * 0.4;
	SpawnBlood (self.origin + vel, damage);
};
