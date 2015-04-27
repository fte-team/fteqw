//addons cannot use certain optimisations - the engine MUST be able to find+fix their string+function indexes.
#pragma optimise addon	//disables any optimisations that would break addons.

//#define CAMQUAKE

//with addons, the engine only tracks one set of globals for most things.
//thus we use pointers in the addon to refer to that set of globals.
//this avoids having to make things shared, and having to pay for every transition.

vector *ptr_trace_endpos;
#define trace_endpos (*ptr_trace_endpos)

vector *ptr_trace_plane_normal;
#define trace_plane_normal (*ptr_trace_plane_normal)

entity *ptr_self;
#define self 		(*ptr_self)

void() csfixups =
{
	ptr_trace_endpos		= (vector*)externvalue(0, "&trace_endpos");
	ptr_trace_plane_normal	= (vector*)externvalue(0, "&trace_plane_normal");
	ptr_self				= (entity*)externvalue(0, "&self");
};


vector mousenear;
vector mousefar;
float mousedown;



enum
{
	MODE_INITIAL=0,

	MODE_LIGHTEDIT=1,
	MODE_SPLINEEDIT=2,
	MODE_TERRAINEDIT=3,
	MODE_BRUSHEDIT=4,
	MODE_PARTICLEEDIT=5,
	MODE_ENTSEDIT=6,
};

var string editornames[] = {"","LIGHTS","SPLINES", "TERRAIN", "BRUSHES", "PARTICLES", "ENTITIES"};
