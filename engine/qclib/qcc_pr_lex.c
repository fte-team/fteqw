#ifndef MINIMAL

#include "qcc.h"
#ifdef QCC
#define print printf
#endif

#define MEMBERFIELDNAME "__m%s"

#define STRCMP(s1,s2) (((*s1)!=(*s2)) || strcmp(s1+1,s2+1))	//saves about 2-6 out of 120 - expansion of idea from fastqcc

void QCC_PR_ConditionCompilation(void);
pbool QCC_PR_UndefineName(char *name);
char *QCC_PR_CheakCompConstString(char *def);
CompilerConstant_t *QCC_PR_CheckCompConstDefined(char *def);
pbool QCC_Include(char *filename);
float QCC_PR_LexOctal (void);

char *compilingfile;

int			pr_source_line;

char		*pr_file_p;
char		*pr_line_start;		// start of current source line

int			pr_bracelevel;

char		pr_token[8192];
token_type_t	pr_token_type;
QCC_type_t		*pr_immediate_type;
QCC_eval_t		pr_immediate;

char	pr_immediate_string[8192];

int		pr_error_count;


CompilerConstant_t *CompilerConstant;
int numCompilerConstants;



char	*pr_punctuation[] =
// longer symbols must be before a shorter partial match
{"&&", "||", "<=", ">=","==", "!=", "/=", "*=", "+=", "-=", "(+)", "(-)", "++", "--", "::", ";", ",", "!", "*", "/", "(", ")", "-", "+", "=", "[", "]", "{", "}", "...", "..", ".", "<<", "<", ">>", ">" , "#" , "@", "&" , "|", "^", ":", NULL};

// simple types.  function types are dynamically allocated
QCC_type_t	*type_void;// = {ev_void/*, &def_void*/};
QCC_type_t	*type_string;// = {ev_string/*, &def_string*/};
QCC_type_t	*type_float;// = {ev_float/*, &def_float*/};
QCC_type_t	*type_vector;// = {ev_vector/*, &def_vector*/};
QCC_type_t	*type_entity;// = {ev_entity/*, &def_entity*/};
QCC_type_t	*type_field;// = {ev_field/*, &def_field*/};
QCC_type_t	*type_function;// = {ev_function/*, &def_function*/,NULL,&type_void};
// type_function is a void() function used for state defs
QCC_type_t	*type_pointer;// = {ev_pointer/*, &def_pointer*/};
QCC_type_t	*type_integer;// = {ev_integer/*, &def_integer*/};

QCC_type_t	*type_floatfield;// = {ev_field/*, &def_field*/, NULL, &type_float};

#ifdef QCCONLY
const int		type_size[9] = {1,1,1,3,1,1,1,1,1};
#endif

/*QCC_def_t	def_void = {type_void, "temp"};
QCC_def_t	def_string = {type_string, "temp"};
QCC_def_t	def_float = {type_float, "temp"};
QCC_def_t	def_vector = {type_vector, "temp"};
QCC_def_t	def_entity = {type_entity, "temp"};
QCC_def_t	def_field = {type_field, "temp"};
QCC_def_t	def_function = {type_function, "temp"};
QCC_def_t	def_pointer = {type_pointer, "temp"};
QCC_def_t	def_integer = {type_integer, "temp"};
*/
QCC_def_t	def_ret, def_parms[MAX_PARMS];

//QCC_def_t	*def_for_type[9] = {&def_void, &def_string, &def_float, &def_vector, &def_entity, &def_field, &def_function, &def_pointer, &def_integer};

void QCC_PR_LexWhitespace (void);




//for compiler constants and file includes.

typedef struct qcc_includechunk_s {
	struct qcc_includechunk_s *prev;
	char *filename;
	char *currentdatapoint;
	int currentlinenumber;
} qcc_includechunk_t;
qcc_includechunk_t *currentchunk;
void QCC_PR_IncludeChunk (char *data, pbool duplicate, char *filename)
{
	qcc_includechunk_t *chunk = qccHunkAlloc(sizeof(qcc_includechunk_t));
	chunk->prev = currentchunk;
	currentchunk = chunk;

	chunk->currentdatapoint = pr_file_p;
	chunk->currentlinenumber = pr_source_line;

	if (duplicate)
	{
		pr_file_p = qccHunkAlloc(strlen(data)+1);
		strcpy(pr_file_p, data);
	}
	else
		pr_file_p = data;
}

pbool QCC_PR_UnInclude(void)
{
	if (!currentchunk)
		return false;
	pr_file_p = currentchunk->currentdatapoint;
	pr_source_line = currentchunk->currentlinenumber;

	currentchunk = currentchunk->prev;

	return true;
}


/*
==============
PR_PrintNextLine
==============
*/
void QCC_PR_PrintNextLine (void)
{
	char	*t;

	printf ("%3i:",pr_source_line);
	for (t=pr_line_start ; *t && *t != '\n' ; t++)
		printf ("%c",*t);
	printf ("\n");
}

