#include "quakedef.h"
#include "shader.h"
#undef stderr
#define stderr stdout

#include <limits.h>
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
qboolean FS_Remove (const char *path, enum fs_relative relativeto)
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

static enum uploadfmt ImgTool_ASTCToLDR(uploadfmt_t fmt)
{
	if (fmt >= PTI_ASTC_FIRST && fmt <= PTI_ASTC_LAST)
	{
		if (fmt >= PTI_ASTC_4X4_HDR)
			return (fmt-PTI_ASTC_4X4_HDR)+PTI_ASTC_4X4_LDR;
		if (fmt >= PTI_ASTC_4X4_SRGB)
			return (fmt-PTI_ASTC_4X4_SRGB)+PTI_ASTC_4X4_LDR;
	}
	if (fmt == PTI_BC1_RGB)
		return PTI_BC1_RGBA;
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
static qboolean ImgTool_ConvertPixelFormat(struct opts_s *args, const char *inname, struct pendingtextureinfo *mips)
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

	//force it to bc1 if bc2 or bc3 with no alpha channel.
	if ((targfmt == PTI_BC2_RGBA || targfmt == PTI_BC3_RGBA) && !Image_FormatHasAlpha(mips->encoding))
		targfmt = PTI_BC1_RGB;

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
	else if (targfmt == PTI_BC6_RGB_SFLOAT || targfmt == PTI_BC6_RGB_UFLOAT)
		Q_snprintfz(command, sizeof(command), "nvcompress -bc6");
	else if (targfmt == PTI_BC7_RGBA)
		Q_snprintfz(command, sizeof(command), "nvcompress -bc7");
	else
		return false;
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
	if (targfmt >= PTI_BC1_RGB && targfmt <= PTI_BC7_RGBA_SRGB && (strstr(inname, "_n.")||strstr(inname, "_norm.")))
		Q_strncatz(command, " -normal", sizeof(command));	//looks like a normalmap... tweak metrics to favour normalised results.
	Q_strncatz(command, ">> /dev/null", sizeof(command));

