#include "../plugin.h"

#define MAX_ELEMENTS 128

int K_UPARROW;
int K_DOWNARROW;
int K_LEFTARROW;
int K_RIGHTARROW;
int K_ESCAPE;
int K_MOUSE1;
int K_HOME;
int K_SHIFT;


#define	MAX_CL_STATS		32
#define	STAT_HEALTH			0
#define	STAT_WEAPON			2
#define	STAT_AMMO			3
#define	STAT_ARMOR			4
#define	STAT_WEAPONFRAME	5
#define	STAT_SHELLS			6
#define	STAT_NAILS			7
#define	STAT_ROCKETS		8
#define	STAT_CELLS			9
#define	STAT_ACTIVEWEAPON	10
#define	STAT_TOTALSECRETS	11
#define	STAT_TOTALMONSTERS	12
#define	STAT_SECRETS		13		// bumped on client side by svc_foundsecret
#define	STAT_MONSTERS		14		// bumped by svc_killedmonster
#define	STAT_ITEMS			15

//some engines can use more.
//any mod specific ones should be 31 and downwards rather than upwards.


#define	IT_GUN1			(1<<0)
#define	IT_GUN2			(1<<1)		//the code assumes these are linear.
#define	IT_GUN3			(1<<2)		//be careful with strange mods.
#define	IT_GUN4			(1<<3)
#define	IT_GUN5			(1<<4)
#define	IT_GUN6			(1<<5)
#define	IT_GUN7			(1<<6)
#define	IT_GUN8			(1<<7)	//quake doesn't normally use this.

#define	IT_AMMO1		(1<<8)
#define	IT_AMMO2		(1<<9)
#define	IT_AMMO3		(1<<10)
#define	IT_AMMO4		(1<<11)

#define	IT_GUN0			(1<<12)

#define	IT_ARMOR1		(1<<13)
#define	IT_ARMOR2		(1<<14)
#define	IT_ARMOR3		(1<<15)
#define	IT_SUPERHEALTH	(1<<16)

#define	IT_PUP1			(1<<17)
#define	IT_PUP2			(1<<18)
#define	IT_PUP3			(1<<19)
#define	IT_PUP4			(1<<20)
#define	IT_PUP5			(1<<21)
#define	IT_PUP6			(1<<22)

#define	IT_RUNE1		(1<<23)
#define	IT_RUNE2		(1<<24)
#define	IT_RUNE3		(1<<25)
#define	IT_RUNE4		(1<<26)

//these are linear and treated the same
#define	numpups			6

//the names of the cvars, as they will appear on the console
#define UI_NOSBAR "ui_defaultsbar"
#define UI_NOIBAR "ui_noibar"
#define UI_NOFLASH "ui_nosbarflash"

static char *weaponabbreviation[] = {	//the postfix for the weapon anims
	"shotgun",
	"sshotgun",
	"nailgun",
	"snailgun",
	"rlaunch",	//grenades actually.
	"srlaunch",
	"lightng"
};
#define numweaps (sizeof(weaponabbreviation) / sizeof(char *))

static char *pupabbr[] = {	//the postfix for the powerup anims
	"key1",
	"key2",
	"invis",
	"invul",
	"suit",
	"quad"
};
static char *pupabbr2[] = {	//the postfix for the powerup anims
	"key1",
	"key2",
	"invis",
	"invuln",
	"suit",
	"quad"
};

//0 = owned, 1 selected, 2-7 flashing
static qhandle_t con_chars;
static qhandle_t pic_weapon[8][numweaps];
static qhandle_t sbarback, ibarback;

//0 = owned, 1-6 flashing
static qhandle_t pic_pup[7][numpups];
static qhandle_t pic_armour[3];
static qhandle_t pic_ammo[4];
static qhandle_t pic_rune[4];
static qhandle_t pic_num[13];
static qhandle_t pic_anum[11];

//faces
static qhandle_t pic_face[5];
static qhandle_t pic_facep[5];
static qhandle_t pic_facequad;
static qhandle_t pic_faceinvis;
static qhandle_t pic_faceinvisinvuln;
static qhandle_t pic_faceinvuln;
static qhandle_t pic_faceinvulnquad;

static int currenttime;
static int gotweapontime[numweaps];
static int gotpuptime[numpups];

float sbarminx;
float sbarminy;
float sbarscalex;
float sbarscaley;