/*
==============
PR_NewLine

Call at start of file and when *pr_file_p == '\n'
==============
*/
int ForcedCRC;
int QCC_PR_LexInteger (void);
void	QCC_AddFile (char *filename);
void QCC_PR_LexString (void);
pbool QCC_PR_SimpleGetToken (void);
void QCC_PR_NewLine (pbool incomment)
{
	char msg[1024];
	int ifmode;
	int a;
	pbool	m;
	static int ifs = 0;
	int level;	//#if level
	pbool eval = false;
	
	if (*pr_file_p == '\n')
	{
		pr_file_p++;
		m = true;
	}
	else
		m = false;

	pr_source_line++;
	pr_line_start = pr_file_p;
	while(*pr_file_p==' ' || *pr_file_p == '\t')
		pr_file_p++;
	if (incomment)	//no constants if in a comment.
	{
	}
	else if (*pr_file_p == '#')
	{
		char *directive;
		for (directive = pr_file_p+1; *directive; directive++)
		{
			if (*directive == '\r' || *directive == '\n')
				QCC_PR_ParseError(ERR_UNKNOWNPUCTUATION, "Hanging # with no directive\n");
			if (*directive > ' ')
				break;
		}
		if (!strncmp(directive, "define", 6))
		{
			pr_file_p = directive;
			QCC_PR_ConditionCompilation();
			while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
			{
				pr_file_p++;
			}
			if (!m)
				pr_file_p++;
		}
		else if (!strncmp(directive, "undef", 5))
		{
			pr_file_p = directive+5;
			while(*pr_file_p <= ' ')
				pr_file_p++;

			QCC_PR_SimpleGetToken ();
			QCC_PR_UndefineName(pr_token);

	//		QCC_PR_ConditionCompilation();
			while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
			{
				pr_file_p++;
			}
			if (!m)
				pr_file_p++;
		}
		else if (!strncmp(directive, "if", 2))
		{
 			pr_file_p = directive+2;
			if (!strncmp(pr_file_p, "def ", 4))
			{
				ifmode = 0;
				pr_file_p+=4;
			}
			else if (!strncmp(pr_file_p, "ndef ", 5))
			{
				ifmode = 1;
				pr_file_p+=5;
			}
			else
			{
				ifmode = 2;
				pr_file_p+=0;
				//QCC_PR_ParseError("bad \"#if\" type");
			}

			QCC_PR_SimpleGetToken ();
			level = 1;

			while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
			{
				pr_file_p++;
			}
	//		pr_file_p++;
	//		pr_source_line++;

			if (ifmode == 2)
			{
				if (atof(pr_token))
					eval = true;
			}
			else
			{
	//			if (!STRCMP(pr_token, "COOP_MODE"))
	//				eval = false;
				if (QCC_PR_CheckCompConstDefined(pr_token))
					eval = true;

				if (ifmode == 1)		
					eval = eval?false:true;		
			}

			if (eval)
				ifs+=1;
			else
			{
				while (1)
				{
					while(*pr_file_p==' ' || *pr_file_p == '\t')
						pr_file_p++;
					if (*pr_file_p == '#')
					{
						pr_file_p++;
						while(*pr_file_p==' ' || *pr_file_p == '\t')
							pr_file_p++;
						if (!strncmp(pr_file_p, "endif", 5))
							level--;
						if (!strncmp(pr_file_p, "if", 2))
							level++;
						if (!strncmp(pr_file_p, "else", 4) && level == 1)
						{
							ifs+=1;
							while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
							{
								pr_file_p++;
							}
							break;
						}
					}

					while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
					{
						pr_file_p++;
					}
					if (level <= 0)
						break;
					pr_file_p++;	//next line
					pr_source_line++;
				}
			}
		}
		else if (!strncmp(directive, "else", 4))
		{
			ifs -= 1;
			level = 1;
			
			while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
			{
				pr_file_p++;
			}
			while (1)
			{
				while(*pr_file_p==' ' || *pr_file_p == '\t')
					pr_file_p++;
				if (*pr_file_p == '#')
				{
					pr_file_p++;
					while(*pr_file_p==' ' || *pr_file_p == '\t')
						pr_file_p++;

					if (!strncmp(pr_file_p, "endif", 5))
						level--;
					if (!strncmp(pr_file_p, "if", 2))
							level++;
					if (!strncmp(pr_file_p, "else", 4) && level == 1)
					{
						ifs+=1;
						break;
					}
				}

				while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
				{
					pr_file_p++;
				}
				if (level <= 0)
					break;
				pr_file_p++;	//go off the end
				pr_source_line++;
			}
		}
		else if (!strncmp(directive, "endif", 5))
		{		
			while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
			{
				pr_file_p++;
			}		
			if (ifs <= 0)
				QCC_PR_ParseError(ERR_NOPRECOMPILERIF, "unmatched #endif");
			else
				ifs-=1;
		}
		else if (!strncmp(directive, "eof", 3))
		{
			pr_file_p = NULL;
			return;
		}
		else if (!strncmp(directive, "error", 5))
		{		
			pr_file_p = directive+5;
			for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';

			while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line, yes, I KNOW we are going to register an error, and not properly leave this function tree, but...
			{
				pr_file_p++;
			}

			QCC_PR_ParseError(ERR_HASHERROR, "#Error: %s", msg);
		}
		else if (!strncmp(directive, "warning", 7))
		{		
			pr_file_p = directive+7;
			for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';

			while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
			{
				pr_file_p++;
			}

			QCC_PR_ParseWarning(WARN_PRECOMPILERMESSAGE, "#warning: %s", msg);
		}
		else if (!strncmp(directive, "message", 7))
		{		
			pr_file_p = directive+7;
			for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';

			while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
			{
				pr_file_p++;
			}

			printf("#message: %s\n", msg);
		}
		else if (!strncmp(directive, "copyright", 9))
		{
			pr_file_p = directive+9;
			for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';

			while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
			{
				pr_file_p++;
			}

			if (strlen(msg) >= sizeof(QCC_copyright))
				QCC_PR_ParseWarning(WARN_STRINGTOOLONG, "Copyright message is too long\n");
			strncpy(QCC_copyright, msg, sizeof(QCC_copyright)-1);
		}
		else if (!strncmp(directive, "pack", 4))
		{
			ifmode = 0;
			pr_file_p=directive+4;
			if (!strncmp(pr_file_p, "id", 2))
				pr_file_p+=3;
			else	
			{
				ifmode = QCC_PR_LexInteger();					
				if (ifmode == 0)
					ifmode = 1;
				pr_file_p++;
			}
			for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';

			while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
			{
				pr_file_p++;
			}

			if (ifmode == 0)
				QCC_packid = atoi(msg);
			else if (ifmode <= 5)
				strcpy(QCC_Packname[ifmode-1], msg);
			else
				QCC_PR_ParseError(ERR_TOOMANYPACKFILES, "No more than 5 packs are allowed");		
		}
		else if (!strncmp(directive, "forcecrc", 8))
		{		
			pr_file_p=directive+8;

			ForcedCRC = QCC_PR_LexInteger();					

			pr_file_p++;
				
			for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';

			while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
			{
				pr_file_p++;
			}	
		}
		else if (!strncmp(directive, "includelist", 11))
		{
			pr_file_p=directive+11;

			while(*pr_file_p <= ' ')
				pr_file_p++;

			while(1)
			{
				QCC_PR_LexWhitespace();
				if (!QCC_PR_SimpleGetToken())
				{
					if (!*pr_file_p)
						QCC_Error(ERR_EOF, "eof in includelist");
					else
					{
						pr_file_p++;
						pr_source_line++;
					}
					continue;
				}
				if (!strcmp(pr_token, "#endlist"))
					break;
				printf("Including: %s\n", pr_token);
				QCC_Include(pr_token);

				if (*pr_file_p == '\r')
					pr_file_p++;

				for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
					msg[a] = pr_file_p[a];

				msg[a-1] = '\0';

				while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
				{
					pr_file_p++;
				}
			}
			
			while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
			{
				pr_file_p++;
			}
		}
		else if (!strncmp(directive, "include", 7))
		{		
			pr_file_p=directive+7;

			while(*pr_file_p <= ' ')
				pr_file_p++;

			QCC_PR_LexString();
			printf("Including: %s\n", pr_token);
			QCC_Include(pr_token);

			pr_file_p++;

			for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';

			while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
			{
				pr_file_p++;
			}
		}
		else if (!strncmp(directive, "datafile", 8))
		{		
			pr_file_p=directive+8;

			while(*pr_file_p <= ' ')
				pr_file_p++;

			QCC_PR_LexString();
			printf("Including datafile: %s\n", pr_token);
			QCC_AddFile(pr_token);

			pr_file_p++;

			for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';

			while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
			{
				pr_file_p++;
			}
		}
		else if (!strncmp(directive, "output", 6))
		{
			extern char		destfile[1024];
			pr_file_p=directive+6;

			while(*pr_file_p <= ' ')
				pr_file_p++;

			QCC_PR_LexString();
			strcpy(destfile, pr_token);
			printf("Outputfile: %s\n", destfile);

			pr_file_p++;
				
			for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';

			while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
			{
				pr_file_p++;
			}
		}
		else if (!strncmp(directive, "pragma", 6))
		{
			pr_file_p=directive+6;
			while(*pr_file_p <= ' ')
				pr_file_p++;

			qcc_token[0] = '\0';
			for(a = 0; *pr_file_p != '\n' && *pr_file_p != '\0'; pr_file_p++)	//read on until the end of the line
			{
				if ((*pr_file_p == ' ' || *pr_file_p == '\t') && !*qcc_token)
				{
					msg[a] = '\0';
					strcpy(qcc_token, msg);
					a=0;
					continue;
				}
				msg[a++] = *pr_file_p;
			}
			
			msg[a] = '\0';
			{
				char *end;
				for (end = msg + a-1; end>=msg && *end <= ' '; end--)
					*end = '\0';
			}

			if (!*qcc_token)
			{
				strcpy(qcc_token, msg);
				msg[0] = '\0';
			}

			{
				char *end;
				for (end = msg + a-1; end>=msg && *end <= ' '; end--)
					*end = '\0';
			}

			if (!QC_strcasecmp(qcc_token, "DONT_COMPILE_THIS_FILE"))
			{
				while (*pr_file_p)
				{
					while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
						pr_file_p++;

					if (*pr_file_p == '\n')
					{
						QCC_PR_NewLine(false);
						pr_file_p++;
					}
				}
			}
			else if (!QC_strcasecmp(qcc_token, "COPYRIGHT"))
			{
				if (strlen(msg) >= sizeof(QCC_copyright))
					QCC_PR_ParseWarning(WARN_STRINGTOOLONG, "Copyright message is too long\n");
				strncpy(QCC_copyright, msg, sizeof(QCC_copyright)-1);
			}
			else if (!QC_strcasecmp(qcc_token, "TARGET"))
			{
				if (qcc_targetformat == QCF_HEXEN2 && numstatements)
					QCC_PR_ParseWarning(WARN_BADTARGET, "Cannot switch from hexen2 target \'%s\'. Ignored.", msg);
				else if (!QC_strcasecmp(msg, "H2"))
				{
					if (numstatements)
						QCC_PR_ParseWarning(WARN_BADTARGET, "Cannot switch from hexen2 target \'%s\'. Ignored.", msg);
					else
						qcc_targetformat = QCF_HEXEN2;
				}
				else if (!QC_strcasecmp(msg, "KK7"))
					qcc_targetformat = QCF_KK7;
				else if (!QC_strcasecmp(msg, "FTEDEBUG"))
					qcc_targetformat = QCF_FTEDEBUG;
				else if (!QC_strcasecmp(msg, "FTE"))
					qcc_targetformat = QCF_FTE;
				else if (!QC_strcasecmp(msg, "FTEDEBUG32") || !QC_strcasecmp(msg, "FTE32DEBUG"))
					qcc_targetformat = QCF_FTEDEBUG32;
				else if (!QC_strcasecmp(msg, "FTE32"))
					qcc_targetformat = QCF_FTE32;
				else if (!QC_strcasecmp(msg, "STANDARD") || !QC_strcasecmp(msg, "ID"))
					qcc_targetformat = QCF_STANDARD;
				else if (!QC_strcasecmp(msg, "DEBUG"))
				{
					if (qcc_targetformat == QCF_FTE32)
						qcc_targetformat = QCF_FTEDEBUG32;
					else if (qcc_targetformat == QCF_FTE)
						qcc_targetformat = QCF_FTEDEBUG;
					else if (qcc_targetformat == QCF_STANDARD)
						qcc_targetformat = QCF_FTEDEBUG;
				}
				else
					QCC_PR_ParseWarning(WARN_BADTARGET, "Unknown target \'%s\'. Ignored.", msg);
			}
			else if (!QC_strcasecmp(qcc_token, "PROGS_SRC"))
			{	//doesn't make sence, but silenced if you are switching between using a certain precompiler app used with CuTF.
			}
			else if (!QC_strcasecmp(qcc_token, "PROGS_DAT"))
			{	//doesn't make sence, but silenced if you are switching between using a certain precompiler app used with CuTF.
				extern char		destfile[1024];
				QCC_COM_Parse(msg);
				strcpy(destfile, qcc_token);
				printf("Outputfile: %s\n", destfile);
			}
			else if (!QC_strcasecmp(qcc_token, "disable"))
			{
				qccwarningdisabled[atoi(msg)] = true;
			}
			else if (!QC_strcasecmp(qcc_token, "enable"))
			{
				qccwarningdisabled[atoi(msg)] = false;
			}
			else
				QCC_PR_ParseWarning(WARN_BADPRAGMA, "Unknown pragma \'%s\'", qcc_token);
		}
	}

//	if (pr_dumpasm)
//		PR_PrintNextLine ();
	if (m)
		pr_file_p--;
}

