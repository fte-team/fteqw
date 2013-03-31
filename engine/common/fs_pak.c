#include "quakedef.h"
#include "fs.h"

//
// in memory
//

typedef struct
{
	fsbucket_t bucket;

	char	name[MAX_QPATH];
	int		filepos, filelen;
} mpackfile_t;

typedef struct pack_s
{
	char	descname[MAX_OSPATH];
	vfsfile_t	*handle;
	unsigned int filepos;	//the pos the subfiles left it at (to optimize calls to vfs_seek)
	int		numfiles;
	mpackfile_t	*files;
	int references;	//seeing as all vfiles from a pak file use the parent's vfsfile, we need to keep the parent open until all subfiles are closed.
} pack_t;

//
// on disk
//
typedef struct
{
	char	name[56];
	int		filepos, filelen;
} dpackfile_t;

typedef struct
{
	int		filepos, filelen;
	char	name[8];
} dwadfile_t;

typedef struct
{
	char	id[4];
	int		dirofs;
	int		dirlen;
} dpackheader_t;

typedef struct
{
	char	id[4];
	int		dirlen;
	int		dirofs;
} dwadheader_t;

#define	MAX_FILES_IN_PACK	2048

void FSPAK_GetDisplayPath(void *handle, char *out, unsigned int outlen)
{
	pack_t *pak = handle;

	if (pak->references != 1)
		Q_snprintfz(out, outlen, "%s (%i)", pak->descname, pak->references-1);
	else
		Q_snprintfz(out, outlen, "%s", pak->descname);
}
void FSPAK_ClosePath(void *handle)
{
	pack_t *pak = handle;

	pak->references--;
	if (pak->references > 0)
		return;	//not free yet


	VFS_CLOSE (pak->handle);
	if (pak->files)
		Z_Free(pak->files);
	Z_Free(pak);
}
void FSPAK_BuildHash(void *handle, int depth)
{
	pack_t *pak = handle;
	int i;

	for (i = 0; i < pak->numfiles; i++)
	{
		FS_AddFileHash(depth, pak->files[i].name, &pak->files[i].bucket, &pak->files[i]);
	}
}
qboolean FSPAK_FLocate(void *handle, flocation_t *loc, const char *filename, void *hashedresult)
{
	mpackfile_t *pf = hashedresult;
	int i, len;
	pack_t		*pak = handle;

// look through all the pak file elements

	if (pf)
	{	//is this a pointer to a file in this pak?
		if (pf < pak->files || pf > pak->files + pak->numfiles)
			return false;	//was found in a different path
	}
	else
	{
		for (i=0 ; i<pak->numfiles ; i++)	//look for the file
		{
			if (!strcmp (pak->files[i].name, filename))
			{
				pf = &pak->files[i];
				break;
			}
		}
	}

	if (pf)
	{
		len = pf->filelen;
		if (loc)
		{
			loc->index = pf - pak->files;
			snprintf(loc->rawname, sizeof(loc->rawname), "%s", pak->descname);
			loc->offset = pf->filepos;
			loc->len = pf->filelen;
		}
		return true;
	}
	return false;
}
int FSPAK_EnumerateFiles (void *handle, const char *match, int (*func)(const char *, int, void *, void *spath), void *parm)
{
	pack_t	*pak = handle;
	int		num;

	for (num = 0; num<(int)pak->numfiles; num++)
	{
		if (wildcmp(match, pak->files[num].name))
		{
			if (!func(pak->files[num].name, pak->files[num].filelen, parm, handle))
				return false;
		}
	}

	return true;
}

int FSPAK_GeneratePureCRC(void *handle, int seed, int crctype)
{
	pack_t *pak = handle;

	int result;
	int *filecrcs;
	int numcrcs=0;
	int i;

	filecrcs = BZ_Malloc((pak->numfiles+1)*sizeof(int));
	filecrcs[numcrcs++] = seed;

	for (i = 0; i < pak->numfiles; i++)
	{
		if (pak->files[i].filelen > 0)
		{
			filecrcs[numcrcs++] = pak->files[i].filepos ^ pak->files[i].filelen ^ QCRC_Block(pak->files[i].name, sizeof(56));
		}
	}

	if (crctype)
		result = Com_BlockChecksum(filecrcs, numcrcs*sizeof(int));
	else
		result = Com_BlockChecksum(filecrcs+1, (numcrcs-1)*sizeof(int));

	BZ_Free(filecrcs);
	return result;
}

