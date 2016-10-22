/*traceline/tracebox returns trace_brush_id and trace_brush_faceid


mod should track selected brush list instead of polling.
to move 50 brushes, mod needs to get+delete+transform+create
brush ids are ints. this allows different clients to use different ranges without float problems.
*/

int *selectedbrushes;
int selectedbrushcount;

int selectedbrush;
int selectedbrushface;
int selectedbrushmodel;
vector facepoints[64];

var float autocvar_ca_brush_view = 0;		//0=normal, 1=x, 2=y, 3=z
var float autocvar_ca_brush_viewsize = 1024;	//for different views.

#define EPSILON (1.0 / 32)	//inprecision sucks.

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

int(int brushid) brush_isselected =
{
	for (int i = 0; i < selectedbrushcount; i++)
		if (selectedbrushes[i] == brushid)
			return i+1;
	return 0;
};
int(int brushid) brush_deselect =
{
	int i = brush_isselected(brushid);
	if (!i)
		return FALSE;
	brush_selected(selectedbrushmodel, brushid, -1, FALSE);
	memcpy(&selectedbrushes[i-1], &selectedbrushes[i], sizeof(int)*(selectedbrushcount-i));
	selectedbrushcount--;
	return TRUE;
};
void() brush_deselectall =
{
	brush_selected(selectedbrushmodel, selectedbrush, -1, FALSE);
	selectedbrush = 0;
	selectedbrushface = 0;
	selectedbrushcount = 0;
};
void(int brushid) brush_select =
{
	if (!brush_isselected(brushid))
	{
		int *n = memalloc(sizeof(int) * (selectedbrushcount+1));
		memcpy(n, selectedbrushes, sizeof(int)*selectedbrushcount);
		memfree(selectedbrushes);
		selectedbrushes = n;
		n[selectedbrushcount++] = brushid;
		brush_selected(selectedbrushmodel, brushid, -1, TRUE);
	}
	if (!selectedbrush)
		selectedbrush = brushid;
};

