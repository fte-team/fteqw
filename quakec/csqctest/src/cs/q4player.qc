float(float modlindex) skel_create = #263; // create a skeleton (be sure to assign the value to .skeletonindex for use), returns skeleton index (1 or higher) on success, returns 0 on failure  (for example if the modelindex is not skeletal)
float(float skel, entity ent, float modlindex, float retainfrac, float firstbone, float lastbone) skel_build = #264; // blend in a percentage of standard animation, 0 replaces entirely, 1 does nothing, 0.5 blends half, etc, and this only alters the bones in the specified range for which out of bounds values like 0,100000 are safe (uses .frame, .frame2, .frame3, .frame4, .lerpfrac, .lerpfrac3, .lerpfrac4, .frame1time, .frame2time, .frame3time, .frame4time), returns skel on success, 0 on failure
float(float skel) skel_get_numbones = #265; // returns how many bones exist in the created skeleton
string(float skel, float bonenum) skel_get_bonename = #266; // returns name of bone (as a tempstring)
float(float skel, float bonenum) skel_get_boneparent = #267; // returns parent num for supplied bonenum, -1 if bonenum has no parent or bone does not exist (returned value is always less than bonenum, you can loop on this)
float(float skel, string tagname) skel_find_bone = #268; // get number of bone with specified name, 0 on failure, tagindex (bonenum+1) on success, same as using gettagindex on the modelindex
vector(float skel, float bonenum) skel_get_bonerel = #269; // get matrix of bone in skeleton relative to its parent - sets v_forward, v_right, v_up, returns origin (relative to parent bone)
vector(float skel, float bonenum) skel_get_boneabs = #270; // get matrix of bone in skeleton in model space - sets v_forward, v_right, v_up, returns origin (relative to entity)
void(float skel, float bonenum, vector org, vector fwd, vector rgt, vector up) skel_set_bone = #271; // set matrix of bone relative to its parent, reads v_forward, v_right, v_up, takes origin as parameter (relative to parent bone)
void(float skel, float bonenum, vector org) skel_mul_bone = #272; // transform bone matrix (relative to its parent) by the supplied matrix in v_forward, v_right, v_up, takes origin as parameter (relative to parent bone)
void(float skel, float startbone, float endbone, vector org, vector f, vector r, vector u) skel_mul_bones = #273; // transform bone matrices (relative to their parents) by the supplied matrix in v_forward, v_right, v_up, takes origin as parameter (relative to parent bones)
void(float skeldst, float skelsrc, float startbone, float endbone) skel_copybones = #274; // copy bone matrices (relative to their parents) from one skeleton to another, useful for copying a skeleton to a corpse
void(float skel) skel_delete = #275; // deletes skeleton at the beginning of the next frame (you can add the entity, delete the skeleton, renderscene, and it will still work)
//float(float modlindex, string framename) frameforname = #276; // finds number of a specified frame in the animation, returns -1 if no match found
float(float modlindex, float framenum) frameduration = #277; // returns the intended play time (in seconds) of the specified framegroup, if it does not exist the result is 0, if it is a single frame it may be a small value around 0.1 or 0.
.float skeletonindex;


enum
{
ANIM_FORWARD,
ANIM_BACK,
ANIM_LEFT,
ANIM_RIGHT,
ANIM_IDLE,
ANIM_MAX
};

float anim[ANIM_MAX];

.vector lastorg;

.float aweight[ANIM_MAX];
.float atime[ANIM_MAX];

.float rangle;

/*pick an angle for the legs to run in such that the movement is in 0, 90, 180, -90 angles from movement direction. attempt to clamp past a full circle*/
float() anglequadrant =
{
	float ideal, diff;

	if (!self.velocity_x && !self.velocity_y)
		ideal = self.angles_y;
	else
		ideal = vectoyaw(self.velocity);
	diff = ideal - self.angles_y;
	if (diff > 180)
		diff -= 360;
	if (diff < -180)
		diff += 360;

	if (diff < -180)
		return self.angles_y + 180;
	else if (diff > 180)
		return self.angles_y - 180;
	else if (diff < -150)
		return ideal + 180;
	else if (diff > 150)
		return ideal - 180;
	else if (diff > 75)
		return ideal - 90;
	else if (diff < -75)
		return ideal + 90;
	else
		return ideal;
};

void(float ideal, float speed) mychangeyaw =
{
	float move;
	move = ideal - self.angles_y;
	speed *= frametime;
	if (move > 180)
		move -= 360;
	if (move < -180)
		move += 360;
	if (move >= 0)
	{
		if (move > speed)
			move = speed;
	}
	else
	{
		if (move < -speed)
			move = -speed;
	}
	self.angles_y = self.angles_y + move;
};

