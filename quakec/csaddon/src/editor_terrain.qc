enum
{
	ter_reload = 0,			//reload the entire thing
	ter_save = 1,			//save the entire heightmap
	ter_sethole = 2,
	ter_height_set = 3,		//set heights to a specific value
	ter_height_smooth = 4,	//smooth the heights slightly (non-destructive)
	ter_height_spread = 5,	//smooth the heights slightly (leaves center as-is)
	ter_height_raise = 6,		//raise the terrain in a bell (negative value to lower)
	ter_height_lower = 7,		//lower the terrain in a bell (negative value to raise)
	ter_tex_kill = 8,		//set section texture
	ter_tex_get = 9,		//get section texture
	ter_mixpaint = 10,		//paint a specific texture
	ter_mixconcentrate = 11,	//figure out which is the strongest mixed texture and make it stronger
	ter_mixnoise = 12,		//add random noise to the affected samples
	ter_mixblur = 13,		//blur the texture mixture
	ter_water_set = 14,		//lower the terrain in a bell (negative value to raise)
	ter_mesh_add = 15,		//add a mesh
	ter_mesh_kill = 16,		//remove meshes within the radius
	ter_tint = 17,			//pants new colour modifiers/tints
	ter_height_flatten,		//smooth the heights slightly (non-destructive), such that the middle becomes flatter faster than the edges

	ter_blank,
	ter_radius,
	ter_quant,
	ter_strength,
	ter_mesh,
	ter_tintval,
	ter_tex,
	ter_count
};
static var float eradius = 256;
static var float equant = 8;
static var float epercent = 40;
static string tex[8];
static var string tint[8] = {"1 1 1", "1.2 0.9 0.9", "0 1 0"};
static string meshname;
static float curtool;
static int painttex;
static float mautorepeattime;
static entity tempent;

float autocvar_mod_terrain_networked;

vector vidsize;
float mousedown;

static string toolname[ter_count] =
{
	"reload",
	"save",
	"holes",
	"height set",
	"height smooth",
	"height spread",
	"height raise",
	"height lower",
	"tex kill",
	"tex get",
	"mix paint",
	"mix concentrate",
	"mix noise",
	"mix blur",
	"water set",
	"mesh add",
	"mesh kill",
	"mesh tint",
	"",
	"rad",
	"quant",
	"str",
	"mesh",
	"tex"
};

#define terrain_edit terrain_editfoo

__variant(float action, ...) terrain_edit = #278;

void(vector m) editor_do =
{
	vector t = unproject(m + '0 0 8192');
	vector o = unproject(m);
	if (vlen(o - t) > 8192)
		t = o + normalize(t)*8192;
	traceline(o, t, TRUE, world);

	self = world;

	switch(curtool)
	{
	case ter_reload:
	case ter_save:
		mautorepeattime = 0;	//don't autorepeat that...
		if (autocvar_mod_terrain_networked)
			sendevent("teredit", "f", curtool);
		else
		{
			if (curtool == ter_save)
				print("Saving...\n");
			else if (curtool == ter_reload)
				print("Reloading...\n");
			terrain_edit(curtool);
		}
		break;
	case ter_sethole:	//use view center instead of targetted - you cannot target that which is not there
		if (autocvar_mod_terrain_networked)
			sendevent("teredit", "fvff", curtool, trace_endpos, eradius, equant);
		else
			terrain_edit(curtool, trace_endpos, eradius, equant);
		break;

	case ter_height_smooth:
	case ter_height_spread:
		if (autocvar_mod_terrain_networked)
			sendevent("teredit", "fvff", curtool, trace_endpos, eradius, epercent/100.0);
		else
			terrain_edit(curtool, trace_endpos, eradius, epercent/100.0);
		break;
	case ter_water_set:
	case ter_height_set:
	case ter_height_smooth:
	case ter_height_raise:
	case ter_height_lower:
		if (autocvar_mod_terrain_networked)
			sendevent("teredit", "fvff", curtool, trace_endpos, eradius, equant);
		else
			terrain_edit(curtool, trace_endpos, eradius, equant);
		break;
	case ter_tex_get:
		strunzone(tex[0]);
		strunzone(tex[1]);
		strunzone(tex[2]);
		strunzone(tex[3]);
		tex[0] = strzone(terrain_edit(curtool, trace_endpos, 0, 0));
		tex[1] = strzone(terrain_edit(curtool, trace_endpos, 0, 1));
		tex[2] = strzone(terrain_edit(curtool, trace_endpos, 0, 2));
		tex[3] = strzone(terrain_edit(curtool, trace_endpos, 0, 3));
		break;
//	case ter_mixset:
//		terrain_edit(curtool, trace_endpos, eradius, equant, emix);
//		break;
	case ter_mixpaint:
		if (autocvar_mod_terrain_networked)
			sendevent("teredit", "fvffs", curtool, trace_endpos, eradius, epercent/100.0, tex[painttex]);
		else
			terrain_edit(curtool, trace_endpos, eradius, epercent/100.0, tex[painttex]);
		break;
	case ter_tex_kill:
		if (autocvar_mod_terrain_networked)
			sendevent("teredit", "fvffs", curtool, trace_endpos, eradius, equant, tex[painttex]);
		else
			terrain_edit(curtool, trace_endpos, eradius, equant, tex[painttex]);
		break;
	case ter_mixconcentrate:
	case ter_mixnoise:
	case ter_mixblur:
		if (autocvar_mod_terrain_networked)
			sendevent("teredit", "fvff", curtool, trace_endpos, eradius, equant);
		else
			terrain_edit(curtool, trace_endpos, eradius, equant);
		break;

	case ter_tint:
		if (autocvar_mod_terrain_networked)
			sendevent("teredit", "fvffvf", curtool, trace_endpos, eradius, epercent/100.0, stov(tint[painttex]), 1);
		else
			terrain_edit(curtool, trace_endpos, eradius, epercent/100.0, stov(tint[painttex]), 1);
		break;

	case ter_mesh_add:
		terrain_edit(curtool, tempent);
		break;
	case ter_mesh_kill:
		if (autocvar_mod_terrain_networked)
			sendevent("teredit", "fvf", curtool, trace_endpos, eradius);
		else
			terrain_edit(curtool, trace_endpos, eradius);
		break;
	}
};

