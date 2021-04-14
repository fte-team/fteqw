void(int mod, int id) DrawEngineBrushWireframe =
{
	const vector col = '1 0 0';
	for(int facenum = 0;;)
	{
		int points = brush_getfacepoints(mod, id, ++facenum, &facepoints[0], MAX_FACEPOINTS);
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
		points = brush_calcfacepoints(++f, faces, numfaces, facepoints, MAX_FACEPOINTS);
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
		points = brush_calcfacepoints(++f, faces, numfaces, facepoints, MAX_FACEPOINTS);
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
		points = brush_calcfacepoints(1+f, faces, numfaces, facepoints, MAX_FACEPOINTS);
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
