/* unzip.c -- IO on .zip files using zlib 
   Version 0.15 beta, Mar 19th, 1998,

   Read unzip.h for more info
*/

//Spike: ported to AMD64
//Spike: FTE specific tweeks are in here starting with FTE's VFS filesystem stuff

//# pragma comment (lib, "zip/zlib.lib") 


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zlib.h"
#include "unzip.h"

#ifdef STDC
#  include <stddef.h>
#  include <string.h>
#  include <stdlib.h>
#endif


#ifndef local
#  define local static
#endif
/* compile with -Dlocal if your debugger can't find static symbols */



#if !defined(unix) && !defined(CASESENSITIVITYDEFAULT_YES) && \
                      !defined(CASESENSITIVITYDEFAULT_NO)
#define CASESENSITIVITYDEFAULT_NO
#endif


#ifndef UNZ_BUFSIZE
#define UNZ_BUFSIZE (16384)
#endif

#ifndef UNZ_MAXFILENAMEINZIP
#define UNZ_MAXFILENAMEINZIP (256)
#endif

#ifndef ALLOC
# define ALLOC(size) (malloc(size))
#endif
#ifndef TRYFREE
# define TRYFREE(p) {if (p) free(p);}
#endif

#define SIZECENTRALDIRITEM (0x2e)
#define SIZEZIPLOCALHEADER (0x1e)


/* I've found an old Unix (a SunOS 4.1.3_U1) without all SEEK_* defined.... */

#ifndef SEEK_CUR
#define SEEK_CUR    1
#endif

#ifndef SEEK_END
#define SEEK_END    2
#endif

#ifndef SEEK_SET
#define SEEK_SET    0
#endif

/* unz_file_info_interntal contain internal info about a file in zipfile*/
typedef struct unz_file_info_internal_s {
	unsigned long offset_curfile;/* relative offset of local header 4 bytes */
} unz_file_info_internal;


/* file_in_zip_read_info_s contain internal information about a file in zipfile,
    when reading and decompress it */
typedef struct {
	char  *read_buffer;         /* internal buffer for compressed data */
	z_stream stream;            /* zLib stream structure for inflate */

	unsigned long pos_in_zipfile;       /* position in byte on the zipfile, for fseek*/
	unsigned long stream_initialised;   /* flag set if stream structure is initialised*/

	unsigned long offset_local_extrafield;/* offset of the local extra field */
	unsigned int  size_local_extrafield;/* size of the local extra field */
	unsigned long pos_local_extrafield;   /* position in the local extra field in read*/

	unsigned long crc32;                /* crc32 of all data uncompressed */
	unsigned long crc32_wait;           /* crc32 we must obtain after decompress all */
	unsigned long rest_read_compressed; /* number of byte to be decompressed */
	unsigned long rest_read_uncompressed;/*number of byte to be obtained after decomp*/
	vfsfile_t* file;                 /* io structore of the zipfile */
	unsigned long compression_method;   /* compression method (0==store) */
	unsigned long byte_before_the_zipfile;/* byte before the zipfile, (>0 for sfx)*/
} file_in_zip_read_info_s;


/* unz_s contain internal information about the zipfile
*/
typedef struct {
	vfsfile_t* file;                 /* io structore of the zipfile */
	unz_global_info gi;       /* public global information */
	unsigned long byte_before_the_zipfile;/* byte before the zipfile, (>0 for sfx)*/
	unsigned long num_file;             /* number of the current file in the zipfile*/
	unsigned long pos_in_central_dir;   /* pos of the current file in the central dir*/
	unsigned long current_file_ok;      /* flag about the usability of the current file*/
	unsigned long central_pos;          /* position of the beginning of the central dir*/

	unsigned long size_central_dir;     /* size of the central directory  */
	unsigned long offset_central_dir;   /* offset of start of central directory with
								   respect to the starting disk number */

	unz_file_info cur_file_info; /* public info about the current file in zip*/
	unz_file_info_internal cur_file_info_internal; /* private info about it*/
    file_in_zip_read_info_s* pfile_in_zip_read; /* structure about the current
	                                    file if we are decompressing it */
} unz_s;


/* ===========================================================================
     Read a byte from a gz_stream; update next_in and avail_in. Return EOF
   for end of file.
   IN assertion: the stream s has been sucessfully opened for reading.
*/

local int unzlocal_getShortSane(vfsfile_t *fin, unsigned short *pi)
{
	unsigned short c;
	int err = VFS_READ(fin, &c, 2);
	if (err==2)
	{
		*pi = LittleShort(c);
		return UNZ_OK;
	}
	else
	{
		*pi = 0;
		if (VFS_TELL(fin) != VFS_GETLEN(fin))
			return UNZ_ERRNO;
		else
			return UNZ_EOF;
	}
}

local int unzlocal_getShort(vfsfile_t *fin,unsigned long *pi)
{
	unsigned short c;
	int err = VFS_READ(fin, &c, 2);
	if (err==2)
	{
		*pi = LittleShort(c);
		return UNZ_OK;
	}
	else
	{
		*pi = 0;
		if (VFS_TELL(fin) != VFS_GETLEN(fin)) return UNZ_ERRNO;
		else return UNZ_EOF;
	}
}

