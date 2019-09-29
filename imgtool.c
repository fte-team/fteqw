#include "quakedef.h"
#include "shader.h"
#undef stderr
#define stderr stdout

#ifdef _WIN32
#include <windows.h>
#endif
void VARGS Sys_Error (const char *fmt, ...)
{
	va_list		argptr;

	va_start (argptr,fmt);
	vfprintf (stderr,fmt,argptr);
	va_end (argptr);
	fflush(stderr);

	exit(1);
}
void VARGS Con_Printf (const char *fmt, ...)
{
	va_list		argptr;

	va_start (argptr,fmt);
	vfprintf (stderr,fmt,argptr);
	va_end (argptr);
	fflush(stderr);
}
void VARGS Con_DPrintf (const char *fmt, ...)
{
	va_list		argptr;

	va_start (argptr,fmt);
	vfprintf (stderr,fmt,argptr);
	va_end (argptr);
	fflush(stderr);
}
void VARGS Con_ThrottlePrintf (float *timer, int developerlevel, const char *fmt, ...)
{
	va_list		argptr;

	va_start (argptr,fmt);
	vfprintf (stderr,fmt,argptr);
	va_end (argptr);
	fflush(stderr);
}

void *ZF_Malloc(size_t size)
{
#if defined(__linux__)
	void *ret = NULL;
	if (!posix_memalign(&ret, max(sizeof(float)*4, sizeof(void*)), size))
		memset(ret, 0, size);
	return ret;
#else
	return calloc(size, 1);
#endif
}
void *Z_Malloc(size_t size)
{
	void *r = ZF_Malloc(size);
	if (!r)
		exit(1);
	return r;
}
void *BZ_Malloc(size_t size)
{
	return Z_Malloc(size);
}
void *BZF_Malloc(size_t size)
{
	return Z_Malloc(size);
}
void BZ_Free(void *p)
{
	free(p);
}
void Z_Free(void *p)
{
	free(p);
}

#include <sys/stat.h>

void FS_CreatePath(const char *pname, enum fs_relative relativeto)
{
	char *t = strdup(pname), *sl = t;
	while ((sl=strchr(sl, '/')))
	{
		*sl=0;
#ifdef _WIN32
		CreateDirectoryA(t, NULL);
#else
		mkdir(t, 0777);
#endif
		*sl++='/';
	}
	free(t);
}
qboolean Sys_remove (const char *path)
{
	//remove is part of c89.
	if (remove(path) == -1)
		return false;
	return true;
}
qboolean FS_NativePath(const char *fname, enum fs_relative relativeto, char *out, int outlen)
{
	Q_strncpyz(out, fname, outlen);
	return true;
}

#ifdef __unix__
#include <sys/stat.h>
#endif
qbyte *FS_LoadMallocFile (const char *path, size_t *fsize)
{
	qbyte *data = NULL;
	FILE *f;
#ifdef __unix__
	struct stat sb;
	if (stat(path, &sb) < 0)
		return NULL;
	if ((sb.st_mode&S_IFMT) != S_IFREG)
		return NULL;
#endif

	f = fopen(path, "rb");
	if (f)
	{
		long int sz;
		if (fseek(f, 0, SEEK_END) >= 0)
		{
			sz = ftell(f);
			if (sz >= 0)
			{
				*fsize = sz;
				fseek(f, 0, SEEK_SET);
				data = ZF_Malloc(*fsize+1);
				if (data)
				{
					data[*fsize] = 0;
					fread(data, 1, *fsize, f);
				}
				else
					Con_Printf("Unable to allocate memory for %s\n", path);
			}
		}
		fclose(f);
	}
	return data;
}
#ifdef _WIN32
dllhandle_t *Sys_LoadLibrary(const char *name, dllfunction_t *funcs)
{
	return NULL;
}
#else
#include <dlfcn.h>
void Sys_CloseLibrary(dllhandle_t *lib)
{
	dlclose((void*)lib);
}
dllhandle_t *Sys_LoadLibrary(const char *name, dllfunction_t *funcs)
{
	int i;
	dllhandle_t *lib;

	lib = NULL;
	if (!lib)
		lib = dlopen (name, RTLD_LOCAL|RTLD_LAZY);
//	if (!lib && !strstr(name, ".so"))
//		lib = dlopen (va("%s.so", name), RTLD_LOCAL|RTLD_LAZY);
	if (!lib)
	{
//		Con_DPrintf("%s\n", dlerror());
		return NULL;
	}

	if (funcs)
	{
		for (i = 0; funcs[i].name; i++)
		{
			*funcs[i].funcptr = dlsym(lib, funcs[i].name);
			if (!*funcs[i].funcptr)
				break;
		}
		if (funcs[i].name)
		{
			Con_DPrintf("Unable to find symbol \"%s\" in \"%s\"\n", funcs[i].name, name);
			Sys_CloseLibrary((dllhandle_t*)lib);
			lib = NULL;
		}
	}

	return (dllhandle_t*)lib;
}
#endif