/*
==============
PR_LexString

Parses a quoted string
==============
*/
#if 0
void QCC_PR_LexString (void)
{
	int		c;
	int		len;
	char tmpbuf[2048];
			
	char *text;
	char *oldf;
	int oldline;

	bool fromfile = true;

	len = 0;

	text = pr_file_p;
	do
	{
		QCC_COM_Parse(text);
//		print("Next token is \"%s\"\n", com_token);
		if (*text == '\"')
		{
			text++;
			if (fromfile) pr_file_p++;
		}
	do
	{
		c = *text++;
		if (fromfile) pr_file_p++;
		if (!c)
			QCC_PR_ParseError ("EOF inside quote");
		if (c=='\n')
			QCC_PR_ParseError ("newline inside quote");
		if (c=='\\')
		{	// escape char
			c = *text++;
			if (fromfile) pr_file_p++;
			if (!c)
				QCC_PR_ParseError ("EOF inside quote");
			if (c == 'n')
				c = '\n';
			else if (c == '"')
				c = '"';
			else if (c == '\\')
				c = '\\';
			else
				QCC_PR_ParseError ("Unknown escape char");
		}
		else if (c=='\"')
		{
			if (fromfile) pr_file_p++;
			break;
		}
		tmpbuf[len] = c;
		len++;
	} while (1);
		tmpbuf[len] = 0;
//		if (fromfile) pr_file_p++;

		pr_immediate_type=NULL;
		oldline=pr_source_line;
		oldf=pr_file_p;
		QCC_PR_Lex();
		if (pr_immediate_type == &type_string)
		{
//			print("Appending \"%s\" to \"%s\"\n", pr_immediate_string, tmpbuf);
			strcat(tmpbuf, pr_immediate_string);
			len+=strlen(pr_immediate_string);
		}
		else
		{
			pr_source_line = oldline;
			pr_file_p = oldf-1;
			QCC_PR_LexWhitespace();
			if (*pr_file_p != '\"')	//annother string
				break;				
		}

		QCC_PR_LexWhitespace();
		text = pr_file_p;		
		
	} while (1);

	strcpy(pr_token, tmpbuf);
	pr_token_type = tt_immediate;
	pr_immediate_type = &type_string;
	strcpy (pr_immediate_string, pr_token);

//	print("Found \"%s\"\n", pr_immediate_string);
}
#else
void QCC_PR_LexString (void)
{
	int		c;
	int		len;
	char	*end, *cnst;

	int texttype=0;
	
	len = 0;
	pr_file_p++;
	do
	{
		c = *pr_file_p++;
		if (!c)
			QCC_PR_ParseError (ERR_EOF, "EOF inside quote");
		if (c=='\n')
			QCC_PR_ParseError (ERR_INVALIDSTRINGIMMEDIATE, "newline inside quote");
		if (c=='\\')
		{	// escape char
			c = *pr_file_p++;
			if (!c)
				QCC_PR_ParseError (ERR_EOF, "EOF inside quote");
			if (c == 'n')
				c = '\n';
			else if (c == 'r')
				c = '\r';
			else if (c == '"')
				c = '"';
			else if (c == 't')
				c = '\t';
			else if (c == 's' || c == 'b')
			{
				texttype ^= 128;
				continue;
			}
			else if (c == '[')
				c = 16;
			else if (c == ']')
				c = 17;
			else if (c == '{')
			{
				int d;
				c = 0;
				while ((d = *pr_file_p++) != '}')
				{
					c = c * 10 + d - '0';
					if (d < '0' || d > '9' || c > 255)
						QCC_PR_ParseError(ERR_BADCHARACTURECODE, "Bad character code");
				}
			}
			else if (c == '<')
				c = 29;
			else if (c == '-')
				c = 30;
			else if (c == '>')
				c = 31;
			else if (c == '\\')
				c = '\\';
			else if (c == '\'')
				c = '\'';
			else if (c >= '0' && c <= '9')
				c = (int)QCC_PR_LexOctal();

			else if (c == '\r')
			{	//sigh
				c = *pr_file_p++;
				if (c != '\n')
					QCC_PR_ParseWarning(WARN_HANGINGSLASHR, "Hanging \\\\\r");
				pr_source_line++;
			}
			else if (c == '\n')
			{	//sigh
				pr_source_line++;
			}
			else
				QCC_PR_ParseError (ERR_INVALIDSTRINGIMMEDIATE, "Unknown escape char %c", c);
		}
		else if (c=='\"')
		{
			if (len >= sizeof(pr_immediate_string)-1)
				QCC_Error(ERR_INVALIDSTRINGIMMEDIATE, "String length exceeds %i", sizeof(pr_immediate_string)-1);

			while(*pr_file_p && *pr_file_p <= ' ')
				pr_file_p++;
			if (*pr_file_p == '\"')	//have annother go
			{
				pr_file_p++;
				continue;
			}
			pr_token[len] = 0;
			pr_token_type = tt_immediate;
			pr_immediate_type = type_string;
			strcpy (pr_immediate_string, pr_token);			
			return;
		}
		else if (c == '#')
		{
			for (end = pr_file_p; ; end++)
			{
				if (*end <= ' ')
					break;

				if (*end == ')'
					||	*end == '('
					||	*end == '+'
					||	*end == '-'
					||	*end == '*'
					||	*end == '/'
					||	*end == '\\'
					||	*end == '|'
					||	*end == '&'
					||	*end == '='
					||	*end == '^'
					||	*end == '~'
					||	*end == '['
					||	*end == ']'
					||	*end == '\"'
					||	*end == '{'
					||	*end == '}'
					||	*end == ';'
					||	*end == ':'
					||	*end == ','
					||	*end == '.'
					||	*end == '#')
						break;
			}

			c = *end;
			*end = '\0';
			cnst = QCC_PR_CheakCompConstString(pr_file_p);
			if (cnst==pr_file_p)
				cnst=NULL;
			*end = c;
			c = '#';	//undo
			if (cnst)
			{
				QCC_PR_ParseWarning(WARN_MACROINSTRING, "Macro expansion in string");

				if (len+strlen(cnst) >= sizeof(pr_token)-1)
					QCC_Error(ERR_INVALIDSTRINGIMMEDIATE, "String length exceeds %i", sizeof(pr_token)-1);

				strcpy(pr_token+len, cnst);
				len+=strlen(cnst);
				pr_file_p = end;
				continue;
			}
		}

		pr_token[len] = c|texttype;
		len++;
		if (len >= sizeof(pr_token)-1)
			QCC_Error(ERR_INVALIDSTRINGIMMEDIATE, "String length exceeds %i", sizeof(pr_token)-1);
	} while (1);
}
#endif

/*
==============
PR_LexNumber
==============
*/
float QCC_PR_LexOctal (void)
{
	int		c;
	int		len;
	int result;

	char *s;
	
	len = 0;
	c = *pr_file_p;
	do
	{
		pr_token[len] = c;
		len++;
		pr_file_p++;
		c = *pr_file_p;
		if (len >= 3)
			break;	//max of 3
	} while ((c >= '0' && c<= '7'));
	pr_token[len] = 0;

	result = 0;
	for (s = pr_token, result = 0; *s; s++)
	{
		result*=8;
		result+=*s-'0';
	}
	return (float)result;
}

int QCC_PR_LexInteger (void)
{
	int		c;
	int		len;
	
	len = 0;
	c = *pr_file_p;
	if (pr_file_p[0] == '0' && pr_file_p[1] == 'x')
	{
		pr_token[0] = '0';
		pr_token[1] = 'x';
		len = 2;
		c = *(pr_file_p+=2);
	}
	do
	{
		pr_token[len] = c;
		len++;
		pr_file_p++;
		c = *pr_file_p;
	} while ((c >= '0' && c<= '9') || c == '.' || (c>='a' && c <= 'f'));
	pr_token[len] = 0;
	return atoi (pr_token);
}

