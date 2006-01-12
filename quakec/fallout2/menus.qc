/*

	return ("
weaponry\n\n
ê1ë melee          \n
ê2ë thrown         \n
ê3ë pistols&smgs   \n
ê4ë shotguns       \n
ê5ë rifles         \n
ê6ë heavy guns     \n
êeë leave          
*/

/*
ê1ë 1911a1     .45   01       5$\n
ê2ë d. eagle   .44   02       7$\n
ê3ë mk23 socom .45   02       9$\n
ê4ë h&k mp7    4mm   03      14$\n
ê5ë h&k mp5    9mm   03      17$\n
ê6ë alien blaster    02      21$\n

ê1ë pipe rifle .44   02       4$\n
ê2ë winchester 12g   03       8$\n
ê3ë mossberg   12g   04      14$\n
ê4ë citykiller 12g   05      35$\n

ê1ë rangemaster 7mm  03      11$\n
ê2ë ak-112      5mm  04      21$\n
ê3ë remington  .308  05      24$\n
ê4ë ak-74       5mm  04      27$\n
ê5ë moonlight  .223  05      36$\n
ê6ë sa-80       5mm  05      23$\n
ê7ë plasma rifle     07      41$\n
ê8ë gauss rifle 2mm  08      51$\n
*/

string () ShopString =
{
	return ("--- S H O P -------\n\nê1ë traits         \nê2ë perks          \nê3ë body armour    \nê4ë protection     \nê5ë weapons        \nê6ë equipment      \nê7ë chems          \nê8ë special        \nêeë leave          \n");
};

string () WeaponString =
{
	return ("weaponry\n\nê1ë melee          \nê2ë thrown         \nê3ë pistols&smgs   \nê4ë shotguns       \nê5ë rifles         \nêeë leave          \n");
};

string () TraitString =
{
	return ("traits\n\nê1ë one handed     \nê2ë small frame    \nê3ë bruiser        \nê4ë heavy handed   \nê5ë bloody mess    \nê6ë bad luck       \nêeë leave          \n");
};

string () ThrownString =
{
	return ("grenades\n	GRENADE | COST		  \n\nê1ë smoke grenade             3$\nê2ë frag grenade              4$\nê3ë emp grenade               5$\nê4ë flashbang                 7$\nêeë exit                        \n");
};

string () BuildString =
{
	return ("BUILD A STRUCTURE\n      NAME | UPGRADE |   SCRAPS\n\nê1ë Mr. Ammo                  4\nê2ë Barricade                 6\nê3ë AutoDoc(tm)              10\nê4ë Robo-Fang                11\nêeë exit                       \n");
};

string () HelmetString =
{
	return ("helmets\nprotect you from headshots so\nmake sure you use a decent one\n\n                  ABS% WT DEFLECT PRC\nê1ë combat helm   -10% 01    5%    05\nê2ë combat helm 2  +0% 02    5%    20\nê3ë heavy-duty    +20% 03    5%    20\nê4ë ceramic helm  -20% ...");
};

string () ArmorString =
{
	return ("body armour           wt  abs  prc\n\nê1ë light kevlar      03 1/20% 03$\nê2ë leather armor     05 2/30% 08$\nê3ë kevlar armor      09 3/35% 10$\nê4ë metal armor       15 5/35% 12$\nê5ë combat armor      12 4/40% 25$\nê6ë brotherhood armor 17 5/45% 35$\nê7ë force armor       06 7/10% 45$\nê8ë metal armor mkii  20 8/50% 55$\n");
};

string () PerkString =
{
	return ("perks\n  ABILITY | FRAGS NEEDED \n\nê1ë bonus movement       2\nê2ë strong back          2\nê3ë quick pockets        2\nê4ë awareness            2\nê5ë silent running       3\nê6ë better criticals     3\nê7ë bonus ranged damage  3\nê8ë divine favor         3\nê9ë slayer               3\nê0ë sharpshooter         4\n");
};

string () ProtectString =
{
	return ("++ high-tech protective devices ++ \n\n  HARDWARE | SHIELDS VS |  PRICE   \nê1ë energy amulet  |damage: 7% ê15ë\nê2ë force field    |front: 15% ê20ë\nê3ë safety ring    |absorb: 3  ê35ë\nê4ë smokescreen    |obscures   ê40ë\nê5ë sentient cube  |regenerate ê45ë\n");
};


/*return ("++ high-tech protective devices ++ \n\n
  HARDWARE | SHIELDS VS |  PRICE   \n
ê1ë energy amulet  |damage: 7% ê15ë\n
ê2ë force field    |front: 15% ê20ë\n
ê3ë safety ring    |absorb: 3  ê35ë\n
ê4ë smokescreen    |obscures   ê40ë\n
ê5ë sentient cube  |regenerate ê45ë\n");*/


string () OtherString =
{
	return
		"++ miscellaneous items ++\n"
		"  ITEM   |   CLASS   |   PRICE     \n"
		"\n"
		"\n"
		"ê1ë (25) bandages for medic      2$\n"
		"ê2ë (5) metal scraps             5$\n"
		"ê3ë (20) mr.ammo!               20$\n"
		"ê4ë (20) auto-doc!              20$\n"
		"ê5ë (20) shield-gen!            20$\n"
		;
};


string () MeleeString =
{
	return ("MELEE WEAPONS\nWEAPON | TYPE | WT | PRICE   \n\nê1ë knife        melee 01 01$\nê2ë hand axe     melee 08 03$\nê3ë vibroblade   melee 04 10$\nê4ë power axe    melee 07 15$\nêeë exit                     \n");
};

