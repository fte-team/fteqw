float IID_NONE = 0;

#define IsMelee(iid) (iid == IID_NONE || (iid >= IID_WP_WRENCH && iid <= IID_WP_POWERAXE))
#define IsGrenade(iid) (iid >= IID_GREN_FRAG && iid <= IID_GREN_STUN)
#define IsRanged(iid) (iid >= IID_WP_GLOCK && iid <= IID_WP_ACR)


//weapons with ammo, things with a toggle state
#define NotStackable(iid) (IsRanged(iid))

//the ammoless weapons
float IID_WP_WRENCH = 399;
float IID_WP_KNIFE = 400;
float IID_WP_SLEDGE = 401;
float IID_WP_SPEAR = 402;
float IID_WP_POWERAXE = 403;

//the ammoed weapons
float IID_WP_GLOCK = 404;
float IID_WP_USP = 405;
float IID_WP_DEAGLE = 406;
float IID_WP_NEEDLER = 407;
float IID_WP_ALIENBLASTER = 408;
float IID_WP_WINCHESTER = 409;
float IID_WP_MOSSBERG = 410;
float IID_WP_JACKHAMMER = 411;
float IID_WP_GREASEGUN = 412;
float IID_WP_MP9 = 413;
float IID_WP_RANGEMASTER = 414;
float IID_WP_AK112 = 415;
float IID_WP_AK74 = 416;
float IID_WP_DKS1 = 417;
float IID_WP_MOONLIGHT = 418;
float IID_WP_G11 = 419;
float IID_WP_GLOCK_S = 420;
float IID_WP_USP_S = 421;
float IID_WP_GREASEGUN_S = 422;
float IID_WP_MP9_S = 423;
float IID_WP_PIPERIFLE = 424;
float IID_WP_PIPERIFLE_R = 425;
float IID_WP_PIPERIFLE_S = 426;
float IID_WP_PIPERIFLE_SCOPE = 427;
float IID_WP_PIPERIFLE_SCOPE_S = 428;
float IID_WP_PIPERIFLE_SCOPE_R = 429;
float IID_WP_PIPERIFLE_SCOPE_S_R = 430;
float IID_WP_PIPERIFLE_S_R = 431;
float IID_WP_DEAGLE_M = 432;
float IID_WP_AK112_M = 433;

float IID_WP_GAUSERIFLE = 434;      //2mm EC
float IID_WP_PULSERIFLE = 435;
float IID_WP_PLASMACARBINE = 436;
float IID_WP_TURBOPLASMA = 437;
float IID_WP_FNFAL = 438;		//.308 AP
float IID_WP_ROCKETLAUNCHER = 439;
float IID_WP_LASERGATLING = 440;
float IID_WP_DKS1_S = 441;
float IID_WP_RANGEMASTER_SCOPE = 442;
float IID_WP_AK47 = 443;
float IID_WP_ACR = 444;
float IID_WP_GRENADELAUNCHER = 445;

//and ammo for those guns
float IID_AM_NEEDLER = 507;
float IID_AM_10MM = 508; //10mm ammo";
float IID_AM_556MM = 509; //"5.56mm ammo";
float IID_AM_5MMHIGHVEL = 510; //"5mm high-velocity ammo";
float IID_AM_12GAUGESHELLS = 511; //"12-guage shotgun shells";
float IID_AM_ENERGYCELL = 512; //"small energy cell";
float IID_AM_2MMEC = 513; //12mmec ammo";
float IID_AM_762MM = 514; //"7.62mm ammo";
float IID_AM_44MAGNUM = 515; //".44 magnum ammo";
float IID_AM_45ACP = 516; //".45 ACP ammo";
float IID_AM_ROCKET = 517; //"explosive rocket"
float IID_AM_CASELESS = 518; //caseless
float IID_AM_WARSAW = 519; //ak-47 ammo
float IID_AM_GRENADE = 520; //grenade launcher ammo

//grenade items
float IID_GREN_FRAG = 101;
float IID_GREN_EMP = 102;
float IID_GREN_SMOKE = 103;
float IID_GREN_FLASH = 104;
float IID_GREN_STUN = 105;
float IID_GREN_FLARE = 106;

//armour
#define IsArmor(iid) (iid >= IID_ARM_DESERT && iid <= IID_ARM_LPOWER)
float IID_ARM_DESERT = 200;
float IID_ARM_LEATHER = 201;
float IID_ARM_VANDAL = 202;
float IID_ARM_METAL = 203;
float IID_ARM_TESLA = 204;
float IID_ARM_COMBAT = 205;
float IID_ARM_SEVA = 206;
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
float IID_CHEM_RADX = 307;

float IID_BUILD_MRAMMO = 350;
float IID_BUILD_SHIELDGEN = 351;
float IID_BUILD_AUTODOC = 352;
float IID_BUILD_ROBOFANG = 353;
float IID_BUILD_TTURRET = 354;
float IID_BUILD_TELEPAD = 355;
float IID_BUILD_RTURRET = 356;
float IID_BUILD_GTURRET = 357;

#define IsEquip(iid) (iid >= IID_EQUIP_MEDIC_BAG && iid <= IID_EQUIP_EXTENDER)
#define IsJunk(iid) (iid >= IID_MISC_JUNK && iid <= IID_MISC_XRAYTUBE)

float IID_EQUIP_MEDIC_BAG = 375;
float IID_EQUIP_GOGGLES = 376;
float IID_EQUIP_SILENCER = 377;
float IID_EQUIP_BELTPOUCH = 378;
float IID_EQUIP_BACKPACK = 379;
float IID_EQUIP_TOOLKIT = 380;
float IID_EQUIP_CLIMBINGGEAR = 381;
float IID_EQUIP_BATTERY = 382;
float IID_EQUIP_STEALTHBOY = 383;
float IID_EQUIP_SPRINTKIT = 384;
float IID_EQUIP_HOVERBOOTS = 385;
float IID_EQUIP_XRAY = 386;
float IID_EQUIP_EXTENDER = 387;

float IID_MISC_JUNK = 310;
float IID_MISC_NUKACOLA = 311;
float IID_MISC_CHEMICALS = 312;
float IID_MISC_AEROSOL = 313;
float IID_MISC_RDXCRYSTAL = 314;
float IID_MISC_STEELPIPE = 315;
float IID_MISC_DUCKTAPE = 316;
float IID_MISC_LCD = 317;
float IID_MISC_CIRCUITBOARD = 318;
float IID_MISC_COPPERWIRE = 319;
float IID_MISC_HMXCOMPOUND = 320;
float IID_MISC_GUM = 321;
float IID_MISC_XRAYTUBE = 322;


#define IsShootable(iid) (IsMelee(iid) || IsRanged(iid) || IsGrenade(iid))

float(float slotno, float iid) FitsInSlot;

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
.float          islot17;
.float          islot18;
.float          islot19;
.float          islot20;
.float          islot21;
.float          islot22;
.float          islot23;
.float          islot24;
.float          mixslot1;
.float          mixslot2;
.float          xslot1;
.float          xslot2;
.float          xslot3;
.float          xslot4;
.float          xslot5;
.float          xslot6;
.float          xslot7;
.float          xslot8;
.float          xslot9;
.float          xslot10;
.float          xslot11;
.float          xslot12;
.float          xslot13;
.float          xslot14;
.float          xslot15;
.float          xslot16;
.float          xslot17;
.float          xslot18;
.float          xslot19;
.float          xslot20;

#define MAXSLOTS 26



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
	if (slotno == 17)
		return e.islot17;
	if (slotno == 18)
		return e.islot18;
	if (slotno == 19)
		return e.islot19;
	if (slotno == 20)
		return e.islot20;
	if (slotno == 21)
		return e.islot21;
	if (slotno == 22)
		return e.islot22;
	if (slotno == 23)
		return e.islot23;
	if (slotno == 24)
		return e.islot24;
	if (slotno == 25)
		return e.mixslot1;
	if (slotno == 26)
		return e.mixslot2;
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
	else if (slotno == 17)
		e.islot17 = item;
	else if (slotno == 18)
		e.islot18 = item;
	else if (slotno == 19)
		e.islot19 = item;
	else if (slotno == 20)
		e.islot20 = item;
	else if (slotno == 21)
		e.islot21 = item;
	else if (slotno == 22)
		e.islot22 = item;
	else if (slotno == 23)
		e.islot23 = item;
	else if (slotno == 24)
		e.islot24 = item;
	else if (slotno == 25)
		e.mixslot1 = item;
	else if (slotno == 26)
		e.mixslot2 = item;
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
	if (ToIID(e.islot17) == iid)
		return 17;
	if (ToIID(e.islot18) == iid)
		return 18;
	if (ToIID(e.islot19) == iid)
		return 19;
	if (ToIID(e.islot20) == iid)
		return 20;
	if (ToIID(e.islot21) == iid)
		return 21;
	if (ToIID(e.islot22) == iid)
		return 22;
	if (ToIID(e.islot23) == iid)
		return 23;
	if (ToIID(e.islot24) == iid)
		return 24;
	if (ToIID(e.islot23) == iid)
		return 25;
	if (ToIID(e.islot24) == iid)
		return 26;
	return 0;
};

float(entity e, float iid) ShadowSlotOfItem =
{
	if (ToIID(getstati(32)) == iid)
		return 1;
	if (ToIID(getstati(33)) == iid)
		return 2;
	if (ToIID(getstati(34)) == iid)
		return 3;
	if (ToIID(getstati(35)) == iid)
		return 4;
	if (ToIID(getstati(36)) == iid)
		return 5;
	if (ToIID(getstati(37)) == iid)
		return 6;
	if (ToIID(getstati(38)) == iid)
		return 7;
	if (ToIID(getstati(39)) == iid)
		return 8;
	if (ToIID(getstati(40)) == iid)
		return 9;
	if (ToIID(getstati(41)) == iid)
		return 10;
	if (ToIID(getstati(42)) == iid)
		return 11;
	if (ToIID(getstati(43)) == iid)
		return 12;
	if (ToIID(getstati(44)) == iid)
		return 13;
	if (ToIID(getstati(45)) == iid)
		return 14;
	if (ToIID(getstati(46)) == iid)
		return 15;
	if (ToIID(getstati(47)) == iid)
		return 16;

	if (ToIID(getstati(91)) == iid)
		return 17;
	if (ToIID(getstati(92)) == iid)
		return 18;
	if (ToIID(getstati(93)) == iid)
		return 19;
	if (ToIID(getstati(94)) == iid)
		return 20;
	if (ToIID(getstati(95)) == iid)
		return 21;
	if (ToIID(getstati(96)) == iid)
		return 22;
	if (ToIID(getstati(97)) == iid)
		return 23;
	if (ToIID(getstati(98)) == iid)
		return 24;
	if (ToIID(getstati(104)) == iid)
		return 25;
	if (ToIID(getstati(105)) == iid)
		return 26;
	return 0;
};

