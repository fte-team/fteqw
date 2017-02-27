/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

//can either be built as a separate dll or statically linked in the engine (will still be treated as if dynamically)
//to enable as static, add the file to the makefile/project and edit engine/common/plugin.c to list Plug_$foo_Init


#ifndef FTEPLUGIN
#define FTEENGINE
#define FTEPLUGIN
#define pCvar_Register Cvar_Get
#define pCvar_GetFloat(x) Cvar_FindVar(x)->value
#define pSys_Error Sys_Error
#define Plug_Init Plug_Bullet_Init
#pragma comment(lib,"../../plugins/bullet/libs/bullet_dbg.lib")
#endif

#include "../../plugins/plugin.h"
#include "../../plugins/engine.h"

#include "pr_common.h"
#include "com_mesh.h"

#ifndef FTEENGINE
#define BZ_Malloc malloc
#define BZ_Free free
#define Z_Free BZ_Free
static vec3_t vec3_origin;
static int VectorCompare (const vec3_t v1, const vec3_t v2)
{
	int		i;
	for (i=0 ; i<3 ; i++)
		if (v1[i] != v2[i])
			return 0;
	return 1;
}

#endif
static modplugfuncs_t *modfuncs;


//============================================================================
// physics engine support
//============================================================================

#define DEG2RAD(d) (d * M_PI * (1/180.0f))
#define RAD2DEG(d) ((d*180) / M_PI)

#include <btBulletDynamicsCommon.h>


static void World_Bullet_RunCmd(world_t *world, rbecommandqueue_t *cmd);

void World_Bullet_Init(void)
{
	pCvar_Register("physics_bullet_enable",					"1",	0, "Bullet");
	pCvar_Register("physics_bullet_maxiterationsperframe",	"10",	0, "Bullet");
	pCvar_Register("physics_bullet_framerate",				"60",	0, "Bullet");
}

void World_Bullet_Shutdown(void)
{
}

typedef struct bulletcontext_s
{
	rigidbodyengine_t funcs;

	qboolean hasextraobjs;
//	void *ode_space;
//	void *ode_contactgroup;
	// number of constraint solver iterations to use (for dWorldStepFast)
//	int ode_iterations;
	// actual step (server frametime / ode_iterations)
//	vec_t ode_step;
	// max velocity for a 1-unit radius object at current step to prevent
	// missed collisions
//	vec_t ode_movelimit;
	rbecommandqueue_t *cmdqueuehead;
	rbecommandqueue_t *cmdqueuetail;


	world_t *gworld;


	btBroadphaseInterface *broadphase;
	btDefaultCollisionConfiguration *collisionconfig;
	btCollisionDispatcher *collisiondispatcher;
	btSequentialImpulseConstraintSolver *solver;
	btDiscreteDynamicsWorld *dworld;
	btOverlapFilterCallback *ownerfilter;
} bulletcontext_t;

class QCFilterCallback : public btOverlapFilterCallback
{
	// return true when pairs need collision
	virtual bool	needBroadphaseCollision(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1) const
	{
		//dimensions don't collide
		bool collides = (proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0;
		collides = collides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask);

		//owners don't collide (unless one is world, obviouslyish)
		if (collides)
		{
			btRigidBody *b1 = (btRigidBody*)proxy0->m_clientObject;
			btRigidBody *b2 = (btRigidBody*)proxy1->m_clientObject;
			//don't let two qc-controlled entities collide in Bullet, that's the job of quake.
			if (b1->isStaticOrKinematicObject() && b2->isStaticOrKinematicObject())
				return false;
			wedict_t *e1 = (wedict_t*)b1->getUserPointer();
			wedict_t *e2 = (wedict_t*)b2->getUserPointer();
			if ((e1->v->solid == SOLID_TRIGGER && e2->v->solid != SOLID_BSP) ||
				(e2->v->solid == SOLID_TRIGGER && e1->v->solid != SOLID_BSP))
				return false;	//triggers only collide with bsp objects.
			if (e1->entnum && e2->entnum)
				collides = e1->v->owner != e2->entnum && e2->v->owner != e1->entnum;
		}

		return collides;
	}
};

static void QDECL World_Bullet_End(world_t *world)
{
	struct bulletcontext_s *ctx = (struct bulletcontext_s*)world->rbe;
	world->rbe = NULL;
	delete ctx->dworld;
	delete ctx->solver;
	delete ctx->collisionconfig;
	delete ctx->collisiondispatcher;
	delete ctx->broadphase;
	delete ctx->ownerfilter;
	Z_Free(ctx);
}

static void QDECL World_Bullet_RemoveJointFromEntity(world_t *world, wedict_t *ed)
{
	ed->ode.ode_joint_type = 0;
//	if(ed->ode.ode_joint)
//		dJointDestroy((dJointID)ed->ode.ode_joint);
	ed->ode.ode_joint = NULL;
}

static void QDECL World_Bullet_RemoveFromEntity(world_t *world, wedict_t *ed)
{
	struct bulletcontext_s *ctx = (struct bulletcontext_s*)world->rbe;
	btRigidBody *body;
	btCollisionShape *geom;
	if (!ed->ode.ode_physics)
		return;

	// entity is not physics controlled, free any physics data
	ed->ode.ode_physics = qfalse;

	body = (btRigidBody*)ed->ode.ode_body;
	ed->ode.ode_body = NULL;
	if (body)
		ctx->dworld->removeRigidBody (body);

	geom = (btCollisionShape*)ed->ode.ode_geom;
	ed->ode.ode_geom = NULL;
	if (ed->ode.ode_geom)
		delete geom;

	//FIXME: joints
	modfuncs->ReleaseCollisionMesh(ed);
	if(ed->ode.ode_massbuf)
		BZ_Free(ed->ode.ode_massbuf);
	ed->ode.ode_massbuf = NULL;
}

static void World_Bullet_Frame_BodyToEntity(world_t *world, wedict_t *ed)
{
	return;

#if 0
	model_t *model;
	const float *avel;
	const float *o;
	const float *r; // for some reason dBodyGetRotation returns a [3][4] matrix
	const float *vel;
	btRigidBody *body = (btRigidBody*)ed->ode.ode_body;
	int movetype;
	float bodymatrix[16];
	float entitymatrix[16];
	vec3_t angles;
	vec3_t avelocity;
	vec3_t forward, left, up;
	vec3_t origin;
	vec3_t spinvelocity;
	vec3_t velocity;
	if (!body)
		return;

	movetype = (int)ed->v->movetype;
	if (movetype != MOVETYPE_PHYSICS)
	{
		switch((int)ed->xv->jointtype)
		{
			// TODO feed back data from physics
			case JOINTTYPE_POINT:
				break;
			case JOINTTYPE_HINGE:
				break;
			case JOINTTYPE_SLIDER:
				break;
			case JOINTTYPE_UNIVERSAL:
				break;
			case JOINTTYPE_HINGE2:
				break;
			case JOINTTYPE_FIXED:
				break;
		}
		return;
	}
	// store the physics engine data into the entity

	btTransform trans;
	body->getMotionState()->getWorldTransform(trans);
//	o = dBodyGetPosition(body);
//	r = dBodyGetRotation(body);
//	vel = dBodyGetLinearVel(body);
//	avel = dBodyGetAngularVel(body);
//	VectorCopy(o, origin);
//	forward[0] = r[0];
//	forward[1] = r[4];
//	forward[2] = r[8];
//	left[0] = r[1];
//	left[1] = r[5];
//	left[2] = r[9];
//	up[0] = r[2];
//	up[1] = r[6];
//	up[2] = r[10];
	vel = body->getLinearVelocity();
	avel = body->getAngularVelocity();
	VectorCopy(vel, velocity);
	VectorCopy(avel, spinvelocity);
	trans.getBasis().getOpenGLSubMatrix(bodymatrix);
	foo Matrix4x4_RM_FromVectors(bodymatrix, forward, left, up, origin);
	foo Matrix4_Multiply(ed->ode.ode_offsetimatrix, bodymatrix, entitymatrix);
	foo Matrix3x4_RM_ToVectors(entitymatrix, forward, left, up, origin);

	VectorAngles(forward, up, angles);
	angles[0]*=r_meshpitch.value;

	avelocity[PITCH] = RAD2DEG(spinvelocity[PITCH]);
	avelocity[YAW] = RAD2DEG(spinvelocity[ROLL]);
	avelocity[ROLL] = RAD2DEG(spinvelocity[YAW]);

	if (ed->v->modelindex)
	{
		model = world->Get_CModel(world, ed->v->modelindex);
		if (!model || model->type == mod_alias)
		{
			angles[PITCH] *= r_meshpitch.value;
			avelocity[PITCH] *= r_meshpitch.value;
		}
	}

	VectorCopy(origin, ed->v->origin);
	VectorCopy(velocity, ed->v->velocity);
	//VectorCopy(forward, ed->xv->axis_forward);
	//VectorCopy(left, ed->xv->axis_left);
	//VectorCopy(up, ed->xv->axis_up);
	//VectorCopy(spinvelocity, ed->xv->spinvelocity);
	VectorCopy(angles, ed->v->angles);
	VectorCopy(avelocity, ed->v->avelocity);

	// values for BodyFromEntity to check if the qc modified anything later
	VectorCopy(origin, ed->ode.ode_origin);
	VectorCopy(velocity, ed->ode.ode_velocity);
	VectorCopy(angles, ed->ode.ode_angles);
	VectorCopy(avelocity, ed->ode.ode_avelocity);
//	ed->ode.ode_gravity = (qboolean)dBodyGetGravityMode(body);

	World_LinkEdict(world, ed, true);
#endif
}

