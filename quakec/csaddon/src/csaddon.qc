float autocvar_ca_show;
float autocvar_ca_editormode;
string autocvar_ca_colourtint;

enum
{
	MODE_INITIAL=0,

	MODE_LIGHTEDIT=1,
	MODE_SPLINEEDIT=2,
	MODE_PARTICLEEDIT=3
};

var vector vidsize = '640 480 0';
vector curmousepos;
vector mousediff;
vector originalmousepos;
float mousedown;

/*the renderscene builtin in the parent progs is redirected to here*/
void() wrap_renderscene =
{
	local int retval;
	vector col;
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

	if (orig_input_event)
		return orig_input_event(event, parama, paramb);
	return FALSE;
}



/*this is a fallback function, in case the main progs does not have one*/
void(float width, float height, float do2d) CSQC_UpdateView =
{
	vidsize_x = width;
	vidsize_y = height;
	vidsize_z = 0;
	clearscene();
	setproperty(VF_DRAWENGINESBAR, 1);
	setproperty(VF_DRAWCROSSHAIR, 1);
	addentities(intermission?MASK_ENGINE:(MASK_VIEWMODEL|MASK_ENGINE));
	wrap_renderscene();
};

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