local int unzlocal_getLong(vfsfile_t *fin,unsigned long *pi)
{
	unsigned int c;
	int err = VFS_READ(fin, &c, 4);
	if (err==4)
	{
		*pi = LittleLong(c);
		return UNZ_OK;
	}
	else
	{
		*pi = 0;
		if (VFS_TELL(fin) != VFS_GETLEN(fin))
			return UNZ_ERRNO;
		else
			return UNZ_EOF;
	}
}


#define BUFREADCOMMENT (0x400)

/*
  Locate the Central directory of a zipfile (at the end, just before
    the global comment)
*/
local unsigned long unzlocal_SearchCentralDir(vfsfile_t *fin) {
	unsigned char* buf;
	unsigned long uSizeFile;
	unsigned long uBackRead;
	unsigned long uMaxBack=0xffff; /* maximum size of global comment */
	unsigned long uPosFound=0;
	
	uSizeFile = VFS_GETLEN(fin);
	
	if (uMaxBack>uSizeFile) uMaxBack = uSizeFile;

	buf = (unsigned char*)ALLOC(BUFREADCOMMENT+4);
	if (!buf) return 0;

	uBackRead = 4;
	while (uBackRead<uMaxBack) {
		unsigned long uReadSize,uReadPos ;
		int i;
		if (uBackRead+BUFREADCOMMENT>uMaxBack) uBackRead = uMaxBack;
		else uBackRead+=BUFREADCOMMENT;
		uReadPos = uSizeFile-uBackRead ;
		
		uReadSize = ((BUFREADCOMMENT+4) < (uSizeFile-uReadPos)) ? 
                     (BUFREADCOMMENT+4) : (uSizeFile-uReadPos);

		if (!VFS_SEEK(fin, uReadPos))
			break;

		if (VFS_READ(fin,buf,uReadSize)!=uReadSize)
			break;

		for (i=(int)uReadSize-3; (i--)>0;)
			if (((*(buf+i))==0x50) && ((*(buf+i+1))==0x4b) && 
				((*(buf+i+2))==0x05) && ((*(buf+i+3))==0x06)) {
				uPosFound = uReadPos+i;
				break;
			}

		if (uPosFound!=0)
			break;
	}
	TRYFREE(buf);
	return uPosFound;
}

/*
  Open a Zip file. path contain the full pathname (by example,
     on a Windows NT computer "c:\\test\\zlib109.zip" or on an Unix computer
	 "zlib/zlib109.zip".
	 If the zipfile cannot be opened (file don't exist or in not valid), the
	   return value is NULL.
     Else, the return value is a unzFile Handle, usable with other function
	   of this unzip package.
*/
extern unzFile ZEXPORT unzOpen (vfsfile_t *fin) {
	unz_s us = {0};
	unz_s *s;
	unsigned long central_pos,uL;

	unsigned long number_disk = 0;          /* number of the current dist, used for 
								   spaning ZIP, unsupported, always 0*/
	unsigned long number_disk_with_CD = 0;  /* number the the disk with central dir, used
								   for spaning ZIP, unsupported, always 0*/
	unsigned long number_entry_CD = 0;      /* total number of entries in
	                               the central dir 
	                               (same than number_entry on nospan) */

	int err=UNZ_OK;


	if (!fin) return NULL;

	central_pos = unzlocal_SearchCentralDir(fin);
	if (!central_pos) err=UNZ_ERRNO;

	if (!VFS_SEEK(fin,central_pos)) err=UNZ_ERRNO;

	/* the signature, already checked */
	if (unzlocal_getLong(fin,&uL)!=UNZ_OK) err=UNZ_ERRNO;

	/* number of this disk */
	if (unzlocal_getShort(fin,&number_disk)!=UNZ_OK) err=UNZ_ERRNO;

	/* number of the disk with the start of the central directory */
	if (unzlocal_getShort(fin,&number_disk_with_CD)!=UNZ_OK) err=UNZ_ERRNO;

	/* total number of entries in the central dir on this disk */
	if (unzlocal_getShort(fin,&us.gi.number_entry)!=UNZ_OK) err=UNZ_ERRNO;

	/* total number of entries in the central dir */
	if (unzlocal_getShort(fin,&number_entry_CD)!=UNZ_OK) err=UNZ_ERRNO;

	if ((number_entry_CD!=us.gi.number_entry) || (number_disk_with_CD!=0) || (number_disk!=0)) err=UNZ_BADZIPFILE;

	/* size of the central directory */
	if (unzlocal_getLong(fin,&us.size_central_dir)!=UNZ_OK) err=UNZ_ERRNO;

	/* offset of start of central directory with respect to the 
	      starting disk number */
	if (unzlocal_getLong(fin,&us.offset_central_dir)!=UNZ_OK) err=UNZ_ERRNO;

	/* zipfile comment length */
	if (unzlocal_getShort(fin,&us.gi.size_comment)!=UNZ_OK) err=UNZ_ERRNO;

	if ((central_pos<us.offset_central_dir+us.size_central_dir) && (err==UNZ_OK)) err=UNZ_BADZIPFILE;

	if (err!=UNZ_OK) {
		return NULL;
	}

	us.file=fin;
	us.byte_before_the_zipfile = central_pos - (us.offset_central_dir+us.size_central_dir);
	us.central_pos = central_pos;
    us.pfile_in_zip_read = NULL;
	

	s=(unz_s*)ALLOC(sizeof(unz_s));
	*s=us;
	unzGoToFirstFile((unzFile)s);	
	return (unzFile)s;	
}