static void World_Bullet_Frame_JointFromEntity(world_t *world, wedict_t *ed)
{
#if 0
	dJointID j = 0;
	dBodyID b1 = 0;
	dBodyID b2 = 0;
	int movetype = 0;
	int jointtype = 0;
	int enemy = 0, aiment = 0;
	wedict_t *o;
	vec3_t origin, velocity, angles, forward, left, up, movedir;
	vec_t CFM, ERP, FMax, Stop, Vel;
	VectorClear(origin);
	VectorClear(velocity);
	VectorClear(angles);
	VectorClear(movedir);
	movetype = (int)ed->v->movetype;
	jointtype = (int)ed->xv->jointtype;
	enemy = ed->v->enemy;
	aiment = ed->v->aiment;
	VectorCopy(ed->v->origin, origin);
	VectorCopy(ed->v->velocity, velocity);
	VectorCopy(ed->v->angles, angles);
	VectorCopy(ed->v->movedir, movedir);
	if(movetype == MOVETYPE_PHYSICS)
		jointtype = 0; // can't have both

	o = (wedict_t*)PROG_TO_EDICT(world->progs, enemy);
	if(o->isfree || o->ode.ode_body == 0)
		enemy = 0;
	o = (wedict_t*)PROG_TO_EDICT(world->progs, aiment);
	if(o->isfree || o->ode.ode_body == 0)
		aiment = 0;
	// see http://www.ode.org/old_list_archives/2006-January/017614.html
	// we want to set ERP? make it fps independent and work like a spring constant
	// note: if movedir[2] is 0, it becomes ERP = 1, CFM = 1.0 / (H * K)
	if(movedir[0] > 0 && movedir[1] > 0)
	{
		float K = movedir[0];
		float D = movedir[1];
		float R = 2.0 * D * sqrt(K); // we assume D is premultiplied by sqrt(sprungMass)
		CFM = 1.0 / (world->ode.ode_step * K + R); // always > 0
		ERP = world->ode.ode_step * K * CFM;
		Vel = 0;
		FMax = 0;
		Stop = movedir[2];
	}
	else if(movedir[1] < 0)
	{
		CFM = 0;
		ERP = 0;
		Vel = movedir[0];
		FMax = -movedir[1]; // TODO do we need to multiply with world.physics.ode_step?
		Stop = movedir[2] > 0 ? movedir[2] : dInfinity;
	}
	else // movedir[0] > 0, movedir[1] == 0 or movedir[0] < 0, movedir[1] >= 0
	{
		CFM = 0;
		ERP = 0;
		Vel = 0;
		FMax = 0;
		Stop = dInfinity;
	}
	if(jointtype == ed->ode.ode_joint_type && VectorCompare(origin, ed->ode.ode_joint_origin) && VectorCompare(velocity, ed->ode.ode_joint_velocity) && VectorCompare(angles, ed->ode.ode_joint_angles) && enemy == ed->ode.ode_joint_enemy && aiment == ed->ode.ode_joint_aiment && VectorCompare(movedir, ed->ode.ode_joint_movedir))
		return; // nothing to do
	AngleVectorsFLU(angles, forward, left, up);
	switch(jointtype)
	{
		case JOINTTYPE_POINT:
			j = dJointCreateBall(world->ode.ode_world, 0);
			break;
		case JOINTTYPE_HINGE:
			j = dJointCreateHinge(world->ode.ode_world, 0);
			break;
		case JOINTTYPE_SLIDER:
			j = dJointCreateSlider(world->ode.ode_world, 0);
			break;
		case JOINTTYPE_UNIVERSAL:
			j = dJointCreateUniversal(world->ode.ode_world, 0);
			break;
		case JOINTTYPE_HINGE2:
			j = dJointCreateHinge2(world->ode.ode_world, 0);
			break;
		case JOINTTYPE_FIXED:
			j = dJointCreateFixed(world->ode.ode_world, 0);
			break;
		case 0:
		default:
			// no joint
			j = 0;
			break;
	}
	if(ed->ode.ode_joint)
	{
		//Con_Printf("deleted old joint %i\n", (int) (ed - prog->edicts));
		dJointAttach(ed->ode.ode_joint, 0, 0);
		dJointDestroy(ed->ode.ode_joint);
	}
	ed->ode.ode_joint = (void *) j;
	ed->ode.ode_joint_type = jointtype;
	ed->ode.ode_joint_enemy = enemy;
	ed->ode.ode_joint_aiment = aiment;
	VectorCopy(origin, ed->ode.ode_joint_origin);
	VectorCopy(velocity, ed->ode.ode_joint_velocity);
	VectorCopy(angles, ed->ode.ode_joint_angles);
	VectorCopy(movedir, ed->ode.ode_joint_movedir);
	if(j)
	{
		//Con_Printf("made new joint %i\n", (int) (ed - prog->edicts));
		dJointSetData(j, (void *) ed);
		if(enemy)
			b1 = (dBodyID)((WEDICT_NUM(world->progs, enemy))->ode.ode_body);
		if(aiment)
			b2 = (dBodyID)((WEDICT_NUM(world->progs, aiment))->ode.ode_body);
		dJointAttach(j, b1, b2);

		switch(jointtype)
		{
			case JOINTTYPE_POINT:
				dJointSetBallAnchor(j, origin[0], origin[1], origin[2]);
				break;
			case JOINTTYPE_HINGE:
				dJointSetHingeAnchor(j, origin[0], origin[1], origin[2]);
				dJointSetHingeAxis(j, forward[0], forward[1], forward[2]);
				dJointSetHingeParam(j, dParamFMax, FMax);
				dJointSetHingeParam(j, dParamHiStop, Stop);	
				dJointSetHingeParam(j, dParamLoStop, -Stop);
				dJointSetHingeParam(j, dParamStopCFM, CFM);
				dJointSetHingeParam(j, dParamStopERP, ERP);
				dJointSetHingeParam(j, dParamVel, Vel);
				break;
			case JOINTTYPE_SLIDER:
				dJointSetSliderAxis(j, forward[0], forward[1], forward[2]);
				dJointSetSliderParam(j, dParamFMax, FMax);
				dJointSetSliderParam(j, dParamHiStop, Stop);
				dJointSetSliderParam(j, dParamLoStop, -Stop);
				dJointSetSliderParam(j, dParamStopCFM, CFM);
				dJointSetSliderParam(j, dParamStopERP, ERP);
				dJointSetSliderParam(j, dParamVel, Vel);
				break;
			case JOINTTYPE_UNIVERSAL:
				dJointSetUniversalAnchor(j, origin[0], origin[1], origin[2]);
				dJointSetUniversalAxis1(j, forward[0], forward[1], forward[2]);
				dJointSetUniversalAxis2(j, up[0], up[1], up[2]);
				dJointSetUniversalParam(j, dParamFMax, FMax);
				dJointSetUniversalParam(j, dParamHiStop, Stop);
				dJointSetUniversalParam(j, dParamLoStop, -Stop);
				dJointSetUniversalParam(j, dParamStopCFM, CFM);
				dJointSetUniversalParam(j, dParamStopERP, ERP);
				dJointSetUniversalParam(j, dParamVel, Vel);
				dJointSetUniversalParam(j, dParamFMax2, FMax);
				dJointSetUniversalParam(j, dParamHiStop2, Stop);
				dJointSetUniversalParam(j, dParamLoStop2, -Stop);
				dJointSetUniversalParam(j, dParamStopCFM2, CFM);
				dJointSetUniversalParam(j, dParamStopERP2, ERP);
				dJointSetUniversalParam(j, dParamVel2, Vel);
				break;
			case JOINTTYPE_HINGE2:
				dJointSetHinge2Anchor(j, origin[0], origin[1], origin[2]);
				dJointSetHinge2Axis1(j, forward[0], forward[1], forward[2]);
				dJointSetHinge2Axis2(j, velocity[0], velocity[1], velocity[2]);
				dJointSetHinge2Param(j, dParamFMax, FMax);
				dJointSetHinge2Param(j, dParamHiStop, Stop);
				dJointSetHinge2Param(j, dParamLoStop, -Stop);
				dJointSetHinge2Param(j, dParamStopCFM, CFM);
				dJointSetHinge2Param(j, dParamStopERP, ERP);
				dJointSetHinge2Param(j, dParamVel, Vel);
				dJointSetHinge2Param(j, dParamFMax2, FMax);
				dJointSetHinge2Param(j, dParamHiStop2, Stop);
				dJointSetHinge2Param(j, dParamLoStop2, -Stop);
				dJointSetHinge2Param(j, dParamStopCFM2, CFM);
				dJointSetHinge2Param(j, dParamStopERP2, ERP);
				dJointSetHinge2Param(j, dParamVel2, Vel);
				break;
			case JOINTTYPE_FIXED:
				break;
			case 0:
			default:
				break;
		}
#undef SETPARAMS

	}
#endif
}