float(float keyc, float unic, vector m) editor_terrain_key =
{
	if (curtool >= ter_radius && curtool <= ter_tex)
	{
		string txt;
		float nt = curtool;
		if (curtool == ter_radius)
			txt = itos((int)eradius);
		if (curtool == ter_quant)
			txt = itos((int)equant);
		if (curtool == ter_strength)
			txt = itos((int)epercent);
		if (curtool == ter_mesh)
			txt = meshname;
		if (curtool == ter_tintval)
			txt = tint[painttex];
		if (curtool == ter_tex)
			txt = tex[painttex];
			
		if (keyc == 10 || keyc == 13)
		{
			if (curtool == ter_mesh)
				nt = ter_mesh_add;
			else if (curtool == ter_tintval)
				nt = ter_tint;
			else
				nt = ter_mixpaint;
		}
		else if (keyc == 127)
			txt = substring(txt, 0, -2);
		else if (keyc == 8)
			txt = "";
		else if (unic)
			txt = strcat(txt, chr2str(unic));

		if (curtool == ter_radius)
			eradius = stof(txt);
		if (curtool == ter_quant)
			equant = stof(txt);
		if (curtool == ter_strength)
			epercent = stof(txt);
		if (curtool == ter_mesh)
		{
			txt = strzone(txt);
			strunzone(meshname);
			meshname = txt;
		}
		if (curtool == ter_tex)
		{
			txt = strzone(txt);
			strunzone(tex[painttex]);
			tex[painttex] = txt;
		}
		if (curtool == ter_tintval)
		{
			txt = strzone(txt);
			strunzone(tint[painttex]);
			tint[painttex] = txt;
		}

		curtool = nt;
	}
	else if (keyc == 13 || (keyc == 512 && m_x < 128))
	{
		if (m_x < 128)
			curtool = floor((m_y-16) / 8);
		else if (keyc == 13)
		{
			editor_do(m);
		}
	}
	else if (unic == '+' || unic == '=')
		eradius += 16;
	else if (unic == '-')
		eradius -= 16;
	else if (curtool == ter_mesh_add && tempent)
	{
		if (unic == '[')
			tempent.angles_y -= 12.5;
		else if (unic == ']')
			tempent.angles_y += 12.5;
		else if (unic == '{')
			tempent.angles_x -= 12.5;
		else if (unic == '}')
			tempent.angles_x += 12.5;
		else if (unic == '(')
			tempent.angles_z -= 12.5;
		else if (unic == ')')
			tempent.angles_z += 12.5;
		else if (unic == '@')
		{
			tempent.angles_x = gettime(5)*360*360;
			tempent.angles_y = gettime(5)*360;
			tempent.angles_z = gettime(5)*360*360*360;
		}
		else if (keyc == 127 || keyc == 8)
		{
			tempent.angles_x = 0;
			tempent.angles_y = 0;
			tempent.angles_z = 0;
		}
		else
			return FALSE;
	}
	else if (unic == '[')
		equant -= 1;
	else if (unic == ']')
		equant += 1;
	else if (unic == '{')
		painttex = (painttex + 7) & 7;
	else if (unic == '}')
		painttex = (painttex + 1) & 7;
	else if (unic == '1')
		painttex = 0;
	else if (unic == '2')
		painttex = 1;
	else if (unic == '3')
		painttex = 2;
	else if (unic == '4')
		painttex = 3;
	else if (unic == '5')
		painttex = 4;
	else if (unic == '6')
		painttex = 5;
	else if (unic == '7')
		painttex = 6;
	else if (unic == '8')
		painttex = 7;
	else
		return FALSE;

	if (curtool == ter_save || curtool == ter_reload)
	{
		editor_do('0 0 0');
		curtool = -1;
	}
	return TRUE;
};

