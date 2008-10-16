float chasecam;
entity player_local;

.entity headent;
.entity torsoent;
.entity legsent;
.entity weaponent;
.float modelnum;
.float animnum;
.float framechangetime;
.float lerptime;

.float sveffects;	//lower 4 is weapon, next is nopred, upper 3 is powerups
#define SVE_INVIS 128
#define SVE_GOD 64
#define SVE_QUAD 32
#define SVE_NOPRED 16
#define SVE_WEAPONSMASK 15

.float gibbable;