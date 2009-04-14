float chasecam;	//chasecam active
entity player_local;	//handle to the local player entity
float isdp;	//if we're running under DP
string levelname;

//q3playerm
.entity headent;
.entity torsoent;
.entity legsent;
.entity weaponent;
.float modelnum;
.float animnum;
.float framechangetime;

//player
.float lerptime;
.void() removefunc;
.vector lastorg;
.vector lastvel;
.float haddied;
.float modelstyle;
.string oldskin;

//the genders you can have.
//we're generous with three.
enum {
	GENDER_NEUTER,
	GENDER_MALE,
	GENDER_FEMALE
};

//
.float ideal_yaw;
.float yaw_speed;
.float tag_index;


.float sveffects;	//lower 4 is weapon, next is nopred, upper 3 is powerups
#define SVE_INVIS 128
#define SVE_GOD 64
#define SVE_QUAD 32
#define SVE_NOPRED 16
#define SVE_WEAPONSMASK 15

.float removetime;
.float gibbable;

//q3playerm
float(string newmodelskin) Anim_SetModel;
void() Anim_Draw;
void() Anim_UnsetModel;
entity() Anim_DupModel;
float(string fname) Anim_ReadAnimationFile;
float() Anim_GetGender;
float(string skinname) Anim_GetHeadModelIndex;
float(string skinname) Anim_GetHeadSkinNumber;
vector(string skinname) Anim_GetHeadOffset;

//prediction
void() Pred_PlayerUpdated;
void() Pred_UpdateLocalMovement;
vector vieworg;

//menu
void(void(float, float, float, float) fnc) Menu_Activate;
void() Menu_Main;
void() Menu_Think;
var void(float event, float button, float mousex, float mousey) MenuEventFunc;

void() Movetype_Bounce;