void(vector mousepos) editor_terrain_add =
{
	float s,c;
	float r;
	vector tx, p, col,t;
	float a;
	if (mousepos_x < 128)
		return;

	vector t = unproject(mousepos + '0 0 8192');
	vector o = unproject(mousepos);
	if (vlen(o - t) > 8192)
		t = o + normalize(t)*8192;
	traceline(o, t, TRUE, world);

	if (curtool == ter_mesh_add || curtool == ter_mesh)
	{
		if (!tempent)
			tempent = spawn();

		precache_model(meshname); /*just to silence it*/
		setmodel(tempent, meshname);
		setorigin(tempent, trace_endpos);
		tempent.scale = eradius/256;
		addentity(tempent);
	}
	else
	{
		shaderforname("terrainedit",
		"{"
			"{\n"
				"map terrainedit\n"
				"blendfunc add\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
			"}\n"
		"}");

		r = eradius;//sqrt(eradius*eradius/2);

		s = sin(gettime(5)) * r;
		c = cos(gettime(5)) * r;

		col_x = (sin(gettime(5))+1.5)*0.1;

		t = trace_endpos;
		R_BeginPolygon("terrainedit");
		for (a = 0; a < 3.14*2; a += 3.14*2/8)
		{
			tx_x = sin(a);
			tx_y = cos(a);

			//-1 -1
			p_x = t_x + tx_x*c - tx_y*s;
			p_y = t_y + tx_y*c + tx_x*s;
			p_z = t_z + 5;
			tx = (tx*0.5)+'0.5 0.5 0';
			R_PolygonVertex(p, tx, col, 1);
		}
		R_EndPolygon();
	}
};

void(vector mousepos) editor_terrain_overlay =
{
	float i;
	vector pos;
	vector colour;

	float ctime = gettime(5);
	if (mautorepeattime)
	{
		if (mousedown != 1)
			mautorepeattime = 0;
		else if (mautorepeattime < ctime)
		{
			mautorepeattime = ctime + 0.05;
			editor_do(mousepos);
		}
	}
	else if (mousedown == 1)
	{
		mautorepeattime = ctime + 0.5;
		editor_do(mousepos);
	}

	pos = '128 0 0';
	pos_y = vidsize_y - 32;
	drawfill('0 16 0', pos, '0 0 0', 0.3);

	pos = '0 16 0';
	for (i = 0; i < ter_count; i+=1)
	{
		if (curtool == i)
			colour = '1 0 0';
		else if (mousepos_x < 128 && mousepos_y >= pos_y && mousepos_y < pos_y + 8)
			colour = '0 0 1';
		else
			colour = '1 1 1';

		if (i == ter_radius)
			drawstring(pos, sprintf("radius: %g", eradius), '8 8 0', colour, 1, 0);
		else if (i == ter_quant)
			drawstring(pos, sprintf("quantity: %g", equant), '8 8 0', colour, 1, 0);
		else if (i == ter_strength)
			drawstring(pos, sprintf("percent: %g%%", epercent), '8 8 0', colour, 1, 0);
		else if (i == ter_mesh)
			drawstring(pos, sprintf("mesh: %s", meshname), '8 8 0', colour, 1, 0);
		else if (i == ter_tex)
		{
			if (curtool == ter_tex_get)
			{
				drawstring(pos, sprintf("Tex0: %s", tex[0]), '8 8 0', colour, 1, 0);	pos_y += 8;
				drawstring(pos, sprintf("Tex1: %s", tex[1]), '8 8 0', colour, 1, 0);	pos_y += 8;
				drawstring(pos, sprintf("Tex2: %s", tex[2]), '8 8 0', colour, 1, 0);	pos_y += 8;
				drawstring(pos, sprintf("Tex3: %s", tex[3]), '8 8 0', colour, 1, 0);
			}
			else
				drawstring(pos, sprintf("Tex%1i: %s", painttex, tex[painttex]), '8 8 0', colour, 1, 0);
			if (tex[painttex] != "" && whichpack(strcat("textures/", tex[painttex], ".tga")) != "")
				drawpic(pos + '0 8 0', tex[painttex], '128 128 0', '1 1 1', 1);
		
		}
		else if (i == ter_tintval)
			drawstring(pos, sprintf("Tint%1i: %s", painttex, tint[painttex]), '8 8 0', colour, 1, 0);
		else
			drawstring(pos, toolname[i], '8 8 0', colour, 1, 0);
		pos_y += 8;
	}
};
