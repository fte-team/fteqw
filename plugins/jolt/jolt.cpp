/*
JOLT PHYSICS PLUGIN FOR FTEQW
written by erysdren (it/its)
started work on May 14 2025
*/

/*
# TODO
## last updated 2025-06-03
- [ ] sort out frametime issues, especially if sv_nqplayerphysics == 0
- [ ] sort out native tracelines (requires engine changes)
- [ ] implement the rest of the joint types (JOINTTYPE_UNIVERSAL,
JOINTTYPE_HINGE2)
- [ ] implement new joint types based on what Jolt offers (Pulley, Gear)
- [x] implement axis-lock for 2d games
- [ ] build brush entities as multiple convex solids if they have brushes in
the BIH data, otherwise build them as trisoup (if world) or build a single
convex solid (if submodel)
- [ ] implement ragdoll callbacks
- [ ] figure out how to do buoyency in a way that doesn't suck
- [ ] hook up particle system to client's RBE functions for physically
simulated particles (CL_SpawnSpriteEffect in cl_tent.c)
- [ ] figure out why loading a savegame causes a segfault
- [x] test if demos work
- [ ] add a debug drawing cvar for Jolt convex hulls
- [ ] add a QC builtin for supplying a convex hull for a given entity or modelindex
- [ ] make sure bullet and ode plugins can still work (as well as they ever did)
- [x] add an entity field for locking movement to particular to axes, should
correspond to Jolt's AllowedDOFs enum.
- [ ] think of a way for QC to create Jolt CompoundShapes
- [ ] update EnumerateBrushes stuff to address submodels
- [x] update EnumerateBrushes stuff to work with Q2BSP and Q3BSP
- [ ] destroy associated constraints when bodys are deleted to prevent crashes
*/

/*
# CHANGELOG
## last updated 2025-06-08
- Added Jolt Physics as both a standalone and internal plugin
- server/progdefs.h: added missing physics fields to entityrbe_t
- server/progdefs.h: changed gravity field in entityrbe_t from a qboolean to a float
- server/progdefs.h: added docstrings to the various DP/FTE entity physics fields
- server/world.c: re-enabled the use of the RBE trace callback in World_Move()
- added EnumerateBrushes callback for models so a brush list can be gathered
- added .alloweddof entity field (with corresponding constants) to limit the
movement and rotation of rigidbody entities
- common/console.h: fixed a whitespace warning that only affects C++
- engine/makefile: updated makelibs cmake invocation for web target
*/

#ifndef FTEPLUGIN
#define FTEENGINE
#define FTEPLUGIN
#define Plug_Init Plug_Jolt_Init
#endif

#include "quakedef.h"
#include "../plugin.h"
#include "../engine.h"

#include "pr_common.h"
#include "com_mesh.h"
#include "com_bih.h"

#include "Jolt/Jolt.h"
#include "Jolt/RegisterTypes.h"
#include "Jolt/Core/Factory.h"
#include "Jolt/Core/QuickSort.h"
#include "Jolt/Core/TempAllocator.h"
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Physics/PhysicsSettings.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Physics/Collision/RayCast.h"
#include "Jolt/Physics/Collision/CastResult.h"
#include "Jolt/Physics/Collision/Shape/Shape.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"
#include "Jolt/Physics/Collision/Shape/CylinderShape.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/MeshShape.h"
#include "Jolt/Physics/Collision/Shape/ConvexHullShape.h"
#include "Jolt/Physics/Collision/Shape/StaticCompoundShape.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Body/BodyActivationListener.h"
#include "Jolt/Physics/Constraints/PointConstraint.h"
#include "Jolt/Physics/Constraints/FixedConstraint.h"
#include "Jolt/Physics/Constraints/SliderConstraint.h"
#include "Jolt/Physics/Constraints/HingeConstraint.h"

