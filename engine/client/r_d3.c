#include "quakedef.h"
#ifdef MAP_PROC

#ifndef SERVERONLY
#include "shader.h"
#endif

void RMod_SetParent (mnode_t *node, mnode_t *parent);
int	D3_LeafnumForPoint (struct model_s *model, vec3_t point);

#ifndef SERVERONLY
qboolean Mod_LoadMap_Proc(model_t *model, char *data)
{
	char token[256];
	data = COM_ParseOut(data, token, sizeof(token));
	if (strcmp(token, "mapProcFile003"))
	{
		Con_Printf("proc format not compatible %s\n", token);
		return false;
	}
	/*FIXME: add sanity checks*/

	while(1)
	{
		data = COM_ParseOut(data, token, sizeof(token));
		if (!data)
			break;
		else if (!strcmp(token, "model"))
		{
			batch_t *b;
			mesh_t *m;
			model_t *sub;
			float f;
			int numsurfs, surf;
			int numverts, v, j;
			int numindicies;
			char *vdata;

			data = COM_ParseOut(data, token, sizeof(token));
			if (strcmp(token, "{"))
				return false;

			data = COM_ParseOut(data, token, sizeof(token));
			sub = Mod_FindName(va("*%s", token));

			data = COM_ParseOut(data, token, sizeof(token));
			numsurfs = atoi(token);
			if (numsurfs < 0 || numsurfs > 10000)
				return false;
			b = Hunk_Alloc(sizeof(*b) * numsurfs);
			m = Hunk_Alloc(sizeof(*m) * numsurfs);
			sub->numsurfaces = numsurfs;

			sub->batches[0] = b;

			for (surf = 0; surf < numsurfs; surf++)
			{
				data = COM_ParseOut(data, token, sizeof(token));
				if (strcmp(token, "{"))
					break;
				if (!data)
					return false;
				b[surf].meshes = 1;
				b[surf].mesh = (mesh_t**)&m[surf];
				b[surf].lightmap = -1;

				data = COM_ParseOut(data, token, sizeof(token));
				b[surf].shader = R_RegisterShader_Vertex(token);
				data = COM_ParseOut(data, token, sizeof(token));
				numverts = atoi(token);
				data = COM_ParseOut(data, token, sizeof(token));
				numindicies = atoi(token);

				m[surf].numvertexes = numverts;
				m[surf].numindexes = numindicies;
				vdata = Hunk_Alloc(numverts * (sizeof(vecV_t) + sizeof(vec2_t) + sizeof(vec3_t)) + numindicies * sizeof(index_t));

				m[surf].xyz_array = (vecV_t*)vdata;vdata += sizeof(vecV_t)*numverts;
				m[surf].st_array = (vec2_t*)vdata;vdata += sizeof(vec2_t)*numverts;
				m[surf].normals_array = (vec3_t*)vdata;vdata += sizeof(vec3_t)*numverts;
				m[surf].indexes = (index_t*)vdata;
				sub->mins[0] = 99999999;
				sub->mins[1] = 99999999;
				sub->mins[2] = 99999999;
				sub->maxs[0] = -99999999;
				sub->maxs[1] = -99999999;
				sub->maxs[2] = -99999999;

				for (v = 0; v < numverts; v++)
				{
					data = COM_ParseOut(data, token, sizeof(token));
					if (strcmp(token, "("))
						return false;

					data = COM_ParseOut(data, token, sizeof(token));
					m[surf].xyz_array[v][0] = atof(token);
					data = COM_ParseOut(data, token, sizeof(token));
					m[surf].xyz_array[v][1] = atof(token);
					data = COM_ParseOut(data, token, sizeof(token));
					m[surf].xyz_array[v][2] = atof(token);
					data = COM_ParseOut(data, token, sizeof(token));
					m[surf].st_array[v][0] = atof(token);
					data = COM_ParseOut(data, token, sizeof(token));
					m[surf].st_array[v][1] = atof(token);
					data = COM_ParseOut(data, token, sizeof(token));
					m[surf].normals_array[v][0] = atof(token);
					data = COM_ParseOut(data, token, sizeof(token));
					m[surf].normals_array[v][1] = atof(token);
					data = COM_ParseOut(data, token, sizeof(token));
					m[surf].normals_array[v][2] = atof(token);

					for (j = 0; j < 3; j++)
					{
						f = m[surf].xyz_array[v][j];
						if (f > sub->maxs[j])
							sub->maxs[j] = f;
						else if (f < sub->mins[j])
							sub->mins[j] = f;
					}

					data = COM_ParseOut(data, token, sizeof(token));
					if (strcmp(token, ")"))
						return false;
				}
				for (v = 0; v < numindicies; v++)
				{
					data = COM_ParseOut(data, token, sizeof(token));
					m[surf].indexes[v] = atoi(token);
				}
				data = COM_ParseOut(data, token, sizeof(token));
				if (strcmp(token, "}"))
					return false;
			}
			data = COM_ParseOut(data, token, sizeof(token));
			if (strcmp(token, "}"))
				return false;
			sub->needload = false;
			sub->fromgame = fg_doom3;
			sub->type = mod_brush;
		}
		else if (!strcmp(token, "shadowModel"))
		{
			int numverts, v;
			int numindexes, i;
			data = COM_ParseOut(data, token, sizeof(token));
			if (strcmp(token, "{"))
				return false;

			data = COM_ParseOut(data, token, sizeof(token));
			//name
			data = COM_ParseOut(data, token, sizeof(token));
			numverts = atoi(token);
			data = COM_ParseOut(data, token, sizeof(token));
			//nocaps
			data = COM_ParseOut(data, token, sizeof(token));
			//nofrontcaps
			data = COM_ParseOut(data, token, sizeof(token));
			numindexes = atoi(token);
			data = COM_ParseOut(data, token, sizeof(token));
			//planebits

			for (v = 0; v < numverts; v++)
			{
				data = COM_ParseOut(data, token, sizeof(token));
				if (strcmp(token, "("))
					return false;

				data = COM_ParseOut(data, token, sizeof(token));
				//x
				data = COM_ParseOut(data, token, sizeof(token));
				//y
				data = COM_ParseOut(data, token, sizeof(token));
				//z

				data = COM_ParseOut(data, token, sizeof(token));
				if (strcmp(token, ")"))
					return false;
			}

			for (i = 0; i < numindexes; i++)
			{
				data = COM_ParseOut(data, token, sizeof(token));
			}

			data = COM_ParseOut(data, token, sizeof(token));
			if (strcmp(token, "}"))
				return false;
		}
		else if (!strcmp(token, "nodes"))
		{
			int numnodes, n;
			data = COM_ParseOut(data, token, sizeof(token));
			if (strcmp(token, "{"))
				return false;

			data = COM_ParseOut(data, token, sizeof(token));
			numnodes = atoi(token);
			model->nodes = Hunk_Alloc(sizeof(*model->nodes)*numnodes);
			model->planes = Hunk_Alloc(sizeof(*model->planes)*numnodes);

			for (n = 0; n < numnodes; n++)
			{
				data = COM_ParseOut(data, token, sizeof(token));
				if (strcmp(token, "("))
					return false;

				model->nodes[n].plane = &model->planes[n];

				data = COM_ParseOut(data, token, sizeof(token));
				model->planes[n].normal[0] = atof(token);
				data = COM_ParseOut(data, token, sizeof(token));
				model->planes[n].normal[1] = atof(token);
				data = COM_ParseOut(data, token, sizeof(token));
				model->planes[n].normal[2] = atof(token);
				data = COM_ParseOut(data, token, sizeof(token));
				model->planes[n].dist = atof(token);

				data = COM_ParseOut(data, token, sizeof(token));
				if (strcmp(token, ")"))
					return false;

				data = COM_ParseOut(data, token, sizeof(token));
				model->nodes[n].childnum[0] = atoi(token);
				data = COM_ParseOut(data, token, sizeof(token));
				model->nodes[n].childnum[1] = atoi(token);
			}

			data = COM_ParseOut(data, token, sizeof(token));
			if (strcmp(token, "}"))
				return false;

			RMod_SetParent(model->nodes, NULL);
		}
		else if (!strcmp(token, "interAreaPortals"))
		{
			int numareas;
			int pno, v;
			portal_t *p;

			data = COM_ParseOut(data, token, sizeof(token));
			if (strcmp(token, "{"))
				return false;

			data = COM_ParseOut(data, token, sizeof(token));
			numareas = atoi(token);
			data = COM_ParseOut(data, token, sizeof(token));
			model->numportals = atoi(token);

			model->portal = p = Hunk_Alloc(sizeof(*p) * model->numportals);

			for (pno = 0; pno < model->numportals; pno++, p++)
			{
				data = COM_ParseOut(data, token, sizeof(token));
				p->numpoints = atoi(token);
				data = COM_ParseOut(data, token, sizeof(token));
				p->area[0] = atoi(token);
				data = COM_ParseOut(data, token, sizeof(token));
				p->area[1] = atoi(token);

				p->points = Hunk_Alloc(sizeof(*p->points) * p->numpoints);

				ClearBounds(p->min, p->max);
				for (v = 0; v < p->numpoints; v++)
				{
					data = COM_ParseOut(data, token, sizeof(token));
					if (strcmp(token, "("))
						return false;

					data = COM_ParseOut(data, token, sizeof(token));
					p->points[v][0] = atof(token);
					data = COM_ParseOut(data, token, sizeof(token));
					p->points[v][1] = atof(token);
					data = COM_ParseOut(data, token, sizeof(token));
					p->points[v][2] = atof(token);
					p->points[v][3] = 1;

					AddPointToBounds(p->points[v], p->min, p->max);

					data = COM_ParseOut(data, token, sizeof(token));
					if (strcmp(token, ")"))
						return false;
				}

			}

			data = COM_ParseOut(data, token, sizeof(token));
			if (strcmp(token, "}"))
				return false;
		}
		else
		{
			Con_Printf("unexpected token %s\n", token);
			return false;
		}
	}

	return true;
}


