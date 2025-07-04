
FTEQW uses [GLSL](https://www.khronos.org/opengl/wiki/Core_Language_%28GLSL%29)
for all of its shaders, with the legacy
[Quake 3 shader format](https://graphics.stanford.edu/courses/cs448-00-spring/q3ashader_manual.pdf)
acting as the glue between the raw textures and the GLSL.

A standard "wall" Q3 shader in FTEQW might be located at
`mygame/scripts/brick.shader` and might look like this:

```glsl
// material name used in level file
brick/wall001a
{
	// glsl program to use
	program defaultwall

	// diffuse texture file path, relative to gamedir
	diffusemap textures/brick/wall001a.tga
}
```

`defaultwall` is the embedded GLSL shader in FTEQW for anything that is not
water, sky, sprites, or models. These shader files are present in
[FTEQW's source code](https://github.com/fte-team/fteqw/tree/master/engine/shaders/glsl),
but you can also dump them with the console command `r_dumpshaders`. `diffusemap`
is an FTEQW-specific shortcut that passes the texture name into the GLSL shader
as the diffuse sampler.

Here is an incomplete list of the FTEQW shortcuts for passing samplers to GLSL
shaders:

- `diffusemap`: Used for the base texture.
- `fullbrightmap`: Used for glowing pixels.
- `specularmap`: Used for specular reflections.
- `normalmap`: Used for bump mapping.
- `lightmap`: Used for adding baked lighting to a surface.

Here is an incomplete list of the default shaders, and their uses:

- `defaultskin.glsl`: Used for models and their skins.
- `defaultsky.glsl`: Used for Quake's scrolling skies.
- `defaultskybox.glsl`: Used for Quake II and Half-Life's skyboxes.
- `defaultsprite.glsl`: Used for particles and sprites.
- `defaultwall.glsl`: Used for most level geometry.
- `defaultwarp.glsl`: Used for Quake's turbulent water surfaces.

These can be used in any Q3 shader with `program <name>`. Any Q3 shader file
with the extension `.shader` in the `scripts` folder of your game or mod will be
automatically loaded by the engine.

## Example Code

Here is an example of a very simple `defaultwall.glsl` replacement. It only takes
a diffuse sampler, an optional fullbrightmap, and a lightmap:

```glsl
!!ver 130 460
!!permu FULLBRIGHT
!!permu FOG
!!permu LIGHTSTYLED
!!samps diffuse lightmap fullbright

varying vec2 txc;
varying vec2 lmc;

#include "sys/defs.h"
#include "sys/fog.h"

#ifdef VERTEX_SHADER
	void main()
	{
		txc = v_texcoord;
		lmc = v_lmcoord;

		gl_Position = ftetransform();
	}
#endif

#ifdef FRAGMENT_SHADER
	void main()
	{
		// diffuse sampler
		vec4 diffuse = texture2D(s_diffuse, txc);

		// apply lightmap
		vec3 lightmaps = (texture2D(s_lightmap, lmc) * e_lmscale).rgb;
		diffuse.rgb *= lightmaps.rgb;

		// apply brightmap
		#if defined(FULLBRIGHT)
			vec4 brightmap = texture2D(s_fullbright, txc);
			diffuse = brightmap * brightmap.a + diffuse * (1.0 - brightmap.a);
		#endif

		// final color
		gl_FragColor = fog4(diffuse * e_colourident);
	}
#endif
```
