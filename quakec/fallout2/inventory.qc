float IID_NONE = 0;

#define IsMelee(iid) (iid == IID_NONE || (iid >= IID_WP_TOOLKIT && iid <= IID_WP_POWERAXE))
#define IsGrenade(iid) (iid >= IID_GREN_FRAG && iid <= IID_GREN_FLASH)
#define IsRanged(iid) (iid >= IID_WP_USP && iid <= IID_WP_PULSERIFLE)

//weapons with ammo, things with a toggle state
#define NotStackable(iid) (IsRanged(iid))

//the ammoless weapons
float IID_WP_TOOLKIT = 400;
float IID_WP_KNIFE = 401;
float IID_WP_AXE = 402;
float IID_WP_VIBROBLADE = 403;
float IID_WP_POWERAXE = 404;

//the ammoed weapons
float IID_WP_USP = 405;
float IID_WP_DEAGLE = 406;
float IID_WP_NEEDLER = 407;
float IID_WP_ALIENBLASTER = 408;
float IID_WP_PIPERIFLE = 409;
float IID_WP_WINCHESTER = 410;
float IID_WP_MOSSBERG = 411;
float IID_WP_JACKHAMMER = 412;
float IID_WP_MP9 = 413;
float IID_WP_MP7 = 414;
float IID_WP_RANGERMASTER = 415;
float IID_WP_AK112 = 416;
float IID_WP_AK74 = 417;
float IID_WP_DKS1 = 418;
float IID_WP_MOONLIGHT = 419;
float IID_WP_SA80 = 420;
float IID_WP_GAUSERIFLE = 421;
float IID_WP_PULSERIFLE = 422;

//and ammo for those guns
float IID_AM_USP = 505;
float IID_AM_DEAGLE = 506;
float IID_AM_NEEDLER = 507;
float IID_AM_ALIENBLASTER = 508;
float IID_AM_PIPERIFLE = 509;
float IID_AM_WINCHESTER = 510;
float IID_AM_MOSSBERG = 511;
float IID_AM_JACKHAMMER = 512;
float IID_AM_MP9 = 513;
float IID_AM_MP7 = 514;
float IID_AM_RANGERMASTER = 515;
float IID_AM_AK112 = 516;
float IID_AM_AK74 = 517;
float IID_AM_DKS1 = 518;
float IID_AM_MOONLIGHT = 519;
float IID_AM_SA80 = 520;
float IID_AM_GAUSERIFLE = 521;
float IID_AM_PULSERIFLE = 522;

//grenade items
float IID_GREN_FRAG = 101;
float IID_GREN_EMP = 102;
float IID_GREN_SMOKE = 103;
float IID_GREN_FLASH = 104;

//armour
#define IsArmor(iid) (iid >= IID_ARM_SHIRT && iid <= IID_ARM_LPOWER)
float IID_ARM_SHIRT = 201;
float IID_ARM_LEATHER = 202;
float IID_ARM_KEVLAR = 203;
float IID_ARM_METAL = 204;
float IID_ARM_COMBAT = 205;
float IID_ARM_BROTHERHOOD = 206;
float IID_ARM_FORCE = 207;
float IID_ARM_LPOWER = 208;

//stims
#define IsChem(iid) (iid >= IID_CHEM_STIMPACK && iid <= IID_CHEM_BESERK)
float IID_CHEM_STIMPACK = 301;
float IID_CHEM_MEDICALBAG = 302;
float IID_CHEM_SUPERSTIM = 303;
float IID_CHEM_ADRENALINE = 304;
float IID_CHEM_PSYCHO = 305;
float IID_CHEM_BESERK = 306;

float IID_BUILD_MRAMMO = 350;
float IID_BUILD_SHIELDGEN = 351;
float IID_BUILD_AUTODOC = 352;
float IID_BUILD_ROBOFANG = 353;

#define IsShootable(iid) (IsMelee(iid) || IsRanged(iid) || IsGrenade(iid))



//slot1 and slot2 are the two hand slots
//slot3 is the armour slot.
//the other slots are for misilaneous things.

.float          islot1;
.float          islot2;
.float          islot3;
.float          islot4;
.float          islot5;
.float          islot6;
.float          islot7;
.float          islot8;
.float          islot9;
.float          islot10;
.float          islot11;
.float          islot12;
.float          islot13;
.float          islot14;
.float          islot15;
.float          islot16;
#define MAXSLOTS 16