static qboolean D3_PolyBounds(vec_t result[4], int count, vec4_t *vlist)
{
	qboolean ret = false;
	int i;
	vec4_t tempv, v;
	/*inverted*/
	result[0] = 10000;
	result[1] = -10000;
	result[2] = 10000;
	result[3] = -10000;
	for (i = 0; i < count; i++)
	{
		Matrix4x4_CM_Transform4(r_refdef.m_view, vlist[i], tempv); 
		Matrix4x4_CM_Transform4(r_refdef.m_projection, tempv, v);

		v[0] /= v[3];
		v[1] /= v[3];
	//	if (v[2] < 0)
	//		continue;

		if (result[0] > v[0])
			result[0] = v[0];
		if (result[1] < v[0])
			result[1] = v[0];
		if (result[2] > v[1])
			result[2] = v[1];
		if (result[3] < v[1])
			result[3] = v[1];
		ret = true;
	}
	return ret;
}

qboolean R_CullBox (vec3_t mins, vec3_t maxs);

static int walkno;
/*convert each portal to a 2d box, because its much much simpler than true poly clipping*/
void D3_WalkPortal(model_t *mod, int start, vec_t bounds[4], unsigned char *vis)
{
	int i;
	portal_t *p;
	int side;
	vec_t newbounds[4];
	
	vis[start>>3] |= 1<<(start&7);

	for (i = 0; i < mod->numportals; i++)
	{
		p = mod->portal+i;
		if (p->walkno == walkno)
			continue;
		if (p->area[0] == start)
			side = 0;
		else if (p->area[1] == start)
			side = 1;
		else
			continue;

		R_CullBox(p->min, p->max);

		if (!D3_PolyBounds(newbounds, p->numpoints, p->points))
		{
			p->walkno = walkno;
			continue;
		}
		/*new poly was to the right of it, or fully to the left*/
		if (newbounds[1] <= bounds[0] || newbounds[0] >= bounds[1])
			continue;
		if (newbounds[3] <= bounds[2] || newbounds[2] >= bounds[3])
			continue;

		if (newbounds[0] < bounds[0])
			newbounds[0] = bounds[0];
		if (newbounds[1] > bounds[1])
			newbounds[1] = bounds[1];

		if (newbounds[2] < bounds[2])
			newbounds[2] = bounds[2];
		if (newbounds[3] > bounds[3])
			newbounds[3] = bounds[3];

		/*FIXME: clip the new bounds to the old bounds*/

		p->walkno = walkno;
		D3_WalkPortal(mod, p->area[!side], newbounds, vis);
	}
}