#ifndef FTEENGINE
#define BZ_Malloc malloc
#define BZ_Free free
int VectorCompare(const vec3_t v1, const vec3_t v2)
{
	for (int i = 0; i < 3; i++)
		if (v1[i] != v2[i])
			return 0;
	return 1;
}
vec_t Length(const vec3_t v)
{
	int		i;
	float	length;

	length = 0;
	for (i=0 ; i< 3 ; i++)
		length += v[i]*v[i];
	length = sqrt (length);		// FIXME

	return length;
}
float RadiusFromBounds(const vec3_t mins, const vec3_t maxs)
{
	int		i;
	vec3_t	corner;

	for (i=0 ; i<3 ; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return Length (corner);
}
void CrossProduct (const vec3_t v1, const vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}
float QDECL VectorNormalize (vec3_t v)
{
	float	length;
	float	ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = sqrt (length);		// FIXME

	if (length)
	{
		ilength = 1.0/length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;
}
// common/q1bsp.c
#define MAXFRAGMENTVERTS 360
int Fragment_ClipPolyToPlane(float *inverts, float *outverts, int incount, float *plane, float planedist)
{
#define C (sizeof(vecV_t)/sizeof(vec_t))
	float dotv[MAXFRAGMENTVERTS+1];
	char keep[MAXFRAGMENTVERTS+1];
#define KEEP_KILL 0
#define KEEP_KEEP 1
#define KEEP_BORDER 2
	int i;
	int outcount = 0;
	int clippedcount = 0;
	float d, *p1, *p2, *out;
#define FRAG_EPSILON (1.0/32) //0.5

	for (i = 0; i < incount; i++)
	{
		dotv[i] = DotProduct((inverts+i*C), plane) - planedist;
		if (dotv[i]<-FRAG_EPSILON)
		{
			keep[i] = KEEP_KILL;
			clippedcount++;
		}
		else if (dotv[i] > FRAG_EPSILON)
			keep[i] = KEEP_KEEP;
		else
			keep[i] = KEEP_BORDER;
	}
	dotv[i] = dotv[0];
	keep[i] = keep[0];

	if (clippedcount == incount)
		return 0;	//all were clipped
	if (clippedcount == 0)
	{	//none were clipped
		for (i = 0; i < incount; i++)
			VectorCopy((inverts+i*C), (outverts+i*C));
		return incount;
	}

	for (i = 0; i < incount; i++)
	{
		p1 = inverts+i*C;
		if (keep[i] == KEEP_BORDER)
		{
			out = outverts+outcount++*C;
			VectorCopy(p1, out);
			continue;
		}
		if (keep[i] == KEEP_KEEP)
		{
			out = outverts+outcount++*C;
			VectorCopy(p1, out);
		}
		if (keep[i+1] == KEEP_BORDER || keep[i] == keep[i+1])
			continue;
		p2 = inverts+((i+1)%incount)*C;
		d = dotv[i] - dotv[i+1];
		if (d)
			d = dotv[i] / d;

		out = outverts+outcount++*C;
		VectorInterpolate(p1, d, p2, out);
	}
	return outcount;
}

//the plane itself must be a vec4_t, but can have other data packed between
size_t Fragment_ClipPlaneToBrush(vecV_t *points, size_t maxpoints, void *planes, size_t planestride, size_t numplanes, vec4_t face)
{
	int p, a;
	vecV_t verts[MAXFRAGMENTVERTS];
	vecV_t verts2[MAXFRAGMENTVERTS];
	vecV_t *cverts;
	int flip;
//	vec3_t d1, d2, n;
	size_t numverts;

	//generate some huge quad/poly aligned with the plane
	vec3_t tmp;
	vec3_t right, forward;
	double t;
	float *plane;

//	if (face[2] != 1)
//		return 0;

	t = fabs(face[2]);
	if (t > fabs(face[0]) && t > fabs(face[1]))
		VectorSet(tmp, 1, 0, 0);
	else
		VectorSet(tmp, 0, 0, 1);

	CrossProduct(face, tmp, right);
	VectorNormalize(right);
	CrossProduct(face, right, forward);
	VectorNormalize(forward);

	VectorScale(face, face[3],			verts[0]);
	VectorMA(verts[0], 32767, right,		verts[0]);
	VectorMA(verts[0], 32767, forward,	verts[0]);

	VectorScale(face, face[3],			verts[1]);
	VectorMA(verts[1], 32767, right,		verts[1]);
	VectorMA(verts[1], -32767, forward,	verts[1]);

	VectorScale(face, face[3],			verts[2]);
	VectorMA(verts[2], -32767, right,	verts[2]);
	VectorMA(verts[2], -32767, forward,	verts[2]);

	VectorScale(face, face[3],			verts[3]);
	VectorMA(verts[3], -32767, right,	verts[3]);
	VectorMA(verts[3], 32767, forward,	verts[3]);

	numverts = 4;


	//clip the quad to the various other planes
	flip = 0;
	for (p = 0; p < numplanes; p++)
	{
		plane = (float*)((qbyte*)planes + p*planestride);
		if (plane != face)
		{
			vec3_t norm;
			flip^=1;
			VectorNegate(plane, norm);
			if (flip)
				numverts = Fragment_ClipPolyToPlane((float*)verts, (float*)verts2, numverts, norm, -plane[3]);
			else
				numverts = Fragment_ClipPolyToPlane((float*)verts2, (float*)verts, numverts, norm, -plane[3]);

			if (numverts < 3)	//totally clipped.
				return 0;
		}
	}

	if (numverts > maxpoints)
		return 0;

	if (flip)
		cverts = verts2;
	else
		cverts = verts;
	for (p = 0; p < numverts; p++)
	{
		for (a = 0; a < 3; a++)
		{
			float f = cverts[p][a];
			int rounded = floor(f + 0.5);
			//if its within 1/1000th of a qu, just round it.
			if (fabs(f - rounded) < 0.001)
				points[p][a] = rounded;
			else
				points[p][a] = f;
		}
	}

	return numverts;
}
#endif

#ifndef DEG2RAD
#define DEG2RAD(d) (d * M_PI * (1/180.0f))
#endif

#ifndef RAD2DEG
#define RAD2DEG(d) ((d*180) / M_PI)
#endif

static plugthreadfuncs_t *threadfuncs = nullptr;
static rbeplugfuncs_t *rbefuncs = nullptr;
static cvar_t *physics_jolt_maxiterationsperframe = nullptr;
static cvar_t *physics_jolt_framerate = nullptr;
static cvar_t *pr_meshpitch = nullptr;

/* taken from VPhysics-Jolt */
static const float InchesToMeters = 0.0254f;
static const float MetersToInches = 1.0f / 0.0254f;

enum {
	LAYER_STATIC,
	LAYER_MOVING,
	NUM_LAYERS
};

typedef struct joltcontext {
	rigidbodyengine_t funcs;

	static const unsigned int max_bodies = 65536;
	static const unsigned int num_body_mutexes = 0;
	static const unsigned int max_body_pairs = 65536;
	static const unsigned int max_contact_constraints = 10240;

	static const unsigned int max_physics_jobs = JPH::cMaxPhysicsJobs;
	static const unsigned int max_physics_barriers = JPH::cMaxPhysicsBarriers;

	JPH::TempAllocatorMalloc temp_allocator;
	JPH::PhysicsSystem physics_system;
	JPH::JobSystemThreadPool job_system;

#if 0
	class JobSystemImpl : public JPH::JobSystemWithBarrier
	{
	public:
		JobSystemImpl() : JobSystemWithBarrier(max_physics_barriers) { }
		~JobSystemImpl() = default;

		virtual JobHandle CreateJob(const char *inName, JPH::ColorArg inColor, const JPH::JobSystem::JobFunction &inJobFunction, uint32_t inNumDependencies = 0)
		{

		}
	} job_system;
#endif

	class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
	{
	public:
		virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
		{
			if (inObject1 == LAYER_STATIC)
				return inObject2 == LAYER_MOVING;
			else
				return inObject1 == LAYER_MOVING;
		}
	} object_vs_object_layer_filter;

	class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
	{
	public:
		virtual unsigned int GetNumBroadPhaseLayers() const override
		{
			return NUM_LAYERS;
		}
		virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
		{
			return (JPH::BroadPhaseLayer)inLayer;
		}
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		virtual const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
		{
			switch (inLayer.GetValue())
			{
				case LAYER_STATIC: return "STATIC";
				case LAYER_MOVING: return "MOVING";
				default: return "INVALID";
			}
		}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED
	} broad_phase_layer_interface;

	class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
		{
			if (inLayer1 == LAYER_STATIC)
				return inLayer2 == (JPH::BroadPhaseLayer)LAYER_MOVING;
			else
				return inLayer1 == LAYER_MOVING;
		}
	} object_vs_broadphase_layer_filter;

	qboolean broadphase_optimized;

	rbecommandqueue_t *cmdqueuehead;
	rbecommandqueue_t *cmdqueuetail;
} joltcontext_t;

static void QDECL World_Jolt_End(world_t *world)
{
	joltcontext_t *ctx = (joltcontext_t *)world->rbe;
	delete ctx;
	world->rbe = nullptr;
}

static void QDECL World_Jolt_RemoveJointFromEntity(world_t *world, wedict_t *ed)
{
	joltcontext_t *ctx = (joltcontext_t *)world->rbe;
	if (ed->rbe.joint.joint)
		 ctx->physics_system.RemoveConstraint((JPH::Constraint *)ed->rbe.joint.joint);
	ed->rbe.joint_type = 0;
	ed->rbe.joint.joint = nullptr;
}

static void QDECL World_Jolt_RemoveFromEntity(world_t *world, wedict_t *ed)
{
	joltcontext_t *ctx = (joltcontext_t *)world->rbe;
	JPH::BodyInterface &body_interface = ctx->physics_system.GetBodyInterface();
	JPH::Body *body;
	JPH::Shape *geom;

	// not physics controlled
	if (!ed->rbe.physics)
		return;

	body = (JPH::Body *)ed->rbe.body.body;
	ed->rbe.body.body = nullptr;
	if (body)
	{
#if 0
		// remove constraints that use this body
		JPH::Constraints constraints = ctx->physics_system.GetConstraints();
		for (auto &c : constraints)
		{
			JPH::Constraint *constraint = c.GetPtr();
			wedict_t *joint = WEDICT_NUM_PB(world->progs, constraint->GetUserData());
			if (joint && (joint->v->enemy == NUM_FOR_EDICT(world->progs, ed) || joint->v->aiment == NUM_FOR_EDICT(world->progs, ed)))
			{
				ctx->physics_system.RemoveConstraint(constraint);
				c = nullptr;
			}
		}
#endif

		// remove body
		body_interface.RemoveBody(body->GetID());
		body_interface.DestroyBody(body->GetID());
	}

	geom = (JPH::Shape *)ed->rbe.body.geom;
	ed->rbe.body.geom = nullptr;
	if (ed->rbe.body.geom)
	{
		delete geom;
	}

	// entity is not physics controlled, free any physics data
	rbefuncs->ReleaseCollisionMesh(ed);
	ed->rbe.massbuf = nullptr;
	ed->rbe.physics = qfalse;
}

static void World_Jolt_RunCommand(world_t *world, rbecommandqueue_t *cmd)
{
	if (!cmd->edict->rbe.body.body)
		return;

	joltcontext_t *ctx = (joltcontext_t *)world->rbe;
	JPH::BodyInterface &body_interface = ctx->physics_system.GetBodyInterface();
	JPH::Body *body = (JPH::Body *)cmd->edict->rbe.body.body;
	const JPH::BodyID &body_id = body->GetID();

	switch (cmd->command)
	{
		case RBECMD_ENABLE:
		{
			body_interface.ActivateBody(body_id);
			break;
		}

		case RBECMD_DISABLE:
		{
			body_interface.DeactivateBody(body_id);
			break;
		}

		case RBECMD_FORCE:
		{
			// JPH::RVec3 center = body_interface.GetCenterOfMassPosition(body_id);
			// JPH::RVec3 v2 = JPH::RVec3(cmd->v2[0], cmd->v2[1], cmd->v2[2]) * InchesToMeters;
			// JPH::RVec3 pos = v2 - center;

			body_interface.AddForce(body_id, JPH::Vec3Arg(cmd->v1[0], cmd->v1[1], cmd->v1[2]), JPH::RVec3(cmd->v2[0], cmd->v2[1], cmd->v2[2]) * InchesToMeters);
			break;
		}

		case RBECMD_TORQUE:
		{
			body_interface.AddTorque(body_id, JPH::Vec3Arg(cmd->v1[0], cmd->v1[1], cmd->v1[2]));
			break;
		}
	}
}

struct enumeratebrushes_userdata_t {
	world_t *world;
	model_t *model;
	wedict_t *ed;
	JPH::StaticCompoundShapeSettings static_compound_shape_settings;
	JPH::Array<JPH::ConvexHullShapeSettings *> convex_hull_shape_settings;

	enumeratebrushes_userdata_t() = default;
	~enumeratebrushes_userdata_t() = default;
#if 0
	~enumeratebrushes_userdata_t()
	{
		for (auto &elem : convex_hull_shape_settings)
			delete elem;
	}
#endif
};

static void QDECL EnumerateBrushes_Callback(model_t *model, q2cbrush_t *brush, void *user)
{
	enumeratebrushes_userdata_t *userdata = (enumeratebrushes_userdata_t *)user;
	joltcontext_t *ctx = (joltcontext_t *)userdata->world->rbe;
	JPH::BodyInterface &body_interface = ctx->physics_system.GetBodyInterface();

	if (!brush->numsides)
		return;

	JPH::ConvexHullShapeSettings *convex_hull_shape_settings = new JPH::ConvexHullShapeSettings;

	// copy planes into a linear array
	vec4_t planes[brush->numsides];
	for (int i = 0; i < brush->numsides; i++)
	{
		VectorCopy(brush->brushside[i].plane->normal, planes[i]);
		planes[i][3] = brush->brushside[i].plane->dist;
	}

	// calculate points from the brushsides
	vecV_t facepoints[JPH::ConvexHullShape::cMaxPointsInHull];
	for (int i = 0; i < brush->numsides; i++)
	{
		int numpoints = Fragment_ClipPlaneToBrush(facepoints, countof(facepoints), planes, sizeof(*planes), brush->numsides, planes[i]);

		// add to convex hull settings
		for (int j = 0; j < numpoints; j++)
			convex_hull_shape_settings->mPoints.push_back(JPH::Vec3(facepoints[j][0], facepoints[j][1], facepoints[j][2]) * InchesToMeters);
	}

	if (!convex_hull_shape_settings->mPoints.size())
	{
		delete convex_hull_shape_settings;
		return;
	}

	// set radius
	if (userdata->model->radius)
		convex_hull_shape_settings->mMaxConvexRadius = userdata->model->radius * InchesToMeters;
	else
		convex_hull_shape_settings->mMaxConvexRadius = RadiusFromBounds(userdata->model->mins, userdata->model->maxs) * InchesToMeters;

	userdata->convex_hull_shape_settings.push_back(convex_hull_shape_settings);
	userdata->static_compound_shape_settings.AddShape(JPH::Vec3::sZero(), JPH::Quat::sIdentity(), convex_hull_shape_settings);
}

static void World_Jolt_BodyFromEntity(world_t *world, wedict_t *ed)
{
	JPH::Body *body = nullptr;
	JPH::Shape *geom = nullptr;
	JPH::MeshShapeSettings mesh_shape_settings;
	JPH::ConvexHullShapeSettings convex_hull_shape_settings;
	JPH::ShapeSettings *shape_settings = nullptr;
	enumeratebrushes_userdata_t enumeratebrushes_userdata;
	vec3_t maxs;
	vec3_t mins;
	vec3_t center;
	vec3_t size;
	vec3_t origin;
	vec3_t velocity;
	vec3_t angles;
	vec3_t avelocity;
	vec3_t spinvelocity;
	vec3_t forward, left, up;
	int modelindex = 0;
	int movetype = MOVETYPE_NONE;
	int solid = SOLID_NOT;
	int geomtype = GEOMTYPE_SOLID;
	vec_t scale = 1;
	vec_t mass = 1;
	model_t *model = nullptr;
	qboolean modified = qfalse;
	int numbrushes = 0;

	joltcontext_t *ctx = (joltcontext_t *)world->rbe;
	JPH::BodyInterface &body_interface = ctx->physics_system.GetBodyInterface();

	// FIXME: find a better way to prevent this crash?
	if (ed->v->movetype == MOVETYPE_NOCLIP)
		return;

	// get entity fields
	VectorCopy(ed->v->origin, origin);
	geomtype = ed->xv->geomtype;
	solid = ed->v->solid;
	movetype = ed->v->movetype;
	if (ed->xv->scale)
		scale = ed->xv->scale;

	// invalid geomtype, figure out what it should be based on solid type
	if (!geomtype)
	{
		switch (solid)
		{
			case SOLID_NOT:
				geomtype = GEOMTYPE_NONE;
				break;
			case SOLID_TRIGGER:
				geomtype = GEOMTYPE_NONE;
				break;
			case SOLID_BSP:
				geomtype = GEOMTYPE_TRIMESH;
				break;
			case SOLID_PHYSICS_TRIMESH:
				geomtype = GEOMTYPE_TRIMESH;
				break;
			case SOLID_PHYSICS_BOX:
				geomtype = GEOMTYPE_BOX;
				break;
			case SOLID_PHYSICS_SPHERE:
				geomtype = GEOMTYPE_SPHERE;
				break;
			case SOLID_PHYSICS_CAPSULE:
				geomtype = GEOMTYPE_CAPSULE;
				break;
			case SOLID_PHYSICS_CYLINDER:
				geomtype = GEOMTYPE_CYLINDER;
				break;
			default:
				geomtype = GEOMTYPE_BOX;
				break;
		}
	}

	// setup values based on geomtype
	switch (geomtype)
	{
		case GEOMTYPE_TRIMESH:
		{
			modelindex = ed->v->modelindex;
			model = world->Get_CModel(world, modelindex);
			if (!model || model->loadstate != MLS_LOADED)
			{
				model = nullptr;
				modelindex = 0;
			}
			if (model)
			{
				VectorScale(model->mins, scale, mins);
				VectorScale(model->maxs, scale, maxs);
				if (ed->xv->mass)
					mass = ed->xv->mass;
			}
			else
			{
				modelindex = 0;
				mass = 1.0f;
			}
			break;
		}

		case GEOMTYPE_BOX:
		case GEOMTYPE_SPHERE:
		case GEOMTYPE_CAPSULE:
		case GEOMTYPE_CAPSULE_X:
		case GEOMTYPE_CAPSULE_Y:
		case GEOMTYPE_CAPSULE_Z:
		case GEOMTYPE_CYLINDER:
		case GEOMTYPE_CYLINDER_X:
		case GEOMTYPE_CYLINDER_Y:
		case GEOMTYPE_CYLINDER_Z:
		{
			VectorCopy(ed->v->mins, mins);
			VectorCopy(ed->v->maxs, maxs);
			// VectorScale(ed->v->mins, scale, mins);
			// VectorScale(ed->v->maxs, scale, maxs);
			if (ed->xv->mass)
				mass = ed->xv->mass;
			break;
		}

		default:
		{
			World_Jolt_RemoveFromEntity(world, ed);
			return;
		}
	}

	// we don't allow point-size physics objects...
	VectorSubtract(maxs, mins, size);
	if (DotProduct(size, size) == 0)
	{
		// Con_Printf("Jolt: entity %i (classname %s) is point sized\n", NUM_FOR_EDICT(world->progs, (edict_t *)ed), PR_GetString(world->progs, ed->v->classname));
		World_Jolt_RemoveFromEntity(world, ed);
		return;
	}

	// check if we need to create or replace the geom
	if (!ed->rbe.physics || !VectorCompare(ed->rbe.mins, mins) || !VectorCompare(ed->rbe.maxs, maxs) || ed->rbe.modelindex != modelindex)
	{
		modified = qtrue;
		World_Jolt_RemoveFromEntity(world, ed);
		ed->rbe.physics = qtrue;
		VectorCopy(mins, ed->rbe.mins);
		VectorCopy(maxs, ed->rbe.maxs);
		ed->rbe.modelindex = modelindex;
		VectorAvg(mins, maxs, center);
		ed->rbe.movelimit = std::min(size[0], std::min(size[1], size[2]));
		ed->rbe.mass = mass;

		switch (geomtype)
		{
			case GEOMTYPE_TRIMESH:
			{
				if (!model)
				{
					// Con_Printf("Jolt: entity %i (classname %s) has no model\n", NUM_FOR_EDICT(world->progs, (edict_t *)ed), PR_GetString(world->progs, ed->v->classname));
					World_Jolt_RemoveFromEntity(world, ed);
					return;
				}

				// if we have brushes, make some convex hulls out of them
				if (model->funcs.EnumerateBrushes)
				{
					enumeratebrushes_userdata.world = world;
					enumeratebrushes_userdata.model = model;
					enumeratebrushes_userdata.ed = ed;
					numbrushes = model->funcs.EnumerateBrushes(model, EnumerateBrushes_Callback, (void *)&enumeratebrushes_userdata);
				}
				if (enumeratebrushes_userdata.convex_hull_shape_settings.size() > 0)
				{
					JPH::ShapeSettings::ShapeResult result;
					if (enumeratebrushes_userdata.static_compound_shape_settings.mSubShapes.size() >= 2)
					{
						geom = new JPH::StaticCompoundShape(enumeratebrushes_userdata.static_compound_shape_settings, ctx->temp_allocator, result);
						enumeratebrushes_userdata.static_compound_shape_settings.SetEmbedded();
						shape_settings = (JPH::ShapeSettings *)&enumeratebrushes_userdata.static_compound_shape_settings;
					}
					else
					{
						geom = new JPH::ConvexHullShape(*enumeratebrushes_userdata.convex_hull_shape_settings[0], result);
						shape_settings = (JPH::ShapeSettings *)enumeratebrushes_userdata.convex_hull_shape_settings[0];
					}

					if (result.HasError())
					{
						Con_Printf("Jolt: %s\n", result.GetError().c_str());
						World_Jolt_RemoveFromEntity(world, ed);
						return;
					}
				}
				else if (numbrushes)
				{
					Con_Printf("Jolt: entity %i (classname %s) failed to generate collision mesh for brushes\n", NUM_FOR_EDICT(world->progs, (edict_t *)ed), PR_GetString(world->progs, ed->v->classname));
					World_Jolt_RemoveFromEntity(world, ed);
					return;
				}
				else
				{
					// makes things simpler
					if (NUM_FOR_EDICT(world->progs, ed) == 0 || (model->type == mod_brush && DotProduct(ed->v->origin, ed->v->origin) == 0))
						VectorCopy(ed->v->origin, center);

					if (!rbefuncs->GenerateCollisionMesh(world, model, ed, center))
					{
						Con_Printf("Jolt: entity %i (classname %s) failed to generate collision mesh\n", NUM_FOR_EDICT(world->progs, (edict_t *)ed), PR_GetString(world->progs, ed->v->classname));
						World_Jolt_RemoveFromEntity(world, ed);
						return;
					}

					// we can only make a MeshShape out of worldspawn
					if (NUM_FOR_EDICT(world->progs, ed) == 0)
					{
						// read in vertices
						JPH::VertexList vertices;
						for (int i = 0; i < ed->rbe.numvertices; i++)
							vertices.push_back(JPH::Float3(ed->rbe.vertex3f[i * 3 + 0] * InchesToMeters, ed->rbe.vertex3f[i * 3 + 1] * InchesToMeters, ed->rbe.vertex3f[i * 3 + 2] * InchesToMeters));

						// read in triangles
						JPH::IndexedTriangleList triangles;
						for (int i = 0; i < ed->rbe.numtriangles; i++)
							triangles.push_back(JPH::IndexedTriangle(ed->rbe.element3i[i * 3 + 0], ed->rbe.element3i[i * 3 + 1], ed->rbe.element3i[i * 3 + 2], 0));

						// create mesh
						JPH::ShapeSettings::ShapeResult result;
						mesh_shape_settings.mTriangleVertices = vertices;
						mesh_shape_settings.mIndexedTriangles = triangles;
						mesh_shape_settings.Sanitize();
						geom = new JPH::MeshShape(mesh_shape_settings, result);
						if (result.HasError())
						{
							Con_Printf("Jolt: %s\n", result.GetError().c_str());
							World_Jolt_RemoveFromEntity(world, ed);
							return;
						}
						mesh_shape_settings.SetEmbedded();
						shape_settings = (JPH::ShapeSettings *)&mesh_shape_settings;
					}
					else
					{
						// otherwise, make a ConvexHullShape

						// read in vertices
						for (int i = 0; i < ed->rbe.numvertices; i++)
							convex_hull_shape_settings.mPoints.push_back(JPH::Vec3(ed->rbe.vertex3f[i * 3 + 0], ed->rbe.vertex3f[i * 3 + 1], ed->rbe.vertex3f[i * 3 + 2]) * InchesToMeters);

						// set radius
						if (model->radius)
							convex_hull_shape_settings.mMaxConvexRadius = model->radius * InchesToMeters;
						else
							convex_hull_shape_settings.mMaxConvexRadius = RadiusFromBounds(model->mins, model->maxs) * InchesToMeters;

						// create shape
						JPH::ShapeSettings::ShapeResult result;
						geom = new JPH::ConvexHullShape(convex_hull_shape_settings, result);
						if (result.HasError())
						{
							Con_Printf("Jolt: %s\n", result.GetError().c_str());
							World_Jolt_RemoveFromEntity(world, ed);
							return;
						}
						convex_hull_shape_settings.SetEmbedded();
						shape_settings = (JPH::ShapeSettings *)&convex_hull_shape_settings;
					}
				}
				break;
			}

			case GEOMTYPE_BOX:
			{
				geom = new JPH::BoxShape(JPH::Vec3Arg(size[0], size[1], size[2]) * 0.5 * InchesToMeters);
				break;
			}

			case GEOMTYPE_SPHERE:
			{
				geom = new JPH::SphereShape(size[0] * 0.5f * InchesToMeters);
				break;
			}

			case GEOMTYPE_CAPSULE:
			case GEOMTYPE_CAPSULE_X:
			case GEOMTYPE_CAPSULE_Y:
			case GEOMTYPE_CAPSULE_Z:
			{
				int axisindex;
				vec_t radius, length;
				if (geomtype == GEOMTYPE_CAPSULE)
				{
					// the qc gives us 3 axis radius, the longest axis is the capsule axis
					axisindex = 0;
					if (size[axisindex] < size[1])
						axisindex = 1;
					if (size[axisindex] < size[2])
						axisindex = 2;
				}
				else
					axisindex = geomtype - GEOMTYPE_CAPSULE_X;

				if (axisindex == 0)
					radius = std::min(size[1], size[2]) * 0.5f;
				else if (axisindex == 1)
					radius = std::min(size[0], size[2]) * 0.5f;
				else
					radius = std::min(size[0], size[1]) * 0.5f;

				length = size[axisindex] - radius * 2;

				if (length <= 0)
				{
					radius -= (1 - length) * 0.5;
					length = 1;
				}

				// TODO: check if length should be divided by 2
				geom = new JPH::CapsuleShape(length * InchesToMeters, radius * InchesToMeters);
				break;
			}

			case GEOMTYPE_CYLINDER:
			case GEOMTYPE_CYLINDER_X:
			case GEOMTYPE_CYLINDER_Y:
			case GEOMTYPE_CYLINDER_Z:
			{
				int axisindex;
				vec_t radius, length;
				if (geomtype == GEOMTYPE_CYLINDER)
				{
					// the qc gives us 3 axis radius, the longest axis is the capsule axis
					axisindex = 0;
					if (size[axisindex] < size[1])
						axisindex = 1;
					if (size[axisindex] < size[2])
						axisindex = 2;
				}
				else
					axisindex = geomtype - GEOMTYPE_CYLINDER_X;

				if (axisindex == 0)
					radius = std::min(size[1], size[2]) * 0.5f;
				else if (axisindex == 1)
					radius = std::min(size[0], size[2]) * 0.5f;
				else
					radius = std::min(size[0], size[1]) * 0.5f;

				length = size[axisindex] - radius * 2;

				if (length <= 0)
				{
					radius -= (1 - length) * 0.5;
					length = 1;
				}

				// TODO: check if length should be divided by 2
				geom = new JPH::CylinderShape(length * InchesToMeters, radius * InchesToMeters);
				break;
			}

			default:
			{
				Con_Printf("Jolt: entity %i (classname %s) has invalid geomtype\n", NUM_FOR_EDICT(world->progs, (edict_t *)ed), PR_GetString(world->progs, ed->v->classname));
				World_Jolt_RemoveFromEntity(world, ed);
				return;
			}
		}

		ed->rbe.body.geom = (void *)geom;
	}

	// non-moving objects need to be static objects (and thus need 0 mass)
	if (movetype != MOVETYPE_PHYSICS && movetype != MOVETYPE_WALK)
		mass = 0;

	// if the mass changes, we'll need to create a new body (but not the shape, so invalidate the current one)
	if (ed->rbe.mass != mass)
	{
		ed->rbe.mass = mass;

		body = (JPH::Body *)ed->rbe.body.body;
		if (body)
		{
			body_interface.RemoveBody(body->GetID());
			body_interface.DestroyBody(body->GetID());
		}

		ed->rbe.body.body = nullptr;
	}

	if (movetype == MOVETYPE_PHYSICS && ed->rbe.mass)
	{
		if (ed->rbe.body.body == nullptr)
		{
			// Con_Printf("created: %s\n", PR_GetString(world->progs, ed->v->classname));
			// setup
			JPH::BodyCreationSettings body_creation_settings;
			if (shape_settings)
				body_creation_settings.SetShapeSettings(shape_settings);
			else
				body_creation_settings.SetShape((JPH::Shape *)ed->rbe.body.geom);
			body_creation_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
			body_creation_settings.mMassPropertiesOverride.mMass = ed->rbe.mass;
			body_creation_settings.mObjectLayer = LAYER_MOVING;
			body_creation_settings.mMotionType = JPH::EMotionType::Dynamic;

			// create body
			body = body_interface.CreateBody(body_creation_settings);
			body_interface.SetUserData(body->GetID(), NUM_FOR_EDICT(world->progs, ed));

			//motion threshhold should be speed/physicsframerate.
			//Threshhold enables CCD when the object moves faster than X
			//FIXME: recalculate...
			//body->setCcdMotionThreshold((geomsize[0]+geomsize[1]+geomsize[2])*(4/3));
			//radius should be the body's radius, or smaller.
			//body->setCcdSweptSphereRadius((geomsize[0]+geomsize[1]+geomsize[2])*(0.5/3));

			// add to world
			body_interface.AddBody(body->GetID(), JPH::EActivation::DontActivate);
			modified = qtrue;
			ed->rbe.body.body = (void *)body;
		}
	}
	else
	{
		if (ed->rbe.body.body == nullptr)
		{
			// Con_Printf("created: %s\n", PR_GetString(world->progs, ed->v->classname));
			// setup
			JPH::BodyCreationSettings body_creation_settings;
			if (shape_settings)
				body_creation_settings.SetShapeSettings(shape_settings);
			else
				body_creation_settings.SetShape((JPH::Shape *)ed->rbe.body.geom);
			body_creation_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
			body_creation_settings.mMassPropertiesOverride.mMass = ed->rbe.mass;
			if (ed->rbe.mass)
			{
				body_creation_settings.mObjectLayer = LAYER_MOVING;
				body_creation_settings.mMotionType = JPH::EMotionType::Dynamic;
			}
			else
			{
				body_creation_settings.mObjectLayer = LAYER_STATIC;
				body_creation_settings.mMotionType = JPH::EMotionType::Static;
			}

			// create body
			body = body_interface.CreateBody(body_creation_settings);
			body_interface.SetUserData(body->GetID(), NUM_FOR_EDICT(world->progs, ed));

			// add to world
			body_interface.AddBody(body->GetID(), JPH::EActivation::DontActivate);
			modified = qtrue;
			ed->rbe.body.body = (void *)body;
		}
	}

	body = (JPH::Body *)ed->rbe.body.body;

	// get current data from entity
	VectorCopy(ed->v->origin, origin);
	VectorCopy(ed->v->velocity, velocity);
	VectorCopy(ed->v->angles, angles);
	VectorCopy(ed->v->avelocity, avelocity);

	// compatibility for legacy entities
	{
		vec3_t qangles, qavelocity;
		VectorCopy(angles, qangles);
		VectorCopy(avelocity, qavelocity);

		if (ed->v->modelindex)
		{
			model = world->Get_CModel(world, ed->v->modelindex);
			if (!model || model->type == mod_alias)
			{
				qangles[PITCH] *= pr_meshpitch->value;
				qavelocity[PITCH] *= pr_meshpitch->value;
			}
		}

		rbefuncs->AngleVectors(qangles, forward, left, up);
		VectorNegate(left,left);
		// convert single-axis rotations in avelocity to spinvelocity
		// FIXME: untested math - check signs
		VectorSet(spinvelocity, DEG2RAD(qavelocity[YAW]), DEG2RAD(qavelocity[ROLL]), DEG2RAD(qavelocity[PITCH]));
	}

	// compatibility for legacy entities
	switch (solid)
	{
		case SOLID_BBOX:
		case SOLID_SLIDEBOX:
		case SOLID_CORPSE:
			VectorSet(forward, 1, 0, 0);
			VectorSet(left, 0, 1, 0);
			VectorSet(up, 0, 0, 1);
			VectorSet(spinvelocity, 0, 0, 0);
			break;
	}

	// check if the qc edited any position data
	if (!VectorCompare(angles, ed->rbe.angles) ||
		!VectorCompare(origin, ed->rbe.origin) ||
		!VectorCompare(velocity, ed->rbe.velocity) ||
		!VectorCompare(avelocity, ed->rbe.avelocity) ||
		ed->xv->friction != ed->rbe.friction ||
		ed->xv->gravity != ed->rbe.gravity ||
		ed->xv->damp_linear != ed->rbe.damp_linear ||
		ed->xv->damp_angular != ed->rbe.damp_angular ||
		ed->xv->max_angular != ed->rbe.max_angular)
		modified = qtrue;

	// store the qc values into the physics engine
	if (modified && body)
	{
		// values for BodyFromEntity to check if the qc modified anything later
		if (!VectorCompare(angles, ed->rbe.angles))
		{
			VectorCopy(angles, ed->rbe.angles);
			body_interface.SetRotation(body->GetID(), JPH::Quat::sEulerAngles(JPH::Vec3Arg(DEG2RAD(angles[ROLL]), DEG2RAD(angles[PITCH]), DEG2RAD(angles[YAW]))), JPH::EActivation::Activate);
		}

		if (!VectorCompare(origin, ed->rbe.origin))
		{
			VectorCopy(origin, ed->rbe.origin);
			body_interface.SetPosition(body->GetID(), JPH::Vec3Arg(origin[0], origin[1], origin[2]) * InchesToMeters, JPH::EActivation::DontActivate);
		}

		if (!VectorCompare(velocity, ed->rbe.velocity))
		{
			VectorCopy(velocity, ed->rbe.velocity);
			body_interface.SetLinearVelocity(body->GetID(), JPH::Vec3Arg(velocity[0], velocity[1], velocity[2]) * InchesToMeters);
		}

		if (!VectorCompare(avelocity, ed->rbe.avelocity))
		{
			VectorCopy(avelocity, ed->rbe.avelocity);
			body_interface.SetAngularVelocity(body->GetID(), JPH::Vec3Arg(spinvelocity[0], spinvelocity[1], spinvelocity[2]));
		}

		JPH::MotionProperties *motion_properties = body->GetMotionPropertiesUnchecked();

		if (ed->xv->damp_linear != ed->rbe.damp_linear)
		{
			ed->rbe.damp_linear = ed->xv->damp_linear;
			if (motion_properties)
				motion_properties->SetLinearDamping(ed->xv->damp_linear);
		}

		if (ed->xv->damp_angular != ed->rbe.damp_angular)
		{
			ed->rbe.damp_angular = ed->xv->damp_angular;
			if (motion_properties)
				motion_properties->SetAngularDamping(ed->xv->damp_angular);
		}

		if (ed->xv->max_angular != ed->rbe.max_angular)
		{
			ed->rbe.max_angular = ed->xv->max_angular;
			if (motion_properties)
				motion_properties->SetMaxAngularVelocity(ed->xv->max_angular);
		}

		if (ed->xv->alloweddof != ed->rbe.alloweddof)
		{
			ed->rbe.alloweddof = ed->xv->alloweddof;
			if (motion_properties)
			{
				JPH::EAllowedDOFs alloweddof = JPH::EAllowedDOFs::None;
				JPH::MassProperties mass_properties;

				// TODO: translate coordinate space for rotations
				if ((int)ed->xv->alloweddof == ALLOWEDDOF_ALL)
				{
					alloweddof = JPH::EAllowedDOFs::All;
				}
				else
				{
					if ((int)ed->xv->alloweddof & ALLOWEDDOF_MOVE_X)
						alloweddof |= JPH::EAllowedDOFs::TranslationX;
					if ((int)ed->xv->alloweddof & ALLOWEDDOF_MOVE_Y)
						alloweddof |= JPH::EAllowedDOFs::TranslationY;
					if ((int)ed->xv->alloweddof & ALLOWEDDOF_MOVE_Z)
						alloweddof |= JPH::EAllowedDOFs::TranslationZ;
					if ((int)ed->xv->alloweddof & ALLOWEDDOF_ROTATE_X)
						alloweddof |= JPH::EAllowedDOFs::RotationX;
					if ((int)ed->xv->alloweddof & ALLOWEDDOF_ROTATE_Y)
						alloweddof |= JPH::EAllowedDOFs::RotationY;
					if ((int)ed->xv->alloweddof & ALLOWEDDOF_ROTATE_Z)
						alloweddof |= JPH::EAllowedDOFs::RotationZ;
				}

				// dont crash
				if (alloweddof == JPH::EAllowedDOFs::None)
					alloweddof = JPH::EAllowedDOFs::All;

				mass_properties.ScaleToMass(ed->rbe.mass);
				motion_properties->SetMassProperties(alloweddof, mass_properties);
			}
		}

		if (ed->xv->friction != ed->rbe.friction)
		{
			ed->rbe.friction = ed->xv->friction;
			body_interface.SetFriction(body->GetID(), ed->xv->friction);
		}

		if (ed->xv->gravity != ed->rbe.gravity)
		{
			ed->rbe.gravity = ed->xv->gravity;
			body_interface.SetGravityFactor(body->GetID(), ed->xv->gravity);
		}

		//something changed. make sure it still falls over appropriately
		body_interface.ActivateBody(body->GetID());
	}
}

static void World_Jolt_BodyToEntity(world_t *world, wedict_t *ed)
{
	model_t *model;
	vec3_t angles;
	vec3_t avelocity;
	vec3_t origin;
	vec3_t velocity;

	joltcontext_t *ctx = (joltcontext_t *)world->rbe;
	JPH::BodyInterface &body_interface = ctx->physics_system.GetBodyInterface();

	JPH::Body *body = (JPH::Body *)ed->rbe.body.body;

	if (!body || !ed->rbe.physics || ed->v->movetype != MOVETYPE_PHYSICS)
		return;

	JPH::RVec3 p = body_interface.GetPosition(body->GetID()) * MetersToInches;
	JPH::Quat q = body_interface.GetRotation(body->GetID());
	JPH::Vec3 qa = q.GetEulerAngles();
	JPH::Vec3 v = body_interface.GetLinearVelocity(body->GetID()) * MetersToInches;
	JPH::Vec3 a = body_interface.GetAngularVelocity(body->GetID());

	// copy to vectors
	origin[0] = p[0];
	origin[1] = p[1];
	origin[2] = p[2];
	velocity[0] = v[0];
	velocity[1] = v[1];
	velocity[2] = v[2];
	avelocity[PITCH] = RAD2DEG(a[YAW]);
	avelocity[YAW] = RAD2DEG(a[ROLL]);
	avelocity[ROLL] = RAD2DEG(a[PITCH]);
	angles[PITCH] = RAD2DEG(qa[YAW]);
	angles[YAW] = RAD2DEG(qa[ROLL]);
	angles[ROLL] = RAD2DEG(qa[PITCH]);

	if (ed->v->modelindex)
	{
		model = world->Get_CModel(world, ed->v->modelindex);
		if (!model || model->type == mod_alias)
		{
			angles[PITCH] *= pr_meshpitch->value;
			avelocity[PITCH] *= pr_meshpitch->value;
		}
	}

	// copy to entity
	VectorCopy(origin, ed->v->origin);
	VectorCopy(velocity, ed->v->velocity);
	VectorCopy(angles, ed->v->angles);
	VectorCopy(avelocity, ed->v->avelocity);

	// values for BodyFromEntity to check if the qc modified anything later
	VectorCopy(origin, ed->rbe.origin);
	VectorCopy(velocity, ed->rbe.velocity);
	VectorCopy(angles, ed->rbe.angles);
	VectorCopy(avelocity, ed->rbe.avelocity);
	ed->rbe.gravity = body_interface.GetGravityFactor(body->GetID());
	ed->rbe.friction = body_interface.GetFriction(body->GetID());

	JPH::MotionProperties *motion_properties = body->GetMotionPropertiesUnchecked();

	if (motion_properties)
	{
		ed->rbe.damp_linear = motion_properties->GetLinearDamping();
		ed->rbe.damp_angular = motion_properties->GetAngularDamping();
		ed->rbe.max_angular = motion_properties->GetMaxAngularVelocity();
	}

	rbefuncs->LinkEdict(world, ed, qtrue);

	// Con_Printf("updated: %s\n", PR_GetString(world->progs, ed->v->classname));
}

static void World_Jolt_JointFromEntity(world_t *world, wedict_t *ed)
{
	joltcontext_t *ctx = (joltcontext_t *)world->rbe;
	JPH::Constraint *joint = nullptr;
	int movetype = ed->v->movetype;
	JPH::Body *enemybody, *aimentbody;
	wedict_t *enemy, *aiment;
	vec_t cfm, erp, fmax;
	vec_t stop;
	vec_t vel;
	vec3_t forward, left, up;
	JPH::Vec3 origin(ed->v->origin[0], ed->v->origin[1], ed->v->origin[2]);

	// nothing to do
	if (ed->xv->jointtype == 0)
		return;

	// can't have both
	if (movetype == MOVETYPE_PHYSICS)
		return;

	enemy = (wedict_t *)PROG_TO_EDICT(world->progs, ed->v->enemy);
	enemybody = (JPH::Body *)enemy->rbe.body.body;
	if (ED_ISFREE(enemy) || !enemybody)
	{
		enemy = (wedict_t *)PROG_TO_EDICT(world->progs, 0);
		enemybody = (JPH::Body *)enemy->rbe.body.body;
	}

	aiment = (wedict_t *)PROG_TO_EDICT(world->progs, ed->v->aiment);
	aimentbody = (JPH::Body *)aiment->rbe.body.body;
	if (ED_ISFREE(aiment) || !aimentbody)
	{
		aiment = (wedict_t *)PROG_TO_EDICT(world->progs, 0);
		aimentbody = (JPH::Body *)aiment->rbe.body.body;
	}

	// FIXME: ??
	if (enemy == aiment)
		return;

	// FIXME: fucking darkplaces
	if (ed->v->movedir[0] > 0 && ed->v->movedir[1] > 0)
	{
		float step = 1.0f / physics_jolt_framerate->value;
		float k = ed->v->movedir[0];
		float d = ed->v->movedir[1];
		float r = 2.0 * d * sqrt(k); // we assume d is premultiplied by sqrt(springMass)
		cfm = 1.0 / (step * k + r); // always > 0
		erp = step * k * cfm;
		vel = 0;
		fmax = 0;
		stop = ed->v->movedir[2] * InchesToMeters;
	}
	else if (ed->v->movedir[1] < 0)
	{
		cfm = 0;
		erp = 0;
		vel = ed->v->movedir[0];
		fmax = -ed->v->movedir[1];
		stop = ed->v->movedir[2] > 0 ? ed->v->movedir[2] * InchesToMeters : INFINITY;
	}
	else
	{
		cfm = 0;
		erp = 0;
		vel = 0;
		fmax = 0;
		stop = INFINITY;
	}

	if (ed->xv->jointtype == ed->rbe.joint_type &&
		VectorCompare(ed->v->origin, ed->rbe.joint_origin) &&
		VectorCompare(ed->v->velocity, ed->rbe.joint_velocity) &&
		VectorCompare(ed->v->angles, ed->rbe.joint_angles) &&
		ed->v->enemy == ed->rbe.joint_enemy &&
		ed->v->aiment == ed->rbe.joint_aiment &&
		VectorCompare(ed->v->movedir, ed->rbe.joint_movedir))
		return; // nothing to do

	if (ed->rbe.joint.joint)
	{
		joint = (JPH::Constraint *)ed->rbe.joint.joint;
		ctx->physics_system.RemoveConstraint(joint);
		ed->rbe.joint.joint = nullptr;
		joint = nullptr;
	}

	JPH::Vec3 enemyorigin(enemy->v->origin[0], enemy->v->origin[1], enemy->v->origin[2]);
	JPH::Vec3 aimentorigin(aiment->v->origin[0], aiment->v->origin[1], aiment->v->origin[2]);

	ed->rbe.joint_type = ed->xv->jointtype;
	ed->rbe.joint_enemy = ed->v->enemy;
	ed->rbe.joint_aiment = ed->v->aiment;
	VectorCopy(ed->v->origin, ed->rbe.joint_origin);
	VectorCopy(ed->v->velocity, ed->rbe.joint_velocity);
	VectorCopy(ed->v->angles, ed->rbe.joint_angles);
	VectorCopy(ed->v->movedir, ed->rbe.joint_movedir);

	rbefuncs->AngleVectors(ed->v->angles, forward, left, up);
	VectorNegate(left, left);

	switch ((int)ed->xv->jointtype)
	{
		case JOINTTYPE_FIXED:
		{
			JPH::FixedConstraintSettings settings;
			settings.mPoint1 = (origin - enemyorigin) * InchesToMeters;
			settings.mPoint2 = (origin - aimentorigin) * InchesToMeters;
			settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;

			joint = new JPH::FixedConstraint(*enemybody, *aimentbody, settings);
			break;
		}

		case JOINTTYPE_POINT:
		{
			JPH::PointConstraintSettings settings;
			settings.mPoint1 = (origin - enemyorigin) * InchesToMeters;
			settings.mPoint2 = (origin - aimentorigin) * InchesToMeters;
			settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;

			joint = new JPH::PointConstraint(*enemybody, *aimentbody, settings);
			break;
		}

		case JOINTTYPE_HINGE:
		{
			JPH::HingeConstraintSettings settings;
			settings.mPoint1 = (origin - enemyorigin) * InchesToMeters;
			settings.mPoint2 = (origin - aimentorigin) * InchesToMeters;
			settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
			settings.mHingeAxis1 = settings.mHingeAxis2 = JPH::Vec3(forward[0], forward[1], forward[2]);
			settings.mNormalAxis1 = settings.mNormalAxis2 = settings.mHingeAxis1.GetNormalizedPerpendicular();

			joint = new JPH::HingeConstraint(*enemybody, *aimentbody, settings);
			break;
		}

		case JOINTTYPE_SLIDER:
		{
			JPH::SliderConstraintSettings settings;
			settings.mPoint1 = (origin - enemyorigin) * InchesToMeters;
			settings.mPoint2 = (origin - aimentorigin) * InchesToMeters;
			settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
			settings.mSliderAxis1 = settings.mSliderAxis2 = JPH::Vec3(forward[0], forward[1], forward[2]);
			settings.mNormalAxis1 = settings.mNormalAxis2 = settings.mSliderAxis1.GetNormalizedPerpendicular();
			settings.mLimitsMin = -stop;
			settings.mLimitsMax = stop;

			// FIXME: is this stupid?
			settings.mLimitsSpringSettings.mFrequency = ed->xv->friction;

			joint = new JPH::SliderConstraint(*enemybody, *aimentbody, settings);
			break;
		}
	}

	ed->rbe.joint.joint = (void *)joint;
	if (joint)
	{
		joint->SetUserData(NUM_FOR_EDICT(world->progs, ed));
		ctx->physics_system.AddConstraint(joint);
	}
}

static void QDECL World_Jolt_RunFrame(world_t *world, double frametime, double gravity)
{
	joltcontext_t *ctx = (joltcontext_t *)world->rbe;

	// no physics objects active
	if (!world->rbe_hasphysicsents)
		return;

	// fix up cvars
	if (physics_jolt_framerate->value <= 0)
		cvarfuncs->SetFloat("physics_jolt_framerate", 60);
	if (physics_jolt_maxiterationsperframe->value <= 0)
		cvarfuncs->SetFloat("physics_jolt_maxiterationsperframe", 1);

	// copy physics properties from entities to physics engine
	for (unsigned int i = 0; i < world->num_edicts; i++)
	{
		wedict_t *ed = WEDICT_NUM_PB(world->progs, i);
		if (!ED_ISFREE(ed))
			World_Jolt_BodyFromEntity(world, ed);
	}

	// oh, and it must be called after all bodies were created
	for (unsigned int i = 0; i < world->num_edicts; i++)
	{
		wedict_t *ed = WEDICT_NUM_PB(world->progs, i);
		if (!ED_ISFREE(ed))
			World_Jolt_JointFromEntity(world, ed);
	}

	// process command queue
	while (ctx->cmdqueuehead)
	{
		rbecommandqueue_t *cmd = ctx->cmdqueuehead;
		ctx->cmdqueuehead = cmd->next;
		if (!cmd->next)
			ctx->cmdqueuetail = nullptr;
		World_Jolt_RunCommand(world, cmd);
		BZ_Free(cmd);
	}

	// set gravity
	ctx->physics_system.SetGravity(JPH::Vec3Arg(0, 0, -gravity * InchesToMeters));

	// optimize broadphase only once
	if (!ctx->broadphase_optimized)
	{
		ctx->physics_system.OptimizeBroadPhase();
		ctx->broadphase_optimized = qtrue;
	}

	// step the world
	ctx->physics_system.Update(1.0f/physics_jolt_framerate->value, physics_jolt_maxiterationsperframe->value, &ctx->temp_allocator, &ctx->job_system);

	// copy physics properties from physics engine to entities
	for (unsigned int i = 0; i < world->num_edicts; i++)
	{
		wedict_t *ed = WEDICT_NUM_PB(world->progs, i);
		if (!ED_ISFREE(ed))
			World_Jolt_BodyToEntity(world, ed);
	}
}

static void QDECL World_Jolt_PushCommand(world_t *world, rbecommandqueue_t *val)
{
	joltcontext_t *ctx = (joltcontext_t *)world->rbe;
	rbecommandqueue_t *cmd = (rbecommandqueue_t *)BZ_Malloc(sizeof(*cmd));
	world->rbe_hasphysicsents = qtrue;
	memcpy(cmd, val, sizeof(*cmd));
	cmd->next = nullptr;
	if (ctx->cmdqueuehead)
	{
		rbecommandqueue_t *oldtail = ctx->cmdqueuetail;
		oldtail->next = ctx->cmdqueuetail = cmd;
	}
	else
	{
		ctx->cmdqueuetail = ctx->cmdqueuehead = cmd;
	}
}

static void QDECL World_Jolt_Trace(world_t *world, wedict_t *ed, vec3_t start, vec3_t end, trace_t *trace)
{
	joltcontext_t *ctx = (joltcontext_t *)world->rbe;
	const JPH::NarrowPhaseQuery &query = ctx->physics_system.GetNarrowPhaseQuery();
	JPH::BodyInterface &body_interface = ctx->physics_system.GetBodyInterface();

	JPH::RRayCast ray;
	ray.mOrigin = JPH::RVec3(start[0], start[1], start[2]) * InchesToMeters;
	ray.mDirection = (JPH::RVec3(end[0], end[1], end[2]) - JPH::RVec3(start[0], start[1], start[2])) * InchesToMeters;
	// Con_Printf("%f %f %f -> %f %f %f\n", ray.mOrigin[0], ray.mOrigin[1], ray.mOrigin[2], ray.mDirection[0], ray.mDirection[1], ray.mDirection[2]);
	// Con_Printf("start=%f %f %f end=%f %f %f frac=%f\n", start[0], start[1], start[2], end[0], end[1], end[2], trace->fraction);

	class BodyFilterImpl : public JPH::BodyFilter {
	public:
		virtual bool ShouldCollide(const JPH::BodyID &inBodyID) const
		{
			wedict_t *touch = WEDICT_NUM_PB(mWorld->progs, mBodyInterface->GetUserData(inBodyID));

			if (mSelf->rbe.body.body && (((JPH::Body *)mSelf->rbe.body.body)->GetID() == inBodyID))
				return false; // don't clip against own body
			if (mSelf == touch)
				return false; // don't clip against self
			if (!((int)mSelf->xv->dimension_hit & (int)touch->xv->dimension_solid))
				return false; // respect dimension value
			if ((wedict_t *)PROG_TO_EDICT(mWorld->progs, touch->v->owner) == mSelf)
				return false; // don't clip against own missiles
			if ((wedict_t *)PROG_TO_EDICT(mWorld->progs, mSelf->v->owner) == touch)
				return false; // don't clip against owner
			if (touch->v->movetype != MOVETYPE_PHYSICS)
				return false; // only touch physics objects anyway

			return true;
		}
		world_t *mWorld = nullptr;
		wedict_t *mSelf = nullptr;
		JPH::BodyInterface *mBodyInterface;
	} body_filter;

	body_filter.mWorld = world;
	body_filter.mSelf = ed;
	body_filter.mBodyInterface = &body_interface;

	JPH::BroadPhaseLayerFilter broadphase_layer_filter;
	JPH::ObjectLayerFilter object_layer_filter;

	JPH::RayCastSettings ray_settings;
	// ray_settings.SetBackFaceMode(JPH::EBackFaceMode::CollideWithBackFaces);
	// ray_settings.mTreatConvexAsSolid = false;

	class CastRayCollectorImpl : public JPH::CastRayCollector {
	public:
		virtual void AddHit(const JPH::RayCastResult &inResult) override
		{
			if (inResult.mFraction == 0.0f)
				mStartSolid = qtrue;

			mHits.push_back(inResult);
		}
		void Sort()
		{
			JPH::QuickSort(mHits.begin(), mHits.end(), [](const JPH::RayCastResult &inLHS, const JPH::RayCastResult &inRHS) { return inLHS.GetEarlyOutFraction() < inRHS.GetEarlyOutFraction(); });
		}
		JPH::Array<JPH::RayCastResult> mHits;
		qboolean mStartSolid = qfalse;
	} collector;

	collector.UpdateEarlyOutFraction(trace->fraction);

	query.CastRay(ray, ray_settings, collector, broadphase_layer_filter, object_layer_filter, body_filter);
	collector.Sort();

	// go over actual hits
	for (auto &result : collector.mHits)
	{
		// Con_Printf("HIT fraction=%f startsolid=%d\n", result.mFraction, collector.mStartSolid);

		if (collector.mStartSolid)
			result.mFraction = 1.0f;

		memset(trace, 0, sizeof(*trace));
		trace->fraction = trace->truefraction = result.mFraction;
		JPH::RVec3 endpos = ray.GetPointOnRay(result.mFraction);
		trace->endpos[0] = endpos[0] * MetersToInches;
		trace->endpos[1] = endpos[1] * MetersToInches;
		trace->endpos[2] = endpos[2] * MetersToInches;
		trace->ent = WEDICT_NUM_PB(world->progs, body_interface.GetUserData(result.mBodyID));
		trace->startsolid = collector.mStartSolid;

		// ugh
		const JPH::BodyLockInterface &lock_interface = ctx->physics_system.GetBodyLockInterface();
		JPH::BodyLockRead lock(lock_interface, result.mBodyID);
		if (lock.Succeeded())
		{
			const JPH::Body &body = lock.GetBody();
			JPH::Vec3 n = body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, endpos);
			trace->plane.normal[0] = n[0];
			trace->plane.normal[1] = n[1];
			trace->plane.normal[2] = n[2];

			lock.ReleaseLock();
		}

		break;
	}
}