/*
  Close a ZipFile opened with unzipOpen.
  If there is files inside the .Zip opened with unzipOpenCurrentFile (see later),
    these files MUST be closed with unzipCloseCurrentFile before call unzipClose.
  return UNZ_OK if there is no problem. */
extern int ZEXPORT unzClose (unzFile file) {
	unz_s* s;
	if (!file) return UNZ_PARAMERROR;
	s=(unz_s*)file;

    if (s->pfile_in_zip_read) unzCloseCurrentFile(file);

	VFS_CLOSE(s->file);
	TRYFREE(s);
	return UNZ_OK;
}


/*
  Write info about the ZipFile in the *pglobal_info structure.
  No preparation of the structure is needed
  return UNZ_OK if there is no problem. */
extern int ZEXPORT unzGetGlobalInfo (unzFile file, unz_global_info *pglobal_info) {
	unz_s* s;
	if (!file) return UNZ_PARAMERROR;
	s=(unz_s*)file;
	*pglobal_info=s->gi;
	return UNZ_OK;
}


/*
  Get Info about the current file in the zipfile, with internal only info
*/
local int unzlocal_GetCurrentFileInfoInternal (unzFile file,
                                                  unz_file_info *pfile_info,
                                                  unz_file_info_internal 
                                                  *pfile_info_internal,
                                                  char *szFileName,
						  unsigned long fileNameBufferSize,
                                                  void *extraField,
						  unsigned long extraFieldBufferSize,
                                                  char *szComment,
						  unsigned long commentBufferSize);

local int unzlocal_GetCurrentFileInfoInternal (unzFile file,
                                              unz_file_info *pfile_info,
                                              unz_file_info_internal *pfile_info_internal,
					      char *szFileName, unsigned long fileNameBufferSize,
					      void *extraField, unsigned long extraFieldBufferSize,
					      char *szComment,  unsigned long commentBufferSize) {
	unz_s* s;
	unz_file_info file_info;
	unz_file_info_internal file_info_internal = {0};
	int err=UNZ_OK;
	unsigned long uMagic = 0;
	long lSeek=0;

	if (!file) return UNZ_PARAMERROR;
	s=(unz_s*)file;
	if (!VFS_SEEK(s->file,s->pos_in_central_dir+s->byte_before_the_zipfile)) err=UNZ_ERRNO;


	/* we check the magic */
	if (err==UNZ_OK) 
	{
		if (unzlocal_getLong(s->file,&uMagic) != UNZ_OK) err=UNZ_ERRNO;
		else if (uMagic!=0x02014b50) err=UNZ_BADZIPFILE;
	}

	unzlocal_getShortSane(s->file, &file_info.version);
	unzlocal_getShortSane(s->file, &file_info.version_needed);
	unzlocal_getShortSane(s->file, &file_info.flag);
	unzlocal_getShortSane(s->file, &file_info.compression_method);
	unzlocal_getLong(s->file, &file_info.dosDate);
	unzlocal_getLong(s->file, &file_info.crc);
	unzlocal_getLong(s->file, &file_info.compressed_size);
	unzlocal_getLong(s->file, &file_info.uncompressed_size);
	unzlocal_getShortSane(s->file, &file_info.size_filename);
	unzlocal_getShortSane(s->file, &file_info.size_file_extra);
	unzlocal_getShortSane(s->file, &file_info.size_file_comment);
	unzlocal_getShortSane(s->file, &file_info.disk_num_start);
	unzlocal_getShortSane(s->file, &file_info.internal_fa);
	unzlocal_getLong(s->file, &file_info.external_fa);
/*
	VFS_READ(s->file, &file_info, sizeof(file_info)-2*4); // 2*4 is the size of 2 my vars
	file_info.version = LittleShort(file_info.version);
	file_info.version_needed = LittleShort(file_info.version_needed);
	file_info.flag = LittleShort(file_info.flag);
	file_info.compression_method = LittleShort(file_info.compression_method);
	file_info.dosDate = LittleLong(file_info.dosDate);
	file_info.crc = LittleLong(file_info.crc);
	file_info.compressed_size = LittleLong(file_info.compressed_size);
	file_info.uncompressed_size = LittleLong(file_info.uncompressed_size);
	file_info.size_filename = LittleShort(file_info.size_filename);
	file_info.size_file_extra = LittleShort(file_info.size_file_extra);
	file_info.size_file_comment = LittleShort(file_info.size_file_comment);
	file_info.disk_num_start = LittleShort(file_info.disk_num_start);
	file_info.internal_fa = LittleShort(file_info.internal_fa);
	file_info.external_fa = LittleLong(file_info.external_fa);
*/
	if (unzlocal_getLong(s->file,&file_info_internal.offset_curfile) != UNZ_OK) err=UNZ_ERRNO;

	file_info.offset = file_info_internal.offset_curfile;
	file_info.c_offset = s->pos_in_central_dir;

	lSeek+=file_info.size_filename;
	if ((err==UNZ_OK) && (szFileName))
	{
		unsigned long uSizeRead ;
		if (file_info.size_filename<fileNameBufferSize)
		{
			*(szFileName+file_info.size_filename)='\0';
			uSizeRead = file_info.size_filename;
		}
		else uSizeRead = fileNameBufferSize;

		if ((file_info.size_filename>0) && (fileNameBufferSize>0))
			if (VFS_READ(s->file, szFileName,(unsigned int)uSizeRead)!=uSizeRead) err=UNZ_ERRNO;
		lSeek -= uSizeRead;
	}

	
	if ((err==UNZ_OK) && (extraField))
	{
		unsigned long uSizeRead ;
		if (file_info.size_file_extra<extraFieldBufferSize) uSizeRead = file_info.size_file_extra;
		else uSizeRead = extraFieldBufferSize;

		if (lSeek!=0) 
		{
			if (VFS_SEEK(s->file, VFS_TELL(s->file)+lSeek)) lSeek=0;
			else err=UNZ_ERRNO;
		}
		if ((file_info.size_file_extra>0) && (extraFieldBufferSize>0))
			if (VFS_READ(s->file, extraField,(unsigned int)uSizeRead)!=uSizeRead) err=UNZ_ERRNO;
		lSeek += file_info.size_file_extra - uSizeRead;
	}
	else lSeek+=file_info.size_file_extra; 

	
	if ((err==UNZ_OK) && (szComment))
	{
		unsigned long uSizeRead ;
		if (file_info.size_file_comment<commentBufferSize) {
			*(szComment+file_info.size_file_comment)='\0';
			uSizeRead = file_info.size_file_comment;
		} else uSizeRead = commentBufferSize;

		if (lSeek!=0)
		{
			if (VFS_SEEK(s->file, VFS_TELL(s->file)+lSeek)) lSeek=0;
			else err=UNZ_ERRNO;
		}
		if ((file_info.size_file_comment>0) && (commentBufferSize>0))
			if (VFS_READ(s->file, szComment,(unsigned int)uSizeRead)!=uSizeRead) err=UNZ_ERRNO;
		lSeek+=file_info.size_file_comment - uSizeRead;
	} else lSeek+=file_info.size_file_comment;

	if ((err==UNZ_OK) && (pfile_info)) *pfile_info=file_info;

	if ((err==UNZ_OK) && (pfile_info_internal)) *pfile_info_internal=file_info_internal;

	return err;
}