static int hudedit;

enum {
	DZ_BOTTOMLEFT,
	DZ_BOTTOMRIGHT
};

typedef struct {
	int width;
	int height;
	float defaultx;	//used if couldn't load a config
	float defaulty;
	int defaultzone;
	float defaultalpha;
	void (*DrawElement)(void);
} hudelementtype_t;

void Hud_SBar(void);
void Hud_ArmourPic(void);
void Hud_ArmourBig(void);
void Hud_HealthPic(void);
void Hud_HealthBig(void);
void Hud_CurrentAmmoPic(void);
void Hud_CurrentAmmoBig(void);
void Hud_IBar(void);
void Hud_W_Shotgun(void);
void Hud_W_SuperShotgun(void);
void Hud_W_Nailgun(void);
void Hud_W_SuperNailgun(void);
void Hud_W_GrenadeLauncher(void);
void Hud_W_RocketLauncher(void);
void Hud_W_Lightning(void);
void Hud_Key1(void);
void Hud_Key2(void);
void Hud_PUPInvis(void);
void Hud_PUPInvuln(void);
void Hud_PUPSuit(void);
void Hud_PUPQuad(void);
void Hud_Rune1(void);
void Hud_Rune2(void);
void Hud_Rune3(void);
void Hud_Rune4(void);
void Hud_Shells(void);
void Hud_Nails(void);
void Hud_Rockets(void);
void Hud_Cells(void);

hudelementtype_t hetype[] = {
	{
		320, 24, 
		0, -24, DZ_BOTTOMLEFT,
		0.3f,
		Hud_SBar
	},

	{
		24, 24, 
		0, -24, DZ_BOTTOMLEFT,
		1,
		Hud_ArmourPic
	},
	{
		24*3, 24, 
		24, -24, DZ_BOTTOMLEFT,
		1,
		Hud_ArmourBig
	},

	{
		24, 24, 
		112, -24, DZ_BOTTOMLEFT,
		1,
		Hud_HealthPic
	},
	{
		24*3, 24, 
		24*6, -24, DZ_BOTTOMLEFT,
		1,
		Hud_HealthBig
	},

	{
		24*3, 24, 
		224, -24, DZ_BOTTOMLEFT,
		1,
		Hud_CurrentAmmoPic
	},
	{
		24*3, 24, 
		248, -24, DZ_BOTTOMLEFT,
		1,
		Hud_CurrentAmmoBig
	},

	{
		320, 24, 
		0, -48, DZ_BOTTOMLEFT,
		0.3f,
		Hud_IBar
	},

	{
		24, 16, 
		0, -40, DZ_BOTTOMLEFT,
		1,
		Hud_W_Shotgun
	},
	{
		24, 16, 
		24, -40, DZ_BOTTOMLEFT,
		1,
		Hud_W_SuperShotgun
	},
	{
		24, 16, 
		48, -40, DZ_BOTTOMLEFT,
		1,
		Hud_W_Nailgun
	},
	{
		24, 16, 
		72, -40, DZ_BOTTOMLEFT,
		1,
		Hud_W_SuperNailgun
	},
	{
		24, 16, 
		96, -40, DZ_BOTTOMLEFT,
		1,
		Hud_W_GrenadeLauncher
	},
	{
		24, 16, 
		120, -40, DZ_BOTTOMLEFT,
		1,
		Hud_W_RocketLauncher
	},
	{
		24, 16, 
		146, -40, DZ_BOTTOMLEFT,
		1,
		Hud_W_Lightning
	},
	{
		24, 16, 
		194, -40, DZ_BOTTOMLEFT,
		0.3f,
		Hud_Key1
	},
	{
		24, 16, 
		208, -40, DZ_BOTTOMLEFT,
		0.3f,
		Hud_Key2
	},
	{
		24, 16, 
		224, -40, DZ_BOTTOMLEFT,
		1,
		Hud_PUPInvis
	},
	{
		24, 16, 
		240, -40, DZ_BOTTOMLEFT,
		1,
		Hud_PUPInvuln
	},
	{
		24, 16, 
		256, -40, DZ_BOTTOMLEFT,
		1,
		Hud_PUPSuit
	},
	{
		24, 16, 
		272, -40, DZ_BOTTOMLEFT,
		1,
		Hud_PUPQuad
	},
	{
		24, 16, 
		288, -40, DZ_BOTTOMLEFT,
		0.3f,
		Hud_Rune1
	},
		{
		24, 16, 
		296, -40, DZ_BOTTOMLEFT,
		0.3f,
		Hud_Rune2
	},
	{
		24, 16, 
		304, -40, DZ_BOTTOMLEFT,
		0.3f,
		Hud_Rune3
	},
	{
		24, 16, 
		312, -40, DZ_BOTTOMLEFT,
		0.3f,
		Hud_Rune4
	},

	{
		42, 11, 
		0, -48, DZ_BOTTOMLEFT,
		1,
		Hud_Shells
	},
	{
		42, 11, 
		42, -48, DZ_BOTTOMLEFT,
		1,
		Hud_Nails
	},
	{
		42, 11, 
		42*2, -48, DZ_BOTTOMLEFT,
		1,
		Hud_Rockets
	},
	{
		42, 11, 
		42*3, -48, DZ_BOTTOMLEFT,
		1,
		Hud_Cells
	}
};
typedef struct {
	int type;

	float x, y;
	float scalex;
	float scaley;
	float alpha;
} hudelement_t;
hudelement_t element[MAX_ELEMENTS];	//look - Spike used a constant - that's a turn up for the books!
int numelements;