static qboolean QDECL World_Bullet_RagMatrixToBody(rbebody_t *bodyptr, float *mat)
{
	btRigidBody *body;

/*
	dVector3 r[3];

	r[0][0] = mat[0];
	r[0][1] = mat[1];
	r[0][2] = mat[2];
	r[1][0] = mat[4];
	r[1][1] = mat[5];
	r[1][2] = mat[6];
	r[2][0] = mat[8];
	r[2][1] = mat[9];
	r[2][2] = mat[10];

	dBodySetPosition(bodyptr->ode_body, mat[3], mat[7], mat[11]);
	dBodySetRotation(bodyptr->ode_body, r[0]);
	dBodySetLinearVel(bodyptr->ode_body, 0, 0, 0);
	dBodySetAngularVel(bodyptr->ode_body, 0, 0, 0);
*/
	return qtrue;
}
static qboolean QDECL World_Bullet_RagCreateBody(world_t *world, rbebody_t *bodyptr, rbebodyinfo_t *bodyinfo, float *mat, wedict_t *ent)
{
/*
	dMass mass;
	float radius;
	if (!world->ode.ode_space)
		return false;
	world->ode.hasodeents = true;	//I don't like this, but we need the world etc to be solid.
	world->ode.hasextraobjs = true;
	
	switch(bodyinfo->geomshape)
	{
	case GEOMTYPE_CAPSULE:
		radius = (bodyinfo->dimensions[0] + bodyinfo->dimensions[1]) * 0.5;
		bodyptr->ode_geom = (void *)dCreateCapsule(world->ode.ode_space, radius, bodyinfo->dimensions[2]);
		dMassSetCapsuleTotal(&mass, bodyinfo->mass, 3, radius, bodyinfo->dimensions[2]);
		//aligned along the geom's local z axis
		break;
	case GEOMTYPE_SPHERE:
		//radius
		radius = (bodyinfo->dimensions[0] + bodyinfo->dimensions[1] + bodyinfo->dimensions[2]) / 3;
		bodyptr->ode_geom = dCreateSphere(world->ode.ode_space, radius);
		dMassSetSphereTotal(&mass, bodyinfo->mass, radius);
		//aligned along the geom's local z axis
		break;
	case GEOMTYPE_CYLINDER:
		//radius, length
		radius = (bodyinfo->dimensions[0] + bodyinfo->dimensions[1]) * 0.5;
		bodyptr->ode_geom = dCreateCylinder(world->ode.ode_space, radius, bodyinfo->dimensions[2]);
		dMassSetCylinderTotal(&mass, bodyinfo->mass, 3, radius, bodyinfo->dimensions[2]);
		//alignment is irreleevnt, thouse I suppose it might be scaled wierdly.
		break;
	default:
	case GEOMTYPE_BOX:
		//diameter
		bodyptr->ode_geom = dCreateBox(world->ode.ode_space, bodyinfo->dimensions[0], bodyinfo->dimensions[1], bodyinfo->dimensions[2]);
		dMassSetBoxTotal(&mass, bodyinfo->mass, bodyinfo->dimensions[0], bodyinfo->dimensions[1], bodyinfo->dimensions[2]);
		//monkey
		break;
	}
	bodyptr->ode_body = dBodyCreate(world->ode.ode_world);
	dBodySetMass(bodyptr->ode_body, &mass);
	dGeomSetBody(bodyptr->ode_geom, bodyptr->ode_body);
	dGeomSetData(bodyptr->ode_geom, (void*)ent);
*/
	return World_Bullet_RagMatrixToBody(bodyptr, mat);
}

static void QDECL World_Bullet_RagMatrixFromJoint(rbejoint_t *joint, rbejointinfo_t *info, float *mat)
{
/*
	dVector3 dr3;
	switch(info->type)
	{
	case JOINTTYPE_POINT:
		dJointGetBallAnchor(joint->ode_joint, dr3);
		mat[3] = dr3[0];
		mat[7] = dr3[1];
		mat[11] = dr3[2];
		VectorClear(mat+4);
		VectorClear(mat+8);
		break;

	case JOINTTYPE_HINGE:
		dJointGetHingeAnchor(joint->ode_joint, dr3);
		mat[3] = dr3[0];
		mat[7] = dr3[1];
		mat[11] = dr3[2];

		dJointGetHingeAxis(joint->ode_joint, dr3);
		VectorCopy(dr3, mat+4);
		VectorClear(mat+8);

		CrossProduct(mat+4, mat+8, mat+0);
		return;
		break;
	case JOINTTYPE_HINGE2:
		dJointGetHinge2Anchor(joint->ode_joint, dr3);
		mat[3] = dr3[0];
		mat[7] = dr3[1];
		mat[11] = dr3[2];

		dJointGetHinge2Axis1(joint->ode_joint, dr3);
		VectorCopy(dr3, mat+4);
		dJointGetHinge2Axis2(joint->ode_joint, dr3);
		VectorCopy(dr3, mat+8);
		break;

	case JOINTTYPE_SLIDER:
		//no anchor point...
		//get the two bodies and average their origin for a somewhat usable representation of an anchor.
		{
			const dReal *p1, *p2;
			dReal n[3];
			dBodyID b1 = dJointGetBody(joint->ode_joint, 0), b2 = dJointGetBody(joint->ode_joint, 1);
			if (b1)
				p1 = dBodyGetPosition(b1);
			else
			{
				p1 = n;
				VectorClear(n);
			}
			if (b2)
				p2 = dBodyGetPosition(b2);
			else
				p2 = p1;
			dJointGetSliderAxis(joint->ode_joint, dr3 + 0);
			VectorInterpolate(p1, 0.5, p2, dr3);
			mat[3] = dr3[0];
			mat[7] = dr3[1];
			mat[11] = dr3[2];

			VectorClear(mat+4);
			VectorClear(mat+8);
		}
		break;

	case JOINTTYPE_UNIVERSAL:
		dJointGetUniversalAnchor(joint->ode_joint, dr3);
		mat[3] = dr3[0];
		mat[7] = dr3[1];
		mat[11] = dr3[2];

		dJointGetUniversalAxis1(joint->ode_joint, dr3);
		VectorCopy(dr3, mat+4);
		dJointGetUniversalAxis2(joint->ode_joint, dr3);
		VectorCopy(dr3, mat+8);

		CrossProduct(mat+4, mat+8, mat+0);
		return;
		break;
	}
	AngleVectorsFLU(vec3_origin, mat+0, mat+4, mat+8);
*/
}

