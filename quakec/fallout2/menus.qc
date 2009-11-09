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
	return ("êëêëêë supply merchant êëêëêë\n"
		"\n"
		"ê1ë perk [slot 1]  \n"
		"ê2ë perk [slot 2]  \n"
		"ê3ë body armour    \n"
		"ê4ë ammunition     \n"
		"ê5ë weapons        \n"
		"ê6ë equipment      \n"
		"ê7ë chems          \n"
		"ê8ë special        \n"
		"êeë leave          \n");
};

string () WeaponString =
{
	return ("weaponry\n"
		"\n"
		"ê1ë melee          \n"
		"ê2ë explosives     \n"
		"ê3ë pistols&smgs   \n"
		"ê4ë shotguns       \n"
		"ê5ë rifles         \n"
		"ê6ë heavy guns     \n"
		"êeë leave          \n");
};

string () TraitString =
{
	return ("traits\n"
		"\n"
		"ê1ë unidextrous      \n"
		"ê2ë ruthless         \n"
		"ê3ë chem addict      \n"
		"ê4ë red scare        \n"
		"ê5ë living lightning \n"
		"ê6ë one in a million \n"
		"êeë leave            \n");
};


string () ThrownString =
{
	return ("ê!ë explosives ê!ë\n"
		"\n"
		"ê1ë flash bomb               8$\n"
		"ê2ë frag grenade            12$\n"
		"ê3ë ultra-light necklace     3$\n"
		"ê4ë hmx compound             2$\n"
		"ê5ë rdx crystals             2$\n"
		"ê6ë explosive rocket        10$\n"
		"êeë exit                       \n");
};

string () BuildString =
{
	return ("BUILD A STRUCTURE\n"
		"      NAME         COST IN JUNK\n"
		"\n"
		"ê1ë Super Flash Bomb          4\n"
		"ê2ë Homemade Grenade          5\n"
		"ê3ë Needler Scope             5\n"
		"ê4ë Ghost Goggles             7\n"
		"ê5ë Gas Bomb                 10\n"
		"ê6ë Deflector Ring           15\n"
		"ê7ë Auto-Doc                 20\n"
		"êeë exit                       \n");
};

string () HelmetString =
{
	return ("helmets\n"
		"protect you from headshots so\n"
		"make sure you use a decent one\n"
		"\n"
		"                  ABS% WT DEFLECT PRC\n"
		"ê1ë combat helm   -10% 01    5%    05\n"
		"ê2ë combat helm 2  +0% 02    5%    20\n"
		"ê3ë heavy-duty    +20% 03    5%    20\n"
		"ê4ë ceramic helm  -20% ...");
};

string () ArmorString =
{
	return ("body armour           wt  abs  prc\n"
		"\n"
		"ê1ë kevlar shirt     04 1/25%  50$\n"
		"ê2ë leather armor    07 3/30% 100$\n"
		"ê3ë double kevlar    09 1/50% 125$\n"
		"ê4ë ceramic armor    15 7/25% 150$\n"
		"ê5ë combat armor     12 6/40% 200$\n"
		"ê6ë brotherhood      17 7/45% 250$\n"
		"ê7ë force armor      06 9/15% 300$\n"
		"ê8ë power armor      25 8/50% 400$\n");
};

string () PerkString =
{
	return ("perks\n"
		"\n"
		"ê 1ë unbreakable          \n"
		"ê 2ë feral swiftness      \n"
		"ê 3ë stunt man            \n"
		"ê 4ë kick in the door!    \n"
		"ê 5ë quick draw           \n"
		"ê 6ë light step           \n"
		"ê 7ë surprise attack      \n"
		"ê 8ë large and in charge  \n"
		"ê 9ë nintendo gamer       \n"
		"ê 0ë extrasensory paranoia\n"
		"êF1ë last man standing    \n"
		"êF2ë bodysnatcher         \n"
		"êF3ë natural aim          \n"
		"êF4ë dark alchemist       \n"
		"êF5ë renegade commando    \n"
		"êF6ë power shot           \n"
		"êF7ë one in a million     \n"
		"êF8ë master blaster       \n"
		"êF9ë death wish           \n");
};


string () AmmoString =
{
	return ("++ ammunition ++\n"
		"\n"
		"  AMMO  | POWER | TYPE |  COST[50]\n"
		"ê1ë 10mm       2       FMJ   ê015ë\n"
		"ê2ë 12gauge    4      SHOT   ê025ë\n"
		"ê3ë 44 mag     3        AP   ê030ë\n"
		"ê4ë 45 acp     3       JHP   ê020ë\n"
		"ê5ë .223       3       FMJ   ê030ë\n"
		"ê6ë 5mm        3       JHP   ê025ë\n"
		"ê7ë .308       4        AP   ê040ë\n"
		"ê8ë needle     2        AP   ê035ë\n"
		"ê9ë caseless   3       FMJ   ê040ë\n"
		"ê0ë energycell 4      CELL   ê055ë\n");
};