static void QDECL World_Jolt_Start(world_t *world)
{
	joltcontext_t *ctx;

	// already has one somehow
	if (world->rbe)
		return;

	ctx = new joltcontext_t();
	ctx->funcs.End = World_Jolt_End;
	ctx->funcs.RemoveJointFromEntity = World_Jolt_RemoveJointFromEntity;
	ctx->funcs.RemoveFromEntity = World_Jolt_RemoveFromEntity;
#if 0
	ctx->funcs.RagMatrixToBody = World_Jolt_RagMatrixToBody;
	ctx->funcs.RagCreateBody = World_Jolt_RagCreateBody;
	ctx->funcs.RagMatrixFromJoint = World_Jolt_RagMatrixFromJoint;
	ctx->funcs.RagMatrixFromBody = World_Jolt_RagMatrixFromBody;
	ctx->funcs.RagEnableJoint = World_Jolt_RagEnableJoint;
	ctx->funcs.RagCreateJoint = World_Jolt_RagCreateJoint;
	ctx->funcs.RagDestroyBody = World_Jolt_RagDestroyBody;
	ctx->funcs.RagDestroyJoint = World_Jolt_RagDestroyJoint;
#endif
	ctx->funcs.RunFrame = World_Jolt_RunFrame;
	ctx->funcs.PushCommand = World_Jolt_PushCommand;
	ctx->funcs.Trace = World_Jolt_Trace;
	world->rbe = (rigidbodyengine_t *)ctx;

	// init jobs system
	// TODO: use FTE threads
	ctx->job_system.Init(ctx->max_physics_jobs, ctx->max_physics_barriers);

	// init physics system
	ctx->physics_system.Init(
		ctx->max_bodies,
		ctx->num_body_mutexes,
		ctx->max_body_pairs,
		ctx->max_contact_constraints,
		ctx->broad_phase_layer_interface,
		ctx->object_vs_broadphase_layer_filter,
		ctx->object_vs_object_layer_filter
	);

	ctx->broadphase_optimized = qfalse;
}