void QCC_PR_LexNumber (void)
{
	int num=0;
	int base=10;
	int c;
	int sign=1;
	if (*pr_file_p == '-')
	{
		sign=-1;
		pr_file_p++;
	}
	if (pr_file_p[1] == 'x')
	{
		pr_file_p+=2;
		base = 16;
	}

	while((c = *pr_file_p))
	{		
		if (c >= '0' && c <= '9')
		{
			num*=base;
			num += c-'0';
		}
		else if (c >= 'a' && c <= 'f')
		{
			num*=base;
			num += c -'a'+10;
		}
		else if (c >= 'A' && c <= 'F')
		{
			num*=base;
			num += c -'A'+10;
		}
		else if (c == '.')
		{
			pr_file_p++;
			pr_immediate_type = type_float;
			pr_immediate._float = (float)num;
			num = 1;
			while(1)
			{
				c = *pr_file_p;
				if (c >= '0' && c <= '9')
				{
					num*=base;
					pr_immediate._float += (c-'0')/(float)(num);
				}
				else
				{						
					break;
				}
				pr_file_p++;
			}
			pr_immediate._float *= sign;
			return;
		}
		else if (c == 'i')
		{
			pr_file_p++;
			pr_immediate_type = type_integer;
			pr_immediate._int = num*sign;
			return;
		}
		else break;
		pr_file_p++;
	}

	pr_immediate_type = type_float;
	pr_immediate._float = (float)(num*sign);
}


float QCC_PR_LexFloat (void)
{
	int		c;
	int		len;
	
	len = 0;
	c = *pr_file_p;
	do
	{
		pr_token[len] = c;
		len++;
		pr_file_p++;
		c = *pr_file_p;
	} while ((c >= '0' && c<= '9') || (c == '.'&&pr_file_p[1]!='.'));	//only allow a . if the next isn't too...
	pr_token[len] = 0;
	return (float)atof (pr_token);
}

/*
==============
PR_LexVector

Parses a single quoted vector
==============
*/
void QCC_PR_LexVector (void)
{
	int		i;
	
	pr_file_p++;

	if (*pr_file_p == '\\')
	{//extended characture constant
		pr_token_type = tt_immediate;
		pr_immediate_type = type_float;
		pr_file_p++;
		switch(*pr_file_p)
		{
		case 'n':
			pr_immediate._float = '\n';
			break;
		case 'r':
			pr_immediate._float = '\r';
			break;
		case 't':
			pr_immediate._float = '\t';
			break;
		case '\'':
			pr_immediate._float = '\'';
			break;
		case '\"':
			pr_immediate._float = '\"';
			break;
		case '\\':
			pr_immediate._float = '\\';
			break;
		case '0':
		case '1':
		case '2':	//assume string constant
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			pr_immediate._float = QCC_PR_LexOctal();	//what's the real point? Neatness?
			break;
		default:
			QCC_PR_ParseError (ERR_INVALIDVECTORIMMEDIATE, "Bad characture constant");
		}
		if (*pr_file_p != '\'')
			QCC_PR_ParseError (ERR_INVALIDVECTORIMMEDIATE, "Bad characture constant");
		pr_file_p++;
		return;
	}
	if (pr_file_p[1] == '\'')
	{//character constant
		pr_token_type = tt_immediate;
		pr_immediate_type = type_float;
		pr_immediate._float = pr_file_p[0];
		pr_file_p+=2;
		return;
	}
	pr_token_type = tt_immediate;
	pr_immediate_type = type_vector;
	QCC_PR_LexWhitespace ();
	for (i=0 ; i<3 ; i++)
	{
		pr_immediate.vector[i] = QCC_PR_LexFloat ();
		QCC_PR_LexWhitespace ();
	}
	if (*pr_file_p != '\'')
		QCC_PR_ParseError (ERR_INVALIDVECTORIMMEDIATE, "Bad vector");
	pr_file_p++;
}