void(float sttype, float stnum, string fieldname) clientstat = #232;
void() SetupStats =
{
	//2 is ev_float
	//32 is the first stat we are allowed to use
	clientstat(2, 32, "islot1");
	clientstat(2, 33, "islot2");
	clientstat(2, 34, "islot3");
	clientstat(2, 35, "islot4");
	clientstat(2, 36, "islot5");
	clientstat(2, 37, "islot6");
	clientstat(2, 38, "islot7");
	clientstat(2, 39, "islot8");
	clientstat(2, 40, "islot9");
	clientstat(2, 41, "islot10");
	clientstat(2, 42, "islot11");
	clientstat(2, 43, "islot12");
	clientstat(2, 44, "islot13");
	clientstat(2, 45, "islot14");
	clientstat(2, 46, "islot15");
	clientstat(2, 47, "islot16");
};


#define ToIID(it) floor(it/512)
#define ToStatus(it) (it&511)
#define SlotVal(iid,st) ((iid*512) | (st&511))


float(entity e, float slotno) ItemInSlot =
{
	if (slotno == 1)
		return e.islot1;
	if (slotno == 2)
		return e.islot2;
	if (slotno == 3)
		return e.islot3;
	if (slotno == 4)
		return e.islot4;
	if (slotno == 5)
		return e.islot5;
	if (slotno == 6)
		return e.islot6;
	if (slotno == 7)
		return e.islot7;
	if (slotno == 8)
		return e.islot8;
	if (slotno == 9)
		return e.islot9;
	if (slotno == 10)
		return e.islot10;
	if (slotno == 11)
		return e.islot11;
	if (slotno == 12)
		return e.islot12;
	if (slotno == 13)
		return e.islot13;
	if (slotno == 14)
		return e.islot14;
	if (slotno == 15)
		return e.islot15;
	if (slotno == 16)
		return e.islot16;

	return 0;
};

void(entity e, float slotno, float item) SetItemSlot =
{
	if (slotno == 1)
		e.islot1 = item;
	else if (slotno == 2)
		e.islot2 = item;
	else if (slotno == 3)
		e.islot3 = item;
	else if (slotno == 4)
		e.islot4 = item;
	else if (slotno == 5)
		e.islot5 = item;
	else if (slotno == 6)
		e.islot6 = item;
	else if (slotno == 7)
		e.islot7 = item;
	else if (slotno == 8)
		e.islot8 = item;
	else if (slotno == 9)
		e.islot9 = item;
	else if (slotno == 10)
		e.islot10 = item;
	else if (slotno == 11)
		e.islot11 = item;
	else if (slotno == 12)
		e.islot12 = item;
	else if (slotno == 13)
		e.islot13 = item;
	else if (slotno == 14)
		e.islot14 = item;
	else if (slotno == 15)
		e.islot15 = item;
	else if (slotno == 16)
		e.islot16 = item;
};

float(entity e, float iid) SlotOfItem =
{
	if (ToIID(e.islot1) == iid)
		return 1;
	if (ToIID(e.islot2) == iid)
		return 2;
	if (ToIID(e.islot3) == iid)
		return 3;
	if (ToIID(e.islot4) == iid)
		return 4;
	if (ToIID(e.islot5) == iid)
		return 5;
	if (ToIID(e.islot6) == iid)
		return 6;
	if (ToIID(e.islot7) == iid)
		return 7;
	if (ToIID(e.islot8) == iid)
		return 8;
	if (ToIID(e.islot9) == iid)
		return 9;
	if (ToIID(e.islot10) == iid)
		return 10;
	if (ToIID(e.islot11) == iid)
		return 11;
	if (ToIID(e.islot12) == iid)
		return 12;
	if (ToIID(e.islot13) == iid)
		return 13;
	if (ToIID(e.islot14) == iid)
		return 14;
	if (ToIID(e.islot15) == iid)
		return 15;
	if (ToIID(e.islot16) == iid)
		return 16;

	return 0;
};

