#if 0
typedef struct
{
	string	shadername;	//aka: texture name
	vector	planenormal;
	float	planedist;
	vector	sdir;
	float	sbias;
	vector	tdir;
	float	tbias;
} brushface_t;
#define out //for annotation only
#define in //for annotation only
#define inout //for annotation only
int brush_get(float modelid, int brushid, out brushface_t *faces, int maxfaces, int *contents) = #0; //returns number of faces, or 0 on error. 
int brush_create(float modelid, in brushface_t *faces, int numfaces, int contents) = #0; //returns newly created brushid.
void brush_delete(float modelid, int brushid) = #0; //destroys brush
//float brush_editplane(float modelid, int brushid, int faceid, in brushface_t *face) = #0; //allows changing shader/s/t info, but ignores plane changes. returns success
float brush_selected(float modelid, int brushid, int faceid, float selectedstate) = #0; //allows you to easily set visual properties of a brush. if brush/face is -1, applies to all. returns old value. selectedstate=-1 changes nothing (called for its return value).
int(float modelid, int brushid, int faceid, vector *points, int maxpoints) brush_getfacepoints = #0;
int(float modelid, vector *planes, float *dists, int numplanes, int *out_brushes, int *out_faces, int maxresults) brush_findinvolume = #0;
#endif
int trace_brush_id;	//set by traceline
int trace_brush_faceid;
/*traceline/tracebox returns trace_brush_id and trace_brush_faceid


mod should track selected brush list instead of polling.
to move 50 brushes, mod needs to get+delete+transform+create
brush ids are ints. this allows different clients to use different ranges without float problems.
*/

int selectedbrush;
int selectedbrushface;
float selectedbrushmodel;
vector facepoints[64];

//history is implemented using a ringbuffer
typedef struct
{
	float timestamp;
	int brushmodel;
	int wasdelete;
	int id;
	brushface_t *brushdata;
	int numfaces;
	int contents;
} history_t;
nosave history_t historyring[128];
int history_min;	//oldest we can go
int history;		//updated on each change
int history_max;	//max value for redo

void() brush_undo =
{
	do
	{
		if (history <= history_min)
			return;
		history--;

		history_t *h = &historyring[history & (historyring.length-1)];

		//we're undoing, so deletes create and creates delete. weird, yes.
		if (h->wasdelete)
			h->id = brush_create(h->brushmodel, h->brushdata, h->numfaces, h->contents);
		else
		{
			brush_delete(h->brushmodel, h->id);
			h->id = 0;
		}
	} while (historyring[(history-1) & (historyring.length-1)].timestamp == h->timestamp);
};
void() brush_redo =
{
	do
	{
		if (history >= history_max)
			return;

		history_t *h = &historyring[history & (historyring.length-1)];

		//we're redoing stuff that has previously been doing. yay.
		if (h->wasdelete)
		{
			brush_delete(h->brushmodel, h->id);
			h->id = 0;
		}
		else
		{
			h->id = brush_create(h->brushmodel, h->brushdata, h->numfaces, h->contents);
		}
		history++;
	} while (historyring[history & (historyring.length-1)].timestamp == h->timestamp);
};
//create and journal.
int(int mod, brushface_t *faces, int numfaces, int contents) brush_history_create =
{
	history_t *h = &historyring[history & (historyring.length-1)];

	h->timestamp = time;
	memfree(h->brushdata);
	h->brushdata = memalloc(sizeof(brushface_t) * h->numfaces);
	memcpy(h->brushdata, faces, sizeof(brushface_t) * h->numfaces);
	h->numfaces = numfaces;
	h->contents = contents;
	h->brushmodel = mod;
	h->wasdelete = FALSE;

	h->id = brush_create(h->brushmodel, h->brushdata, h->numfaces, h->contents);

	history++;
	history_max = history;	//always break any pending redos
	if (history_min < history_max - historyring.length)
		history_min = history_max - historyring.length;
	return h->id;
};
//delete and journal.
void(int mod, int id) brush_history_delete =
{
	int numfaces = brush_get(mod, id, __NULL__, 0, __NULL__);
	if (!numfaces)
		return;	//doesn't exist or something, some kind of error. we can't log a delete if it didn't delete anything, if only because we can't recreate it if its undone.
	history_t *h = &historyring[history & (historyring.length-1)];

	h->timestamp = time;
	h->brushmodel = mod;
	h->id = 0;
	h->contents = 0;
	h->wasdelete = TRUE;

	memfree(h->brushdata);
	h->brushdata = memalloc(sizeof(brushface_t) * numfaces);
	h->numfaces = brush_get(mod, id, h->brushdata, numfaces, &h->contents);
	brush_delete(mod, id);

	history++;
	history_max = history;	//always break any pending redos
	if (history_min < history_max - historyring.length)
		history_min = history_max - historyring.length;
};