float(entity e, float iid) TotalQuantity =
{	//iid must be stackable.
	local float ret;
	if (ToIID(e.islot1) == iid)
		ret += ToStatus(e.islot1);
	if (ToIID(e.islot2) == iid)
		ret += ToStatus(e.islot2);
	if (ToIID(e.islot3) == iid)
		ret += ToStatus(e.islot3);
	if (ToIID(e.islot4) == iid)
		ret += ToStatus(e.islot4);
	if (ToIID(e.islot5) == iid)
		ret += ToStatus(e.islot5);
	if (ToIID(e.islot6) == iid)
		ret += ToStatus(e.islot6);
	if (ToIID(e.islot7) == iid)
		ret += ToStatus(e.islot7);
	if (ToIID(e.islot8) == iid)
		ret += ToStatus(e.islot8);
	if (ToIID(e.islot9) == iid)
		ret += ToStatus(e.islot9);
	if (ToIID(e.islot10) == iid)
		ret += ToStatus(e.islot10);
	if (ToIID(e.islot11) == iid)
		ret += ToStatus(e.islot11);
	if (ToIID(e.islot12) == iid)
		ret += ToStatus(e.islot12);
	if (ToIID(e.islot13) == iid)
		ret += ToStatus(e.islot13);
	if (ToIID(e.islot14) == iid)
		ret += ToStatus(e.islot14);
	if (ToIID(e.islot15) == iid)
		ret += ToStatus(e.islot15);
	if (ToIID(e.islot16) == iid)
		ret += ToStatus(e.islot16);
	if (ToIID(e.islot17) == iid)
		ret += ToStatus(e.islot17);
	if (ToIID(e.islot18) == iid)
		ret += ToStatus(e.islot18);
	if (ToIID(e.islot19) == iid)
		ret += ToStatus(e.islot19);
	if (ToIID(e.islot20) == iid)
		ret += ToStatus(e.islot20);
	if (ToIID(e.islot21) == iid)
		ret += ToStatus(e.islot21);
	if (ToIID(e.islot22) == iid)
		ret += ToStatus(e.islot22);
	if (ToIID(e.islot23) == iid)
		ret += ToStatus(e.islot23);
	if (ToIID(e.islot24) == iid)
		ret += ToStatus(e.islot24);
	if (ToIID(e.mixslot1) == iid)
		ret += ToStatus(e.mixslot1);
	if (ToIID(e.mixslot2) == iid)
		ret += ToStatus(e.mixslot2);

	return ret;
};