struct imgfile_s
{
	vfsfile_t pub;
	FILE *f;
};
static qboolean QDECL ImgFile_Close(struct vfsfile_s *file)
{
	struct imgfile_s *f = (struct imgfile_s*)file;
	fclose(f->f);
	free(f);
	return true;
}
static int QDECL ImgFile_WriteBytes(struct vfsfile_s *file, const void *buffer, int bytestowrite)
{
	struct imgfile_s *f = (struct imgfile_s*)file;
	return fwrite(buffer, 1, bytestowrite, f->f);
}
vfsfile_t *QDECL FS_OpenVFS(const char *filename, const char *mode, enum fs_relative relativeto)
{
	if (!strcmp(mode, "wb"))
	{
		struct imgfile_s *r = malloc(sizeof(*r));
		r->f = fopen(filename, mode);
		r->pub.seekstyle = SS_UNSEEKABLE;
		r->pub.Close = ImgFile_Close;
		r->pub.WriteBytes = ImgFile_WriteBytes;
		if (r->f)
			return &r->pub;
		free(r);
	}
	return NULL;
}
qboolean COM_WriteFile (const char *filename, enum fs_relative fsroot, const void *data, int len)
{
	return false;
}
void QDECL Q_strncpyz(char *d, const char *s, int n)
{
	int i;
	n--;
	if (n < 0)
		return;	//this could be an error

	for (i=0; *s; i++)
	{
		if (i == n)
			break;
		*d++ = *s++;
	}
	*d='\0';
}
void VARGS Q_vsnprintfz (char *dest, size_t size, const char *fmt, va_list argptr)
{
#ifdef _WIN32
	_vsnprintf (dest, size, fmt, argptr);
#else
	#ifdef _DEBUG
		if ((size_t)vsnprintf (dest, size, fmt, argptr) > size-1)
			Sys_Error("Q_vsnprintfz: truncation");
	#else
		vsnprintf (dest, size, fmt, argptr);
	#endif
#endif
	dest[size-1] = 0;
}
void VARGS Q_snprintfz (char *dest, size_t size, const char *fmt, ...)
{
	va_list		argptr;

	va_start (argptr, fmt);
	Q_vsnprintfz(dest, size, fmt, argptr);
	va_end (argptr);
}

qbyte		*host_basepal;
unsigned int	d_8to24rgbtable[256];
unsigned int	d_8to24bgrtable[256];
qbyte GetPaletteIndexNoFB(int red, int green, int blue)
{
	return 0;
}

sh_config_t sh_config;
viddef_t vid;

struct opts_s
{
	unsigned int flags;			//image flags to use (affects how textures get interpreted a little)
	unsigned int mipnum;		//when exporting to a mipless format, this is the mip level that is actually written. default 0.
	uploadfmt_t newpixelformat;	//try to convert to this pixel format on export.
};

void Image_GenerateMips(struct pendingtextureinfo *mips, unsigned int flags);
int Image_WritePNG (const char *filename, enum fs_relative fsroot, int compression, void **buffers, int numbuffers, qintptr_t bufferstride, int width, int height, enum uploadfmt fmt, qboolean writemetadata);
qboolean WriteTGA(const char *filename, enum fs_relative fsroot, const qbyte *fte_restrict rgb_buffer, qintptr_t bytestride, int width, int height, enum uploadfmt fmt);