string () AmmoString2 =
{
	return ("++ ammunition ++\n"
		"you get a discount when fighting ghouls!\n"
		"  AMMO  | POWER | TYPE |  COST[50]\n"
		"ê1ë 10mm       2       FMJ   ê005ë\n"
		"ê2ë 12gauge    4       N/A   ê008ë\n"
		"ê3ë 44 mag     3        AP   ê010ë\n"
		"ê4ë 45 acp     3       JHP   ê007ë\n"
		"ê5ë .223       3       FMJ   ê010ë\n"
		"ê6ë 5mm        3       JHP   ê008ë\n"
		"ê7ë .308       4        AP   ê015ë\n"
		"ê8ë needle     2        AP   ê010ë\n"
		"ê9ë caseless   3       FMJ   ê020ë\n"
		"ê0ë energycell 4       N/A   ê015ë\n");
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
    return ("ê?ë miscellaneous ê?ë\n"
		"\n"
		"ê1ë junk                   1$\n"
		"ê2ë nuka-cola              1$\n"
		"ê3ë chemicals              2$\n"
		"ê4ë aerosol can            2$\n"
		"ê5ë circuitboard           5$\n"
		"ê6ë steel pipe             3$\n"
		"ê7ë duct tape              1$\n"
		"ê8ë chewing gum            1$\n"
		"ê9ë copper wire            1$\n"
		"ê0ë x-ray tube             7$\n");
};


string () MeleeString =
{
	return ("MELEE WEAPONS\n"
		"WEAPON | TYPE | WT | PRICE   \n"
		"\n"
		"ê1ë knife              01 06$\n"
		"ê2ë wrench             03 08$\n"
		"ê3ë axe                08 09$\n"
		"ê4ë spear              04 12$\n"
		"êeë exit                     \n");
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
	return ("Pistols and Submachineguns\n"
		"    WEAPON | CAL | WEIGHT | PRICE \n"
		"\n"
		"ê1ë usp socom  .45   01      50$\n"
		"ê2ë d. eagle   .44   02      70$\n"
		"ê3ë glock 21  10mm   01      40$\n"
		"ê4ë needler pistol   02     120$\n"
		"ê5ë h&k mp10  10mm   03     150$\n"
		"ê6ë h&k ump    .45   03     170$\n");
};

string () ShotgunString =
{
	return ("shotguns\n"
		"    WEAPON | TYPE | WEIGHT | PRICE \n"
		"\n"
		"ê1ë pipe rifle .44   02      15$\n"
		"ê2ë winchester 12g   03      60$\n"
		"ê3ë citykiller 12g   04     240$\n"
		"ê4ë jackhammer 12g   07     350$\n");
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
	return ("rifles\n"
		"   RIFLE | TYPE | WEIGHT | PRICE \n"
		"\n"
		"ê1ë rangemaster 223  03     110$\n"
		"ê2ë ak-112      5mm  04     180$\n"
		"ê3ë fn fal      308  08     190$\n"
		"ê4ë widowmaker  308  10     270$\n"
		"ê5ë moonlight   223  05     280$\n"
		"ê6ë hk g11      4mm  06     220$\n");
};

string () HeavyString =
{
	return ("other weaponry\n"
		"  WEAPON | TYPE | WEIGHT | PRICE \n"
		"\n"
		"ê1ë rpg-7 missile launcher     10 350$\n"
		"ê2ë m72 gauss rifle            07 550$\n"
		"ê3ë plasma pistol              02 250$\n"
		"ê4ë m25a2 phased plasma rifle  11 450$\n"
		"ê5ë m95a1 plasma carbine       08 550$\n"
		"ê6ë rsb-80 heavy plasma gun    25 650$\n");
};

string () ChemString =
{
	return ("drugs\n     DRUG  |   EFFECTS   |  PRICE\n"
		"\n"
		"ê1ë stimpack   heal over time 10$\n"
		"ê2ë rad-x      anti-radiation 15$\n"
		"ê3ë adrenaline*+speed/jump    15$\n"
		"ê4ë medkit*    endless heal   25$\n"
		"ê5ë superstim* instant heal   25$\n"
		"ê6ë psycho*    +resist damage 25$\n"
		"ê7ë berserk*   +strength      25$\n"
		"\n"
		"* requires medic                 \n");
};

string () ChemString2 =
{
	return ("chems\n     DRUG  |   EFFECTS   |  PRICE\n"
		"\n"
		"ê1ë adrenaline +60 speed/jump  5$\n"
		"ê2ë stimpack   heals 40       10$\n"
		"ê3ë psycho+    +60 hp/no pain 15$\n"
		"ê4ë medkit*    heals 20+50    15$\n"
		"ê5ë berserk*   adren+psycho   15$\n"
		"ê6ë jet*       aim better     15$\n"
		"êeë exit                         \n"
		"\n"
		"+ requires medic                 \n"
		"* requires advanced medic        \n");
};