/*
==============
PR_LexName

Parses an identifier
==============
*/
void QCC_PR_LexName (void)
{
	int		c;
	int		len;
	
	len = 0;
	c = *pr_file_p;
	do
	{
		pr_token[len] = c;
		len++;
		pr_file_p++;
		c = *pr_file_p;
	} while ( (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' 
	|| (c >= '0' && c <= '9'));	

	pr_token[len] = 0;
	pr_token_type = tt_name;	
}

/*
==============
PR_LexPunctuation
==============
*/
void QCC_PR_LexPunctuation (void)
{
	int		i;
	int		len;
	char	*p;
	
	pr_token_type = tt_punct;
	
	for (i=0 ; (p = pr_punctuation[i]) != NULL ; i++)
	{
		len = strlen(p);
		if (!strncmp(p, pr_file_p, len) )
		{
			strcpy (pr_token, p);
			if (p[0] == '{')
				pr_bracelevel++;
			else if (p[0] == '}')
				pr_bracelevel--;
			pr_file_p += len;
			return;
		}
	}
	
	QCC_PR_ParseError (ERR_UNKNOWNPUCTUATION, "Unknown punctuation");
}

		
/*
==============
PR_LexWhitespace
==============
*/
void QCC_PR_LexWhitespace (void)
{
	int		c;
	
	while (1)
	{
	// skip whitespace
		while ( (c = *pr_file_p) <= ' ')
		{
			if (c=='\n')
			{
				QCC_PR_NewLine (false);
				if (!pr_file_p)
					return;
			}
			if (c == 0)
				return;		// end of file
			pr_file_p++;
		}
		
	// skip // comments
		if (c=='/' && pr_file_p[1] == '/')
		{
			while (*pr_file_p && *pr_file_p != '\n')
				pr_file_p++;
			QCC_PR_NewLine(false);
			pr_file_p++;
			continue;
		}
		
	// skip /* */ comments
		if (c=='/' && pr_file_p[1] == '*')
		{
			do
			{
				pr_file_p++;
				if (pr_file_p[0]=='\n')
					QCC_PR_NewLine(true);
				if (pr_file_p[1] == 0)
				{
					pr_file_p++;
					return;
				}
			} while (pr_file_p[-1] != '*' || pr_file_p[0] != '/');
			pr_file_p++;
			continue;
		}
		
		break;		// a real character has been found
	}
}

//============================================================================

#define	MAX_FRAMES	8192

char	pr_framemacros[MAX_FRAMES][16];
int		pr_framemacrovalue[MAX_FRAMES];
int		pr_nummacros;
int		pr_macrovalue;
int		pr_savedmacro;

void QCC_PR_ClearGrabMacros (void)
{
	pr_nummacros = 0;
	pr_macrovalue = 0;
	pr_savedmacro = -1;
}

void QCC_PR_FindMacro (void)
{
	int		i;

	for (i=0 ; i<pr_nummacros ; i++)
	{
		if (!STRCMP (pr_token, pr_framemacros[i]))
		{
			sprintf (pr_token,"%d", pr_framemacrovalue[i]);
			pr_token_type = tt_immediate;
			pr_immediate_type = type_float;
			pr_immediate._float = (float)pr_framemacrovalue[i];
			return;
		}
	}
	for (i=0 ; i<pr_nummacros ; i++)
	{
		if (!stricmp (pr_token, pr_framemacros[i]))
		{
			sprintf (pr_token,"%d", pr_framemacrovalue[i]);
			pr_token_type = tt_immediate;
			pr_immediate_type = type_float;
			pr_immediate._float = (float)pr_framemacrovalue[i];

			QCC_PR_ParseWarning(WARN_CASEINSENSATIVEFRAMEMACRO, "Case insensative frame macro");
			return;
		}
	}
	QCC_PR_ParseError (ERR_BADFRAMEMACRO, "Unknown frame macro $%s", pr_token);
}

// just parses text, returning false if an eol is reached
pbool QCC_PR_SimpleGetToken (void)
{
	int		c;
	int		i;
	
// skip whitespace
	while ( (c = *pr_file_p) <= ' ')
	{
		if (c=='\n' || c == 0)
			return false;
		pr_file_p++;
	}
	
	i = 0;
	while ( (c = *pr_file_p) > ' ' && c != ',' && c != ';' && c != ')' && c != '(')
	{
		pr_token[i] = c;
		i++;
		pr_file_p++;
	}
	pr_token[i] = 0;
	return true;
}

void QCC_PR_ParseFrame (void)
{
	while (QCC_PR_SimpleGetToken ())
	{
		strcpy (pr_framemacros[pr_nummacros], pr_token);
		pr_framemacrovalue[pr_nummacros] = pr_macrovalue++;
		pr_nummacros++;
		if (pr_nummacros >= MAX_FRAMES)
			QCC_PR_ParseError(ERR_TOOMANYFRAMEMACROS, "Too many frame macros defined");
	}
}

/*
==============
PR_LexGrab

Deals with counting sequence numbers and replacing frame macros
==============
*/
void QCC_PR_LexGrab (void)
{	
	pr_file_p++;	// skip the $
//	if (!QCC_PR_SimpleGetToken ())
//		QCC_PR_ParseError ("hanging $");
	if (*pr_file_p <= ' ')
		QCC_PR_ParseError (ERR_BADFRAMEMACRO, "hanging $");
	QCC_PR_SimpleGetToken();
	if (!*pr_token)
		QCC_PR_ParseError (ERR_BADFRAMEMACRO, "hanging $");
	
// check for $frame
	if (!STRCMP (pr_token, "frame") || !STRCMP (pr_token, "framesave"))
	{
		QCC_PR_ParseFrame ();
		QCC_PR_Lex ();
	}
// ignore other known $commands - just for model/spritegen
	else if (!STRCMP (pr_token, "cd")
	|| !STRCMP (pr_token, "origin")
	|| !STRCMP (pr_token, "base")
	|| !STRCMP (pr_token, "flags")
	|| !STRCMP (pr_token, "scale")
	|| !STRCMP (pr_token, "skin") )
	{	// skip to end of line
		while (QCC_PR_SimpleGetToken ())
		;
		QCC_PR_Lex ();
	}
	else if (!STRCMP (pr_token, "flush"))
	{
		QCC_PR_ClearGrabMacros();
		while (QCC_PR_SimpleGetToken ())
		;
		QCC_PR_Lex ();
	}
	else if (!STRCMP (pr_token, "framevalue"))
	{
		QCC_PR_SimpleGetToken ();
		pr_macrovalue = atoi(pr_token);
		
		QCC_PR_Lex ();
	}
	else if (!STRCMP (pr_token, "framerestore"))
	{
		QCC_PR_SimpleGetToken ();
		QCC_PR_FindMacro();
		pr_macrovalue = (int)pr_immediate._float;
		
		QCC_PR_Lex ();
	}
// look for a frame name macro
	else
		QCC_PR_FindMacro ();
}

//===========================
//compiler constants	- dmw

pbool QCC_PR_UndefineName(char *name)
{
//	int a;
	CompilerConstant_t *c;
	c = Hash_Get(&compconstantstable, name);
	if (!c)
	{
		QCC_PR_ParseWarning(WARN_NOTDEFINED, "Precompiler constant %s was not defined", name);
		return false;
	}

	Hash_Remove(&compconstantstable, name);
	return true;
	/*
	a = c-CompilerConstant;
//	for (a = 0; a < numCompilerConstants; a++)
	{
//		if (!STRCMP(name, CompilerConstant[a].name))
		{
			memmove(&CompilerConstant[a], &CompilerConstant[a+1], sizeof(CompilerConstant_t) * (numCompilerConstants-a));
			numCompilerConstants--;




			if (!STRCMP(name, "OP_NODUP"))
				qccop_noduplicatestrings = false;

			if (!STRCMP(name, "OP_COMP_ALL"))		//group	
			{
				QCC_PR_UndefineName("OP_COMP_STATEMENTS");
				QCC_PR_UndefineName("OP_COMP_DEFS");
				QCC_PR_UndefineName("OP_COMP_FIELDS");
				QCC_PR_UndefineName("OP_COMP_FUNCTIONS");
				QCC_PR_UndefineName("OP_COMP_STRINGS");
				QCC_PR_UndefineName("OP_COMP_GLOBALS");
				QCC_PR_UndefineName("OP_COMP_LINES");
				QCC_PR_UndefineName("OP_COMP_TYPES");
			}

			return true;
		}
	}
//	return false;
*/
}

CompilerConstant_t *QCC_PR_DefineName(char *name)
{
	int i;
	CompilerConstant_t *cnst;

//	if (numCompilerConstants >= MAX_CONSTANTS)
//		QCC_PR_ParseError("Too many compiler constants - %i >= %i", numCompilerConstants, MAX_CONSTANTS);

	if (strlen(name) >= MAXCONSTANTLENGTH || !*name)
		QCC_PR_ParseError(ERR_CONSTANTTOOLONG, "Compiler constant name length is too long or short");
	
	cnst = Hash_Get(&compconstantstable, name);
	if (cnst )
	{
		QCC_PR_ParseWarning(WARN_DUPLICATEDEFINITION, "Duplicate definition for Precompiler constant %s", name);
		Hash_Remove(&compconstantstable, name);
	}

	cnst = qccHunkAlloc(sizeof(CompilerConstant_t));

	cnst->used = false;
	cnst->numparams = 0;
	strcpy(cnst->name, name);
	cnst->namelen = strlen(name);
	*cnst->value = '\0';
	for (i = 0; i < MAXCONSTANTPARAMS; i++)
		cnst->params[i][0] = '\0';

	Hash_Add(&compconstantstable, cnst->name, cnst);

	if (!STRCMP(name, "OP_NODUP"))
		opt_noduplicatestrings = true;


	if (!STRCMP(name, "OP_TIME"))	//group - optimize for a fast compiler
	{
		QCC_PR_UndefineName("OP_SIZE");
		QCC_PR_UndefineName("OP_SPEED");

		QCC_PR_UndefineName("OP_NODUP");
		QCC_PR_UndefineName("OP_COMP_ALL");
	}

	if (!STRCMP(name, "OP_SPEED"))	//group - optimize run speed
	{
		QCC_PR_UndefineName("OP_SIZE");
		QCC_PR_UndefineName("OP_TIME");

//		QCC_PR_UndefineName("OP_NODUP");
		QCC_PR_UndefineName("OP_COMP_ALL");
	}

	if (!STRCMP(name, "OP_SIZE"))	//group - produce small output.
	{
		QCC_PR_UndefineName("OP_SPEED");
		QCC_PR_UndefineName("OP_TIME");

		QCC_PR_DefineName("OP_NODUP");
		QCC_PR_DefineName("OP_COMP_ALL");
	}

	if (!STRCMP(name, "OP_COMP_ALL"))	//group	- compress the output
	{
		QCC_PR_DefineName("OP_COMP_STATEMENTS");
		QCC_PR_DefineName("OP_COMP_DEFS");
		QCC_PR_DefineName("OP_COMP_FIELDS");
		QCC_PR_DefineName("OP_COMP_FUNCTIONS");
		QCC_PR_DefineName("OP_COMP_STRINGS");
		QCC_PR_DefineName("OP_COMP_GLOBALS");
		QCC_PR_DefineName("OP_COMP_LINES");
		QCC_PR_DefineName("OP_COMP_TYPES");
	}



	return cnst;
}

void QCC_PR_Undefine(void)
{
	QCC_PR_SimpleGetToken ();

	QCC_PR_UndefineName(pr_token);
//		QCC_PR_ParseError("%s was not defined.", pr_token);
}

void QCC_PR_ConditionCompilation(void)
{	
	char *d;
	char *s;
	int quote=false;
	CompilerConstant_t *cnst;

	QCC_PR_SimpleGetToken ();

	if (!QCC_PR_SimpleGetToken ())		
		QCC_PR_ParseError(ERR_NONAME, "No name defined for compiler constant");

	cnst = Hash_Get(&compconstantstable, pr_token);
	if (cnst)
	{
		Hash_Remove(&compconstantstable, pr_token);
		QCC_PR_ParseWarning(WARN_DUPLICATEPRECOMPILER, "Duplicate definition of %s", pr_token);
	}

	cnst = QCC_PR_DefineName(pr_token);

	if (*pr_file_p == '(')
	{
		s = pr_file_p+1;
		while(*pr_file_p++)
		{
			if (*pr_file_p == ',')
			{
				strncpy(cnst->params[cnst->numparams], s, pr_file_p-s);
				cnst->params[cnst->numparams][pr_file_p-s] = '\0';
				cnst->numparams++;
				if (cnst->numparams > MAXCONSTANTPARAMS)
					QCC_PR_ParseError(ERR_MACROTOOMANYPARMS, "May not have more than %i parameters to a macro", MAXCONSTANTPARAMS);
				pr_file_p++;
				s = pr_file_p;
			}
			if (*pr_file_p == ')')
			{
				strncpy(cnst->params[cnst->numparams], s, pr_file_p-s);
				cnst->params[cnst->numparams][pr_file_p-s] = '\0';
				cnst->numparams++;
				if (cnst->numparams > MAXCONSTANTPARAMS)
					QCC_PR_ParseError(ERR_MACROTOOMANYPARMS, "May not have more than %i parameters to a macro", MAXCONSTANTPARAMS);
				pr_file_p++;
				break;
			}
		}
	}
	else cnst->numparams = -1;

	s = pr_file_p;
	d = cnst->value;
	while(*s == ' ' || *s == '\t')
		s++;
	while(1)
	{
		if (*s == '\r' || *s == '\n' || *s == '\0')
		{
			if (s[-1] == '\\')
			{
			}
			else if (s[-2] == '\\' && s[-1] == '\r' && s[-1] == '\n')
			{
			}
			else
				break;
		}

		if (!quote && s[0]=='/'&&(s[1]=='/'||s[1]=='*'))
			break;
		if (*s == '\"')
			quote=!quote;

		*d = *s;
		d++;
		s++;
	}
	*d = '\0';
	d--;
	while(*d<= ' ' && d >= cnst->value)
		*d-- = '\0';
	if (strlen(cnst->value) >= sizeof(cnst->value))	//this is too late.
		QCC_PR_ParseError(ERR_CONSTANTTOOLONG, "Macro %s too long (%i not %i)", cnst->name, strlen(cnst->value), sizeof(cnst->value));
}

char *daynames[] =
{
	"Mon",
	"Tue",
	"Wed",
	"Thu",
	"Fri",
	"Sat",
	"Sun"
};
char *monthnames[] =
{
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec"
};
int QCC_PR_CheakCompConst(void)
{
	char		*oldpr_file_p = pr_file_p;

	CompilerConstant_t *c;

	char *end;
	for (end = pr_file_p; ; end++)
	{
		if (*end <= ' ')
			break;

		if (*end == ')'
			||	*end == '('
			||	*end == '+'
			||	*end == '-'
			||	*end == '*'
			||	*end == '/'
			||	*end == '|'
			||	*end == '&'
			||	*end == '='
			||	*end == '^'
			||	*end == '~'
			||	*end == '['
			||	*end == ']'
			||	*end == '\"'
			||	*end == '{'
			||	*end == '}'
			||	*end == ';'
			||	*end == ':'
			||	*end == ','
			||	*end == '.'
			||	*end == '#')
				break;
	}
	strncpy(pr_token, pr_file_p, end-pr_file_p);
	pr_token[end-pr_file_p]='\0';
//	printf("%s\n", pr_token);
	c = Hash_Get(&compconstantstable, pr_token);

	if (c)
	{
		pr_file_p = oldpr_file_p+strlen(c->name);
		QCC_PR_LexWhitespace();
		if (c->numparams>=0)
		{
			if (*pr_file_p == '(')
			{
				int p;
				char *start;
				char buffer[1024];
				char *paramoffset[MAXCONSTANTPARAMS+1];
				int param=0;

				pr_file_p++;
				QCC_PR_LexWhitespace();
				start = pr_file_p;
				while(1)
				{
					if (*pr_file_p == ',' || *pr_file_p == ')')
					{
						paramoffset[param++] = start;
						start = pr_file_p+1;
						if (*pr_file_p == ')')
						{
							*pr_file_p = '\0';
							pr_file_p++;
							break;
						}
						*pr_file_p = '\0';
						pr_file_p++;
						QCC_PR_LexWhitespace();
						if (param == MAXCONSTANTPARAMS)
							QCC_PR_ParseError(ERR_TOOMANYPARAMS, "Too many parameters in macro call");
					}
					if (!*pr_file_p)
						QCC_PR_ParseError(ERR_EOF, "EOF on macro call");
					pr_file_p++;
				}
				if (param < c->numparams)
					QCC_PR_ParseError(ERR_TOOFEWPARAMS, "Not enough macro parameters");
				paramoffset[param] = start;

				*buffer = '\0';

				oldpr_file_p = pr_file_p;
				pr_file_p = c->value;
				do
				{
					pr_file_p = QCC_COM_Parse(pr_file_p);
					if (!pr_file_p)
						break;

					for (p = 0; p < param; p++)
					{
						if (!STRCMP(qcc_token, c->params[p]))
						{
							strcat(buffer, paramoffset[p]);
							break;
						}
					}
					if (p == param)
						strcat(buffer, qcc_token);
				} while(1);

				for (p = 0; p < param-1; p++)
					paramoffset[p][strlen(paramoffset[p])] = ',';
				paramoffset[p][strlen(paramoffset[p])] = ')';

				pr_file_p = oldpr_file_p;
				QCC_PR_IncludeChunk(buffer, true, NULL);
			}
			else
				QCC_PR_ParseError(ERR_TOOFEWPARAMS, "Macro without opening brace");
		}
		else
			QCC_PR_IncludeChunk(c->value, false, NULL);

		QCC_PR_Lex();
		return true;
	}

	if (!strncmp(pr_file_p, "__TIME__", 8))
	{
		static char retbuf[256];
#ifdef WIN32
		SYSTEMTIME Systime;
		GetSystemTime(&Systime);
		sprintf(retbuf, "\"%i:%i\"", Systime.wHour, Systime.wMinute);
#else	//linux
		sprintf(retbuf, "\"unknown time\"");
#endif
		pr_file_p = retbuf;
		QCC_PR_Lex();	//translate the macro's value
		pr_file_p = oldpr_file_p+8;

		return true;
	}
	if (!strncmp(pr_file_p, "__DATE__", 8))
	{
		static char retbuf[256];
#ifdef WIN32
		SYSTEMTIME Systime;
		GetSystemTime(&Systime);
		//dayname, day, month, year
		sprintf(retbuf, "\"%s %i %s %i\"", daynames[Systime.wDayOfWeek], Systime.wDay, monthnames[Systime.wMonth], Systime.wYear);
#else	//linux
	sprintf(retbuf, "\"unknown date\"");
#endif
		pr_file_p = retbuf;
		QCC_PR_Lex();	//translate the macro's value
		pr_file_p = oldpr_file_p+8;

		return true;
	}
	if (!strncmp(pr_file_p, "__FILE__", 8))
	{		
		static char retbuf[256];
		sprintf(retbuf, "\"%s\"", strings + s_file);
		pr_file_p = retbuf;
		QCC_PR_Lex();	//translate the macro's value
		pr_file_p = oldpr_file_p+8;

		return true;
	}
	if (!strncmp(pr_file_p, "__LINE__", 8))
	{
		static char retbuf[256];
		sprintf(retbuf, "\"%i\"", pr_source_line);
		pr_file_p = retbuf;
		QCC_PR_Lex();	//translate the macro's value
		pr_file_p = oldpr_file_p+8;
		return true;
	}
	if (!strncmp(pr_file_p, "__FUNC__", 8))
	{		
		static char retbuf[256];
		sprintf(retbuf, "\"%s\"",pr_scope->name);
		pr_file_p = retbuf;
		QCC_PR_Lex();	//translate the macro's value
		pr_file_p = oldpr_file_p+8;
		return true;
	}
	if (!strncmp(pr_file_p, "__NULL__", 8))
	{
		static char retbuf[256];
		sprintf(retbuf, "~0");
		pr_file_p = retbuf;
		QCC_PR_Lex();	//translate the macro's value
		pr_file_p = oldpr_file_p+8;
		return true;
	}
	return false;
}

char *QCC_PR_CheakCompConstString(char *def)
{
	char *s;
	
	CompilerConstant_t *c;

	c = Hash_Get(&compconstantstable, def);

	if (c)	
	{
		s = QCC_PR_CheakCompConstString(c->value);
		return s;
	}
	return def;
}

CompilerConstant_t *QCC_PR_CheckCompConstDefined(char *def)
{
	CompilerConstant_t *c = Hash_Get(&compconstantstable, def);
	return c;
	/*int a;	
	for (a = 0; a < numCompilerConstants; a++)
	{
		if (!strncmp(def, CompilerConstant[a].name, CompilerConstant[a].namelen+1))		
			return &CompilerConstant[a];								
	}
	return NULL;
	*/
}

//============================================================================

/*
==============
PR_Lex

Sets pr_token, pr_token_type, and possibly pr_immediate and pr_immediate_type
==============
*/
void QCC_PR_Lex (void)
{
	int		c;

	pr_token[0] = 0;
	
	if (!pr_file_p)
	{
		if (QCC_PR_UnInclude())
		{	
			QCC_PR_Lex();
			return;
		}
		pr_token_type = tt_eof;
		return;
	}

	QCC_PR_LexWhitespace ();

	if (!pr_file_p)
	{
		if (QCC_PR_UnInclude())
		{	
			QCC_PR_Lex();
			return;
		}
		pr_token_type = tt_eof;
		return;
	}

	c = *pr_file_p;
		
	if (!c)
	{
		if (QCC_PR_UnInclude())
		{	
			QCC_PR_Lex();
			return;
		}
		pr_token_type = tt_eof;
		return;
	}

// handle quoted strings as a unit
	if (c == '\"')
	{
		QCC_PR_LexString ();
		return;
	}

// handle quoted vectors as a unit
	if (c == '\'')
	{
		QCC_PR_LexVector ();
		return;
	}

// if the first character is a valid identifier, parse until a non-id
// character is reached
	if ( c == '~' )
	{
		pr_file_p++;
		pr_token_type = tt_immediate;
		pr_immediate_type = type_integer;
		pr_immediate._int = QCC_PR_LexInteger ();
		return;
	}
	if ( c == '0' && pr_file_p[1] == 'x')
	{
		pr_token_type = tt_immediate;
		QCC_PR_LexNumber();
		return;
	}
	if ( (c == '.'&&pr_file_p[1] >='0' && pr_file_p[1] <= '9') || (c >= '0' && c <= '9') || ( c=='-' && pr_file_p[1]>='0' && pr_file_p[1] <='9') )
	{
		pr_token_type = tt_immediate;
		pr_immediate_type = type_float;
		pr_immediate._float = QCC_PR_LexFloat ();

//		pr_token_type = tt_immediate;
//		QCC_PR_LexNumber ();
		return;
	}

	if (c == '#' && !(pr_file_p[1]=='-' || (pr_file_p[1]>='0' && pr_file_p[1] <='9')))	//hash and not number
	{
		pr_file_p++;
		if (!QCC_PR_CheakCompConst())
		{
			if (!QCC_PR_SimpleGetToken())
				strcpy(pr_token, "unknown");
			QCC_PR_ParseError(ERR_CONSTANTNOTDEFINED, "Explicit precompiler usage when not defined %s", pr_token);
		}
		else
			if (pr_token_type == tt_eof)
				QCC_PR_Lex();

		return;
	}
	
	if ( (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' )
	{
		if (!QCC_PR_CheakCompConst())	//look for a macro.
			QCC_PR_LexName ();
		else
			if (pr_token_type == tt_eof)
			{
				if (QCC_PR_UnInclude())
				{	
					QCC_PR_Lex();
					return;
				}
				pr_token_type = tt_eof;
			}
		return;
	}
	
	if (c == '$')
	{
		QCC_PR_LexGrab ();
		return;
	}
	
// parse symbol strings until a non-symbol is found
	QCC_PR_LexPunctuation ();
}

//=============================================================================


void QCC_PR_ParsePrintDef (int type, QCC_def_t *def)
{
	if (qccwarningdisabled[type])
		return;
	if (def->s_file)
		printf ("%s:%i:    %s  is defined here\n", strings + def->s_file, def->s_line, def->name);
}

/*
============
PR_ParseError

Aborts the current file load
============
*/
#ifndef QCC
void editbadfile(char *file, int line);
#endif
void VARGS QCC_PR_ParseError (int errortype, char *error, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,error);
	QC_vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);

#ifndef QCC
	editbadfile(strings+s_file, pr_source_line);
#endif
	printf ("%s:%i: error: %s\n", strings + s_file, pr_source_line, string);
	
	longjmp (pr_parse_abort, 1);
}
void VARGS QCC_PR_ParseErrorPrintDef (int errortype, QCC_def_t *def, char *error, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,error);
	QC_vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);