/*
=================
COM_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
void *FSPAK_LoadPackFile (vfsfile_t *file, const char *desc)
{
	dpackheader_t	header;
	int				i;
//	int				j;
	mpackfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	vfsfile_t		*packhandle;
	dpackfile_t		info;
	int read;
//	unsigned short		crc;

	packhandle = file;
	if (packhandle == NULL)
		return NULL;

	VFS_READ(packhandle, &header, sizeof(header));
	if (header.id[0] != 'P' || header.id[1] != 'A'
	|| header.id[2] != 'C' || header.id[3] != 'K')
	{
		Con_Printf("%s is not a pak - %c%c%c%c\n", desc, header.id[0], header.id[1], header.id[2], header.id[3]);
		return NULL;
	}
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

//	if (numpackfiles > MAX_FILES_IN_PACK)
//		Sys_Error ("%s has %i files", packfile, numpackfiles);

//	if (numpackfiles != PAK0_COUNT)
//		com_modified = true;	// not the original file

	newfiles = (mpackfile_t*)Z_Malloc (numpackfiles * sizeof(mpackfile_t));

	VFS_SEEK(packhandle, header.dirofs);
//	fread (&info, 1, header.dirlen, packhandle);

// crc the directory to check for modifications
//	crc = QCRC_Block((qbyte *)info, header.dirlen);


//	QCRC_Init (&crc);

	pack = (pack_t*)Z_Malloc (sizeof (pack_t));
// parse the directory
	for (i=0 ; i<numpackfiles ; i++)
	{
		*info.name = '\0';
		read = VFS_READ(packhandle, &info, sizeof(info));
/*
		for (j=0 ; j<sizeof(info) ; j++)
			CRC_ProcessByte(&crc, ((qbyte *)&info)[j]);
*/
		strcpy (newfiles[i].name, info.name);
		newfiles[i].name[MAX_QPATH-1] = 0; //paranoid
		COM_CleanUpPath(newfiles[i].name);	//blooming tanks.
		newfiles[i].filepos = LittleLong(info.filepos);
		newfiles[i].filelen = LittleLong(info.filelen);
	}
/*
	if (crc != PAK0_CRC)
		com_modified = true;
*/
	strcpy (pack->descname, desc);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;
	pack->filepos = 0;
	VFS_SEEK(packhandle, pack->filepos);

	pack->references++;

	Con_TPrintf (TL_ADDEDPACKFILE, desc, numpackfiles);
	return pack;
}