brushface_t tmp_faces[64];
int tmp_numfaces;
int tmp_contents;

//int selected;
int brushlist[64];

var string	autocvar_ca_newbrushtexture = "metal4_2";
var float	autocvar_ca_newbrushheight = 64;
var float	autocvar_ca_brushdisplacement = 16;
var float	autocvar_ca_brushrotation = 45.0/2;
var float	autocvar_ca_grid = 16;

enum
{
	BT_NONE,	//selection
	BT_MOVE,
	BT_MERGE,
	BT_MOVEFACE,
	BT_CREATE,
	BT_CLONEDISPLACE,
	BT_SLICE
};
int brushtool;
int bt_points;
vector bt_point[64];

//basic cube plane normals, for selection.
static nosave const vector axis[6] = {
	'-1 0 0',
	'0 -1 0',
	'0 0 -1',
	'1 0 0',
	'0 1 0',
	'0 0 1'
};
float dists[6];

vector(vector a, vector b) cross =
{
    return [	a_y * b_z - a_z * b_y,
    			a_z * b_x - a_x * b_z, 
    			a_x * b_y - a_y * b_x];
};
#define dot(a,b) ((a)*(b))

vector(vector guess) brush_snappoint =
{
	int facenum, points, point;
	float bestdist = autocvar_ca_grid*autocvar_ca_grid;	//worst case value so we don't snap to grid when there's a vertex 0.001qu away from the grid.
	vector best = guess * (1.0/autocvar_ca_grid);
	best_x = rint(best_x);	//snap to grid
	best_y = rint(best_y);
	best_z = rint(best_z);
	best *= autocvar_ca_grid;

	//find surfaces within 32qu of the point (via plane volume). use a tetrahedron instead if you want something more circular
	for (facenum = 0; facenum < axis.length; facenum++)
		dists[facenum] = (guess * axis[facenum]) + autocvar_ca_grid;
	int brushnum, numbrushes = brush_findinvolume(selectedbrushmodel, &axis[0], &dists[0], 6, &brushlist[0], __NULL__, brushlist.length);

	for (brushnum = 0; brushnum < numbrushes; brushnum++)
	{
		for (facenum = 0; ; )
		{
			points = brush_getfacepoints(selectedbrushmodel, brushlist[brushnum], ++facenum, &facepoints[0], facepoints.length);
			if (!points)
				break;	//end of face list, I guess

			//walk the faces we found and use the point if its nearer than our previous guess.
			for (point = 0; point < points; point++)
			{
				vector disp = facepoints[point] - guess;
				float dist = disp*disp;
				if (dist < bestdist)
					best = facepoints[point];
			}
		}
	}

	return best;
};

void(float brushid) editor_drawbbox =
{
	static vector bbox[2];
	int p = brush_getfacepoints(selectedbrushmodel, brushid, 0, &bbox[0], bbox.length);
	if (p == 2)
	{
		R_BeginPolygon("chop");
#define line(x,y) 			R_PolygonVertex(x, '0 0', '1 0 0', 1);	R_PolygonVertex(y, '0 0', '1 0 0', 1);	R_EndPolygon()
		line(bbox[0], ([bbox[1][0], bbox[0][1], bbox[0][2]]));
		line(bbox[0], ([bbox[0][0], bbox[1][1], bbox[0][2]]));
		line(bbox[0], ([bbox[0][0], bbox[0][1], bbox[1][2]]));
		line(bbox[1], ([bbox[0][0], bbox[1][1], bbox[1][2]]));
		line(bbox[1], ([bbox[1][0], bbox[0][1], bbox[1][2]]));
		line(bbox[1], ([bbox[1][0], bbox[1][1], bbox[0][2]]));

		line(([bbox[1][0], bbox[0][1], bbox[0][2]]), ([bbox[1][0], bbox[1][1], bbox[0][2]]));
		line(([bbox[1][0], bbox[0][1], bbox[0][2]]), ([bbox[1][0], bbox[0][1], bbox[1][2]]));
		line(([bbox[0][0], bbox[1][1], bbox[0][2]]), ([bbox[0][0], bbox[1][1], bbox[1][2]]));
		line(([bbox[0][0], bbox[1][1], bbox[0][2]]), ([bbox[1][0], bbox[1][1], bbox[0][2]]));
		line(([bbox[0][0], bbox[0][1], bbox[1][2]]), ([bbox[0][0], bbox[1][1], bbox[1][2]]));
		line(([bbox[0][0], bbox[0][1], bbox[1][2]]), ([bbox[1][0], bbox[0][1], bbox[1][2]]));
#undef line
	}
};