/*
  Write info about the ZipFile in the *pglobal_info structure.
  No preparation of the structure is needed
  return UNZ_OK if there is no problem.
*/
extern int ZEXPORT unzGetCurrentFileInfo (unzFile file,
											unz_file_info *pfile_info,
										char *szFileName, unsigned long fileNameBufferSize,
										void *extraField, unsigned long extraFieldBufferSize,
										char *szComment,  unsigned long commentBufferSize) {
	return unzlocal_GetCurrentFileInfoInternal(file,pfile_info,NULL,
												szFileName,fileNameBufferSize,
												extraField,extraFieldBufferSize,
												szComment,commentBufferSize);
}

/*
  Set the current file of the zipfile to the first file.
  return UNZ_OK if there is no problem
*/
extern int ZEXPORT unzGoToFirstFile (unzFile file) {
	int err=UNZ_OK;
	unz_s* s;
	if (!file) return UNZ_PARAMERROR;
	s=(unz_s*)file;
	s->pos_in_central_dir=s->offset_central_dir;
	s->num_file=0;
	err=unzlocal_GetCurrentFileInfoInternal(file,&s->cur_file_info,
											 &s->cur_file_info_internal,
											 NULL,0,NULL,0,NULL,0);
	s->current_file_ok = (err == UNZ_OK);
	return err;
}


/*
  Set the current file of the zipfile to the next file.
  return UNZ_OK if there is no problem
  return UNZ_END_OF_LIST_OF_FILE if the actual file was the latest.
*/
extern int ZEXPORT unzGoToNextFile (unzFile file) {
	unz_s* s;	
	int err;

	if (!file) return UNZ_PARAMERROR;
	s=(unz_s*)file;
	if (!s->current_file_ok) return UNZ_END_OF_LIST_OF_FILE;
	if (s->num_file+1==s->gi.number_entry) return UNZ_END_OF_LIST_OF_FILE;

	s->pos_in_central_dir += SIZECENTRALDIRITEM + s->cur_file_info.size_filename +
			s->cur_file_info.size_file_extra + s->cur_file_info.size_file_comment ;
	s->num_file++;
	err = unzlocal_GetCurrentFileInfoInternal(file,&s->cur_file_info,
											   &s->cur_file_info_internal,
											   NULL,0,NULL,0,NULL,0);
	s->current_file_ok = (err == UNZ_OK);
	return err;
}