void UI_DrawPic(qhandle_t pic, int x, int y, int width, int height)
{
	Draw_Image((float)x*sbarscalex+sbarminx, (float)y*sbarscaley+sbarminy, (float)width*sbarscalex, (float)height*sbarscaley, 0, 0, 1, 1, pic);
}
void UI_DrawChar(unsigned int c, int x, int y)
{
static float size = 1.0f/16.0f;
	float s1 = size * (c&15);
	float t1 = size * (c>>4);
	Draw_Image((float)x*sbarscalex+sbarminx, (float)y*sbarscaley+sbarminy, 8*sbarscalex, 8*sbarscaley, s1, t1, s1+size, t1+size, con_chars);
}

void UI_DrawBigNumber(int num, int x, int y, qboolean red)
{
	char *s;
	int len;
	s = va("%i", num);


	len = strlen(s);
	if (len < 3)
		x += 24*(3-len);
	else
		s += len-3;

	if (red)
	{
		while(*s)
		{
			if (*s == '-')
				UI_DrawPic (pic_anum[10], x, y, 24, 24);
			else
				UI_DrawPic (pic_anum[*s-'0'], x, y, 24, 24);
			s++;
			x+=24;
		}
	}
	else
	{
		while(*s)
		{
			if (*s == '-')
				UI_DrawPic (pic_num[10], x, y, 24, 24);
			else
				UI_DrawPic (pic_num[*s-'0'], x, y, 24, 24);
			s++;
			x+=24;
		}
	}
}

void SBar_FlushAll(void)
{
	numelements = 0;
}

void SBar_ReloadDefaults(void)
{
	int i;
	for (i = 0; i < sizeof(hetype)/sizeof(hetype[0]); i++)
	{
		if (hetype[i].defaultalpha)
		{
			if (numelements >= MAX_ELEMENTS)
				break;
			element[numelements].type = i;
			element[numelements].alpha = hetype[i].defaultalpha;
			element[numelements].scalex = 1;
			element[numelements].scaley = 1;
			switch(hetype[i].defaultzone)
			{
			case DZ_BOTTOMLEFT:
				element[numelements].x = hetype[i].defaultx;
				element[numelements].y = 480+hetype[i].defaulty;
				break;
			case DZ_BOTTOMRIGHT:
				element[numelements].x = 640+hetype[i].defaultx;
				element[numelements].y = 480+hetype[i].defaulty;
				break;
			}
			numelements++;
		}
	}
}