string () EnergyWeaponsString =
{
	return ("high-tech weaponry\n"
		"  WEAPON | TYPE | WEIGHT | PRICE \n"
		"\n"
		"ê1ë [*] flash gun         semi 03 21$\n"
		"ê2ë [&] plasma rifle      semi 08 34$\n"
		"ê3ë [*] laser rifle       semi 11 40$\n"
		"ê4ë [*] laser carbine     auto 06 57$\n"
		"ê5ë [?] alien blaster     semi 02 72$\n");
};

//ê6ë bozar                    14 81$\nê7ë firestorm                12 97$

string () HeavyGunsString =
{
	return ("heavy guns\n"
		"    WEAPON | TYPE | WEIGHT | PRICE \n"
		"\n"
		"ê1ë light support weapon     15 55$\n"
		"ê2ë rocket launcher          11 75$\n"
		"ê3ë 50oc flamethrower        16 35$\n"
		"ê4ë steyr amr .50 flechette  17 72$\n"
		"ê5ë m72 gauss rifle 2mm      12 81$\n"
		"ê6ë bozar                    14 81$\n"
		"ê7ë firestorm                12 97$");
};

string () de_dust =
{
	return ("             DE_DUST          \n"
		"        (BOMB/DEFUSE MAP)     \n"
		"\n"
		"rangers have obtained two ufos\n"
		"that have crash landed in the \n"
		"desert. raiders must blow them\n"
		"up with c4 before its too late\n"
		"\n"
		"(activate electronic tools and\n"
		"c4 by pressing 4)         ...");
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
	return ("+ special equipment +\n"
		"we only let you use one of these!\n"
		"ê1ë medic's bag                   ê5cë\n"
		"ê2ë ghost goggles                 ê5cë\n"
		"ê3ë stealth boy                   ê5cë\n"
		"ê4ë belt pouch                    ê5cë\n"
		"ê5ë backpack                      ê5cë\n"
		"ê6ë toolkit mark ii               ê5cë\n"
		"ê7ë climbing gear                 ê5cë\n"
		"ê8ë enhanced battery              ê5cë\n"
		"\n");
};


string () EnterString =
{
	return ("êëêëêëêë fallout êëêëêëêë\n"
		"êë   a post-nuclear fps   êë\n"
		"\n"
            "scour the wastes with your friends!\n"
            "this quake mod is entirely based on\n"
            "the fallout series of computer rpgs\n"
            "for best performance the fte client\n"
            "is required for play, available at:\n"
            "        êë www.fteqw.com êë        \n" 
            "\n" 
            "êpress 1 to continueë\n"
            "\n" 

		"\n");
};


string () WaitString =
{
	return ("êëêëêëêë fallout êëêëêëêë\n"
		"êë   a post-nuclear fps   êë\n"
		"\n"
            "scour the wastes with your friends!\n"
            "this quake mod is entirely based on\n"
            "the fallout series of computer rpgs\n"
            "for best performance the fte client\n"
            "is required for play, available at:\n"
            "        êë www.fteqw.com êë        \n" 
            "\n" 
            "êwait until next roundë\n"
            "\n" 

		"\n");
};

string () LockpickString =
{
	return ("êë Picking êë\n"
            "\n\n"
            "this is tricky business! wait until\n"
            "all three tumblers are in place by \n"
            "watching for three [!] symbols. but\n"
            "you have to be quick! failure means\n" 
            "you jam the lock.\n\n" 
            "êpress 1 to pickë\n"
            "êpress 2 to quitë\n");
};

string () DefuseString =
{
	return ("êë Defuse êë\n"
            "\n\n"
            "this is tricky business! wait until\n"
            "all three circuits are in place by \n"
            "watching for three [!] symbols. but\n"
            "you have to be quick! failure means\n" 
            "you reset the switch bypass.\n\n" 
            "êpress 1 to pickë\n"
            "êpress 2 to quitë\n");
};



string () MissionString =
{
	return ("êëêëêë   mission  select  êëêëêë\n"
		"\n"
		"\n"
		"ê1ë hostage rescue   \n"
		"ê2ë capture supplies \n"
		"ê3ë shadow mission   \n"
		"ê4ë zombie mission   \n"
		"ê5ë destroy supplies \n"
		"6ë bomb defusal     \n\n"
		"ê7ë grab one later   \n");
};

string () DepartString =
{
	return ("êëêëêë saddle up! êëêëêë\n"
		"\n"
		"\n"
		"ê1ë take off to mission site\n"
		"ê2ë wait a second           \n\n\n");
};