static void QDECL World_Bullet_RagMatrixFromBody(world_t *world, rbebody_t *bodyptr, float *mat)
{
/*
	const dReal *o = dBodyGetPosition(bodyptr->ode_body);
	const dReal *r = dBodyGetRotation(bodyptr->ode_body);
	mat[0] = r[0];
	mat[1] = r[1];
	mat[2] = r[2];
	mat[3] = o[0];

	mat[4] = r[4];
	mat[5] = r[5];
	mat[6] = r[6];
	mat[7] = o[1];

	mat[8] = r[8];
	mat[9] = r[9];
	mat[10] = r[10];
	mat[11] = o[2];
*/
}
static void QDECL World_Bullet_RagEnableJoint(rbejoint_t *joint, qboolean enabled)
{
/*
	if (enabled)
		dJointEnable(joint->ode_joint);
	else
		dJointDisable(joint->ode_joint);
*/
}
static void QDECL World_Bullet_RagCreateJoint(world_t *world, rbejoint_t *joint, rbejointinfo_t *info, rbebody_t *body1, rbebody_t *body2, vec3_t aaa2[3])
{
/*
	switch(info->type)
	{
		case JOINTTYPE_POINT:
			joint->ode_joint = dJointCreateBall(world->ode.ode_world, 0);
			break;
		case JOINTTYPE_HINGE:
			joint->ode_joint = dJointCreateHinge(world->ode.ode_world, 0);
			break;
		case JOINTTYPE_SLIDER:
			joint->ode_joint = dJointCreateSlider(world->ode.ode_world, 0);
			break;
		case JOINTTYPE_UNIVERSAL:
			joint->ode_joint = dJointCreateUniversal(world->ode.ode_world, 0);
			break;
		case JOINTTYPE_HINGE2:
			joint->ode_joint = dJointCreateHinge2(world->ode.ode_world, 0);
			break;
		case JOINTTYPE_FIXED:
			joint->ode_joint = dJointCreateFixed(world->ode.ode_world, 0);
			break;
		default:
			joint->ode_joint = NULL;
			break;
	}
	if (joint->ode_joint)
	{
		//Con_Printf("made new joint %i\n", (int) (ed - prog->edicts));
//		dJointSetData(joint->ode_joint, NULL);
		dJointAttach(joint->ode_joint, body1?body1->ode_body:NULL, body2?body2->ode_body:NULL);

		switch(info->type)
		{
			case JOINTTYPE_POINT:
				dJointSetBallAnchor(joint->ode_joint, aaa2[0][0], aaa2[0][1], aaa2[0][2]);
				break;
			case JOINTTYPE_HINGE:
				dJointSetHingeAnchor(joint->ode_joint, aaa2[0][0], aaa2[0][1], aaa2[0][2]);
				dJointSetHingeAxis(joint->ode_joint, aaa2[1][0], aaa2[1][1], aaa2[1][2]);
				dJointSetHingeParam(joint->ode_joint, dParamFMax, info->FMax);
				dJointSetHingeParam(joint->ode_joint, dParamHiStop, info->HiStop);	
				dJointSetHingeParam(joint->ode_joint, dParamLoStop, info->LoStop);
				dJointSetHingeParam(joint->ode_joint, dParamStopCFM, info->CFM);
				dJointSetHingeParam(joint->ode_joint, dParamStopERP, info->ERP);
				dJointSetHingeParam(joint->ode_joint, dParamVel, info->Vel);
				break;
			case JOINTTYPE_SLIDER:
				dJointSetSliderAxis(joint->ode_joint, aaa2[1][0], aaa2[1][1], aaa2[1][2]);
				dJointSetSliderParam(joint->ode_joint, dParamFMax, info->FMax);
				dJointSetSliderParam(joint->ode_joint, dParamHiStop, info->HiStop);
				dJointSetSliderParam(joint->ode_joint, dParamLoStop, info->LoStop);
				dJointSetSliderParam(joint->ode_joint, dParamStopCFM, info->CFM);
				dJointSetSliderParam(joint->ode_joint, dParamStopERP, info->ERP);
				dJointSetSliderParam(joint->ode_joint, dParamVel, info->Vel);
				break;
			case JOINTTYPE_UNIVERSAL:
				dJointSetUniversalAnchor(joint->ode_joint, aaa2[0][0], aaa2[0][1], aaa2[0][2]);
				dJointSetUniversalAxis1(joint->ode_joint, aaa2[1][0], aaa2[1][1], aaa2[1][2]);
				dJointSetUniversalAxis2(joint->ode_joint, aaa2[2][0], aaa2[2][1], aaa2[2][2]);
				dJointSetUniversalParam(joint->ode_joint, dParamFMax, info->FMax);
				dJointSetUniversalParam(joint->ode_joint, dParamHiStop, info->HiStop);
				dJointSetUniversalParam(joint->ode_joint, dParamLoStop, info->LoStop);
				dJointSetUniversalParam(joint->ode_joint, dParamStopCFM, info->CFM);
				dJointSetUniversalParam(joint->ode_joint, dParamStopERP, info->ERP);
				dJointSetUniversalParam(joint->ode_joint, dParamVel, info->Vel);
				dJointSetUniversalParam(joint->ode_joint, dParamFMax2, info->FMax2);
				dJointSetUniversalParam(joint->ode_joint, dParamHiStop2, info->HiStop2);
				dJointSetUniversalParam(joint->ode_joint, dParamLoStop2, info->LoStop2);
				dJointSetUniversalParam(joint->ode_joint, dParamStopCFM2, info->CFM2);
				dJointSetUniversalParam(joint->ode_joint, dParamStopERP2, info->ERP2);
				dJointSetUniversalParam(joint->ode_joint, dParamVel2, info->Vel2);
				break;
			case JOINTTYPE_HINGE2:
				dJointSetHinge2Anchor(joint->ode_joint, aaa2[0][0], aaa2[0][1], aaa2[0][2]);
				dJointSetHinge2Axis1(joint->ode_joint, aaa2[1][0], aaa2[1][1], aaa2[1][2]);
				dJointSetHinge2Axis2(joint->ode_joint, aaa2[2][0], aaa2[2][1], aaa2[2][2]);
				dJointSetHinge2Param(joint->ode_joint, dParamFMax, info->FMax);
				dJointSetHinge2Param(joint->ode_joint, dParamHiStop, info->HiStop);
				dJointSetHinge2Param(joint->ode_joint, dParamLoStop, info->LoStop);
				dJointSetHinge2Param(joint->ode_joint, dParamStopCFM, info->CFM);
				dJointSetHinge2Param(joint->ode_joint, dParamStopERP, info->ERP);
				dJointSetHinge2Param(joint->ode_joint, dParamVel, info->Vel);
				dJointSetHinge2Param(joint->ode_joint, dParamFMax2, info->FMax2);
				dJointSetHinge2Param(joint->ode_joint, dParamHiStop2, info->HiStop2);
				dJointSetHinge2Param(joint->ode_joint, dParamLoStop2, info->LoStop2);
				dJointSetHinge2Param(joint->ode_joint, dParamStopCFM2, info->CFM2);
				dJointSetHinge2Param(joint->ode_joint, dParamStopERP2, info->ERP2);
				dJointSetHinge2Param(joint->ode_joint, dParamVel2, info->Vel2);
				break;
			case JOINTTYPE_FIXED:
				dJointSetFixed(joint->ode_joint);
				break;
		}
	}
*/
}

static void QDECL World_Bullet_RagDestroyBody(world_t *world, rbebody_t *bodyptr)
{
/*
	if (bodyptr->ode_geom)
		dGeomDestroy(bodyptr->ode_geom);
	bodyptr->ode_geom = NULL;
	if (bodyptr->ode_body)
		dBodyDestroy(bodyptr->ode_body);
	bodyptr->ode_body = NULL;
*/
}

static void QDECL World_Bullet_RagDestroyJoint(world_t *world, rbejoint_t *joint)
{
/*
	if (joint->ode_joint)
		dJointDestroy(joint->ode_joint);
	joint->ode_joint = NULL;
*/
}

//bullet gives us a handy way to get/set motion states. we can cheesily update entity fields this way.
class QCMotionState : public btMotionState
{
	wedict_t *edict;
	qboolean dirty;
	btTransform trans;
	world_t *world;


public:
	void ReloadMotionState(void)
	{
		vec3_t offset;
		vec3_t axis[3];
		btVector3 org;
		modfuncs->AngleVectors(edict->v->angles, axis[0], axis[1], axis[2]);
		VectorNegate(axis[1], axis[1]);
		VectorAvg(edict->ode.ode_mins, edict->ode.ode_maxs, offset);
		VectorMA(edict->v->origin, offset[0]*1, axis[0], org);
		VectorMA(org, offset[1]*1, axis[1], org);
		VectorMA(org, offset[2]*1, axis[2], org);

		trans.setBasis(btMatrix3x3(axis[0][0], axis[1][0], axis[2][0],
								axis[0][1],	axis[1][1],	axis[2][1],
								axis[0][2],	axis[1][2],	axis[2][2]));
		trans.setOrigin(org);
	}
	QCMotionState(wedict_t *ed, world_t *w)
	{
		dirty = qtrue;
		edict = ed;
		world = w;

		ReloadMotionState();
	}
	virtual ~QCMotionState()
	{
	}

	virtual void getWorldTransform(btTransform &worldTrans) const
	{
		worldTrans = trans;
	}

	virtual void setWorldTransform(const btTransform &worldTrans)
	{
		vec3_t fwd, left, up, offset;
		trans = worldTrans;

		btVector3 pos = worldTrans.getOrigin();
		VectorCopy(worldTrans.getBasis().getColumn(0), fwd);
		VectorCopy(worldTrans.getBasis().getColumn(1), left);
		VectorCopy(worldTrans.getBasis().getColumn(2), up);
		VectorAvg(edict->ode.ode_mins, edict->ode.ode_maxs, offset);
		VectorMA(pos, offset[0]*-1, fwd, pos);
		VectorMA(pos, offset[1]*-1, left, pos);
		VectorMA(pos, offset[2]*-1, up, edict->v->origin);

		modfuncs->VectorAngles(fwd, up, edict->v->angles);
		if (edict->v->modelindex)
		{
			model_t *model = world->Get_CModel(world, edict->v->modelindex);
			if (!model || model->type == mod_alias)
				;
			else
				edict->v->angles[PITCH] *= r_meshpitch.value;
		}

		//so it doesn't get rebuilt
		VectorCopy(edict->v->origin, edict->ode.ode_origin);
		VectorCopy(edict->v->angles, edict->ode.ode_angles);

//		World_LinkEdict(world, edict, false);

//        if(mSceneNode == nullptr)
//            return; // silently return before we set a node

//        btQuaternion rot = worldTrans.getRotation();
//        mSceneNode ->setOrientation(rot.w(), rot.x(), rot.y(), rot.z());
//        btVector3 pos = worldTrans.getOrigin();
//        mSceneNode ->setPosition(pos.x(), pos.y(), pos.z());
	}
};