float(entity e) FindEmptySlot =
{
	if (ToIID(e.islot3) == IID_NONE)
		return 3;
	if (ToIID(e.islot4) == IID_NONE)
		return 4;
	if (ToIID(e.islot5) == IID_NONE)
		return 5;
	if (ToIID(e.islot6) == IID_NONE)
		return 6;
	if (ToIID(e.islot7) == IID_NONE)
		return 7;
	if (ToIID(e.islot8) == IID_NONE)
		return 8;
	if (ToIID(e.islot9) == IID_NONE)
		return 9;
	if (ToIID(e.islot10) == IID_NONE)
		return 10;
	if (ToIID(e.islot11) == IID_NONE)
		return 11;
	if (ToIID(e.islot12) == IID_NONE)
		return 12;
	if (ToIID(e.islot13) == IID_NONE)
		return 13;
	if (ToIID(e.islot14) == IID_NONE)
		return 14;
	if (ToIID(e.islot15) == IID_NONE)
		return 15;
	if (ToIID(e.islot16) == IID_NONE)
		return 16;

	return 0;
};

typedef .float slot_t;
slot_t(float slot) SlotField =
{
	if (slot == 1)
		return islot1;
	if (slot == 2)
		return islot2;
	if (slot == 3)
		return islot3;
	if (slot == 4)
		return islot4;
	if (slot == 5)
		return islot5;
	if (slot == 6)
		return islot6;
	if (slot == 7)
		return islot7;
	if (slot == 8)
		return islot8;
	if (slot == 9)
		return islot9;
	if (slot == 10)
		return islot10;
	if (slot == 11)
		return islot11;
	if (slot == 12)
		return islot12;
	if (slot == 13)
		return islot13;
	if (slot == 14)
		return islot14;
	if (slot == 15)
		return islot15;
	if (slot == 16)
		return islot16;


	bprint(PRINT_MEDIUM, "ERROR: Invalid slot number (", ftos(slot), ")\n");
	return islot1;
};


string(float iid) GetItemVModel =
{
	if (iid == IID_NONE)
		return "progs/v_fist.mdl";
	if (iid == IID_WP_KNIFE)
		return "progs/v_knife.mdl";
	if (iid == IID_WP_AXE)
		return "progs/v_axe.mdl";
	if (iid == IID_WP_VIBROBLADE)
		return "progs/v_knife.mdl";
	if (iid == IID_WP_POWERAXE)
		return "progs/v_axe.mdl";
	if (iid == IID_WP_USP)
		return "progs/v_1911.mdl";

	if (iid == IID_WP_DEAGLE)
		return "progs/v_deagle.mdl";
	if (iid == IID_WP_NEEDLER)
		return "progs/v_1911.mdl";
	if (iid == IID_WP_ALIENBLASTER)
		return "progs/v_alien.mdl";
	if (iid == IID_WP_PIPERIFLE)
		return "progs/v_piperifle.mdl";
	if (iid == IID_WP_WINCHESTER)
		return "progs/v_double.mdl";
	if (iid == IID_WP_MOSSBERG)
		return "progs/v_shotgun.mdl";

	if (iid == IID_WP_JACKHAMMER)
		return "progs/v_jackhammer.mdl";
	if (iid == IID_WP_MP9)
		return "progs/v_mp9.mdl";
	if (iid == IID_WP_MP7)
		return "progs/v_smg.mdl";
	if (iid == IID_WP_RANGERMASTER)
 		return "progs/v_rangem.mdl";
	if (iid == IID_WP_AK112)
		return "progs/v_ak47.mdl";
	if (iid == IID_WP_AK74)
		return "progs/v_ak47.mdl";
	if (iid == IID_WP_DKS1)
		return "progs/v_srifle.mdl";
	if (iid == IID_WP_MOONLIGHT)
		return "progs/v_night.mdl";

	if (iid == IID_GREN_FRAG)
		return "progs/v_handgren.mdl";
	if (iid == IID_GREN_EMP)
		return "progs/v_handgren.mdl";
	if (iid == IID_GREN_SMOKE)
		return "progs/v_handgren.mdl";
	if (iid == IID_GREN_FLASH)
		return "progs/v_handgren.mdl";

	bprint(PRINT_MEDIUM, ftos(iid), " without a vmodel!\n");
	return "";
};