static qboolean ImgTool_ASTCToLDR(uploadfmt_t fmt)
{
	if (fmt >= PTI_ASTC_FIRST && fmt <= PTI_ASTC_LAST)
	{
		if (fmt >= PTI_ASTC_4X4_HDR)
			return (fmt-PTI_ASTC_4X4_HDR)+PTI_ASTC_4X4_LDR;
		if (fmt >= PTI_ASTC_4X4_SRGB)
			return (fmt-PTI_ASTC_4X4_SRGB)+PTI_ASTC_4X4_LDR;
	}
	return fmt;
}
#ifdef _WIN32
#include <fcntl.h>
static void FS_MakeTempName(char *out, size_t outsize, char *prefix, char *suffix)
{
	int fd;
	unsigned int n;
	unsigned int s = rand();
	for (n = 0; n < 0xffffff; n++)
	{
		Q_snprintfz(out, outsize, "/tmp/%s%06x%s", prefix, (n+s)&0xffffff, suffix);
		fd = _open(out, _O_CREAT | _O_EXCL, _S_IREAD | _S_IWRITE);
		if (fd == -1)
			continue;
		close(fd);
		return;
	}
	Sys_Error("FS_MakeTempName failed\n");
}
#else
#include <unistd.h>
static void FS_MakeTempName(char *out, size_t outsize, char *prefix, char *suffix)
{
	snprintf(out, outsize, "/tmp/%sXXXXXX%s", prefix, suffix);
	close(mkstemps(out, strlen(suffix)));	//bsd4.3/posix1-2001
}
#endif
static void ImgTool_ConvertPixelFormat(struct opts_s *args, const char *inname, struct pendingtextureinfo *mips)
{
	struct pendingtextureinfo tmp, *ret;
	size_t m;
	char raw[MAX_OSPATH];
	char comp[MAX_OSPATH];
	char command[MAX_OSPATH*3];

	qbyte *fdata;
	size_t fsize;
	int bb,bw,bh;
	qboolean canktx = false;
	uploadfmt_t targfmt = args->newpixelformat;

	if (targfmt >= PTI_ASTC_FIRST && targfmt <= PTI_ASTC_LAST)
	{
		Q_snprintfz(command, sizeof(command), "astcenc -c");
		canktx = true;
	}
	else if (targfmt == PTI_BC1_RGB)
		Q_snprintfz(command, sizeof(command), "nvcompress -bc1%s", (args->flags&IF_TRYBUMP)?"n":"");
	else if (targfmt == PTI_BC1_RGBA)
		Q_snprintfz(command, sizeof(command), "nvcompress -bc1a");
	else if (targfmt == PTI_BC2_RGBA)
		Q_snprintfz(command, sizeof(command), "nvcompress -bc2");
	else if (targfmt == PTI_BC3_RGBA)
		Q_snprintfz(command, sizeof(command), "nvcompress -bc3%s", (args->flags&IF_TRYBUMP)?"n":"");
	else if (targfmt == PTI_BC4_R8)
		Q_snprintfz(command, sizeof(command), "nvcompress -bc4");
	else if (targfmt == PTI_BC5_RG8)
		Q_snprintfz(command, sizeof(command), "nvcompress -bc5");
	else
		return;
	if (canktx)
		FS_MakeTempName(raw, sizeof(raw), "itr", ".ktx");
	else
		FS_MakeTempName(raw, sizeof(raw), "itr", ".png");
	FS_MakeTempName(comp, sizeof(comp), "itc", ".ktx");

	tmp.type = mips->type;
	tmp.encoding = mips->encoding;
	tmp.extrafree = NULL;
	tmp.mipcount = 1;

	Image_BlockSizeForEncoding(targfmt, &bb, &bw, &bh);
	Q_snprintfz(command+strlen(command), sizeof(command)-strlen(command), " \"%s\" \"%s\"", raw, comp);
	if (targfmt >= PTI_ASTC_FIRST && targfmt <= PTI_ASTC_LAST)
		Q_snprintfz(command+strlen(command), sizeof(command)-strlen(command), " %ix%i -exhaustive", bw, bh);
	if (targfmt >= PTI_ASTC_4X4_SRGB && targfmt <= PTI_ASTC_12X12_SRGB)
		Q_strncatz(command, " -srgb", sizeof(command));
	if (targfmt >= PTI_ASTC_4X4_HDR && targfmt <= PTI_ASTC_12X12_HDR)
		Q_strncatz(command, " -hdr", sizeof(command));
	Q_strncatz(command, ">> /dev/null", sizeof(command));

	Image_BlockSizeForEncoding(mips->encoding, &bb, &bw, &bh);
	for (m = 0; m < mips->mipcount; m++)
	{
		tmp.mip[0] = mips->mip[m];
		tmp.mip[0].needfree = false;

		if (canktx)
		{
			if (!Image_WriteKTXFile(raw, FS_SYSTEM, &tmp))
				break;
		}
		else
		{
#ifdef AVAIL_PNGLIB
			if (!Image_WritePNG(raw, FS_SYSTEM, 0, &mips->mip[m].data, 1, mips->mip[m].width*bb, mips->mip[m].width, mips->mip[m].height, mips->encoding, false))
#endif
				break;
		}

		system(command);

		fdata = FS_LoadMallocFile(comp, &fsize);
		ret = Image_LoadMipsFromMemory(IF_NOMIPMAP, comp, comp, fdata, fsize);
		if (ret &&	ret->mip[0].width == mips->mip[m].width &&
					ret->mip[0].height == mips->mip[m].height &&
					ret->mip[0].depth == mips->mip[m].depth &&
					ImgTool_ASTCToLDR(ret->encoding) == ImgTool_ASTCToLDR(targfmt))
		{
			mips->mip[m] = ret->mip[0];
			continue;
		}
		break;
	}

	mips->encoding = targfmt;
	mips->mipcount = m;

	if (mips->mipcount && targfmt >= PTI_BC1_RGB && targfmt <= PTI_BC7_RGBA_SRGB)
	{	//d3d has some annoying limitations.
		//do not warn for astc files, their block sizes are too weird.
		Image_BlockSizeForEncoding(targfmt, &bb, &bw, &bh);
		if (mips->mip[0].width%bw || mips->mip[0].height%bh)
			Con_Printf("%s: d3d warning: %i*%i is not a multiple of %i*%i\n", inname, mips->mip[0].width, mips->mip[0].height, bw, bh);
	}

	Sys_remove(raw);
	Sys_remove(comp);
}

