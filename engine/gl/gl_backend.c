#include "quakedef.h"
#include "glquake.h"
#include "shader.h"

#ifdef RGLQUAKE


#define MAX_MESH_VERTS 8192

//we don't support multitexturing yet.


static float tempstarray[MAX_MESH_VERTS*3];
static vec4_t tempxyzarray[MAX_MESH_VERTS];

shader_t nullshader, wallbumpshader, modelbumpshader;
#define frand() (rand()&32767)* (1.0/32767)

#define FTABLE_SIZE		1024
#define FTABLE_CLAMP(x)	(((int)((x)*FTABLE_SIZE) & (FTABLE_SIZE-1)))
#define FTABLE_EVALUATE(table,x) (table ? table[FTABLE_CLAMP(x)] : frand())//*((x)-floor(x)))

static	float	r_sintable[FTABLE_SIZE];
static	float	r_triangletable[FTABLE_SIZE];
static	float	r_squaretable[FTABLE_SIZE];
static	float	r_sawtoothtable[FTABLE_SIZE];
static	float	r_inversesawtoothtable[FTABLE_SIZE];

void GLR_MeshInit(void)
{
	int i;
	double t;
	for ( i = 0; i < FTABLE_SIZE; i++ )
	{
		t = (double)i / (double)FTABLE_SIZE;

		r_sintable[i] = sin ( t * M_PI*2 );
		
		if (t < 0.25) 
			r_triangletable[i] = t * 4.0;
		else if (t < 0.75)
			r_triangletable[i] = 2 - 4.0 * t;
		else
			r_triangletable[i] = (t - 0.75) * 4.0 - 1.0;

		if (t < 0.5) 
			r_squaretable[i] = 1.0f;
		else
			r_squaretable[i] = -1.0f;

		r_sawtoothtable[i] = t;
		r_inversesawtoothtable[i] = 1.0 - t;
	}

	{
		nullshader.numdeforms = 0;//1;
		nullshader.deforms[0].type = DEFORMV_WAVE;
		nullshader.deforms[0].args[0] = 10;
		nullshader.deforms[0].func.type = SHADER_FUNC_SIN;
		nullshader.deforms[0].func.args[1] = 1;
		nullshader.deforms[0].func.args[3] = 10;

		nullshader.pass[0].texturetype = GL_TEXTURE_2D;
		nullshader.pass[0].envmode = GL_MODULATE;
		nullshader.pass[0].blendsrc = GL_SRC_ALPHA;
		nullshader.pass[0].blenddst = GL_ONE_MINUS_SRC_ALPHA;

		nullshader.pass[1].flags |= SHADER_PASS_BLEND;
		nullshader.pass[1].tcgen = TC_GEN_LIGHTMAP;
		nullshader.pass[1].blendsrc = GL_SRC_ALPHA;
		nullshader.pass[1].blenddst = GL_ONE_MINUS_SRC_ALPHA;
		nullshader.pass[1].texturetype = GL_TEXTURE_2D;
	}

	{
		modelbumpshader.numpasses = 3;

		if (1)
			modelbumpshader.pass[0].mergedpasses = 4;
		else
			modelbumpshader.pass[0].mergedpasses = 2;
		modelbumpshader.pass[2].mergedpasses = 1;

		modelbumpshader.pass[0].tcgen = TC_GEN_BASE;
		modelbumpshader.pass[0].envmode = GL_COMBINE_ARB;
		modelbumpshader.pass[0].combinesrc0 = GL_TEXTURE;
		modelbumpshader.pass[0].combinemode = GL_REPLACE;
		modelbumpshader.pass[0].blendsrc = GL_SRC_ALPHA;
		modelbumpshader.pass[0].blenddst = GL_ONE_MINUS_SRC_ALPHA;
		modelbumpshader.pass[0].anim_frames[0] = 0;//bumpmap
		modelbumpshader.pass[0].texturetype = GL_TEXTURE_2D;

		modelbumpshader.pass[1].tcgen = TC_GEN_DOTPRODUCT;
		modelbumpshader.pass[1].envmode = GL_COMBINE_ARB;
		modelbumpshader.pass[1].combinesrc0 = GL_TEXTURE;
		modelbumpshader.pass[1].combinesrc1 = GL_PREVIOUS_ARB;
		modelbumpshader.pass[1].combinemode = GL_DOT3_RGB_ARB;
		modelbumpshader.pass[1].anim_frames[0] = 0;//delux
		modelbumpshader.pass[1].texturetype = GL_TEXTURE_2D;

		modelbumpshader.pass[2].flags |= SHADER_PASS_BLEND;
		modelbumpshader.pass[2].tcgen = TC_GEN_BASE;
		modelbumpshader.pass[2].envmode = GL_MODULATE;
		modelbumpshader.pass[2].blendsrc = GL_DST_COLOR;
		modelbumpshader.pass[2].blenddst = GL_ZERO;
		modelbumpshader.pass[2].anim_frames[0] = 0;//texture
		modelbumpshader.pass[2].texturetype = GL_TEXTURE_2D;

		//gl_combine states that we need to use a textures.
		modelbumpshader.pass[3].tcgen = TC_GEN_BASE;	//multiply by colors
		modelbumpshader.pass[3].envmode = GL_COMBINE_ARB;
		modelbumpshader.pass[3].combinesrc0 = GL_PREVIOUS_ARB;
		modelbumpshader.pass[3].combinesrc1 = GL_PRIMARY_COLOR_ARB;
		modelbumpshader.pass[3].combinemode = GL_MODULATE;
		modelbumpshader.pass[3].anim_frames[0] = 1;		//any, has to be present
		modelbumpshader.pass[3].texturetype = GL_TEXTURE_2D;
	}
	
	{
		wallbumpshader.numpasses = 4;

		if (1)
			wallbumpshader.pass[0].mergedpasses = 4;
		else
			wallbumpshader.pass[0].mergedpasses = 2;
		wallbumpshader.pass[2].mergedpasses = 2;

		wallbumpshader.pass[0].tcgen = TC_GEN_BASE;
		wallbumpshader.pass[0].envmode = GL_COMBINE_ARB;
		wallbumpshader.pass[0].combinesrc0 = GL_TEXTURE;
		wallbumpshader.pass[0].combinemode = GL_REPLACE;
		wallbumpshader.pass[0].anim_frames[0] = 0;//bumpmap
		wallbumpshader.pass[0].blendsrc = GL_SRC_ALPHA;
		wallbumpshader.pass[0].blenddst = GL_ONE_MINUS_SRC_ALPHA;
		wallbumpshader.pass[0].texturetype = GL_TEXTURE_2D;

		wallbumpshader.pass[1].tcgen = TC_GEN_LIGHTMAP;
		wallbumpshader.pass[1].envmode = GL_COMBINE_ARB;
		wallbumpshader.pass[1].combinesrc0 = GL_TEXTURE;
		wallbumpshader.pass[1].combinesrc1 = GL_PREVIOUS_ARB;
		wallbumpshader.pass[1].combinemode = GL_DOT3_RGB_ARB;
		wallbumpshader.pass[1].anim_frames[0] = 0;//delux
		wallbumpshader.pass[1].texturetype = GL_TEXTURE_2D;

		wallbumpshader.pass[2].flags |= SHADER_PASS_BLEND;
		wallbumpshader.pass[2].tcgen = TC_GEN_BASE;
		wallbumpshader.pass[2].envmode = GL_MODULATE;
		wallbumpshader.pass[2].blendsrc = GL_DST_COLOR;
		wallbumpshader.pass[2].blenddst = GL_ZERO;
		wallbumpshader.pass[2].anim_frames[0] = 0;//texture
		wallbumpshader.pass[2].texturetype = GL_TEXTURE_2D;

		wallbumpshader.pass[3].tcgen = TC_GEN_LIGHTMAP;
		wallbumpshader.pass[3].envmode = GL_BLEND;
		wallbumpshader.pass[3].anim_frames[0] = 0;//lightmap
		wallbumpshader.pass[3].texturetype = GL_TEXTURE_2D;
	}
}

