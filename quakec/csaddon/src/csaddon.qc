enum
{
	MODE_INITIAL=0,

	MODE_LIGHTEDIT=1,
	MODE_SPLINEEDIT=2,
	MODE_TERRAINEDIT=3,
	MODE_PARTICLEEDIT=4,
};

float autocvar_ca_show;
var float autocvar_ca_editormode = MODE_LIGHTEDIT;
string autocvar_ca_colourtint;

var vector vidsize = '640 480 0';
vector curmousepos;
vector mousediff;
vector originalmousepos;
float mousedown;
float shiftdown;

string pointedshadername;
vector pointedsurfacenormal;
entity pointedent;
float pointedsurface;

/*the renderscene builtin in the parent progs is redirected to here*/
void() wrap_renderscene =
{
	local int retval;
	vector col;

	vidsize = getproperty(VF_SCREENVSIZE);

	/*inactive? then show nothing*/
	if (!autocvar_ca_show)
	{
		if (isdemo())
			spline_overrides(gettime(5));
		renderscene();

		if (autocvar_ca_colourtint)
		{
			local string shdrname = strcat("tint_", autocvar_ca_colourtint);
			/*make sure the shader exist*/
			shaderforname(shdrname,
					"{\n"
						//if we can actually use glsl...
						"if $glsl\n"
							"glslprogram\n"
							"{\n"
								"varying vec4 tf;\n"
								"#ifdef VERTEX_SHADER\n"
									"void main ()\n"
									"{\n"
										"gl_Position = tf = ftetransform();\n"
									"}\n"
								"#endif\n"

								"#ifdef FRAGMENT_SHADER\n"
									"uniform sampler2D s_t0;\n"
									"uniform sampler3D s_t1;\n"
									"void main()\n"
									"{\n"
										"vec2 fc;\n"
										"fc = tf.xy / tf.w;\n"
										"vec3 raw = texture2D(s_t0, (1.0 + fc) / 2.0).rgb;\n"

										//scale+bias the sample to not clamp out at the edges
										"#define LUTSIZE 16.0\n"
										"vec3 scale = vec3((LUTSIZE-1.0)/LUTSIZE);\n"
										"vec3 bias = vec3(1.0/(2.0*LUTSIZE));\n"

										"gl_FragColor = texture3D(s_t1, raw * scale + bias);\n"
									"}\n"
								"#endif\n"
							"}\n"
							"{\n"
								"map $currentrender\n"
							"}\n"
							"{\n"
								//16*16*16 image
								"clampmap $3d:",autocvar_ca_colourtint,"\n"
							"}\n"
						//else (glsl not available)
						"][\n"
							//just don't draw anything.
							"surfaceparm nodraw\n"
						"]\n"
					"}\n"
				);
			drawpic(getproperty(VF_MIN), shdrname, getproperty(VF_SIZE), '1 1 1', 1);
		}
		return;
	}

	/*hide hud and crosshair*/
	setproperty(VF_DRAWENGINESBAR, 0);
	setproperty(VF_DRAWCROSSHAIR, 0);

	if (autocvar_ca_editormode == MODE_LIGHTEDIT)
		editor_lights_add();
	else if (autocvar_ca_editormode == MODE_SPLINEEDIT)
		editor_spline_add();
	else if (autocvar_ca_editormode == MODE_TERRAINEDIT)
		editor_terrain_add(curmousepos);

	renderscene();

	if (autocvar_ca_editormode == MODE_LIGHTEDIT)
		col = '1 0 0';
	else if (curmousepos_y < 8 && curmousepos_x >= 64*(MODE_LIGHTEDIT-1) && curmousepos_x < 64*MODE_LIGHTEDIT)
		col = '0 0 1';
	else
		col = '1 1 1';
	drawrawstring('64 0 0'*(MODE_LIGHTEDIT-1), "LIGHTS", '8 8 0', col, 1);

	if (autocvar_ca_editormode == MODE_SPLINEEDIT)
		col = '1 0 0';
	else if (curmousepos_y < 8 && curmousepos_x >= 64*(MODE_SPLINEEDIT-1) && curmousepos_x < 64*MODE_SPLINEEDIT)
		col = '0 0 1';
	else
		col = '1 1 1';
	drawrawstring('64 0 0'*(MODE_SPLINEEDIT-1), "SPLINES", '8 8 0', col, 1);

	if (autocvar_ca_editormode == MODE_PARTICLEEDIT)
		col = '1 0 0';
	else if (curmousepos_y < 8 && curmousepos_x >= 64*(MODE_PARTICLEEDIT-1) && curmousepos_x < 64*MODE_PARTICLEEDIT)
		col = '0 0 1';
	else
		col = '1 1 1';
	drawrawstring('64 0 0'*(MODE_PARTICLEEDIT-1), "PARTICLES", '8 8 0', col, 1);

	if (autocvar_ca_editormode == MODE_TERRAINEDIT)
		col = '1 0 0';
	else if (curmousepos_y < 8 && curmousepos_x >= 64*(MODE_TERRAINEDIT-1) && curmousepos_x < 64*MODE_TERRAINEDIT)
		col = '0 0 1';
	else
		col = '1 1 1';
	drawrawstring('64 0 0'*(MODE_TERRAINEDIT-1), "TERRAIN", '8 8 0', col, 1);



	if (autocvar_ca_editormode == MODE_LIGHTEDIT)
		editor_lights_overlay(curmousepos);
	else if (autocvar_ca_editormode == MODE_SPLINEEDIT)
	{
		editor_spline_overlay(&curmousepos, mousediff);
		originalmousepos = curmousepos;
		mousediff = '0 0 0';
	}
	else if (autocvar_ca_editormode == MODE_PARTICLEEDIT)
		editor_particles_overlay(curmousepos);
	else if (autocvar_ca_editormode == MODE_TERRAINEDIT)
		editor_terrain_overlay(curmousepos);

	drawcharacter(curmousepos - '4 4', '+', '8 8', '1 1 1', 1);
};