extern int ZEXPORT unzLocateFileMy (unzFile file, unsigned long num, unsigned long pos) {
	unz_s* s;	
	s = (unz_s *)file;
	s->pos_in_central_dir = pos;
	s->num_file = num;
	unzlocal_GetCurrentFileInfoInternal(file,&s->cur_file_info,&s->cur_file_info_internal,NULL,0,NULL,0,NULL,0);
	return 1;
}


/*
  Read the local header of the current zipfile
  Check the coherency of the local header and info in the end of central
        directory about this file
  store in *piSizeVar the size of extra info in local header
        (filename and size of extra field data)
*/
local int unzlocal_CheckCurrentFileCoherencyHeader (unz_s *s, unsigned int *piSizeVar,
		unsigned long *poffset_local_extrafield,
		unsigned int *psize_local_extrafield) {
										unsigned long uMagic = 0,uData = 0,uFlags = 0;
										unsigned long size_filename = 0;
										unsigned long size_extra_field = 0;
	int err=UNZ_OK;

	*piSizeVar = 0;
	*poffset_local_extrafield = 0;
	*psize_local_extrafield = 0;

	if (!VFS_SEEK(s->file,s->cur_file_info_internal.offset_curfile + s->byte_before_the_zipfile)) return UNZ_ERRNO;


	if (err==UNZ_OK)
	{
		if (unzlocal_getLong(s->file,&uMagic) != UNZ_OK) err=UNZ_ERRNO;
		else if (uMagic!=0x04034b50) err=UNZ_BADZIPFILE;
	}

	if (unzlocal_getShort(s->file,&uData) != UNZ_OK) err=UNZ_ERRNO;

	if (unzlocal_getShort(s->file,&uFlags) != UNZ_OK)
		err=UNZ_ERRNO;

	if (unzlocal_getShort(s->file,&uData) != UNZ_OK) err=UNZ_ERRNO;
	else if ((err==UNZ_OK) && (uData!=s->cur_file_info.compression_method)) err=UNZ_BADZIPFILE;

    if ((err==UNZ_OK) && (s->cur_file_info.compression_method!=0) && (s->cur_file_info.compression_method!=Z_DEFLATED)) err=UNZ_BADZIPFILE;

	/* date/time */
	if (unzlocal_getLong(s->file,&uData) != UNZ_OK) err=UNZ_ERRNO;

	/* crc */
	if (unzlocal_getLong(s->file,&uData) != UNZ_OK) err=UNZ_ERRNO;
	else if ((err==UNZ_OK) && (uData!=s->cur_file_info.crc) && ((uFlags & 8)==0)) err=UNZ_BADZIPFILE;

	/* size compr */
	if (unzlocal_getLong(s->file,&uData) != UNZ_OK) err=UNZ_ERRNO;
	else if ((err==UNZ_OK) && (uData!=s->cur_file_info.compressed_size) && ((uFlags & 8)==0)) err=UNZ_BADZIPFILE;

	 /* size uncompr */
	if (unzlocal_getLong(s->file,&uData) != UNZ_OK) err=UNZ_ERRNO;
	else if ((err==UNZ_OK) && (uData!=s->cur_file_info.uncompressed_size) && ((uFlags & 8)==0)) err=UNZ_BADZIPFILE;


	if (unzlocal_getShort(s->file,&size_filename) != UNZ_OK) err=UNZ_ERRNO;
	else if ((err==UNZ_OK) && (size_filename!=s->cur_file_info.size_filename)) err=UNZ_BADZIPFILE;

	*piSizeVar += (unsigned int)size_filename;

	if (unzlocal_getShort(s->file,&size_extra_field) != UNZ_OK)	err=UNZ_ERRNO;
	*poffset_local_extrafield= s->cur_file_info_internal.offset_curfile + SIZEZIPLOCALHEADER + size_filename;
	*psize_local_extrafield = (unsigned int)size_extra_field;

	*piSizeVar += (unsigned int)size_extra_field;

	return err;
}
												