void() Q4PM_Draw =
{
	float fw, rt;
	float tweight;
	float i;
	float ideal;
	vector aimang;

	vector move = self.origin - self.lastorg;
	self.lastorg = self.origin;

	if (!self.skeletonindex)
	{
		self.skeletonindex = skel_create(self.modelindex);
		if (!self.skeletonindex)
			return;
	}

	ideal = anglequadrant();
	aimang_x = self.angles_y;
	aimang_z = self.angles_x*-3;

	self.angles_x = 0;
	self.angles_y = self.rangle;
	self.angles_z = 0;
	mychangeyaw(ideal, 360);
	self.rangle = self.angles_y;

	makevectors(self.angles);
	
	fw = (move * v_forward)/(320);
	rt = (move * v_right)/(320);

	aimang_x -= self.angles_y;
	aimang_y = 0;
	if (aimang_x > 180)
		aimang_x -= 360;
	if (aimang_x < -180)
		aimang_x += 360;
	makevectors(aimang*0.333);

	if (fw*fw > 0)
	{
		if (fw > 0)
			self.aweight[ANIM_FORWARD] += fw*32;
		else
			self.aweight[ANIM_BACK] -= fw*32;
	}
	if (fw > 0)
		self.atime[ANIM_FORWARD] += fw;
	else
		self.atime[ANIM_BACK] -= fw;
	if (rt*rt > 0)
	{
		if (rt > 0)
			self.aweight[ANIM_RIGHT] += rt*32;
		else
			self.aweight[ANIM_LEFT] -= rt*32;
	}
	if (rt > 0)
		self.atime[ANIM_RIGHT] += rt;
	else
		self.atime[ANIM_LEFT] -= rt;
	if (move_x == 0 && move_y == 0)
		self.aweight[ANIM_IDLE] += frametime*4;
	self.atime[ANIM_IDLE] += frametime;
	

	tweight = self.aweight[ANIM_FORWARD]+self.aweight[ANIM_BACK]+self.aweight[ANIM_LEFT]+self.aweight[ANIM_RIGHT]+self.aweight[ANIM_IDLE];
	if (tweight > 1)
	{
		tweight = 1/tweight;
		self.aweight[ANIM_FORWARD] *= tweight;
		self.aweight[ANIM_BACK] *= tweight;
		self.aweight[ANIM_LEFT] *= tweight;
		self.aweight[ANIM_RIGHT] *= tweight;
		self.aweight[ANIM_IDLE] *= tweight;
	}
	else
	{
		self.aweight[ANIM_IDLE] += 1-tweight;
		self.atime[ANIM_IDLE] += frametime;
	}

	self.origin_z += self.mins_z;
	setmodel(self, self.model);
	for (i = 0; i < ANIM_MAX; i++)
	{
		self.frame = 0;
		self.frame2 = 0;
		self.lerpfrac = 0;
		self.frame1time = self.atime[i];
		self.frame2time = self.atime[i];
		skel_build(self.skeletonindex, self, anim[i], ((i==0)?0:1), 0, -1, self.aweight[i]);
	}

	skel_mul_bone(self.skeletonindex, 18, '0 0 0', v_forward, v_right, v_up);
	skel_mul_bone(self.skeletonindex, 19, '0 0 0', v_forward, v_right, v_up);
	skel_mul_bone(self.skeletonindex, 20, '0 0 0', v_forward, v_right, v_up);

	//obliterate the bone movement, so that it doesn't move forwards.
	skel_get_bonerel(self.skeletonindex, 1);
	skel_set_bone(self.skeletonindex, 1, '0 0 0', v_forward*(64/72), v_right*(64/72), v_up*(64/72));
};

float(string skinname) Q4PM_SetModel =
{
	string mname = strcat("models/characters/player/", skinname, ".md5mesh");

	//check to see if it exists
	if not (whichpack(mname))
		return false;

	anim[ANIM_IDLE]    = precache_model("models/characters/player/idle.md5anim");
	anim[ANIM_FORWARD] = precache_model("models/characters/player/run.md5anim");
	anim[ANIM_BACK]    = precache_model("models/characters/player/run_backwards.md5anim");
	anim[ANIM_RIGHT]   = precache_model("models/characters/player/strafe_right.md5anim");
	anim[ANIM_LEFT]    = precache_model("models/characters/player/strafe_left.md5anim");

	setmodel(self, mname);

	self.rangle = self.angles_y;

	return true;
};

void() Q4PM_UnsetModel =
{
	if (self.skeletonindex)
		skel_delete(self.skeletonindex);
	self.skeletonindex = 0;
};

entity() Q4PM_DupModel
{
	return spawn();
};