const char *COM_GetFileExtension (const char *in, const char *term)
{
	const char *dot;

	if (!term)
		term = in + strlen(in);

	for (dot = term-1; dot >= in && *dot != '/' && *dot != '\\'; dot--)
	{
		if (*dot == '.')
			return dot;
	}
	return "";
}
static void ImgTool_Convert(struct opts_s *args, const char *inname, const char *outname)
{
	qbyte *indata;
	size_t fsize;
	struct pendingtextureinfo *in;
	const char *outext = COM_GetFileExtension(outname, NULL);

	indata = FS_LoadMallocFile(inname, &fsize);
	if (indata)
	{
		in = Image_LoadMipsFromMemory(args->flags, inname, inname, indata, fsize);
		if (in)
		{
			printf("%s: %s, %i*%i, %i mips\n", inname, Image_FormatName(in->encoding), in->mip[0].width, in->mip[0].height, in->mipcount);
			/*if (imgtool_convertto >= PTI_BC1_RGB && imgtool_convertto <= PTI_BC5_RG8_SNORM)
				ImgTool_ConvertToBCn(imgtool_convertto, in);
			else*/
			{
				if (!(args->flags & IF_NOMIPMAP) && in->mipcount == 1)
					Image_GenerateMips(in, args->flags);

				if (args->newpixelformat != PTI_INVALID)
					ImgTool_ConvertPixelFormat(args, inname, in);
			}

			if (!in->mipcount)
			{
				printf("%s: unable to convert any mips\n", inname);
				return;
			}

			if (!strcmp(outext, ".ktx"))
				Image_WriteKTXFile(outname, FS_SYSTEM, in);
			else if (!strcmp(outext, ".dds"))
				Image_WriteDDSFile(outname, FS_SYSTEM, in);
			else
			{
				int bb,bw,bh;
				Image_BlockSizeForEncoding(in->encoding, &bb, &bw,&bh);
				if (args->mipnum < in->mipcount)
				{
					if (!strcmp(outext, ".png"))
					{
#ifdef AVAIL_PNGLIB
						if (!Image_WritePNG(outname, FS_SYSTEM, 0, &in->mip[args->mipnum].data, 1, in->mip[args->mipnum].width*bb, in->mip[args->mipnum].width, in->mip[args->mipnum].height, in->encoding, false))
#endif
							Con_Printf("%s(%s): Write failed\n", outname, Image_FormatName(in->encoding));
					}
					else if (!strcmp(outext, ".tga"))
					{
						if (!WriteTGA(outname, FS_SYSTEM, in->mip[args->mipnum].data, in->mip[args->mipnum].width*bb, in->mip[args->mipnum].width, in->mip[args->mipnum].height, in->encoding))
							Con_Printf("%s(%s): Write failed\n", outname, Image_FormatName(in->encoding));
					}
					else
						Con_Printf("%s: Unknown output file format\n", outname);
				}
				else
					Con_Printf("%s: Requested output mip number was out of bounds %i >= %i\n", outname, args->mipnum, in->mipcount);
			}

//			printf("%s: %s, %i*%i, %i mips\n", outname, Image_FormatName(in->encoding), in->mip[0].width, in->mip[0].height, in->mipcount);

//			for (m = 0; m < in->mipcount; m++)
//				printf("\t%u: %i*%i*%i, %u\n", (unsigned)m, in->mip[m].width, in->mip[m].height, in->mip[m].depth, (unsigned)in->mip[m].datasize);
		}
		else
			printf("%s: unsupported format\n", inname);
	}
	else
		printf("%s: unable to read\n", inname);
	fflush(stdout);
}
static void ImgTool_Info(struct opts_s *args, const char *inname)
{
	qbyte *indata;
	size_t fsize;
	size_t m;
	struct pendingtextureinfo *in;
	indata = FS_LoadMallocFile(inname, &fsize);
	if (!indata)
		printf("%s: unable to read\n", inname);
	else
	{
		in = Image_LoadMipsFromMemory(args->flags, inname, inname, indata, fsize);
		if (!in)
			printf("%s: unsupported format\n", inname);
		else
		{
			printf("%s: %s, %i*%i, %i mips\n", inname, Image_FormatName(in->encoding), in->mip[0].width, in->mip[0].height, in->mipcount);
			for (m = 0; m < in->mipcount; m++)
				printf("\t%u: %i*%i*%i, %u\n", (unsigned)m, in->mip[m].width, in->mip[m].height, in->mip[m].depth, (unsigned)in->mip[m].datasize);
		}
	}
	fflush(stdout);
}