void UI_SbarInit(void)
{
	int i;
	int j;

//main bar (add cvars later)
	ibarback = Draw_LoadImage("ibar", true);
	sbarback = Draw_LoadImage("sbar", true);

	con_chars = Draw_LoadImage("conchars", true);

//load images.
	for (i = 0; i < 10; i++)
	{
		pic_num[i] = Draw_LoadImage(va("num_%i", i), true);
		pic_anum[i] = Draw_LoadImage(va("anum_%i", i), true);
	}
	pic_num[10] = Draw_LoadImage("num_minus", true);
	pic_anum[10] = Draw_LoadImage("anum_minus", true);
	pic_num[11] = Draw_LoadImage("num_colon", true);
	pic_num[12] = Draw_LoadImage("num_slash", true);

	for (i = 0; i < numweaps; i++)
	{
		gotweapontime[i] = 0;
		pic_weapon[0][i] = Draw_LoadImage(va("inv_%s", weaponabbreviation[i]), true);
		pic_weapon[1][i] = Draw_LoadImage(va("inv2_%s", weaponabbreviation[i]), true);
		for (j = 0; j < 5; j++)
		{
			pic_weapon[2+j][i] = Draw_LoadImage(va("inva%i_%s", j+1, weaponabbreviation[i]), true);
		}
	}
	for (i = 0; i < numpups; i++)
	{
		gotpuptime[i] = 0;
		pic_pup[0][i] = Draw_LoadImage(va("sb_%s", pupabbr2[i]), true);
		for (j = 0; j < 5; j++)
		{
			pic_pup[1+j][i] = Draw_LoadImage(va("sba%i_%s", j+1, pupabbr[i]), true);
		}
	}
	pic_armour[0] = Draw_LoadImage("sb_armor1", true);
	pic_armour[1] = Draw_LoadImage("sb_armor2", true);
	pic_armour[2] = Draw_LoadImage("sb_armor3", true);

	pic_ammo[0] = Draw_LoadImage("sb_shells", true);
	pic_ammo[1] = Draw_LoadImage("sb_nails", true);
	pic_ammo[2] = Draw_LoadImage("sb_rocket", true);
	pic_ammo[3] = Draw_LoadImage("sb_cells", true);

	pic_rune[0] = Draw_LoadImage("sb_sigil1", true);
	pic_rune[1] = Draw_LoadImage("sb_sigil2", true);
	pic_rune[2] = Draw_LoadImage("sb_sigil3", true);
	pic_rune[3] = Draw_LoadImage("sb_sigil4", true);

	pic_face[0] = Draw_LoadImage("face1", true);
	pic_face[1] = Draw_LoadImage("face2", true);
	pic_face[2] = Draw_LoadImage("face3", true);
	pic_face[3] = Draw_LoadImage("face4", true);
	pic_face[4] = Draw_LoadImage("face5", true);

	pic_facep[0] = Draw_LoadImage("face_p1", true);
	pic_facep[1] = Draw_LoadImage("face_p2", true);
	pic_facep[2] = Draw_LoadImage("face_p3", true);
	pic_facep[3] = Draw_LoadImage("face_p4", true);
	pic_facep[4] = Draw_LoadImage("face_p5", true);

	pic_facequad = Draw_LoadImage("face_quad", true);
	pic_faceinvis = Draw_LoadImage("face_invis", true);
	pic_faceinvisinvuln = Draw_LoadImage("face_inv2", true);
	pic_faceinvuln = Draw_LoadImage("face_invul1", true);
	pic_faceinvulnquad = Draw_LoadImage("face_invul2", true);

	SBar_FlushAll();
	SBar_ReloadDefaults();
}

unsigned int stats[MAX_CL_STATS];

void Hud_SBar(void)
{
	UI_DrawPic(sbarback, 0, 0, 320, 24);
}
void Hud_ArmourPic(void)
{
	if (stats[STAT_ITEMS] & IT_ARMOR3)
		UI_DrawPic(pic_armour[2], 0, 0, 24, 24);
	else if (stats[STAT_ITEMS] & IT_ARMOR2)
		UI_DrawPic(pic_armour[1], 0, 0, 24, 24);
	else if (stats[STAT_ITEMS] & IT_ARMOR1)
		UI_DrawPic(pic_armour[0], 0, 0, 24, 24);
}
void Hud_ArmourBig(void)
{
	int i = stats[STAT_ARMOR];
	UI_DrawBigNumber(i, 0, 0, i < 25);
}
void Hud_HealthPic(void)
{
	int hl;

	if (stats[STAT_ITEMS] & IT_PUP3)
	{	//invisability
		if (stats[STAT_ITEMS] & IT_PUP4)
			UI_DrawPic(pic_faceinvisinvuln, 0, 0, 24, 24);
		else
			UI_DrawPic(pic_faceinvis, 0, 0, 24, 24);
		return;
	}

	if (stats[STAT_ITEMS] & IT_PUP4)
	{	//invuln
		if (stats[STAT_ITEMS] & IT_PUP6)
			UI_DrawPic(pic_faceinvulnquad, 0, 0, 24, 24);
		else
			UI_DrawPic(pic_faceinvuln, 0, 0, 24, 24);
		return;
	}
	if (stats[STAT_ITEMS] & IT_PUP6)
	{
		UI_DrawPic(pic_facequad, 0, 0, 24, 24);
		return;
	}

	hl = stats[STAT_HEALTH]/20; 
	if (hl > 4)
		hl = 4;
	if (hl < 0)
		hl = 0;
//	if (innpain)
//		UI_DrawPic(pic_facep[4-hl], 0, 0, 24, 24);
//	else
		UI_DrawPic(pic_face[4-hl], 0, 0, 24, 24);
}
void Hud_HealthBig(void)
{
	int i = stats[STAT_HEALTH];
	UI_DrawBigNumber(i, 0, 0, i < 25);
}