/*
  Open for reading data the current file in the zipfile.
  If there is no error and the file is opened, the return value is UNZ_OK.
*/
extern int ZEXPORT unzOpenCurrentFile (unzFile file) {
	int err=UNZ_OK;
	int Store;
	unsigned int iSizeVar;
	unz_s* s;
	file_in_zip_read_info_s* pfile_in_zip_read_info;
	unsigned long offset_local_extrafield;  /* offset of the local extra field */
	unsigned int  size_local_extrafield;    /* size of the local extra field */

	if (!file) return UNZ_PARAMERROR;
	s=(unz_s*)file;
	if (!s->current_file_ok) return UNZ_PARAMERROR;

    if (s->pfile_in_zip_read) unzCloseCurrentFile(file);

	if (unzlocal_CheckCurrentFileCoherencyHeader(s,&iSizeVar, &offset_local_extrafield,&size_local_extrafield)!=UNZ_OK) return UNZ_BADZIPFILE;

	pfile_in_zip_read_info = (file_in_zip_read_info_s*)ALLOC(sizeof(file_in_zip_read_info_s));
	if (!pfile_in_zip_read_info) return UNZ_INTERNALERROR;

	pfile_in_zip_read_info->read_buffer=(char*)ALLOC(UNZ_BUFSIZE);
	pfile_in_zip_read_info->offset_local_extrafield = offset_local_extrafield;
	pfile_in_zip_read_info->size_local_extrafield = size_local_extrafield;
	pfile_in_zip_read_info->pos_local_extrafield=0;

	if (!pfile_in_zip_read_info->read_buffer) {
		TRYFREE(pfile_in_zip_read_info);
		return UNZ_INTERNALERROR;
	}

	pfile_in_zip_read_info->stream_initialised=0;
	
	if ((s->cur_file_info.compression_method!=0) && (s->cur_file_info.compression_method!=Z_DEFLATED)) err=UNZ_BADZIPFILE;
	Store = s->cur_file_info.compression_method==0;

	pfile_in_zip_read_info->crc32_wait=s->cur_file_info.crc;
	pfile_in_zip_read_info->crc32=0;
	pfile_in_zip_read_info->compression_method = s->cur_file_info.compression_method;
	pfile_in_zip_read_info->file=s->file;
	pfile_in_zip_read_info->byte_before_the_zipfile=s->byte_before_the_zipfile;

    pfile_in_zip_read_info->stream.total_out = 0;

	if (!Store)	{
	  pfile_in_zip_read_info->stream.zalloc = (alloc_func)0;
	  pfile_in_zip_read_info->stream.zfree = (free_func)0;
	  pfile_in_zip_read_info->stream.opaque = (voidpf)0; 
      
	  err=qinflateInit2(&pfile_in_zip_read_info->stream, -MAX_WBITS);
	  if (err == Z_OK) pfile_in_zip_read_info->stream_initialised=1;
        /* windowBits is passed < 0 to tell that there is no zlib header.
         * Note that in this case inflate *requires* an extra "dummy" byte
         * after the compressed stream in order to complete decompression and
         * return Z_STREAM_END. 
         * In unzip, i don't wait absolutely Z_STREAM_END because I known the 
         * size of both compressed and uncompressed data
         */
	}
	pfile_in_zip_read_info->rest_read_compressed = s->cur_file_info.compressed_size ;
	pfile_in_zip_read_info->rest_read_uncompressed = s->cur_file_info.uncompressed_size ;

	
	pfile_in_zip_read_info->pos_in_zipfile = s->cur_file_info_internal.offset_curfile + SIZEZIPLOCALHEADER + iSizeVar;
	
	pfile_in_zip_read_info->stream.avail_in = (unsigned int)0;


	s->pfile_in_zip_read = pfile_in_zip_read_info;
    return UNZ_OK;
}




//dmw's addition
extern FILE* ZEXPORT unzOpenCurrentFileFile (unzFile file, char *zipwasnamed)
{
	FILE *F;
//	int Store;
	unsigned int iSizeVar;
	unsigned int pos;
	unz_s* s;
//	file_in_zip_read_info_s* pfile_in_zip_read_info;
	unsigned long offset_local_extrafield;  /* offset of the local extra field */
	unsigned int  size_local_extrafield;    /* size of the local extra field */

	if (!file) return NULL;
	s=(unz_s*)file;
	if (!s->current_file_ok) return NULL;

    if (s->pfile_in_zip_read) unzCloseCurrentFile(file);

	if (unzlocal_CheckCurrentFileCoherencyHeader(s,&iSizeVar, &offset_local_extrafield,&size_local_extrafield)!=UNZ_OK) return NULL;



	if (s->cur_file_info.compression_method!=0) return NULL;	

	//opens a file into the uncompressed steam.
	pos = s->cur_file_info_internal.offset_curfile + SIZEZIPLOCALHEADER + iSizeVar;

	F = fopen (zipwasnamed, "rb");

	if (!F)
		Sys_Error ("Couldn't reopen %s", zipwasnamed);	
	fseek (F, pos, SEEK_SET);
    return F;
}

extern int ZEXPORT unzGetCurrentFileUncompressedPos (unzFile file) {
//	int err=UNZ_OK;
//	int Store;
	unsigned int iSizeVar;
	unsigned int pos;
	unz_s* s;
//	file_in_zip_read_info_s* pfile_in_zip_read_info;
	unsigned long offset_local_extrafield;  /* offset of the local extra field */
	unsigned int  size_local_extrafield;    /* size of the local extra field */

	if (!file) return -1;
	s=(unz_s*)file;
	if (!s->current_file_ok) return -1;

    if (s->pfile_in_zip_read) unzCloseCurrentFile(file);

	if (unzlocal_CheckCurrentFileCoherencyHeader(s,&iSizeVar, &offset_local_extrafield,&size_local_extrafield)!=UNZ_OK) return -1;



	if (s->cur_file_info.compression_method!=0) return -1;	

	//opens a file into the uncompressed steam.
	pos = s->cur_file_info_internal.offset_curfile + SIZEZIPLOCALHEADER + iSizeVar;

	return pos;
}




