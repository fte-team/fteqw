/*FTE has some special light editing builtins, I don't ever expect them to be standard or anything, but they're handy for this*/
enum
{
	lf_origin=0,
	lf_colour=1,
	lf_radius=2,
	lf_flags=3,
	lf_style=4
};
float(float l, float f) dl_getf = #372;
vector(float l, float f) dl_getv = #372;
void(float l, float f, float v) dl_setf = #373;
void(float l, float f, vector v) dl_setv = #373;

/*If you want to edit lights for hexen2, edit this cvar, use models/s_light.spr or something.*/
DEFCVAR_STRING(cg_editor_lightmodel, "progs/s_light.spr")

static float selectedlight;
static float editfield;
static string editvalue;
static entity tempent;
void() editor_lights_add =
{
	float l;
	if (!tempent)
		tempent = spawn();

	l = dl_getf(-1, -1);
	setmodel(tempent, CVARS(cg_editor_lightmodel));
	while(l > 0)
	{
		l = l-1;
		if (l == selectedlight)
		{
			if (time*5 & 1)
				continue;
			tempent.effects |= 8192;
		}
		else
			tempent.effects (-) 8192;
		if (!dl_getf(l, lf_radius))
			continue;
		setorigin(tempent, dl_getv(l, lf_origin));
		addentity(tempent);
	}
};

static string fldname[8] = {
	"bad",
	"num",
	"org",
	"rgb",
	"rad",
	"flg",
	"sty",
	"???"
};
static string(float fld, float foredit) readfield =
{
	switch(fld)
	{
	case 1:
		if (foredit)
			return ftos(selectedlight);
		return strcat(ftos(selectedlight), " / ", ftos(dl_getf(-1, -1)));
	case 2:
		return vtos(dl_getv(selectedlight, lf_origin));
	case 3:
		return vtos(dl_getv(selectedlight, lf_colour));
	case 4:
		return ftos(dl_getf(selectedlight, lf_radius));
	case 5:
		float fl = dl_getf(selectedlight, lf_flags);
		if (foredit)
			return ftos(fl);
		return strcat(	((fl & 1)?" alwaysvisible":""),
					((fl & 2)?" realtime":""),
					((fl & 65536)?" no-shadows":"")
				 );
	case 6:
		return ftos(dl_getf(selectedlight, lf_style));
	default:
		return "";
	}
};

static void(float fld, string newval) writefield =
{
	switch(fld)
	{
	case 1:
		selectedlight = stof(newval);
		return;
	case 2:
		dl_setv(selectedlight, lf_origin, stov(newval));
		return;
	case 3:
		dl_setv(selectedlight, lf_colour, stov(newval));
		return;
	case 4:
		dl_setf(selectedlight, lf_radius, stof(newval));
		return;
	case 5:
		dl_getf(selectedlight, lf_flags, stof(newval));
		return;
	case 6:
		dl_getf(selectedlight, lf_style, stof(newval));
		return;
	default:
		return "";
	}
};

void() editor_lights_overlay =
{
	float i;
	string s;
//void(vector position, string text, vector scale, vector rgb, float alpha, ...) drawrawstring
	drawrawstring('0 0 0', "LIGHTS EDITOR", '8 8 0', '1 1 1', 1);
	for (i = 1; i < 8; i++)
	{
		if (editfield == i)
			s = editvalue;
		else
			s = readfield(i, 0);
		s = strcat(ftos(i), " ", fldname[i], ": ", s);
		drawrawstring('0 32 0' + '0 8 0' * i, s, '8 8 0', ((editfield == i)?'1 0 0':'1 1 1'), 1);
	}
};

static void(vector fwd, vector vorg) selectbestlight =
{
	float l, b=selectedlight, d, bd = 0;
	vector ldir;
	l = dl_getf(-1, -1);
	while(l > 0)
	{
		l--;
		ldir = dl_getv(l, lf_origin);
		ldir = normalize(ldir - vorg);
		d = fwd*ldir;
		if (d > bd)
		{
			bd = d;
			b = l;
		}
	}
	selectedlight = b;
};

float(float keyc, float unic) editor_lights_key =
{
	vector v, o;
	string ns;
	if (editfield)
	{
		ns = strcat(editvalue);
		if (keyc == 10 || keyc == 13)
		{
			writefield(editfield, ns);
			editfield = 0;
		}
		else if (keyc == 127)
		{
			if (ns != "")
				ns = substring(ns, 0, -2);
		}
		else if (keyc == 8)
		{
			ns = "";
		}
		else
			ns = strcat(ns, chr2str(unic));

		ns = strzone(ns);
		strunzone(editvalue);
		editvalue = ns;

		writefield(editfield, ns);
	}
	else if (keyc >= '0' && keyc <= '9')
	{
		editfield = keyc - '0';
		editvalue = strzone(readfield(editfield, 1));
	}
	else if (keyc == '=')
		selectedlight++;
	else if (keyc == '-')
		selectedlight--;
	else if (keyc == 'n')
		localcmd("noclip\n");
	else if (keyc == 's')
	{
		v = getviewpropv(15);
		o = getviewpropv(11);
		makevectors(v);
		selectbestlight(v_forward, o);
	}
	else if (keyc == 'm')
	{
		v = getviewpropv(15);
		o = getviewpropv(11);
		makevectors(v);
		traceline(o, o + v_forward*8192, true, world);
		dl_setv(selectedlight, lf_origin, trace_endpos + trace_plane_normal*4);
	}
	else if (keyc == 'i')
	{
		for (selectedlight = 32; ; selectedlight++)
		{
			if (!dl_getf(selectedlight, lf_radius))
			{
				dl_setf(selectedlight, lf_radius, 300);
				v = getviewpropv(15);
				o = getviewpropv(11);
				makevectors(v);
				traceline(o, o + v_forward*8192, true, world);
				dl_setv(selectedlight, lf_origin, trace_endpos + trace_plane_normal*4);
				break;
			}
		}
	}
	else if (keyc == '[')
	{
		v = getviewpropv(15);
		o = getviewpropv(11);
		makevectors(v);
		traceline(o, o + v_forward*8192, true, world);
		dl_setv(selectedlight, lf_origin, dl_getv(selectedlight, lf_origin) - trace_plane_normal);
	}
	else if (keyc == ']')
	{
		v = getviewpropv(15);
		o = getviewpropv(11);
		makevectors(v);
		traceline(o, o + v_forward*8192, true, world);
		dl_setv(selectedlight, lf_origin, dl_getv(selectedlight, lf_origin) + trace_plane_normal);
	}
	else if (keyc == 127)
		dl_setf(selectedlight, lf_radius, 0);
	else
		return false;
	return true;
};