unsigned char *D3_CalcVis(model_t *mod, vec3_t org)
{
	int start;
	static unsigned char vis[256];
	vec_t newbounds[4];

	start = D3_LeafnumForPoint(mod, org);
	/*figure out which area we're in*/
	if (start < 0)
	{
		/*outside the world, just make it all visible, and take the fps hit*/
		memset(vis, 255, 4);
		return vis;
	}
	else if (r_novis.value)
		return vis;
	else
	{
		memset(vis, 0, 4);
		/*make a bounds the size of the view*/
		newbounds[0] = -1;
		newbounds[1] = 1;
		newbounds[2] = -1;
		newbounds[3] = 1;
		walkno++;
		D3_WalkPortal(mod, start, newbounds, vis);
//		Con_Printf("%x %x %x %x\n", vis[0], vis[1], vis[2], vis[3]);
		return vis;
	}
}

/*emits static entities, one for each area, which is only visible if that area is in the vis*/
void D3_GenerateAreas(model_t *mod)
{
	entity_t *ent;

	int area;

	for (area = 0; area < 256*8; area++)
	{
		if (cl.num_statics == cl_max_static_entities)
		{
			cl_max_static_entities += 16;
			cl_static_entities = BZ_Realloc(cl_static_entities, sizeof(*cl_static_entities) * cl_max_static_entities);
		}

		ent = &cl_static_entities[cl.num_statics].ent;
		cl_static_entities[cl.num_statics].mdlidx = 0;
		memset(ent, 0, sizeof(*ent));
		ent->model = Mod_FindName(va("*_area%i", area));
		ent->scale = 1;
		AngleVectors(ent->angles, ent->axis[0], ent->axis[1], ent->axis[2]);
		VectorInverse(ent->axis[1]);
		ent->shaderRGBAf[0] = 1;
		ent->shaderRGBAf[1] = 1;
		ent->shaderRGBAf[2] = 1;
		ent->shaderRGBAf[3] = 1;

		/*put it in that area*/
		cl_static_entities[cl.num_statics].pvscache.num_leafs = 1;
		cl_static_entities[cl.num_statics].pvscache.leafnums[0] = area;

		if (ent->model && !ent->model->needload)
			cl.num_statics++;
		else
			break;
	}
}

