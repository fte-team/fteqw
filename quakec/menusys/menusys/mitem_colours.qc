/***************************************************************************
hue selection thing, directly linked to a cvar.
We hard code 1 in the saturation+value arguments for the selection

Screw Quake colours, they're too brown!
*/
class mitem_colours : mitem
{
	virtual void(vector pos) item_draw;
	virtual float(vector pos, float scan, float char, float down) item_keypress;
};

//http://axonflux.com/handy-rgb-to-hsl-and-rgb-to-hsv-color-model-c
static vector(vector rgb) rgbtohsv =
{
	float r = rgb_x, g = rgb_y, b = rgb_z;
	float maxc = max(r, g, b), minc = min(r, g, b);
    	float h, s, l = (maxc + minc) / 2;

	local float d = maxc - minc;
	if (maxc)
		s = d / maxc;
	else
		s = 0;

	if(maxc == minc)
	{
		h = 0; // achromatic
	}
	else
	{
		if (maxc == r)
			h = (g - b) / d + ((g < b) ? 6 : 0);
		else if (maxc == g)
			h = (b - r) / d + 2;
		else
			h = (r - g) / d + 4;
		h /= 6;
    }

    return [h, s, l];
};

//http://axonflux.com/handy-rgb-to-hsl-and-rgb-to-hsv-color-model-c
static vector(vector hsv) hsvtorgb =
{
	float h = hsv_x, s = hsv_y, v = hsv_z;
	float r=0, g=1, b=0;

	while(h < 0)
		h+=1;
	while(h >= 1)
		h-=1;

	float i = floor(h * 6);
	float f = h * 6 - i;
	float p = v * (1 - s);
	float q = v * (1 - f * s);
	float t = v * (1 - (1 - f) * s);
	switch(i)
	{
		case 0: r = v, g = t, b = p; break;
		case 1: r = q, g = v, b = p; break;
		case 2: r = p, g = v, b = t; break;
		case 3: r = p, g = q, b = v; break;
		case 4: r = t, g = p, b = v; break;
		case 5: r = v, g = p, b = q; break;
	}

	return [r, g, b];
};

float(float chr) nibbletofloat =
{
	chr = chr&0xff;
	if (chr >= '0' && chr <= '9')
		return chr - '0';
	if (chr >= 'a' && chr <= 'f')
		return chr - 'a' + 10;
	if (chr >= 'A' && chr <= 'F')
		return chr - 'A' + 10;
	return 0;
};

static vector(string v) hextorgb =
{
	if (!strncmp(v, "0x", 2))
	{
		vector r;
		r_x = (nibbletofloat(str2chr(v, 2))*16 + nibbletofloat(str2chr(v, 3)))/255;
		r_y = (nibbletofloat(str2chr(v, 4))*16 + nibbletofloat(str2chr(v, 5)))/255;
		r_z = (nibbletofloat(str2chr(v, 6))*16 + nibbletofloat(str2chr(v, 7)))/255;
		return r;
	}
	else
		return '0 0 0';
};
static string(vector v) rgbtohex =
{
	v *= 255;
	return sprintf("0x%02x%02x%02x", v_x, v_y, v_z);
};

void(vector pos) mitem_colours::item_draw =
{
	local float step;
	local float stride;
	local string curval;
	local vector rgb;

	super::item_draw(pos);

	//calculate the rgb from hue at each step across the colour block
#define STEPS 32
	pos_x += item_size_x / 2;

	if (ui.mgrabs == this)
	{
		float frac;
		//if we're sliding it, update the value
		frac = (ui.mousepos[1] - pos_x-(item_size_y+4)) / (item_size_x / 2 - (item_size_y+4));
		if (frac >= 0 && frac <= 1)
		{
			set(item_command, rgbtohex(hsvtorgb([frac, 1, 1])));
		}
	}
	curval = get(item_command);

	stride = (item_size_x / 2 - (item_size_y+4)) / STEPS;

	ui.drawfill(pos, [item_size_y, item_size_y], hextorgb(curval), item_alpha, 0);
	pos_x += item_size_y+4;

	pos_y += 1;
#if defined(MENU) || 1
	for (step = 0; step < STEPS; step += 1, pos_x += stride)
	{
		rgb = hsvtorgb([step/STEPS, 1, 1]);
		ui.drawfill(pos, [stride, item_size_y-2], rgb, item_alpha, 0);
	}
#else
//FIXME: WTF is going on here? it comes out as black? wtf?
	//draw quads (we should probably not use an internal-to-engine shader here...)
	R_BeginPolygon("fill_opaque", 4);	//outside so we can skip it for faster reuse by avoiding lookups
	rgb = hsvtorgb([0, 1, 1]);
	for (step = 0; step < STEPS;)
	{
		R_PolygonVertex([pos_x, pos_y+item_size_y-2], '0 1', rgb, item_alpha);
		R_PolygonVertex([pos_x, pos_y], '0 0', rgb, 1);

		pos_x += stride;
		step += 1;
		rgb = hsvtorgb([step/STEPS, 1, 1]);

		R_PolygonVertex([pos_x, pos_y], '1 0', rgb, 1);
		R_PolygonVertex([pos_x, pos_y+item_size_y-2], '1 1', rgb, item_alpha);

		R_EndPolygon();
	}
#endif
#undef STEPS
};

float(vector pos, float scan, float char, float down) mitem_colours::item_keypress =
{
	if (!down)
	{
		if (ui.mgrabs == this)
			ui.mgrabs = __NULL__;
		return FALSE;
	}
	local float curval = rgbtohsv(hextorgb(get(item_command)))[0];
	if (scan == K_MWHEELUP || scan == K_MWHEELDOWN)
	{
		if (mouseinbox(pos, item_size))
			scan = ((scan == K_MWHEELDOWN)?K_LEFTARROW:K_RIGHTARROW);
	}
	if (scan == K_MOUSE1)
	{
		pos_x += item_size_x / 2;
		pos_x += item_size_y+4;
		curval = (ui.mousepos[0] - pos_x) / (item_size_x / 2 - item_size_y+4);
		if (curval < 0 || curval > 1)
			return FALSE;
		curval = curval;
		set(item_command, rgbtohex(hsvtorgb([(curval), 1, 1])));
		ui.mgrabs = this;
	}
	else if (scan == K_LEFTARROW || scan == K_SPACE)
	{
		set(item_command, rgbtohex(hsvtorgb([curval - (1/64.0), 1, 1])));	//yay autorepeat
	}
	else if (scan == K_RIGHTARROW || scan == K_ENTER)
	{
		set(item_command, rgbtohex(hsvtorgb([curval + (1/64.0), 1, 1])));
	}
	else
		return FALSE;
	return TRUE;
};
mitem_colours(string text, string command, vector sz) menuitemcolour_spawn =
{
	mitem_colours n = spawn(mitem_colours);
	n.item_scale = sz_y;
	n.item_text = text;
	n.item_size = sz;

	n.item_command = command;
	n.item_flags |= IF_SELECTABLE;
	return n;
};