void Hud_CurrentAmmoPic(void)
{
		 if (stats[STAT_ITEMS] & IT_AMMO1)
		UI_DrawPic(pic_ammo[0], 0, 0, 24, 24);
	else if (stats[STAT_ITEMS] & IT_AMMO2)
		UI_DrawPic(pic_ammo[1], 0, 0, 24, 24);
	else if (stats[STAT_ITEMS] & IT_AMMO3)
		UI_DrawPic(pic_ammo[2], 0, 0, 24, 24);
	else if (stats[STAT_ITEMS] & IT_AMMO4)
		UI_DrawPic(pic_ammo[3], 0, 0, 24, 24);
}
void Hud_CurrentAmmoBig(void)
{
	int i = stats[STAT_AMMO];
	UI_DrawBigNumber(i, 0, 0, i < 25);
}
void Hud_IBar(void)
{
	UI_DrawPic(ibarback, 0, 0, 320, 24);
}

void Hud_Weapon(int wnum)
{
	int flash;
	if (!(stats[STAT_ITEMS] & (IT_GUN1 << wnum)) && !hudedit)
		return;
	if (!gotweapontime[wnum])
		gotweapontime[wnum] = currenttime;
	flash = (currenttime - gotweapontime[wnum])/100;
	if (flash < 0)	//errr... whoops...
		flash = 0;
	
	if (flash > 10)
	{
		if (stats[STAT_WEAPON] & (IT_GUN1 << wnum))
			flash = 1;	//selected.
		else
			flash = 0;
	}
	else
		flash = (flash%5) + 2;

	UI_DrawPic(pic_weapon[flash][wnum], 0, 0, 24, 16);
}

void Hud_W_Shotgun(void)
{
	Hud_Weapon(0);
}
void Hud_W_SuperShotgun(void)
{
	Hud_Weapon(1);
}
void Hud_W_Nailgun(void)
{
	Hud_Weapon(2);
}
void Hud_W_SuperNailgun(void)
{
	Hud_Weapon(3);
}
void Hud_W_GrenadeLauncher(void)
{
	Hud_Weapon(4);
}
void Hud_W_RocketLauncher(void)
{
	Hud_Weapon(5);
}
void Hud_W_HalfLightning(void)	//left half only (needed due to LG icon being twice as wide)
{
	int flash;
	int wnum = 6;

	if (!(stats[STAT_ITEMS] & (IT_GUN1 << wnum)) && !hudedit)
		return;

	if (!gotweapontime[wnum])
		gotweapontime[wnum] = currenttime;
	flash = (currenttime - gotweapontime[wnum])/100;
	if (flash < 0)	//errr... whoops...
		flash = 0;
	
	if (flash > 10)
	{
		if (stats[STAT_WEAPON] & (IT_GUN1 << wnum))
			flash = 1;	//selected.
		else
			flash = 0;
	}
	else
		flash = (flash%5) + 2;

	Draw_Image(sbarminx, sbarminy, (float)24*sbarscalex, (float)16*sbarscaley, 0, 0, 0.5, 1, pic_weapon[flash][wnum]);
}
void Hud_W_Lightning(void)
{
	int flash;
	int wnum = 6;

	if (!(stats[STAT_ITEMS] & (IT_GUN1 << wnum)) && !hudedit)
		return;

	if (!gotweapontime[wnum])
		gotweapontime[wnum] = currenttime;
	flash = (currenttime - gotweapontime[wnum])/100;
	if (flash < 0)	//errr... whoops...
		flash = 0;
	
	if (flash > 10)
	{
		if (stats[STAT_WEAPON] & (IT_GUN1 << wnum))
			flash = 1;	//selected.
		else
			flash = 0;
	}
	else
		flash = (flash%5) + 2;

	UI_DrawPic(pic_weapon[flash][wnum], 0, 0, 48, 16);
}