#endif

//edict system as opposed to q2 game dll system.
void D3_FindTouchedLeafs (struct model_s *model, struct pvscache_s *ent, vec3_t cullmins, vec3_t cullmaxs)
{
}
qbyte *D3_LeafPVS (struct model_s *model, int num, qbyte *buffer, unsigned int buffersize)
{
	return buffer;
}
int	D3_LeafnumForPoint (struct model_s *model, vec3_t point)
{
	float p;
	int c;
	mnode_t *node;
	node = model->nodes;
	while(1)
	{
		p = DotProduct(point, node->plane->normal) + node->plane->dist;
		c = node->childnum[p<0];
		if (c <= 0)
			return -1-c;
		node = model->nodes + c;
	}
	return 0;
}
unsigned int D3_FatPVS (struct model_s *model, vec3_t org, qbyte *pvsbuffer, unsigned int buffersize, qboolean merge)
{
	return 0;
}

void D3_StainNode			(struct mnode_s *node, float *parms)
{
}

qboolean D3_Trace (struct model_s *model, int hulloverride, int frame, vec3_t axis[3], vec3_t p1, vec3_t p2, vec3_t mins, vec3_t maxs, struct trace_s *trace)
{
	trace->fraction = 0;
	VectorCopy(p1, trace->endpos);
	trace->allsolid = true;
	trace->startsolid = true;
	trace->ent = NULL;
	return false;
}

unsigned int D3_PointContents (struct model_s *model, vec3_t axis[3], vec3_t p)
{
	return FTECONTENTS_SOLID;
}

void D3_LightPointValues (struct model_s *model, vec3_t point, vec3_t res_diffuse, vec3_t res_ambient, vec3_t res_dir)
{
	VectorClear(res_diffuse);
	VectorClear(res_ambient);
	VectorClear(res_dir);
	res_dir[2] = 1;
}


qboolean D3_EdictInFatPVS (struct model_s *model, struct pvscache_s *edict, qbyte *pvsbuffer)
{
	int i;
	for (i = 0; i < edict->num_leafs; i++)
		if (pvsbuffer[edict->leafnums[i]>>3] & (1u<<(edict->leafnums[i]&7)))
			return true;
	return false;
}

qboolean D3_LoadMap_CollisionMap(model_t *mod, char *buf)
{
	char token[256];
	buf = COM_ParseOut(buf, token, sizeof(token));
	if (strcmp(token, "CM"))
		return false;
	
	buf = COM_ParseOut(buf, token, sizeof(token));
	if (atof(token) != 1.0)
		return false;

	/*load up the .map so we can get some entities (anyone going to bother making a qc mod compatible with this?)*/
	COM_StripExtension(mod->name, token, sizeof(token));
	mod->entities = FS_LoadMallocFile(va("%s.map", token));

	mod->funcs.FindTouchedLeafs = D3_FindTouchedLeafs;
	mod->funcs.Trace = D3_Trace;
	mod->funcs.PointContents = D3_PointContents;
	mod->funcs.FatPVS = D3_FatPVS;
	mod->funcs.LeafnumForPoint = D3_LeafnumForPoint;
	mod->funcs.StainNode = D3_StainNode;
	mod->funcs.LightPointValues = D3_LightPointValues;
	mod->funcs.EdictInFatPVS = D3_EdictInFatPVS;

	mod->fromgame = fg_doom3;

	/*that's the physics sorted*/
#ifndef SERVERONLY
	if (!isDedicated)
	{
		COM_StripExtension(mod->name, token, sizeof(token));
		buf = FS_LoadMallocFile(va("%s.proc", token));
		Mod_LoadMap_Proc(mod, buf);
		BZ_Free(buf);
	}
#endif
	return true;
}

#endif