/*
ê6ë h&k mp5   9mmP   03      17$\n
ê7ë h&k mp7  4.60mm  03      14$\n
ê8ë fn p90   5.57mm  03      22$\n
ê9ë h&k mp10  10mm   03      24$\n
ê0ë thompson  .45    03      20$\n


ê6ë h&k mp5   9mmP   03      17$\nê7ë h&k mp7  4.60mm  03      14$\nê8ë fn p90   5.57mm  03      22$\nê9ë h&k mp10  10mm   03      24$\nê0ë thompson  .45    03      20$\n
*/

string () PistolString =
{
	return ("Pistols and Submachineguns\n    WEAPON | CAL | WEIGHT | PRICE \n\nê1ë mk23 socom .45   01       5$\nê2ë d. eagle   .44   02       7$\nê3ë needler pistol   02       9$\nê4ë h&k mp7    4mm   03      14$\nê5ë grease gun 9mm   03      17$\nê6ë alien blaster    02      21$\n");
};

string () ShotgunString =
{
	return ("shotguns\n    WEAPON | TYPE | WEIGHT | PRICE \n\nê1ë pipe rifle .44   02       4$\nê2ë winchester 12g   03       8$\nê3ë mossberg   12g   04      14$\nê4ë citykiller 12g   05      35$\n");
};

/*
ê6ë dks-1      .338   bolt 08 32$\n
ê7ë moonlight  .223   auto 06 54$\n
ê8ë xl70e3      5mm   auto 08 27$\n
ê9ë fn-fal     .308   auto 04 51$\n
ê0ë sa-80      .223   auto 07 45$\n

*/
string () RifleString =
{
	return ("rifles\n   RIFLE | TYPE | WEIGHT | PRICE \n\nê1ë rangemaster 7mm  03      11$\nê2ë ak-112      5mm  04      21$\nê3ë remington  .308  05      24$\nê4ë ak-74       5mm  04      27$\nê5ë moonlight  .223  05      36$\nê6ë sa-80       5mm  05      32$\nê7ë plasma rifle     12      41$\nê8ë gauss rifle 2mm  07      32$\nê9ë fn-fal     .308  09      25$\n");
};

string () ChemString =
{
	return ("drugs\n     DRUG  |   EFFECTS   |  PRICE\n"
		"\nê1ë stimpack   heals 05+20     3$\n"
		"ê2ë medkit+    heals 10+50     5$\n"
		"ê3ë superstim* heals 20+60    12$\n"
		"ê4ë adrenaline*+speed/jump    10$\n"
		"ê5ë jet*       aim better     18$\n"
		"êeë exit                         \n"
		"\n"
		"+ requires medic                 \n"
		"* requires advanced medic        \n");
};

string () ChemString2 =
{
	return ("chems\n     DRUG  |   EFFECTS   |  PRICE\n"
		"\nê1ë adrenaline +60 speed/jump  3$\n"
		"ê2ë stimpack   heals 40        5$\n"
		"ê3ë psycho+    +60 hp/no pain 11$\n"
		"ê4ë medkit*    heals 20+50    12$\n"
		"ê5ë berserk*   adren+psycho   12$\n"
		"ê6ë jet*       aim better     15$\n"
		"êeë exit                         \n"
		"\n"
		"+ requires shaman                \n"
		"* requires advanced shaman       \n");
};

string () EnergyWeaponsString =
{
	return ("high-tech weaponry\n  WEAPON | TYPE | WEIGHT | PRICE \n\nê1ë [*] flash gun         semi 03 21$\nê2ë [&] plasma rifle      semi 08 34$\nê3ë [*] laser rifle       semi 11 40$\nê4ë [*] laser carbine     auto 06 57$\nê5ë [?] alien blaster     semi 02 72$\n");
};

//ê6ë bozar                    14 81$\nê7ë firestorm                12 97$

string () HeavyGunsString =
{
	return ("heavy guns\n    WEAPON | TYPE | WEIGHT | PRICE \n\nê1ë light support weapon     15 55$\nê2ë rocket launcher          11 75$\nê3ë 50oc flamethrower        16 35$\nê4ë steyr amr .50 flechette  17 72$\nê5ë m72 gauss rifle 2mm      12 81$\nê6ë bozar                    14 81$\nê7ë firestorm                12 97$");
};

string () de_dust =
{
	return ("             DE_DUST          \n        (BOMB/DEFUSE MAP)     \n\nrangers have obtained two ufos\nthat have crash landed in the \ndesert. raiders must blow them\nup with c4 before its too late\n\n(activate electronic tools and\nc4 by pressing 4)         ...");
};

/*
ê4ë motion sensor                 ê20cë\n
ê5ë extra magazines               ê20cë\n
ê6ë electronic tools mark ii      ê30cë\n
ê7ë climbing gear                 ê30cë\n
ê8ë remote camera                 ê40cë\n
ê9ë cooling module                ê50cë\n
ê0ë laser defense field           ê50cë\n
*/



string () EquipmentString =
{
	return ("+ special equipment +\npress your c key to activate!\n\nê1ë medic's bag                  ê15cë\nê2ë security alarm                ê15cë\nê3ë remote camera                 ê15cë\nê4ë belt pouch                    ê15cë\nê5ë backpack                      ê15cë\nê6ë toolkit mark ii               ê15cë\nê7ë climbing gear                 ê15cë\nê8ë enhanced battery              ê15cë\nê9ë stealth boy                   ê45cë\n\n");
};