static float *R_TableForFunc ( unsigned int func )
{
	switch (func)
	{
	case SHADER_FUNC_SIN:
			return r_sintable;

		case SHADER_FUNC_TRIANGLE:
			return r_triangletable;

		case SHADER_FUNC_SQUARE:
			return r_squaretable;

		case SHADER_FUNC_SAWTOOTH:
			return r_sawtoothtable;

		case SHADER_FUNC_INVERSESAWTOOTH:
			return r_inversesawtoothtable;
	}

	// assume noise
	return NULL;
}

static void MakeDeforms(shader_t *shader, vec4_t *out, vec4_t *in, int number)
{
	float *table;
	int d, j;
	float args[4], deflect;
	deformv_t *dfrm = shader->deforms; 

	for (d = 0; d < shader->numdeforms; d++, dfrm++)
	{
		switch(dfrm->type)
		{
		case DEFORMV_WAVE:
			args[0] = dfrm->func.args[0];
			args[1] = dfrm->func.args[1];
			args[3] = dfrm->func.args[2] + dfrm->func.args[3] * realtime;
			table = R_TableForFunc ( dfrm->func.type );

			for ( j = 0; j < number; j++ ) {
				deflect = dfrm->args[0] * (in[j][0]+in[j][1]+in[j][2]) + args[3];
				deflect = sin(deflect)/*FTABLE_EVALUATE ( table, deflect )*/ * args[1] + args[0];

				out[j][0] = in[j][0]+deflect;
				out[j][1] = in[j][1]+deflect;
				out[j][2] = in[j][2]+deflect;

				// Deflect vertex along its normal by wave amount
//				VectorMA ( out[j], deflect, normalsArray[j], in[j] );
			}
			break;
		case DEFORMV_MOVE:
			table = R_TableForFunc ( dfrm->func.type );
			deflect = dfrm->func.args[2] + realtime * dfrm->func.args[3];
			deflect = FTABLE_EVALUATE (table, deflect) * dfrm->func.args[1] + dfrm->func.args[0];

			for ( j = 0; j < number; j++ )
				VectorMA ( out[j], deflect, dfrm->args, in[j] );
			break;
		default:
			Sys_Error("Bad deform type %i\n", dfrm->type);
		}

		in = out;
	}
}