void Hud_Powerup(int wnum)
{
	int flash;
	if (!(stats[STAT_ITEMS] & (IT_PUP1 << wnum)) && !hudedit)
		return;

	if (!gotpuptime[wnum])
		gotpuptime[wnum] = currenttime;
	flash = (currenttime - gotpuptime[wnum])/100;
	if (flash < 0)	//errr... whoops...
		flash = 0;
	
	if (flash > 10)
	{
		flash = 0;
	}
	else
		flash = (flash%5) + 2;

	UI_DrawPic(pic_pup[flash][wnum], 0, 0, 16, 16);
}

void Hud_Key1(void)
{
	Hud_Powerup(0);
}
void Hud_Key2(void)
{
	Hud_Powerup(1);
}
void Hud_PUPInvis(void)
{
	Hud_Powerup(2);
}
void Hud_PUPInvuln(void)
{
	Hud_Powerup(3);
}
void Hud_PUPSuit(void)
{
	Hud_Powerup(4);
}
void Hud_PUPQuad(void)
{
	Hud_Powerup(5);
}

void Hud_Rune1(void)
{
	if (!(stats[STAT_ITEMS] & (IT_RUNE1 << 0)) && !hudedit)
		return;
	UI_DrawPic(pic_rune[0], 0, 0, 8, 16);
}
void Hud_Rune2(void)
{
	if (!(stats[STAT_ITEMS] & (IT_RUNE1 << 1)) && !hudedit)
		return;
	UI_DrawPic(pic_rune[1], 0, 0, 8, 16);
}
void Hud_Rune3(void)
{
	if (!(stats[STAT_ITEMS] & (IT_RUNE1 << 2)) && !hudedit)
		return;
	UI_DrawPic(pic_rune[2], 0, 0, 8, 16);
}
void Hud_Rune4(void)
{
	if (!(stats[STAT_ITEMS] & (IT_RUNE1 << 3)) && !hudedit)
		return;
	UI_DrawPic(pic_rune[3], 0, 0, 8, 16);
}

void Hud_Ammo(int type)
{
	int num;
	Draw_Image(sbarminx, sbarminy, (float)42*sbarscalex, (float)11*sbarscaley, (3+(type*48))/320.0f, 0, (3+(type*48)+42)/320.0f, 11/24.0f, ibarback);

	num = stats[STAT_SHELLS+type];
	UI_DrawChar(num%10+18, 19, 0);
	num/=10;
	if (num%10)
		UI_DrawChar(num%10+18, 11, 0);
	num/=10;
	if (num%10)
		UI_DrawChar(num%10+18, 3, 0);
}
void Hud_Shells(void)
{
	Hud_Ammo(0);
}
void Hud_Nails(void)
{
	Hud_Ammo(1);
}
void Hud_Rockets(void)
{
	Hud_Ammo(2);
}
void Hud_Cells(void)
{
	Hud_Ammo(3);
}