/*
  Read bytes from the current file.
  buf contain buffer where data must be copied
  len the size of buf.

  return the number of byte copied if somes bytes are copied
  return 0 if the end of file was reached
  return <0 with error code if there is an error
    (UNZ_ERRNO for IO error, or zLib error for uncompress error)
*/
extern int ZEXPORT unzReadCurrentFile  (unzFile file, voidp buf, unsigned len) {
	int err=UNZ_OK;
	unsigned int iRead = 0;
	unz_s* s;
	file_in_zip_read_info_s* pfile_in_zip_read_info;
	if (!file) return UNZ_PARAMERROR;
	s=(unz_s*)file;
    pfile_in_zip_read_info=s->pfile_in_zip_read;

	if (!pfile_in_zip_read_info) return UNZ_PARAMERROR;


	if ((!pfile_in_zip_read_info->read_buffer)) return UNZ_END_OF_LIST_OF_FILE;
	if (!len) return 0;

	pfile_in_zip_read_info->stream.next_out = (Bytef*)buf;

	pfile_in_zip_read_info->stream.avail_out = (unsigned int)len;
	
	if (len>pfile_in_zip_read_info->rest_read_uncompressed)
		pfile_in_zip_read_info->stream.avail_out = (unsigned int)pfile_in_zip_read_info->rest_read_uncompressed;

	while (pfile_in_zip_read_info->stream.avail_out>0) {
		if ((pfile_in_zip_read_info->stream.avail_in==0) && (pfile_in_zip_read_info->rest_read_compressed>0)) {
			unsigned int uReadThis = UNZ_BUFSIZE;
			if (pfile_in_zip_read_info->rest_read_compressed<uReadThis) uReadThis = (unsigned int)pfile_in_zip_read_info->rest_read_compressed;
			if (!uReadThis) return UNZ_EOF;
			if (!VFS_SEEK(pfile_in_zip_read_info->file,
                      pfile_in_zip_read_info->pos_in_zipfile + 
                         pfile_in_zip_read_info->byte_before_the_zipfile)) return UNZ_ERRNO;
			if (VFS_READ(pfile_in_zip_read_info->file, pfile_in_zip_read_info->read_buffer,uReadThis)!=uReadThis)	return UNZ_ERRNO;
			pfile_in_zip_read_info->pos_in_zipfile += uReadThis;

			pfile_in_zip_read_info->rest_read_compressed-=uReadThis;
			
			pfile_in_zip_read_info->stream.next_in = 
                (Bytef*)pfile_in_zip_read_info->read_buffer;
			pfile_in_zip_read_info->stream.avail_in = (unsigned int)uReadThis;
		}

		if (pfile_in_zip_read_info->compression_method==0) {
			unsigned int uDoCopy,i ;
			if (pfile_in_zip_read_info->stream.avail_out < pfile_in_zip_read_info->stream.avail_in) uDoCopy = pfile_in_zip_read_info->stream.avail_out ;
			else uDoCopy = pfile_in_zip_read_info->stream.avail_in ;
				
			for (i=0;i<uDoCopy;i++)
				*(pfile_in_zip_read_info->stream.next_out+i) = *(pfile_in_zip_read_info->stream.next_in+i);
					
			pfile_in_zip_read_info->crc32 = qcrc32(pfile_in_zip_read_info->crc32, pfile_in_zip_read_info->stream.next_out, uDoCopy);
			pfile_in_zip_read_info->rest_read_uncompressed-=uDoCopy;
			pfile_in_zip_read_info->stream.avail_in -= uDoCopy;
			pfile_in_zip_read_info->stream.avail_out -= uDoCopy;
			pfile_in_zip_read_info->stream.next_out += uDoCopy;
			pfile_in_zip_read_info->stream.next_in += uDoCopy;
            pfile_in_zip_read_info->stream.total_out += uDoCopy;
			iRead += uDoCopy;
		} else {
			unsigned long uTotalOutBefore,uTotalOutAfter;
			const Bytef *bufBefore;
			unsigned long uOutThis;
			int flush=Z_SYNC_FLUSH;

			uTotalOutBefore = pfile_in_zip_read_info->stream.total_out;
			bufBefore = pfile_in_zip_read_info->stream.next_out;

			err=qinflate(&pfile_in_zip_read_info->stream,flush);

			uTotalOutAfter = pfile_in_zip_read_info->stream.total_out;
			uOutThis = uTotalOutAfter-uTotalOutBefore;
			
			pfile_in_zip_read_info->crc32 = qcrc32(pfile_in_zip_read_info->crc32,bufBefore, (unsigned int)(uOutThis));

			pfile_in_zip_read_info->rest_read_uncompressed -= uOutThis;

			iRead += (unsigned int)(uTotalOutAfter - uTotalOutBefore);
            
			if (err==Z_STREAM_END) return (iRead==0) ? UNZ_EOF : iRead;
			if (err!=Z_OK) break;
		}
	}

	if (err==Z_OK) return iRead;
	return err;
}


