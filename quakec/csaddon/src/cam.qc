typedef struct
{
	int numpoints;
	int type;
	vector pos[64];
	float totallength;
	vector color;
	int selected_point;
	float position;
} spline_t;

typedef struct
{
	float starttime;
	float endtime;
	float startfov;
	float endfov;
	spline_t spline;
} cam_t;

typedef struct
{
	int ver;
	int position_count;
	int position_selected;
	int view_count;
	int view_selected;
	cam_t position[64];
	cam_t view[64];
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
	if (s->numpoints < 2)
	{
		return;
	}
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

void(vector p1, vector p2, vector color) fakeline = {
	R_BeginPolygon("camsplineshader");
	R_PolygonVertex(p1+'0 0 1', '0 0', color,1);
	R_PolygonVertex(p1-'0 0 1', '0 1', color,1);
	R_PolygonVertex(p2-'0 0 1', '1 1', color,1);
	R_PolygonVertex(p2+'0 0 1', '1 0', color,1);
	R_PolygonVertex(p1-'0 0 1', '0 0', color,1);
	R_PolygonVertex(p1+'0 0 1', '0 1', color,1);
	R_PolygonVertex(p2+'0 0 1', '1 1', color,1);
	R_PolygonVertex(p2-'0 0 1', '1 0', color,1);

	R_EndPolygon();
};

void (vector pos, vector color) draw_point
{
	local vector pos1, pos2;
	//x
	pos1 = pos - '10 0 0';
	pos2 = pos + '10 0 0';
	fakeline(pos1, pos2, color);
	//y
	pos1 = pos - '0 10 0';
	pos2 = pos + '0 10 0';
	fakeline(pos1, pos2, color);
	//z -- this isnt drawing right for some reason
	pos1 = pos - '0 0 10';
	pos2 = pos + '0 0 10';
	fakeline(pos1, pos2, color);
};

// time has to be between 0 and 1
vector(spline_t *s, float frac_time) spline_calculate_position =
{
	local vector r, point1, point2, point3, point4;
	local float t1, t2, t3, t4, t5, t_scaled;
	local int i;

	if (s->numpoints == 0)
	{
		return '0 0 0';
	}

	// point
	if (s->numpoints == 1)
	{
		return s->pos[0];
	}

	// straight line
	if (s->numpoints == 2)
	{
		r = s->pos[1] - s->pos[0];
		r = r * frac_time;
		r = r + s->pos[0];
		return r;
	}

	// cubic spline
	if (s->numpoints == 3)// || (s->numpoints > 3 && s->type == 1))
	{
		t1 = 1-frac_time;
		t2 = pow(t1, s->numpoints - 1);
		t3 = pow(frac_time, s->numpoints - 1);

		for (i=0; i<s->numpoints; i++)
		{
			if (i == 0)
			{
				r = s->pos[i] * t2;
			}
			else if (i == s->numpoints - 1)
			{
				r = r + s->pos[i] * t3;
			}
			else
			{
				t4 = (s->numpoints-1) * pow(frac_time, i) * pow(t1, s->numpoints - 1 - i);
				r = r + s->pos[i] * t4;
			}
		}
		return r;
	}

	// catmull rom
	if (s->numpoints > 3)
	{
		// getting the start point -- might change if we add times to points
		// first point is always an entry point last point also
		t5 = 1 / (s->numpoints - 3);
		for (i=1, t4=t5; t4<frac_time; t4 = t4 + t5)
		{
			i++;
		}
		//print(sprintf("end: %i\n", i));
		if (i > s->numpoints)
		{
			print(sprintf("something went wrong while getting the start point!\n"));
			return s->pos[0];
		}


		point1 = s->pos[i - 1i];
		point2 = s->pos[i];
		point3 = s->pos[i+1i];
		point4 = s->pos[i+2i];

		// scale the time so its 0 -> 1 between point1 and 2
		/*
			1		2		3		4		5		6
					0		1
					0						1
		*/

		t_scaled = (frac_time - (t4 - t5))/t5; // hope this works :P

		// would be really nice if i could access vector components via [0] [1] ... here
		// warning RETARDED!
		t1 = point2_x;
		t2 = -0.5 * point1_x + 0.5 * point3_x;
		t3 = point1_x + (-2.5 * point2_x) + 2.0* point3_x +(-0.5 *point4_x);
		t4 = -0.5 * point1_x + 1.5 * point2_x + (-1.5 * point3_x) + 0.5 * point4_x;
		r_x = ((t4 * t_scaled + t3)*t_scaled + t2)*t_scaled + t1;

		t1 = point2_y;
		t2 = -0.5 * point1_y + 0.5 * point3_y;
		t3 = point1_y + (-2.5 * point2_y) + 2.0* point3_y +(-0.5 *point4_y);
		t4 = -0.5 * point1_y + 1.5 * point2_y + (-1.5 * point3_y) + 0.5 * point4_y;
		r_y = ((t4 * t_scaled + t3)*t_scaled + t2)*t_scaled + t1;

		t1 = point2_z;
		t2 = -0.5 * point1_z + 0.5 * point3_z;
		t3 = point1_z + (-2.5 * point2_z) + 2.0* point3_z +(-0.5 *point4_z);
		t4 = -0.5 * point1_z + 1.5 * point2_z + (-1.5 * point3_z) + 0.5 * point4_z;
		r_z = ((t4 * t_scaled + t3)*t_scaled + t2)*t_scaled + t1;




		return r;
	}

	return s->pos[0];

};

static void(spline_t *s) spline_draw =
{
	local float step, frac;
	local vector pos, pos1, pos2, vpos;
	local vector bdir;
	local int i;
	local vector color;

	/*example of drawing convex polygons*/
	R_BeginPolygon("camsplineshader");

	vpos = getviewprop(VF_ORIGIN);

	if (s->numpoints < 3)
	{
		step = 1; // should be fine
	}
	else
	{
		step = 0.01;
	}

	color = '1 0 1';

	for (frac=0; frac<=1; frac = frac + step)
	{
		pos = spline_calculate_position(s, frac);
		if (frac == 0)
		{
			pos2 = pos;
			continue;
		}

		fakeline(pos2, pos, color);
		pos2 = pos;
	}

	//fakeline(pos2, s->pos[s->numpoints-1]);

//	draw points
	for (i=0; i<s->numpoints; i++)
	{
		color = '0.25 0.25 0.25';
		if (i == s->selected_point)
			color = '1 1 1';
		draw_point(s->pos[i], color);
	}
	// draw position
	pos = spline_calculate_position(s, s->position);
	draw_point(pos, '1 0 0');
};

void(float attime) spline_overrides =
{
};
/*called to move the view to some simulation time*/
/*
void(float attime) spline_overrides =
{
	local cam_t *cam;
	local int i;
	local vector src, dst;
	local float frac;

	//find correct spline based upon time
	for (i = 0; i < camdata->count; i++)
	{
		cam = &camdata->cam[i];
		if (cam->starttime <= attime && cam->endtime > attime)
			break;
	}
	//give up if no matches
	if (i == camdata->count)
		return;

	frac = (attime - cam->starttime) / (cam->endtime - cam->starttime);
	src = spline_pos(&cam->view, frac);
	dst = spline_pos(&cam->focus, frac);

	setviewprop(VF_ORIGIN, src);
	setviewprop(VF_ANGLES, vectoangles(dst - src));
	setviewprop(VF_AFOV, (cam->startfov * (1-frac)) + (cam->endfov * frac));
};
*/

void() editor_spline_add =
{
	int i;

	if (splinefile < 0)
		spline_init();

	/*add visible splines to the scene*/
	for (i = 0; i < camdata->position_count; i+=1i)
		spline_draw(&camdata->position[i].spline);

	/*sort out the overrides*/
//	spline_overrides(simtime);
};
void(vector curmousepos) editor_spline_overlay =
{
	local vector pos, v;
	local spline_t *s;

	if (splinefile < 0)
		spline_init();


	/*draw menu*/
	/*dunno if the light editor has any convienient code*/
	pos_y = 40;
	drawrawstring(pos, sprintf("cam  : %i / %i", camdata->position_selected + 1i , camdata->position_count), '8 8 0', '1 1 1', 1);
	if (camdata->position_count)
	{
		pos_y += 8;
		s = &camdata->position[camdata->position_selected].spline;
		v = s->pos[s->selected_point];
		drawrawstring(pos, sprintf(" point: %i / %i -- %f %f %f", s->selected_point + 1i, s->numpoints, v_x, v_y, v_z), '8 8 0', '1 1 1', 1);
	}
	pos_y += 8;
	drawrawstring(pos, sprintf("view : %i / %i", camdata->view_selected + 1i, camdata->view_count), '8 8 0', '1 1 1', 1);
};

int (spline_t *s, vector point) spline_add_point = 
{
	if (s->numpoints + 1 >= 64)
	{
		return 1;
	}

	s->pos[s->numpoints] = point;
	s->selected_point = s->numpoints;
	s->numpoints = s->numpoints + 1i; //oldskool!
	return 0;
};

void (spline_t *s, vector point) spline_set_point =
{
	s->pos[s->selected_point] = point;
};

void (int type) position_add_point =
{
	vector v;
	int i;
	local cam_t *c;
	local spline_t *s;

	i = camdata->position_selected;
	c = &camdata->position[i];
	s = &c->spline;
	v = getviewprop(VF_ORIGIN);
	if (type == 0)
	{
		if (spline_add_point(s, v))
		{
//			print(sprintf("point could not be added.\n"));
		}
		else
		{
//			print(sprintf("added point: %i %f %f %f\n", s->selected_point, v_x, v_y, v_z));
		}
	}
}

void(void) position_add
{
	print(sprintf("test\n"));
	if (camdata->position_count >= 64)
	{
		return;
	}

	print(sprintf("adding a position\n"));

	camdata->position_selected = camdata->position_count;
	camdata->position_count = camdata->position_count + 1i; //oldskool
}

void(void) view_add
{
	if (camdata->view_count >= 64)
	{
		return;
	}
	camdata->view_selected = camdata->view_count;
	camdata->view_count = camdata->view_count + 1i;
}

void(spline_t *s, vector change) spline_change_point_position
{
	if (s->numpoints == 0)
		return;
	s->pos[s->selected_point] = s->pos[s->selected_point] + change;
};

void (spline_t *s, float change) spline_change_position
{
	s->position = s->position + change;
	if (s->position < 0)
		s->position = 0;
	else if (s->position > 1)
		s->position = 1;
}

void(spline_t *s, int change) spline_change_selected_point
{
	if (s->numpoints == 0)
		return;

	if (change + s->selected_point < 0)
		s->selected_point = s->numpoints -1;
	else if (change + s->selected_point >= s->numpoints)
		s->selected_point = 0;
	else
		s->selected_point = s->selected_point + change;
};

float(float keycode, float unicode, vector curmousepos) editor_spline_key
{
	if (keycode == 'j')
	{
		if (camdata->position_count == 0)
			position_add();
		position_add_point(0);
	}

	if (keycode == 'k')
	{
		if (camdata->position_count == 0)
			return FALSE;
		spline_change_selected_point(&camdata->position[camdata->position_selected].spline, -1);
		return TRUE;
	}

	if (keycode == 'l')
	{
		if (camdata->position_count == 0)
			return FALSE;
		spline_change_selected_point(&camdata->position[camdata->position_selected].spline, 1);
		return TRUE;
	}

	if (keycode == '9')
	{
		if (camdata->position_count == 0)
			return FALSE;
		spline_change_point_position(&camdata->position[camdata->position_selected].spline, '0 0 -1');
		return TRUE;
	}

	if (keycode == '0')
	{
		if (camdata->position_count == 0)
			return FALSE;
		spline_change_point_position(&camdata->position[camdata->position_selected].spline, '0 0 1');
		return TRUE;
	}

	if (keycode == '7')
	{
		if (camdata->position_count == 0)
			return FALSE;
		spline_change_point_position(&camdata->position[camdata->position_selected].spline, '0 -1 0');
		return TRUE;
	}

	if (keycode == '8')
	{
		if (camdata->position_count == 0)
			return FALSE;
		spline_change_point_position(&camdata->position[camdata->position_selected].spline, '0 1 0');
		return TRUE;
	}

	if (keycode == '5')
	{
		if (camdata->position_count == 0)
			return FALSE;
		spline_change_point_position(&camdata->position[camdata->position_selected].spline, '-1 0 0');
		return TRUE;
	}

	if (keycode == '6')
	{
		if (camdata->position_count == 0)
			return FALSE;
		spline_change_point_position(&camdata->position[camdata->position_selected].spline, '1 0 0');
		return TRUE;
	}

	if (keycode == '-')
	{
		if (camdata->position_count == 0)
			return FALSE;
		spline_change_position(&camdata->position[camdata->position_selected].spline, -0.01);
		return TRUE;
	}

	if (keycode == '=')
	{
		if (camdata->position_count == 0)
			return FALSE;
		spline_change_position(&camdata->position[camdata->position_selected].spline, 0.01);
		return TRUE;
	}





	return FALSE;
};