//draw cody of sbar
//arg[0] is playernum
//arg[1]/arg[2] is x/y start of subwindow
//arg[3]/arg[4] is width/height of subwindow
int UI_StatusBar(int *arg)
{
//	int flash;
	int i;
//	int x;
//	char *s;
//	unsigned int items;
//	unsigned int weapon;
//	int mx, my;

//	qboolean noflash = Cvar_GetFloat(UI_NOFLASH);

	float vsx, vsy;

	CL_GetStats(arg[0], stats, sizeof(stats)/sizeof(int));

	vsx = arg[3]/640.0f;
	vsy = arg[4]/480.0f;
	for (i = 0; i < numelements; i++)
	{
		sbarminx = arg[1] + element[i].x*vsx;
		sbarminy = arg[2] + element[i].y*vsy;
		sbarscalex  = element[i].scalex*vsx;
		sbarscaley  = element[i].scaley*vsy;
		hetype[element[i].type].DrawElement();
	}
/*
	items = stats[STAT_ITEMS];
	weapon = stats[STAT_WEAPON];

	//background of sbar
	UI_DrawPic(sbarback, 0, vid.height-24, 320, 24);

	//armour quant
	i = stats[STAT_ARMOR];
	UI_DrawBigNumber(i, 24, vid.height-24, i < 25);

	//armour pic
	if (items & IT_ARMOR3)
		UI_DrawPic(pic_armour[2], 0, vid.height-24, 24, 24);
	else if (items & IT_ARMOR2)
		UI_DrawPic(pic_armour[1], 0, vid.height-24, 24, 24);
	else if (items & IT_ARMOR1)
		UI_DrawPic(pic_armour[0], 0, vid.height-24, 24, 24);

	//health quant
	i = stats[STAT_HEALTH];
	UI_DrawBigNumber(i, 24*6, vid.height-24, i < 25);

	//faces
//FIXME: implement

	if (Cvar_GetFloat(UI_NOIBAR))
		return true;

	//back of ibar
	UI_DrawPic(ibarback, 0, vid.height-24-24, 320, 24);

	//weapons
	for (i = 0; i < numweaps; i++)
	{
		if (items & (IT_GUN1 << i))
		{
			if (!gotweapontime[i])
				gotweapontime[i] = time;
			flash = (int)((time - gotweapontime[i])*10);
			if (flash < 0)	//errr... whoops...
				flash = 0;
			
			if (flash > 10 || noflash)
			{
				if (weapon & (IT_GUN1 << i))
					flash = 1;	//selected.
				else
					flash = 0;
			}
			else
				flash = (flash%5) + 2;

			if (i == 6)
				UI_DrawPic(pic_weapon[flash][i], 24*i, vid.height-16-24, 48, 16);
			else
				UI_DrawPic(pic_weapon[flash][i], 24*i, vid.height-16-24, 24, 16);
		}
		else
			gotweapontime[i] = 0;
	}

	//currentammo
//FIXME: implement

	//powerups
	for (i = 0; i < numpups; i++)
	{
		if (items & (IT_PUP1 << i))
		{
			if (!gotpuptime[i])
				gotpuptime[i] = time;
			flash = (int)((time - gotpuptime[i])*10);
			if (flash < 0)	//errr... whoops...
				flash = 0;
			
			if (flash > 10 || noflash)
			{
				flash = 0;
			}
			else
				flash = (flash%5) + 1;

			UI_DrawPic(pic_pup[flash][i], (24*8)+(16*i), vid.height-16-24, 16, 16);
		}
		else
			gotpuptime[i] = 0;
	}

	//runes
//FIXME: implement

	//ammo counts
	for (i = 0; i < 4; i++)
	{
		s = va("%i", stats[STAT_SHELLS+i]);

		x = (6*i+1)*8;

		flash = strlen(s);
		if (flash < 3)
			x += 8*(3-flash);
		else
			s += flash-3;


		while(*s)
		{
			UI_DrawChar((unsigned)*s + 18 - '0', x, vid.height-24-24);
			s++;
			x+=8;
		}
	}

	//small 4player scorecard
//FIXME: implement
*/
	return true;
}

int currentitem;
qboolean mousedown, shiftdown;
float mouseofsx, mouseofsy;