void(vector mousepos) editor_brushes_add =
{
	vector col = '0 0 0';
	int points, point;
	int facenum = 0;
	float intensity = (sin(gettime(5)*4)+1)*0.05;

	//make sure this shader was generated already.
	shaderforname("terrainedit",
		"{"
			"{\n"
				"map terrainedit\n"
				"blendfunc add\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
			"}\n"
		"}");

	//make sure this shader was generated already.
	shaderforname("chop",
		"{"
			"cull disable\n"
			"{\n"
				"map terrainedit\n"
				"blendfunc add\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
				"sort nearest\n"
				"nodepthtest\n"
			"}\n"
		"}");

	//draw the selected brush.
	for(;;)
	{
		points = brush_getfacepoints(selectedbrushmodel, selectedbrush, ++facenum, &facepoints[0], facepoints.length);
		if (!points)
			break;	//end of face list, I guess

		if (facenum == selectedbrushface)
			col = [0,intensity,0];
		else
			col = [intensity,0,0];

		R_BeginPolygon("terrainedit");
		for (point = 0; point < points; point++)
			R_PolygonVertex(facepoints[point], '0 0', col, 1);
		R_EndPolygon();
	}
	editor_drawbbox(selectedbrush);

	vector t = mousefar;
	vector o = mousenear;
	if (vlen(o - t) > 8192)
		t = o + normalize(t)*8192;
	traceline(o, t, TRUE, world);

#if 0
	//find all the brushes within 32qu of the mouse cursor's impact point
	//you can tweak this to find the nearest brush vertex efficiently (or snap it to a grid or so).
	//you can then slice through a brush by generating a plane between three points found this way and inserting it into the brush, clipping away the extra part.
	//(remember, the engine will automatically discard any degenerate planes)
	col = '0 0 1';

	//generate the volume
	for (facenum = 0; facenum < 6; facenum++)
		dists[facenum] = (trace_endpos * axis[facenum]) + 32;

	int brushnum, numbrushes = brush_findinvolume(selectedbrushmodel, axis, dists, dists.length, &brushlist[0], __NULL__, brushlist.length);
	for (brushnum = 0; brushnum < numbrushes; brushnum++)
	{
		for (facenum = 0; ; )
		{
			points = brush_getfacepoints(selectedbrushmodel, brushlist[brushnum], ++facenum, &facepoints[0], facepoints.length);
			if (!points)
				break;	//end of face list, I guess

			R_BeginPolygon("terrainedit");
			for (point = 0; point < points; point++)
				R_PolygonVertex(facepoints[point], '0 0', col, 1);
			R_EndPolygon();
		}
	}
#endif

	//draw a line/triangle to show the selection.
	int showpoints = bt_points;
	if (brushtool == BT_SLICE)
	{
//		bt_point[showpoints++] = brush_snappoint(trace_endpos);
		showpoints = 3;
	}
	if (brushtool == BT_CREATE || brushtool == BT_CLONEDISPLACE)
		bt_point[showpoints++] = brush_snappoint(trace_endpos);

	//FIXME: if slicing, draw the intersection

	if (showpoints)
	{
		col = '0 0 1';

		R_BeginPolygon("chop");
		for (point = 0; point < showpoints; point++)
			R_PolygonVertex(bt_point[point], '0 0', col, 1);
		R_EndPolygon();
	}

	//FIXME: if creating, draw a wireframe brush.
};

