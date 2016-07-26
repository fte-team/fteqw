#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char shaders[][64] =
{
	"fixedemu",
	"altwater",
	"bloom_blur",
	"bloom_filter",
	"bloom_final",
	"colourtint",
	"crepuscular_opaque",
	"crepuscular_rays",
	"crepuscular_sky",
	"depthonly",
	"default2d",
	"defaultadditivesprite",
	"defaultskin",
	"defaultsky",
	"defaultfill",
	"defaultsprite",
	"defaultwall",
	"defaultwarp",
	"defaultgammacb",
	"drawflat_wall",
	"lpp_depthnorm",
	"lpp_light",
	"lpp_wall",
	"postproc_fisheye",
	"postproc_panorama",
	"postproc_laea",
	"postproc_stereographic",
	"postproc_equirectangular",
	"fxaa",
	"underwaterwarp",
	"menutint",
	"terrain",
	"rtlight",
	""
};

void dumpprogstring(FILE *out, FILE *src)
{
	int j;
	char line[1024];

	while(fgets(line, sizeof(line), src))
	{
		j = 0;
		while (line[j] == ' ' || line[j] == '\t')
			j++;
		if ((line[j] == '/' && line[j] == '/') || line[j] == '\r' || line[j] == '\n')
		{
			while (line[j])
				fputc(line[j++], out);
		}
		else
		{
			fputc('\"', out);				
			while (line[j] && line[j] != '\r' && line[j] != '\n')
			{
				if (line[j] == '\t')
					fputc(' ', out);
				else if (line[j] == '\"')
				{
					fputc('\\', out);
					fputc(line[j], out);
				}
				else
					fputc(line[j], out);
				j++;
			}
			fputs("\\n\"\n", out);
		}
	}
	fflush(out);
}
void dumpprogblob(FILE *out, unsigned char *buf, unsigned int size)
{
	size_t totallen, i, linelen;
	totallen = 0;
	linelen = 32;
	fflush(out);
	fprintf(out, "\"");
	for (i=0;i<size;i++)
	{
		fprintf(out, "\\x%02X",buf[i]);
		if (i % linelen == linelen - 1)
			fprintf(out, "\"\n\"");
	}
	fprintf(out, "\"");
	fflush(out);
}

struct blobheader
{
	unsigned char blobmagic[4];	//\xffSPV
	unsigned int blobversion;
	unsigned int defaulttextures;	//s_diffuse etc flags
	unsigned int numtextures;		//s_t0 count
	unsigned int permutations;		//

	unsigned int cvarsoffset;	//double-null terminated string. I,XYZW prefixes
	unsigned int cvarslength;

	unsigned int vertoffset;
	unsigned int vertlength;
	unsigned int fragoffset;
	unsigned int fraglength;
};
void generateprogsblob(struct blobheader *prototype, FILE *out, FILE *vert, FILE *frag)
{
	struct blobheader *blob;
	int fraglen, vertlen, blobsize, cvarlen;
	cvarlen = prototype->cvarslength;
	cvarlen = (cvarlen + 3) & ~3;	//round up for padding.
	fseek(vert, 0, SEEK_END);
	fseek(frag, 0, SEEK_END);
	vertlen = ftell(vert);
	fraglen = ftell(frag);
	fseek(vert, 0, SEEK_SET);
	fseek(frag, 0, SEEK_SET);
	blobsize = sizeof(*blob) + cvarlen + fraglen + vertlen;
	blob = malloc(blobsize);
	*blob = *prototype;
	blob->cvarsoffset = sizeof(*blob);
	blob->cvarslength = prototype->cvarslength;	//unpadded length
	blob->vertoffset = blob->cvarsoffset+cvarlen;
	blob->vertlength = vertlen;
	blob->fragoffset = blob->vertoffset+vertlen;
	blob->fraglength = fraglen;
	memcpy((char*)blob+blob->cvarsoffset, (char*)prototype+prototype->cvarsoffset, prototype->cvarslength);
	fread((char*)blob+blob->vertoffset, blob->vertlength, 1, vert);
	fread((char*)blob+blob->fragoffset, blob->fraglength, 1, frag);
	dumpprogblob(out, (unsigned char*)blob, blobsize);
	free(blob);
}