float(entity e) FindEmptySlot =
{

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

float(entity e, float iid) FindSuitableEmptySlot =
{
	if (!IsJunk(iid))
	{
	if (ToIID(e.islot7) == IID_NONE)
		if (FitsInSlot(7, iid))
			return 7;
	if (ToIID(e.islot8) == IID_NONE)
		if (FitsInSlot(8, iid))
			return 8;
	if (ToIID(e.islot9) == IID_NONE)
		if (FitsInSlot(9, iid))
			return 9;
	if (ToIID(e.islot10) == IID_NONE)
		if (FitsInSlot(10, iid))
			return 10;
	if (ToIID(e.islot11) == IID_NONE)
		if (FitsInSlot(11, iid))
			return 11;
	if (ToIID(e.islot12) == IID_NONE)
		if (FitsInSlot(12, iid))
			return 12;
	if (ToIID(e.islot13) == IID_NONE)
		if (FitsInSlot(13, iid))
			return 13;
	if (ToIID(e.islot14) == IID_NONE)
		if (FitsInSlot(14, iid))
			return 14;
	if (ToIID(e.islot15) == IID_NONE)
		if (FitsInSlot(15, iid))
			return 15;
	if (ToIID(e.islot16) == IID_NONE)
		if (FitsInSlot(16, iid))
			return 16;
	if (ToIID(e.islot15) == IID_NONE)
		if (FitsInSlot(23, iid))
			return 23;
	if (ToIID(e.islot16) == IID_NONE)
		if (FitsInSlot(24, iid))
			return 24;
	}
	else if (IsJunk(iid))
	{
		if (ToIID(e.islot17) == IID_NONE)
				return 17;
		if (ToIID(e.islot18) == IID_NONE)
				return 18;
		if (ToIID(e.islot19) == IID_NONE)
				return 19;
		if (ToIID(e.islot20) == IID_NONE)
				return 20;
		if (ToIID(e.islot21) == IID_NONE)
				return 21;
		if (ToIID(e.islot22) == IID_NONE)
				return 22;
	}
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
	if (slot == 17)
		return islot17;
	if (slot == 18)
		return islot18;
	if (slot == 19)
		return islot19;
	if (slot == 20)
		return islot20;
	if (slot == 21)
		return islot21;
	if (slot == 22)
		return islot22;
	if (slot == 23)
		return islot23;
	if (slot == 24)
		return islot24;

	//bprint(PRINT_MEDIUM, "ERROR: Invalid slot number (", ftos(slot), ")\n");
	return islot1;
};


string(float iid) GetItemVModel =
{
	if (iid == IID_NONE)
		return "progs/v_fist.mdl";
	if (iid == IID_WP_WRENCH)
		return "progs/v_span.mdl";
	if (iid == IID_WP_KNIFE)
		return "progs/v_knife.mdl";
	if (iid == IID_WP_SLEDGE)
		return "progs/v_disruptor.mdl";
	if (iid == IID_WP_SPEAR)
		return "progs/v_spear.mdl";
	if (iid == IID_WP_POWERAXE)
		return "progs/v_disruptor.mdl";
	if (iid == IID_WP_USP)
		return "progs/v_usp.mdl";
	if (iid == IID_WP_USP_S)
		return "progs/v_usp.mdl";
	if (iid == IID_WP_GLOCK)
		return "progs/v_10mmpistol.mdl";
	if (iid == IID_WP_GLOCK_S)
		return "progs/v_10mmpistol.mdl";
	if (iid == IID_WP_DEAGLE)
		return "progs/v_deagle.mdl";
	if (iid == IID_WP_NEEDLER)
		return "progs/v_needler.mdl";
	if (iid == IID_WP_ALIENBLASTER)
		return "progs/v_blaster.mdl";
	if (iid >= IID_WP_PIPERIFLE && iid <= IID_WP_PIPERIFLE_S_R)
		return "progs/v_pipe.mdl";
	if (iid == IID_WP_WINCHESTER)
		return "progs/v_double.mdl";
	if (iid == IID_WP_MOSSBERG)
		return "progs/v_combat.mdl";

	if (iid == IID_WP_JACKHAMMER)
		return "progs/v_jackhammer.mdl";
	if (iid == IID_WP_GREASEGUN)
		return "progs/v_smg.mdl";
	if (iid == IID_WP_MP9)
		return "progs/v_mp9.mdl";
	if (iid == IID_WP_GREASEGUN_S)
		return "progs/v_smg.mdl";
	if (iid == IID_WP_MP9_S)
		return "progs/v_mp9.mdl";
	if (iid == IID_WP_RANGEMASTER)
 		return "progs/v_rangem.mdl";
	if (iid == IID_WP_RANGEMASTER_SCOPE)
 		return "progs/v_rangem_s.mdl";
	if (iid == IID_WP_AK112)
		return "progs/v_ak47.mdl";
	if (iid == IID_WP_AK74)
		return "progs/v_ak47.mdl";
	if (iid == IID_WP_ACR)
		return "progs/v_bozar.mdl";
	if (iid == IID_WP_DKS1)
		return "progs/v_srifle.mdl";
	if (iid == IID_WP_DKS1_S)
		return "progs/v_srifle.mdl";
	if (iid == IID_WP_MOONLIGHT)
		return "progs/v_night.mdl";
	if (iid == IID_WP_FNFAL)
 		return "progs/v_fnfal.mdl";
	if (iid == IID_WP_AK112_M)
		return "progs/v_lsw.mdl";

	if (iid == IID_WP_G11)
		return "progs/v_g11.mdl";
	if (iid == IID_WP_GAUSERIFLE)
		return "progs/v_gauss.mdl";
	if (iid == IID_WP_PULSERIFLE)
		return "progs/v_plasma.mdl";
	if (iid == IID_WP_TURBOPLASMA)
		return "progs/v_plasma.mdl";
	if (iid == IID_WP_PLASMACARBINE)
		return "progs/v_carbine.mdl";
	if (iid == IID_WP_ROCKETLAUNCHER)
		return "progs/v_rocket.mdl";
	if (iid == IID_WP_GRENADELAUNCHER)
		return "progs/v_gl.mdl";
	if (iid == IID_WP_LASERGATLING)
		return "progs/v_chain.mdl";

	if (iid == IID_GREN_FRAG)
		return "progs/v_handgren.mdl";
	if (iid == IID_GREN_STUN)
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
	if (iid == IID_WP_WRENCH)
		return "progs/w_span.mdl";
	if (iid == IID_WP_SLEDGE)
		return "progs/w_knife.mdl";
	if (iid == IID_WP_SPEAR)
		return "progs/w_spear.mdl";
	if (iid == IID_WP_POWERAXE)
		return "progs/w_axe.mdl";

	if (iid == IID_WP_USP)
		return "progs/w_usp.mdl";
	if (iid == IID_WP_USP_S)
		return "progs/w_usp.mdl";
	if (iid == IID_WP_GLOCK)
		return "progs/w_10mmpistol.mdl";
	if (iid == IID_WP_GLOCK_S)
		return "progs/w_10mmpistol.mdl";
	if (iid == IID_WP_DEAGLE)
		return "progs/w_deagle.mdl";
	if (iid == IID_WP_DEAGLE_M)
		return "progs/w_deagle.mdl";
	if (iid == IID_WP_NEEDLER)
		return "progs/w_usp.mdl";
	if (iid == IID_WP_ALIENBLASTER)
		return "progs/w_alien.mdl";
	if (iid >= IID_WP_PIPERIFLE && iid <= IID_WP_PIPERIFLE_S_R)
		return "progs/w_pipe.mdl";
	if (iid == IID_WP_WINCHESTER)
		return "progs/w_double.mdl";
	if (iid == IID_WP_MOSSBERG)
		return "progs/w_combat.mdl";
	if (iid == IID_WP_JACKHAMMER)
		return "progs/w_jackhammer.mdl";

	if (iid == IID_WP_GREASEGUN)
		return "progs/w_mp9.mdl";
	if (iid == IID_WP_MP9)
		return "progs/w_mp5.mdl";
	if (iid == IID_WP_GREASEGUN_S)
		return "progs/w_mp9.mdl";
	if (iid == IID_WP_MP9_S)
		return "progs/w_mp7.mdl";
	if (iid == IID_WP_RANGEMASTER)
		return "progs/w_rangem.mdl";
	if (iid == IID_WP_RANGEMASTER_SCOPE)
		return "progs/w_rangem.mdl";
	if (iid == IID_WP_AK112)
		return "progs/w_ak47.mdl";
	if (iid == IID_WP_AK112_M)
		return "progs/w_ak47.mdl";
	if (iid == IID_WP_AK74)
		return "progs/w_ak47.mdl";
	if (iid == IID_WP_ACR)
		return "progs/w_bozar.mdl";
	if (iid == IID_WP_DKS1)
		return "progs/w_srifle.mdl";
	if (iid == IID_WP_DKS1_S)
		return "progs/w_srifle.mdl";
	if (iid == IID_WP_MOONLIGHT)
		return "progs/w_night.mdl";
	if (iid == IID_WP_G11)
		return "progs/w_g11.mdl";
	if (iid == IID_WP_FNFAL)
		return "progs/w_fnfal.mdl";

	if (iid == IID_WP_GAUSERIFLE)
		return "progs/w_gauss.mdl";
	if (iid == IID_WP_PULSERIFLE)
		return "progs/w_plasma.mdl";
	if (iid == IID_WP_TURBOPLASMA)
		return "progs/w_plasma.mdl";
	if (iid == IID_WP_PLASMACARBINE)
		return "progs/w_carbine.mdl";

	if (iid == IID_WP_ROCKETLAUNCHER)
		return "progs/w_rocket.mdl";
	if (iid == IID_WP_GRENADELAUNCHER)
		return "progs/w_gl.mdl";
	if (iid == IID_WP_LASERGATLING)
		return "progs/w_chain.mdl";

	if (iid == IID_GREN_STUN)
		return "progs/grenade2.mdl";
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

	if (iid >= IID_AM_NEEDLER && iid <= IID_AM_CASELESS)
		return "progs/ammobox.mdl";

//fixme
	if (iid == IID_CHEM_ADRENALINE)
		return "maps/b_bh10.bsp";
	if (iid == IID_CHEM_PSYCHO)
		return "maps/b_bh25.bsp";
	if (iid == IID_CHEM_BESERK)
		return "maps/b_bh100.bsp";
	if (iid == IID_CHEM_RADX)
		return "maps/b_bh25.bsp";

	if (iid >= IID_MISC_JUNK && iid <= IID_MISC_XRAYTUBE)
		return "progs/junk1.mdl";


	return "progs/s_light.spr";	//no model. :/
};

float(float iid) WeaponAmmoType =
{
	if (iid == IID_GREN_STUN)
		return IID_GREN_STUN;
	if (iid == IID_GREN_FRAG)
		return IID_GREN_FRAG;
	if (iid == IID_GREN_EMP)
		return IID_GREN_EMP;
	if (iid == IID_GREN_SMOKE)
		return IID_GREN_SMOKE;
	if (iid == IID_GREN_FLASH)
		return IID_GREN_FLASH;

	if (iid == IID_WP_USP)
		return IID_AM_45ACP;
	if (iid == IID_WP_USP_S)
		return IID_AM_45ACP;
	if (iid == IID_WP_GLOCK)
		return IID_AM_10MM;
	if (iid == IID_WP_GLOCK_S)
		return IID_AM_10MM;
	if (iid == IID_WP_DEAGLE)
		return IID_AM_44MAGNUM;
	if (iid == IID_WP_AK47)
		return IID_AM_WARSAW;
	if (iid == IID_WP_DEAGLE_M)
		return IID_AM_44MAGNUM;
	if (iid == IID_WP_NEEDLER)
		return IID_AM_NEEDLER;
	if (iid == IID_WP_ALIENBLASTER)
		return IID_AM_ENERGYCELL;
	if (iid >= IID_WP_PIPERIFLE && iid <= IID_WP_PIPERIFLE_S_R)
		return IID_AM_44MAGNUM;
	if (iid == IID_WP_WINCHESTER)
		return IID_AM_12GAUGESHELLS;
	if (iid == IID_WP_MOSSBERG)
		return IID_AM_12GAUGESHELLS;
	if (iid == IID_WP_JACKHAMMER)
		return IID_AM_12GAUGESHELLS;
	if (iid == IID_WP_GREASEGUN)
		return IID_AM_45ACP;
	if (iid == IID_WP_MP9)
		return IID_AM_10MM;
	if (iid == IID_WP_GREASEGUN_S)
		return IID_AM_45ACP;
	if (iid == IID_WP_MP9_S)
		return IID_AM_10MM;
	if (iid == IID_WP_RANGEMASTER)
		return IID_AM_556MM;
	if (iid == IID_WP_RANGEMASTER_SCOPE)
		return IID_AM_556MM;
	if (iid == IID_WP_AK112)
		return IID_AM_5MMHIGHVEL;
	if (iid == IID_WP_AK47)
		return IID_AM_WARSAW;
	if (iid == IID_WP_AK112_M)
		return IID_AM_556MM;
	if (iid == IID_WP_AK74)
		return IID_AM_WARSAW;
	if (iid == IID_WP_ACR)
		return IID_AM_762MM;
	if (iid == IID_WP_DKS1)
		return IID_AM_762MM;
	if (iid == IID_WP_DKS1_S)
		return IID_AM_762MM;
	if (iid == IID_WP_MOONLIGHT)
		return IID_AM_556MM;
	if (iid == IID_WP_G11)
		return IID_AM_CASELESS;
	if (iid == IID_WP_FNFAL)
		return IID_AM_762MM;

	if (iid == IID_WP_GAUSERIFLE)
		return IID_AM_2MMEC;
	if (iid == IID_WP_PULSERIFLE)
		return IID_AM_ENERGYCELL;
	if (iid == IID_WP_PULSERIFLE)
		return IID_AM_ENERGYCELL;
	if (iid == IID_WP_TURBOPLASMA)
		return IID_AM_ENERGYCELL;
	if (iid == IID_WP_PLASMACARBINE)
		return IID_AM_ENERGYCELL;
	if (iid == IID_WP_ROCKETLAUNCHER)
		return IID_AM_ROCKET;
	if (iid == IID_WP_LASERGATLING)
		return IID_AM_ENERGYCELL;
	if (iid == IID_WP_GRENADELAUNCHER)
		return IID_AM_GRENADE;

	return IID_NONE;
};

float(float iid) WeaponMagQuant =
{
	if (iid == IID_WP_USP)
		return 12;
	if (iid == IID_WP_USP_S)
		return 12;
	if (iid == IID_WP_GLOCK)
		return 10;
	if (iid == IID_WP_GLOCK_S)
		return 10;
	if (iid == IID_WP_DEAGLE)
		return 7;
	if (iid == IID_WP_DEAGLE_M)
		return 11;
	if (iid == IID_WP_NEEDLER)
		return 15;
	if (iid == IID_WP_ALIENBLASTER)
		return 6;
	if (iid >= IID_WP_PIPERIFLE && iid <= IID_WP_PIPERIFLE_S_R)
		return 1;
	if (iid == IID_WP_WINCHESTER)
		return 2;
	if (iid == IID_WP_MOSSBERG)
		return 12;
	if (iid == IID_WP_JACKHAMMER)
		return 20;
	if (iid == IID_WP_GREASEGUN)
		return 25;
	if (iid == IID_WP_MP9)
		return 30;
	if (iid == IID_WP_GREASEGUN_S)
		return 25;
	if (iid == IID_WP_MP9_S)
		return 30;
	if (iid == IID_WP_RANGEMASTER)
		return 10;
	if (iid == IID_WP_RANGEMASTER_SCOPE)
		return 10;
	if (iid == IID_WP_AK47)
		return 30;
	if (iid == IID_WP_AK112)
		return 24;
	if (iid == IID_WP_AK112_M)
		return 100;
	if (iid == IID_WP_AK74)
		return 30;
	if (iid == IID_WP_ACR)
		return 20;
	if (iid == IID_WP_DKS1)
		return 8;
	if (iid == IID_WP_DKS1_S)
		return 8;
	if (iid == IID_WP_MOONLIGHT)
		return 30;
	if (iid == IID_WP_G11)
		return 50;
	if (iid == IID_WP_GAUSERIFLE)
		return 10;
	if (iid == IID_WP_PULSERIFLE)
		return 40;
	if (iid == IID_WP_TURBOPLASMA)
		return 40;
	if (iid == IID_WP_PLASMACARBINE)
		return 50;
	if (iid == IID_WP_FNFAL)
		return 20;
	if (iid == IID_WP_ROCKETLAUNCHER)
		return 1;
	if (iid == IID_WP_LASERGATLING)
		return 120;

	return 0;
};

float(float iid) GetItemWeight =
{
	if (iid == IID_NONE)
		return 0;
	if (iid == IID_WP_KNIFE)
		return 1;
	if (iid == IID_WP_SLEDGE)
		return 8;
	if (iid == IID_WP_SPEAR)
		return 8;
	if (iid == IID_WP_POWERAXE)
		return 6;
	if (iid == IID_WP_USP)
		return 1;
	if (iid == IID_WP_USP_S)
		return 1;
	if (iid == IID_WP_GLOCK)
		return 1;
	if (iid == IID_WP_GLOCK_S)
		return 1;
	if (iid == IID_WP_DEAGLE)
		return 3;
	if (iid == IID_WP_DEAGLE_M)
		return 4;
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
	if (iid == IID_WP_GREASEGUN)
		return 3;
	if (iid == IID_WP_GREASEGUN_S)
		return 3;
	if (iid == IID_WP_MP9)
		return 3;
	if (iid == IID_WP_MP9_S)
		return 3;
	if (iid == IID_WP_RANGEMASTER)
		return 5;
	if (iid == IID_WP_RANGEMASTER_SCOPE)
		return 5;
	if (iid == IID_WP_AK112)
		return 5;
	if (iid == IID_WP_AK47)
		return 6;
	if (iid == IID_WP_AK112_M)
		return 8;
	if (iid == IID_WP_AK74)
		return 5;
	if (iid == IID_WP_ACR)
		return 10;
	if (iid == IID_WP_DKS1)
		return 9;
	if (iid == IID_WP_DKS1_S)
		return 9;
	if (iid == IID_WP_MOONLIGHT)
		return 5;
	if (iid == IID_WP_G11)
		return 6;
	if (iid == IID_WP_GAUSERIFLE)
		return 9;
	if (iid == IID_WP_PULSERIFLE)
		return 11;
	if (iid == IID_WP_TURBOPLASMA)
		return 11;
	if (iid == IID_WP_PLASMACARBINE)
		return 08;
	if (iid == IID_WP_FNFAL)
		return 8;
	if (iid == IID_WP_ROCKETLAUNCHER)
		return 10;
	if (iid == IID_WP_LASERGATLING)
		return 25;


	if (iid == IID_ARM_LEATHER)
		return 5;
	if (iid == IID_ARM_VANDAL)
		return 10;
	if (iid == IID_ARM_METAL)
		return 25;
	if (iid == IID_ARM_TESLA)
		return 10;
	if (iid == IID_ARM_COMBAT)
		return 15;
	if (iid == IID_ARM_SEVA)
		return 15;
	if (iid == IID_ARM_FORCE)
		return 10;
	if (iid == IID_ARM_LPOWER)
		return 35;

	if (iid == IID_BUILD_MRAMMO)
		return 2;
	if (iid == IID_BUILD_SHIELDGEN)
		return 4;
	if (iid == IID_BUILD_AUTODOC)
		return 3;
	if (iid == IID_BUILD_ROBOFANG)
		return 2;
	if (iid == IID_BUILD_TTURRET)
		return 3;
	if (iid == IID_BUILD_GTURRET)
		return 3;
	if (iid == IID_BUILD_RTURRET)
		return 3;


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


	if (iid == IID_WP_WRENCH)
		return "WRENCH";
	if (iid == IID_WP_KNIFE)
		return "knife";
	if (iid == IID_WP_SLEDGE)
		return "axe";
	if (iid == IID_WP_SPEAR)
		return "spear";
	if (iid == IID_WP_POWERAXE)
		return "disruptor";

	if (iid == IID_WP_USP)
		return "usp";
	if (iid == IID_WP_USP_S)
		return "silenced usp";
	if (iid == IID_WP_GLOCK)
		return "10mm pistol";
	if (iid == IID_WP_GLOCK_S)
		return "silenced 10mm pistol";
	if (iid == IID_WP_DEAGLE)
		return "desert eagle";
	if (iid == IID_WP_DEAGLE_M)
		return "modified desert eagle";
	if (iid == IID_WP_NEEDLER)
		return "needler pistol";
	if (iid == IID_WP_ALIENBLASTER)
		return "alien blaster";
	if (iid == IID_WP_PIPERIFLE)
		return "pipe rifle";
	if (iid == IID_WP_PIPERIFLE_S)
		return "silenced pipe rifle";
	if (iid == IID_WP_PIPERIFLE_R)
		return "modified pipe rifle";
	if (iid == IID_WP_PIPERIFLE_SCOPE)
		return "scoped pipe rifle";
	if (iid == IID_WP_PIPERIFLE_S_R)
		return "super piperifle mark i";
	if (iid == IID_WP_PIPERIFLE_SCOPE_R)
		return "super piperifle mark ii";
	if (iid == IID_WP_PIPERIFLE_SCOPE_S)
		return "super piperifle mark iii";
	if (iid == IID_WP_PIPERIFLE_SCOPE_S_R)
		return "super piperifle mark iv";
	if (iid == IID_WP_WINCHESTER)
		return "sawed-off shotgun";
	if (iid == IID_WP_MOSSBERG)
		return "citykiller";
	if (iid == IID_WP_JACKHAMMER)
		return "jackhammer";
	if (iid == IID_WP_GREASEGUN)
		return "grease gun";
	if (iid == IID_WP_MP9)
		return "mp9";
	if (iid == IID_WP_RANGEMASTER)
		return "rangemaster";
	if (iid == IID_WP_RANGEMASTER_SCOPE)
		return "scoped rangemaster";
	if (iid == IID_WP_AK47)
		return "ak-47";
	if (iid == IID_WP_AK112)
		return "ak-112";
	if (iid == IID_WP_AK112_M)
		return "light support weapon";
	if (iid == IID_WP_AK74)
		return "ak-47";
	if (iid == IID_WP_ACR)
		return "bozar";
	if (iid == IID_WP_DKS1)
		return "dks-1";
	if (iid == IID_WP_DKS1_S)
		return "dks-1";
	if (iid == IID_WP_MOONLIGHT)
		return "moonlight";
	if (iid == IID_WP_G11)
		return "hk g11";
	if (iid == IID_WP_GAUSERIFLE)
		return "gauss rifle";
	if (iid == IID_WP_PULSERIFLE)
		return "plasma rifle";
	if (iid == IID_WP_TURBOPLASMA)
		return "turbo plasma rifle";
	if (iid == IID_WP_PLASMACARBINE)
		return "laser pistol";
	if (iid == IID_WP_FNFAL)
		return "fn fal";
	if (iid == IID_WP_ROCKETLAUNCHER)
		return "rocket launcher";
	if (iid == IID_WP_LASERGATLING)
		return "laser minigun";

	if (iid == IID_AM_NEEDLER)
		return "needler cartidge";
	if (iid == IID_AM_2MMEC)
		return "2mm AP";
	if (iid == IID_AM_10MM)
		return "10mm JHP";
	if (iid == IID_AM_556MM)
		return "5.56mm FMJ";
	if (iid == IID_AM_5MMHIGHVEL)
		return "5mm JHP";
	if (iid == IID_AM_12GAUGESHELLS)
		return "12GA shotgun shells";
	if (iid == IID_AM_ENERGYCELL)
		return "small energy cell";
	if (iid == IID_AM_762MM)
		return "7.62mm AP";
	if (iid == IID_AM_44MAGNUM)
		return ".44 magnum FMJ";
	if (iid == IID_AM_WARSAW)
		return "7.62mmx38 Warsaw Pact";
	if (iid == IID_AM_45ACP)
		return ".45 ACP";
	if (iid == IID_AM_ROCKET)
		return "explosive rocket";
	if (iid == IID_AM_CASELESS)
		return "4.7mm caseless";


	if (iid == IID_GREN_STUN)
		return "stun bomb";
	if (iid == IID_GREN_FRAG)
		return "nuka bomb";
	if (iid == IID_GREN_EMP)
		return "emp bomb";
	if (iid == IID_GREN_SMOKE)
		return "smoke bomb";
	if (iid == IID_GREN_FLASH)
		return "flash bomb";


	if (iid == IID_ARM_DESERT)
		return "desert armor";
	if (iid == IID_ARM_LEATHER)
		return "leather armor";
	if (iid == IID_ARM_VANDAL)
		return "vandal armor";
	if (iid == IID_ARM_METAL)
		return "metal armor";
	if (iid == IID_ARM_TESLA)
		return "tesla armor";
	if (iid == IID_ARM_COMBAT)
		return "combat armor";
	if (iid == IID_ARM_SEVA)
		return "seva suit";
	if (iid == IID_ARM_FORCE)
		return "force armor";
	if (iid == IID_ARM_LPOWER)
		return "power armor";


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
	if (iid == IID_CHEM_RADX)
		return "rad-x";

	if (iid == IID_BUILD_MRAMMO)
		return "mr. ammo";
	if (iid == IID_BUILD_SHIELDGEN)
		return "shield-gen";
	if (iid == IID_BUILD_AUTODOC)
		return "auto-doc";
	if (iid == IID_BUILD_ROBOFANG)
		return "robofang";
	if (iid == IID_BUILD_TTURRET)
		return "tesla turret";
	if (iid == IID_BUILD_RTURRET)
		return "rocket turret";
	if (iid == IID_BUILD_GTURRET)
		return "machine-gun turret";


	if (iid == IID_EQUIP_MEDIC_BAG)
		return "medics bag";
	if (iid == IID_EQUIP_GOGGLES)
		return "ghost goggles";
	if (iid == IID_EQUIP_SILENCER)
		return "universal silencer";
	if (iid == IID_EQUIP_BELTPOUCH)
		return "belt pouch";
	if (iid == IID_EQUIP_TOOLKIT)
		return "super WRENCH";
	if (iid == IID_EQUIP_BACKPACK)
		return "haversack";
	if (iid == IID_EQUIP_CLIMBINGGEAR)
		return "climbing gear";
	if (iid == IID_EQUIP_BATTERY)
		return "enhanced battery";
	if (iid == IID_EQUIP_STEALTHBOY)
		return "stealthboy";
	if (iid == IID_EQUIP_SPRINTKIT)
		return "lucozade sport!";
	if (iid == IID_EQUIP_HOVERBOOTS)
		return "hover boots";
	if (iid == IID_EQUIP_EXTENDER)
		return "barrel extender";
	if (iid == IID_EQUIP_XRAY)
		return "x-ray device";

	if (iid == IID_MISC_JUNK)
		return "random junk";
	if (iid == IID_MISC_NUKACOLA)
		return "nuka-cola";
	if (iid == IID_MISC_CHEMICALS)
		return "chemicals";
	if (iid == IID_MISC_AEROSOL)
		return "aerosol can";
	if (iid == IID_MISC_RDXCRYSTAL)
		return "rdx crystals";
	if (iid == IID_MISC_STEELPIPE)
		return "steel pipe";
	if (iid == IID_MISC_DUCKTAPE)
		return "duct tape";
	if (iid == IID_MISC_LCD)
		return "liquid crystal display";
	if (iid == IID_MISC_CIRCUITBOARD)
		return "circuit board";
	if (iid == IID_MISC_COPPERWIRE)
		return "copper wire";
	if (iid == IID_MISC_HMXCOMPOUND)
		return "hmx compound";
	if (iid == IID_MISC_GUM)
		return "chewing gum";
	if (iid == IID_MISC_XRAYTUBE)
		return "x-ray tube";


	bprint(PRINT_MEDIUM, ftos(iid), " without a name!\n");
	return strcat("unknown", ftos(iid));
};

float(float iid) GetBaseValue =
{
	if (iid == IID_NONE)
		return 0;


	if (iid == IID_WP_WRENCH)
		return 5;
	if (iid == IID_WP_KNIFE)
		return 5;
	if (iid == IID_WP_SLEDGE)
		return 15;
	if (iid == IID_WP_SPEAR)
		return 25;


	if (iid == IID_WP_USP)
		return 40;
	if (iid == IID_WP_USP_S)
		return 60;
	if (iid == IID_WP_GLOCK)
		return 30;
	if (iid == IID_WP_GLOCK_S)
		return 50;
	if (iid == IID_WP_DEAGLE)
		return 70;
	if (iid == IID_WP_DEAGLE_M)
		return 120;
	if (iid == IID_WP_NEEDLER)
		return 80;
	if (iid == IID_WP_ALIENBLASTER)
		return 150;
	if (iid == IID_WP_PIPERIFLE)
		return 15;
	if (iid == IID_WP_PIPERIFLE_S)
		return 250;
	if (iid == IID_WP_PIPERIFLE_R)
		return 250;
	if (iid == IID_WP_PIPERIFLE_SCOPE)
		return 250;
	if (iid == IID_WP_PIPERIFLE_S_R)
		return 350;
	if (iid == IID_WP_PIPERIFLE_SCOPE_R)
		return 350;
	if (iid == IID_WP_PIPERIFLE_SCOPE_S)
		return 350;
	if (iid == IID_WP_PIPERIFLE_SCOPE_S_R)
		return 500;
	if (iid == IID_WP_WINCHESTER)
		return 50;
	if (iid == IID_WP_MOSSBERG)
		return 150;
	if (iid == IID_WP_JACKHAMMER)
		return 250;
	if (iid == IID_WP_GREASEGUN)
		return 100;
	if (iid == IID_WP_GREASEGUN_S)
		return 125;
	if (iid == IID_WP_MP9)
		return 110;
	if (iid == IID_WP_MP9_S)
		return 125;
	if (iid == IID_WP_RANGEMASTER)
		return 80;
	if (iid == IID_WP_RANGEMASTER_SCOPE)
		return 120;
	if (iid == IID_WP_AK47)
		return 150;
	if (iid == IID_WP_AK112)
		return 140;
	if (iid == IID_WP_AK112_M)
		return 170;
	if (iid == IID_WP_AK74)
		return 20;
	if (iid == IID_WP_ACR)
		return 190;
	if (iid == IID_WP_DKS1)
		return 110;
	if (iid == IID_WP_DKS1_S)
		return 150;
	if (iid == IID_WP_MOONLIGHT)
		return 225;
	if (iid == IID_WP_G11)
		return 240;
	if (iid == IID_WP_GAUSERIFLE)
		return 300;
	if (iid == IID_WP_PULSERIFLE)
		return 280;
	if (iid == IID_WP_TURBOPLASMA)
		return 380;
	if (iid == IID_WP_PLASMACARBINE)
		return 400;
	if (iid == IID_WP_FNFAL)
		return 190;
	if (iid == IID_WP_ROCKETLAUNCHER)
		return 200;
	if (iid == IID_WP_LASERGATLING)
		return 450;



	if (iid == IID_AM_NEEDLER)
		return 35;
	if (iid == IID_AM_2MMEC)
		return 120;
	if (iid == IID_AM_10MM)
		return 12;
	if (iid == IID_AM_556MM)
		return 20;
	if (iid == IID_AM_5MMHIGHVEL)
		return 16;
	if (iid == IID_AM_12GAUGESHELLS)
		return 16;
	if (iid == IID_AM_ENERGYCELL)
		return 48;
	if (iid == IID_AM_762MM)
		return 32;
	if (iid == IID_AM_44MAGNUM)
		return 24;
	if (iid == IID_AM_WARSAW)
		return 20;
	if (iid == IID_AM_45ACP)
		return 16;
	if (iid == IID_AM_ROCKET)
		return 30;
	if (iid == IID_AM_CASELESS)
		return 40;


	if (iid == IID_GREN_STUN)
		return 5;
	if (iid == IID_GREN_FRAG)
		return 10;
	if (iid == IID_GREN_EMP)
		return 15;
	if (iid == IID_GREN_SMOKE)
		return 5;
	if (iid == IID_GREN_FLASH)
		return 5;


	if (iid == IID_ARM_DESERT)
		return 45;
	if (iid == IID_ARM_LEATHER)
		return 50;
	if (iid == IID_ARM_VANDAL)
		return 90;
	if (iid == IID_ARM_METAL)
		return 100;
	if (iid == IID_ARM_TESLA)
		return 125;
	if (iid == IID_ARM_COMBAT)
		return 250;
	if (iid == IID_ARM_SEVA)
		return 275;
	if (iid == IID_ARM_FORCE)
		return 300;
	if (iid == IID_ARM_LPOWER)
		return 450;


	if (iid == IID_CHEM_STIMPACK)
		return 5;
	if (iid == IID_CHEM_MEDICALBAG)
		return 25;
	if (iid == IID_CHEM_SUPERSTIM)
		return 15;
	if (iid == IID_CHEM_ADRENALINE)
		return 9;
	if (iid == IID_CHEM_PSYCHO)
		return 12;
	if (iid == IID_CHEM_BESERK)
		return 14;
	if (iid == IID_CHEM_RADX)
		return 15;

	if (iid == IID_EQUIP_MEDIC_BAG)
		return 35;
	if (iid == IID_EQUIP_GOGGLES)
		return 45;
	if (iid == IID_EQUIP_SILENCER)
		return 20;
	if (iid == IID_EQUIP_BELTPOUCH)
		return 70;
	if (iid == IID_EQUIP_TOOLKIT)
		return 50;
	if (iid == IID_EQUIP_BACKPACK)
		return 10;
	if (iid == IID_EQUIP_CLIMBINGGEAR)
		return 150;
	if (iid == IID_EQUIP_BATTERY)
		return 100;
	if (iid == IID_EQUIP_STEALTHBOY)
		return 50;
	if (iid == IID_EQUIP_SPRINTKIT)
		return 1000;
	if (iid == IID_EQUIP_HOVERBOOTS)
		return 1000;
	if (iid == IID_EQUIP_EXTENDER)
		return 10;
	if (iid == IID_EQUIP_XRAY)
		return 10;

	if (iid == IID_MISC_JUNK)
		return 1;
	if (iid == IID_MISC_NUKACOLA)
		return 1;
	if (iid == IID_MISC_CHEMICALS)
		return 2;
	if (iid == IID_MISC_AEROSOL)
		return 2;
	if (iid == IID_MISC_RDXCRYSTAL)
		return 3;
	if (iid == IID_MISC_STEELPIPE)
		return 1;
	if (iid == IID_MISC_DUCKTAPE)
		return 1;
	if (iid == IID_MISC_LCD)
		return 2;
	if (iid == IID_MISC_CIRCUITBOARD)
		return 2;
	if (iid == IID_MISC_COPPERWIRE)
		return 1;
	if (iid == IID_MISC_HMXCOMPOUND)
		return 1;
	if (iid == IID_MISC_GUM)
		return 2;
	if (iid == IID_MISC_XRAYTUBE)
		return 5;


	bprint(PRINT_MEDIUM, ftos(iid), " without a sell price!\n");
	return 0;
};

string(float iid) GetItemDesc =
{
	if (iid == IID_NONE)
		return "";


	if (iid == IID_WP_WRENCH)
		return "a heavy multi-function wrench";
	if (iid == IID_WP_KNIFE)
		return "great for stealth attacks";
	if (iid == IID_WP_SLEDGE)
		return "a futuristic battle axe";
	if (iid == IID_WP_SPEAR)
		return "a long, sharp polearm";
	if (iid == IID_WP_POWERAXE)
		return "a futuristic axe of some sort";

	if (iid == IID_WP_USP)
		return "uses 45 acp, a solid round";
	if (iid == IID_WP_USP_S)
		return "this usp has a silencer attached for maximum stealth";
	if (iid == IID_WP_GLOCK)
		return "a rugged, reliable sidearm";
	if (iid == IID_WP_GLOCK_S)
		return "silencer allows for hit-and-run tactics";
	if (iid == IID_WP_DEAGLE)
		return "the favorite of late 20th century action heroes";
	if (iid == IID_WP_DEAGLE_M)
		return "a modified desert eagle with extended magazines";
	if (iid == IID_WP_NEEDLER)
		return "an experimental weapon of some sort";
	if (iid == IID_WP_ALIENBLASTER)
		return "this gun is of an unknown origin";
	if (iid == IID_WP_PIPERIFLE)
		return "a homemade rifle made out of a steel pipe";
	if (iid == IID_WP_PIPERIFLE_S)
		return "this pipe rifle has a silencer attached";
	if (iid == IID_WP_PIPERIFLE_SCOPE)
		return "a crude scope has been attached to this pipe rifle";
	if (iid == IID_WP_PIPERIFLE_R)
		return "a modified pipe rifle with a box magazine";
	if (iid == IID_WP_PIPERIFLE_S_R)
		return "pipe rifle with a silencer and box magazine";
	if (iid == IID_WP_PIPERIFLE_SCOPE_R)
		return "pipe rifle with a scope and box magazine";
	if (iid == IID_WP_PIPERIFLE_SCOPE_S)
		return "pipe rifle with a scope and silencer";
	if (iid == IID_WP_PIPERIFLE_SCOPE_S_R)
		return "the ultimate makeshift rifle!";
	if (iid == IID_WP_WINCHESTER)
		return "double-barrelled and sawed off";
	if (iid == IID_WP_MOSSBERG)
		return "semi-auto combat shotgun, magazine fed";
	if (iid == IID_WP_JACKHAMMER)
		return "the ultimate for close range battles";
	if (iid == IID_WP_GREASEGUN)
		return "higher recoil due to larger .45 acp round";
	if (iid == IID_WP_GREASEGUN_S)
		return "higher recoil due to larger .45 acp round";
	if (iid == IID_WP_MP9)
		return "great all-around weapon; good burst fire";
	if (iid == IID_WP_MP9_S)
		return "silenced 10mm smg; less recoil";
	if (iid == IID_WP_RANGEMASTER)
		return "civilian battle rifle. sturdy and effective";
	if (iid == IID_WP_RANGEMASTER_SCOPE)
		return "civilian battle rifle. this one has a scope";
	if (iid == IID_WP_AK47)
		return "an outdated assault rifle with extreme durability";
	if (iid == IID_WP_AK112)
		return "a common assault rifle with extreme durability";
	if (iid == IID_WP_AK112_M)
		return "a great weapon for providing suppressing fire";
	if (iid == IID_WP_DKS1)
		return "the larger caliber 7.62mm provides extra power";
	if (iid == IID_WP_DKS1_S)
		return ".308 sniper rifle, silenced for stealth-ops";
	if (iid == IID_WP_DKS1)
		return "semi-auto sniper rifle";
	if (iid == IID_WP_MOONLIGHT)
		return "silenced m16a2 with night-vision scope";
	if (iid == IID_WP_G11)
		return "a prototype rifle that uses caseless ammo";
	if (iid == IID_WP_GAUSERIFLE)
		return "fires extremely high-velocity rounds";
	if (iid == IID_WP_PULSERIFLE)
		return "high-tech plasma weapon released just before the war";
	if (iid == IID_WP_TURBOPLASMA)
		return "modified plasma rifle";
	if (iid == IID_WP_PLASMACARBINE)
		return "prototype plasma rifle";
	if (iid == IID_WP_FNFAL)
		return "rifle designed for longer range; high recoil";
	if (iid == IID_WP_ROCKETLAUNCHER)
		return "great for devastating tactical strikes";
	if (iid == IID_WP_LASERGATLING)
		return "quite simply, the ultimate in heavy firepower";
	if (iid == IID_WP_ACR)
		return "refinement of the sniper's art";


	if (iid == IID_AM_NEEDLER)
		return "good armor-peircing properties";
	if (iid == IID_AM_2MMEC)
		return "virtually ignores armor";
	if (iid == IID_AM_10MM)
		return "jacketed hollow-points for good damage";
	if (iid == IID_AM_556MM)
		return "standard rifle round, moderate recoil";
	if (iid == IID_AM_5MMHIGHVEL)
		return "experimental high-velocity rifle ammo";
	if (iid == IID_AM_12GAUGESHELLS)
		return "the old standard, has trouble with heavy armor";
	if (iid == IID_AM_ENERGYCELL)
		return "this energy cell comes from an unknown origin";
	if (iid == IID_AM_762MM)
		return "good stopping power and armor-piercing performance";
	if (iid == IID_AM_44MAGNUM)
		return "full metal jacket, hit 'em hard";
	if (iid == IID_AM_WARSAW)
		return "ancient soviet rifle round, used in the ak47";
	if (iid == IID_AM_45ACP)
		return "a common pistol round with good stopping power";
	if (iid == IID_AM_ROCKET)
		return "expensive and extremely lethal";
	if (iid == IID_AM_CASELESS)
		return "a brick of small-caliber caseless ammo";


	if (iid == IID_GREN_FRAG)
		return "all-purpose fragmentation grenade";
	if (iid == IID_GREN_EMP)
		return "useful against robots and cyborgs";
	if (iid == IID_GREN_SMOKE)
		return "very useful for a quick getaway!";
	if (iid == IID_GREN_FLASH)
		return "temporarily blinds nearby enemies";
	if (iid == IID_GREN_STUN)
		return "delivers a forceful shock to nearby enemies";

	if (iid == IID_ARM_DESERT)
		return "the light armor of a raider; makes a great disguise";
	if (iid == IID_ARM_LEATHER)
		return "lightweight leather armor. provides great mobility.";
	if (iid == IID_ARM_VANDAL)
		return "enhanced leather armor for extra protection.";
	if (iid == IID_ARM_METAL)
		return "gives great protection at a sacrifice to speed.";
	if (iid == IID_ARM_TESLA)
		return "experimental armor. great against small rounds.";
	if (iid == IID_ARM_COMBAT)
		return "this all-purpose armor is designed for combat missions.";
	if (iid == IID_ARM_SEVA)
		return "high-tech suit provides more environmental defense";
	if (iid == IID_ARM_FORCE)
		return "experimental energy-field armor, very low absorb";
	if (iid == IID_ARM_LPOWER)
		return "the heaviest armor with great protection";


	if (iid == IID_CHEM_STIMPACK)
		return "advanced healing chem that replaces lost blood";
	if (iid == IID_CHEM_MEDICALBAG)
		return "first aid kit with many high-tech supplies";
	if (iid == IID_CHEM_SUPERSTIM)
		return "contains nanobots that heal very fast";
	if (iid == IID_CHEM_ADRENALINE)
		return "boosts the metabolism, increasing jump and run speed";
	if (iid == IID_CHEM_PSYCHO)
		return "unknown chemicals, probably of military origin";
	if (iid == IID_CHEM_BESERK)
		return "unknown chemicals, probably of military origin";
	if (iid == IID_CHEM_RADX)
		return "powerful anti-radiation pills";


	if (iid == IID_BUILD_MRAMMO)
		return "provides extra ammunition";
	if (iid == IID_BUILD_SHIELDGEN)
		return "protects all nearby allies";
	if (iid == IID_BUILD_AUTODOC)
		return "the auto-doc will repair injuries";
	if (iid == IID_BUILD_ROBOFANG)
		return "robofang guardian unit";
	if (iid == IID_BUILD_TTURRET)
		return "dispatches hostile intruders";
	if (iid == IID_BUILD_RTURRET)
		return "rocket turret";
	if (iid == IID_BUILD_GTURRET)
		return "machine-gun turret";


	if (iid == IID_EQUIP_MEDIC_BAG)
		return "allows more chems to be carried";
	if (iid == IID_EQUIP_GOGGLES)
		return "protects against visual impairments";
	if (iid == IID_EQUIP_SILENCER)
		return "attaches to any smallarm";
	if (iid == IID_EQUIP_BELTPOUCH)
		return "useful for carrying extra grenades";
	if (iid == IID_EQUIP_TOOLKIT)
		return "for picking all sorts of locks";
	if (iid == IID_EQUIP_BACKPACK)
		return "this handy bag lets you carry more ammunition";
	if (iid == IID_EQUIP_CLIMBINGGEAR)
		return "for those hard to reach places";
	if (iid == IID_EQUIP_BATTERY)
		return "provides prolonged battery life";
	if (iid == IID_EQUIP_STEALTHBOY)
		return "makes user blurry and hard to see";
	if (iid == IID_EQUIP_EXTENDER)
		return "gives a weapon slightly longer range and damage";
	if (iid == IID_EQUIP_XRAY)
		return "allows for enemy detection through walls";

	if (iid == IID_MISC_NUKACOLA)
		return "a bottle of tasty nuka-cola";
	if (iid == IID_MISC_JUNK)
		return "metallic bits, pipe and other debris";
	if (iid == IID_MISC_CHEMICALS)
		return "standard household cleaner";
	if (iid == IID_MISC_AEROSOL)
		return "a half-full aerosol can";
	if (iid == IID_MISC_RDXCRYSTAL)
		return "a batch of highly unstable explosive crystals";
	if (iid == IID_MISC_STEELPIPE)
		return "a slightly rusted steel pipe";
	if (iid == IID_MISC_DUCKTAPE)
		return "ancient, but still highly functional";
	if (iid == IID_MISC_LCD)
		return "an lcd from a Lameboy DS videogame unit";
	if (iid == IID_MISC_CIRCUITBOARD)
		return "a corroded circuit board from a remote control";
	if (iid == IID_MISC_COPPERWIRE)
		return "a small spool of thin copper wire";
	if (iid == IID_MISC_HMXCOMPOUND)
		return "a small amount of high-grade military explosive";
	if (iid == IID_MISC_GUM)
		return "trident chewing gum, limited edition watermelon flavor";
	if (iid == IID_MISC_XRAYTUBE)
		return "??? a strange device ???";

	bprint(PRINT_MEDIUM, ftos(iid), " without a desc!\n");
	return strcat("", ftos(iid));
};

float(float iid) GetArmorThreshold =
{

	if (iid == IID_ARM_DESERT)
		return 2;
	if (iid == IID_ARM_LEATHER)
		return 3;
	if (iid == IID_ARM_VANDAL)
		return 4;
	if (iid == IID_ARM_METAL)
		return 1;
	if (iid == IID_ARM_TESLA)
		return 5;
	if (iid == IID_ARM_COMBAT)
		return 7;
	if (iid == IID_ARM_SEVA)
		return 6;
	if (iid == IID_ARM_FORCE)
		return 10;
	if (iid == IID_ARM_LPOWER)
		return 8;
};


float(float iid) GetArmorAbsorb =
{

	if (iid == IID_ARM_DESERT)
		return 10;
	if (iid == IID_ARM_LEATHER)
		return 20;
	if (iid == IID_ARM_VANDAL)
		return 30;
	if (iid == IID_ARM_METAL)
		return 50;
	if (iid == IID_ARM_TESLA)
		return 40;
	if (iid == IID_ARM_COMBAT)
		return 40;
	if (iid == IID_ARM_SEVA)
		return 40;
	if (iid == IID_ARM_FORCE)
		return 10;
	if (iid == IID_ARM_LPOWER)
		return 50;
};


float(float iid) GetWeaponRange =
{
	if (iid == IID_NONE)
		return 0;


	if (iid == IID_WP_KNIFE)
		return 5;


	if (iid == IID_WP_USP)
		return 20;
	if (iid == IID_WP_USP_S)
		return 15;
	if (iid == IID_WP_GLOCK)
		return 15;
	if (iid == IID_WP_GLOCK_S)
		return 10;
	if (iid == IID_WP_DEAGLE)
		return 25;
	if (iid == IID_WP_DEAGLE_M)
		return 25;
	if (iid == IID_WP_NEEDLER)
		return 20;
	if (iid == IID_WP_ALIENBLASTER)
		return 6+(getstati(103)*1);
	if (iid == IID_WP_PIPERIFLE)
		return 40;
	if (iid == IID_WP_PIPERIFLE_S)
		return 40;
	if (iid == IID_WP_PIPERIFLE_SCOPE)
		return 40;
	if (iid == IID_WP_PIPERIFLE_R)
		return 40;
	if (iid == IID_WP_PIPERIFLE_S_R)
		return 40;
	if (iid == IID_WP_PIPERIFLE_SCOPE_R)
		return 40;
	if (iid == IID_WP_PIPERIFLE_SCOPE_S)
		return 40;
	if (iid == IID_WP_PIPERIFLE_SCOPE_S_R)
		return 40;
	if (iid == IID_WP_WINCHESTER)
		return 10;
	if (iid == IID_WP_MOSSBERG)
		return 10;
	if (iid == IID_WP_JACKHAMMER)
		return 10;
	if (iid == IID_WP_GREASEGUN)
		return 20;
	if (iid == IID_WP_GREASEGUN_S)
		return 20;
	if (iid == IID_WP_MP9)
		return 15;
	if (iid == IID_WP_MP9_S)
		return 15;
	if (iid == IID_WP_RANGEMASTER)
		return 40;
	if (iid == IID_WP_RANGEMASTER_SCOPE)
		return 50;
	if (iid == IID_WP_AK112)
		return 40;
	if (iid == IID_WP_AK112_M)
		return 40;
	if (iid == IID_WP_DKS1)
		return 60;
	if (iid == IID_WP_DKS1_S)
		return 60;
	if (iid == IID_WP_MOONLIGHT)
		return 50;
	if (iid == IID_WP_G11)
		return 60;
	if (iid == IID_WP_GAUSERIFLE)
		return 70;
	if (iid == IID_WP_PULSERIFLE)
		return 50;
	if (iid == IID_WP_TURBOPLASMA)
		return 40;
	if (iid == IID_WP_PLASMACARBINE)
		return 40;
	if (iid == IID_WP_FNFAL)
		return 60;
	if (iid == IID_WP_ROCKETLAUNCHER)
		return 15;
	if (iid == IID_WP_LASERGATLING)
		return 35;
	if (iid == IID_WP_ACR)
		return 90;

	return 0;
};

float(float iid) GetWeaponDamage =
{
	if (iid == IID_NONE)
		return 0;


	if (iid == IID_WP_KNIFE)
		return 10;
	if (iid == IID_WP_USP)
		return 14;
	if (iid == IID_WP_USP_S)
		return 12;
	if (iid == IID_WP_GLOCK)
		return 12;
	if (iid == IID_WP_GLOCK_S)
		return 10;
	if (iid == IID_WP_DEAGLE)
		return 18;
	if (iid == IID_WP_DEAGLE_M)
		return 18;
	if (iid == IID_WP_NEEDLER)
		return 16;
	if (iid == IID_WP_ALIENBLASTER)
		return 20+(getstati(103)*6);
	if (iid == IID_WP_PIPERIFLE)
		return 20;
	if (iid == IID_WP_PIPERIFLE_S)
		return 20;
	if (iid == IID_WP_PIPERIFLE_SCOPE)
		return 20;
	if (iid == IID_WP_PIPERIFLE_R)
		return 20;
	if (iid == IID_WP_PIPERIFLE_S_R)
		return 20;
	if (iid == IID_WP_PIPERIFLE_SCOPE_R)
		return 20;
	if (iid == IID_WP_PIPERIFLE_SCOPE_S)
		return 20;
	if (iid == IID_WP_PIPERIFLE_SCOPE_S_R)
		return 20;
	if (iid == IID_WP_WINCHESTER)
		return 35;
	if (iid == IID_WP_MOSSBERG)
		return 35;
	if (iid == IID_WP_JACKHAMMER)
		return 35;
	if (iid == IID_WP_GREASEGUN)
		return 14;
	if (iid == IID_WP_GREASEGUN_S)
		return 12;
	if (iid == IID_WP_MP9)
		return 12;
	if (iid == IID_WP_MP9_S)
		return 10;
	if (iid == IID_WP_RANGEMASTER)
		return 18;
	if (iid == IID_WP_RANGEMASTER_SCOPE)
		return 18;
	if (iid == IID_WP_AK112)
		return 16;
	if (iid == IID_WP_AK112_M)
		return 16;
	if (iid == IID_WP_DKS1)
		return 30;
	if (iid == IID_WP_DKS1_S)
		return 30;
	if (iid == IID_WP_MOONLIGHT)
		return 16;
	if (iid == IID_WP_G11)
		return 14;
	if (iid == IID_WP_GAUSERIFLE)
		return 22;
	if (iid == IID_WP_PULSERIFLE)
		return 26;
	if (iid == IID_WP_TURBOPLASMA)
		return 24;
	if (iid == IID_WP_PLASMACARBINE)
		return 20;
	if (iid == IID_WP_FNFAL)
		return 20;
	if (iid == IID_WP_ROCKETLAUNCHER)
		return 75;
	if (iid == IID_WP_LASERGATLING)
		return 24;
	if (iid == IID_WP_ACR)
		return 40;

	return 0;
};

string(float iid) GetItemImage =
{
	if (iid == IID_NONE)
		return "blank.jpg";


	if (iid == IID_WP_WRENCH)
		return "wrench.jpg";
	if (iid == IID_WP_KNIFE)
		return "knife.jpg";
	if (iid == IID_WP_SLEDGE)
		return "disruptor.jpg";
	if (iid == IID_WP_SPEAR)
		return "spear.jpg";
	if (iid == IID_WP_POWERAXE)
		return "disruptor.jpg";

	if (iid == IID_WP_USP)
		return "usp.jpg";
	if (iid == IID_WP_USP_S)
		return "usp.jpg";
	if (iid == IID_WP_GLOCK)
		return "10mmpistol.jpg";
	if (iid == IID_WP_GLOCK_S)
		return "10mmpistol.jpg";
	if (iid == IID_WP_DEAGLE)
		return "deagle.jpg";
	if (iid == IID_WP_DEAGLE_M)
		return "deagle.jpg";
	if (iid == IID_WP_NEEDLER)
		return "needler.jpg";
	if (iid == IID_WP_ALIENBLASTER)
		return "blaster.jpg";
	if (iid >= IID_WP_PIPERIFLE && iid <= IID_WP_PIPERIFLE_S_R)
		return "prifle.jpg";
	if (iid == IID_WP_WINCHESTER)
		return "sawedoff.jpg";
	if (iid == IID_WP_MOSSBERG)
		return "citykiller.jpg";
	if (iid == IID_WP_JACKHAMMER)
		return "jackhammer.jpg";
	if (iid == IID_WP_GREASEGUN)
		return "greasegun.jpg";
	if (iid == IID_WP_MP9)
		return "mp9.jpg";
	if (iid == IID_WP_RANGEMASTER)
		return "rangem.png";
	if (iid == IID_WP_RANGEMASTER_SCOPE)
		return "rangem_s.jpg";
	if (iid == IID_WP_AK47)
		return "ak47.jpg";
	if (iid == IID_WP_AK112)
		return "ak112.png";
	if (iid == IID_WP_AK112_M)
		return "lsw.png";
	if (iid == IID_WP_AK74)
		return "ak47.jpg";
	if (iid == IID_WP_ACR)
		return "bozar.png";
	if (iid == IID_WP_DKS1)
		return "dks1.png";
	if (iid == IID_WP_DKS1_S)
		return "dks1_s.jpg";
	if (iid == IID_WP_MOONLIGHT)
		return "moonlight.jpg";
	if (iid == IID_WP_G11)
		return "g11.jpg";
	if (iid == IID_WP_GAUSERIFLE)
		return "gauss.jpg";
	if (iid == IID_WP_PULSERIFLE)
		return "plasma.jpg";
	if (iid == IID_WP_TURBOPLASMA)
		return "plasma.jpg";
	if (iid == IID_WP_PULSERIFLE)
		return "carbine.jpg";
	if (iid == IID_WP_FNFAL)
		return "fnfal.jpg";
	if (iid == IID_WP_ROCKETLAUNCHER)
		return "rocket.jpg";
	if (iid == IID_WP_LASERGATLING)
		return "gatling.jpg";


	if (iid == IID_AM_NEEDLER)
		return "needles.jpg";
	if (iid == IID_AM_2MMEC)
		return "2mmec.jpg";
	if (iid == IID_AM_10MM)
		return "10mm.jpg";
	if (iid == IID_AM_556MM)
		return "556mm.png";
	if (iid == IID_AM_5MMHIGHVEL)
		return "5mmhv.jpg";
	if (iid == IID_AM_12GAUGESHELLS)
		return "12gauge.jpg";
	if (iid == IID_AM_ENERGYCELL)
		return "cell.jpg";
	if (iid == IID_AM_762MM)
		return "762mm.png";
	if (iid == IID_AM_44MAGNUM)
		return "44mag.jpg";
	if (iid == IID_AM_WARSAW)
		return "762mm.jpg";
	if (iid == IID_AM_45ACP)
		return "45acp.jpg";
	if (iid == IID_AM_ROCKET)
		return "rocket.jpg";
	if (iid == IID_AM_CASELESS)
		return "caseless.jpg";

	if (iid == IID_GREN_FRAG)
		return "frag.jpg";
	if (iid == IID_GREN_EMP)
		return "emp.jpg";
	if (iid == IID_GREN_SMOKE)
		return "smoke.jpg";
	if (iid == IID_GREN_FLASH)
		return "flash.jpg";
	if (iid == IID_GREN_STUN)
		return "stun.jpg";

	if (iid == IID_ARM_DESERT)
		return "desert.jpg";
	if (iid == IID_ARM_LEATHER)
		return "leather.jpg";
	if (iid == IID_ARM_VANDAL)
		return "vandal.jpg";
	if (iid == IID_ARM_METAL)
		return "metal.jpg";
	if (iid == IID_ARM_TESLA)
		return "tesla.jpg";
	if (iid == IID_ARM_COMBAT)
		return "combat.jpg";
	if (iid == IID_ARM_SEVA)
		return "seva.jpg";
	if (iid == IID_ARM_FORCE)
		return "force.jpg";
	if (iid == IID_ARM_LPOWER)
		return "power.jpg";


	if (iid == IID_CHEM_STIMPACK)
		return "stimpack.jpg";
	if (iid == IID_CHEM_MEDICALBAG)
		return "medkit.jpg";
	if (iid == IID_CHEM_SUPERSTIM)
		return "superstim.jpg";
	if (iid == IID_CHEM_ADRENALINE)
		return "adrenaline.jpg";
	if (iid == IID_CHEM_PSYCHO)
		return "psycho.jpg";
	if (iid == IID_CHEM_RADX)
		return "radx.jpg";
	if (iid == IID_CHEM_BESERK)
		return "beserk.jpg";


	if (iid == IID_BUILD_MRAMMO)
		return "mrammo.jpg";
	if (iid == IID_BUILD_SHIELDGEN)
		return "shieldgen.jpg";
	if (iid == IID_BUILD_AUTODOC)
		return "autodoc.jpg";
	if (iid == IID_BUILD_ROBOFANG)
		return "robofang.jpg";
	if (iid == IID_BUILD_TTURRET)
		return "tesla.jpg";
	if (iid == IID_BUILD_RTURRET)
		return "rturret.jpg";
	if (iid == IID_BUILD_GTURRET)
		return "gturret.jpg";

	if (iid == IID_EQUIP_MEDIC_BAG)
		return "equip/medbag.jpg";
	if (iid == IID_EQUIP_GOGGLES)
		return "equip/goggles.jpg";
	if (iid == IID_EQUIP_SILENCER)
		return "equip/silencer.jpg";
	if (iid == IID_EQUIP_BELTPOUCH)
		return "equip/beltpouch.jpg";
	if (iid == IID_EQUIP_TOOLKIT)
		return "equip/WRENCH2.jpg";
	if (iid == IID_EQUIP_BACKPACK)
		return "equip/haversack.jpg";
	if (iid == IID_EQUIP_CLIMBINGGEAR)
		return "equip/cgear.jpg";
	if (iid == IID_EQUIP_BATTERY)
		return "equip/battery.jpg";
	if (iid == IID_EQUIP_STEALTHBOY)
		return "equip/stealthboy.jpg";
	if (iid == IID_EQUIP_EXTENDER)
		return "equip/silencer.jpg";
	if (iid == IID_EQUIP_XRAY)
		return "equip/xray.jpg";


	if (iid == IID_MISC_NUKACOLA)
		return "junk/nukacola.jpg";
	if (iid == IID_MISC_JUNK)
		return "junk/junk.jpg";
	if (iid == IID_MISC_CHEMICALS)
		return "junk/chemicals.jpg";
	if (iid == IID_MISC_AEROSOL)
		return "junk/aerosol.jpg";
	if (iid == IID_MISC_RDXCRYSTAL)
		return "junk/rdxcrystal.jpg";
	if (iid == IID_MISC_STEELPIPE)
		return "junk/pipe.jpg";
	if (iid == IID_MISC_DUCKTAPE)
		return "junk/tape.jpg";
	if (iid == IID_MISC_LCD)
		return "junk/lcd.jpg";
	if (iid == IID_MISC_CIRCUITBOARD)
		return "junk/board.jpg";
	if (iid == IID_MISC_COPPERWIRE)
		return "junk/wire.jpg";
	if (iid == IID_MISC_HMXCOMPOUND)
		return "junk/hmx.jpg";
	if (iid == IID_MISC_GUM)
		return "junk/gum.jpg";
	if (iid == IID_MISC_XRAYTUBE)
		return "junk/tube.jpg";

	bprint(PRINT_MEDIUM, ftos(iid), " without a name!\n");
	return strcat("unknown.jpg", ftos(iid));
};


float(string itname) ItemIDOfName = 
{
	if (itname == "nothing")
		return IID_NONE;
	if (itname == "WRENCH")
		return IID_WP_WRENCH;
	if (itname == "knife")
		return IID_WP_KNIFE;
	if (itname == "hand-axe")
		return IID_WP_SLEDGE;
	if (itname == "spear")
		return IID_WP_SPEAR;
	if (itname == "poweraxe")
		return IID_WP_POWERAXE;
	if (itname == "usp")
		return IID_WP_USP;
	if (itname == "glock")
		return IID_WP_GLOCK;
	if (itname == "desert eagle")
		return IID_WP_DEAGLE;
	if (itname == "deagle")
		return IID_WP_DEAGLE;
	if (itname == "needler")
		return IID_WP_NEEDLER;
	if (itname == "alien blaster")
		return IID_WP_ALIENBLASTER;
	if (itname == "blaster")
		return IID_WP_ALIENBLASTER;
	if (itname == "pipe rifle")
		return IID_WP_PIPERIFLE;
	if (itname == "winchester")
		return IID_WP_WINCHESTER;
	if (itname == "mossberg")
		return IID_WP_MOSSBERG;
	if (itname == "jackhammer")
		return IID_WP_JACKHAMMER;
	if (itname == "grease gun")
		return IID_WP_GREASEGUN;
	if (itname == "mp9")
		return IID_WP_MP9;
	if (itname == "rangemaster")
		return IID_WP_RANGEMASTER;
	if (itname == "rangemaster (scope)")
		return IID_WP_RANGEMASTER_SCOPE;
	if (itname == "fnfal")
		return IID_WP_FNFAL;
	if (itname == "ak-47")
		return IID_WP_AK112;
	if (itname == "ak-112")
		return IID_WP_AK112;
	if (itname == "ak-112 m")
		return IID_WP_AK112_M;
	if (itname == "ak-74")
		return IID_WP_AK74;
	if (itname == "bozar")
		return IID_WP_ACR;
	if (itname == "dks-1")
		return IID_WP_DKS1;
	if (itname == "dks-1 silenced")
		return IID_WP_DKS1_S;
	if (itname == "moonlight")
		return IID_WP_MOONLIGHT;
	if (itname == "g11")
		return IID_WP_G11;
	if (itname == "gauss rifle")
		return IID_WP_GAUSERIFLE;
	if (itname == "phased plasma rifle")
		return IID_WP_PULSERIFLE;
	if (itname == "plasma carbine")
		return IID_WP_PLASMACARBINE;
	if (itname == "turbo plasma rifle")
		return IID_WP_TURBOPLASMA;
	if (itname == "rocket")
		return IID_WP_ROCKETLAUNCHER;
	if (itname == "laser minigun")
		return IID_WP_LASERGATLING;
	if (itname == "frag grenade")
		return IID_GREN_FRAG;
	if (itname == "stun grenade")
		return IID_GREN_STUN;
	if (itname == "emp grenade")
		return IID_GREN_EMP;
	if (itname == "smoke grenade")
		return IID_GREN_SMOKE;
	if (itname == "flash grenade")
		return IID_GREN_FLASH;
	if (itname == "leather armor")
		return IID_ARM_LEATHER;
	if (itname == "vandal armor")
		return IID_ARM_VANDAL;
	if (itname == "metal armor")
		return IID_ARM_METAL;
	if (itname == "tesla armor")
		return IID_ARM_TESLA;
	if (itname == "combat armor")
		return IID_ARM_COMBAT;
	if (itname == "seva suit")
		return IID_ARM_SEVA;
	if (itname == "force armor")
		return IID_ARM_FORCE;
	if (itname == "light power armor")
		return IID_ARM_LPOWER;
	if (itname == "stimpacks")
		return IID_CHEM_STIMPACK;
	if (itname == "stimpack")
		return IID_CHEM_STIMPACK;
	if (itname == "medical bag")
		return IID_CHEM_MEDICALBAG;
	if (itname == "bandages")
		return IID_CHEM_MEDICALBAG;
	if (itname == "superstims")
		return IID_CHEM_SUPERSTIM;
	if (itname == "adrenaline")
		return IID_CHEM_ADRENALINE;
	if (itname == "psycho")
		return IID_CHEM_PSYCHO;
	if (itname == "rad-x")
		return IID_CHEM_RADX;
	if (itname == "beserk")
		return IID_CHEM_BESERK;
	if (itname == "mr.ammo")
		return IID_BUILD_MRAMMO;
	if (itname == "mrammo")
		return IID_BUILD_MRAMMO;
	if (itname == "shield-gen")
		return IID_BUILD_SHIELDGEN;
	if (itname == "shieldgen")
		return IID_BUILD_SHIELDGEN;
	if (itname == "auto-doc")
		return IID_BUILD_AUTODOC;
	if (itname == "autodoc")
		return IID_BUILD_AUTODOC;
	if (itname == "robofang")
		return IID_BUILD_ROBOFANG;
	if (itname == "doggie")
		return IID_BUILD_ROBOFANG;
	if (itname == "climbinggear")
		return IID_EQUIP_CLIMBINGGEAR;
	if (itname == "gear")
		return IID_EQUIP_CLIMBINGGEAR;
	if (itname == "stealthboy")
		return IID_EQUIP_STEALTHBOY;
	if (itname == "xray")
		return IID_EQUIP_XRAY;
	if (itname == "extender")
		return IID_EQUIP_EXTENDER;

	if (itname == "nukacola")
		return IID_MISC_NUKACOLA;
	if (itname == "junk")
		return IID_MISC_JUNK;
	if (itname == "chemicals")
		return IID_MISC_CHEMICALS;
	if (itname == "aerosol")
		return IID_MISC_AEROSOL;
	if (itname == "rdx")
		return IID_MISC_RDXCRYSTAL;
	if (itname == "steelpipe")
		return IID_MISC_STEELPIPE;
	if (itname == "duct-tape")
		return IID_MISC_DUCKTAPE;
	if (itname == "lcd")
		return IID_MISC_LCD;
	if (itname == "circuit board")
		return IID_MISC_CIRCUITBOARD;
	if (itname == "copper wire")
		return IID_MISC_COPPERWIRE;
	if (itname == "nukacola")
		return IID_MISC_NUKACOLA;
	if (itname == "hmx compound")
		return IID_MISC_HMXCOMPOUND;
	if (itname == "chewing gum")
		return IID_MISC_GUM;
	if (itname == "x-ray tube")
		return IID_MISC_XRAYTUBE;

	if (itname == "sprintkit")
		return IID_EQUIP_SPRINTKIT;

	if (itname == "tesla")
		return IID_BUILD_TTURRET;

	return 0;
};



float(float slotno, float iid) FitsInSlot =
{

	if (!iid)	//nothing can be put in every slot.
		return true;
	if (slotno == 1 || slotno == 2)
		return IsShootable(iid);
	if (slotno == 3)
		return IsArmor(iid);
	if (slotno == 4)
		return IsEquip(iid);
	if (slotno == 17)
		return IsJunk(iid);
	if (slotno == 18)
		return IsJunk(iid);
	if (slotno == 19)
		return IsJunk(iid);
	if (slotno == 20)
		return IsJunk(iid);
	if (slotno == 21)
		return IsJunk(iid);
	if (slotno == 22)
		return IsJunk(iid);


	return true;
};

string (float slot, float int) PerkName =
{
		if (int == 1)
			return "Support Fire";
		if (int == 2)
			return "I Disappear";
		if (int == 3)
			return "Puppet Master";
		if (int == 4)
			return "Stay Low";
		if (int == 5)
			return "Surgical Precision";
		if (int == 6)
			return "Mutation";
		if (int == 7)
			return "Narrow Escape";
		if (int == 8)
			return "Schizophrenia!";
		if (int == 9)
			return "Placebo Effect";
		if (int == 10)
			return "Economic Moves";
		if (int == 11)
			return "Nuclear Man";
};

string (float slot, float int) PerkNameCentered =
{
		if (int == 1)
			return "   Support Fire   ";
		if (int == 2)
			return "   I  Disappear   ";
		if (int == 3)
			return "  Puppet  Master  ";
		if (int == 4)
			return "     Stay Low     ";
		if (int == 5)
			return "Surgical Precision";
		if (int == 6)
			return "     Mutation     ";
		if (int == 7)
			return "  Narrow  Escape  ";
		if (int == 8)
			return "  Schizophrenia!  ";
		if (int == 9)
			return "  Placebo Effect  ";
		if (int == 10)
			return "  Economic Moves  ";
		if (int == 11)
			return "   Nuclear  Man   ";
};


string(float int) GetPerkImage =
{
	if (int == 1)
		return "gui/perks/Support Fire.jpg";
	else if (int == 2)
		return "gui/perks/I Disappear.jpg";
	else if (int == 3)
		return "gui/perks/Puppet Master.jpg";
	else if (int == 4)
		return "gui/perks/Stay Low.jpg";
	else if (int == 5)
		return "gui/perks/Surgical Precision.jpg";
	else if (int == 6)
		return "gui/perks/Mutation.jpg";
	else if (int == 7)
		return "gui/perks/Narrow Escape.jpg";
	else if (int == 8)
		return "gui/perks/Schizophrenia.jpg";
	else if (int == 9)
		return "gui/perks/Placebo Effect.jpg";
	else if (int == 10)
		return "gui/perks/Economic Moves.jpg";
	else if (int == 11)
		return "gui/perks/Nuclear Man.jpg";
	else if (int == 12)
		return "gui/perks/Commander.jpg";
	else
		return "gui/perks/none.jpg";

};


string(float int) GetPerkDesc =
{
	if (int == 1)
		return "For those who like climbing into the trenches! This perk";
	else if (int == 2)
		return "Being a hero is overrated! You try your best to not look";
	else if (int == 3)
		return "Your black heart has a strange effect on the denizens of";
	else if (int == 4)
		return "You know how to take cover! While prone, any attack that";
	else if (int == 5)
		return "You have gained in-depth knowledge of vital organs. Your";
	else if (int == 6)
		return "Exposure to the wasteland has caused your body to mutate";
	else if (int == 7)
		return "For when your friend really, really needs that stimpack!";
	else if (int == 8)
		return "You are haunted by the ghost of a wasteland wanderer who";
	else if (int == 9)
		return "Through convincing acts you can force people (even you!)";
	else if (int == 10)
		return "You can move more effectively than most people. Movement";
	else if (int == 11)
		return "Your body has adapted to radiation to the point that you";
	else if (int == 12)
		return "Your supreme leadership affects all of those around you,";
	else
		return "";

};

string(float int) GetPerkDesc2 =
{
	if (int == 1)
		return "will drastically reduce recoil while lying down.";
	else if (int == 2)
		return "dangerous - making monsters forget about you!";
	else if (int == 3)
		return "the wasteland - you can turn a few to your side!";
	else if (int == 4)
		return "is non-aimed deals much less damage to you.";
	else if (int == 5)
		return "aimed attacks are boosted by your Doctor skill.";
	else if (int == 6)
		return "and change, granting mysterious effects.";
	else if (int == 7)
		return "Provides you a few free seconds to escape danger.";
	else if (int == 8)
		return "sometimes takes over your body, boosting all skills.";
	else if (int == 9)
		return "to believe you are giving them a super stimpack.";
	else if (int == 10)
		return "actions suffer much less time-related penalties.";
	else if (int == 11)
		return "can absorb small amounts, healing you as well!";
	else if (int == 12)
		return " inspiring allies and causing foes to tremble.";
	else
		return "";

};

string(float int) GetPerkRequirements =
{
	if (int == 1)
		return "Combat 5";
	else if (int == 2)
		return "Sneak 5, Speech 5";
	else if (int == 3)
		return "Speech 7, Science 5";
	else if (int == 4)
		return "Combat 3";
	else if (int == 5)
		return "Doctor 5, Sneak 3";
	else if (int == 6)
		return "None";
	else if (int == 7)
		return "Sneak: 7";
	else if (int == 8)
		return "None";
	else if (int == 9)
		return "Speech 5, Doctor 5";
	else if (int == 10)
		return "Combat 5, Sneak 5";
	else if (int == 11)
		return "None";
	else if (int == 12)
		return "Combat 7, Speech 5";
	else if (int == 13)
		return "Sneak 5, Science 5";
	else
		return "";

};

float(float iid) GetItemThreatLevel =
{
	if (iid == IID_NONE)
		return 0;
	if (iid == IID_WP_KNIFE)
		return 0;
	if (iid == IID_WP_SLEDGE)
		return 0;
	if (iid == IID_WP_SPEAR)
		return 0;
	if (iid == IID_WP_POWERAXE)
		return 0;
	if (iid == IID_WP_USP)
		return 0;
	if (iid == IID_WP_USP_S)
		return 0;
	if (iid == IID_WP_GLOCK)
		return 0;
	if (iid == IID_WP_GLOCK_S)
		return 0;
	if (iid == IID_WP_DEAGLE)
		return 0;
	if (iid == IID_WP_DEAGLE_M)
		return 0;
	if (iid == IID_WP_NEEDLER)
		return 2;
	if (iid == IID_WP_ALIENBLASTER)
		return 2;
	if (iid == IID_WP_PIPERIFLE)
		return 0;
	if (iid == IID_WP_WINCHESTER)
		return 0;
	if (iid == IID_WP_MOSSBERG)
		return 1;
	if (iid == IID_WP_JACKHAMMER)
		return 2;
	if (iid == IID_WP_GREASEGUN)
		return 0;
	if (iid == IID_WP_GREASEGUN_S)
		return 0;
	if (iid == IID_WP_MP9)
		return 0;
	if (iid == IID_WP_MP9_S)
		return 0;
	if (iid == IID_WP_RANGEMASTER)
		return 0;
	if (iid == IID_WP_RANGEMASTER_SCOPE)
		return 0;
	if (iid == IID_WP_AK112)
		return 1;
	if (iid == IID_WP_AK47)
		return 1;
	if (iid == IID_WP_AK112_M)
		return 1;
	if (iid == IID_WP_AK74)
		return 1;
	if (iid == IID_WP_ACR)
		return 2;
	if (iid == IID_WP_DKS1)
		return 1;
	if (iid == IID_WP_DKS1_S)
		return 1;
	if (iid == IID_WP_MOONLIGHT)
		return 2;
	if (iid == IID_WP_G11)
		return 2;
	if (iid == IID_WP_GAUSERIFLE)
		return 2;
	if (iid == IID_WP_PULSERIFLE)
		return 2;
	if (iid == IID_WP_TURBOPLASMA)
		return 2;
	if (iid == IID_WP_PLASMACARBINE)
		return 2;
	if (iid == IID_WP_FNFAL)
		return 2;
	if (iid == IID_WP_ROCKETLAUNCHER)
		return 2;
	if (iid == IID_WP_LASERGATLING)
		return 2;


	if (iid == IID_ARM_LEATHER)
		return 0;
	if (iid == IID_ARM_VANDAL)
		return 0;
	if (iid == IID_ARM_METAL)
		return 0;
	if (iid == IID_ARM_TESLA)
		return 1;
	if (iid == IID_ARM_COMBAT)
		return 1;
	if (iid == IID_ARM_SEVA)
		return 1;
	if (iid == IID_ARM_FORCE)
		return 2;
	if (iid == IID_ARM_LPOWER)
		return 2;



//	bprint(PRINT_MEDIUM, ftos(iid), " without a threat level!\n");
	return 0;
};