void(vector mousepos) editor_brushes_overlay =
{
	switch(brushtool)
	{
	case BT_CLONEDISPLACE:
		drawrawstring('0 32 0', "Clone+Displace", '8 8 0', '1 1 1', 1);
		if (!selectedbrush)
			brushtool = BT_NONE;
		break;
	case BT_CREATE:
		drawrawstring('0 32 0', "Paint+Create", '8 8 0', '1 1 1', 1);
		break;
	case BT_SLICE:
		drawrawstring('0 32 0', "Slice Tool", '8 8 0', '1 1 1', 1);
		if (!selectedbrush)
			brushtool = BT_NONE;
		break;
	}
};

//yay pointers!
//move a brush so that its planes all move without any translations in positions or texcoords
static void brushface_translate(brushface_t *fa, int numfaces, vector displacement)
{
	int i;
	for (i = 0; i < numfaces; i++, fa++)
	{
		fa->planedist += fa->planenormal * displacement;
		fa->sbias -= fa->sdir * displacement;
		fa->tbias -= fa->tdir * displacement;
	}
};

//rotates a list of faces by the current v_* vectors, around the origin.
//translate before+after first in order to deal with pivots.
static void brushface_rotate(brushface_t *fa, int numfaces)
{
	int i;
	for (i = 0; i < numfaces; i++, fa++)
	{
		vector orig = fa->planenormal;
		fa->planenormal[0] = orig * v_forward;
		fa->planenormal[1] = orig * -v_right;	//quake just isn't right...
		fa->planenormal[2] = orig * v_up;

		orig = fa->sdir;
		fa->sdir[0] = orig * v_forward;
		fa->sdir[1] = orig * -v_right;	//quake just isn't right...
		fa->sdir[2] = orig * v_up;

		orig = fa->tdir;
		fa->tdir[0] = orig * v_forward;
		fa->tdir[1] = orig * -v_right;	//quake just isn't right...
		fa->tdir[2] = orig * v_up;
	}
};

//generates default quakeed-style texture mapping for the given surface.
//this sucks for cylinders, but keeps slopes and things easy.
void(brushface_t *fa) reset_texturecoords =
{
	//figure out some default texture coords	
	fa->sdir = '0 0 0';
	fa->sbias = 0;
	fa->tdir = '0 0 0';
	fa->tbias = 0;
	float a=fabs(fa->planenormal[0]),b=fabs(fa->planenormal[1]),c=fabs(fa->planenormal[2]);
	if (a>=b&&a>=c)
		fa->sdir[1] = 1;
	else
		fa->sdir[0] = 1;
	if (c>a&&c>b)
		fa->tdir[1] = -1;
	else
		fa->tdir[2] = -1;
};

void() editor_brushes_registercommands =
{
	registercommand("brushedit_undo");
	registercommand("brushedit_redo");
	registercommand("brushedit_create");
	registercommand("brushedit_clone");
	registercommand("brushedit_slice");
	registercommand("brushedit_matchface");
	registercommand("brushedit_resettexcoords");
	registercommand("brushedit_settexture");
	registercommand("brushedit_rotate");
	registercommand("brushedit_binds");
};
float() editor_brushes_command =
{
	switch(argv(0))
	{
	case "brushedit_undo":
		brush_undo();
		break;
	case "brushedit_redo":
		brush_undo();
		break;
	case "brushedit_create":
		brushtool = BT_CREATE;
		bt_points = 0;
		break;
	case "brushedit_clone":
		brushtool = BT_CLONEDISPLACE;
		bt_points = 0;
		break;
	case "brushedit_slice":
		brushtool = BT_SLICE;
		bt_points = 0;
		break;
	case "brushedit_matchface":
		//FIXME
		brushtool = BT_NONE;
		bt_points = 0;
		break;
	case "brushedit_resettexcoords":
		//FIXME
		brushtool = BT_NONE;
		bt_points = 0;
		break;
	case "brushedit_settexture":
		//FIXME
		brushtool = BT_NONE;
		bt_points = 0;
		break;
	case "brushedit_rotate":
		//FIXME
		brushtool = BT_NONE;
		bt_points = 0;
		break;
	case "brushedit_binds":
		localcmd("conmenu\n");
		localcmd("menubind 0  0 \"Creation Tool\" \"brushedit_create\"\n");
		localcmd("menubind 0  8 \"Clone Tool\" \"brushedit_clone\"\n");
		localcmd("menubind 0 16 \"Slice Tool\" \"brushedit_slice\"\n");
		localcmd("menubind 0 24 \"Undo\" \"brushedit_undo\"\n");
		localcmd("menubind 0 32 \"Redo\" \"brushedit_redo\"\n");
		break;
	default:
		return FALSE;
	}

	//just in case.
	cvar_set("ca_show", "1");
	cvar_set("ca_editormode", ftos(MODE_BRUSHEDIT));
	return TRUE;
};