#ifndef QCC
	editbadfile(strings+s_file, pr_source_line);
#endif
	printf ("%s:%i: error: %s\n", strings + s_file, pr_source_line, string);

	QCC_PR_ParsePrintDef(WARN_ERROR, def);
	
	longjmp (pr_parse_abort, 1);
}
void VARGS QCC_PR_ParseWarning (int type, char *error, ...)
{
	va_list		argptr;
	char		string[1024];

	if (type < ERR_PARSEERRORS && qccwarningdisabled[type])
		return;

	va_start (argptr,error);
	QC_vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);

	if (type >= ERR_PARSEERRORS)
	{
		printf ("%s:%i: error: %s\n", strings + s_file, pr_source_line, string);
		pr_error_count++;
	}
	else
		printf ("%s:%i: warning: %s\n", strings + s_file, pr_source_line, string);
}

void VARGS QCC_PR_Warning (int type, char *file, int line, char *error, ...)
{
	va_list		argptr;
	char		string[1024];

	if (qccwarningdisabled[type])
		return;

	va_start (argptr,error);
	QC_vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);

	if (file)
		printf ("%s:%i: warning: %s\n", file, line, string);
	else
		printf ("warning: %s\n", string);
}


/*
=============
PR_Expect

Issues an error if the current token isn't equal to string
Gets the next token
=============
*/
#ifndef COMMONINLINES
void QCC_PR_Expect (char *string)
{
	if (STRCMP (string, pr_token))
		QCC_PR_ParseError (ERR_EXPECTED, "expected %s, found %s",string, pr_token);
	QCC_PR_Lex ();
}
#endif