typedef struct {
	vfsfile_t funcs;
	pack_t *parentpak;
	unsigned long startpos;
	unsigned long length;
	unsigned long currentpos;
} vfspack_t;
int VFSPAK_ReadBytes (struct vfsfile_s *vfs, void *buffer, int bytestoread)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	int read;

	if (bytestoread == 0)
		return 0;

	if (vfsp->currentpos - vfsp->startpos + bytestoread > vfsp->length)
		bytestoread = vfsp->length - (vfsp->currentpos - vfsp->startpos);
	if (bytestoread <= 0)
	{
		return -1;
	}

	if (vfsp->parentpak->filepos != vfsp->currentpos)
		VFS_SEEK(vfsp->parentpak->handle, vfsp->currentpos);
	read = VFS_READ(vfsp->parentpak->handle, buffer, bytestoread);
	vfsp->currentpos += read;
	vfsp->parentpak->filepos = vfsp->currentpos;

	return read;
}
int VFSPAK_WriteBytes (struct vfsfile_s *vfs, const void *buffer, int bytestoread)
{	//not supported.
	Sys_Error("Cannot write to pak files\n");
	return 0;
}
qboolean VFSPAK_Seek (struct vfsfile_s *vfs, unsigned long pos)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	if (pos < 0 || pos > vfsp->length)
		return false;
	vfsp->currentpos = pos + vfsp->startpos;

	return true;
}
unsigned long VFSPAK_Tell (struct vfsfile_s *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	return vfsp->currentpos - vfsp->startpos;
}
unsigned long VFSPAK_GetLen (struct vfsfile_s *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	return vfsp->length;
}
void VFSPAK_Close(vfsfile_t *vfs)
{
	vfspack_t *vfsp = (vfspack_t*)vfs;
	FSPAK_ClosePath(vfsp->parentpak);	//tell the parent that we don't need it open any more (reference counts)
	Z_Free(vfsp);	//free ourselves.
}
vfsfile_t *FSPAK_OpenVFS(void *handle, flocation_t *loc, const char *mode)
{
	pack_t *pack = (pack_t*)handle;
	vfspack_t *vfs;

	if (strcmp(mode, "rb"))
		return NULL; //urm, unable to write/append

	vfs = Z_Malloc(sizeof(vfspack_t));

	vfs->parentpak = pack;
	vfs->parentpak->references++;

	vfs->startpos = loc->offset;
	vfs->length = loc->len;
	vfs->currentpos = vfs->startpos;

#ifdef _DEBUG
	Q_strncpyz(vfs->funcs.dbgname, pack->files[loc->index].name, sizeof(vfs->funcs.dbgname));
#endif
	vfs->funcs.Close = VFSPAK_Close;
	vfs->funcs.GetLen = VFSPAK_GetLen;
	vfs->funcs.ReadBytes = VFSPAK_ReadBytes;
	vfs->funcs.Seek = VFSPAK_Seek;
	vfs->funcs.Tell = VFSPAK_Tell;
	vfs->funcs.WriteBytes = VFSPAK_WriteBytes;	//not supported

	return (vfsfile_t *)vfs;
}

void FSPAK_ReadFile(void *handle, flocation_t *loc, char *buffer)
{
	vfsfile_t *f;
	f = FSPAK_OpenVFS(handle, loc, "rb");
	if (!f)	//err...
		return;
	VFS_READ(f, buffer, loc->len);
	VFS_CLOSE(f);
/*
	FILE *f;
	f = fopen(loc->rawname, "rb");
	if (!f)	//err...
		return;
	fseek(f, loc->offset, SEEK_SET);
	fread(buffer, 1, loc->len, f);
	fclose(f);
*/
}

searchpathfuncs_t packfilefuncs = {
	FSPAK_GetDisplayPath,
	FSPAK_ClosePath,
	FSPAK_BuildHash,
	FSPAK_FLocate,
	FSPAK_ReadFile,
	FSPAK_EnumerateFiles,
	FSPAK_LoadPackFile,
	FSPAK_GeneratePureCRC,
	FSPAK_OpenVFS
};