/*
  Give the current position in uncompressed data
*/
extern z_off_t ZEXPORT unztell (unzFile file) {
	unz_s* s;
	file_in_zip_read_info_s* pfile_in_zip_read_info;
	if (!file) return UNZ_PARAMERROR;
	s=(unz_s*)file;
    pfile_in_zip_read_info=s->pfile_in_zip_read;

	if (!pfile_in_zip_read_info) return UNZ_PARAMERROR;

	return (z_off_t)pfile_in_zip_read_info->stream.total_out;
}


/*
  return 1 if the end of file was reached, 0 elsewhere 
*/
extern int ZEXPORT unzeof (unzFile file) {
	unz_s* s;
	file_in_zip_read_info_s* pfile_in_zip_read_info;
	if (!file) return UNZ_PARAMERROR;
	s=(unz_s*)file;
    pfile_in_zip_read_info=s->pfile_in_zip_read;

	if (!pfile_in_zip_read_info) return UNZ_PARAMERROR;
	
	if (pfile_in_zip_read_info->rest_read_uncompressed == 0) return 1;
	else return 0;
}



/*
  Read extra field from the current file (opened by unzOpenCurrentFile)
  This is the local-header version of the extra field (sometimes, there is
    more info in the local-header version than in the central-header)

  if buf==NULL, it return the size of the local extra field that can be read

  if buf!=NULL, len is the size of the buffer, the extra header is copied in
	buf.
  the return value is the number of bytes copied in buf, or (if <0) 
	the error code
*/
extern int ZEXPORT unzGetLocalExtrafield (unzFile file,voidp buf,unsigned len) {
	unz_s* s;
	file_in_zip_read_info_s* pfile_in_zip_read_info;
	unsigned int read_now;
	unsigned long size_to_read;

	if (!file) return UNZ_PARAMERROR;
	s=(unz_s*)file;
    pfile_in_zip_read_info=s->pfile_in_zip_read;

	if (!pfile_in_zip_read_info) return UNZ_PARAMERROR;

	size_to_read = (pfile_in_zip_read_info->size_local_extrafield - pfile_in_zip_read_info->pos_local_extrafield);

	if (!buf) return (int)size_to_read;
	
	if (len>size_to_read) read_now = (unsigned int)size_to_read;
	else read_now = (unsigned int)len ;

	if (!read_now) return 0;
	
	if (!VFS_SEEK(pfile_in_zip_read_info->file,
              pfile_in_zip_read_info->offset_local_extrafield + 
			  pfile_in_zip_read_info->pos_local_extrafield)) return UNZ_ERRNO;

	if (VFS_READ(pfile_in_zip_read_info->file, buf,(unsigned int)size_to_read)!=size_to_read) return UNZ_ERRNO;

	return (int)read_now;
}

/*
  Close the file in zip opened with unzipOpenCurrentFile
  Return UNZ_CRCERROR if all the file was read but the CRC is not good
*/
extern int ZEXPORT unzCloseCurrentFile (unzFile file) {
	int err=UNZ_OK;

	unz_s* s;
	file_in_zip_read_info_s* pfile_in_zip_read_info;
	if (!file) return UNZ_PARAMERROR;
	s=(unz_s*)file;
    pfile_in_zip_read_info=s->pfile_in_zip_read;

	if (!pfile_in_zip_read_info) return UNZ_PARAMERROR;


	if (!pfile_in_zip_read_info->rest_read_uncompressed) {
		if (pfile_in_zip_read_info->crc32 != pfile_in_zip_read_info->crc32_wait) err=UNZ_CRCERROR;
	}


	TRYFREE(pfile_in_zip_read_info->read_buffer);
	pfile_in_zip_read_info->read_buffer = NULL;
	if (pfile_in_zip_read_info->stream_initialised)	qinflateEnd(&pfile_in_zip_read_info->stream);

	pfile_in_zip_read_info->stream_initialised = 0;
	TRYFREE(pfile_in_zip_read_info);

    s->pfile_in_zip_read=NULL;

	return err;
}


/*
  Get the global comment string of the ZipFile, in the szComment buffer.
  uSizeBuf is the size of the szComment buffer.
  return the number of byte copied or an error code <0
*/
extern int ZEXPORT unzGetGlobalComment (unzFile file, char *szComment, unsigned long uSizeBuf) {
//	int err=UNZ_OK;
	unz_s* s;
	unsigned long uReadThis ;
	if (!file) return UNZ_PARAMERROR;
	s=(unz_s*)file;

	uReadThis = uSizeBuf;
	if (uReadThis>s->gi.size_comment) uReadThis = s->gi.size_comment;

	if (!VFS_SEEK(s->file,s->central_pos+22)) return UNZ_ERRNO;

	if (uReadThis>0) {
      *szComment='\0';
	  if (VFS_READ(s->file, szComment,(unsigned int)uReadThis)!=uReadThis) return UNZ_ERRNO;
    }

	if ((szComment != NULL) && (uSizeBuf > s->gi.size_comment)) *(szComment+s->gi.size_comment)='\0';
	return (int)uReadThis;
}