	Image_BlockSizeForEncoding(mips->encoding, &bb, &bw, &bh);
	for (m = 0; m < mips->mipcount; m++)
	{
		tmp.mip[0] = mips->mip[m];
		tmp.mip[0].needfree = false;
		(void)tmp;

		if (canktx)
		{
#ifdef IMAGEFMT_KTX
			if (!Image_WriteKTXFile(raw, FS_SYSTEM, &tmp))
#endif
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
			Con_Printf("%s: mip0 of %i*%i is not a multiple of %i*%i (d3d warning)\n", inname, mips->mip[0].width, mips->mip[0].height, bw, bh);
	}

	FS_Remove(raw, FS_SYSTEM);
	FS_Remove(comp, FS_SYSTEM);
	return true;
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
	qboolean allowcompressed = false;

	if (!strcmp(outext, ".dds") || !strcmp(outext, ".ktx"))
		allowcompressed = true;

	indata = FS_LoadMallocFile(inname, &fsize);
	if (indata)
	{
		in = Image_LoadMipsFromMemory(args->flags|(allowcompressed?0:IF_NOMIPMAP), inname, inname, indata, fsize);
		if (in)
		{
			printf("%s: %s, %i*%i, %i mips\n", inname, Image_FormatName(in->encoding), in->mip[0].width, in->mip[0].height, in->mipcount);
			/*if (imgtool_convertto >= PTI_BC1_RGB && imgtool_convertto <= PTI_BC5_RG8_SNORM)
				ImgTool_ConvertToBCn(imgtool_convertto, in);
			else*/
			{
				if (!(args->flags & IF_NOMIPMAP) && in->mipcount == 1)
					Image_GenerateMips(in, args->flags);

				if (args->newpixelformat != PTI_INVALID && allowcompressed && ImgTool_ConvertPixelFormat(args, inname, in))
					printf("\t(Converted to %s)\n", Image_FormatName(in->encoding));
			}

			if (!in->mipcount)
			{
				printf("%s: unable to convert any mips\n", inname);
				return;
			}

			if (0)
				;
#ifdef IMAGEFMT_KTX
			else if (!strcmp(outext, ".ktx"))
				Image_WriteKTXFile(outname, FS_SYSTEM, in);
#endif
#ifdef IMAGEFMT_DDS
			else if (!strcmp(outext, ".dds"))
				Image_WriteDDSFile(outname, FS_SYSTEM, in);
#endif
			else
			{
				int bb,bw,bh;
				Image_BlockSizeForEncoding(in->encoding, &bb, &bw,&bh);
				if (args->mipnum < in->mipcount)
				{
					if (0)
						;
#ifdef IMAGEFMT_PNG
					else if (!strcmp(outext, ".png"))
					{
#ifdef AVAIL_PNGLIB
						if (!Image_WritePNG(outname, FS_SYSTEM, 0, &in->mip[args->mipnum].data, 1, in->mip[args->mipnum].width*bb, in->mip[args->mipnum].width, in->mip[args->mipnum].height, in->encoding, false))
#endif
							Con_Printf("%s(%s): Write failed\n", outname, Image_FormatName(in->encoding));
					}
#endif
#ifdef IMAGEFMT_TGA
					else if (!strcmp(outext, ".tga"))
					{
						if (!WriteTGA(outname, FS_SYSTEM, in->mip[args->mipnum].data, in->mip[args->mipnum].width*bb, in->mip[args->mipnum].width, in->mip[args->mipnum].height, in->encoding))
							Con_Printf("%s(%s): Write failed\n", outname, Image_FormatName(in->encoding));
					}
#endif
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

struct filelist_s
{
	const char **exts;
	size_t numfiles;
	struct {
		char *name;
		size_t baselen; //length up to but not including the filename extension.
	} *file;
	size_t maxfiles; //to avoid reallocs
};
static void FileList_Release(struct filelist_s *list)
{
	size_t i;
	for (i = 0; i < list->numfiles; i++)
		free(list->file[i].name);
	free(list->file);
	list->numfiles = 0;
	list->maxfiles = 0;
}
static void FileList_Add(struct filelist_s *list, char *fname)
{
	size_t i;
	size_t baselen;
	const char *ext = COM_GetFileExtension(fname, NULL);
	for (i = 0; ; i++)
	{
		if (!list->exts[i])
			return;	//extension wasn't in the list.
		if (!strcmp(list->exts[i], ext))
			break;	//one of the accepted extensions
	}
	baselen = ext?ext-fname:strlen(fname);
	for (i = 0; i < list->numfiles; i++)
	{
		if (list->file[i].baselen == baselen && !strncasecmp(list->file[i].name, fname, baselen))
		{
			Con_Printf("Ignoring dupe file %s (using %s)\n", fname, list->file[i].name);
			return; //file already listed, but maybe with a different extension
		}
	}
	if (i == list->maxfiles)
	{
		list->maxfiles += 64;
		list->file = realloc(list->file, sizeof(*list->file)*list->maxfiles);
	}
	list->file[i].name = strdup(fname);
	list->file[i].baselen = baselen;
	list->numfiles++;
}
#ifdef _WIN32
static void ImgTool_TreeScan(struct filelist_s *list, const char *basepath, const char *subpath)
{
	Con_Printf("ImgTool_TreeScan not implemented on windows.\n");
}
#else
#include <dirent.h>
#include <fnmatch.h>
static void ImgTool_TreeScan(struct filelist_s *list, const char *basepath, const char *subpath)
{
	DIR *dir;
	char file[MAX_OSPATH];
	struct dirent *ent;

	if (subpath && *subpath)
		Q_snprintfz(file, sizeof(file), "%s/%s", basepath, subpath);
	else
		Q_snprintfz(file, sizeof(file), "%s", basepath);
	dir = opendir(file);
	if (!dir)
	{
		Con_Printf("Failed to open dir %s\n", file);
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
			if (!subpath)
				continue;
			if (*subpath)
				Q_snprintfz(file, sizeof(file), "%s/%s", subpath, ent->d_name);
			else
				Q_snprintfz(file, sizeof(file), "%s", ent->d_name);
			ImgTool_TreeScan(list, basepath, file);
		}
		else if (ent->d_type == DT_REG)
		{
			if (subpath && *subpath)
				Q_snprintfz(file, sizeof(file), "%s/%s", subpath, ent->d_name);
			else
				Q_snprintfz(file, sizeof(file), "%s", ent->d_name);
			FileList_Add(list, file);
		}
	}
	closedir(dir);
}
#endif
static void ImgTool_TreeConvert(struct opts_s *args, const char *srcpath, const char *destpath)
{
	size_t newfiles=0, skippedfiles=0, processedfiles=0;
	char file[MAX_OSPATH];
	char dest[MAX_OSPATH];
	const char *exts[] = {".png", ".bmp", ".tga", ".jpg", ".exr", ".hdr", NULL};
	struct filelist_s list = {exts};
	size_t i, destlen = strlen(destpath)+1;
	ImgTool_TreeScan(&list, srcpath, "");

	if (!list.numfiles)
		Con_Printf("No suitable files found in directory: %s\n", srcpath);

	for (i = 0; i < list.numfiles; i++)
	{
		struct stat statsrc, statdst;
		Q_snprintfz(file, sizeof(file), "%s/%s", srcpath, list.file[i].name);
		Q_snprintfz(dest, sizeof(dest), "%s/%s", destpath, list.file[i].name);
		Q_snprintfz(dest+destlen+list.file[i].baselen, sizeof(dest)-destlen-list.file[i].baselen, ".dds");

		if (stat(file, &statsrc) < 0)
		{
			Con_Printf("stat(\"%s\") failed...\n", file);
			continue;
		}
		if (stat(dest, &statdst) < 0)
		{
			statdst.st_mtim.tv_sec = INT_MIN; //make it look old
			newfiles++;
		}
		if (statdst.st_mtim.tv_sec <= statsrc.st_mtim.tv_sec)
		{
			processedfiles++;
//			Con_Printf("Image file %s -> %s\n", file, dest);
			FS_CreatePath(dest, FS_SYSTEM);
			ImgTool_Convert(args, file, dest);
		}
		else
		{
			skippedfiles++;
//			Con_Printf("Unmodified image file %s -> %s\n", file, dest);
		}
	}
	Con_Printf("found: %u, processed: %u, skipped: %u, new: %u\n", (unsigned int)list.numfiles, (unsigned int)processedfiles, (unsigned int)skippedfiles, (unsigned int)newfiles);
	FileList_Release(&list);
	return;
}






/*
typedef struct
{
   long offset;                 	// Position of the entry in WAD
   long dsize;                  	// Size of the entry in WAD file
   long size;                   	// Size of the entry in memory
   char type;                   	// type of entry
   char cmprs;                  	// Compression. 0 if none.
   short dummy;                 	// Not used
   char name[16];               	// we use only first 8
} wad2entry_t;
typedef struct
{
   char magic[4]; 			//should be WAD2
   long num;				//number of entries
   long offset;				//location of directory
} wad2_t;
static void ImgTool_WadConvert(struct opts_s *args, const char *srcpath, const char *destpath)
{
	char file[MAX_OSPATH];
	const char *exts[] = {".png", ".bmp", ".tga", ".jpg", ".exr", ".hdr", NULL};
	struct filelist_s list = {exts};
	size_t i, u;
	vfsfile_t *f;
	char *inname;
	qbyte *indata;
	size_t fsize;
	wad2_t wad2;
	wad2entry_t *wadentries = NULL, *entry;
	size_t maxentries;
	miptex_t mip;
	ImgTool_TreeScan(&list, srcpath, NULL);

	f = FS_OpenVFS(destpath, "wb", FS_SYSTEM);
	wad2.magic[0] = 'W';
	wad2.magic[1] = 'A';
	wad2.magic[2] = 'D';
	wad2.magic[3] = '3';	//wad3 instead of 2, so we can include a palette for tools to validate against
	VFS_WRITE(f, &wad2, 12);

	//try to decompress everything to a nice friendly palletizable range.
	for (u = 1; u < countof(sh_config.texfmt); u++)
		sh_config.texfmt[u] = (u==PTI_RGBA8)||(u==PTI_RGBX8)||(u==PTI_P8);

	for (i = 0; i < list.numfiles; i++)
	{
		Q_snprintfz(file, sizeof(file), "%s/%s", srcpath, list.file[i].name);
		inname = list.file[i].name;
		if (list.file[i].baselen > 15)
		{
			Con_Printf("Path too long for wad - %s\n", inname);
			continue;
		}
		indata = FS_LoadMallocFile(file, &fsize);
		if (indata)
		{
			struct pendingtextureinfo *in = Image_LoadMipsFromMemory(args->flags|IF_PALETTIZE, inname, file, indata, fsize);
			Image_GenerateMips(in, args->flags);
			if (in)
			{
				if (in->mipcount == 1)
					Image_GenerateMips(in, args->flags);

				if (!in->mipcount)
				{
					printf("%s: unable to load any mips\n", inname);
					continue;
				}
			}
			if (args->mipnum >= in->mipcount)
			{
				printf("%s: not enough mips\n", inname);
				continue;
			}
			if ((in->mip[args->mipnum].width|in->mip[args->mipnum].height) & 15)
				printf("%s(%i): WARNING: not multiple of 16 - %i*%i\n", inname, args->mipnum, in->mip[args->mipnum].width, in->mip[args->mipnum].height);

			if (wad2.num == maxentries)
			{
				maxentries += 64;
				wadentries = realloc(wadentries, sizeof(*wadentries)*maxentries);
			}
			entry = &wadentries[wad2.num++];
			Q_strncpyz(entry->name, inname, 16);
			entry->name[list.file[i].baselen] = 0; //kill any .tga
			entry->type = TYP_MIPTEX;
			entry->cmprs = 0;
			entry->dummy = 0;
			entry->offset = VFS_TELL(f);

			memcpy(mip.name, entry->name, sizeof(mip.name));
			mip.width = in->mip[args->mipnum].width;
			mip.height = in->mip[args->mipnum].height;
			mip.offsets[0] = in->mip[args->mipnum+0].datasize?sizeof(mip):0;
			mip.offsets[1] = in->mip[args->mipnum+1].datasize?mip.offsets[args->mipnum+0]+in->mip[args->mipnum+0].datasize:0;
			mip.offsets[2] = in->mip[args->mipnum+2].datasize?mip.offsets[args->mipnum+1]+in->mip[args->mipnum+1].datasize:0;
			mip.offsets[3] = in->mip[args->mipnum+3].datasize?mip.offsets[args->mipnum+2]+in->mip[args->mipnum+2].datasize:0;

			VFS_WRITE(f, &mip, sizeof(mip));
			VFS_WRITE(f, in->mip[args->mipnum+0].data, in->mip[args->mipnum+0].datasize);
			VFS_WRITE(f, in->mip[args->mipnum+1].data, in->mip[args->mipnum+1].datasize);
			VFS_WRITE(f, in->mip[args->mipnum+2].data, in->mip[args->mipnum+2].datasize);
			VFS_WRITE(f, in->mip[args->mipnum+3].data, in->mip[args->mipnum+3].datasize);
			if (wad2.magic[3] == '3')
			{
				VFS_WRITE(f, "\x00\x01", 2);
				VFS_WRITE(f, host_basepal, 256*3);
			}

			entry->size = entry->dsize = VFS_TELL(f)-entry->offset;
		}
	}
	wad2.offset = VFS_TELL(f);
	VFS_WRITE(f, wadentries, sizeof(*wadentries)*wad2.num);
	VFS_SEEK(f, 0);
	VFS_WRITE(f, &wad2, sizeof(wad2));
	VFS_CLOSE(f);

	FileList_Release(&list);
}
*/

int main(int argc, const char **argv)
{
	enum
	{
		mode_info,
		mode_convert,
		mode_autotree,
		mode_genwad
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
				Con_Printf("compress  : %s --bc3_rgba [--premul] [--nomips] in.png out.dds\n\tConvert pixel format (to bc3 aka dxt5) before writing to output file.\n", argv[0]);
				Con_Printf("convert   : %s --convert in.exr out.dds\n\tConvert to different file format, while trying to preserve pixel formats.\n", argv[0]);
				Con_Printf("decompress: %s --decompress [--exportmip 0] [--nomips] in.ktx out.png\n\tDecompresses any block-compressed pixel data.\n", argv[0]);
//				Con_Printf("gen wad   : %s --genwad [--exportmip 2] srcdir out.wad\n", argv[0]);
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
					else if (f==PTI_BC6_RGB_UFLOAT || f==PTI_BC6_RGB_SFLOAT || f==PTI_BC7_RGBA)
						Con_Printf(" --%-15s %.2fbpp (requires nvcompress 2.1+)\n", Image_FormatName(f), 8*(float)bb/(bw*bh));
				}
				break;
			}
			else if (!strcmp(argv[u], "-c") || !strcmp(argv[u], "--convert"))
				mode = mode_convert;
			else if (!strcmp(argv[u], "-d") || !strcmp(argv[u], "--decompress"))
			{	//remove any (weird) gpu formats
				for (f = PTI_BC1_RGB; f < PTI_ASTC_LAST; f++)
					sh_config.texfmt[f] = false;
				mode = mode_convert;
			}
			else if (!strcmp(argv[u], "-r") || !strcmp(argv[u], "--auto"))
				mode = mode_autotree;
			else if (!strcmp(argv[u], "-i") || !strcmp(argv[u], "--info"))
				mode = mode_info;
			else if (!strcmp(argv[u], "-w") || !strcmp(argv[u], "--genwad"))
				mode = mode_genwad;
			else if (!strcmp(argv[u], "--nomips")	)
				args.flags |= IF_NOMIPMAP;
			else if (!strcmp(argv[u], "--mips"))
				args.flags &= ~IF_NOMIPMAP;
			else if (!strcmp(argv[u], "--premul")	)
				args.flags |= IF_PREMULTIPLYALPHA;
			else if (!strcmp(argv[u], "--nopremul"))
				args.flags &= ~IF_PREMULTIPLYALPHA;
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
			else if (mode == mode_genwad)
			{
				if (u+1 < argc)
				{
					//ImgTool_WadConvert(&args, argv[u], argv[u+1]);
					u++;
				}
			}
		}
	}
	return 0;
}