string(float iid) GetItemWModel =
{
	if (iid == IID_WP_KNIFE)
		return "progs/w_knife.mdl";
	if (iid == IID_WP_AXE)
		return "progs/w_knife.mdl";
	if (iid == IID_WP_VIBROBLADE)
		return "progs/w_axe.mdl";
	if (iid == IID_WP_POWERAXE)
		return "progs/w_axe.mdl";

	if (iid == IID_WP_USP)
		return "progs/w_1911.mdl";
	if (iid == IID_WP_DEAGLE)
		return "progs/w_deagle.mdl";
	if (iid == IID_WP_NEEDLER)
		return "progs/w_1911.mdl";
	if (iid == IID_WP_ALIENBLASTER)
		return "progs/w_alien.mdl";
	if (iid == IID_WP_PIPERIFLE)
		return "progs/w_pipe.mdl";
	if (iid == IID_WP_WINCHESTER)
		return "progs/w_shotgun.mdl";
	if (iid == IID_WP_MOSSBERG)
		return "progs/w_pipe.mdl";
	if (iid == IID_WP_JACKHAMMER)
		return "progs/w_jackhammer.mdl";

	if (iid == IID_WP_MP9)
		return "progs/w_mp9.mdl";
	if (iid == IID_WP_MP7)
		return "progs/w_mp7.mdl";
	if (iid == IID_WP_RANGERMASTER)
		return "progs/w_rangem.mdl";
	if (iid == IID_WP_AK112)
		return "progs/w_ak47.mdl";
	if (iid == IID_WP_AK74)
		return "progs/w_ak47.mdl";
	if (iid == IID_WP_DKS1)
		return "progs/w_srifle.mdl";
	if (iid == IID_WP_MOONLIGHT)
		return "progs/w_night.mdl";
	if (iid == IID_WP_SA80)
		return "progs/w_sa80.mdl";

	if (iid == IID_WP_GAUSERIFLE)
		return "progs/w_gauss.mdl";
	if (iid == IID_WP_PULSERIFLE)
		return "progs/w_carbine.mdl";

	if (iid == IID_GREN_FRAG)
		return "progs/grenade2.mdl";
	if (iid == IID_GREN_EMP)
		return "progs/grenade2.mdl";
	if (iid == IID_GREN_SMOKE)
		return "progs/grenade2.mdl";
	if (iid == IID_GREN_FLASH)
		return "progs/grenade2.mdl";

	if (iid == IID_CHEM_STIMPACK)
		return "maps/b_bh10.bsp";
	if (iid == IID_CHEM_MEDICALBAG)
		return "maps/b_bh25.bsp";
	if (iid == IID_CHEM_SUPERSTIM)
		return "maps/b_bh100.bsp";

//fixme
	if (iid == IID_CHEM_ADRENALINE)
		return "maps/b_bh10.bsp";
	if (iid == IID_CHEM_PSYCHO)
		return "maps/b_bh25.bsp";
	if (iid == IID_CHEM_BESERK)
		return "maps/b_bh100.bsp";


	return "progs/s_light.spr";	//no model. :/
};

float(float iid) WeaponAmmoType =
{
	if (iid == IID_WP_USP)
		return IID_AM_USP;
	if (iid == IID_WP_DEAGLE)
		return IID_AM_DEAGLE;
	if (iid == IID_WP_NEEDLER)
		return IID_AM_NEEDLER;
	if (iid == IID_WP_ALIENBLASTER)
		return IID_AM_ALIENBLASTER;
	if (iid == IID_WP_PIPERIFLE)
		return IID_AM_PIPERIFLE;
	if (iid == IID_WP_WINCHESTER)
		return IID_AM_WINCHESTER;
	if (iid == IID_WP_MOSSBERG)
		return IID_AM_MOSSBERG;
	if (iid == IID_WP_JACKHAMMER)
		return IID_AM_JACKHAMMER;
	if (iid == IID_WP_MP9)
		return IID_AM_MP9;
	if (iid == IID_WP_MP7)
		return IID_AM_MP7;
	if (iid == IID_WP_RANGERMASTER)
		return IID_AM_RANGERMASTER;
	if (iid == IID_WP_AK112)
		return IID_AM_AK112;
	if (iid == IID_WP_AK74)
		return IID_AM_AK74;
	if (iid == IID_WP_DKS1)
		return IID_AM_DKS1;
	if (iid == IID_WP_MOONLIGHT)
		return IID_AM_MOONLIGHT;
	if (iid == IID_WP_SA80)
		return IID_AM_SA80;
	if (iid == IID_WP_GAUSERIFLE)
		return IID_AM_GAUSERIFLE;
	if (iid == IID_WP_PULSERIFLE)
		return IID_AM_PULSERIFLE;

	return IID_NONE;
};

