void GoToDefinition(char *name);
void EditFile(char *name, int line);

void GUI_SetDefaultOpts(void);
int GUI_BuildParms(char *args, char **argv);

char *QCC_ReadFile (char *fname, void *buffer, int len);
int QCC_FileSize (char *fname);
pbool QCC_WriteFile (char *name, void *data, int len);
void GUI_DialogPrint(char *title, char *text);

extern char parameters[16384];

extern char progssrcname[256];
extern char progssrcdir[256];

extern pbool fl_hexen2;
extern pbool fl_autohighlight;
extern pbool fl_compileonstart;
extern pbool fl_showall;
extern pbool fl_log;