static void World_Bullet_Frame_BodyFromEntity(world_t *world, wedict_t *ed)
{
	bulletcontext_t *ctx = (bulletcontext_t*)world->rbe;
	btRigidBody *body = NULL;
	btScalar mass;
	float test;
	void *dataID;
	model_t *model;
	int axisindex;
	int modelindex = 0;
	int movetype = MOVETYPE_NONE;
	int solid = SOLID_NOT;
	int geomtype = GEOMTYPE_SOLID;
	qboolean modified = qfalse;
	vec3_t angles;
	vec3_t avelocity;
	vec3_t entmaxs;
	vec3_t entmins;
	vec3_t forward;
	vec3_t geomcenter;
	vec3_t geomsize;
	vec3_t left;
	vec3_t origin;
	vec3_t spinvelocity;
	vec3_t up;
	vec3_t velocity;
	vec_t f;
	vec_t length;
	vec_t massval = 1.0f;
//	vec_t movelimit;
	vec_t radius;
	vec_t scale;
	vec_t spinlimit;
	qboolean gravity;

	geomtype = (int)ed->xv->geomtype;
	solid = (int)ed->v->solid;
	movetype = (int)ed->v->movetype;
	scale = ed->xv->scale?ed->xv->scale:1;
	modelindex = 0;
	model = NULL;

	if (!geomtype)
	{
		switch((int)ed->v->solid)
		{
		case SOLID_NOT:				geomtype = GEOMTYPE_NONE;		break;
		case SOLID_TRIGGER:			geomtype = GEOMTYPE_NONE;		break;
		case SOLID_BSP:				geomtype = GEOMTYPE_TRIMESH;	break;
		case SOLID_PHYSICS_TRIMESH:	geomtype = GEOMTYPE_TRIMESH;	break;
		case SOLID_PHYSICS_BOX:		geomtype = GEOMTYPE_BOX;		break;
		case SOLID_PHYSICS_SPHERE:	geomtype = GEOMTYPE_SPHERE;		break;
		case SOLID_PHYSICS_CAPSULE:	geomtype = GEOMTYPE_CAPSULE;	break;
		case SOLID_PHYSICS_CYLINDER:geomtype = GEOMTYPE_CYLINDER;	break;
		default:					geomtype = GEOMTYPE_BOX;		break;
		}
	}

	switch(geomtype)
	{
	case GEOMTYPE_TRIMESH:
		modelindex = (int)ed->v->modelindex;
		model = world->Get_CModel(world, modelindex);
		if (!model || model->loadstate != MLS_LOADED)
		{
			model = NULL;
			modelindex = 0;
		}
		if (model)
		{
			VectorScale(model->mins, scale, entmins);
			VectorScale(model->maxs, scale, entmaxs);
			if (ed->xv->mass)
				massval = ed->xv->mass;
		}
		else
		{
			modelindex = 0;
			massval = 1.0f;
		}
		massval = 0;	//bullet does not support trisoup moving through the world.
		break;
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
		VectorCopy(ed->v->mins, entmins);
		VectorCopy(ed->v->maxs, entmaxs);
		if (ed->xv->mass)
			massval = ed->xv->mass;
		break;
	default:
//	case GEOMTYPE_NONE:
		if (ed->ode.ode_physics)
			World_Bullet_RemoveFromEntity(world, ed);
		return;
	}

	VectorSubtract(entmaxs, entmins, geomsize);
	if (DotProduct(geomsize,geomsize) == 0)
	{
		// we don't allow point-size physics objects...
		if (ed->ode.ode_physics)
			World_Bullet_RemoveFromEntity(world, ed);
		return;
	}

	// check if we need to create or replace the geom
	if (!ed->ode.ode_physics
	 || !VectorCompare(ed->ode.ode_mins, entmins)
	 || !VectorCompare(ed->ode.ode_maxs, entmaxs)
	 || ed->ode.ode_modelindex != modelindex)
	{
		btCollisionShape *geom;

		modified = qtrue;
		World_Bullet_RemoveFromEntity(world, ed);
		ed->ode.ode_physics = qtrue;
		VectorCopy(entmins, ed->ode.ode_mins);
		VectorCopy(entmaxs, ed->ode.ode_maxs);
		ed->ode.ode_modelindex = modelindex;
		VectorAvg(entmins, entmaxs, geomcenter);
		ed->ode.ode_movelimit = min(geomsize[0], min(geomsize[1], geomsize[2]));

/*		memset(ed->ode.ode_offsetmatrix, 0, sizeof(ed->ode.ode_offsetmatrix));
		ed->ode.ode_offsetmatrix[0] = 1;
		ed->ode.ode_offsetmatrix[5] = 1;
		ed->ode.ode_offsetmatrix[10] = 1;
		ed->ode.ode_offsetmatrix[3] = -geomcenter[0];
		ed->ode.ode_offsetmatrix[7] = -geomcenter[1];
		ed->ode.ode_offsetmatrix[11] = -geomcenter[2];
*/
		ed->ode.ode_mass = massval;

		switch(geomtype)
		{
		case GEOMTYPE_TRIMESH:
//			foo Matrix4x4_Identity(ed->ode.ode_offsetmatrix);
			geom = NULL;
			if (!model)
			{
				Con_Printf("entity %i (classname %s) has no model\n", NUM_FOR_EDICT(world->progs, (edict_t*)ed), PR_GetString(world->progs, ed->v->classname));
				if (ed->ode.ode_physics)
					World_Bullet_RemoveFromEntity(world, ed);
				return;
			}
			if (!modfuncs->GenerateCollisionMesh(world, model, ed, geomcenter))
			{
				if (ed->ode.ode_physics)
					World_Bullet_RemoveFromEntity(world, ed);
				return;
			}

//			foo Matrix4x4_RM_CreateTranslate(ed->ode.ode_offsetmatrix, geomcenter[0], geomcenter[1], geomcenter[2]);

			{
				btTriangleIndexVertexArray *tiva = new btTriangleIndexVertexArray();
				btIndexedMesh mesh;
				mesh.m_vertexType = PHY_FLOAT;
				mesh.m_indexType = PHY_INTEGER;
				mesh.m_numTriangles = ed->ode.ode_numtriangles;
				mesh.m_numVertices = ed->ode.ode_numvertices;
				mesh.m_triangleIndexBase = (const unsigned char*)ed->ode.ode_element3i;
				mesh.m_triangleIndexStride = sizeof(*ed->ode.ode_element3i)*3;
				mesh.m_vertexBase = (const unsigned char*)ed->ode.ode_vertex3f;
				mesh.m_vertexStride = sizeof(*ed->ode.ode_vertex3f)*3;
				tiva->addIndexedMesh(mesh);
				geom = new btBvhTriangleMeshShape(tiva, true);
			}
			break;

		case GEOMTYPE_BOX:
			geom = new btBoxShape(btVector3(geomsize[0], geomsize[1], geomsize[2]) * 0.5);
			break;

		case GEOMTYPE_SPHERE:
			geom = new btSphereShape(geomsize[0] * 0.5f);
			break;

		case GEOMTYPE_CAPSULE:
		case GEOMTYPE_CAPSULE_X:
		case GEOMTYPE_CAPSULE_Y:
		case GEOMTYPE_CAPSULE_Z:
			if (geomtype == GEOMTYPE_CAPSULE)
			{
				// the qc gives us 3 axis radius, the longest axis is the capsule axis
				axisindex = 0;
				if (geomsize[axisindex] < geomsize[1])
					axisindex = 1;
				if (geomsize[axisindex] < geomsize[2])
					axisindex = 2;
			}
			else
				axisindex = geomtype-GEOMTYPE_CAPSULE_X;
			if (axisindex == 0)
				radius = min(geomsize[1], geomsize[2]) * 0.5f;
			else if (axisindex == 1)
				radius = min(geomsize[0], geomsize[2]) * 0.5f;
			else
				radius = min(geomsize[0], geomsize[1]) * 0.5f;
			length = geomsize[axisindex] - radius*2;
			if (length <= 0)
			{
				radius -= (1 - length)*0.5;
				length = 1;
			}
			if (axisindex == 0)
				geom = new btCapsuleShapeX(radius, length);
			else if (axisindex == 1)
				geom = new btCapsuleShape(radius, length);
			else
				geom = new btCapsuleShapeZ(radius, length);
			break;

		case GEOMTYPE_CYLINDER:
		case GEOMTYPE_CYLINDER_X:
		case GEOMTYPE_CYLINDER_Y:
		case GEOMTYPE_CYLINDER_Z:
			if (geomtype == GEOMTYPE_CYLINDER)
			{
				// the qc gives us 3 axis radius, the longest axis is the capsule axis
				axisindex = 0;
				if (geomsize[axisindex] < geomsize[1])
					axisindex = 1;
				if (geomsize[axisindex] < geomsize[2])
					axisindex = 2;
			}
			else
				axisindex = geomtype-GEOMTYPE_CYLINDER_X;
			if (axisindex == 0)
				geom = new btCylinderShapeX(btVector3(geomsize[0], geomsize[1], geomsize[2])*0.5);
			else if (axisindex == 1)
				geom = new btCylinderShape(btVector3(geomsize[0], geomsize[1], geomsize[2])*0.5);
			else
				geom = new btCylinderShapeZ(btVector3(geomsize[0], geomsize[1], geomsize[2])*0.5);
			break;

		default:
//			Con_Printf("World_Bullet_BodyFromEntity: unrecognized solid value %i was accepted by filter\n", solid);
			if (ed->ode.ode_physics)
				World_Bullet_RemoveFromEntity(world, ed);
			return;
		}
//		Matrix3x4_InvertTo4x4_Simple(ed->ode.ode_offsetmatrix, ed->ode.ode_offsetimatrix);
//		ed->ode.ode_massbuf = BZ_Malloc(sizeof(dMass));
//		memcpy(ed->ode.ode_massbuf, &mass, sizeof(dMass));

		ed->ode.ode_geom = (void *)geom;
	}

	//non-moving objects need to be static objects (and thus need 0 mass)
	if (movetype != MOVETYPE_PHYSICS && movetype != MOVETYPE_WALK) //enabling kinematic objects for everything else destroys framerates (!movetype || movetype == MOVETYPE_PUSH)
		massval = 0;

	//if the mass changes, we'll need to create a new body (but not the shape, so invalidate the current one)
	if (ed->ode.ode_mass != massval)
	{
		ed->ode.ode_mass = massval;
		body = (btRigidBody*)ed->ode.ode_body;
		if (body)
			ctx->dworld->removeRigidBody(body);
		ed->ode.ode_body = NULL;
	}

//	if(ed->ode.ode_geom)
//		dGeomSetData(ed->ode.ode_geom, (void*)ed);
	if (movetype == MOVETYPE_PHYSICS && ed->ode.ode_mass)
	{
		if (ed->ode.ode_body == NULL)
		{
//			ed->ode.ode_body = (void *)(body = dBodyCreate(world->ode.ode_world));
//			dGeomSetBody(ed->ode.ode_geom, body);
//			dBodySetData(body, (void*)ed);
//			dBodySetMass(body, (dMass *) ed->ode.ode_massbuf);

			btVector3 fallInertia(0, 0, 0);
			((btCollisionShape*)ed->ode.ode_geom)->calculateLocalInertia(ed->ode.ode_mass, fallInertia);
			btRigidBody::btRigidBodyConstructionInfo fallRigidBodyCI(ed->ode.ode_mass, new QCMotionState(ed,world), (btCollisionShape*)ed->ode.ode_geom, fallInertia);
			body = new btRigidBody(fallRigidBodyCI);
			body->setUserPointer(ed);
//			btTransform trans;
//			trans.setFromOpenGLMatrix(ed->ode.ode_offsetmatrix);
//			body->setCenterOfMassTransform(trans);
			ed->ode.ode_body = (void*)body;

			//continuous collision detection prefers using a sphere within the object. tbh I have no idea what values to specify.
			body->setCcdMotionThreshold((geomsize[0]+geomsize[1]+geomsize[2])*(4/3));
			body->setCcdSweptSphereRadius((geomsize[0]+geomsize[1]+geomsize[2])*(0.5/3));

			ctx->dworld->addRigidBody(body, ed->xv->dimension_solid, ed->xv->dimension_hit);

			modified = qtrue;
		}
	}
	else
	{
		if (ed->ode.ode_body == NULL)
		{
			btRigidBody::btRigidBodyConstructionInfo rbci(ed->ode.ode_mass, new QCMotionState(ed,world), (btCollisionShape*)ed->ode.ode_geom, btVector3(0, 0, 0));
			body = new btRigidBody(rbci);
			body->setUserPointer(ed);
//			btTransform trans;
//			trans.setFromOpenGLMatrix(ed->ode.ode_offsetmatrix);
//			body->setCenterOfMassTransform(trans);
			ed->ode.ode_body = (void*)body;
			if (ed->ode.ode_mass)
				body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
			else
				body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
			ctx->dworld->addRigidBody(body, ed->xv->dimension_solid, ed->xv->dimension_hit);

			modified = qtrue;
		}
	}

	body = (btRigidBody*)ed->ode.ode_body;

	// get current data from entity
	gravity = qtrue;
	VectorCopy(ed->v->origin, origin);
	VectorCopy(ed->v->velocity, velocity);
	VectorCopy(ed->v->angles, angles);
	VectorCopy(ed->v->avelocity, avelocity);
	if (ed == world->edicts || (ed->xv->gravity && ed->xv->gravity <= 0.01))
		gravity = qfalse;

	// compatibility for legacy entities
//	if (!DotProduct(forward,forward) || solid == SOLID_BSP)
	{
		vec3_t qangles, qavelocity;
		VectorCopy(angles, qangles);
		VectorCopy(avelocity, qavelocity);
	
		if (ed->v->modelindex)
		{
			model = world->Get_CModel(world, ed->v->modelindex);
			if (!model || model->type == mod_alias)
			{
				qangles[PITCH] *= r_meshpitch.value;
				qavelocity[PITCH] *= r_meshpitch.value;
			}
		}

		modfuncs->AngleVectors(qangles, forward, left, up);
		VectorNegate(left,left);
		// convert single-axis rotations in avelocity to spinvelocity
		// FIXME: untested math - check signs
		VectorSet(spinvelocity, DEG2RAD(qavelocity[PITCH]), DEG2RAD(qavelocity[ROLL]), DEG2RAD(qavelocity[YAW]));
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


	// we must prevent NANs...
	test = DotProduct(origin,origin) + DotProduct(forward,forward) + DotProduct(left,left) + DotProduct(up,up) + DotProduct(velocity,velocity) + DotProduct(spinvelocity,spinvelocity);
	if (IS_NAN(test))
	{
		modified = qtrue;
		//Con_Printf("Fixing NAN values on entity %i : .classname = \"%s\" .origin = '%f %f %f' .velocity = '%f %f %f' .axis_forward = '%f %f %f' .axis_left = '%f %f %f' .axis_up = %f %f %f' .spinvelocity = '%f %f %f'\n", PRVM_NUM_FOR_EDICT(ed), PRVM_GetString(PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.classname)->string), origin[0], origin[1], origin[2], velocity[0], velocity[1], velocity[2], forward[0], forward[1], forward[2], left[0], left[1], left[2], up[0], up[1], up[2], spinvelocity[0], spinvelocity[1], spinvelocity[2]);
		Con_Printf("Fixing NAN values on entity %i : .classname = \"%s\" .origin = '%f %f %f' .velocity = '%f %f %f' .angles = '%f %f %f' .avelocity = '%f %f %f'\n", NUM_FOR_EDICT(world->progs, (edict_t*)ed), PR_GetString(world->progs, ed->v->classname), origin[0], origin[1], origin[2], velocity[0], velocity[1], velocity[2], angles[0], angles[1], angles[2], avelocity[0], avelocity[1], avelocity[2]);
		test = DotProduct(origin,origin);
		if (IS_NAN(test))
			VectorClear(origin);
		test = DotProduct(forward,forward) * DotProduct(left,left) * DotProduct(up,up);
		if (IS_NAN(test))
		{
			VectorSet(angles, 0, 0, 0);
			VectorSet(forward, 1, 0, 0);
			VectorSet(left, 0, 1, 0);
			VectorSet(up, 0, 0, 1);
		}
		test = DotProduct(velocity,velocity);
		if (IS_NAN(test))
			VectorClear(velocity);
		test = DotProduct(spinvelocity,spinvelocity);
		if (IS_NAN(test))
		{
			VectorClear(avelocity);
			VectorClear(spinvelocity);
		}
	}

	// check if the qc edited any position data
	if (
		0//!VectorCompare(origin, ed->ode.ode_origin)
	 || !VectorCompare(velocity, ed->ode.ode_velocity)
	 //|| !VectorCompare(angles, ed->ode.ode_angles)
	 || !VectorCompare(avelocity, ed->ode.ode_avelocity)
	 || gravity != ed->ode.ode_gravity)
		modified = qtrue;

	// store the qc values into the physics engine
	body = (btRigidBody*)ed->ode.ode_body;
	if (modified && body)
	{
//		dVector3 r[3];
		float entitymatrix[16];
		float bodymatrix[16];

#if 0
		Con_Printf("entity %i got changed by QC\n", (int) (ed - prog->edicts));
		if(!VectorCompare(origin, ed->ode.ode_origin))
			Con_Printf("  origin: %f %f %f -> %f %f %f\n", ed->ode.ode_origin[0], ed->ode.ode_origin[1], ed->ode.ode_origin[2], origin[0], origin[1], origin[2]);
		if(!VectorCompare(velocity, ed->ode.ode_velocity))
			Con_Printf("  velocity: %f %f %f -> %f %f %f\n", ed->ode.ode_velocity[0], ed->ode.ode_velocity[1], ed->ode.ode_velocity[2], velocity[0], velocity[1], velocity[2]);
		if(!VectorCompare(angles, ed->ode.ode_angles))
			Con_Printf("  angles: %f %f %f -> %f %f %f\n", ed->ode.ode_angles[0], ed->ode.ode_angles[1], ed->ode.ode_angles[2], angles[0], angles[1], angles[2]);
		if(!VectorCompare(avelocity, ed->ode.ode_avelocity))
			Con_Printf("  avelocity: %f %f %f -> %f %f %f\n", ed->ode.ode_avelocity[0], ed->ode.ode_avelocity[1], ed->ode.ode_avelocity[2], avelocity[0], avelocity[1], avelocity[2]);
		if(gravity != ed->ode.ode_gravity)
			Con_Printf("  gravity: %i -> %i\n", ed->ide.ode_gravity, gravity);
#endif

		// values for BodyFromEntity to check if the qc modified anything later
		VectorCopy(origin, ed->ode.ode_origin);
		VectorCopy(velocity, ed->ode.ode_velocity);
		VectorCopy(angles, ed->ode.ode_angles);
		VectorCopy(avelocity, ed->ode.ode_avelocity);
		ed->ode.ode_gravity = gravity;

//		foo Matrix4x4_RM_FromVectors(entitymatrix, forward, left, up, origin);
//		foo Matrix4_Multiply(ed->ode.ode_offsetmatrix, entitymatrix, bodymatrix);
//		foo Matrix3x4_RM_ToVectors(bodymatrix, forward, left, up, origin);

//		r[0][0] = forward[0];
//		r[1][0] = forward[1];
//		r[2][0] = forward[2];
//		r[0][1] = left[0];
//		r[1][1] = left[1];
//		r[2][1] = left[2];
//		r[0][2] = up[0];
//		r[1][2] = up[1];
//		r[2][2] = up[2];

		QCMotionState *ms = (QCMotionState*)body->getMotionState();
		ms->ReloadMotionState();
		body->setMotionState(ms);
		body->setLinearVelocity(btVector3(velocity[0], velocity[1], velocity[2]));
		body->setAngularVelocity(btVector3(spinvelocity[0], spinvelocity[1], spinvelocity[2]));
//		body->setGravity(btVector3(ed->xv->gravitydir[0], ed->xv->gravitydir[1], ed->xv->gravitydir[2]) * ed->xv->gravity);

		//something changed. make sure it still falls over appropriately
		body->setActivationState(1);
	}

/* FIXME: check if we actually need this insanity with bullet (ode sucks).
	if(body)
	{
		// limit movement speed to prevent missed collisions at high speed
		btVector3 ovelocity = body->getLinearVelocity();
		btVector3 ospinvelocity = body->getAngularVelocity();
		movelimit = ed->ode.ode_movelimit * world->ode.ode_movelimit;
		test = DotProduct(ovelocity,ovelocity);
		if (test > movelimit*movelimit)
		{
			// scale down linear velocity to the movelimit
			// scale down angular velocity the same amount for consistency
			f = movelimit / sqrt(test);
			VectorScale(ovelocity, f, velocity);
			VectorScale(ospinvelocity, f, spinvelocity);
			body->setLinearVelocity(btVector3(velocity[0], velocity[1], velocity[2]));
			body->setAngularVelocity(btVector3(spinvelocity[0], spinvelocity[1], spinvelocity[2]));
		}

		// make sure the angular velocity is not exploding
		spinlimit = physics_bullet_spinlimit.value;
		test = DotProduct(ospinvelocity,ospinvelocity);
		if (test > spinlimit)
		{
			body->setAngularVelocity(btVector3(0, 0, 0));
		}
	}
*/
}

/*
#define MAX_CONTACTS 16
static void VARGS nearCallback (void *data, dGeomID o1, dGeomID o2)
{
	world_t *world = (world_t *)data;
	dContact contact[MAX_CONTACTS]; // max contacts per collision pair
	dBodyID b1;
	dBodyID b2;
	dJointID c;
	int i;
	int numcontacts;

	float bouncefactor1 = 0.0f;
	float bouncestop1 = 60.0f / 800.0f;
	float bouncefactor2 = 0.0f;
	float bouncestop2 = 60.0f / 800.0f;
	float erp;
	dVector3 grav;
	wedict_t *ed1, *ed2;

	if (dGeomIsSpace(o1) || dGeomIsSpace(o2))
	{
		// colliding a space with something
		dSpaceCollide2(o1, o2, data, &nearCallback);
		// Note we do not want to test intersections within a space,
		// only between spaces.
		//if (dGeomIsSpace(o1)) dSpaceCollide(o1, data, &nearCallback);
		//if (dGeomIsSpace(o2)) dSpaceCollide(o2, data, &nearCallback);
		return;
	}

	b1 = dGeomGetBody(o1);
	b2 = dGeomGetBody(o2);

	// at least one object has to be using MOVETYPE_PHYSICS or we just don't care
	if (!b1 && !b2)
		return;

	// exit without doing anything if the two bodies are connected by a joint
	if (b1 && b2 && dAreConnectedExcluding(b1, b2, dJointTypeContact))
		return;

	ed1 = (wedict_t *) dGeomGetData(o1);
	ed2 = (wedict_t *) dGeomGetData(o2);
	if (ed1 == ed2 && ed1)
	{
		//ragdolls don't make contact with the bbox of the doll entity
		//the origional entity should probably not be solid anyway.
		//these bodies should probably not collide against bboxes of other entities with ragdolls either, but meh.
		if (ed1->ode.ode_body == b1 || ed2->ode.ode_body == b2)
			return;
	}
	if(!ed1 || ed1->isfree)
		ed1 = world->edicts;
	if(!ed2 || ed2->isfree)
		ed2 = world->edicts;

	// generate contact points between the two non-space geoms
	numcontacts = dCollide(o1, o2, MAX_CONTACTS, &(contact[0].geom), sizeof(contact[0]));
	if (numcontacts)
	{
		if(ed1 && ed1->v->touch)
		{
			world->Event_Touch(world, ed1, ed2);
		}
		if(ed2 && ed2->v->touch)
		{
			world->Event_Touch(world, ed2, ed1);
		}

		// if either ent killed itself, don't collide 
		if ((ed1&&ed1->isfree) || (ed2&&ed2->isfree))
			return;
	}

	if(ed1)
	{
		if (ed1->xv->bouncefactor)
			bouncefactor1 = ed1->xv->bouncefactor;

		if (ed1->xv->bouncestop)
			bouncestop1 = ed1->xv->bouncestop;
	}

	if(ed2)
	{
		if (ed2->xv->bouncefactor)
			bouncefactor2 = ed2->xv->bouncefactor;

		if (ed2->xv->bouncestop)
			bouncestop2 = ed2->xv->bouncestop;
	}

	if ((ed2->entnum&&ed1->v->owner == ed2->entnum) || (ed1->entnum&&ed2->v->owner == ed1->entnum))
		return;

	// merge bounce factors and bounce stop
	if(bouncefactor2 > 0)
	{
		if(bouncefactor1 > 0)
		{
			// TODO possibly better logic to merge bounce factor data?
			if(bouncestop2 < bouncestop1)
				bouncestop1 = bouncestop2;
			if(bouncefactor2 > bouncefactor1)
				bouncefactor1 = bouncefactor2;
		}
		else
		{
			bouncestop1 = bouncestop2;
			bouncefactor1 = bouncefactor2;
		}
	}
	dWorldGetGravity(world->ode.ode_world, grav);
	bouncestop1 *= fabs(grav[2]);

	erp = (DotProduct(ed1->v->velocity, ed1->v->velocity) > DotProduct(ed2->v->velocity, ed2->v->velocity)) ? ed1->xv->erp : ed2->xv->erp;

	// add these contact points to the simulation
	for (i = 0;i < numcontacts;i++)
	{
		contact[i].surface.mode =	(physics_bullet_contact_mu.value != -1 ? dContactApprox1 : 0) |
									(physics_bullet_contact_erp.value != -1 ? dContactSoftERP : 0) |
									(physics_bullet_contact_cfm.value != -1 ? dContactSoftCFM : 0) |
									(bouncefactor1 > 0 ? dContactBounce : 0);
		contact[i].surface.mu = physics_bullet_contact_mu.value;
		if (ed1->xv->friction)
			contact[i].surface.mu *= ed1->xv->friction;
		if (ed2->xv->friction)
			contact[i].surface.mu *= ed2->xv->friction;
		contact[i].surface.mu2 = 0;
		contact[i].surface.soft_erp = physics_bullet_contact_erp.value + erp;
		contact[i].surface.soft_cfm = physics_bullet_contact_cfm.value;
		contact[i].surface.bounce = bouncefactor1;
		contact[i].surface.bounce_vel = bouncestop1;
		c = dJointCreateContact(world->ode.ode_world, world->ode.ode_contactgroup, contact + i);
		dJointAttach(c, b1, b2);
	}
}
*/

static void QDECL World_Bullet_Frame(world_t *world, double frametime, double gravity)
{
	struct bulletcontext_s *ctx = (struct bulletcontext_s*)world->rbe;
	if (world->rbe_hasphysicsents || ctx->hasextraobjs)
	{
		int i;
		wedict_t *ed;

//		world->ode.ode_iterations = bound(1, physics_bullet_iterationsperframe.ival, 1000);
//		world->ode.ode_step = frametime / world->ode.ode_iterations;
//		world->ode.ode_movelimit = physics_bullet_movelimit.value / world->ode.ode_step;


		// copy physics properties from entities to physics engine
		for (i = 0;i < world->num_edicts;i++)
		{
			ed = (wedict_t*)EDICT_NUM(world->progs, i);
			if (!ed->isfree)
				World_Bullet_Frame_BodyFromEntity(world, ed);
		}
		// oh, and it must be called after all bodies were created
		for (i = 0;i < world->num_edicts;i++)
		{
			ed = (wedict_t*)EDICT_NUM(world->progs, i);
			if (!ed->isfree)
				World_Bullet_Frame_JointFromEntity(world, ed);
		}
		while(ctx->cmdqueuehead)
		{
			rbecommandqueue_t *cmd = ctx->cmdqueuehead;
			ctx->cmdqueuehead = cmd->next;
			if (!cmd->next)
				ctx->cmdqueuetail = NULL;
			World_Bullet_RunCmd(world, cmd);
			Z_Free(cmd);
		}

		ctx->dworld->setGravity(btVector3(0, 0, -gravity));

		ctx->dworld->stepSimulation(frametime, max(0, pCvar_GetFloat("physics_bullet_maxiterationsperframe")), 1/bound(1, pCvar_GetFloat("physics_bullet_framerate"), 500));

		// set the tolerance for closeness of objects
//		dWorldSetContactSurfaceLayer(world->ode.ode_world, max(0, physics_bullet_contactsurfacelayer.value));

		// run collisions for the current world state, creating JointGroup
//		dSpaceCollide(world->ode.ode_space, (void *)world, nearCallback);

		// run physics (move objects, calculate new velocities)
//		if (physics_bullet_worldquickstep.ival)
//		{
//			dWorldSetQuickStepNumIterations(world->ode.ode_world, bound(1, physics_bullet_worldquickstep_iterations.ival, 200));
//			dWorldQuickStep(world->ode.ode_world, world->ode.ode_step);
//		}
//		else
//			dWorldStep(world->ode.ode_world, world->ode.ode_step);

		// clear the JointGroup now that we're done with it
//		dJointGroupEmpty(world->ode.ode_contactgroup);

		if (world->rbe_hasphysicsents)
		{
			// copy physics properties from physics engine to entities
			for (i = 1;i < world->num_edicts;i++)
			{
				ed = (wedict_t*)EDICT_NUM(world->progs, i);
				if (!ed->isfree)
					World_Bullet_Frame_BodyToEntity(world, ed);
			}
		}
	}
}

static void World_Bullet_RunCmd(world_t *world, rbecommandqueue_t *cmd)
{
	btRigidBody *body = (btRigidBody*)(cmd->edict->ode.ode_body);
	switch(cmd->command)
	{
	case RBECMD_ENABLE:
		if (body)
			body->setActivationState(1);
		break;
	case RBECMD_DISABLE:
		if (body)
			body->setActivationState(0);
		break;
	case RBECMD_FORCE:
		if (body)
		{
			body->setActivationState(1);
			body->applyForce(btVector3(cmd->v1[0], cmd->v1[1], cmd->v1[2]), btVector3(cmd->v2[0], cmd->v2[1], cmd->v2[2]));
		}
		break;
	case RBECMD_TORQUE:
		if (cmd->edict->ode.ode_body)
		{
			body->setActivationState(1);
			body->applyTorque(btVector3(cmd->v1[0], cmd->v1[1], cmd->v1[2]));
		}
		break;
	}
}

static void QDECL World_Bullet_PushCommand(world_t *world, rbecommandqueue_t *val)
{
	struct bulletcontext_s *ctx = (struct bulletcontext_s*)world->rbe;
	rbecommandqueue_t *cmd = (rbecommandqueue_t*)BZ_Malloc(sizeof(*cmd));
	world->rbe_hasphysicsents = qtrue;	//just in case.
	memcpy(cmd, val, sizeof(*cmd));
	cmd->next = NULL;
	//add on the end of the queue, so that order is preserved.
	if (ctx->cmdqueuehead)
	{
		rbecommandqueue_t *ot = ctx->cmdqueuetail;
		ot->next = ctx->cmdqueuetail = cmd;
	}
	else
		ctx->cmdqueuetail = ctx->cmdqueuehead = cmd;
}

static void QDECL World_Bullet_TraceEntity(world_t *world, vec3_t start, vec3_t end, wedict_t *ed)
{
	struct bulletcontext_s *ctx = (struct bulletcontext_s*)world->rbe;
	btCollisionShape *shape = (btCollisionShape*)ed->ode.ode_geom;

	class myConvexResultCallback : public btCollisionWorld::ConvexResultCallback
	{
	public:
		virtual	btScalar	addSingleResult(btCollisionWorld::LocalConvexResult& convexResult,bool normalInWorldSpace)
		{
			return 0;
		}
	} result;

	btTransform from(btMatrix3x3(1, 0, 0, 0, 1, 0, 0, 0, 1), btVector3(start[0], start[1], start[2]));
	btTransform to(btMatrix3x3(1, 0, 0, 0, 1, 0, 0, 0, 1), btVector3(end[0], end[1], end[2]));
	ctx->dworld->convexSweepTest((btConvexShape*)shape, from, to, result, 1);
}

static void QDECL World_Bullet_Start(world_t *world)
{
	struct bulletcontext_s *ctx;
	if (world->rbe)
		return;	//no thanks, we already have one. somehow.

	if (!pCvar_GetFloat("physics_bullet_enable"))
		return;

	ctx = (struct bulletcontext_s*)BZ_Malloc(sizeof(*ctx));
	memset(ctx, 0, sizeof(*ctx));
	ctx->gworld = world;
	ctx->funcs.End						= World_Bullet_End;
	ctx->funcs.RemoveJointFromEntity	= World_Bullet_RemoveJointFromEntity;
	ctx->funcs.RemoveFromEntity			= World_Bullet_RemoveFromEntity;
	ctx->funcs.RagMatrixToBody			= World_Bullet_RagMatrixToBody;
	ctx->funcs.RagCreateBody			= World_Bullet_RagCreateBody;
	ctx->funcs.RagMatrixFromJoint		= World_Bullet_RagMatrixFromJoint;
	ctx->funcs.RagMatrixFromBody		= World_Bullet_RagMatrixFromBody;
	ctx->funcs.RagEnableJoint			= World_Bullet_RagEnableJoint;
	ctx->funcs.RagCreateJoint			= World_Bullet_RagCreateJoint;
	ctx->funcs.RagDestroyBody			= World_Bullet_RagDestroyBody;
	ctx->funcs.RagDestroyJoint			= World_Bullet_RagDestroyJoint;
	ctx->funcs.Frame					= World_Bullet_Frame;
	ctx->funcs.PushCommand				= World_Bullet_PushCommand;
	world->rbe = &ctx->funcs;


	ctx->broadphase = new btDbvtBroadphase();
	ctx->collisionconfig = new btDefaultCollisionConfiguration();
	ctx->collisiondispatcher = new btCollisionDispatcher(ctx->collisionconfig);
	ctx->solver = new btSequentialImpulseConstraintSolver;
	ctx->dworld = new btDiscreteDynamicsWorld(ctx->collisiondispatcher, ctx->broadphase, ctx->solver, ctx->collisionconfig);

	ctx->ownerfilter = new QCFilterCallback();
	ctx->dworld->getPairCache()->setOverlapFilterCallback(ctx->ownerfilter);



	ctx->dworld->setGravity(btVector3(0, -10, 0));

/*
	if(physics_bullet_world_erp.value >= 0)
		dWorldSetERP(world->ode.ode_world, physics_bullet_world_erp.value);
	if(physics_bullet_world_cfm.value >= 0)
		dWorldSetCFM(world->ode.ode_world, physics_bullet_world_cfm.value);
	if (physics_bullet_world_damping.ival)
	{
		dWorldSetLinearDamping(world->ode.ode_world, (physics_bullet_world_damping_linear.value >= 0) ? (physics_bullet_world_damping_linear.value * physics_bullet_world_damping.value) : 0);
		dWorldSetLinearDampingThreshold(world->ode.ode_world, (physics_bullet_world_damping_linear_threshold.value >= 0) ? (physics_bullet_world_damping_linear_threshold.value * physics_bullet_world_damping.value) : 0);
		dWorldSetAngularDamping(world->ode.ode_world, (physics_bullet_world_damping_angular.value >= 0) ? (physics_bullet_world_damping_angular.value * physics_bullet_world_damping.value) : 0);
		dWorldSetAngularDampingThreshold(world->ode.ode_world, (physics_bullet_world_damping_angular_threshold.value >= 0) ? (physics_bullet_world_damping_angular_threshold.value * physics_bullet_world_damping.value) : 0);
	}
	else
	{
		dWorldSetLinearDamping(world->ode.ode_world, 0);
		dWorldSetLinearDampingThreshold(world->ode.ode_world, 0);
		dWorldSetAngularDamping(world->ode.ode_world, 0);
		dWorldSetAngularDampingThreshold(world->ode.ode_world, 0);
	}
	if (physics_bullet_autodisable.ival)
	{
		dWorldSetAutoDisableSteps(world->ode.ode_world, bound(1, physics_bullet_autodisable_steps.ival, 100)); 
		dWorldSetAutoDisableTime(world->ode.ode_world, physics_bullet_autodisable_time.value);
		dWorldSetAutoDisableAverageSamplesCount(world->ode.ode_world, bound(1, physics_bullet_autodisable_threshold_samples.ival, 100));
		dWorldSetAutoDisableLinearThreshold(world->ode.ode_world, physics_bullet_autodisable_threshold_linear.value); 
		dWorldSetAutoDisableAngularThreshold(world->ode.ode_world, physics_bullet_autodisable_threshold_angular.value); 
		dWorldSetAutoDisableFlag (world->ode.ode_world, true);
	}
	else
		dWorldSetAutoDisableFlag (world->ode.ode_world, false);
	*/
}

static qintptr_t QDECL World_Bullet_Shutdown(qintptr_t *args)
{
	if (modfuncs)
		modfuncs->UnregisterPhysicsEngine("Bullet");
	return 0;
}

static bool World_Bullet_DoInit(void)
{
	if (!modfuncs || !modfuncs->RegisterPhysicsEngine)
		Con_Printf("Bullet plugin failed: Engine doesn't support physics engine plugins.\n");
	else if (!modfuncs->RegisterPhysicsEngine("Bullet", World_Bullet_Start))
		Con_Printf("Bullet plugin failed: Engine already has a physics plugin active.\n");
	else
	{
		World_Bullet_Init();
		return true;
	}
	return false;
}

extern "C" qintptr_t Plug_Init(qintptr_t *args)
{
	CHECKBUILTIN(Mod_GetPluginModelFuncs);

	if (BUILTINISVALID(Mod_GetPluginModelFuncs))
	{
		modfuncs = pMod_GetPluginModelFuncs(sizeof(modplugfuncs_t));
		if (modfuncs && modfuncs->version < MODPLUGFUNCS_VERSION)
			modfuncs = NULL;
	}

	Plug_Export("Shutdown", World_Bullet_Shutdown);
	return World_Bullet_DoInit();
}