var float(float,float,float) orig_input_event = __NULL__;
float (float event, float parama, float paramb) wrap_InputEvent =
{
	if (autocvar_ca_show)
	{
		if (event == 0)
		{
			if (parama == 512 && curmousepos_y < 8)
			{
				cvar_set("ca_editormode", ftos(floor(curmousepos_x / 64)+1));
				return TRUE;
			}
			if (autocvar_ca_editormode == MODE_LIGHTEDIT)
			{
				if (editor_lights_key(parama, paramb, curmousepos))
					return TRUE;
			}
			else if (autocvar_ca_editormode == MODE_SPLINEEDIT)
			{
				if (editor_spline_key(parama, paramb, &curmousepos, mousediff))
				{
					originalmousepos = curmousepos;
					mousediff = '0 0 0';
					return TRUE;
				}
			}
			else if (autocvar_ca_editormode == MODE_PARTICLEEDIT)
			{
				if (editor_particles_key(parama, paramb, curmousepos))
					return TRUE;
			}
			else if (autocvar_ca_editormode == MODE_TERRAINEDIT)
			{
				if (editor_terrain_key(parama, paramb, curmousepos))
					return TRUE;
			}
			if (parama == 512)
			{
				mousedown = 1;
				return TRUE;
			}
			if (parama == 513)
			{
				mousedown = 2;
				return TRUE;
			}
		}
		else if (event == 1)
		{
			if (parama == 511+mousedown)
				mousedown = FALSE;
		}
		else if (event == 2)
		{
			if (mousedown == 2)
				return FALSE;
			originalmousepos = curmousepos;
			curmousepos_x += parama;
			curmousepos_y += paramb;
			mousediff = curmousepos - originalmousepos;
			if (curmousepos_x < 0)
				curmousepos_x = 0;
			if (curmousepos_y < 0)
				curmousepos_y = 0;
			if (curmousepos_x > vidsize_x)
				curmousepos_x = vidsize_x;
			if (curmousepos_y > vidsize_y)
				curmousepos_y = vidsize_y;
			return TRUE;
		}
		else if (event == 3)
		{
			curmousepos_x = parama;
			curmousepos_y = paramb;
			if (mousedown == 2)
				return FALSE;
			return TRUE;
		}
	}
	else if (pointedshadername != "")
	{
/*		if (parama == 513)
		{
			strunzone(pointedshadername);
			pointedshadername = "";
			print("release (mouse2)!\n");
			return TRUE;
		}
*/
		if (event == 0 || event == 1)
		{
			gecko_keyevent(pointedshadername, parama, event);
			if (event == 0)
				return TRUE;
		}
	}

	if (orig_input_event)
		return orig_input_event(event, parama, paramb);
	return FALSE;
}



/*this is a fallback function, in case the main progs does not have one*/
void(float width, float height, float do2d) CSQC_UpdateView =
{
	clearscene();
	setproperty(VF_DRAWENGINESBAR, 1);
	setproperty(VF_DRAWCROSSHAIR, 1);
	addentities(intermission?MASK_ENGINE:(MASK_VIEWMODEL|MASK_ENGINE));
	wrap_renderscene();
};

