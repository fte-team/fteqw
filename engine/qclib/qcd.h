pbool QC_decodeMethodSupported(int method);
char *QC_decode(progfuncs_t *progfuncs, int complen, int len, int method, char *info, char *buffer);
int QC_encode(progfuncs_t *progfuncs, int len, int method, char *in, int handle);
int QC_encodecrc(int len, char *in);

char *PDECL filefromprogs(pubprogfuncs_t *progfuncs, progsnum_t prnum, char *fname, size_t *size, char *buffer);
char *filefromnewprogs(pubprogfuncs_t *progfuncs, char *prname, char *fname, size_t *size, char *buffer);//fixme - remove parm 1