/*
=============
PR_Check

Returns true and gets the next token if the current token equals string
Returns false and does nothing otherwise
=============
*/
#ifndef COMMONINLINES
pbool QCC_PR_Check (char *string)
{
	if (STRCMP (string, pr_token))
		return false;
		
	QCC_PR_Lex ();
	return true;
}
#endif

/*
============
PR_ParseName

Checks to see if the current token is a valid name
============
*/
char *QCC_PR_ParseName (void)
{
	static char	ident[MAX_NAME];
	char *ret;
	
	if (pr_token_type != tt_name)
		QCC_PR_ParseError (ERR_NOTANAME, "\"%s\" - not a name", pr_token);	
	if (strlen(pr_token) >= MAX_NAME-1)
		QCC_PR_ParseError (ERR_NAMETOOLONG, "name too long");
	strcpy (ident, pr_token);
	QCC_PR_Lex ();
	
	ret = qccHunkAlloc(strlen(ident)+1);
	strcpy(ret, ident);
	return ret;
//	return ident;
}

/*
============
PR_FindType

Returns a preexisting complex type that matches the parm, or allocates
a new one and copies it out.
============
*/

//0 if same
QCC_type_t *QCC_PR_NewType (char *name, int basictype);
int typecmp(QCC_type_t *a, QCC_type_t *b)
{
	if (a == b)
		return 0;
	if (!a || !b)
		return 1;	//different (^ and not both null)

	if (a->type != b->type)
		return 1;
	if (a->num_parms != b->num_parms)
		return 1;

	if (a->size != b->size)
		return 1;
	if (STRCMP(a->name, b->name))
		return 1;

	if (typecmp(a->aux_type, b->aux_type))
		return 1;

	if (a->param || b->param)
	{
		a = a->param;
		b = b->param;

		while(a || b)
		{
			if (typecmp(a, b))
				return 1;

			a=a->next;
			b=b->next;
		}
	}

	return 0;
}

QCC_type_t *QCC_PR_DuplicateType(QCC_type_t *in)
{
	QCC_type_t *out, *op, *ip;
	if (!in)
		return NULL;

	out = QCC_PR_NewType(in->name, in->type);
	out->aux_type = QCC_PR_DuplicateType(in->aux_type);
	out->param = QCC_PR_DuplicateType(in->param);
	ip = in->param;
	op = NULL;
	while(ip)
	{
		if (!op)
			out->param = op = QCC_PR_DuplicateType(ip);
		else
			op = (op->next = QCC_PR_DuplicateType(ip));
		ip = ip->next;
	}
	out->size = in->size;
	out->num_parms = in->num_parms;
	out->ofs = in->ofs;
	out->name = in->name;
	out->parentclass = in->parentclass;

	return out;
}

char *TypeName(QCC_type_t *type)
{
	static char buffer[2][512];
	static int op;
	char *ret;


	op++;
	ret = buffer[op&1];
	if (type->type == ev_field)
	{
		type = type->aux_type;
		*ret++ = '.';
	}
	*ret = 0;

	if (type->type == ev_function)
	{
		strcat(ret, type->aux_type->name);
		strcat(ret, " (");
		type = type->param;
		while(type)
		{
			strcat(ret, type->name);
			type = type->next;

			if (type)
				strcat(ret, ", ");
		}
		strcat(ret, ")");
	}
	else if (type->type == ev_entity && type->parentclass)
	{
		op++;
		ret = buffer[op&1];
		*ret = 0;
		strcat(ret, "class ");
		strcat(ret, type->name);
/*		strcat(ret, " {");
		type = type->param;
		while(type)
		{
			strcat(ret, type->name);
			type = type->next;

			if (type)
				strcat(ret, ", ");
		}
		strcat(ret, "}");
*/
	}
	else
		strcpy(ret, type->name);

	return buffer[op&1];
}
//#define typecmp(a, b) (a && ((a)->type==(b)->type) && !STRCMP((a)->name, (b)->name))

QCC_type_t *QCC_PR_FindType (QCC_type_t *type)
{
	int t;
	for (t = 0; t < numtypeinfos; t++)
	{
//		check = &qcc_typeinfo[t];
		if (typecmp(&qcc_typeinfo[t], type))
			continue;
		

//		c2 = check->next;
//		n2 = type->next;
//		for (i=0 ; n2&&c2 ; i++)
//		{
//			if (!typecmp((c2), (n2)))
//				break;
//			c2=c2->next;
//			n2=n2->next;
//		}

//		if (n2==NULL&&c2==NULL)
		{
			return &qcc_typeinfo[t];
		}
	}
QCC_Error(ERR_INTERNAL, "Error with type");

	return type;
}
/*
QCC_type_t *QCC_PR_NextSubType(QCC_type_t *type, QCC_type_t *prev)
{
	int p;
	if (!prev)
		return type->next;

	for (p = prev->num_parms; p; p--)
		prev = QCC_PR_NextSubType(prev, NULL);
	if (prev->num_parms)

	switch(prev->type)
	{
	case ev_function:

	}

	return prev->next;
}
*/