float(float iid) WeaponMagQuant =
{
	if (iid == IID_WP_USP)
		return 12;
	if (iid == IID_WP_DEAGLE)
		return 7;
	if (iid == IID_WP_NEEDLER)
		return 15;
	if (iid == IID_WP_ALIENBLASTER)
		return 6;
	if (iid == IID_WP_PIPERIFLE)
		return 1;
	if (iid == IID_WP_WINCHESTER)
		return 2;
	if (iid == IID_WP_MOSSBERG)
		return 6;
	if (iid == IID_WP_JACKHAMMER)
		return 10;
	if (iid == IID_WP_MP9)
		return 30;
	if (iid == IID_WP_MP7)
		return 30;
	if (iid == IID_WP_RANGERMASTER)
		return 10;
	if (iid == IID_WP_AK112)
		return 24;
	if (iid == IID_WP_AK74)
		return 30;
	if (iid == IID_WP_DKS1)
		return 8;
	if (iid == IID_WP_MOONLIGHT)
		return 30;
	if (iid == IID_WP_SA80)
		return 30;
	if (iid == IID_WP_GAUSERIFLE)
		return 10;
	if (iid == IID_WP_PULSERIFLE)
		return 40;

	return 0;
};

float(float iid) GetItemWeight =
{
	if (iid == IID_NONE)
		return 0;
	if (iid == IID_WP_KNIFE)
		return 1;
	if (iid == IID_WP_AXE)
		return 2;
	if (iid == IID_WP_VIBROBLADE)
		return 8;
	if (iid == IID_WP_POWERAXE)
		return 6;
	if (iid == IID_WP_USP)
		return 1;
	if (iid == IID_WP_DEAGLE)
		return 2;
	if (iid == IID_WP_NEEDLER)
		return 2;
	if (iid == IID_WP_ALIENBLASTER)
		return 2;
	if (iid == IID_WP_PIPERIFLE)
		return 3;
	if (iid == IID_WP_WINCHESTER)
		return 4;
	if (iid == IID_WP_MOSSBERG)
		return 5;
	if (iid == IID_WP_JACKHAMMER)
		return 6;
	if (iid == IID_WP_MP9)
		return 3;
	if (iid == IID_WP_MP7)
		return 3;
	if (iid == IID_WP_RANGERMASTER)
		return 5;
	if (iid == IID_WP_AK112)
		return 5;
	if (iid == IID_WP_AK74)
		return 5;
	if (iid == IID_WP_DKS1)
		return 7;
	if (iid == IID_WP_MOONLIGHT)
		return 5;
	if (iid == IID_WP_SA80)
		return 5;
	if (iid == IID_WP_GAUSERIFLE)
		return 9;
	if (iid == IID_WP_PULSERIFLE)
		return 10;


	if (iid == IID_ARM_SHIRT)
		return 3;
	if (iid == IID_ARM_LEATHER)
		return 5;
	if (iid == IID_ARM_KEVLAR)
		return 9;
	if (iid == IID_ARM_METAL)
		return 15;
	if (iid == IID_ARM_COMBAT)
		return 12;
	if (iid == IID_ARM_BROTHERHOOD)
		return 17;
	if (iid == IID_ARM_FORCE)
		return 6;
	if (iid == IID_ARM_LPOWER)
		return 20;

	if (iid == IID_BUILD_MRAMMO)
		return 2;
	if (iid == IID_BUILD_SHIELDGEN)
		return 4;
	if (iid == IID_BUILD_AUTODOC)
		return 3;
	if (iid == IID_BUILD_ROBOFANG)
		return 2;


//	bprint(PRINT_MEDIUM, ftos(iid), " without a weight!\n");
	return 0;
};

float(float item) GetItemsWeight =
{
	local float iid;
	iid = ToIID(item);
	if (NotStackable(iid))
		return GetItemWeight(iid);
	else
		return GetItemWeight(iid) * ToStatus(item);
};