vector(vector dir) axialize =
{
	vector a;
	a_x = fabs(dir_x);
	a_y = fabs(dir_y);
	a_z = fabs(dir_z);
	if (a_x > a_y && a_x > a_z)
		return (dir_x > 0)?'1 0 0':'-1 0 0';
	if (a_y > a_x && a_y > a_z)
		return (dir_y > 0)?'0 1 0':'0 -1 0';
	return (dir_z > 0)?'0 0 1':'0 0 -1';
};

float(float key, float unic, vector mousepos) editor_brushes_key =
{
	brushface_t *fa;
	brushface_t *infa;
	vector t = mousefar;
	vector o = mousenear;
	vector displacement;
	int i, p;
	if (vlen(o - t) > 8192)
		t = o + normalize(t)*8192;
	if (key == K_ESCAPE)
	{
		if (brushtool)
			brushtool = BT_NONE;
		return TRUE;
	}
	if (key == K_MOUSE1)
	{
		traceline(o, t, TRUE, world);

		if (brushtool == BT_SLICE || brushtool == BT_CREATE || brushtool == BT_CLONEDISPLACE)
		{
			if (brushtool == BT_SLICE && bt_points > 2)
				bt_points = 2;

//FIXME: BT_CREATE is planar. so keep the points planar too.
//FIXME: need a way to move points already placed
//FIXME: create needs to ensure verts are clockwise and convex.
			if (bt_points < bt_point.length)
			{
				bt_point[bt_points] = brush_snappoint(trace_endpos);
				bt_points++;
			}

			if (brushtool == BT_SLICE && bt_points == 1)
			{	//slice makes assumptions about the brush, so that you don't have to move to the other side of it first.
				//it ALWAYS draws 3 points if any are defined, so make sure 2+3 have valid locations once point 1 is defined.
				int majoraxis;
				traceline(o, t, TRUE, world);

				brush_selected(selectedbrushmodel, selectedbrush, -1, FALSE);

				static vector bbox[2];
				p = brush_getfacepoints(selectedbrushmodel, selectedbrush, 0, &bbox[0], bbox.length);
	
				t[0] = fabs(trace_plane_normal[0]);
				t[1] = fabs(trace_plane_normal[1]);
				t[2] = fabs(trace_plane_normal[2]);
				if (t[2] > t[0] && t[2] > t[1]) 
					majoraxis = 2;
				else if (t[1] > t[0])
					majoraxis = 1;
				else
					majoraxis = 0;

				bt_point[1] = bt_point[0];
				bt_point[1][majoraxis] = bbox[trace_plane_normal[majoraxis]<0][majoraxis];
				majoraxis = !majoraxis;
				bt_point[2] = bt_point[0];
				bt_point[2][majoraxis] = bbox[trace_plane_normal[majoraxis]<0][majoraxis];
			}
			if  (brushtool == BT_CLONEDISPLACE && bt_points == 2)
			{
				displacement = bt_point[1] - bt_point[0];
				if (selectedbrush && displacement != '0 0 0')
				{
					bt_points = 0;
					tmp_numfaces = brush_get(selectedbrushmodel, selectedbrush, &tmp_faces[0], tmp_faces.length, &tmp_contents);
					brushface_translate(&tmp_faces[0], tmp_numfaces, displacement);
					selectedbrush = brush_history_create(selectedbrushmodel, &tmp_faces[0], tmp_numfaces, tmp_contents);
				}
			}
		}
		else	if (trace_brush_id != selectedbrush || selectedbrushface != trace_brush_faceid || selectedbrushmodel != trace_ent.modelindex)
		{
			brush_selected(selectedbrushmodel, selectedbrush, -1, FALSE);
			selectedbrush = trace_brush_id;
			selectedbrushface = trace_brush_faceid;
			selectedbrushmodel = trace_ent.modelindex;
			brush_selected(selectedbrushmodel, selectedbrush, trace_brush_faceid, TRUE);
		}
		return TRUE;
	}

	if (key == K_ENTER)
	{
		if (brushtool == BT_CREATE && bt_points >= 3)
		{
			fa = &tmp_faces[0];
			fa->planenormal = normalize(cross(bt_point[2] - bt_point[0], bt_point[1] - bt_point[0]));
			fa->planedist = bt_point[0] * fa->planenormal + autocvar_ca_newbrushheight;
			fa->shadername = autocvar_ca_newbrushtexture;
			reset_texturecoords(fa);
			fa++;

			fa->planenormal = -normalize(cross(bt_point[2] - bt_point[0], bt_point[1] - bt_point[0]));
			fa->planedist = (bt_point[0] * fa->planenormal);
			fa->shadername = autocvar_ca_newbrushtexture;
			reset_texturecoords(fa);
			fa++;
			tmp_numfaces = 2;

			for (p = 0; p < bt_points; p++)
			{
				int n = p + 1i;
				if (n == bt_points)
					n = 0;
				fa->planenormal = normalize(cross(bt_point[p] - bt_point[n], tmp_faces[0].planenormal));
				fa->planedist = bt_point[p] * fa->planenormal;
				fa->shadername = autocvar_ca_newbrushtexture;
				reset_texturecoords(fa);
				fa++;
				tmp_numfaces++;
			}

//			tmp_numfaces = fa - &tmp_faces[0];

			bt_points = 0;

			selectedbrush = brush_history_create(selectedbrushmodel, &tmp_faces[0], tmp_numfaces, 1i);
		}
		else if (brushtool == BT_SLICE)
		{
			//get the current faces
			tmp_numfaces = brush_get(selectedbrushmodel, selectedbrush, &tmp_faces[0], tmp_faces.length-1, &tmp_contents);

			//generate a new face plane
			fa = &tmp_faces[tmp_numfaces];
			fa->planenormal = normalize(cross(bt_point[2] - bt_point[0], bt_point[1] - bt_point[0]));
			fa->planedist = bt_point[0] * fa->planenormal;
			fa->shadername = tmp_faces[0].shadername;	//find a neighbour?

			//make sure its okay
			for (i = 0; i < tmp_numfaces; i++)
			{
				if (tmp_faces[i].planenormal == fa->planenormal && tmp_faces[i].planedist == fa->planedist)
				{
					print("that would be co-planar\n");
					return TRUE;
				}
			}
			bt_points = 0;
			tmp_numfaces++;

			reset_texturecoords(fa);		

			//delete the old one and insert the new
			brush_history_delete(selectedbrushmodel, selectedbrush);
			selectedbrush = brush_history_create(selectedbrushmodel, &tmp_faces[0], tmp_numfaces, tmp_contents);

			//and insert another new one too, because inserting a plane like this generates two fragments and I'm too lazy to work out which is the front and which is the back.
			fa->planenormal *= -1;
			fa->planedist *= -1;
			selectedbrush = brush_history_create(selectedbrushmodel, &tmp_faces[0], tmp_numfaces, tmp_contents);
			selectedbrushface = 0;
		}
		return TRUE;
	}

	if (key == 'p')
	{
		brushtool = BT_CREATE;
		bt_points = 0;
		return TRUE;
	}
	if (key == 'l')
	{
		brushtool = BT_CLONEDISPLACE;
		bt_points = 0;
		return TRUE;
	}

	//begin the cut tool
	if (key == 'o') //I'd use #, but that would cause a problem for americans
	{
		brushtool = BT_SLICE;
		bt_points = 0;
		return TRUE;
	}
	if (key == 'm') //I'd use #, but that would cause a problem for americans
	{
		traceline(o, t, TRUE, world);

		int extrafaces;
		float found = FALSE;

		if (selectedbrush == trace_brush_id)
		{
			print("cannot merge brush with itself\n");
			return TRUE;
		}
		if (!trace_brush_id || !selectedbrush)
		{
			print("no brush targetted\n");
			return TRUE;
		}

		tmp_numfaces = brush_get(selectedbrushmodel, selectedbrush, &tmp_faces[0], tmp_faces.length, &tmp_contents);
		infa = &tmp_faces[tmp_numfaces];
		extrafaces = brush_get(selectedbrushmodel, trace_brush_id, &tmp_faces[tmp_numfaces], tmp_faces.length-tmp_numfaces, &tmp_contents);

		//merge the two sets of planes together.
		for(infa = &tmp_faces[tmp_numfaces]; extrafaces > 0; infa++, extrafaces--)
		{
			for (fa = &tmp_faces[0], i = 0; i < tmp_numfaces; i++, fa++)
			{
//print(sprintf("%v %g vs %v %g\n", infa->planenormal, infa->planedist, fa->planenormal, fa->planedist));
				if (fa->planenormal == -infa->planenormal && fa->planedist == -infa->planedist)
				{
//print(sprintf("inverted (%i)\n", ((tmp_numfaces-i)-1i)));
					//inverted. this is the plane we're merging over, so strip it out
					memcpy(fa, &fa[1], sizeof(brushface_t) * ((tmp_numfaces-i)-1));
					tmp_numfaces--;
					i--;
					fa--;
					found = TRUE;
					break;
				}
				else if (fa->planenormal == infa->planenormal && fa->planedist == infa->planedist)
				{
//print("coplanar\n");
					//coplanar surfaces are considered safe to ignore. we use the selected brush's texturing info
					break;
				}
			}
			if (i == tmp_numfaces)
			{	//okay, this plane is important, merge it into the selected brush.
//print("merge plane\n");
				memcpy(fa, infa, sizeof(brushface_t));
				tmp_numfaces++;
			}
		}
		if (!found)
		{
			print("Brushes do not share a plane\n");

			//try to find a surface to move to match to the given plane
			float val;
			float bestval = -0.707;	//must be within 45 degrees
			int bestface = -1;
			tmp_numfaces = brush_get(selectedbrushmodel, selectedbrush, &tmp_faces[0], tmp_faces.length, &tmp_contents);
			brush_get(selectedbrushmodel, trace_brush_id, &tmp_faces[tmp_numfaces], tmp_faces.length-tmp_numfaces, &tmp_contents);
			infa = &tmp_faces[tmp_numfaces + trace_brush_faceid-1i];
			for (i = 0; i < tmp_numfaces; i++)
			{
				val = tmp_faces[i].planenormal * infa->planenormal;
				if (val < bestval)
				{
					bestval = val;
					bestface = i;
				}
			}
			if (bestface != -1)
			{
				//FIXME: determine volume and reject it if we shrink?
				tmp_faces[bestface].planenormal = -infa->planenormal;
				tmp_faces[bestface].planedist = -infa->planedist;

				brush_history_delete(selectedbrushmodel, selectedbrush);
				selectedbrush = brush_history_create(selectedbrushmodel, &tmp_faces[0], tmp_numfaces, tmp_contents);
			}
			return TRUE;
		}
		//FIXME: verify that no planes got dropped, as this indicates that the region wasn't convex, and we probably just destroyed the entire thing.

		brush_history_delete(selectedbrushmodel, trace_brush_id);
		brush_history_delete(selectedbrushmodel, selectedbrush);

		selectedbrush = brush_history_create(selectedbrushmodel, &tmp_faces[0], tmp_numfaces, tmp_contents);
		selectedbrushface = 0;
		brush_selected(selectedbrushmodel, selectedbrush, -1, FALSE);
		return TRUE;
	}

	if (key == K_DEL || key == K_BACKSPACE)
	{
		if (brushtool)
			brushtool = BT_NONE;
		if (selectedbrush)
		{
			brush_history_delete(selectedbrushmodel, selectedbrush);
			selectedbrush = 0;
		}
		return TRUE;
	}
	displacement = '0 0 0';
	if (key == K_PGUP)
		displacement = autocvar_ca_brushdisplacement * axialize(v_up);
	if (key == K_PGDN)
		displacement = autocvar_ca_brushdisplacement * axialize(-v_up);
	if (key == K_KP_UPARROW)
		displacement = autocvar_ca_brushdisplacement * axialize(v_forward);
	if (key == K_KP_DOWNARROW)
		displacement = autocvar_ca_brushdisplacement * axialize(-v_forward);
	if (key == K_KP_RIGHTARROW)
		displacement = autocvar_ca_brushdisplacement * axialize(v_right);
	if (key == K_KP_LEFTARROW)
		displacement = autocvar_ca_brushdisplacement * axialize(-v_right);
	if (displacement != '0 0 0')
	{
		//this is  just an example to show how to move a brush without hurting texture coords at all.
		//note: due to precision loss, this is probably NOT a good way to move brushes around. cache the original position if possible and express transformations relative to that.
		if (selectedbrush)
		{
			tmp_numfaces = brush_get(selectedbrushmodel, selectedbrush, &tmp_faces[0], tmp_faces.length, &tmp_contents);
			brushface_translate(&tmp_faces[0], tmp_numfaces, displacement);
			brush_history_delete(selectedbrushmodel, selectedbrush);
			selectedbrush = brush_history_create(selectedbrushmodel, &tmp_faces[0], tmp_numfaces, tmp_contents);
		}
		return TRUE;
	}
	if (key == '[' || key == ']')
	{
		//this is  just an example to show how to rotate a brush.
		//note: due to precision loss, this is probably NOT a good way to move brushes around. cache the original position if possible and express transformations relative to that.

		//determine rotation matrix
		if (key == '[')
			makevectors([0, -autocvar_ca_brushrotation, 0]);
		else
			makevectors([0, autocvar_ca_brushrotation, 0]);

		//rotate the selected brush
		if (selectedbrush)
		{
			//find the brush's middle (based on its bbox)
			brush_getfacepoints(selectedbrushmodel, selectedbrush, 0, &displacement, 1);

			//grab the existing brush's planes
			tmp_numfaces = brush_get(selectedbrushmodel, selectedbrush, &tmp_faces[0], tmp_faces.length, &tmp_contents);
			//move it so its pivot is at the origin
			brushface_translate(&tmp_faces[0], tmp_numfaces, -displacement);
			//rotate it
			brushface_rotate(&tmp_faces[0], tmp_numfaces);
			//reposition it around its pivot again
			brushface_translate(&tmp_faces[0], tmp_numfaces, displacement);
			//delete the old version of the brush and insert the newly translated/rotated version of it.
			brush_history_delete(selectedbrushmodel, selectedbrush);
			selectedbrush = brush_history_create(selectedbrushmodel, &tmp_faces[0], tmp_numfaces, tmp_contents);
		}
		return TRUE;
	}

	if (key == K_KP_PLUS || key == K_KP_MINUS)
	{
		//push a plane along its normal. yes, this will deform it if its not a cube, as the adjacent planes will not be changed, allowing them to cut into it differently.
		if (selectedbrush && selectedbrushface)
		{
			fa = &tmp_faces[selectedbrushface-1];
			i = tmp_numfaces = brush_get(selectedbrushmodel, selectedbrush, &tmp_faces[0], tmp_faces.length, &tmp_contents);
			displacement = fa->planenormal;
			if (key == K_KP_MINUS)
				displacement *= -1;
			fa->planedist += fa->planenormal * displacement;
			fa->sbias -= fa->sdir * displacement;
			fa->tbias -= fa->tdir * displacement;
			brush_history_delete(selectedbrushmodel, selectedbrush);
			selectedbrush = brush_history_create(selectedbrushmodel, &tmp_faces[0], tmp_numfaces, tmp_contents);

			tmp_numfaces = brush_get(selectedbrushmodel, selectedbrush, &tmp_faces[0], tmp_faces.length, &tmp_contents);
			if (i != tmp_numfaces)
				selectedbrushface = 0;	//if the engine rejected a plane, then it's because something became invalid. don't edit a different face.
				
		}
		return TRUE;
	}
	if (key == 'u')
	{
		brush_undo();
		return TRUE;
	}
	if (key == 'i')
	{
		brush_redo();
		return TRUE;
	}
/*	if (key == 's')
	{
		//CSG subtraction is actually quite easy...
		//for each brush that intersects us, split it by every single one of our planes that intesect
		//drop the resulting brushes if they contain contain points only within the subtraction region
	}
*/	return FALSE;
};