static void Mesh_DeformTextureCoords(mesh_t *mesh, shaderpass_t *pass)
{
	int d;
	tcmod_t *tcmod = pass->tcmod;
	float *in, *out;
	switch(pass->tcgen)
	{
	case TC_GEN_DOTPRODUCT:	//take normals, use the dotproduct and produce texture coords for bumpmapping
		{			
			out = tempstarray;
			in = (float*)mesh->normals_array;

			for (d = 0; d < mesh->numvertexes; d++)
			{
				out[d*3+0] = DotProduct((in+d*3), mesh->lightaxis[0]);
				out[d*3+1] = DotProduct((in+d*3), mesh->lightaxis[1]);
				out[d*3+2] = DotProduct((in+d*3), mesh->lightaxis[2]);
			}
			glTexCoordPointer(3, GL_FLOAT, 0, out);
			
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
		}
		return;
	case TC_GEN_LIGHTMAP:
		in = (float*)mesh->lmst_array;
		if (in)
			break;	//fallthrought
	case TC_GEN_BASE:
		in = (float*)mesh->st_array;
		break;
	default:
		Sys_Error("Mesh_DeformTextureCoords: Bad TC_GEN type\n");
		return;
	}
/*
	for (d = 0; d < pass->numtcmods; d++, dfrm++)
	{
	}
*/

	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	glTexCoordPointer(2, GL_FLOAT, 0, in);
}

static void Mesh_SetShaderpassState ( shaderpass_t *pass, qboolean mtex )
{
	if ( (mtex && (pass->blendmode != GL_REPLACE)) || (pass->flags & SHADER_PASS_BLEND) )
	{
		glEnable (GL_BLEND);
		glBlendFunc (pass->blendsrc, pass->blenddst);
	}
	else
	{
//		glDisable (GL_BLEND);
	}

	if (pass->flags & SHADER_PASS_ALPHAFUNC)
	{
		glEnable (GL_ALPHA_TEST);

		if (pass->alphafunc == SHADER_ALPHA_GT0)
		{
			glAlphaFunc (GL_GREATER, 0);
		}
		else if (pass->alphafunc == SHADER_ALPHA_LT128)
		{
			glAlphaFunc (GL_LESS, 0.5f);
		}
		else if (pass->alphafunc == SHADER_ALPHA_GE128)
		{
			glAlphaFunc (GL_GEQUAL, 0.5f);
		}
	}
	else
	{
//		glDisable (GL_ALPHA_TEST);
	}

//	glDepthFunc (pass->depthfunc);

	if (pass->flags & SHADER_PASS_DEPTHWRITE)
	{
		glDepthMask (GL_TRUE);
	}
	else
	{
//		glDepthMask (GL_FALSE);
	}
}

static void Mesh_DrawPass(shaderpass_t *pass, mesh_t *mesh)
{
	Mesh_SetShaderpassState(pass, false);
	if (pass->mergedpasses>1 && gl_mtexarbable)
	{
		int p;
//		Mesh_DrawPass(pass+2,mesh);
//		return;
		for (p = 0; p < pass->mergedpasses; p++)
		{
			qglActiveTextureARB(GL_TEXTURE0_ARB+p);
			qglClientActiveTextureARB(GL_TEXTURE0_ARB+p);
			GL_BindType(pass[p].texturetype, pass[p].anim_frames[0]);
			glEnable(pass[p].texturetype);

			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, pass[p].envmode);
			if (pass[p].envmode == GL_COMBINE_ARB)
			{
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, pass[p].combinesrc0);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, pass[p].combinesrc1);
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, pass[p].combinemode);
			}

			Mesh_DeformTextureCoords(mesh, pass+p);
		}
		glDrawElements(GL_TRIANGLES, mesh->numindexes, GL_UNSIGNED_INT, mesh->indexes);
		for (p = pass->mergedpasses-1; p >= 0; p--)
		{
			qglActiveTextureARB(GL_TEXTURE0_ARB+p);
			qglClientActiveTextureARB(GL_TEXTURE0_ARB+p);
			glDisable(pass[p].texturetype);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
		}
	}
	else
	{
		Mesh_DeformTextureCoords(mesh, pass);

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, pass->envmode);

		GL_Bind(pass->anim_frames[0]);
		if (pass->texturetype != GL_TEXTURE_2D)
		{
			glDisable(pass->texturetype);
			glEnable(pass->texturetype);
		}
		glDrawElements(GL_TRIANGLES, mesh->numindexes, GL_UNSIGNED_INT, mesh->indexes);

		if (pass->texturetype != GL_TEXTURE_2D)
		{
			glDisable(pass->texturetype);
			glEnable(GL_TEXTURE_2D);
		}

		glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	}
}