static void QDECL Plug_Jolt_Shutdown(void)
{
	if (rbefuncs)
		rbefuncs->UnregisterPhysicsEngine("Jolt");

	// do this globally rather than per-context
	JPH::UnregisterTypes();
	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;
}

#ifdef JPH_ENABLE_ASSERTS
static bool Plug_Jolt_AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, unsigned int inLine)
{
	char buffer[1024];
	Q_snprintfz(buffer, sizeof(buffer), "%s:%d: (%s) %s", inFile, inLine, inExpression, inMessage ? inMessage : "");
	Con_Printf("Jolt: %.1024s\n", buffer);
	Plug_Jolt_Shutdown();
	return false;
}
#endif // JPH_ENABLE_ASSERTS

static void Plug_Jolt_TraceImpl(const char *fmt, ...)
{
	char buffer[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

	Con_Printf("Jolt: %.1024s\n", buffer);
}

extern "C" qboolean Plug_Init(void)
{
	// get rbe funcs
	rbefuncs = (rbeplugfuncs_t *)plugfuncs->GetEngineInterface("RBE", sizeof(*rbefuncs));
	if (!rbefuncs || rbefuncs->version != RBEPLUGFUNCS_VERSION || rbefuncs->wedictsize != sizeof(wedict_t) || !rbefuncs->RegisterPhysicsEngine)
	{
		Con_Printf("Jolt: engine not compatible with physics plugins\n");
		return qfalse;
	}

	// get thread funcs
	threadfuncs = (plugthreadfuncs_t *)plugfuncs->GetEngineInterface(plugthreadfuncs_name, sizeof(*threadfuncs));
	if (!threadfuncs)
	{
		Con_Printf("Jolt: couldn't get multithreading functions\n");
		//return qfalse;
	}

	// register the shutdown func
	if (!plugfuncs->ExportFunction("Shutdown", (funcptr_t)Plug_Jolt_Shutdown))
	{
		Con_Printf("Jolt: plugfuncs->ExportFunction failed\n");
		return qfalse;
	}

	// register the engine
	if (!rbefuncs->RegisterPhysicsEngine("Jolt", World_Jolt_Start))
	{
		Con_Printf("Jolt: engine already has a physics plugin active.\n");
		return qfalse;
	}

	// get cvars
	physics_jolt_maxiterationsperframe = cvarfuncs->GetNVFDG("physics_jolt_maxiterationsperframe", "1", 0, "Maximum times to run the simulation per frame", "Jolt");
	physics_jolt_framerate = cvarfuncs->GetNVFDG("physics_jolt_framerate", "60", 0, "Physics framerate", "Jolt");
	pr_meshpitch = cvarfuncs->GetNVFDG("r_meshpitch", "-1", 0, "", "Jolt");

	// do this globally rather than per-context
	JPH::RegisterDefaultAllocator();
	JPH::Trace = Plug_Jolt_TraceImpl;
	JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = Plug_Jolt_AssertFailedImpl;)
	JPH::Factory::sInstance = new JPH::Factory;
	JPH::RegisterTypes();
	return qtrue;
}
