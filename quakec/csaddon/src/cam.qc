typedef struct
{
	int numpoints;
	vector pos[64];
float totallength;
} spline_t;

typedef struct
{
	float starttime;
	float endtime;
	float startfov;
	float endfov;
	spline_t view;
	spline_t focus;
} cam_t;
typedef struct
{
	int numcams;
	int selcam;
	int ver;
	cam_t cam[64];
} camdata_t;
static camdata_t *camdata;
static var float splinefile = -1;

void() spline_init =
{
	/*precache the shader*/
	shaderforname("camsplineshader", "{\n{\nmap construction.tga\nblendfunc blend\nrgbgen vertex\nalphagen vertex\n}\n}\n");

	splinefile = fopen("spline.dat", FILE_MMAP_RW, sizeof(camdata_t));
	if (splinefile < 0)
	{
		/*too lazy to create a file, just use it as a malloc*/
		splinefile = fopen("", FILE_MMAP_RW, sizeof(camdata_t));
	}

	camdata = (camdata_t*)(fgets(splinefile));
};
void() spline_shutdown =
{
	fclose(splinefile);
	splinefile = -1;
};

static void spline_calclength(spline_t *s)
{
	int i;
	s->totallength = 0;
	for (i = 0; i < s->numpoints - 1; i++)
	{
		s->totallength = s->totallength + vlen(s->pos[i] - s->pos[i+1]);
	}
};

static vector(spline_t *s, float frac) spline_pos =
{
	vector v1, v2;
	frac *= s->numpoints;
	if (frac < 0)
	{
		v1 = s->pos[0];
		v2 = s->pos[1];
	}
	else if (frac >= s->numpoints)
	{
		v1 = s->pos[s->numpoints-2];
		v2 = s->pos[s->numpoints-1];
		frac -= s->numpoints;
	}
	else
	{
		v1 = s->pos[s->numpoints];
		v2 = s->pos[s->numpoints+1];
		frac = frac - (float)(int)frac;
	}

	return v1 * (1-frac) + v2 * frac;
};

static vector(spline_t *s, float frac) spline_dir =
{
	return normalize(spline_pos(s, frac+0.001) - spline_pos(s, frac-0.001));
};

static vector(vector pos, vector dir, vector view) beamdir =
{
	vector result;
	view = normalize(view - pos);
	//crossproduct view+dir
	result_x = dir_y*view_z - dir_z*view_y;
	result_y = dir_z*view_x - dir_x*view_z;
	result_z = dir_x*view_y - dir_y*view_x;
	return result;
};

static void(spline_t *s) spline_draw =
{
	local float step, frac;
	local vector pos, pos1, pos2, vpos;
	local vector bdir;

	/*example of drawing convex polygons*/
	R_BeginPolygon("camsplineshader");

	vpos = getviewprop(VF_ORIGIN);

	/*calc the initial beam positions*/
	pos = spline_pos(s, 0);
	bdir = beamdir(pos, spline_dir(s, 0), vpos)*8;
	pos1 = pos + bdir;
	pos2 = pos - bdir;
	
	for (frac = 0; frac < 1; )
	{
		frac += step;
		if (frac > 1)
			frac = 1;

		pos = spline_pos(s, frac);

		/*emit prior verts*/
		R_PolygonVertex(pos1, '0 0', '1 1 1', 1);
		R_PolygonVertex(pos2, '1 0', '1 1 1', 1);

		/*calc intersitial beam position*/
		pos2 = spline_dir(s, frac);
		bdir = beamdir(pos, spline_dir(s, frac), vpos)*8;
		pos1 = pos + bdir;
		pos2 = pos - bdir;

		/*emit current verts*/
		R_PolygonVertex(pos2, '1 1', '1 1 1', 1);
		R_PolygonVertex(pos1, '0 1', '1 1 1', 1);
		R_EndPolygon();
	}
};

/*called to move the view to some simulation time*/
void(float attime) spline_overrides =
{
	local cam_t *cam;
	local int i;
	local vector src, dst;
	local float frac;

	/*find correct spline based upon time*/
	for (i = 0; i < camdata->numcams; i++)
	{
		cam = &camdata->cam[i];
		if (cam->starttime <= attime && cam->endtime > attime)
			break;
	}
	/*give up if no matches*/
	if (i == camdata->numcams)
		return;

	frac = (attime - cam->starttime) / (cam->endtime - cam->starttime);
	src = spline_pos(&cam->view, frac);
	dst = spline_pos(&cam->focus, frac);

	setviewprop(VF_ORIGIN, src);
	setviewprop(VF_ANGLES, vectoangles(dst - src));
	setviewprop(VF_AFOV, (cam->startfov * (1-frac)) + (cam->endfov * frac));
};

void() editor_spline_add =
{
	int i;

	if (splinefile < 0)
		spline_init();

	/*add visible splines to the scene*/
	for (i = 0; i < camdata->numcams; i+=1i)
		spline_draw(&camdata->cam[i].view);

	/*sort out the overrides*/
//	spline_overrides(simtime);
};
void(vector curmousepos) editor_spline_overlay =
{
	if (splinefile < 0)
		spline_init();


	/*draw menu*/
	/*dunno if the light editor has any convienient code*/
	drawrawstring('0 32 0', sprintf("spline %i / %i", camdata->selcam, camdata->numcams), '8 8 0', '1 1 1', 1);
};
float(float keycode, float unicode, vector curmousepos) editor_spline_key
{
	/*print/figure out the codes yourself :P */
	return FALSE;
};