#ifdef _WIN32
static void ImgTool_TreeConvert(struct opts_s *args, const char *srcpath, const char *destpath)
{
	Con_Printf("ImgTool_TreeConvert not implemented on windows.\n");
}
#else
#include <dirent.h>
#include <fnmatch.h>
static void ImgTool_TreeConvert(struct opts_s *args, const char *srcpath, const char *destpath)
{
	DIR *dir;
	char file[MAX_OSPATH];
	char dest[MAX_OSPATH];
	struct dirent *ent;

	dir = opendir(srcpath);
	if (!dir)
	{
		Con_Printf("Failed to open dir %s\n", srcpath);
		return;
	}
	for (;;)
	{
		ent = readdir(dir);
		if (!ent)
			break;
		if (*ent->d_name == '.')
			continue;
		else if (ent->d_type == DT_DIR)
		{
			Q_snprintfz(file, sizeof(file), "%s/%s", srcpath, ent->d_name);
			Q_snprintfz(dest, sizeof(dest), "%s/%s", destpath, ent->d_name);
			Con_Printf("Recurse %s -> %s\n", file, dest);
			ImgTool_TreeConvert(args, file, dest);
		}
		else if (ent->d_type == DT_REG)
		{
			const char *ext = COM_GetFileExtension(ent->d_name, NULL);
			if (!strcmp(ext, ".png")||!strcmp(ext, ".bmp")||!strcmp(ext, ".tga")||!strcmp(ext, ".jpg")||!strcmp(ext, ".exr")||!strcmp(ext, ".hdr"))
			{
				struct stat statsrc, statdst;
				Q_snprintfz(file, sizeof(file), "%s/%s", srcpath, ent->d_name);
				Q_snprintfz(dest, sizeof(dest), "%s/%s", destpath, ent->d_name);
				Q_snprintfz(dest+strlen(dest)-strlen(ext), sizeof(dest)-(strlen(dest)-strlen(ext)), ".ktx");

				if (stat(file, &statsrc) < 0)
				{
					Con_Printf("stat(\"%s\") failed...\n", file);
					continue;
				}
				if (stat(dest, &statdst) < 0)
					statdst.st_mtim.tv_sec = INT_MIN; //make it look old
				if (statdst.st_mtim.tv_sec <= statsrc.st_mtim.tv_sec)
				{
					Con_Printf("Image file %s -> %s\n", file, dest);
					FS_CreatePath(dest, FS_SYSTEM);
					ImgTool_Convert(args, file, dest);
				}
				else
					Con_Printf("Unmodified image file %s -> %s\n", file, dest);
			}
		}
	}
	closedir(dir);
	return;
}
#endif