//when replaying history, ids get changed, which makes 
static void(int old, int new) history_remap =
{
	if (selectedbrush == old)
		selectedbrush = new;
	for (int i = history_min; i < history_max; i++)
	{
		history_t *h = &historyring[i & (historyring.length-1)];
		if (h->id == old)
			h->id = new;
	}
};

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
			history_remap(h->id, brush_create(h->brushmodel, h->brushdata, h->numfaces, h->contents));
		else
		{
			brush_delete(h->brushmodel, h->id);
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

		//we're redoing stuff that has previously been done. yay.
		if (h->wasdelete)
		{
			brush_delete(h->brushmodel, h->id);
		}
		else
		{
			history_remap(h->id, brush_create(h->brushmodel, h->brushdata, h->numfaces, h->contents));
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
	h->brushdata = memalloc(sizeof(brushface_t) * numfaces);
	memcpy(h->brushdata, faces, sizeof(brushface_t) * numfaces);
	h->numfaces = numfaces;
	h->contents = contents;
	h->brushmodel = mod;
	h->wasdelete = FALSE;

	h->id = brush_create(h->brushmodel, h->brushdata, h->numfaces, h->contents);

	if (h->id)
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
	h->id = id;
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

int(int mod, int id, brushface_t *faces, int numfaces, int contents) brush_history_edit =
{
	int wasselected = brush_isselected(id);
	int wasmainselected = selectedbrush == id;
	
	brush_history_delete(mod, id);
	id = brush_history_create(mod, faces, numfaces, contents);
	if (wasselected)
		selectedbrushes[wasselected-1] = id;
	if (wasmainselected)
		selectedbrush = id;
	return id;
};

typedef struct
{
	int numverts;
	int numidx;
	vector *p;
	int *i;
	
	int selectedvert1;
	int selectedvert2;
} vertsoup_t;
vertsoup_t vertedit;
brushface_t tmp_faces[64];
int tmp_numfaces;
int tmp_contents;

//int selected;
int brushlist[2048];

var string	autocvar_ca_newbrushtexture = "metal4_2";
var float	autocvar_ca_newbrushheight = 64;
var float	autocvar_ca_grid = 16;

enum int
{
	BT_NONE,	//selection
	BT_MOVE = BT_NONE,
	BT_ROTATE,
	BT_MERGE,
	BT_PUSHFACE,
	BT_CREATE,
	BT_CREATEDRAG,
	BT_CLONEDISPLACE,
	BT_SLICE,
	BT_MOVETEXTURE,
	BT_VERTEXEDIT
};
int mousetool;
int brushtool;
int bt_points;
vector bt_displace;
vector bt_point[64];
int nogrid;	//+nogrid, temporarily disables grid locks

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

static int(brushface_t *fa, int famax, vector *points, int numpoints, float height) BrushFromPoints =
{
	int count = 0;
	
	fa->planenormal = normalize(crossproduct(points[2] - points[0], points[1] - points[0]));
	fa->planedist = bt_point[0] * fa->planenormal + height;
	fa->shadername = autocvar_ca_newbrushtexture;
	reset_texturecoords(fa);
	fa++;

	fa->planenormal = -normalize(crossproduct(points[2] - points[0], points[1] - points[0]));
	fa->planedist = (bt_point[0] * fa->planenormal);
	fa->shadername = autocvar_ca_newbrushtexture;
	reset_texturecoords(fa);
	fa++;
	count = 2;

	for (int p = 0; p < numpoints; p++)
	{
		int n = p + 1;
		if (n == numpoints)
			n = 0;
		fa->planenormal = normalize(crossproduct(points[p] - bt_point[n], tmp_faces[0].planenormal));
		fa->planedist = points[p] * fa->planenormal;
		fa->shadername = autocvar_ca_newbrushtexture;
		reset_texturecoords(fa);
		fa++;
		count++;
	}

	return count;
}


vector(vector guess) brush_snappoint =
{
	if (nogrid)
		return guess;
	int facenum, points;
	float bestdist = autocvar_ca_grid*autocvar_ca_grid;	//worst case value so we don't snap to grid when there's a vertex 0.001qu away from the grid.
	vector best = guess * (1.0/autocvar_ca_grid);
	best_x = rint(best_x);	//snap to grid
	best_y = rint(best_y);
	best_z = rint(best_z);
	best *= autocvar_ca_grid;

	//find surfaces within 32qu of the point (via plane volume). use a tetrahedron instead if you want something more circular
	for (facenum = 0; facenum < axis.length; facenum++)
		dists[facenum] = (guess * axis[facenum]) + autocvar_ca_grid;
	int numbrushes = brush_findinvolume(selectedbrushmodel, axis, dists, 6, brushlist, __NULL__, brushlist.length);

	for (int brushnum = 0; brushnum < numbrushes; brushnum++)
	{
		for (facenum = 0; ; )
		{
			points = brush_getfacepoints(selectedbrushmodel, brushlist[brushnum], ++facenum, facepoints, facepoints.length);
			if (!points)
				break;	//end of face list, I guess

			//walk the faces we found and use the point if its nearer than our previous guess.
			for (int point = 0; point < points; point++)
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

void(int brushid) editor_drawbbox =
{
	static vector bbox[2];
	int p = brush_getfacepoints(selectedbrushmodel, brushid, 0, bbox, bbox.length);
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
	for (int i = 0; i < numfaces; i++, fa++)
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

vector(vector in) channelizeangle =
{
	in_x = anglemod(in_x);
	in_y = anglemod(in_y);
	in_z = anglemod(in_z);
	if (in_x > 180)
		in_x -= 360;
	if (in_y > 180)
		in_y -= 360;
	if (in_z > 180)
		in_z -= 360;

	float fx = fabs(in_x);
	float fy = fabs(in_y);
	float fz = fabs(in_z);
	cprint(sprintf("%v", in));
	if (fx > fy && fx > fz)
		return [in_x,0,0];
	if (fy > fz)
		return [0,in_y,0];
	return [0,0,in_z];
}

vector(vector p1, vector p2, vector norm, float dist) planelinepoint =
{
	float d1 = p1*norm - dist;
	float d2 = p2*norm - dist;
	float frac = d1 / (d2-d1);

//	frac = bound(0, frac, 1);

	return p2 + (p1-p2)*frac;	//convert that frac into an actual position
};


int(brushface_t *faces, int numfaces, vector *points, int numpoints) isconcave =
{
	int result = 0;

	//if any of the points are outside the brush, then we know that one of the planes cut straight through in a concavey kind of way
	for(int f = 0; f < numfaces; f++)
	{
		vector n = faces[f].planenormal;
		float d = faces[f].planedist + EPSILON; //epsilon to cover precision issues
		for (int p = 0; p < numpoints; p++)
		{
			if (points[p] * n > d)
			{
				result++;
				break;
			}
		}
	}

	return result;
};

//returns the resulting brush id
int(int model, int brush1, int brush2, int face1, int face2) mergebrushes =
{
	int extrafaces;
	float found = FALSE;
	brushface_t *fa;
	brushface_t *infa;
	int i;

	if (brush1 == brush2)
	{
		print("cannot merge brush with itself\n");
		return 0;
	}
	if (!brush1 || !brush2)
	{
		print("no brush targetted\n");
		return 0;
	}

	tmp_numfaces = brush_get(model, brush1, tmp_faces, tmp_faces.length, &tmp_contents);
	infa = &tmp_faces[tmp_numfaces];
	extrafaces = brush_get(model, brush2, &tmp_faces[tmp_numfaces], tmp_faces.length-tmp_numfaces, &tmp_contents);

	//merge the two sets of planes together.
	for(infa = &tmp_faces[tmp_numfaces]; extrafaces > 0; infa++, extrafaces--)
	{
		for (fa = tmp_faces, i = 0; i < tmp_numfaces; i++, fa++)
		{
//fixme: needs some tolerance / epsilon
			if (fa->planenormal == -infa->planenormal && fa->planedist == -infa->planedist)
			{
				//inverted. this is the plane we're merging over, so strip it out
				memcpy(fa, &fa[1], sizeof(brushface_t) * ((tmp_numfaces-i)-1));
				tmp_numfaces--;
				i--;
				fa--;
				found = TRUE;
				break;
			}
			else if (fa->planenormal * infa->planenormal > 0.999 && fa->planedist == infa->planedist)
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
#if 0
		//try to find a surface to move to match to the given plane
		float val;
		float bestval = -0.707;	//must be within 45 degrees
		int bestface = -1;
		tmp_numfaces = brush_get(model, brush1, tmp_faces, tmp_faces.length, &tmp_contents);
		brush_get(model, brush2, &tmp_faces[tmp_numfaces], tmp_faces.length-tmp_numfaces, &tmp_contents);
		infa = &tmp_faces[tmp_numfaces + face2-1i];
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

		//	if (isconcave(tmp_faces, tmp_numfaces))
		//	{
		//		print("Resulting brush would be concave\n");
		//		return 0;
		//	}

			brush_history_delete(model, brush1);
			return brush_history_create(model, tmp_faces, tmp_numfaces, tmp_contents);
		}
		else
#endif
			return 0;
	}
	else
	{
		vector *points = memalloc(sizeof(vector)*64*64);
		int numpoints = 0, f;
		for(f = 0; (i = brush_getfacepoints(model, brush1, ++f, points+numpoints, 64*64-numpoints)); )
			numpoints += i;
		for(f = 0; (i = brush_getfacepoints(model, brush2, ++f, points+numpoints, 64*64-numpoints)); )
			numpoints += i;

		if (isconcave(tmp_faces, tmp_numfaces, points, numpoints))
		{
			print("Resulting brush would be concave\n");
			memfree(points);
			return 0;
		}
		memfree(points);

	//FIXME: verify that no planes got dropped, as this indicates that the region wasn't convex, and we probably just destroyed the entire thing.
		brush_history_delete(model, brush1);
		brush_history_delete(model, brush2);

		return brush_history_create(model, tmp_faces, tmp_numfaces, tmp_contents);
	}
};

void(int id) DrawEngineBrushWireframe =
{
	const vector col = '1 0 0';
	for(int facenum = 0;;)
	{
		int points = brush_getfacepoints(selectedbrushmodel, id, ++facenum, &facepoints[0], facepoints.length);
		if (!points)
			break;	//end of face list, I guess

		R_BeginPolygon("chop");
		R_PolygonVertex(facepoints[0], '0 0', col, 1);
		for (int point = 1; point < points; point++)
		{
			R_PolygonVertex(facepoints[point], '0 0', col, 1);
			R_EndPolygon();
			R_PolygonVertex(facepoints[point], '0 0', col, 1);
		}
		R_PolygonVertex(facepoints[0], '0 0', col, 1);
		R_EndPolygon();
	}
};

void(brushface_t *faces, int numfaces, string shader, vector col, float alpha) DrawQCBrushWireframe =
{
	int f;
	int point, points;
	for(f = 0; f < numfaces;)
	{
		points = brush_calcfacepoints(++f, faces, numfaces, facepoints, facepoints.length);
		if (!points)
			continue;	//should probably warn somehow about this
		R_BeginPolygon(shader);
		R_PolygonVertex(facepoints[0], '0 0', col, alpha);
		for (point = 0; point < points-1; )
		{
			point++;
			R_PolygonVertex(facepoints[point], '0 0', col, alpha);
			R_EndPolygon();
			R_PolygonVertex(facepoints[point], '0 0', col, alpha);
		}
		R_PolygonVertex(facepoints[0], '0 0', col, alpha);
		R_EndPolygon();
	}
};
void(brushface_t *faces, int numfaces, string shader, vector col, float alpha) DrawQCBrushSolid =
{
	int f;
	int point, points;
	for(f = 0; f < numfaces;)
	{
		points = brush_calcfacepoints(++f, faces, numfaces, facepoints, facepoints.length);
		if (!points)
			continue;	//should probably warn somehow about this
		R_BeginPolygon(shader);
		for (point = 0; point < points; point++)
			R_PolygonVertex(facepoints[point], '0 0', col, alpha);
		R_EndPolygon();
	}
};
void(brushface_t *faces, int numfaces, vector col, float alpha) DrawQCBrushTextured =
{
	int f;
	int point, points;
	for(f = 0; f < numfaces; f++)
	{
		points = brush_calcfacepoints(1+f, faces, numfaces, facepoints, facepoints.length);
		if (points)
		{
			//this is unfortunate. the built in shaders expect to use lightmaps. we don't have those.
			//because lightmaps are special things, we end up in a real mess. so lets just make sure there's a shader now, because we can.
			//FIXME: we don't manage to pick up the size of the original wad image
			shaderforname(faces[f].shadername,
				sprintf("{"
					"{\n"
						"map \"%s\"\n"
						"rgbgen vertex\n"
						"alphagen vertex\n"
					"}\n"
				"}", faces[f].shadername));

			vector sz = drawgetimagesize(faces[f].shadername);
			R_BeginPolygon(faces[f].shadername);
			for (point = 0; point < points; point++)
				R_PolygonVertex(facepoints[point], [(facepoints[point] * faces[f].sdir + faces[f].sbias)/sz_x, (facepoints[point] * faces[f].tdir + faces[f].tbias)/sz_y], col, alpha);
			R_EndPolygon();
		}
	}
};

void(vector *p, int points, string shader, vector col, float alpha) DrawAxisExtensions =
{
	R_BeginPolygon(shader);
	for (int point = 0; point < points; point++)
	{
		R_PolygonVertex(p[point] + [ 64, 0, 0], '0 0', col, alpha);
		R_PolygonVertex(p[point] + [-64, 0, 0], '0 0', col, alpha);
		R_EndPolygon();
		R_PolygonVertex(p[point] + [0,  64, 0], '0 0', col, alpha);
		R_PolygonVertex(p[point] + [0, -64, 0], '0 0', col, alpha);
		R_EndPolygon();
		R_PolygonVertex(p[point] + [0, 0,  64], '0 0', col, alpha);
		R_PolygonVertex(p[point] + [0, 0, -64], '0 0', col, alpha);
		R_EndPolygon();
	}
};

static float(vertsoup_t *soup, int *idx, __inout vector norm, __inout float dist) planenormal =
{
	vector v1 = soup->p[idx[0]];
	vector v2 = soup->p[idx[1]];
	vector v3 = soup->p[idx[2]];
	vector d1 = v3 - v1;
	vector d2 = v2 - v1;
	vector d3 = v3 - v2;
	norm = normalize(crossproduct(d1, d2));
	dist = norm * v1;

	if (!d1 || !d2 || !d3 || !norm)
		return FALSE;
	return TRUE;
};

void(vertsoup_t *soup, int drawit) Rebrushify =
{
	brushface_t faces[64];
	int numfaces, f, point;
	string shader = 0;
	
	vector n;
	float d;
		
	
	tmp_numfaces = brush_get(selectedbrushmodel, selectedbrush, tmp_faces, tmp_faces.length, &tmp_contents);
	
	//if any triangle's neighbour opposes it, reform the edge across the quad to try to keep it convex.
	for(point = 0; point+2 < soup->numidx; point+=3)
	{
		int p1 = soup->i[point+0];
		int p2 = soup->i[point+1];
		int p3 = soup->i[point+2];
		if (!planenormal(soup, &soup->i[point], n, d))
			continue;	//degenerate
		d += EPSILON;
		for(f = point+3; f+2 < soup->numidx; f+=3)
		{
			int o1 = soup->i[f+0];
			int o2 = soup->i[f+1];
			int o3 = soup->i[f+2];
			int o;
p1p2edge:
			if (o2 == p1 && o1 == p2)
				o = o3;
			else if (o3 == p1 && o2 == p2)
				o = o1;
			else if (o1 == p1 && o3 == p2)
				o = o2;
			else
				goto p2p3edge;
			if (soup->p[o] * n > d)
			{
				soup->i[f+0] = p3;
				soup->i[f+1] = o;
				soup->i[f+2] = p2;
				p2 = o;
			}
			continue;
p2p3edge:
			if (o2 == p2 && o1 == p3)
				o = o3;
			else if (o3 == p2 && o2 == p3)
				o = o1;
			else if (o1 == p2 && o3 == p3)
				o = o2;
			else
				goto p3p1edge;
			if (soup->p[o] * n > d)
			{
				soup->i[f+0] = p1;
				soup->i[f+1] = o;
				soup->i[f+2] = p3;
				p3 = o;
			}
			continue;
p3p1edge:
			if (o2 == p3 && o1 == p1)
				o = o3;
			else if (o3 == p3 && o2 == p1)
				o = o1;
			else if (o1 == p3 && o3 == p1)
				o = o2;
			else
				continue;
			if (soup->p[o] * n > d)
			{
				soup->i[f+0] = p2;
				soup->i[f+1] = o;
				soup->i[f+2] = p1;
				p1 = o;
			}
			continue;
		}
		
		soup->i[point+0] = p1;
		soup->i[point+1] = p2;
		soup->i[point+2] = p3;
	}
	
	//generate the plane info
	numfaces = 0;
	for(point = 0; point+2 < soup->numidx; point+=3)
	{
		if (!planenormal(soup, &soup->i[point], n, d))
			continue;	//a degenerate triangle is one that probably got merged or something

		for (f = 0; f < numfaces; f++)
		{
			if (faces[f].planenormal == n && faces[f].planedist == d)
				break;
		}
		if (f < numfaces)
			continue;	//duplicate plane
			
		for (f = 0; f < tmp_numfaces; f++)
		{
			if (tmp_faces[f].planenormal * n > 0.999) //stupid inprecisions
			{
				if (numfaces == 64)
					return;
					
				//note that we only care about the normal, not the dist. this means you can move the entire face forward+back without loosing textures.
				faces[numfaces].shadername = shader = tmp_faces[f].shadername;
				faces[numfaces].planenormal = n;
				faces[numfaces].planedist = d;
				faces[numfaces].sdir = tmp_faces[f].sdir;
				faces[numfaces].sbias = tmp_faces[f].sbias;
				faces[numfaces].tdir = tmp_faces[f].tdir;
				faces[numfaces].tbias = tmp_faces[f].tbias;
				numfaces++;
				break;
			}
		}
		if (f < tmp_numfaces)
			continue;	//matched a plane in the original brush
			
		if (numfaces == 64)
			return;
			
		//FIXME: find aproximate faces to give corners or something

		//okay, it appears to be new. that's a pain.
		faces[numfaces].shadername = 0;
		faces[numfaces].planenormal = n;
		faces[numfaces].planedist = d;
		reset_texturecoords(&faces[numfaces]);
		numfaces++;
	}
	
	//any surface without a texture/shader yet should inherit one from some other surface
	if (!shader)
		shader = autocvar_ca_newbrushtexture;
	for(f = 0; f < numfaces; f++)
	{
		if (!faces[f].shadername)
			faces[f].shadername = shader;
	}

	int internals = 0;
	//If we have a point outside a plane, then we know we have a concave volume.
	//chop the points 
	for(f = 0; f < numfaces; f++)
	{
		n = faces[f].planenormal;
		d = faces[f].planedist;
		for (point = 0; point < soup->numidx; point++)	//would ideally use points, but I want to cover dead ones
		{
			if (soup->p[soup->i[point]] * n > d + EPSILON)
			{
				internals++;
				break;
			}
		}
	}
//	cprint(sprintf("%i internal splits, %i faces\n", internals, numfaces));
	
	if (numfaces <= 3)
		return;	//can't possibly be valid.
	
	if (drawit)
	{
		//draw it wireframe WITH depth testing
		//DrawQCBrushWireframe(faces, numfaces, "terrainedit", '1 0 0', 1);
		
		//draw textured preview (yay for block lighting)
		DrawQCBrushTextured(faces, numfaces, '0.7 0.7 0.7', 1);
		
		//draw it wireframe without depth testing
		DrawQCBrushWireframe(faces, numfaces, "chop", internals?'1 0 0':'0 0 1', 1);
	}
	else
	{
		if (selectedbrush)
			brush_history_delete(selectedbrushmodel, selectedbrush);
		selectedbrush = brush_history_create(selectedbrushmodel, faces, numfaces, 1i);
		selectedbrushface = 0;
		
		soup->numidx = 0;
		soup->numverts = 0;
	}	
};

//take a brush apart and generate trisoup from it.
//note that the trisoup does not have textures. they will be reconciled when reforming the brush.
void(void) Debrushify =
{
	int *ni;
	int i, j, k;
	int points;
	vector p;
	vector *np;
	static int fi[64];
	vertedit.numidx = 0;
	vertedit.numverts = 0;
	for (i = 0; ; )
	{
		points = brush_getfacepoints(selectedbrushmodel, selectedbrush, ++i, facepoints, facepoints.length);
		if (!points)
			break;
		
		//allocate a few new indexes
		ni = memalloc(sizeof(*ni) * (vertedit.numidx+3*(points-2)));
		memcpy(ni, vertedit.i, sizeof(*ni) * vertedit.numidx);
		memfree(vertedit.i);
		vertedit.i = ni;
		ni += vertedit.numidx;
		vertedit.numidx += 3*(points-2);

		for (j = 0; j < points; j++)
		{
			p = facepoints[j];
			p_x = floor(p_x + 0.5);	//gah, bloomin inprecision.
			p_y = floor(p_y + 0.5);
			p_z = floor(p_z + 0.5);
			for (k = 0; k < vertedit.numverts; k++)
			{	//try to be nice and re-use verts.
				if (vertedit.p[k] == p)
					break;
			}
			if (k == vertedit.numverts)
			{
				//if it wasn't found, we need to allocate a new one
				np = memalloc(sizeof(*np) * (vertedit.numverts+1));
				memcpy(np, vertedit.p, sizeof(*np) * vertedit.numverts);
				memfree(vertedit.p);
				vertedit.p = np;
				np += vertedit.numverts;
				vertedit.numverts += 1;
				*np = p;
			}
			fi[j] = k;
		}
		for (j = 2; j < points; j++)
		{
			*ni++ = fi[0];
			*ni++ = fi[j-1];
			*ni++ = fi[j];
		}
	}
};

//determines only the various points of the brush, without duplicates. doesn't care about indexes.
void(brushface_t *faces, int numfaces) DebrushifyLite =
{
	int points, k;
	vector p;
	vector *np;
	int fi[64];
	vertedit.numidx = 0;
	vertedit.numverts = 0;
	for (int i = 0; ; )
	{
		points = brush_calcfacepoints(++i, faces, numfaces, facepoints, facepoints.length);
		if (!points)
			break;
		
		for (int j = 0; j < points; j++)
		{
			p = facepoints[j];
			p_x = floor(p_x + 0.5);	//gah, bloomin inprecision.
			p_y = floor(p_y + 0.5);
			p_z = floor(p_z + 0.5);
			for (k = 0; k < vertedit.numverts; k++)
			{	//try to be nice and re-use verts.
				if (vertedit.p[k] == p)
					break;
			}
			if (k == vertedit.numverts)
			{
				//if it wasn't found, we need to allocate a new one
				np = memalloc(sizeof(*np) * (vertedit.numverts+1));
				memcpy(np, vertedit.p, sizeof(*np) * vertedit.numverts);
				memfree(vertedit.p);
				vertedit.p = np;
				np += vertedit.numverts;
				vertedit.numverts += 1;
				*np = p;
			}
			fi[j] = k;
		}
	}
};

void(vector org, vector ang) editor_brushes_simpleclone =
{
	vector midpoint;
	if (!selectedbrush)
		return;

	tmp_numfaces = brush_get(selectedbrushmodel, selectedbrush, tmp_faces, tmp_faces.length, &tmp_contents);
	if (ang != '0 0 0')
	{
		brush_getfacepoints(selectedbrushmodel, selectedbrush, 0, &midpoint, 1);
		brushface_translate(tmp_faces, tmp_numfaces, -midpoint);
		makevectors(ang);
		brushface_rotate(tmp_faces, tmp_numfaces);
		brushface_translate(tmp_faces, tmp_numfaces, midpoint + org);
	}
	else
		brushface_translate(tmp_faces, tmp_numfaces, org);
	selectedbrush = brush_history_create(selectedbrushmodel, tmp_faces, tmp_numfaces, tmp_contents);
};

void() brushedit_subtract =
{
	int discard;
	vector selnormals[tmp_faces.length];
	float seldists[tmp_faces.length];
	
	int planecount = brush_get(selectedbrushmodel, selectedbrush, tmp_faces, tmp_faces.length, &tmp_contents);		
	for (int i = 0; i < planecount; i++)
	{
		selnormals[i] = tmp_faces[i].planenormal;
		seldists[i] = tmp_faces[i].planedist;
	}
	int numbrushes = brush_findinvolume(selectedbrushmodel, selnormals, seldists, planecount, brushlist, __NULL__, brushlist.length);

	while (numbrushes --> 0)
	{
		int br = brushlist[numbrushes];
		if (br == selectedbrush)
			continue;

		int counto = brush_get(selectedbrushmodel, br, tmp_faces, tmp_faces.length, &tmp_contents);
		int counts = counto + brush_get(selectedbrushmodel, selectedbrush, tmp_faces+counto, tmp_faces.length-counto, &discard);

		brush_history_delete(selectedbrushmodel, br);
		while(counts --> counto)
		{
			//only consider the resulting brush if the new face actually contributed anything.
			//this reduces dupes.
			if (brush_calcfacepoints(1+counts, tmp_faces, counts+1, facepoints, facepoints.length))
			{
				//determine the brush defined by this plane
				tmp_faces[counts].planenormal *= -1;
				tmp_faces[counts].planedist *= -1;
				brush_history_create(selectedbrushmodel, tmp_faces, counts+1, tmp_contents);
			}
		}
	}
};

void() brushedit_resettextures =
{
	int planecount = brush_get(selectedbrushmodel, selectedbrush, tmp_faces, tmp_faces.length, &tmp_contents);		
	for (int i = 0; i < planecount; i++)
		reset_texturecoords(&tmp_faces[i]);

	selectedbrush = brush_history_edit(selectedbrushmodel, selectedbrush, tmp_faces, planecount, tmp_contents);
};

//when we're drawing sideviews, we normally need them wireframe in order to actually see anything
static void(vector sizes) drawwireframeview =
{
	//figure out the planes that we care about.
	vector org = getviewprop(VF_ORIGIN);
	vector vaxis[6];
	vaxis[0] = v_right;
	vaxis[1] = v_up;
	vaxis[2] = v_forward;
	for (int i = 0; i < 3; i++)
		dists[i] = (org*vaxis[i]) + sizes[i];
	for (int i = 0; i < 3; i++)
	{
		vaxis[i+3] = -vaxis[i];
		dists[i+3] = (org*vaxis[i+3]) + sizes[i];
	}
		
	int numbrushes = brush_findinvolume(selectedbrushmodel, vaxis, dists, dists.length, &brushlist[0], __NULL__, brushlist.length);
	
	//this can be a bit slow with lots of brushes. would be nice to batch them a bit better.
	while(numbrushes --> 0)
		DrawEngineBrushWireframe(brushlist[numbrushes]);
	setviewprop(VF_DRAWWORLD, FALSE);
};

void(vector mousepos) editor_brushes_addentities =
{
	vector col = '0 0 0';
	int points, point;
	int facenum;
	float intensity = (sin(gettime(5)*4)+1)*0.05;
	vector displace, tmp;
	float bestdist, dist;
	vector mid;
	
	autocvar_ca_brush_view = fabs(autocvar_ca_brush_view) % 4f;
	if (autocvar_ca_brush_view)
	{
		vector vmn = (vector)getproperty(VF_MIN);
		vector vsz = (vector)getproperty(VF_SIZE);
		vector ang = vectoangles([autocvar_ca_brush_view==1,autocvar_ca_brush_view==2,autocvar_ca_brush_view==3]);
		vector fov;
		if (vsz_x > vsz_y)
			fov = [autocvar_ca_brush_viewsize, autocvar_ca_brush_viewsize * (vsz_y/vsz_x), 8192];
		else
			fov = [autocvar_ca_brush_viewsize * (vsz_x/vsz_y), autocvar_ca_brush_viewsize, 8192];
		makevectors(ang);
		drawfill(vmn, vsz, '0 0 0', 1, 0);
		setviewprop(VF_VIEWENTITY, 0);
		setviewprop(VF_PERSPECTIVE, FALSE);
		setviewprop(VF_ANGLES, ang);
		setviewprop(VF_MAXDIST, fov_z*0.5);
		setviewprop(VF_MINDIST, -fov_z*0.5);	//I'm not entirely sure where the near clip plane should be. oh well.
		if (vsz_x > vsz_y)
			setviewprop(VF_FOV, fov);
		else
			setviewprop(VF_FOV, fov);

		if (curmousepos_x >= vmn_x && curmousepos_x <= vmn_x+vsz_x && curmousepos_y >= vmn_y && curmousepos_y <= vmn_y+vsz_y)
		{
			mousefar = unproject(curmousepos + '0 0 1');
			mousenear = unproject(curmousepos);
		}

		drawwireframeview(fov*0.5);
	}
	else
		makevectors(input_angles);
	
	if ((mousetool == BT_VERTEXEDIT || mousetool == BT_CREATEDRAG || brushtool == BT_PUSHFACE || brushtool == BT_CLONEDISPLACE || brushtool == BT_MOVE || brushtool == BT_MOVETEXTURE) && bt_points)
	{
		vector dir = v_forward;
		if (!altdown)	//if alt is pressed, we'll axialize later.
			dir = axialize(dir);
		tmp = normalize(mousefar-mousenear) + mousenear;
		tmp = planelinepoint(mousenear, tmp, dir, bt_point[0] * dir); //find where the cursor impacts the screen grid (moved along the view vector to match where the drag was last frame)
		displace = tmp - bt_point[0];
		if (mousetool != BT_VERTEXEDIT && mousetool != BT_CREATEDRAG)
			displace = brush_snappoint(displace);
		if (altdown)	//if alt is held, rotate the move by 90 degrees, and move ONLY in the axial dir instead of the screen's xy plane
			bt_displace_z = (displace * v_right + displace * v_up);
		else
		{
			bt_displace_x = displace * axialize(v_right);
			bt_displace_y = displace * axialize(v_up);
		}
		displace = bt_displace_x * axialize(v_right) + bt_displace_y * axialize(v_up) - bt_displace_z * axialize(v_forward);
		
		if (mousetool == BT_VERTEXEDIT)
			displace = brush_snappoint(displace+bt_point[0]);
	}
	else if (brushtool == BT_ROTATE)
	{
		te_lightning2(world, bt_point[0], bt_point[0]+-bt_displace);
		col = vectoangles(bt_point[0]+bt_displace - tmp) - vectoangles(bt_point[0] - tmp);
		col = channelizeangle([col_x*-1,col_y,col_z]);
	}
	
	if (vertedit.numidx && mousetool != BT_VERTEXEDIT)
	{
		vertedit.numidx = 0;
		vertedit.numverts = 0;
	}

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

	if (mousetool == BT_VERTEXEDIT && vertedit.numidx)
	{
#if 0
		//draw it solid WITH depth testing
		R_BeginPolygon("terrainedit");
		for(point = 0; point+2 < vertedit.numidx; point+=3)
		{
			col = '0 0 0.1';
			R_PolygonVertex(vertedit.p[vertedit.i[point+0]], '0 0', col, 1);
			R_PolygonVertex(vertedit.p[vertedit.i[point+1]], '0 0', col, 1);
			R_PolygonVertex(vertedit.p[vertedit.i[point+2]], '0 0', col, 1);
			R_EndPolygon();
		}
#endif
#if 1
		//draw the wires (no depth)
		R_BeginPolygon("chop");
		for(point = 0; point+2 < vertedit.numidx; point+=3)
		{
			col = '0 0.1 0';
			R_PolygonVertex(vertedit.p[vertedit.i[point+0]], '0 0', col, 1);
			R_PolygonVertex(vertedit.p[vertedit.i[point+1]], '0 0', col, 1);
			R_EndPolygon();
			R_PolygonVertex(vertedit.p[vertedit.i[point+1]], '0 0', col, 1);
			R_PolygonVertex(vertedit.p[vertedit.i[point+2]], '0 0', col, 1);
			R_EndPolygon();
			R_PolygonVertex(vertedit.p[vertedit.i[point+2]], '0 0', col, 1);
			R_PolygonVertex(vertedit.p[vertedit.i[point+0]], '0 0', col, 1);
			R_EndPolygon();
		}
#endif

		DrawAxisExtensions(vertedit.p, vertedit.numverts, "terrainedit", '1 0.3 0.3', 1);

		if (mousedown)
		{
			if (bt_points == 1 && vertedit.selectedvert1 != -1)
			{
				if (vertedit.selectedvert2 != -1)
					vertedit.p[vertedit.selectedvert2] = displace + bt_point[2];
				vertedit.p[vertedit.selectedvert1] = displace + bt_point[1];
			}
		}
		else
			bt_points = 0;

		Rebrushify(&vertedit, TRUE);
	}
	else if (brushtool == BT_CREATE)
	{
		if (bt_points > 2)
		{
			tmp_numfaces = BrushFromPoints(tmp_faces, tmp_faces.length, bt_point, bt_points, autocvar_ca_newbrushheight);
			
			DrawQCBrushWireframe(tmp_faces, tmp_numfaces, "chop", '1 0 0', 1);
		}
	}
	else if (mousetool == BT_CREATEDRAG)
	{
		if (bt_points)
		{
			displace -= axialize(v_forward)*autocvar_ca_grid;

			mid = bt_point[0];
			displace = brush_snappoint(mid + displace);
			mid = brush_snappoint(mid);
			displace = displace - mid;

			tmp_faces[0].planenormal = '1 0 0';
			tmp_faces[1].planenormal = '-1 0 0';
			tmp_faces[2].planenormal = '0 1 0';
			tmp_faces[3].planenormal = '0 -1 0';
			tmp_faces[4].planenormal = '0 0 1';
			tmp_faces[5].planenormal = '0 0 -1';
			tmp_faces[0].planedist = mid[0];
			tmp_faces[1].planedist = -mid[0];
			tmp_faces[2].planedist = mid[1];
			tmp_faces[3].planedist = -mid[1];
			tmp_faces[4].planedist = mid[2];
			tmp_faces[5].planedist = -mid[2];
			tmp_numfaces = 6;
			tmp_contents = 1;
			
			tmp_faces[0 + (displace_x<0.0)].planedist += fabs(displace_x);
			tmp_faces[2 + (displace_y<0.0)].planedist += fabs(displace_y);
			tmp_faces[4 + (displace_z<0.0)].planedist += fabs(displace_z);
			
			for (facenum = 0; facenum < tmp_numfaces; facenum++)
			{
				tmp_faces[facenum].shadername = autocvar_ca_newbrushtexture;
				reset_texturecoords(&tmp_faces[facenum]);
			}

			DrawQCBrushWireframe(tmp_faces, tmp_numfaces, "chop", '1 0 0', 1);

			if (!mousedown)
			{
				selectedbrush = brush_history_create(selectedbrushmodel, tmp_faces, tmp_numfaces, tmp_contents);
				brush_selected(selectedbrushmodel, selectedbrush, selectedbrushface, TRUE);
				bt_points = 0;
				mousetool = BT_NONE;
			}
		}
		else
			mousedown = FALSE;
	}
	else if ((mousetool == BT_PUSHFACE || mousetool == BT_MOVETEXTURE || mousetool == BT_MOVE || mousetool == BT_CLONEDISPLACE || mousetool == BT_ROTATE) && bt_points)
	{
		int oldselectedbrushcount = selectedbrushcount;
		int *oldselectedbrushes = memalloc(sizeof(int)*selectedbrushcount);
		memcpy(oldselectedbrushes, selectedbrushes, selectedbrushcount*sizeof(int));
		for (int sb = 0; sb < oldselectedbrushcount; sb++)
		{
			int brush = oldselectedbrushes[sb];
			if (!brush)
				continue;

			tmp_numfaces = brush_get(selectedbrushmodel, brush, tmp_faces, tmp_faces.length, &tmp_contents);

			if (mousetool == BT_PUSHFACE && brush == selectedbrush)
				tmp_faces[selectedbrushface-1].planedist += tmp_faces[selectedbrushface-1].planenormal * displace;
			else if (mousetool == BT_MOVETEXTURE && brush == selectedbrush)
			{
				tmp_faces[selectedbrushface-1].sbias -= tmp_faces[selectedbrushface-1].sdir * displace;
				tmp_faces[selectedbrushface-1].tbias -= tmp_faces[selectedbrushface-1].tdir * displace;
			}
			else if (mousetool == BT_MOVE || mousetool == BT_CLONEDISPLACE)
				brushface_translate(tmp_faces, tmp_numfaces, displace);
			else if (mousetool == BT_ROTATE)
			{
				//find the brush's middle (based on its bbox)
				brush_getfacepoints(selectedbrushmodel, brush, 0, &tmp, 1);

				makevectors(col);

				//move it so its pivot is at the origin
				brushface_translate(tmp_faces, tmp_numfaces, -tmp);
				//rotate it
				brushface_rotate(tmp_faces, tmp_numfaces);
				//reposition it around its pivot again
				brushface_translate(tmp_faces, tmp_numfaces, tmp);
			}
			else
				continue;

			if (mousetool == BT_MOVETEXTURE)
			{
				points = brush_calcfacepoints(selectedbrushface, tmp_faces, tmp_numfaces, facepoints, facepoints.length);
				if (points)
				{
					//this is unfortunate. the built in shaders expect to use lightmaps. we don't have those.
					//because lightmaps are special things, we end up in a real mess. so lets just make sure there's a shader now, because we can.
					shaderforname(tmp_faces[selectedbrushface-1].shadername,
						sprintf("{"
							"{\n"
								"map \"%s.lmp\"\n"
								"rgbgen vertex\n"
								"alphagen vertex\n"
							"}\n"
						"}", tmp_faces[selectedbrushface-1].shadername));

					col = '0.7 0.7 0.7'; //fullbright is typically TOO bright. overbrights? meh!
					vector sz = drawgetimagesize(tmp_faces[selectedbrushface-1].shadername);
					R_BeginPolygon(tmp_faces[selectedbrushface-1].shadername);
					for (point = 0; point < points; point++)
						R_PolygonVertex(facepoints[point] + tmp_faces[selectedbrushface-1].planenormal*0.1, [(facepoints[point] * tmp_faces[selectedbrushface-1].sdir + tmp_faces[selectedbrushface-1].sbias)/sz_x, (facepoints[point] * tmp_faces[selectedbrushface-1].tdir + tmp_faces[selectedbrushface-1].tbias)/sz_y], col, 1);
					R_EndPolygon();
				}
			}
			else
			{
				//draw it wireframe
				for(facenum = 0; facenum < tmp_numfaces;)
				{
					points = brush_calcfacepoints(++facenum, tmp_faces, tmp_numfaces, facepoints, facepoints.length);
					if (!points)
						continue;	//should probably warn somehow about this
					//should we use two colour channels? one depth one not?
					if (facenum == selectedbrushface)
						col = '1 0 0';
					else
						col = '0 0.5 0';
					R_BeginPolygon("chop");
					R_PolygonVertex(facepoints[0], '0 0', col, 1);
					for (point = 0; point < points-1; )
					{
						point++;
						R_PolygonVertex(facepoints[point], '0 0', col, 1);
						R_EndPolygon();
						R_PolygonVertex(facepoints[point], '0 0', col, 1);
					}
					R_PolygonVertex(facepoints[0], '0 0', col, 1);
					R_EndPolygon();
				}
			}

			//draw it wireframe WITH depth testing
			DrawQCBrushWireframe(tmp_faces, tmp_numfaces, "terrainedit", '0 0 1', 1);

			DebrushifyLite(tmp_faces, tmp_numfaces);
			DrawAxisExtensions(vertedit.p, vertedit.numverts, "terrainedit", '0 0 0.3', 1);

			if (!mousedown)
			{
				if (mousetool == BT_CLONEDISPLACE)	//doesn't affect the original brush.
				{
					if (displace*displace > 1)
						selectedbrush = brush_history_create(selectedbrushmodel, tmp_faces, tmp_numfaces, tmp_contents);
				}
				else
					selectedbrush = brush_history_edit(selectedbrushmodel, brush, tmp_faces, tmp_numfaces, tmp_contents);
				brush_selected(selectedbrushmodel, brush, selectedbrushface, TRUE);
				bt_points = 0;
				mousetool = BT_NONE;
			}
		}
		memfree(oldselectedbrushes);
	}
	else if (brushtool == BT_SLICE && bt_points == 3)
	{
		tmp_numfaces = brush_get(selectedbrushmodel, selectedbrush, tmp_faces, tmp_faces.length, &tmp_contents);
		tmp_faces[tmp_numfaces].planenormal = normalize(crossproduct(bt_point[2] - bt_point[0], bt_point[1] - bt_point[0]));
		tmp_faces[tmp_numfaces].planedist = bt_point[0] * tmp_faces[tmp_numfaces].planenormal;

		//draw it wireframe
		DrawQCBrushWireframe(tmp_faces, tmp_numfaces+1, "chop", '1 0 0', 1);

		//flip the split
		tmp_faces[tmp_numfaces].planenormal *= -1;
		tmp_faces[tmp_numfaces].planedist *= -1;

		//draw the other side wireframe
		DrawQCBrushWireframe(tmp_faces, tmp_numfaces+1, "chop", '0 1 0', 1);
	}
	else if (selectedbrush)
	{
		if (brushtool == BT_PUSHFACE)
		{	//selected face (not brush) follows cursor when we're not actively dragging a face
			for(facenum = 0;;)
			{
				points = brush_getfacepoints(selectedbrushmodel, selectedbrush, ++facenum, facepoints, facepoints.length);
				if (!points)
					break;	//end of face list, I guess

				mid = 0;
				for (point = 0; point < points; point++)
					mid += facepoints[point];	//FIXME: find the centroid for greater accuracy
				mid = project(mid * (1.0/points));
				dist = (mousepos_x - mid_x) * (mousepos_x - mid_x) + (mousepos_y - mid_y) * (mousepos_y - mid_y);
				if (facenum == 1 || dist < bestdist)
				{
					bestdist = dist;
					selectedbrushface = facenum;
				}	
			}
		}
		
		//draw all selected brush faces.
		for (int sb = 0; sb < selectedbrushcount; sb++)
		{
			int brush = selectedbrushes[sb];
			for(facenum = 0;;)
			{
				points = brush_getfacepoints(selectedbrushmodel, brush, ++facenum, facepoints, facepoints.length);
				if (!points)
					break;	//end of face list, I guess

				if (facenum == selectedbrushface && brush == selectedbrush)
					col = [0,intensity,0];
				else
					col = [intensity,0,0];

				R_BeginPolygon("terrainedit");
				for (point = 0; point < points; point++)
					R_PolygonVertex(facepoints[point], '0 0', col, 1);
				R_EndPolygon();
			}
		}
		//now draw wireframe
		DrawEngineBrushWireframe(selectedbrush);
		
		tmp_numfaces = brush_get(selectedbrushmodel, selectedbrush, tmp_faces, tmp_faces.length, &tmp_contents);
		DebrushifyLite(tmp_faces, tmp_numfaces);
		DrawAxisExtensions(vertedit.p, vertedit.numverts, "terrainedit", '0 0 0.3', 1);
	}
//	editor_drawbbox(selectedbrush);

	vector t = mousefar;
	vector o = mousenear;
	if (vlen(o - t) > 8192 && !autocvar_ca_brush_view)
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
	if (brushtool == BT_CREATE)
		bt_point[showpoints++] = brush_snappoint(trace_endpos);

	//FIXME: if slicing, draw the intersection

	if (showpoints > 1)
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
	int point;
	vector mid;
	float dist, bestdist;
	if (vertedit.numidx)
	{
		if (!mousedown)
		{
			for(vertedit.selectedvert1 = -1, vertedit.selectedvert2 = -1, point = 0, bestdist = 16*16; point < vertedit.numverts; point++)
			{
				mid = project(vertedit.p[point]);
				dist = (mousepos_x - mid_x) * (mousepos_x - mid_x) + (mousepos_y - mid_y) * (mousepos_y - mid_y);
				if (dist < bestdist && mid_z > 0)
				{
					bestdist = dist;
					vertedit.selectedvert1 = point;
				}	
			}
			for(point = 0; point < vertedit.numidx; point+=3)
			{
				mid = project(0.5*(vertedit.p[vertedit.i[point+0]] + vertedit.p[vertedit.i[point+1]]));
				dist = (mousepos_x - mid_x) * (mousepos_x - mid_x) + (mousepos_y - mid_y) * (mousepos_y - mid_y);
				if (dist < bestdist && mid_z > 0)
				{
					bestdist = dist;
					vertedit.selectedvert1 = vertedit.i[point+0];
					vertedit.selectedvert2 = vertedit.i[point+1];
				}
				
				mid = project(0.5*(vertedit.p[vertedit.i[point+1]] + vertedit.p[vertedit.i[point+2]]));
				dist = (mousepos_x - mid_x) * (mousepos_x - mid_x) + (mousepos_y - mid_y) * (mousepos_y - mid_y);
				if (dist < bestdist && mid_z > 0)
				{
					bestdist = dist;
					vertedit.selectedvert1 = vertedit.i[point+1];
					vertedit.selectedvert2 = vertedit.i[point+2];
				}
				
				mid = project(0.5*(vertedit.p[vertedit.i[point+2]] + vertedit.p[vertedit.i[point+0]]));
				dist = (mousepos_x - mid_x) * (mousepos_x - mid_x) + (mousepos_y - mid_y) * (mousepos_y - mid_y);
				if (dist < bestdist && mid_z > 0)
				{
					bestdist = dist;
					vertedit.selectedvert1 = vertedit.i[point+2];
					vertedit.selectedvert2 = vertedit.i[point+0];
				}
			}
		}
		if (vertedit.selectedvert1 != -1)
		{
			mid = project(vertedit.p[vertedit.selectedvert1]);
			if (mid_z >= 0)
				drawfill([mid_x,mid_y] - '2 2', [4,4], [0.2,0.2,1], 1, 0);
		}
		if (vertedit.selectedvert2 != -1)
		{
			mid = project(vertedit.p[vertedit.selectedvert2]);
			if (mid_z >= 0)
				drawfill([mid_x,mid_y] - '2 2', [4,4], [0.2,0.2,1], 1, 0);
		}
		
		drawrawstring('0 32 0', "Vertex Editor", '8 8 0', '1 1 1', 1);
		return;
	}

	switch(brushtool)
	{
	case BT_CLONEDISPLACE:
		drawrawstring('0 32 0', "Clone", '8 8 0', '1 1 1', 1);
		break;
	case BT_CREATEDRAG:
		drawrawstring('0 32 0', "Drag+Create", '8 8 0', '1 1 1', 1);
		break;
	case BT_CREATE:
		drawrawstring('0 32 0', "Paint+Create", '8 8 0', '1 1 1', 1);
		break;
	case BT_SLICE:
		drawrawstring('0 32 0', "Slice Tool", '8 8 0', '1 1 1', 1);
		if (!selectedbrush)
			brushtool = BT_NONE;
		break;
	case BT_PUSHFACE:
		drawrawstring('0 32 0', "Push Face", '8 8 0', '1 1 1', 1);
		break;
	case BT_MOVE:
		drawrawstring('0 32 0', "Move Brush", '8 8 0', '1 1 1', 1);
		break;
	case BT_ROTATE:
		drawrawstring('0 32 0', "Rotate Brush", '8 8 0', '1 1 1', 1);
		break;
	case BT_MOVETEXTURE:
		drawrawstring('0 32 0', "Move Texture", '8 8 0', '1 1 1', 1);
		break;
	case BT_VERTEXEDIT:
		drawrawstring('0 32 0', "Vertex Editor", '8 8 0', '1 1 1', 1);
		break;
	}
};

#define brusheditormodes
//#append brusheditormodes brusheditormode("move", BT_MOVE)
#append brusheditormodes brusheditormode("clone", BT_CLONEDISPLACE)
#append brusheditormodes brusheditormode("draw", BT_CREATEDRAG)
#append brusheditormodes brusheditormode("rotate", BT_ROTATE)
#append brusheditormodes brusheditormode("pushface", BT_PUSHFACE)
#append brusheditormodes brusheditormode("scrolltex", BT_MOVETEXTURE)
#append brusheditormodes brusheditormode("vertex", BT_VERTEXEDIT)

void() editor_brushes_registercommands =
{
#define brusheditormode(n,v) registercommand("+brushedit_"n);registercommand("-brushedit_"n);
brusheditormodes
#undef brusheditormode
	registercommand("brushedit_undo");
	registercommand("brushedit_redo");
	registercommand("brushedit_delete");
	registercommand("brushedit_create");
	registercommand("brushedit_slice");
	registercommand("brushedit_matchface");
	registercommand("brushedit_resettexcoords");
	registercommand("brushedit_settexture");
	registercommand("brushedit_subtract");
	registercommand("brushedit_binds");
	registercommand("brushedit_binds_default");
	registercommand("+brushedit_nogrid");
	registercommand("-brushedit_nogrid");
};

float() editor_brushes_command =
{
	switch(argv(0))
	{
#define brusheditormode(n,v) case "+brushedit_"n: brushtool = v; bt_points = 0; break;  case "-brushedit_"n: if (brushtool == v) brushtool = BT_NONE; break;
brusheditormodes
#undef brusheditormode

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
	case "brushedit_slice":
		brushtool = BT_SLICE;
		bt_points = 0;
		break;
		
	case "+brushedit_nogrid": nogrid = TRUE; break;
	case "-brushedit_nogrid": nogrid = FALSE; break;

	case "brushedit_matchface":
		brushtool = BT_NONE;
		bt_points = 0;
		
		
		{
			vector t = mousefar;
			vector o = mousenear;
			int i;
			if (vlen(o - t) > 8192 && !autocvar_ca_brush_view)
				t = o + normalize(t)*8192;
		
			traceline(o, t, TRUE, world);
			
			i = mergebrushes(selectedbrushmodel, selectedbrush, trace_brush_id, selectedbrushface, trace_brush_faceid);
			if (i)
			{
				brush_selected(selectedbrushmodel, selectedbrush, -1, FALSE);
				selectedbrush = i;
				selectedbrushface = 0;
				brush_selected(selectedbrushmodel, selectedbrush, selectedbrushface, TRUE);
			}
		}
		break;
	case "brushedit_resettexcoords":
		brushedit_resettextures();
		break;
	case "brushedit_settexture":
		//IMPLEMENTME
		brushtool = BT_NONE;
		bt_points = 0;
		break;
	case "brushedit_subtract":
		brushedit_subtract();
		break;
	case "brushedit_delete":
		if (selectedbrushmodel && selectedbrush)
			brush_history_delete(selectedbrushmodel, selectedbrush);
		selectedbrush = 0;
		break;
	case "brushedit_binds_default":
		localcmd("echo Setting default brusheditor bindings\n");
		localcmd("bind ctrl+z		brushedit_undo\n");
		localcmd("bind ctrl+y		brushedit_redo\n");
		localcmd("bind ctrl+i		brushedit_create\n");
		localcmd("bind ctrl+s		brushedit_slice\n");
		localcmd("bind ctrl+m		brushedit_matchface\n");
		localcmd("bind ctrl+delete	brushedit_delete\n");
		localcmd("bind ctrl+backspace	brushedit_subtract\n");
		localcmd("bind ctrl+c		+brushedit_clone\n");
		localcmd("bind ctrl+d		+brushedit_draw\n");
		localcmd("bind ctrl+r		+brushedit_rotate\n");
		localcmd("bind ctrl+f		+brushedit_pushface\n");
		localcmd("bind ctrl+t		+brushedit_scrolltex\n");
		localcmd("bind ctrl+v		+brushedit_vertex\n");
		break;
	case "brushedit_binds":
		localcmd("conmenu \"\"\n");
		float foo = 0;
		localcmd(sprintf("menutext 0 %g \"Set Default Bindings\" \"brushedit_binds_default\"\n", foo+=8));
		foo+=8;
		localcmd(sprintf("menubind 0 %g \"brushedit_create\" \"Creation Tool\"\n", foo+=8));
		localcmd(sprintf("menubind 0 %g \"brushedit_slice\" \"Slice Tool\"\n", foo+=8));
		localcmd(sprintf("menubind 0 %g \"brushedit_delete\" \"Delete\"\n", foo+=8));
		localcmd(sprintf("menubind 0 %g \"brushedit_subtract\" \"Subtract\"\n", foo+=8));
		localcmd(sprintf("menubind 0 %g \"brushedit_undo\" \"Undo\"\n", foo+=8));
		localcmd(sprintf("menubind 0 %g \"brushedit_redo\" \"Redo\"\n", foo+=8));
		localcmd(sprintf("menubind 0 %g \"brushedit_nogrid\" \"Disable Grid\"\n", foo+=8));
		
#define brusheditormode(n,v) localcmd(sprintf("menubind 0 %g \"+brushedit_"n"\" \""n"\"\n", foo+=8));
brusheditormodes
#undef brusheditormode
		break;
	default:
		return FALSE;
	}

	//just in case.
	cvar_set("ca_show", "1");
	cvar_set("ca_editormode", ftos(MODE_BRUSHEDIT));
	return TRUE;
};

void(entity e) editor_brush_set_entity
{
//	faesf = e;
};

float(float key, float unic, vector mousepos) editor_brushes_key =
{
	brushface_t *fa;
	vector t = mousefar;
	vector o = mousenear;
	int i, p;

	if (vlen(o - t) > 8192 && !autocvar_ca_brush_view)
		t = o + normalize(t)*8192;
	if (key == K_ESCAPE)
	{
		vertedit.numidx = 0;
		vertedit.numverts = 0;
		if (brushtool)
			brushtool = BT_NONE;
		else
		{
			brush_deselectall();
		}
		return TRUE;
	}
	if (key == K_MOUSE1)
	{
		if (vertedit.numidx)
		{
			if (vertedit.selectedvert1 != -1)
			{
				mousedown = TRUE;
				mousetool = BT_VERTEXEDIT;
				if (vertedit.selectedvert2 != -1)
				{
					bt_point[0] = 0.5 * (vertedit.p[vertedit.selectedvert1] + vertedit.p[vertedit.selectedvert2]);
					bt_point[1] = vertedit.p[vertedit.selectedvert1] - bt_point[0];
					bt_point[2] = vertedit.p[vertedit.selectedvert2] - bt_point[0];
				}
				else
					bt_point[0] = vertedit.p[vertedit.selectedvert1];
					bt_point[1] = vertedit.p[vertedit.selectedvert1] - bt_point[0];
				bt_points = 1;
				bt_displace = '0 0 0';
			}
			return TRUE;
		}
		
		traceline(o, t, TRUE, world);
		float tracemodel = trace_ent.modelindex;

		if (brushtool == BT_CREATEDRAG || (brushtool == BT_PUSHFACE && selectedbrushface))
		{
			trace_brush_faceid = selectedbrushface;
			trace_brush_id = selectedbrush;
			tracemodel = selectedbrushmodel;
		}
		else if (brushtool != BT_PUSHFACE && brushtool != BT_MOVETEXTURE)
			trace_brush_faceid = 0;
			
		if (brushtool == BT_SLICE || brushtool == BT_CREATE)
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
				p = brush_getfacepoints(selectedbrushmodel, selectedbrush, 0, bbox, bbox.length);
	
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
		}
		//FIXME: selecting a brush by face should select either the front or the back. ideally depending on which one is closest to its respective face center, I suppose.
		else if (trace_brush_id != selectedbrush || selectedbrushface != trace_brush_faceid || selectedbrushmodel != tracemodel)
		{
			brush_selected(selectedbrushmodel, selectedbrush, -1, FALSE);
			if (!shiftdown)
			{
				if (!brush_isselected(trace_brush_id))
					brush_deselectall();
			}
			else if (brush_deselect(trace_brush_id))
				return TRUE;
			brush_select(trace_brush_id);
			selectedbrush = trace_brush_id;
			selectedbrushface = trace_brush_faceid;
			selectedbrushmodel = tracemodel;
			if (trace_brush_faceid)
				brush_selected(selectedbrushmodel, selectedbrush, selectedbrushface, TRUE);
			
			vertedit.numidx = 0;
			vertedit.numverts = 0;
			
			if (selectedbrushface && (brushtool == BT_PUSHFACE || brushtool == BT_MOVETEXTURE))
			{
				mousedown = TRUE;
				mousetool = brushtool;
				vertedit.selectedvert1 = -1;
				vertedit.selectedvert2 = -1;
				bt_point[0] = brush_snappoint(trace_endpos);
				bt_points = 1;
				bt_displace = '0 0 0';
			}
		}
		else if (selectedbrush == trace_brush_id && selectedbrushface == trace_brush_faceid && selectedbrushmodel == tracemodel)
		{
			mousedown = TRUE;
			mousetool = brushtool;
			vertedit.selectedvert1 = -1;
			vertedit.selectedvert2 = -1;
			bt_point[0] = brush_snappoint(trace_endpos);
			bt_points = 1;
			bt_displace = '0 0 0';
		}
		if (brushtool == BT_VERTEXEDIT && !vertedit.numidx && selectedbrush)
		{
			mousedown = FALSE;
			Debrushify();
			brush_selected(selectedbrushmodel, selectedbrush, selectedbrushface, TRUE);
		}
		return TRUE;
	}

	if (key == K_ENTER)
	{
		if (vertedit.numidx)
		{
			Rebrushify(&vertedit, FALSE);
			mousetool = BT_NONE;
			bt_points = 0;
			vertedit.numidx = 0;
			vertedit.numverts = 0;
			mousedown = FALSE;
			return TRUE;
		}
		else if (brushtool == BT_CREATE)
		{
			if (bt_points >= 3)
			{
				tmp_numfaces = BrushFromPoints(tmp_faces, tmp_faces.length, bt_point, bt_points, autocvar_ca_newbrushheight);
				bt_points = 0;
				int newbr = brush_history_create(selectedbrushmodel, tmp_faces, tmp_numfaces, 1i);
				
				brush_deselectall();
				brush_select(newbr);
			}
			return TRUE;
		}
		else if (brushtool == BT_SLICE)
		{
			//get the current faces
			tmp_numfaces = brush_get(selectedbrushmodel, selectedbrush, tmp_faces, tmp_faces.length-1, &tmp_contents);

			//generate a new face plane
			fa = &tmp_faces[tmp_numfaces];
			fa->planenormal = normalize(crossproduct(bt_point[2] - bt_point[0], bt_point[1] - bt_point[0]));
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
			int newbr1 = brush_history_edit(selectedbrushmodel, selectedbrush, tmp_faces, tmp_numfaces, tmp_contents);

			//and insert another new one too, because inserting a plane like this generates two fragments and I'm too lazy to work out which is the front and which is the back.
			fa->planenormal *= -1;
			fa->planedist *= -1;
			int newbr2 = brush_history_create(selectedbrushmodel, tmp_faces, tmp_numfaces, tmp_contents);

			brush_deselectall();
			brush_select(newbr2);
			return TRUE;
		}
		else return FALSE;
	}

	return FALSE;
};