int generatevulkanblobs(struct blobheader *blob, size_t maxblobsize, char *fname)
{
	char command[1024];
	char glslname[256];
	char tempname[256];
	int inheader = 1;
	int i;
	unsigned short constid = 256;	//first few are reserved.

	const char *permutationnames[] =
	{
		"BUMP",
		"FULLBRIGHT",
		"UPPERLOWER",
		"REFLECTCUBEMASK",
		"SKELETAL",
		"FOG",
		"FRAMEBLEND",
		"LIGHTSTYLED",
		NULL
	};

	snprintf(glslname, sizeof(glslname), "vulkan/%s.glsl", fname);
	snprintf(tempname, sizeof(tempname), "vulkan/%s.tmp", fname);
//	snprintf(vertname, sizeof(vertname), "vulkan/%s.vert", fname);
//	snprintf(fragname, sizeof(fragname), "vulkan/%s.frag", fname);

	memcpy(blob->blobmagic, "\xffSPV", 4);
	blob->blobversion = 1;
	blob->defaulttextures = 0;
	blob->numtextures = 0;
	blob->permutations = 0;
	blob->cvarsoffset = sizeof(*blob);
	blob->cvarslength = 0;

	FILE *glsl = fopen(glslname, "rt");
	if (!glsl)
		return 0;
	FILE *temp = fopen(tempname, "wt");
	while(fgets(command, sizeof(command), glsl))
	{
		if (inheader && !strncmp(command, "!!", 2))
		{
			if (!strncmp(command, "!!cvar", 6) || !strncmp(command, "!!arg", 5))
			{
				unsigned int type;
				unsigned int size;
				union
				{
					float f;
					unsigned int u;
				} u[4];
				char *arg;
				unsigned char *cb = (unsigned char*)blob + blob->cvarsoffset + blob->cvarslength;

				if (command[2] == 'a')
				{
					type = command[5] == 'i' || command[5] == 'f' || command[5] == 'b';
					size = type?1:(command[5]-'0');
					arg = strtok(command+7, " ,=\n");
					type = command[6-type] - 'a' + 'A';
				}
				else
				{
					type = command[6] == 'i' || command[6] == 'f' || command[6] == 'b';
					size = type?1:(command[6]-'0');
					arg = strtok(command+8, " ,=\n");
					type = command[7-type];
				}

				cb[0] = (constid>>8)&0xff;
				cb[1] = (constid>>0)&0xff;
				cb[2] = type;
				cb[3] = size + '0';
				cb += 4;
				while(*arg)
					*cb++ = *arg++;
				*cb++ = 0;

				for (i = 0; i < size; i++)
				{
					if (arg)
					{
						arg = strtok(NULL, " ,=\n");
						if (type == 'f' || type == 'F')
							u[i].f = atof(arg);
						else
							u[i].u = atoi(arg);
					}
					else
						u[i].u = 0;
					*cb++ = (u[i].u>>24)&0xff;
					*cb++ = (u[i].u>>16)&0xff;
					*cb++ = (u[i].u>>8)&0xff;
					*cb++ = (u[i].u>>0)&0xff;
				}
				blob->cvarslength = cb - ((unsigned char*)blob + blob->cvarsoffset);
				constid += size;
			}
			else if (!strncmp(command, "!!permu", 7))
			{
				char *arg = strtok(command+7, " ,\n");
				for (i = 0; permutationnames[i]; i++)
				{
					if (!strcmp(arg, permutationnames[i]))
					{
						blob->permutations |= 1u<<i;
						break;
					}
				}
				if (!permutationnames[i])
					printf("Unknown permutation: \"%s\"\n", arg);
			}
			else if (!strncmp(command, "!!samps", 7))
			{
				char *arg = strtok(command+7, " ,\n");
				do
				{
					//light
					if (!strcasecmp(arg, "shadowmap"))
						blob->defaulttextures |= 1u<<0;
					else if (!strcasecmp(arg, "projectionmap"))
						blob->defaulttextures |= 1u<<1;

					//material
					else if (!strcasecmp(arg, "diffuse"))
						blob->defaulttextures |= 1u<<2;
					else if (!strcasecmp(arg, "normalmap"))
						blob->defaulttextures |= 1u<<3;
					else if (!strcasecmp(arg, "specular"))
						blob->defaulttextures |= 1u<<4;
					else if (!strcasecmp(arg, "upper"))
						blob->defaulttextures |= 1u<<5;
					else if (!strcasecmp(arg, "lower"))
						blob->defaulttextures |= 1u<<6;
					else if (!strcasecmp(arg, "fullbright"))
						blob->defaulttextures |= 1u<<7;
					else if (!strcasecmp(arg, "paletted"))
						blob->defaulttextures |= 1u<<8;
					else if (!strcasecmp(arg, "reflectcube"))
						blob->defaulttextures |= 1u<<9;
					else if (!strcasecmp(arg, "reflectmask"))
						blob->defaulttextures |= 1u<<10;

					//batch
					else if (!strcasecmp(arg, "lightmap"))
						blob->defaulttextures |= 1u<<11;
					else if (!strcasecmp(arg, "deluxmap"))
						blob->defaulttextures |= 1u<<12;
					else if (!strcasecmp(arg, "lightmaps"))
						blob->defaulttextures |= 1u<<11 | 1u<<13 | 1u<<14 | 1u<<15;
					else if (!strcasecmp(arg, "deluxmaps"))
						blob->defaulttextures |= 1u<<12 | 1u<<16 | 1u<<17 | 1u<<18;

					//shader pass
					else if (atoi(arg))
						blob->numtextures = atoi(arg);
					else
						printf("Unknown texture: \"%s\"\n", arg);
				} while((arg = strtok(NULL, " ,\n")));
			}
			continue;
		}
		else if (inheader && !strncmp(command, "//", 2))
			continue;
		else if (inheader)
		{
			const char *specialnames[] =
			{
				//light
				"uniform sampler2DShadow s_shadowmap;\n",
				"uniform samplerCube s_projectionmap;\n",

				//material
				"uniform sampler2D s_diffuse;\n",
				"uniform sampler2D s_normalmap;\n",
				"uniform sampler2D s_specular;\n",
				"uniform sampler2D s_upper;\n",
				"uniform sampler2D s_lower;\n",
				"uniform sampler2D s_fullbright;\n",
				"uniform sampler2D s_paletted;\n",
				"uniform samplerCube s_reflectcube;\n",
				"uniform sampler2D s_reflectmask;\n",

				//batch
				"uniform sampler2D s_lightmap;\n#define s_lightmap0 s_lightmap\n",
				"uniform sampler2D s_deluxmap;\n#define s_deluxmap0 s_deluxmap\n",
				"uniform sampler2D s_lightmap1;\n",
				"uniform sampler2D s_lightmap2;\n",
				"uniform sampler2D s_lightmap3;\n",
				"uniform sampler2D s_deluxmap1;\n",
				"uniform sampler2D s_deluxmap2;\n",
				"uniform sampler2D s_deluxmap3;\n"
			};
			int binding = 2;
			inheader = 0;
			fprintf(temp, "#define OFFSETMAPPING (cvar_r_glsl_offsetmapping>0)\n");
			fprintf(temp, "#define SPECULAR (cvar_gl_specular>0)\n");
			fprintf(temp, "#ifdef FRAGMENT_SHADER\n");
			for (i = 0; i < sizeof(specialnames)/sizeof(specialnames[0]); i++)
			{
				if (blob->defaulttextures & (1u<<i))
					fprintf(temp, "layout(set=0, binding=%u) %s", binding++, specialnames[i]);
			}
			for (i = 0; i < blob->numtextures; i++)
			{
				fprintf(temp, "layout(set=0, binding=%u) uniform sampler2D s_t%u;\n", binding++, i);
			}
			fprintf(temp, "#endif\n");

			//cvar specialisation constants
			{
				unsigned char *cb = (unsigned char*)blob + blob->cvarsoffset;
				while (cb < (unsigned char*)blob + blob->cvarsoffset + blob->cvarslength)
				{
					union
					{
						float f;
						unsigned int u;
					} u[4];
					unsigned short id;
					unsigned char type;
					unsigned char size;
					char *name;
					id = *cb++<<8;
					id |= *cb++;
					type = *cb++;
					size = (*cb++)-'0';
					name = cb;
					cb += strlen(name)+1;
					for (i = 0; i < size; i++)
					{
						u[i].u = (cb[0]<<24)|(cb[1]<<16)|(cb[2]<<8)|(cb[3]<<0);
						cb+=4;
					}
#if 0 //all is well
					if (size == 1 && type == 'b')
						fprintf(temp, "layout(constant_id=%u) const bool cvar_%s = %s;\n", id, name, (int)u[0].u?"true":"false");
					else if (size == 1 && type == 'i')
						fprintf(temp, "layout(constant_id=%u) const int cvar_%s = %i;\n", id, name, (int)u[0].u);
					else if (size == 1 && type == 'f')
						fprintf(temp, "layout(constant_id=%u) const float cvar_%s = %f;\n", id, name, u[0].f);
					else if (size == 3 && type == 'f')
					{
						fprintf(temp, "layout(constant_id=%u) const float cvar_%s_x = %f;\n", id+0, name, u[0].f);
						fprintf(temp, "layout(constant_id=%u) const float cvar_%s_y = %f;\n", id+1, name, u[1].f);
						fprintf(temp, "layout(constant_id=%u) const float cvar_%s_z = %f;\n", id+2, name, u[2].f);
						fprintf(temp, "vec3 cvar_%s = vec3(cvar_%s_x, cvar_%s_y, cvar_%s_z);\n", name, name, name, name);
					}
					else 	if (size == 1 && type == 'B')
						fprintf(temp, "layout(constant_id=%u) const bool arg_%s = %s;\n", id, name, (int)u[0].u?"true":"false");
					else 	if (size == 1 && type == 'I')
						fprintf(temp, "layout(constant_id=%u) const int arg_%s = %i;\n", id, name, (int)u[0].u);
					else if (size == 1 && type == 'F')
						fprintf(temp, "layout(constant_id=%u) const float arg_%s = %i;\n", id, name, u[0].f);
					else if (size == 3 && type == 'F')
					{
						fprintf(temp, "layout(constant_id=%u) const float arg_%s_x = %f;\n", id+0, name, u[0].f);
						fprintf(temp, "layout(constant_id=%u) const float arg_%s_y = %f;\n", id+1, name, u[1].f);
						fprintf(temp, "layout(constant_id=%u) const float arg_%s_z = %f;\n", id+2, name, u[2].f);
						fprintf(temp, "vec3 arg_%s = vec3(arg_%s_x, arg_%s_y, arg_%s_z);\n", name, name, name, name);
					}
#else
					//these initialised values are fucked up because glslangvalidator's spirv generator is fucked up and folds specialisation constants.
					//we get around this by ensuring that all such constants are given unique values to prevent them being folded, with the engine overriding everything explicitly.
					if (size == 1 && type == 'b')
					{
						fprintf(temp, "layout(constant_id=%u) const int _cvar_%s = %i;\n", id, name, id);//(int)u[0].u?"true":"false");
						fprintf(temp, "#define cvar_%s (_cvar_%s!=0)\n", name, name);
					}
					else if (size == 1 && type == 'i')
						fprintf(temp, "layout(constant_id=%u) const int cvar_%s = %i;\n", id, name, id);//(int)u[0].u);
					else if (size == 1 && type == 'f')
						fprintf(temp, "layout(constant_id=%u) const float cvar_%s = %i;\n", id, name, id);//u[0].f);
					else if (size == 3 && type == 'f')
					{
						fprintf(temp, "layout(constant_id=%u) const float cvar_%s_x = %i;\n", id+0, name, id+0);//u[0].f);
						fprintf(temp, "layout(constant_id=%u) const float cvar_%s_y = %i;\n", id+1, name, id+1);//u[1].f);
						fprintf(temp, "layout(constant_id=%u) const float cvar_%s_z = %i;\n", id+2, name, id+2);//u[2].f);
						fprintf(temp, "vec3 cvar_%s = vec3(cvar_%s_x, cvar_%s_y, cvar_%s_z);\n", name, name, name, name);
					}
					else 	if (size == 1 && type == 'B')
					{
						fprintf(temp, "layout(constant_id=%u) const int _arg_%s = %i;\n", id, name, id);//(int)u[0].u?"true":"false");
						fprintf(temp, "#define arg_%s (_arg_%s!=0)\n", name, name);
					}
					else 	if (size == 1 && type == 'I')
						fprintf(temp, "layout(constant_id=%u) const int arg_%s = %i;\n", id, name, id);//(int)u[0].u);
					else if (size == 1 && type == 'F')
						fprintf(temp, "layout(constant_id=%u) const float arg_%s = %i;\n", id, name, id);//u[0].f);
					else if (size == 3 && type == 'F')
					{
						fprintf(temp, "layout(constant_id=%u) const float arg_%s_x = %i;\n", id+0, name, id+0);//u[0].f);
						fprintf(temp, "layout(constant_id=%u) const float arg_%s_y = %i;\n", id+1, name, id+1);//u[1].f);
						fprintf(temp, "layout(constant_id=%u) const float arg_%s_z = %i;\n", id+2, name, id+2);//u[2].f);
						fprintf(temp, "vec3 arg_%s = vec3(arg_%s_x, arg_%s_y, arg_%s_z);\n", name, name, name, name);
					}
#endif
				}
			}
			//permutation stuff
			for (i = 0; i < sizeof(specialnames)/sizeof(specialnames[0]); i++)
			{
				if (blob->permutations & (1<<i))
				{
#if 0 //all is well
					fprintf(temp, "layout(constant_id=%u) const bool %s = %s;\n", 16+i, permutationnames[i], "false");
#else
					fprintf(temp, "layout(constant_id=%u) const int _%s = %i;\n", 16+i, permutationnames[i], 16+i);
					fprintf(temp, "#define %s (_%s!=0)\n", permutationnames[i], permutationnames[i]);
#endif
				}
			}
		}
		fputs(command, temp);
	}
	fclose(temp);
	fclose(glsl);

	snprintf(command, sizeof(command), 
		/*preprocess the vertex shader*/
		"echo #version 450 core > vulkan/%s.vert && "
		"cpp vulkan/%s.tmp -DVULKAN -DVERTEX_SHADER -P >> vulkan/%s.vert && "

		/*preprocess the fragment shader*/
		"echo #version 450 core > vulkan/%s.frag && "
		"cpp vulkan/%s.tmp -DVULKAN -DFRAGMENT_SHADER -P >> vulkan/%s.frag && "

		/*convert to spir-v (annoyingly we have no control over the output file names*/
		"glslangValidator -V -l -d vulkan/%s.vert vulkan/%s.frag"

		/*strip stuff out, so drivers don't glitch out from stuff that we don't use*/
		" && spirv-remap -i vert.spv frag.spv -o vulkan/remap"

		,fname, fname, fname, fname, fname, fname, fname, fname);

	system(command);

	return 1;
}