void() CSQC_Input_Frame =
{
	vector t, o;
	if ((input_buttons & 1) && pointedshadername == "")
	{
		t = unproject((vidsize*0.5) + '0 0 8192');
		o = unproject(vidsize*0.5);
		if (vlen(o - t) > 8192)
			t = o + normalize(t)*8192;
		traceline(o, t, TRUE, world);

		if (vlen(trace_endpos - o) < 512)
		{
			float s = getsurfacenearpoint(trace_ent, trace_endpos);
			pointedshadername = getsurfacetexture(trace_ent, s);
			pointedsurfacenormal = getsurfacenormal(trace_ent, s);
			if (gecko_get_texture_extent(pointedshadername) == '0 0 0')
				pointedshadername = "";
			if (pointedshadername == "")
				pointedshadername = "";
			else
			{
				pointedshadername = strzone(pointedshadername);
				//print("sending input to ", pointedshadername, "\n");

				gecko_navigate(pointedshadername, "cmd:focus");

				pointedent = trace_ent;
				pointedsurface = s;
			}
		}
	}

	if (pointedshadername != "")
	{
		input_buttons = input_buttons - (input_buttons & 1);

		makevectors(input_angles);
		if (v_forward * pointedsurfacenormal >= 0)
		{
			gecko_navigate(pointedshadername, "cmd:unfocus");
			strunzone(pointedshadername);
			pointedshadername = "";
		//	print("release (angle)!\n");
		}
		else
		{
			t = unproject((vidsize*0.5) + '0 0 8192');
			o = unproject(vidsize*0.5);
			if (vlen(o - t) > 8192)
				t = o + normalize(t)*8192;
			traceline(o, t, TRUE, world);

			//this code is vile.
			//it expects the texture to be aligned to the surface plane with flat projection.
			//it will also break on any polysoup/patch surfaces
			//it should at least accept any weird shaped cut up triangles, so long as they're all flat on the surface.

			vector xyz1, xyz2, xyz3;
			vector st1, st2, st3;
			xyz1 = getsurfacepointattribute(pointedent, pointedsurface, 0, 0);
			xyz2 = getsurfacepointattribute(pointedent, pointedsurface, 1, 0);
			xyz3 = getsurfacepointattribute(pointedent, pointedsurface, 2, 0);
			st1 = getsurfacepointattribute(pointedent, pointedsurface, 0, 4);
			st2 = getsurfacepointattribute(pointedent, pointedsurface, 1, 4);
			st3 = getsurfacepointattribute(pointedent, pointedsurface, 2, 4);

			float f1, f2, backoff;
			vector dir1 = xyz2 - xyz1;
			vector std1 = st2 - st1;
			vector dir2 = xyz3 - xyz1;
			vector std2 = st3 - st1;
			f1 = 1/vlen(dir1);
			dir1 *= f1;
			std1 *= f1;
			backoff = dir1 * dir2;
			std2 = std2 - (std1 * backoff);
			dir2 = dir2 - (dir1 * backoff);
			f2 = 1/vlen(dir2);
			dir1 *= f2;
			std1 *= f2;
			//the two directions should be perpendicular, and normalized

			float d1 = dir1*xyz1;
			float p = dir1*trace_endpos;
			float d2 = dir1*(xyz1+dir1);
			float f1 = (p-d1) / (d2 - d1);

			d1 = dir2*xyz1;
			p = dir2*trace_endpos;
			d2 = dir2*(xyz1+dir2);
			f2 = (p-d1) / (d2 - d1);

			vector res = st1 + std1*f1 + std2*f2;

			res_x-= floor(res_x);
			res_y-= floor(res_y);
			gecko_mousemove(pointedshadername, res_x, res_y);

//			pointparticles(particleeffectnum("te_spike"), xyz1 + dir1*f1 + dir2*f2, trace_plane_normal, 1);
		}
	}
};

/*
float(string txt, string info) CSQC_ConsoleLink =
{
	print("Console link pressed. Text \"", txt, "\"\n");
	print("Info: \"", info, "\"\n");

	return FALSE;
};
*/

/*this is a fallback function, in case the main progs does not have one*/
float (float event, float parama, float paramb) CSQC_InputEvent =
{
	return wrap_InputEvent(event, parama, paramb);
};

void(float prevprogs) init =
{
	Cam_Init();

	if (prevprogs >= 0)
	{
		/*its easy to wrap a builtin*/
		externset(0, wrap_renderscene, "renderscene");

		/*wrap the parent's input event function*/
		orig_input_event = externvalue(0, "CSQC_InputEvent");
		externset(0, wrap_InputEvent, "CSQC_InputEvent");
	}
};

void() CSQC_Shutdown =
{
	spline_shutdown();
};