void UI_KeyPress(int key, int mx, int my)
{
	int i;
	if (key == K_ESCAPE)
	{
		Menu_Control(0);
		return;
	}

	if (key == K_MOUSE1)
	{	//figure out which one our cursor is over...
		mousedown = false;

		for (i = 0; i < numelements; i++)
		{
			if (element[i].x < mx &&
				element[i].y < my &&
				element[i].x + element[i].scalex*hetype[element[i].type].width > mx &&
				element[i].y + element[i].scaley*hetype[element[i].type].height > my)
			{
				mouseofsx = mx - element[i].x;
				mouseofsy = my - element[i].y;
				mousedown = true;
				currentitem = i;
				break;
			}
		}
		return;
	}

	if (key == 'i')
	{
		if (numelements==MAX_ELEMENTS)
			return;	//too many

		element[numelements].scalex = 1;
		element[numelements].scaley = 1;
		element[numelements].alpha = 1;
		numelements++;
	}
	else if (currentitem < numelements)
	{
		if (key == K_SHIFT)
			shiftdown = true;
		else if (key == 'd')
		{
			mousedown = false;
			memcpy(element+currentitem, element+currentitem+1, sizeof(element[0]) * (numelements - currentitem-1));
			numelements--;
			currentitem = 0;
		}
		else if (key == 'q')
		{
			element[currentitem].type--;
			if (element[currentitem].type < 0)
				element[currentitem].type = sizeof(hetype)/sizeof(hetype[0])-1;
		}
		else if (key == 'w')
		{
			element[currentitem].type++;
			if (element[currentitem].type >= sizeof(hetype)/sizeof(hetype[0]))
				element[currentitem].type = 0;
		}
		else if (key == K_UPARROW)
		{
			element[currentitem].y-=shiftdown?8:1;
		}
		else if (key == K_DOWNARROW)
		{
			element[currentitem].y+=shiftdown?8:1;
		}
		else if (key == K_LEFTARROW)
		{
			element[currentitem].x-=shiftdown?8:1;
		}
		else if (key == K_RIGHTARROW)
		{
			element[currentitem].x+=shiftdown?8:1;
		}
		else if (key == K_HOME)
		{
			element[currentitem].scalex=1.0f;
			element[currentitem].scaley=1.0f;
		}
		else if (key == '+')
		{
			element[currentitem].scalex*=1.1f;
			element[currentitem].scaley*=1.1f;
		}
		else if (key == '-')
		{
			element[currentitem].scalex/=1.1f;
			element[currentitem].scaley/=1.1f;
		}
	}
}

int Plug_MenuEvent(int *args)
{
	int altargs[5];

	args[2]=(int)(args[2]*640.0f/vid.width);
	args[3]=(int)(args[3]*480.0f/vid.height);

	switch(args[0])
	{
	case 0:	//draw

		if (mousedown)
		{
			element[currentitem].x = args[2] - mouseofsx;
			element[currentitem].y = args[3] - mouseofsy;
			if (shiftdown)
			{
				element[currentitem].x -= (int)element[currentitem].x & 7;
				element[currentitem].y -= (int)element[currentitem].y & 7;
			}
		}

		hudedit = !!(currenttime/250)&3;

		altargs[0] = 0;
		altargs[1] = 0;
		altargs[2] = 0;
		altargs[3] = vid.width;
		altargs[4] = vid.height;
		UI_StatusBar(altargs);	//draw it using the same function (we're lazy)

		if ((currenttime/250)&1)
			Draw_Fill(	(int)element[currentitem].x, (int)element[currentitem].y,
						(int)(element[currentitem].scalex*hetype[element[currentitem].type].width), 
						(int)(element[currentitem].scaley*hetype[element[currentitem].type].height));
		break;
	case 1:	//keydown
		UI_KeyPress(args[1], args[2], args[3]);
		break;
	case 2:	//keyup
		if (args[1] == K_MOUSE1)
			mousedown = false;
		else if (args[1] == K_SHIFT)
			shiftdown = false;
		break;
	case 3:	//menu closed (this is called even if we change it).
		hudedit = false;
		break;
	case 4:	//mousemove
		break;
	}

	return 0;
}

int Plug_Tick(int *args)
{
	currenttime = args[0];
	return true;
}

int Plug_ExecuteCommand(int *args)
{
	char cmd[256];
	Cmd_Argv(0, cmd, sizeof(cmd));
	if (!strcmp("sbar_edit", cmd))
	{
		Menu_Control(1);
		mousedown=false;
		return 1;
	}
	return 0;
}

int Plug_Init(int *args)
{
	if (Plug_Export("Tick", Plug_Tick) &&
		Plug_Export("SbarBase", UI_StatusBar) &&
		Plug_Export("ExecuteCommand", Plug_ExecuteCommand) &&
		Plug_Export("MenuEvent", Plug_MenuEvent))
	{
		UI_SbarInit();

		K_UPARROW		= Key_GetKeyCode("uparrow");
		K_DOWNARROW		= Key_GetKeyCode("downarrow");
		K_LEFTARROW		= Key_GetKeyCode("leftarrow");
		K_RIGHTARROW	= Key_GetKeyCode("rightarrow");
		K_ESCAPE		= Key_GetKeyCode("escape");
		K_HOME			= Key_GetKeyCode("home");
		K_MOUSE1		= Key_GetKeyCode("mouse1");
		K_SHIFT			= Key_GetKeyCode("shift");

		return true;
	}
	return false;
}