void GL_DrawMesh(mesh_t *mesh, shader_t *shader, int texturenum, int lmtexturenum)
{
	int i;
	if (!shader)
	{
		shader = &nullshader;
		shader->pass[0].anim_frames[0] = texturenum;
		shader->pass[0].mergedpasses=1;

		if (lmtexturenum && !texturenum)
		{
			shader->pass[0].anim_frames[0] = lmtexturenum;
			shader->numpasses = 1;
			shader->pass[0].flags |= SHADER_PASS_BLEND;
		}
		else if (lmtexturenum)
		{
			shader->numpasses = 2;
			shader->pass[1].anim_frames[0] = lmtexturenum;//lmtexture;
			shader->pass[0].flags &= ~SHADER_PASS_BLEND;
		}
		else
		{
			shader->pass[0].flags &= ~SHADER_PASS_BLEND;
			shader->numpasses = 1;
		}

		shader->pass[0].texturetype = GL_TEXTURE_2D;
	}
	if (!shader->numdeforms)
		glVertexPointer(3, GL_FLOAT, 16, mesh->xyz_array);
	else
	{
		MakeDeforms(shader, tempxyzarray, mesh->xyz_array, mesh->numvertexes);
		glVertexPointer(3, GL_FLOAT, 16, tempxyzarray);
	}
	if (mesh->normals_array && glNormalPointer)
	{
		glNormalPointer(GL_FLOAT, 0, mesh->normals_array);
		glEnableClientState( GL_NORMAL_ARRAY );
	}

	glEnableClientState( GL_VERTEX_ARRAY );
	if (mesh->colors_array && glColorPointer)
	{
		glColorPointer(4, GL_UNSIGNED_BYTE, 0, mesh->colors_array);
		glEnableClientState( GL_COLOR_ARRAY );
	}

	for (i =0 ; i < shader->numpasses; i+=shader->pass[i].mergedpasses)
		Mesh_DrawPass(shader->pass+i, mesh);

	glDisableClientState( GL_VERTEX_ARRAY );
	glDisableClientState( GL_COLOR_ARRAY );
	glDisableClientState( GL_NORMAL_ARRAY );
	glDisableClientState( GL_NORMAL_ARRAY );

/*	//show normals
	if (mesh->normals_array)
	{
		glColor3f(1,1,1);
		glDisable(GL_TEXTURE_2D);
		glBegin(GL_LINES);
		for (i = 0; i < mesh->numvertexes; i++)
		{
			glVertex3f(	mesh->xyz_array[i][0],
						mesh->xyz_array[i][1],
						mesh->xyz_array[i][2]);

			glVertex3f(	mesh->xyz_array[i][0] + mesh->normals_array[i][0],
						mesh->xyz_array[i][1] + mesh->normals_array[i][1],
						mesh->xyz_array[i][2] + mesh->normals_array[i][2]);
		}
		glEnd();
		glEnable(GL_TEXTURE_2D);
	}
*/
}

void GL_DrawMeshBump(mesh_t *mesh, int texturenum, int lmtexturenum, int bumpnum, int deluxnum)
{
	shader_t *shader;
	extern int normalisationCubeMap;

	if (lmtexturenum)
	{
		shader = &wallbumpshader;
		shader->pass[3].anim_frames[0] = lmtexturenum;
	}
	else
		shader = &modelbumpshader;

	shader->pass[0].anim_frames[0] = bumpnum;
	if (deluxnum)
	{
		shader->pass[1].anim_frames[0] = deluxnum;
		shader->pass[1].tcgen = TC_GEN_LIGHTMAP;
		shader->pass[1].texturetype = GL_TEXTURE_2D;
	}
	else
	{
		shader->pass[1].anim_frames[0] = normalisationCubeMap;
		shader->pass[1].tcgen = TC_GEN_DOTPRODUCT;
		shader->pass[1].texturetype = GL_TEXTURE_CUBE_MAP_ARB;
	}
	shader->pass[2].anim_frames[0] = texturenum;

//	mesh->colors_array=NULL;	//don't bother coloring it.

	GL_DrawMesh(mesh, shader, 0, 0);
}

#endif
