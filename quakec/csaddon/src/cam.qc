typedef struct
{
	int maxpoints;
	int something;
	vector pos[64];
} spline_t;
typedef struct
{
	int numsplines;
	spline_t spline[64];
} splinedata_t;
static splinedata_t *splinedata;
static var float splinefile = -1;

void() spline_init =
{
	/*precache the shader*/
	shaderforname("camsplineshader", "{\n{\nmap splinetexture.tga\nblendfunc blend\nrgbgen vertex\nalphagen vertex\n}\n}\n");

	splinefile = fopen("spline.dat", FILE_MMAP_RW, sizeof(splinedata_t));
	if (splinefile < 0)
	{
		/*too lazy to create a file, just use it as a malloc*/
		splinefile = fopen("", FILE_MMAP_RW, sizeof(splinedata_t));
	}

	splinedata = (splinedata_t*)(fgets(splinefile));


	print("Temp test code\n");
	splinedata->numsplines = 1;
	splinedata->spline[0].maxpoints = 4;
	splinedata->spline[0].pos[0] = '-1024 -1024';
	splinedata->spline[0].pos[1] = '+1024 -1024';
	splinedata->spline[0].pos[2] = '+1024 +1024';
	splinedata->spline[0].pos[3] = '-1024 +1024';

	splinedata->spline[0].pos[4] = '-1024 +1024';
	splinedata->spline[0].pos[5] = '+1024 +1024';
	splinedata->spline[0].pos[6] = '+1024 -1024';
	splinedata->spline[0].pos[7] = '-1024 -1024';
};
void() spline_shutdown =
{
	fclose(splinefile);
	splinefile = -1;
};

/*called each frame*/
void(float attime) spline_overrides =
{
	local spline_t *spline;

	/*find correct spline based upon time global*/

	if (spline->something)
	{
//		setviewprop(VF_ORIGIN, somepos);
//		setviewprop(VF_ANGLE, someangle);
//		setviewprop(VF_AFOV, 90);
	}
};

static void(spline_t *s) spline_draw =
{
	/*example of drawing convex polygons*/
	R_BeginPolygon("camsplineshader");

	R_PolygonVertex(s->pos[0], '0 0', '1 1 1', 1);
	R_PolygonVertex(s->pos[1], '1 0', '1 1 1', 1);
	R_PolygonVertex(s->pos[2], '1 1', '1 1 1', 1);
	R_PolygonVertex(s->pos[3], '0 1', '1 1 1', 1);
	R_EndPolygon();

	/*do another with the same shader*/
	R_PolygonVertex(s->pos[4], '0 0', '1 1 1', 1);
	R_PolygonVertex(s->pos[5], '1 0', '1 1 1', 1);
	R_PolygonVertex(s->pos[6], '1 1', '1 1 1', 1);
	R_PolygonVertex(s->pos[7], '0 1', '1 1 1', 1);
	R_EndPolygon();
};

void() editor_spline_add =
{
	int i;

	if (splinefile < 0)
		spline_init();

	/*add visible splines to the scene*/
	for (i = 0; i < splinedata->numsplines; i+=1i)
		spline_draw(&splinedata->spline[i]);

	/*sort out the overrides*/
//	spline_overrides(simtime);
};
void(vector curmousepos) editor_spline_overlay =
{
	if (splinefile < 0)
		spline_init();


	/*draw menu*/
	/*dunno if the light editor has any convienient code*/
	drawrawstring('0 32 0', "crappy not-implemented menu", '8 8 0', '1 1 1', 1);
};
float(float keycode, float unicode, vector curmousepos) editor_spline_key
{
	/*print/figure out the codes yourself :P */
	return FALSE;
};