string(float iid) GetItemName =
{
	if (iid == IID_NONE)
		return "nothing";


	if (iid == IID_WP_TOOLKIT)
		return "toolkit";
	if (iid == IID_WP_KNIFE)
		return "knife";
	if (iid == IID_WP_AXE)
		return "axe";
	if (iid == IID_WP_VIBROBLADE)
		return "ripper";
	if (iid == IID_WP_POWERAXE)
		return "poweraxe";
	if (iid == IID_WP_USP)
		return "1911";
	if (iid == IID_WP_DEAGLE)
		return "desert eagle";
	if (iid == IID_WP_NEEDLER)
		return "needler";
	if (iid == IID_WP_ALIENBLASTER)
		return "alien blaster";
	if (iid == IID_WP_PIPERIFLE)
		return "pipe rifle";
	if (iid == IID_WP_WINCHESTER)
		return "winchester";
	if (iid == IID_WP_MOSSBERG)
		return "mossberg";
	if (iid == IID_WP_JACKHAMMER)
		return "jackhammer";
	if (iid == IID_WP_MP9)
		return "mp9";
	if (iid == IID_WP_MP7)
		return "grease gun";
	if (iid == IID_WP_RANGERMASTER)
		return "rangemaster";
	if (iid == IID_WP_AK112)
		return "ak-112";
	if (iid == IID_WP_AK74)
		return "ak-74";
	if (iid == IID_WP_DKS1)
		return "dks-1";
	if (iid == IID_WP_MOONLIGHT)
		return "moonlight";
	if (iid == IID_WP_SA80)
		return "sa-80";
	if (iid == IID_WP_GAUSERIFLE)
		return "gauss rifle";
	if (iid == IID_WP_PULSERIFLE)
		return "laser carbine";


	if (iid == IID_AM_USP)
		return "ammo for 1911";
	if (iid == IID_AM_DEAGLE)
		return "ammo for desert eagle";
	if (iid == IID_AM_NEEDLER)
		return "ammo for needler";
	if (iid == IID_AM_ALIENBLASTER)
		return "ammo for alien blaster";
	if (iid == IID_AM_PIPERIFLE)
		return "ammo for pipe rifle";
	if (iid == IID_AM_WINCHESTER)
		return "ammo for winchester";
	if (iid == IID_AM_MOSSBERG)
		return "ammo for mossberg";
	if (iid == IID_AM_JACKHAMMER)
		return "ammo for jackhammer";
	if (iid == IID_AM_MP9)
		return "ammo for mp9";
	if (iid == IID_AM_MP7)
		return "ammo for grease gun";
	if (iid == IID_AM_RANGERMASTER)
		return "ammo for rangemaster";
	if (iid == IID_AM_AK112)
		return "ammo for ak-112";
	if (iid == IID_AM_AK74)
		return "ammo for ak-74";
	if (iid == IID_AM_DKS1)
		return "ammo for dks-1";
	if (iid == IID_AM_MOONLIGHT)
		return "ammo for moonlight";
	if (iid == IID_AM_SA80)
		return "ammo for sa-80";
	if (iid == IID_AM_GAUSERIFLE)
		return "ammo for gauss rifle";
	if (iid == IID_AM_PULSERIFLE)
		return "ammo for laser carbine";


	if (iid == IID_GREN_FRAG)
		return "frag grenade";
	if (iid == IID_GREN_EMP)
		return "emp grenade";
	if (iid == IID_GREN_SMOKE)
		return "smoke grenade";
	if (iid == IID_GREN_FLASH)
		return "flashbang";


	if (iid == IID_ARM_SHIRT)
		return "bulletproof shirt";
	if (iid == IID_ARM_LEATHER)
		return "leather armor";
	if (iid == IID_ARM_KEVLAR)
		return "kevlar armor";
	if (iid == IID_ARM_METAL)
		return "metal armor";
	if (iid == IID_ARM_COMBAT)
		return "combat armor";
	if (iid == IID_ARM_BROTHERHOOD)
		return "brotherhood armor";
	if (iid == IID_ARM_FORCE)
		return "force armor";
	if (iid == IID_ARM_LPOWER)
		return "light power armor";


	if (iid == IID_CHEM_STIMPACK)
		return "stimpack";
	if (iid == IID_CHEM_MEDICALBAG)
		return "medical bag";
	if (iid == IID_CHEM_SUPERSTIM)
		return "superstim";
	if (iid == IID_CHEM_ADRENALINE)
		return "adrenaline";
	if (iid == IID_CHEM_PSYCHO)
		return "psycho";
	if (iid == IID_CHEM_BESERK)
		return "beserk";

	if (iid == IID_BUILD_MRAMMO)
		return "mr. ammo";
	if (iid == IID_BUILD_SHIELDGEN)
		return "shield-gen";
	if (iid == IID_BUILD_AUTODOC)
		return "auto-doc";
	if (iid == IID_BUILD_ROBOFANG)
		return "robofang";

	bprint(PRINT_MEDIUM, ftos(iid), " without a name!\n");
	return "unknown";
};



float(float slotno, float iid) FitsInSlot =
{
	if (slotno == 1 || slotno == 2)
		return IsShootable(iid);
	if (slotno == 3)
		return IsArmor(iid);
	return true;
};