QCC_type_t *QCC_TypeForName(char *name)
{
	int i;

	for (i = 0; i < numtypeinfos; i++)
	{
		if (!STRCMP(qcc_typeinfo[i].name, name))
		{
			return &qcc_typeinfo[i];
		}
	}

	return NULL;
}

/*
============
PR_SkipToSemicolon

For error recovery, also pops out of nested braces
============
*/
void QCC_PR_SkipToSemicolon (void)
{
	do
	{
		if (!pr_bracelevel && QCC_PR_Check (";"))
			return;
		QCC_PR_Lex ();
	} while (pr_token_type != tt_eof);
}


/*
============
PR_ParseType

Parses a variable type, including field and functions types
============
*/
#ifdef MAX_EXTRA_PARMS
char	pr_parm_names[MAX_PARMS+MAX_EXTRA_PARMS][MAX_NAME];
#else
char	pr_parm_names[MAX_PARMS][MAX_NAME];
#endif

pbool recursivefunctiontype;

QCC_type_t *QCC_PR_NewType (char *name, int basictype);
//expects a ( to have already been parsed.
QCC_type_t *QCC_PR_ParseFunctionType (int newtype, QCC_type_t *returntype)
{
	QCC_type_t	*ftype, *ptype, *nptype;
	char	*name;
	int definenames = !recursivefunctiontype;

	recursivefunctiontype++;

	ftype = QCC_PR_NewType(type_function->name, ev_function);

	ftype->aux_type = returntype;	// return type
	ftype->num_parms = 0;
	ptype = NULL;


	if (!QCC_PR_Check (")"))
	{
		if (QCC_PR_Check ("..."))
			ftype->num_parms = -1;	// variable args
		else
			do
			{
				if (ftype->num_parms>=MAX_PARMS+MAX_EXTRA_PARMS)
					QCC_PR_ParseError(ERR_TOOMANYTOTALPARAMETERS, "Too many parameters. Sorry. (limit is %i)\n", MAX_PARMS+MAX_EXTRA_PARMS);

				if (QCC_PR_Check ("..."))
				{
					ftype->num_parms = (ftype->num_parms * -1) - 1;
					break;
				}

				nptype = QCC_PR_ParseType(true);

				if (nptype->type == ev_void)
					break;
				if (!ptype)
				{
					ptype = nptype;
					ftype->param = ptype;
				}
				else
				{
					ptype->next = nptype;
					ptype = ptype->next;
				}
//				type->name = "FUNC PARAMETER";


				if (STRCMP(pr_token, ",") && STRCMP(pr_token, ")"))
				{
					name = QCC_PR_ParseName ();
					if (definenames)
						strcpy (pr_parm_names[ftype->num_parms], name);
				}
				else if (definenames)
					strcpy (pr_parm_names[ftype->num_parms], "");
				ftype->num_parms++;
			} while (QCC_PR_Check (","));
	
		QCC_PR_Expect (")");
	}
	recursivefunctiontype--;
	if (newtype)
		return ftype;
	return QCC_PR_FindType (ftype);
}
QCC_type_t *QCC_PR_ParseType (int newtype)
{
	QCC_type_t	*newparm;
	QCC_type_t	*newt;
	QCC_type_t	*type;
	char	*name;
	int i;

//	int ofs;

	if (QCC_PR_Check ("."))
	{
		newt = QCC_PR_NewType("FIELD TYPE", ev_field);
		newt->aux_type = QCC_PR_ParseType (false);

		newt->size = newt->aux_type->size;

		if (newtype)
			return newt;
		return QCC_PR_FindType (newt);
	}

	name = QCC_PR_CheakCompConstString(pr_token);

	if (QCC_PR_Check ("class"))
	{
//		int parms;
		QCC_type_t *fieldtype;
		char membername[2048];
		char *classname = QCC_PR_ParseName();
		newt = QCC_PR_NewType(classname, ev_entity);
		newt->size=type_entity->size;

		type = NULL;

		if (QCC_PR_Check(":"))
		{
			char *parentname = QCC_PR_ParseName();
			newt->parentclass = QCC_TypeForName(parentname);
			if (!newt->parentclass)
				QCC_PR_ParseError(ERR_NOTANAME, "Parent class %s was not defined", parentname);
		}
		else
			newt->parentclass = type_entity;


		QCC_PR_Expect("{");
		if (QCC_PR_Check(","))
			QCC_PR_ParseError(ERR_NOTANAME, "member missing name");
		while (!QCC_PR_Check("}"))
		{
//			if (QCC_PR_Check(","))
//				type->next = QCC_PR_NewType(type->name, type->type);
//			else
				newparm = QCC_PR_ParseType(true);

			if (newparm->type == ev_struct || newparm->type == ev_union)	//we wouldn't be able to handle it.
				QCC_PR_ParseError(ERR_INTERNAL, "Struct or union in class %s", classname);

			if (!QCC_PR_Check(";"))
			{
				newparm->name = QCC_CopyString(pr_token)+strings;
				QCC_PR_Lex();
				if (QCC_PR_Check("["))
				{
					type->next->size*=atoi(pr_token);
					QCC_PR_Lex();
					QCC_PR_Expect("]");
				}
				QCC_PR_Check(";");
			}
			else
				newparm->name = QCC_CopyString("")+strings;

			sprintf(membername, "%s::"MEMBERFIELDNAME, classname, newparm->name);
			fieldtype = QCC_PR_NewType(newparm->name, ev_field);
			fieldtype->aux_type = newparm;
			QCC_PR_GetDef(fieldtype, membername, pr_scope, 2, 1);


			newparm->ofs = newt->size;
			newt->num_parms++;

			if (type)
				type->next = newparm;
			else
				newt->param = newparm;

			type = newparm;
		}


		QCC_PR_Expect(";");
		return NULL;
	}
	if (QCC_PR_Check ("struct"))
	{
		newt = QCC_PR_NewType("struct", ev_struct);
		newt->size=0;
		QCC_PR_Expect("{");

		type = NULL;
		if (QCC_PR_Check(","))
			QCC_PR_ParseError(ERR_NOTANAME, "element missing name");

		newparm = NULL;
		while (!QCC_PR_Check("}"))
		{
			if (QCC_PR_Check(","))
			{
				if (!newparm)
					QCC_PR_ParseError(ERR_NOTANAME, "element missing type");
				newparm = QCC_PR_NewType(newparm->name, newparm->type);
			}
			else
				newparm = QCC_PR_ParseType(true);

			if (!QCC_PR_Check(";"))
			{
				newparm->name = QCC_CopyString(pr_token)+strings;
				QCC_PR_Lex();
				if (QCC_PR_Check("["))
				{
					newparm->size*=atoi(pr_token);
					QCC_PR_Lex();
					QCC_PR_Expect("]");
				}
				QCC_PR_Check(";");
			}
			else
				newparm->name = QCC_CopyString("")+strings;
			newparm->ofs = newt->size;
			newt->size += newparm->size;
			newt->num_parms++;

			if (type)
				type->next = newparm;
			else
				newt->param = newparm;
			type = newparm;
		}
		return newt;
	}
	if (QCC_PR_Check ("union"))
	{
		newt = QCC_PR_NewType("union", ev_union);
		newt->size=0;
		QCC_PR_Expect("{");
		
		type = NULL;
		if (QCC_PR_Check(","))
			QCC_PR_ParseError(ERR_NOTANAME, "element missing name");
		newparm = NULL;
		while (!QCC_PR_Check("}"))
		{
			if (QCC_PR_Check(","))
			{
				if (!newparm)
					QCC_PR_ParseError(ERR_NOTANAME, "element missing type");
				newparm = QCC_PR_NewType(newparm->name, newparm->type);
			}
			else
				newparm = QCC_PR_ParseType(true);
			if (QCC_PR_Check(";"))
				newparm->name = QCC_CopyString("")+strings;
			else
			{
				newparm->name = QCC_CopyString(pr_token)+strings;
				QCC_PR_Lex();
				QCC_PR_Expect(";");
			}
			newparm->ofs = 0;
			if (newparm->size > newt->size)
				newt->size = newparm->size;
			newt->num_parms++;
			
			if (type)
				type->next = newparm;
			else
				newt->param = newparm;
			type = newparm;
		}
		return newt;
	}
	type = NULL;
	for (i = 0; i < numtypeinfos; i++)
	{
		if (!STRCMP(qcc_typeinfo[i].name, name))
		{
			type = &qcc_typeinfo[i];
			break;
		}
	}

	if (i == numtypeinfos)
	{
		QCC_PR_ParseError (ERR_NOTATYPE, "\"%s\" is not a type", name);
		type = type_float;	// shut up compiler warning
	}
	QCC_PR_Lex ();
	
	if (QCC_PR_Check ("("))	//this is followed by parameters. Must be a function.
		return QCC_PR_ParseFunctionType(newtype, type);
	else
	{
		if (newtype)
		{
			type = QCC_PR_DuplicateType(type);			
		}

		return type;
	}
}


#endif

