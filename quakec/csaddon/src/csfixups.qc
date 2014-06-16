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
