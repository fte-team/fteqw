!!ver 110
!!permu FRAMEBLEND
!!permu BUMP
!!permu FOG
!!permu SKELETAL
!!permu AMBIENTCUBE
!!samps diffuse fullbright normalmap
!!permu FAKESHADOWS
!!cvardf r_glsl_pcf
!!samps =FAKESHADOWS shadowmap

// envmaps only
!!samps =REFLECTCUBEMASK reflectmask reflectcube

!!cvardf r_skipDiffuse

#include "sys/defs.h"

varying vec2 tex_c;
varying vec3 norm;

/* CUBEMAPS ONLY */
#ifdef REFLECTCUBEMASK
	varying vec3 eyevector;
	varying mat3 invsurface;
#endif

#ifdef FAKESHADOWS
	varying vec4 vtexprojcoord;
#endif

#ifdef VERTEX_SHADER
	#include "sys/skeletal.h"

	void main (void)
	{
		vec3 n, s, t, w;
		tex_c = v_texcoord;
		gl_Position = skeletaltransform_wnst(w,n,s,t);
		norm = n;

/* CUBEMAPS ONLY */
#ifdef REFLECTCUBEMASK
		invsurface = mat3(v_svector, v_tvector, v_normal);

		vec3 eyeminusvertex = e_eyepos - v_position.xyz;
		eyevector.x = dot(eyeminusvertex, v_svector.xyz);
		eyevector.y = dot(eyeminusvertex, v_tvector.xyz);
		eyevector.z = dot(eyeminusvertex, v_normal.xyz);
#endif
		
		#ifdef FAKESHADOWS
		vtexprojcoord = (l_cubematrix*vec4(v_position.xyz, 1.0));
		#endif
	}
#endif


#ifdef FRAGMENT_SHADER
	#include "sys/fog.h"
	#include "sys/pcf.h"

	float lambert(vec3 normal, vec3 dir)
	{
		return max(dot(normal, dir), 0.0);
	}

	float halflambert(vec3 normal, vec3 dir)
	{
		return (lambert(normal, dir) * 0.5) + 0.5;
	}

	void main (void)
	{
		vec4 diffuse_f = texture2D(s_diffuse, tex_c);
		vec3 light;

#ifdef MASKLT
		if (diffuse_f.a < float(MASK))
			discard;
#endif

/* Normal/Bumpmap Shenanigans */
#ifdef BUMP
		/* Source's normalmaps are in the DX format where the green channel is flipped */
		vec3 normal_f = texture2D(s_normalmap, tex_c).rgb;
		normal_f.g *= -1.0;
		normal_f = normalize(normal_f.rgb - 0.5);
#else
		vec3 normal_f = vec3(0.0,0.0,1.0);
#endif

/* CUBEMAPS ONLY */
#ifdef REFLECTCUBEMASK
	/* when ENVFROMBASE is set or a normal isn't present, we're getting the reflectivity info from the diffusemap's alpha channel */
	#if defined(ENVFROMBASE) || !defined(BUMP)
		#define refl 1.0 - diffuse_f.a
	#else
		#define refl texture2D(s_normalmap, tex_c).a
	#endif
		vec3 cube_c = reflect(normalize(-eyevector), normal_f.rgb);
		cube_c = cube_c.x * invsurface[0] + cube_c.y * invsurface[1] + cube_c.z * invsurface[2];
		cube_c = (m_model * vec4(cube_c.xyz, 0.0)).xyz;
		diffuse_f.rgb += (textureCube(s_reflectcube, cube_c).rgb * vec3(refl,refl,refl));
#endif

#ifdef AMBIENTCUBE
		//no specular effect here. use rtlights for that.
		vec3 nn = norm*norm; //FIXME: should be worldspace normal.
		light = nn.x * e_light_ambientcube[(norm.x<0.0)?1:0] +
				nn.y * e_light_ambientcube[(norm.y<0.0)?3:2] +
				nn.z * e_light_ambientcube[(norm.z<0.0)?5:4];
#else
	#ifdef HALFLAMBERT
		light = e_light_ambient + (e_light_mul * halflambert(norm, e_light_dir));
	#else
		light = e_light_ambient + (e_light_mul * lambert(norm, e_light_dir));
	#endif

		/* the light we're getting is always too bright */
		light *= 0.75;

		/* clamp at 1.5 */
		if (light.r > 1.5)
			light.r = 1.5;
		if (light.g > 1.5)
			light.g = 1.5;
		if (light.b > 1.5)
			light.b = 1.5;
#endif

		diffuse_f.rgb *= light;

	#ifdef FAKESHADOWS
		diffuse_f.rgb *= ShadowmapFilter(s_shadowmap, vtexprojcoord);
	#endif

		gl_FragColor = fog4(diffuse_f * e_colourident) * e_lmscale;
	}
#endif