#ifdef DOOMWADS
void *FSPAK_LoadDoomWadFile (vfsfile_t *packhandle, const char *desc)
{
	dwadheader_t	header;
	int				i;
	mpackfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	dwadfile_t		info;

	int section=0;
	char sectionname[MAX_QPATH];
	char filename[52];
	char neatwadname[52];

	if (packhandle == NULL)
		return NULL;

	VFS_READ(packhandle, &header, sizeof(header));
	if (header.id[1] != 'W'	|| header.id[2] != 'A' || header.id[3] != 'D')
		return NULL;	//not a doom wad

	//doom wads come in two sorts. iwads and pwads.
	//iwads are the master wads, pwads are meant to replace parts of the master wad.
	//this is awkward, of course.
	//we ignore the i/p bit for the most part, but with maps, pwads are given a prefixed name.
	if (header.id[0] == 'I')
		*neatwadname = '\0';
	else if (header.id[0] == 'P')
	{
		COM_FileBase(desc, neatwadname, sizeof(neatwadname));
		strcat(neatwadname, "#");
	}
	else
		return NULL;

	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen;
	newfiles = (mpackfile_t*)Z_Malloc (numpackfiles * sizeof(mpackfile_t));
	VFS_SEEK(packhandle, header.dirofs);

	//doom wads are awkward.
	//they have no directory structure, except for start/end 'files'.
	//they follow along the lines of lumps after the parent name.
	//a map is the name of that map, and then a squence of the lumps that form that map (found by next-with-that-name).
	//this is a problem for a real virtual filesystem, so we add a hack to recognise special names and expand them specially.
	for (i=0 ; i<numpackfiles ; i++)
	{
		VFS_READ (packhandle, &info, sizeof(info));

		strcpy (filename, info.name);
		filename[8] = '\0';
		Q_strlwr(filename);

		newfiles[i].filepos = LittleLong(info.filepos);
		newfiles[i].filelen = LittleLong(info.filelen);

		switch(section)	//be prepared to remap filenames.
		{
newsection:
		case 0:
			if (info.filelen == 0)
			{	//marker for something...

				if (!strcmp(filename, "s_start"))
				{
					section = 2;
					sprintf (newfiles[i].name, "sprites/%s", filename);	//the model loader has a hack to recognise .dsp
					break;
				}
				if (!strcmp(filename, "p_start"))
				{
					section = 3;
					sprintf (newfiles[i].name, "patches/%s", filename); //the map loader will find these.
					break;
				}
				if (!strcmp(filename, "f_start"))
				{
					section = 4;
					sprintf (newfiles[i].name, "flats/%s", filename);	//the map loader will find these
					break;
				}
				if ((filename[0] == 'e' && filename[2] == 'm') || !strncmp(filename, "map", 3))
				{	//this is the start of a beutiful new map
					section = 1;
					strcpy(sectionname, filename);
					sprintf (newfiles[i].name, "maps/%s%s.bsp", neatwadname, filename);	//generate fake bsps to allow the server to find them
					newfiles[i].filepos = 0;
					newfiles[i].filelen = 4;
					break;
				}
				if (!strncmp(filename, "gl_", 3) && ((filename[4] == 'e' && filename[5] == 'm') || !strncmp(filename+3, "map", 3)))
				{	//this is the start of a beutiful new map
					section = 5;
					strcpy(sectionname, filename+3);
					break;
				}
			}

			sprintf (newfiles[i].name, "wad/%s", filename);	//but there are many files that we don't recognise/know about. archive them off to keep the vfs moderatly clean.
			break;
		case 1:	//map section
			if (strcmp(filename, "things") &&
				strcmp(filename, "linedefs") &&
				strcmp(filename, "sidedefs") &&
				strcmp(filename, "vertexes") &&
				strcmp(filename, "segs") &&
				strcmp(filename, "ssectors") &&
				strcmp(filename, "nodes") &&
				strcmp(filename, "sectors") &&
				strcmp(filename, "reject") &&
				strcmp(filename, "blockmap"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "maps/%s%s.%s", neatwadname, sectionname, filename);
			break;
		case 5:	//glbsp output section
			if (strcmp(filename, "gl_vert") &&
				strcmp(filename, "gl_segs") &&
				strcmp(filename, "gl_ssect") &&
				strcmp(filename, "gl_pvs") &&
				strcmp(filename, "gl_nodes"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "maps/%s%s.%s", neatwadname, sectionname, filename);
			break;
		case 2:	//sprite section
			if (!strcmp(filename, "s_end"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "sprites/%s", filename);
			break;
		case 3:	//patches section
			if (!strcmp(filename, "p_end"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "patches/%s", filename);
			break;
		case 4:	//flats section
			if (!strcmp(filename, "f_end"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "flats/%s", filename);
			break;
		}
	}

	pack = (pack_t*)Z_Malloc (sizeof (pack_t));
	strcpy (pack->descname, desc);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;
	pack->filepos = 0;
	VFS_SEEK(packhandle, pack->filepos);

	pack->references++;

	Con_TPrintf (TL_ADDEDPACKFILE, desc, numpackfiles);
	return pack;
}
searchpathfuncs_t doomwadfilefuncs = {
	FSPAK_GetDisplayPath,
	FSPAK_ClosePath,
	FSPAK_BuildHash,
	FSPAK_FLocate,
	FSPAK_ReadFile,
	FSPAK_EnumerateFiles,
	FSPAK_LoadDoomWadFile,
	NULL,
	FSPAK_OpenVFS
};
#endif
