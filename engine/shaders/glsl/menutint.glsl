!!cvari r_menutint_inverse
!!cvard_srgb r_menutint
!!samps 1
!!cvardf gl_stipplealpha_menu=0

#ifdef VERTEX_SHADER
		attribute vec2 v_texcoord;
		varying vec2 texcoord;
		uniform vec4 e_rendertexturescale;
		void main(void)
		{
			texcoord.x = v_texcoord.x*e_rendertexturescale.x;
			texcoord.y = (1.0-v_texcoord.y)*e_rendertexturescale.y;
			gl_Position = ftetransform();
		}
#endif
#ifdef FRAGMENT_SHADER

		varying vec2 texcoord;
		uniform int cvar_r_menutint_inverse;
		const vec3 lumfactors = vec3(0.299, 0.587, 0.114);
		const vec3 invertvec = vec3(1.0, 1.0, 1.0);
		void main(void)
		{
			vec3 texcolor = texture2D(s_t0, texcoord).rgb;
			float luminance = dot(lumfactors, texcolor);
			texcolor = vec3(luminance, luminance, luminance);
			texcolor *= r_menutint;
			texcolor = (cvar_r_menutint_inverse > 0) ? (invertvec - texcolor) : texcolor;

			/*  WinQuake-like stipple, by eukara */
			#if gl_stipplealpha_menu==1
			float alpha = 0.5;
			int x = int(mod(gl_FragCoord.x, 2.0));
			int y = int(mod(gl_FragCoord.y, 2.0));

			if (alpha <= 0.0) {
					discard;
			} else if (alpha <= 0.25) {
				if (x + y == 2)
					discard;
				if (x + y == 1)
					discard;
			} else if (alpha <= 0.5) {
				if (x + y == 2)
					discard;
				if (x + y == 0)
					discard;
			} else if (alpha < 1.0) {
				if (x + y == 2)
					discard;
			}
			#endif
			gl_FragColor = vec4(texcolor, 1.0);
		}
#endif