int main(int argc, const char **argv)
{
	enum
	{
		mode_info,
		mode_convert,
		mode_autotree
	} mode = mode_info;
	size_t u, f;
	struct opts_s args;
	for (u = 1; u < countof(sh_config.texfmt); u++)
		sh_config.texfmt[u] = true;

	args.flags = 0;
	args.newpixelformat = PTI_INVALID;
	args.mipnum = 0;

	sh_config.texture2d_maxsize = 1u<<31;
	sh_config.texture3d_maxsize = 1u<<31;
	sh_config.texture2darray_maxlayers = 1u<<31;
	sh_config.texturecube_maxsize = 8192;
	sh_config.texture_non_power_of_two = true;
	sh_config.texture_non_power_of_two_pic = true;
	sh_config.texture_allow_block_padding = true;
	sh_config.npot_rounddown = true;	//shouldn't be relevant
	sh_config.havecubemaps = true;	//I don't think this matters.

	Image_Init();

	if (argc==1)
		goto showhelp;

	for (u = 1; u < argc; u++)
	{
		if (*argv[u] == '-')
		{
			if (!strcmp(argv[u], "-?") || !strcmp(argv[u], "--help"))
			{
showhelp:
				Con_Printf("show info : %s -i *.ktx\n", argv[0]);
				Con_Printf("compress  : %s --astc_6x6_ldr [--nomips] in.png out.ktx [in2.png out2.ktx]\n", argv[0]);
				Con_Printf("compress  : %s --bc3_rgba [--nomips] in.png out.dds\n", argv[0]);
				Con_Printf("convert   : %s -c in.exr out.dds\n", argv[0]);
				Con_Printf("decompress: %s -d [--exportmip 0] [--nomips] in.ktx out.png\n", argv[0]);
//				Con_Printf("auto      : %s --astc_6x6_ldr -r _postfix.png srcdir destdir\n", argv[0]);

				Image_PrintInputFormatVersions();
				Con_Printf("Supported compressed pixelformats are:\n");
				for (f = 0; f < PTI_MAX; f++)
				{
					int bb,bw,bh;
					Image_BlockSizeForEncoding(f, &bb,&bw,&bh);
					if (f >= PTI_ASTC_FIRST && f <= PTI_ASTC_LAST)
						Con_Printf(" --%-15s %.2fbpp (requires astcenc)\n", Image_FormatName(f), 8*(float)bb/(bw*bh));
					else if (f==PTI_BC1_RGB||f==PTI_BC1_RGBA||f==PTI_BC2_RGBA||f==PTI_BC3_RGBA||f==PTI_BC4_R8||f==PTI_BC5_RG8)
						Con_Printf(" --%-15s %.2fbpp (requires nvcompress)\n", Image_FormatName(f), 8*(float)bb/(bw*bh));
				}
				break;
			}
			else if (!strcmp(argv[u], "-c") || !strcmp(argv[u], "--convert"))
				mode = mode_convert;
			else if (!strcmp(argv[u], "-d") || !strcmp(argv[u], "--decompress"))
			{
				for (f = PTI_BC1_RGB; f < PTI_ASTC_LAST; f++)
					sh_config.texfmt[f] = false;
				mode = mode_convert;
			}
			else if (!strcmp(argv[u], "-r") || !strcmp(argv[u], "--auto"))
				mode = mode_autotree;
			else if (!strcmp(argv[u], "-i") || !strcmp(argv[u], "--info"))
				mode = mode_info;
			else if (!strcmp(argv[u], "--nomips")	)
				args.flags |= IF_NOMIPMAP;
			else if (!strcmp(argv[u], "--mips"))
				args.flags &= ~IF_NOMIPMAP;
			else if (!strcmp(argv[u], "--exportmip"))
			{
				char *e = "erk";
				if (u+1 < argc)
					args.mipnum = strtoul(argv[++u], &e, 10);
				if (*e)
				{
					Con_Printf("--exportmip requires trailing numeric argument\n");
					return 1;
				}
			}
			else
			{
				if (argv[u][1] == '-')
				{
					for (f = 0; f < PTI_MAX; f++)
					{
						if (!strcasecmp(argv[u]+2, Image_FormatName(f)))
						{
							args.newpixelformat = f;
							mode = mode_convert;
							break;
						}
					}
					if (f < PTI_MAX)
						continue;
				}
				Con_Printf("Unknown arg %s\n", argv[u]);
				goto showhelp;
			}
		}
		else
		{
			if (mode == mode_info)
				ImgTool_Info(&args, argv[u]);
			else if (mode == mode_convert)
			{
				if (u+1 < argc)
				{
					ImgTool_Convert(&args, argv[u], argv[u+1]);
					u++;
				}
			}
			else if (mode == mode_autotree)
			{
				if (u+1 < argc)
				{
					ImgTool_TreeConvert(&args, argv[u], argv[u+1]);
					u++;
				}
			}
		}
	}
	return 0;
}