struct shadertype_s
{
	char *filepattern;
	char *preprocessor;
	char *rendererapi;
	int apiversion;
} shadertype[] =
{
	{"glsl/%s.glsl", 	"GLQUAKE", 	"QR_OPENGL", 		110},	//gl2+
	//{"gles/%s.glsl", 	"GLQUAKE", 	"QR_OPENGL", 		100},	//gles
	{"hlsl9/%s.hlsl", 	"D3D9QUAKE", 	"QR_DIRECT3D9", 	9},	//d3d9
	{"hlsl11/%s.hlsl", 	"D3D11QUAKE", 	"QR_DIRECT3D11", 	11},	//d3d11
	{"vulkan/remap/%s.spv", "VKQUAKE", 	"QR_VULKAN", 		-1},	//vulkan
};
//tbh we should precompile the d3d shaders.

int main(void)
{
	FILE *c, *s;
	char line[1024];
	int i, j, a;
	c = fopen("../gl/r_bishaders.h", "wt");

	if (!c)
	{
		printf("unable to open a file\n");
		return;
	}

	fprintf(c, "/*\nWARNING: THIS FILE IS GENERATED BY '"__FILE__"'.\nYOU SHOULD NOT EDIT THIS FILE BY HAND\n*/\n\n");

	for (i = 0; *shaders[i]; i++)
	{
		for (a = 0; a < sizeof(shadertype)/sizeof(shadertype[0]); a++)
		{
			if (shadertype[a].apiversion == -1)
			{
				FILE *v, *f;
				char proto[8192];
				if (!generatevulkanblobs((struct blobheader*)proto, sizeof(proto), shaders[i]))
					continue;
				sprintf(line, shadertype[a].filepattern, "vert");
				v = fopen(line, "rb");
				sprintf(line, shadertype[a].filepattern, "frag");
				f = fopen(line, "rb");
				if (f && v)
				{
					fprintf(c, "#ifdef %s\n", shadertype[a].preprocessor);
					fprintf(c, "{%s, %i, \"%s\",\n", shadertype[a].rendererapi, shadertype[a].apiversion, shaders[i]);
					generateprogsblob((struct blobheader*)proto, c, v, f);
					fputs("},\n", c);
					fprintf(c, "#endif\n");
				}
				fclose(f);
				fclose(v);
			}
			else
			{
				sprintf(line, shadertype[a].filepattern, shaders[i]);
				s = fopen(line, "rt");
				if (!s)
				{
					printf("unable to open %s\n", line);
					continue;
				}
				fprintf(c, "#ifdef %s\n", shadertype[a].preprocessor);
				fprintf(c, "{%s, %i, \"%s\",\n", shadertype[a].rendererapi, shadertype[a].apiversion, shaders[i]);

				dumpprogstring(c, s);

				fputs("},\n", c);
				fprintf(c, "#endif\n");
				fclose(s);
			}
			fflush(c);
		}
	}

	fclose(c);
}
