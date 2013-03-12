#ifndef MINIMAL

#include "qcc.h"
#ifdef QCC
#define print printf
#endif
#include "time.h"

#ifdef _WIN64
        #ifdef _SDL
                #define snprintf linuxlike_snprintf
                int VARGS linuxlike_snprintf(char *buffer, int size, const char *format, ...) LIKEPRINTF(3);
                #define vsnprintf linuxlike_vsnprintf
                int VARGS linuxlike_vsnprintf(char *buffer, int size, const char *format, va_list argptr);
                //void *__imp__vsnprintf = vsnprintf;
        #endif
#endif

#define MEMBERFIELDNAME "__m%s"

#define STRCMP(s1,s2) (((*s1)!=(*s2)) || strcmp(s1+1,s2+1))	//saves about 2-6 out of 120 - expansion of idea from fastqcc

void QCC_PR_ConditionCompilation(void);
pbool QCC_PR_UndefineName(char *name);
char *QCC_PR_CheckCompConstString(char *def);
CompilerConstant_t *QCC_PR_CheckCompConstDefined(char *def);
pbool QCC_Include(char *filename);

char *compilingfile;

int			pr_source_line;

char		*pr_file_p;
char		*pr_line_start;		// start of current source line

int			pr_bracelevel;

char		pr_token[8192];
token_type_t	pr_token_type;
int				pr_token_line;
int				pr_token_line_last;
QCC_type_t		*pr_immediate_type;
QCC_eval_t		pr_immediate;

char	pr_immediate_string[8192];

int		pr_error_count;
int		pr_warning_count;


CompilerConstant_t *CompilerConstant;
int numCompilerConstants;
extern pbool expandedemptymacro;



char	*pr_punctuation[] =
// longer symbols must be before a shorter partial match
{"&&", "||", "<=", ">=","==", "!=", "/=", "*=", "+=", "-=", "(+)", "(-)", "|=", "&~=", "++", "--", "->", "::", ";", ",", "!", "*", "/", "(", ")", "-", "+", "=", "[", "]", "{", "}", "...", "..", ".", "<<", "<", ">>", ">" , "?", "#" , "@", "&" , "|", "^", "~", ":", NULL};

char *pr_punctuationremap[] =	//a nice bit of evilness.
//(+) -> |=
//-> -> .
//(-) -> &~=
{"&&", "||", "<=", ">=","==", "!=", "/=", "*=", "+=", "-=", "|=",  "&~=", "|=", "&~=", "++", "--", ".", "::", ";", ",", "!", "*", "/", "(", ")", "-", "+", "=", "[", "]", "{", "}", "...", "..", ".", "<<", "<", ">>", ">" , "?", "#" , "@", "&" , "|", "^", "~", ":", NULL};

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
QCC_type_t	*type_variant;// = {ev_integer/*, &def_integer*/};
QCC_type_t	*type_floatpointer;
QCC_type_t	*type_intpointer;

QCC_type_t	*type_floatfield;// = {ev_field/*, &def_field*/, NULL, &type_float};

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

qcc_includechunk_t *currentchunk;
void QCC_PR_CloseProcessor(void)
{
	currentchunk = NULL;
}
void QCC_PR_IncludeChunkEx (char *data, pbool duplicate, char *filename, CompilerConstant_t *cnst)
{
	qcc_includechunk_t *chunk = qccHunkAlloc(sizeof(qcc_includechunk_t));
	chunk->prev = currentchunk;
	currentchunk = chunk;

	chunk->currentdatapoint = pr_file_p;
	chunk->currentlinenumber = pr_source_line;
	chunk->cnst = cnst;
	if( cnst )
	{
		cnst->inside++;
	}

	if (duplicate)
	{
		pr_file_p = qccHunkAlloc(strlen(data)+1);
		strcpy(pr_file_p, data);
	}
	else
		pr_file_p = data;
}
void QCC_PR_IncludeChunk (char *data, pbool duplicate, char *filename)
{
	QCC_PR_IncludeChunkEx(data, duplicate, filename, NULL);
}

pbool QCC_PR_UnInclude(void)
{
	if (!currentchunk)
		return false;

	if( currentchunk->cnst )
		currentchunk->cnst->inside--;

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

extern char qccmsourcedir[];
//also meant to include it.
void QCC_FindBestInclude(char *newfile, char *currentfile, char *rootpath)
{
	char fullname[1024];
	int doubledots;

	char *end = fullname;

	if (!*newfile)
		return;

	doubledots = 0;
	/*count how far up we need to go*/
	while(!strncmp(newfile, "../", 3) || !strncmp(newfile, "..\\", 3))
	{
		newfile+=3;
		doubledots++;
	}

#if 0
	currentfile += strlen(rootpath);	//could this be bad?
	strcpy(fullname, rootpath);

	end = fullname+strlen(end);
	if (*fullname && end[-1] != '/')
	{
		strcpy(end, "/");
		end = end+strlen(end);
	}
	strcpy(end, currentfile);
	end = end+strlen(end);
#else
	if (currentfile)
		strcpy(fullname, currentfile);
	else
		*fullname = 0;
	end = fullname+strlen(fullname);
#endif

	while (end > fullname)
	{
		end--;
		/*stop at the slash, unless we're meant to go further*/
		if (*end == '/' || *end == '\\')
		{
			if (!doubledots)
			{
				end++;
				break;
			}
			doubledots--;
		}
	}

	strcpy(end, newfile);

	QCC_Include(fullname);
}

pbool defaultnoref;
pbool defaultstatic;
int ForcedCRC;
int QCC_PR_LexInteger (void);
void	QCC_AddFile (char *filename);
void QCC_PR_LexString (void);
pbool QCC_PR_SimpleGetToken (void);

#define PPI_VALUE 0
#define PPI_NOT 1
#define PPI_DEFINED 2
#define PPI_COMPARISON 3
#define PPI_LOGICAL 4
#define PPI_TOPLEVEL 5
int ParsePrecompilerIf(int level)
{
	CompilerConstant_t *c;
	int eval = 0;
//	pbool notted = false;

	//single term end-of-chain
	if (level == PPI_VALUE)
	{
		/*skip whitespace*/
		while (*pr_file_p && qcc_iswhite(*pr_file_p) && *pr_file_p != '\n')
		{
			pr_file_p++;
		}

		if (*pr_file_p == '(')
		{	//try brackets
			pr_file_p++;
			eval = ParsePrecompilerIf(PPI_TOPLEVEL);
			while (*pr_file_p == ' ' || *pr_file_p == '\t')
				pr_file_p++;
			if (*pr_file_p != ')')
				QCC_PR_ParseError(ERR_EXPECTED, "unclosed bracket condition\n");
			pr_file_p++;
		}
		else if (*pr_file_p == '!')
		{	//try brackets
			pr_file_p++;
			eval = !ParsePrecompilerIf(PPI_NOT);
		}
		else
		{	//simple token...
			if (!strncmp(pr_file_p, "defined", 7))
			{
				pr_file_p+=7;
				while (*pr_file_p == ' ' || *pr_file_p == '\t')
					pr_file_p++;
				if (*pr_file_p != '(')
				{
					eval = false;
					QCC_PR_ParseError(ERR_EXPECTED, "no opening bracket after defined\n");
				}
				else
				{
					pr_file_p++;

					QCC_PR_SimpleGetToken();
					eval = !!QCC_PR_CheckCompConstDefined(pr_token);

					while (*pr_file_p == ' ' || *pr_file_p == '\t')
						pr_file_p++;
					if (*pr_file_p != ')')
						QCC_PR_ParseError(ERR_EXPECTED, "unclosed defined condition\n");
					pr_file_p++;
				}
			}
			else
			{
				if (!QCC_PR_SimpleGetToken())
					QCC_PR_ParseError(ERR_EXPECTED, "unexpected end-of-line\n");
				c = QCC_PR_CheckCompConstDefined(pr_token);
				if (!c)
					eval = atoi(pr_token);
				else
					eval = atoi(c->value);
			}
		}
		return eval;
	}

	eval = ParsePrecompilerIf(level-1);

	while (*pr_file_p && qcc_iswhite(*pr_file_p) && *pr_file_p != '\n')
	{
		pr_file_p++;
	}

	switch(level)
	{
	case PPI_LOGICAL:
		if (!strncmp(pr_file_p, "||", 2))
		{
			pr_file_p+=2;
			eval = ParsePrecompilerIf(level)||eval;
		}
		else if (!strncmp(pr_file_p, "&&", 2))
		{
			pr_file_p+=2;
			eval = ParsePrecompilerIf(level)&&eval;
		}
		break;
	case PPI_COMPARISON:
		if (!strncmp(pr_file_p, "<=", 2))
		{
			pr_file_p+=2;
			eval = eval <= ParsePrecompilerIf(level);
		}
		else if (!strncmp(pr_file_p, ">=", 2))
		{
			pr_file_p += 2;
			eval = eval >= ParsePrecompilerIf(level);
		}
		else if (!strncmp(pr_file_p, "<", 1))
		{
			pr_file_p += 1;
			eval = eval < ParsePrecompilerIf(level);
		}
		else if (!strncmp(pr_file_p, ">", 1))
		{
			pr_file_p += 1;
			eval = eval > ParsePrecompilerIf(level);
		}
		else if (!strncmp(pr_file_p, "!=", 2))
		{
			pr_file_p += 2;
			eval = eval != ParsePrecompilerIf(level);
		}
		else if (!strncmp(pr_file_p, "==", 2))
		{
			pr_file_p += 2;
			eval = eval == ParsePrecompilerIf(level);
		}
		break;
	}
	return eval;
}

static void QCC_PR_SkipToEndOfLine(void)
{
	pbool handlecomments = true;
	while(*pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
	{
		if (*pr_file_p == '/' && pr_file_p[1] == '*' && handlecomments)
		{
			pr_file_p += 2;
			while(*pr_file_p)
			{
				if (*pr_file_p == '*' && pr_file_p[1] == '/')
				{
					pr_file_p+=2;
					break;
				}
				if (*pr_file_p == '\n')
					pr_source_line++;
				pr_file_p++;
			}
		}
		else if (*pr_file_p == '/' && pr_file_p[1] == '/' && handlecomments)
		{
			handlecomments = false;
			pr_file_p += 2;
		}
		else if (*pr_file_p == '\\' && pr_file_p[1] == '\r' && pr_file_p[2] == '\n')
		{	/*windows endings*/
			pr_file_p+=3;
			pr_source_line++;
		}
		else if (*pr_file_p == '\\' && pr_file_p[1] == '\n')
		{	/*linux endings*/
			pr_file_p+=2;

			pr_source_line++;
		}
		else
			pr_file_p++;
	}
}
/*
==============
QCC_PR_Precompiler
==============

Runs precompiler stage
*/
pbool QCC_PR_Precompiler(void)
{
	char msg[1024];
	int ifmode;
	int a;
	static int ifs = 0;
	int level;	//#if level
	pbool eval = false;

	if (*pr_file_p == '#')
	{
		char *directive;
		for (directive = pr_file_p+1; *directive; directive++)	//so #    define works
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
			QCC_PR_SkipToEndOfLine();
		}
		else if (!strncmp(directive, "undef", 5))
		{
			pr_file_p = directive+5;
			while(qcc_iswhitesameline(*pr_file_p))
				pr_file_p++;

			QCC_PR_SimpleGetToken ();
			QCC_PR_UndefineName(pr_token);

	//		QCC_PR_ConditionCompilation();
			QCC_PR_SkipToEndOfLine();
		}
		else if (!strncmp(directive, "if", 2))
		{
			int originalline = pr_source_line;
 			pr_file_p = directive+2;
			if (!strncmp(pr_file_p, "def", 3))
			{
				ifmode = 0;
				pr_file_p+=3;
			}
			else if (!strncmp(pr_file_p, "ndef", 4))
			{
				ifmode = 1;
				pr_file_p+=4;
			}
			else
			{
				ifmode = 2;
				pr_file_p+=0;
				//QCC_PR_ParseError("bad \"#if\" type");
			}

			if (!qcc_iswhite(*pr_file_p))
			{
				pr_file_p = directive;
				QCC_PR_SimpleGetToken ();
				QCC_PR_ParseWarning(WARN_BADPRAGMA, "Unknown pragma \'%s\'", qcc_token);
			}
			else
			{
				if (ifmode == 2)
				{
					eval = ParsePrecompilerIf(PPI_TOPLEVEL);

					if(*pr_file_p != '\r' && *pr_file_p != '\n' && *pr_file_p != '\0')	//read on until the end of the line
					{
						QCC_PR_ParseError (ERR_NOENDIF, "junk on the end of #if line");
					}
				}
				else
				{
					QCC_PR_SimpleGetToken ();

		//			if (!STRCMP(pr_token, "COOP_MODE"))
		//				eval = false;
					if (QCC_PR_CheckCompConstDefined(pr_token))
						eval = true;

					if (ifmode == 1)
						eval = eval?false:true;
				}

				QCC_PR_SkipToEndOfLine();
				level = 1;

				if (eval)
					ifs+=1;
				else
				{
					while (1)
					{
						while(*pr_file_p && (*pr_file_p==' ' || *pr_file_p == '\t'))
							pr_file_p++;

						if (!*pr_file_p)
						{
							pr_source_line = originalline;
							QCC_PR_ParseError (ERR_NOENDIF, "#if with no endif");
						}

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
								QCC_PR_SkipToEndOfLine();
								break;
							}
						}

						QCC_PR_SkipToEndOfLine();

						if (level <= 0)
							break;
						pr_file_p++;	//next line
						pr_source_line++;
					}
				}
			}
		}
		else if (!strncmp(directive, "else", 4))
		{
			int originalline = pr_source_line;

			ifs -= 1;
			level = 1;

			QCC_PR_SkipToEndOfLine();
			while (1)
			{
				while(*pr_file_p && (*pr_file_p==' ' || *pr_file_p == '\t'))
					pr_file_p++;

				if (!*pr_file_p)
				{
					pr_source_line = originalline;
					QCC_PR_ParseError(ERR_NOENDIF, "#if with no endif");
				}

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

				QCC_PR_SkipToEndOfLine();
				if (level <= 0)
					break;
				pr_file_p++;	//go off the end
				pr_source_line++;
			}
		}
		else if (!strncmp(directive, "endif", 5))
		{
			QCC_PR_SkipToEndOfLine();
			if (ifs <= 0)
				QCC_PR_ParseError(ERR_NOPRECOMPILERIF, "unmatched #endif");
			else
				ifs-=1;
		}
		else if (!strncmp(directive, "eof", 3))
		{
			pr_file_p = NULL;
			return true;
		}
		else if (!strncmp(directive, "error", 5))
		{
			pr_file_p = directive+5;
			for (a = 0; a < sizeof(msg)-1 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a] = '\0';

			QCC_PR_SkipToEndOfLine();

			QCC_PR_ParseError(ERR_HASHERROR, "#Error: %s", msg);
		}
		else if (!strncmp(directive, "warning", 7))
		{
			pr_file_p = directive+7;
			for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';

			QCC_PR_SkipToEndOfLine();

			QCC_PR_ParseWarning(WARN_PRECOMPILERMESSAGE, "#warning: %s", msg);
		}
		else if (!strncmp(directive, "message", 7))
		{
			pr_file_p = directive+7;
			for (a = 0; a < 1023 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';

			QCC_PR_SkipToEndOfLine();

			printf("#message: %s\n", msg);
		}
		else if (!strncmp(directive, "copyright", 9))
		{
			pr_file_p = directive+9;
			for (a = 0; a < sizeof(msg)-1 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';

			QCC_PR_SkipToEndOfLine();

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
			for (a = 0; a < sizeof(msg)-1 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
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

			for (a = 0; a < sizeof(msg)-1 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
				msg[a] = pr_file_p[a];

			msg[a-1] = '\0';

			QCC_PR_SkipToEndOfLine();
		}
		else if (!strncmp(directive, "includelist", 11))
		{
			pr_file_p=directive+11;

			while(qcc_iswhite(*pr_file_p))
			{
				if (*pr_file_p == '\n')
					pr_source_line++;
				pr_file_p++;
			}

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

				QCC_FindBestInclude(pr_token, compilingfile, qccmsourcedir);

				if (*pr_file_p == '\r')
					pr_file_p++;

				QCC_PR_SkipToEndOfLine();
			}

			QCC_PR_SkipToEndOfLine();
		}
		else if (!strncmp(directive, "include", 7))
		{
			char sm;

			pr_file_p=directive+7;

			while(qcc_iswhitesameline(*pr_file_p))
				pr_file_p++;

			msg[0] = '\0';
			if (*pr_file_p == '\"')
				sm = '\"';
			else if (*pr_file_p == '<')
				sm = '>';
			else
			{
				QCC_PR_ParseError(0, "Not a string literal (on a #include)");
				sm = 0;
			}
			pr_file_p++;
			a=0;
			while(*pr_file_p != sm)
			{
				if (*pr_file_p == '\n')
				{
					QCC_PR_ParseError(0, "#include continued over line boundry\n");
					break;
				}
				msg[a++] = *pr_file_p;
				pr_file_p++;
			}
			msg[a] = 0;

			QCC_FindBestInclude(msg, compilingfile, qccmsourcedir);

			pr_file_p++;

			while(*pr_file_p != '\n' && *pr_file_p != '\0' && qcc_iswhitesameline(*pr_file_p))
				pr_file_p++;


			QCC_PR_SkipToEndOfLine();
		}
		else if (!strncmp(directive, "datafile", 8))
		{
			pr_file_p=directive+8;

			while(qcc_iswhitesameline(*pr_file_p))
				pr_file_p++;

			QCC_PR_LexString();
			printf("Including datafile: %s\n", pr_token);
			QCC_AddFile(pr_token);

			pr_file_p++;

			for (a = 0; a < sizeof(msg)-1 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
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

			while(qcc_iswhitesameline(*pr_file_p))
				pr_file_p++;

			QCC_PR_LexString();
			strcpy(destfile, pr_token);
			printf("Outputfile: %s\n", destfile);

			pr_file_p++;

			for (a = 0; a < sizeof(msg)-1 && pr_file_p[a] != '\n' && pr_file_p[a] != '\0'; a++)
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
			while(qcc_iswhitesameline(*pr_file_p))
				pr_file_p++;

			qcc_token[0] = '\0';
			for(a = 0; *pr_file_p != '\n' && *pr_file_p != '\0'; pr_file_p++)	//read on until the end of the line
			{
				if ((*pr_file_p == ' ' || *pr_file_p == '\t'|| *pr_file_p == '(') && !*qcc_token)
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
				for (end = msg + a-1; end>=msg && qcc_iswhite(*end); end--)
					*end = '\0';
			}

			if (!*qcc_token)
			{
				strcpy(qcc_token, msg);
				msg[0] = '\0';
			}

			{
				char *end;
				for (end = msg + a-1; end>=msg && qcc_iswhite(*end); end--)
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
						pr_file_p++;
						QCC_PR_NewLine(false);
					}
				}
			}
			else if (!QC_strcasecmp(qcc_token, "COPYRIGHT"))
			{
				if (strlen(msg) >= sizeof(QCC_copyright))
					QCC_PR_ParseWarning(WARN_STRINGTOOLONG, "Copyright message is too long\n");
				strncpy(QCC_copyright, msg, sizeof(QCC_copyright)-1);
			}
			else if (!strncmp(qcc_token, "compress", 8))
			{
				extern pbool compressoutput;
				compressoutput = atoi(msg);
			}
			else if (!strncmp(qcc_token, "forcecrc", 8))
			{
				ForcedCRC = atoi(msg);
			}
			else if (!strncmp(qcc_token, "noref", 8))
			{
				defaultnoref = atoi(msg);
			}
			else if (!strncmp(qcc_token, "defaultstatic", 13))
			{
				defaultstatic = atoi(msg);
			}
			else if (!strncmp(qcc_token, "wrasm", 5))
			{
				pbool on = atoi(msg);

				if (asmfile && !on)
				{
					fclose(asmfile);
					asmfile = NULL;
				}			
				if (!asmfile && on)
					asmfile = fopen("qc.asm", "wb");
			}
			else if (!strncmp(qcc_token, "sourcefile", 10))
			{
	#define MAXSOURCEFILESLIST 8
	extern char sourcefileslist[MAXSOURCEFILESLIST][1024];
	//extern int currentsourcefile; // warning: unused variable âcurrentsourcefileâ
	extern int numsourcefiles;

				int i;

				QCC_COM_Parse(msg);

				for (i = 0; i < numsourcefiles; i++)
				{
					if (!strcmp(sourcefileslist[i], qcc_token))
						break;
				}
				if (i == numsourcefiles && numsourcefiles < MAXSOURCEFILESLIST)
					strcpy(sourcefileslist[numsourcefiles++], qcc_token);
			}
			else if (!QC_strcasecmp(qcc_token, "TARGET"))
			{
				int newtype = qcc_targetformat;
				if (!QC_strcasecmp(msg, "H2") || !QC_strcasecmp(msg, "HEXEN2"))
					newtype = QCF_HEXEN2;
				else if (!QC_strcasecmp(msg, "KK7"))
					newtype = QCF_KK7;
				else if (!QC_strcasecmp(msg, "DP") || !QC_strcasecmp(msg, "DARKPLACES"))
					newtype = QCF_DARKPLACES;
				else if (!QC_strcasecmp(msg, "FTEDEBUG"))
					newtype = QCF_FTEDEBUG;
				else if (!QC_strcasecmp(msg, "FTE"))
					newtype = QCF_FTE;
				else if (!QC_strcasecmp(msg, "FTEH2"))
					newtype = QCF_FTEH2;
				else if (!QC_strcasecmp(msg, "STANDARD") || !QC_strcasecmp(msg, "ID"))
					newtype = QCF_STANDARD;
				else if (!QC_strcasecmp(msg, "DEBUG"))
					newtype = QCF_FTEDEBUG;
				else if (!QC_strcasecmp(msg, "QTEST"))
					newtype = QCF_QTEST;
				else
					QCC_PR_ParseWarning(WARN_BADTARGET, "Unknown target \'%s\'. Ignored.", msg);

				if (numstatements > 1)
				{
					if ((qcc_targetformat == QCF_HEXEN2 || qcc_targetformat == QCF_FTEH2) && (newtype != QCF_HEXEN2 && newtype != QCF_FTEH2))
						QCC_PR_ParseWarning(WARN_BADTARGET, "Cannot switch from hexen2 target \'%s\'. Ignored.", msg);
					if ((newtype == QCF_HEXEN2 || newtype == QCF_FTEH2) && (qcc_targetformat != QCF_HEXEN2 && qcc_targetformat != QCF_FTEH2))
						QCC_PR_ParseWarning(WARN_BADTARGET, "Cannot switch to hexen2 target \'%s\'. Ignored.", msg);
				}

				qcc_targetformat = newtype;
			}
			else if (!QC_strcasecmp(qcc_token, "PROGS_SRC"))
			{	//doesn't make sence, but silenced if you are switching between using a certain precompiler app used with CuTF.
			}
			else if (!QC_strcasecmp(qcc_token, "PROGS_DAT"))
			{	//doesn't make sence, but silenced if you are switching between using a certain precompiler app used with CuTF.
				extern char		destfile[1024];
#ifndef QCCONLY
				extern char qccmfilename[1024];
				int p;
				char *s, *s2;
#endif
				QCC_COM_Parse(msg);

#ifndef QCCONLY
	p=0;
	s2 = qcc_token;
	if (!strncmp(s2, "./", 2))
		s2+=2;
	else
	{
		while(!strncmp(s2, "../", 3))
		{
			s2+=3;
			p++;
		}
	}
	strcpy(qccmfilename, qccmsourcedir);
	for (s=qccmfilename+strlen(qccmfilename);p && s>=qccmfilename; s--)
	{
		if (*s == '/' || *s == '\\')
		{
			*(s+1) = '\0';
			p--;
		}
	}
	sprintf(destfile, "%s", s2);

	while (p>0)
	{
		memmove(destfile+3, destfile, strlen(destfile)+1);
		destfile[0] = '.';
		destfile[1] = '.';
		destfile[2] = '/';
		p--;
	}
#else

				strcpy(destfile, qcc_token);
#endif
				printf("Outputfile: %s\n", destfile);
			}
			else if (!QC_strcasecmp(qcc_token, "keyword") || !QC_strcasecmp(qcc_token, "flag"))
			{
				char *s;
				int st;
				s = QCC_COM_Parse(msg);
				if (!QC_strcasecmp(qcc_token, "enable") || !QC_strcasecmp(qcc_token, "on"))
					st = 1;
				else if (!QC_strcasecmp(qcc_token, "disable") || !QC_strcasecmp(qcc_token, "off"))
					st = 0;
				else
				{
					QCC_PR_ParseWarning(WARN_BADPRAGMA, "compiler flag state not recognised");
					st = -1;
				}
				if (st < 0)
					QCC_PR_ParseWarning(WARN_BADPRAGMA, "warning id not recognised");
				else
				{
					int f;
					s = QCC_COM_Parse(s);

					for (f = 0; compiler_flag[f].enabled; f++)
					{
						if (!QC_strcasecmp(compiler_flag[f].abbrev, qcc_token))
						{
							if (compiler_flag[f].flags & FLAG_MIDCOMPILE)
								*compiler_flag[f].enabled = st;
							else
								QCC_PR_ParseWarning(WARN_BADPRAGMA, "Cannot enable/disable keyword/flag via a pragma");
							break;
						}
					}
					if (!compiler_flag[f].enabled)
						QCC_PR_ParseWarning(WARN_BADPRAGMA, "keyword/flag not recognised");

				}
			}
			else if (!QC_strcasecmp(qcc_token, "warning"))
			{
				int st;
				char *s;
				s = QCC_COM_Parse(msg);
				if (!stricmp(qcc_token, "enable") || !stricmp(qcc_token, "on"))
					st = WA_WARN;
				else if (!stricmp(qcc_token, "disable") || !stricmp(qcc_token, "off") || !stricmp(qcc_token, "ignore"))
					st = WA_IGNORE;
				else if (!stricmp(qcc_token, "error"))
					st = WA_ERROR;
				else if (!stricmp(qcc_token, "toggle"))
					st = 3;
				else
				{
					QCC_PR_ParseWarning(WARN_BADPRAGMA, "warning state not recognised");
					st = -1;
				}
				if (st>=0)
				{
					int wn;
					s = QCC_COM_Parse(s);
					wn = QCC_WarningForName(qcc_token);
					if (wn < 0)
						QCC_PR_ParseWarning(WARN_BADPRAGMA, "warning id not recognised");
					else
					{
						if (st == 3)	//toggle
							qccwarningaction[wn] = !!qccwarningaction[wn];
						else
							qccwarningaction[wn] = st;
					}
				}
			}
			else
				QCC_PR_ParseWarning(WARN_BADPRAGMA, "Unknown pragma \'%s\'", qcc_token);
		}
		return true;
	}

	return false;
}

/*
==============
PR_NewLine

Call at start of file and when *pr_file_p == '\n'
==============
*/
void QCC_PR_NewLine (pbool incomment)
{
	pr_source_line++;
	pr_line_start = pr_file_p;
	while(*pr_file_p==' ' || *pr_file_p == '\t')
		pr_file_p++;
	if (incomment)	//no constants if in a comment.
	{
	}
	else if (QCC_PR_Precompiler())
	{
	}

//	if (pr_dumpasm)
//		PR_PrintNextLine ();
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
				c = '\t';	//tab
			else if (c == 'a')
				c = '\a';	//bell
			else if (c == 'v')
				c = '\v';	//vertical tab
			else if (c == 'f')
				c = '\f';	//form feed
			else if (c == 's' || c == 'b')
			{
				texttype ^= 128;
				continue;
			}
			//else if (c == 'b')
			//	c = '\b';
			else if (c == '[')
				c = 16;	//quake specific
			else if (c == ']')
				c = 17;	//quake specific
			else if (c == '{')
			{
				int d;
				c = 0;
				while ((d = *pr_file_p++) != '}')
				{
					c = c * 10 + d - '0';
					if (d < '0' || d > '9' || c > 255)
						QCC_PR_ParseError(ERR_BADCHARACTERCODE, "Bad character code");
				}
			}
			else if (c == '<')
				c = 29;
			else if (c == '-')
				c = 30;
			else if (c == '>')
				c = 31;
			else if (c == 'u' || c == 'U')
			{
				//lower case u specifies exactly 4 nibbles.
				//upper case U specifies variable length. terminate with a double-doublequote pair, or some other non-hex char.
				int count = 0;
				unsigned long d;
				unsigned long unicode;
				unicode = 0;
				for(;;)
				{
					d = (unsigned char)*pr_file_p;
					if (d >= '0' && d <= '9')
						unicode = (unicode*16) + (d - '0');
					else if (d >= 'A' && d <= 'F')
						unicode = (unicode*16) + (d - 'A') + 10;
					else if (d >= 'a' && d <= 'f')
						unicode = (unicode*16) + (d - 'a') + 10;
					else
						break;
					count++;
					pr_file_p++;
				}
				if (!count || ((c=='u')?(count!=4):(count>8)) || unicode > 0x10FFFFu)	//RFC 3629 imposes the same limit as UTF-16 surrogate pairs.
					QCC_PR_ParseWarning(ERR_BADCHARACTERCODE, "Bad character code");

				//figure out the count of bytes required to encode this char
				count = 1;
				d = 0x7f;
				while (unicode > d)
				{
					count++;
					d = (d<<5) | 0x1f;
				}

				//error if needed
				if (len+count >= sizeof(pr_token))
					QCC_Error(ERR_INVALIDSTRINGIMMEDIATE, "String length exceeds %i", sizeof(pr_token)-1);

				//output it.
				if (count == 1)
					pr_token[len++] = (unsigned char)(c&0x7f);
				else
				{
					c = count*6;
					pr_token[len++] = (unsigned char)((unicode>>c)&(0x0000007f>>count)) | (0xffffff00 >> count);
					do
					{
						c = c-6;
						pr_token[len++] = (unsigned char)((unicode>>c)&0x3f) | 0x80;
					}
					while(c);
				}
				continue;
			}
			else if (c == 'x' || c == 'X')
			{
				int d;
				c = 0;

				d = (unsigned char)*pr_file_p++;
				if (d >= '0' && d <= '9')
					c += d - '0';
				else if (d >= 'A' && d <= 'F')
					c += d - 'A' + 10;
				else if (d >= 'a' && d <= 'f')
					c += d - 'a' + 10;
				else
					QCC_PR_ParseError(ERR_BADCHARACTERCODE, "Bad character code");

				c *= 16;

				d = (unsigned char)*pr_file_p++;
				if (d >= '0' && d <= '9')
					c += d - '0';
				else if (d >= 'A' && d <= 'F')
					c += d - 'A' + 10;
				else if (d >= 'a' && d <= 'f')
					c += d - 'a' + 10;
				else
					QCC_PR_ParseError(ERR_BADCHARACTERCODE, "Bad character code");
			}
			else if (c == '\\')
				c = '\\';
			else if (c == '\'')
				c = '\'';
			else if (c >= '0' && c <= '9')	//WARNING: This is not octal, but uses 'yellow' numbers instead (as on hud).
				c = 18 + c - '0';
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

			while(*pr_file_p && qcc_iswhite(*pr_file_p))
			{
				if (*pr_file_p == '\n')
				{
					pr_file_p++;
					QCC_PR_NewLine(false);
				}
				else
					pr_file_p++;
			}
			if (*pr_file_p == '\"')	//have annother go
			{
				pr_file_p++;
				continue;
			}
			pr_token[len] = 0;
			pr_token_type = tt_immediate;
			pr_immediate_type = type_string;
			strcpy (pr_immediate_string, pr_token);

			if (qccwarningaction[WARN_NOTUTF8])
			{
				len = 0;
				//this doesn't do over-long checks.
				for (c = 0; pr_token[c]; c++)
				{
					if (len)
					{
						if ((pr_token[c] & 0xc0) != 0x80)
							break;
						len--;
					}
					else if (pr_token[c] & 0x80)
					{
						if (!(pr_token[c] & 0x40))
						{
							//error.
							len = 1;
							break;
						}
						else if (!(pr_token[c] & 0x20))
							len = 2;
						else if (!(pr_token[c] & 0x10))
							len = 3;
						else if (!(pr_token[c] & 0x08))
							len = 4;
						else if (!(pr_token[c] & 0x04))
							len = 5;
						else if (!(pr_token[c] & 0x02))
							len = 6;
						else if (!(pr_token[c] & 0x01))
							len = 7;
						else
							len = 8;
					}
				}
				if (len)
					QCC_PR_ParseWarning(WARN_NOTUTF8, "String constant is not valid utf-8");
			}
			return;
		}
		else if (c == '#')
		{
			for (end = pr_file_p; ; end++)
			{
				if (qcc_iswhite(*end))
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
			cnst = QCC_PR_CheckCompConstString(pr_file_p);
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
		else if (c == 0x7C && flag_acc)	//reacc support... reacc is strange.
			c = '\n';
		else
			c |= texttype;

		pr_token[len] = c;
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
	} while ((c >= '0' && c<= '9') || (c == '.'&&pr_file_p[1]!='.') || (c>='a' && c <= 'f'));
	pr_token[len] = 0;
	return atoi (pr_token);
}

void QCC_PR_LexNumber (void)
{
	int tokenlen = 0;
	int num=0;
	int base=0;
	int c;
	int sign=1;
	if (*pr_file_p == '-')
	{
		sign=-1;
		pr_file_p++;

		pr_token[tokenlen++] = '-';
	}
	if (pr_file_p[0] == '0' && pr_file_p[1] == 'x')
	{
		pr_file_p+=2;
		base = 16;

		pr_token[tokenlen++] = '0';
		pr_token[tokenlen++] = 'x';
	}

	pr_immediate_type = NULL;
	//assume base 10 if not stated
	if (!base)
		base = 10;

	while((c = *pr_file_p))
	{
		if (c >= '0' && c <= '9')
		{
			pr_token[tokenlen++] = c;
			num*=base;
			num += c-'0';
		}
		else if (c >= 'a' && c <= 'f' && base > 10)
		{
			pr_token[tokenlen++] = c;
			num*=base;
			num += c -'a'+10;
		}
		else if (c >= 'A' && c <= 'F' && base > 10)
		{
			pr_token[tokenlen++] = c;
			num*=base;
			num += c -'A'+10;
		}
		else if (c == '.' && pr_file_p[1]!='.')
		{
			pr_token[tokenlen++] = c;
			pr_file_p++;
			pr_immediate_type = type_float;
			while(1)
			{
				c = *pr_file_p;
				if (c >= '0' && c <= '9')
				{
					pr_token[tokenlen++] = c;
				}
				else if (c == 'f')
				{
					pr_file_p++;
					break;
				}
				else
				{
					break;
				}
				pr_file_p++;
			}
			pr_token[tokenlen++] = 0;
			pr_immediate._float = (float)atof(pr_token);
			return;
		}
		else if (c == 'f')
		{
			pr_token[tokenlen++] = c;
			pr_token[tokenlen++] = 0;
			pr_file_p++;
			pr_immediate_type = type_float;
			pr_immediate._float = num*sign;
			return;
		}
		else if (c == 'i')
		{
			pr_token[tokenlen++] = c;
			pr_token[tokenlen++] = 0;
			pr_file_p++;
			pr_immediate_type = type_integer;
			pr_immediate._int = num*sign;
			return;
		}
		else
			break;
		pr_file_p++;
	}
	pr_token[tokenlen++] = 0;

	if (!pr_immediate_type)
	{
		if (flag_assume_integer)
			pr_immediate_type = type_integer;
		else
			pr_immediate_type = type_float;
	}

	if (pr_immediate_type == type_integer)
	{
		pr_immediate_type = type_integer;
		pr_immediate._int = num*sign;
	}
	else
	{
		pr_immediate_type = type_float;
		// at this point, we know there's no . in it, so the NaN bug shouldn't happen
		// and we cannot use atof on tokens like 0xabc, so use num*sign, it SHOULD be safe
		//pr_immediate._float = atof(pr_token);
		pr_immediate._float = (float)(num*sign);
	}
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
	if (*pr_file_p == 'f')
		pr_file_p++;
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
	{//extended character constant
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
		default:
			QCC_PR_ParseError (ERR_INVALIDVECTORIMMEDIATE, "Bad character constant");
		}
		pr_file_p++;
		if (*pr_file_p != '\'')
			QCC_PR_ParseError (ERR_INVALIDVECTORIMMEDIATE, "Bad character constant");
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

		if (*pr_file_p == '\'' && i == 1)
		{
			if (i < 2)
				QCC_PR_ParseWarning (WARN_FTE_SPECIFIC, "Bad vector");

			for (i++ ; i<3 ; i++)
				pr_immediate.vector[i] = 0;
			break;
		}
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
			strcpy (pr_token, pr_punctuationremap[i]);
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
		while ((c = *pr_file_p) && qcc_iswhite(c))
		{
			if (c=='\n')
			{
				pr_file_p++;
				QCC_PR_NewLine (false);
				if (!pr_file_p)
					return;
			}
			else
				pr_file_p++;
		}
		if (c == 0)
			return;		// end of file

	// skip // comments
		if (c=='/' && pr_file_p[1] == '/')
		{
			while (*pr_file_p && *pr_file_p != '\n')
				pr_file_p++;

			if (*pr_file_p == '\n')
				pr_file_p++;	//don't break on eof.
			QCC_PR_NewLine(false);
			continue;
		}

	// skip /* */ comments
		if (c=='/' && pr_file_p[1] == '*')
		{
			pr_file_p+=2;
			do
			{
				if (pr_file_p[0]=='\n')
				{
					QCC_PR_NewLine(true);
				}
				if (pr_file_p[1] == 0)
				{
					QCC_PR_ParseError(0, "EOF inside comment\n");
					pr_file_p++;
					return;
				}
				pr_file_p++;
			} while (pr_file_p[0] != '*' || pr_file_p[1] != '/');
			pr_file_p+=2;
			continue;
		}

		break;		// a real character has been found
	}
}

//============================================================================

#define	MAX_FRAMES	8192
char	pr_framemodelname[64];
char	pr_framemacros[MAX_FRAMES][64];
int		pr_framemacrovalue[MAX_FRAMES];
int		pr_nummacros, pr_oldmacros;
int		pr_macrovalue;
int		pr_savedmacro;

void QCC_PR_ClearGrabMacros (pbool newfile)
{
	if (!newfile)
		pr_nummacros = 0;
	pr_oldmacros = pr_nummacros;
	pr_macrovalue = 0;
	pr_savedmacro = -1;
}

int QCC_PR_FindMacro (char *name)
{
	int		i;

	for (i=pr_nummacros-1 ; i>=0 ; i--)
	{
		if (!STRCMP (name, pr_framemacros[i]))
		{
			return pr_framemacrovalue[i];
		}
	}
	for (i=pr_nummacros-1 ; i>=0 ; i--)
	{
		if (!stricmp (name, pr_framemacros[i]))
		{
			QCC_PR_ParseWarning(WARN_CASEINSENSATIVEFRAMEMACRO, "Case insensative frame macro");
			return pr_framemacrovalue[i];
		}
	}
	return -1;
}

void QCC_PR_ExpandMacro(void)
{
	int		i = QCC_PR_FindMacro(pr_token);

	if (i < 0)
		QCC_PR_ParseError (ERR_BADFRAMEMACRO, "Unknown frame macro $%s", pr_token);

	sprintf (pr_token,"%d", i);
	pr_token_type = tt_immediate;
	pr_immediate_type = type_float;
	pr_immediate._float = (float)i;
}

// just parses text, returning false if an eol is reached
pbool QCC_PR_SimpleGetToken (void)
{
	int		c;
	int		i;

	pr_token[0] = 0;

// skip whitespace
	while ((c = *pr_file_p) && qcc_iswhite(c))
	{
		if (c=='\n')
			return false;
		pr_file_p++;
	}
	if (c == 0)	//eof
		return false;
	if (pr_file_p[0] == '/')
	{
		if (pr_file_p[1] == '/')
		{	//comment alert
			while(*pr_file_p && *pr_file_p != '\n')
				pr_file_p++;
			return false;
		}
		if (pr_file_p[1] == '*')
			return false;
	}

	i = 0;
	while ((c = *pr_file_p) && !qcc_iswhite(c) && c != ',' && c != ';' && c != ')' && c != '(' && c != ']')
	{
		if (i == sizeof(qcc_token)-1)
			QCC_Error (ERR_INTERNAL, "token exceeds %i chars", i);
		pr_token[i] = c;
		i++;
		pr_file_p++;
	}
	pr_token[i] = 0;
	return i!=0;
}

pbool QCC_PR_LexMacroName(void)
{
	int		c;
	int		i;

	pr_token[0] = 0;

// skip whitespace
	while ((c = *pr_file_p) && qcc_iswhite(c))
	{
		if (c=='\n')
			return false;
		pr_file_p++;
	}
	if (!c)
		return false;
	if (pr_file_p[0] == '/')
	{
		if (pr_file_p[1] == '/')
		{	//comment alert
			while(*pr_file_p && *pr_file_p != '\n')
				pr_file_p++;
			return false;
		}
		if (pr_file_p[1] == '*')
			return false;
	}

	i = 0;
	while ( (c = *pr_file_p) > ' ' && c != '\n' && c != ',' && c != ';' && c != ')' && c != '(' && c != ']' && !(pr_file_p[0] == '.' && pr_file_p[1] == '.'))
	{
		if (i == sizeof(qcc_token)-1)
			QCC_Error (ERR_INTERNAL, "token exceeds %i chars", i);
		pr_token[i] = c;
		i++;
		pr_file_p++;
	}
	pr_token[i] = 0;
	return i!=0;
}

void QCC_PR_MacroFrame(char *name, int value)
{
	int i;
	for (i=pr_nummacros-1 ; i>=0 ; i--)
	{
		if (!STRCMP (name, pr_framemacros[i]))
		{
			pr_framemacrovalue[i] = value;
			if (i>=pr_oldmacros)
				QCC_PR_ParseWarning(WARN_DUPLICATEMACRO, "Duplicate macro defined (%s)", pr_token);
			//else it's from an old file, and shouldn't be mentioned.
			return;
		}
	}

	if (strlen(name)+1 > sizeof(pr_framemacros[0]))
		QCC_PR_ParseWarning(ERR_TOOMANYFRAMEMACROS, "Name for frame macro %s is too long", name);
	else
	{
		strcpy (pr_framemacros[pr_nummacros], name);
		pr_framemacrovalue[pr_nummacros] = value;
		pr_nummacros++;
		if (pr_nummacros >= MAX_FRAMES)
			QCC_PR_ParseError(ERR_TOOMANYFRAMEMACROS, "Too many frame macros defined");
	}
}

void QCC_PR_ParseFrame (void)
{
	while (QCC_PR_LexMacroName ())
	{
		QCC_PR_MacroFrame(pr_token, pr_macrovalue++);
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
	if (qcc_iswhite(*pr_file_p))
		QCC_PR_ParseError (ERR_BADFRAMEMACRO, "hanging $");
	QCC_PR_LexMacroName();
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
		while (QCC_PR_LexMacroName ())
		;
		QCC_PR_Lex ();
	}
	else if (!STRCMP (pr_token, "flush"))
	{
		QCC_PR_ClearGrabMacros(true);
		while (QCC_PR_LexMacroName ())
		;
		QCC_PR_Lex ();
	}
	else if (!STRCMP (pr_token, "framevalue"))
	{
		QCC_PR_LexMacroName ();
		pr_macrovalue = atoi(pr_token);

		QCC_PR_Lex ();
	}
	else if (!STRCMP (pr_token, "framerestore"))
	{
		QCC_PR_LexMacroName ();
		QCC_PR_ExpandMacro();
		pr_macrovalue = (int)pr_immediate._float;

		QCC_PR_Lex ();
	}
	else if (!STRCMP (pr_token, "modelname"))
	{
		int i;
		QCC_PR_LexMacroName ();

		if (*pr_framemodelname)
			QCC_PR_MacroFrame(pr_framemodelname, pr_macrovalue);

		strncpy(pr_framemodelname, pr_token, sizeof(pr_framemodelname)-1);
		pr_framemodelname[sizeof(pr_framemodelname)-1] = '\0';

		i = QCC_PR_FindMacro(pr_framemodelname);
		if (i)
			pr_macrovalue = i;
		else
			i = 0;

		QCC_PR_Lex ();
	}
// look for a frame name macro
	else
		QCC_PR_ExpandMacro ();
}

//===========================
//compiler constants	- dmw

pbool QCC_PR_UndefineName(char *name)
{
//	int a;
	CompilerConstant_t *c;
	c = pHash_Get(&compconstantstable, name);
	if (!c)
	{
		QCC_PR_ParseWarning(WARN_UNDEFNOTDEFINED, "Precompiler constant %s was not defined", name);
		return false;
	}

	Hash_Remove(&compconstantstable, name);
	return true;
}

CompilerConstant_t *QCC_PR_DefineName(char *name)
{
	int i;
	CompilerConstant_t *cnst;

//	if (numCompilerConstants >= MAX_CONSTANTS)
//		QCC_PR_ParseError("Too many compiler constants - %i >= %i", numCompilerConstants, MAX_CONSTANTS);

	if (strlen(name) >= MAXCONSTANTNAMELENGTH || !*name)
		QCC_PR_ParseError(ERR_NAMETOOLONG, "Compiler constant name length is too long or short");

	cnst = pHash_Get(&compconstantstable, name);
	if (cnst)
	{
		QCC_PR_ParseWarning(WARN_DUPLICATEDEFINITION, "Duplicate definition for Precompiler constant %s", name);
		Hash_Remove(&compconstantstable, name);
	}

	cnst = qccHunkAlloc(sizeof(CompilerConstant_t));

	cnst->used = false;
	cnst->numparams = 0;
	strcpy(cnst->name, name);
	cnst->namelen = strlen(name);
	cnst->value = cnst->name + strlen(cnst->name);
	for (i = 0; i < MAXCONSTANTPARAMS; i++)
		cnst->params[i][0] = '\0';

	pHash_Add(&compconstantstable, cnst->name, cnst, qccHunkAlloc(sizeof(bucket_t)));

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
	char *oldval;
	char *d;
	char *dbuf;
	int dbuflen;
	char *s;
	int quote=false;
	CompilerConstant_t *cnst;

	QCC_PR_SimpleGetToken ();

	if (!QCC_PR_SimpleGetToken ())
		QCC_PR_ParseError(ERR_NONAME, "No name defined for compiler constant");

	cnst = pHash_Get(&compconstantstable, pr_token);
	if (cnst)
	{
		oldval = cnst->value;
		Hash_Remove(&compconstantstable, pr_token);
	}
	else
		oldval = NULL;

	cnst = QCC_PR_DefineName(pr_token);

	if (*pr_file_p == '(')
	{
		s = pr_file_p+1;
		while(*pr_file_p++)
		{
			if (*pr_file_p == ',')
			{
				if (cnst->numparams >= MAXCONSTANTPARAMS)
					QCC_PR_ParseError(ERR_MACROTOOMANYPARMS, "May not have more than %i parameters to a macro", MAXCONSTANTPARAMS);
				strncpy(cnst->params[cnst->numparams], s, pr_file_p-s);
				cnst->params[cnst->numparams][pr_file_p-s] = '\0';
				cnst->numparams++;
				pr_file_p++;
				s = pr_file_p;
			}
			if (*pr_file_p == ')')
			{
				if (cnst->numparams >= MAXCONSTANTPARAMS)
					QCC_PR_ParseError(ERR_MACROTOOMANYPARMS, "May not have more than %i parameters to a macro", MAXCONSTANTPARAMS);
				strncpy(cnst->params[cnst->numparams], s, pr_file_p-s);
				cnst->params[cnst->numparams][pr_file_p-s] = '\0';
				cnst->numparams++;
				pr_file_p++;
				break;
			}
		}
	}
	else cnst->numparams = -1;

	s = pr_file_p;
	d = dbuf = NULL;
	dbuflen = 0;
	while(*s == ' ' || *s == '\t')
		s++;
	while(1)
	{
		if ((d - dbuf) + 2 >= dbuflen)
		{
			int len = d - dbuf;
			dbuflen = (len+128) * 2;
			dbuf = qccHunkAlloc(dbuflen);
			memcpy(dbuf, d - len, len);
			d = dbuf + len;
		}

		if( *s == '\\' )
		{
			// read over a newline if necessary
			if( s[1] == '\n' || s[1] == '\r' )
			{
				s++;
				QCC_PR_NewLine(false);
				*d++ = *s++;
				if( s[-1] == '\r' && s[0] == '\n' )
				{
					*d++ = *s++;
				}
			}
		}
		else if(*s == '\r' || *s == '\n' || *s == '\0')
		{
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

	cnst->value = dbuf;

	if (oldval)
	{	//we always warn if it was already defined
		//we use different warning codes so that -Wno-mundane can be used to ignore identical redefinitions.
		if (strcmp(oldval, cnst->value))
			QCC_PR_ParseWarning(WARN_DUPLICATEPRECOMPILER, "Alternate precompiler definition of %s", pr_token);
		else
			QCC_PR_ParseWarning(WARN_IDENTICALPRECOMPILER, "Identical precompiler definition of %s", pr_token);
	}

	pr_file_p = s;
}

/* *buffer, *bufferlen and *buffermax should be NULL/0 at the start */
static void QCC_PR_ExpandStrCat(char **buffer, int *bufferlen, int *buffermax,   char *newdata, int newlen)
{
	int newmax = *bufferlen + newlen;
	if (newmax < *bufferlen)
	{
		QCC_PR_ParseWarning(ERR_INTERNAL, "out of memory");
		return;
	}
	if (newmax > *buffermax)
	{
		char *newbuf;
		if (newmax < 64)
			newmax = 64;
		if (newmax < *bufferlen * 2)
		{
			newmax = *bufferlen * 2;
			if (newmax < *bufferlen) /*overflowed?*/
			{
				QCC_PR_ParseWarning(ERR_INTERNAL, "out of memory");
				return;
			}
		}
		newbuf = realloc(*buffer, newmax);
		if (!newbuf)
		{
			QCC_PR_ParseWarning(ERR_INTERNAL, "out of memory");
			return; /*OOM*/
		}
		*buffer = newbuf;
		*buffermax = newmax;
	}
	memcpy(*buffer + *bufferlen, newdata, newlen);
	*bufferlen += newlen;
	/*no null terminator, remember to cat one if required*/
}

int QCC_PR_CheckCompConst(void)
{
	char		*oldpr_file_p = pr_file_p;
	int whitestart;

	CompilerConstant_t *c;

	char *end;
	for (end = pr_file_p; ; end++)
	{
		if (!*end || qcc_iswhite(*end))
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
	c = pHash_Get(&compconstantstable, pr_token);

	if (c && !c->inside)
	{
		pr_file_p = oldpr_file_p+strlen(c->name);
		while(*pr_file_p == ' ' || *pr_file_p == '\t')
			pr_file_p++;
		if (c->numparams>=0)
		{
			if (*pr_file_p == '(')
			{
				int p;
				char *start;
				char *starttok;
				char *buffer;
				int buffermax;
				int bufferlen;
				char *paramoffset[MAXCONSTANTPARAMS+1];
				int param=0;
				int plevel=0;

				pr_file_p++;
				while(*pr_file_p == ' ' || *pr_file_p == '\t')
					pr_file_p++;
				start = pr_file_p;
				while(1)
				{
					// handle strings correctly by ignoring them
					if (*pr_file_p == '\"')
					{
						do {
							pr_file_p++;
						} while( (pr_file_p[-1] == '\\' || pr_file_p[0] != '\"') && *pr_file_p && *pr_file_p != '\n' );
					}
					if (*pr_file_p == '(')
						plevel++;
					else if (!plevel && (*pr_file_p == ',' || *pr_file_p == ')'))
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
						while(*pr_file_p == ' ' || *pr_file_p == '\t')
						{
							pr_file_p++;
							start++;
						}
						// move back by one char because we move forward by one at the end of the loop
						pr_file_p--;
						if (param == MAXCONSTANTPARAMS)
							QCC_PR_ParseError(ERR_TOOMANYPARAMS, "Too many parameters in macro call");
					} else if (*pr_file_p == ')' )
						plevel--;
					else if(*pr_file_p == '\n')
						QCC_PR_NewLine(false);

					// see that *pr_file_p = '\0' up there? Must ++ BEFORE checking for !*pr_file_p
					pr_file_p++;
					if (!*pr_file_p)
						QCC_PR_ParseError(ERR_EOF, "EOF on macro call");
				}
				if (param < c->numparams)
					QCC_PR_ParseError(ERR_TOOFEWPARAMS, "Not enough macro parameters");
				paramoffset[param] = start;

				buffer = NULL;
				bufferlen = 0;
				buffermax = 0;

				oldpr_file_p = pr_file_p;
				pr_file_p = c->value;
				for(;;)
				{
					whitestart = bufferlen;
					starttok = pr_file_p;
					while(qcc_iswhite(*pr_file_p))	//copy across whitespace
					{
						if (!*pr_file_p)
							break;
						pr_file_p++;
					}
					if (starttok != pr_file_p)
					{
						QCC_PR_ExpandStrCat(&buffer, &bufferlen, &buffermax,   starttok, pr_file_p - starttok);
					}

					if(*pr_file_p == '\"')
					{
						starttok = pr_file_p;
						do
						{
							pr_file_p++;
						} while( (pr_file_p[-1] == '\\' || pr_file_p[0] != '\"') && *pr_file_p && *pr_file_p != '\n' );
						if(*pr_file_p == '\"')
							pr_file_p++;

						QCC_PR_ExpandStrCat(&buffer, &bufferlen, &buffermax,   starttok, pr_file_p - starttok);
						continue;
					}
					else if (*pr_file_p == '#')	//if you ask for #a##b you will be shot. use #a #b instead, or chain macros.
					{
						if (pr_file_p[1] == '#')
						{	//concatinate (strip out whitespace before the token)
							bufferlen = whitestart;
							pr_file_p+=2;
						}
						else
						{	//stringify
							pr_file_p++;
							pr_file_p = QCC_COM_Parse2(pr_file_p);
							if (!pr_file_p)
								break;

							for (p = 0; p < param; p++)
							{
								if (!STRCMP(qcc_token, c->params[p]))
								{
									QCC_PR_ExpandStrCat(&buffer, &bufferlen, &buffermax,   "\"", 1);
									QCC_PR_ExpandStrCat(&buffer, &bufferlen, &buffermax,   paramoffset[p], strlen(paramoffset[p]));
									QCC_PR_ExpandStrCat(&buffer, &bufferlen, &buffermax,   "\"", 1);
									break;
								}
							}
							if (p == param)
							{
								QCC_PR_ExpandStrCat(&buffer, &bufferlen, &buffermax,   "#", 1);
								QCC_PR_ExpandStrCat(&buffer, &bufferlen, &buffermax,   qcc_token, strlen(qcc_token));
								//QCC_PR_ParseWarning(0, "Stringification ignored");
							}
							continue;	//already did this one
						}
					}

					pr_file_p = QCC_COM_Parse2(pr_file_p);
					if (!pr_file_p)
						break;

					for (p = 0; p < param; p++)
					{
						if (!STRCMP(qcc_token, c->params[p]))
						{
							QCC_PR_ExpandStrCat(&buffer, &bufferlen, &buffermax,   paramoffset[p], strlen(paramoffset[p]));
							break;
						}
					}
					if (p == param)
						QCC_PR_ExpandStrCat(&buffer, &bufferlen, &buffermax,   qcc_token, strlen(qcc_token));
				}

				for (p = 0; p < param-1; p++)
					paramoffset[p][strlen(paramoffset[p])] = ',';
				paramoffset[p][strlen(paramoffset[p])] = ')';

				pr_file_p = oldpr_file_p;
				if (!bufferlen)
					expandedemptymacro = true;
				else
				{
					QCC_PR_ExpandStrCat(&buffer, &bufferlen, &buffermax,   "\0", 1);
					QCC_PR_IncludeChunkEx(buffer, true, NULL, c);
				}
				free(buffer);
			}
			else
				QCC_PR_ParseError(ERR_TOOFEWPARAMS, "Macro without argument list");
		}
		else
		{
			if (!*c->value)
				expandedemptymacro = true;
			QCC_PR_IncludeChunkEx(c->value, false, NULL, c);
		}

		QCC_PR_Lex();
		return true;
	}

	if (!strncmp(pr_file_p, "__TIME__", 8))
	{
		static char retbuf[128];

		time_t long_time;
		time( &long_time );
		strftime( retbuf, sizeof(retbuf),
			 "\"%H:%M\"", localtime( &long_time ));

		pr_file_p = retbuf;
		QCC_PR_Lex();	//translate the macro's value
		pr_file_p = oldpr_file_p+8;

		return true;
	}
	if (!strncmp(pr_file_p, "__DATE__", 8))
	{
		static char retbuf[128];

		time_t long_time;
		time( &long_time );
		strftime( retbuf, sizeof(retbuf),
			 "\"%a %d %b %Y\"", localtime( &long_time ));

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
		sprintf(retbuf, "0i");
		pr_file_p = retbuf;
		QCC_PR_Lex();	//translate the macro's value
		pr_file_p = oldpr_file_p+8;
		return true;
	}
	return false;
}

char *QCC_PR_CheckCompConstString(char *def)
{
	char *s;

	CompilerConstant_t *c;

	c = pHash_Get(&compconstantstable, def);

	if (c)
	{
		s = QCC_PR_CheckCompConstString(c->value);
		return s;
	}
	return def;
}

CompilerConstant_t *QCC_PR_CheckCompConstDefined(char *def)
{
	CompilerConstant_t *c = pHash_Get(&compconstantstable, def);
	return c;
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

	pr_token_line_last = pr_token_line;
	pr_token_line = pr_source_line;

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
	if ((c == '~' || c == '%') && pr_file_p[1] >= '0' && pr_file_p[1] <= '9')	//let's see which one we make into an operator first... possibly both...
	{
		QCC_PR_ParseWarning(0, "~ or %% prefixes to denote integers are deprecated. Please use a postfix of 'i'");
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
	if ( (c == '.'&&pr_file_p[1]!='.'&&pr_file_p[1] >='0' && pr_file_p[1] <= '9') || (c >= '0' && c <= '9') || ( c=='-' && pr_file_p[1]>='0' && pr_file_p[1] <='9') )
	{
		pr_token_type = tt_immediate;
		QCC_PR_LexNumber ();
		return;
	}

	if (c == '#' && !(pr_file_p[1]=='-' || (pr_file_p[1]>='0' && pr_file_p[1] <='9')))	//hash and not number
	{
		pr_file_p++;
		if (!QCC_PR_CheckCompConst())
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
		if (flag_hashonly || !QCC_PR_CheckCompConst())	//look for a macro.
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
	if (!qccwarningaction[type])
		return;
	if (def->s_file)
	{
		if (flag_msvcstyle)
			printf ("%s(%i) :    %s  is defined here\n", strings + def->s_file, def->s_line, def->name);
		else
			printf ("%s:%i:    %s  is defined here\n", strings + def->s_file, def->s_line, def->name);
	}
}
void *errorscope;
void QCC_PR_PrintScope (void)
{
	if (pr_scope)
	{
		if (errorscope != pr_scope)
			printf ("in function %s (line %i),\n", pr_scope->name, pr_scope->s_line);
		errorscope = pr_scope;
	}
	else
	{
		if (errorscope)
			printf ("at global scope,\n");
		errorscope = NULL;
	}
}
void QCC_PR_ResetErrorScope(void)
{
	errorscope = NULL;
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
//will abort.
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

	QCC_PR_PrintScope();
	if (flag_msvcstyle)
		printf ("%s(%i) : error: %s\n", strings + s_file, pr_source_line, string);
	else
		printf ("%s:%i: error: %s\n", strings + s_file, pr_source_line, string);

	longjmp (pr_parse_abort, 1);
}
//will abort.
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
	QCC_PR_PrintScope();
	if (flag_msvcstyle)
		printf ("%s(%i) : error: %s\n", strings + s_file, pr_source_line, string);
	else
		printf ("%s:%i: error: %s\n", strings + s_file, pr_source_line, string);

	QCC_PR_ParsePrintDef(WARN_ERROR, def);

	longjmp (pr_parse_abort, 1);
}

pbool VARGS QCC_PR_PrintWarning (int type, char *file, int line, char *string)
{
	char *wnam = QCC_NameForWarning(type);
	if (!wnam)
		wnam = "";

	QCC_PR_PrintScope();
	if (type >= ERR_PARSEERRORS)
	{
		if (!file)
			printf ("error%s: %s\n", wnam, string);
		else if (flag_msvcstyle)
			printf ("%s(%i) : error%s: %s\n", file, line, wnam, string);
		else
			printf ("%s:%i: error%s: %s\n", file, line, wnam, string);
		pr_error_count++;
	}
	else if (qccwarningaction[type] == 2)
	{	//-werror
		if (!file)
			printf ("werror%s: %s\n", wnam, string);
		else if (flag_msvcstyle)
			printf ("%s(%i) : werror%s: %s\n", file, line, wnam, string);
		else
			printf ("%s:%i: werror%s: %s\n", file, line, wnam, string);
		pr_error_count++;
	}
	else
	{
		if (!file)
			printf ("warning%s: %s\n", wnam, string);
		else if (flag_msvcstyle)
			printf ("%s(%i) : warning%s: %s\n", file, line, wnam, string);
		else
			printf ("%s:%i: warning%s: %s\n", file, line, wnam, string);
		pr_warning_count++;
	}
	return true;
}
pbool VARGS QCC_PR_Warning (int type, char *file, int line, char *error, ...)
{
	va_list		argptr;
	char		string[1024];

	if (!qccwarningaction[type])
		return false;

	va_start (argptr,error);
	QC_vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);

	return QCC_PR_PrintWarning(type, file, line, string);
}

//can be used for errors, qcc execution will continue.
pbool VARGS QCC_PR_ParseWarning (int type, char *error, ...)
{
	va_list		argptr;
	char		string[1024];

	if (!qccwarningaction[type])
		return false;

	va_start (argptr,error);
	QC_vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);

	return QCC_PR_PrintWarning(type, strings + s_file, pr_source_line, string);
}

void VARGS QCC_PR_Note (int type, char *file, int line, char *error, ...)
{
	va_list		argptr;
	char		string[1024];

	if (!qccwarningaction[type])
		return;

	va_start (argptr,error);
	QC_vsnprintf (string,sizeof(string)-1, error,argptr);
	va_end (argptr);

	QCC_PR_PrintScope();
	if (!file)
		printf ("note: %s\n", string);
	else if (flag_msvcstyle)
		printf ("%s(%i) : note: %s\n", file, line, string);
	else
		printf ("%s:%i: note: %s\n", file, line, string);
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
pbool QCC_PR_CheckToken (char *string)
{
	if (pr_token_type != tt_punct)
		return false;

	if (STRCMP (string, pr_token))
		return false;

	QCC_PR_Lex ();
	return true;
}

pbool QCC_PR_CheckImmediate (char *string)
{
	if (pr_token_type != tt_immediate)
		return false;

	if (STRCMP (string, pr_token))
		return false;

	QCC_PR_Lex ();
	return true;
}

pbool QCC_PR_CheckName(char *string)
{
	if (pr_token_type != tt_name)
		return false;
	if (flag_caseinsensative)
	{
		if (stricmp (string, pr_token))
			return false;
	}
	else
	{
		if (STRCMP(string, pr_token))
			return false;
	}
	QCC_PR_Lex ();
	return true;
}

pbool QCC_PR_CheckKeyword(int keywordenabled, char *string)
{
	if (!keywordenabled)
		return false;
	if (flag_caseinsensative)
	{
		if (stricmp (string, pr_token))
			return false;
	}
	else
	{
		if (STRCMP(string, pr_token))
			return false;
	}
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
	{
		if (pr_token_type == tt_eof)
			QCC_PR_ParseError (ERR_EOF, "unexpected EOF", pr_token);
		else
			QCC_PR_ParseError (ERR_NOTANAME, "\"%s\" - not a name", pr_token);
	}
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
int typecmp(QCC_type_t *a, QCC_type_t *b)
{
	if (a == b)
		return 0;
	if (!a || !b)
		return 1;	//different (^ and not both null)

	if (a->type != b->type)
		return 1;
	if (a->num_parms != b->num_parms)
	{
		return 1;
	}

	if (a->size != b->size)
		return 1;
//	if (STRCMP(a->name, b->name))	//This isn't 100% clean.
//		return 1;

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

//compares the types, but doesn't complain if there are optional arguments which differ
int typecmp_lax(QCC_type_t *a, QCC_type_t *b)
{
	int numargs = 0;
	int t;
	if (a == b)
		return 0;
	if (!a || !b)
		return 1;	//different (^ and not both null)

	if (a->type != b->type)
	{
		if (a->type != ev_variant && b->type != ev_variant)
			return 1;
	}
	else if (a->size != b->size)
		return 1;

	t = a->num_parms;
	if (t < 0)
		t = (t * -1) - 1;
	numargs = t;
	t = b->num_parms;
	if (t < 0)
		t = (t * -1) - 1;
	if (numargs > t)
		numargs = t;

//	if (STRCMP(a->name, b->name))	//This isn't 100% clean.
//		return 1;

	if (typecmp_lax(a->aux_type, b->aux_type))
		return 1;

	if (numargs)
	{
		a = a->param;
		b = b->param;

		while(numargs-->0 || (a&&b))
		{
			if (typecmp_lax(a, b))
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

	out = QCC_PR_NewType(in->name, in->type, false);
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
	out->arraysize = in->arraysize;
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
		pbool varg = type->num_parms < 0;
		int args = type->num_parms;
		if (args < 0)
			args = -(args+1);
		strcat(ret, type->aux_type->name);
		strcat(ret, " (");
		type = type->param;
		while(type)
		{
			if (args<=0)
				strcat(ret, "optional ");
			args--;

			strcat(ret, type->name);
			type = type->next;

			if (type || varg)
				strcat(ret, ", ");
		}
		if (varg)
		{
			strcat(ret, "...");
		}
		strcat(ret, ")");
	}
	else if (type->type == ev_entity && type->parentclass)
	{
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
		if (!pr_bracelevel && QCC_PR_CheckToken (";"))
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

//expects a ( to have already been parsed.
QCC_type_t *QCC_PR_ParseFunctionType (int newtype, QCC_type_t *returntype)
{
	QCC_type_t	*ftype, *ptype, *nptype;
	char	*name;
	int definenames = !recursivefunctiontype;
	int optional = 0;
	int numparms = 0;

	recursivefunctiontype++;

	ftype = QCC_PR_NewType(type_function->name, ev_function, false);

	ftype->aux_type = returntype;	// return type
	ftype->num_parms = 0;
	ptype = NULL;


	if (!QCC_PR_CheckToken (")"))
	{
		if (QCC_PR_CheckToken ("..."))
			ftype->num_parms = -1;	// variable args
		else
			do
			{
				if (ftype->num_parms>=MAX_PARMS+MAX_EXTRA_PARMS)
					QCC_PR_ParseError(ERR_TOOMANYTOTALPARAMETERS, "Too many parameters. Sorry. (limit is %i)\n", MAX_PARMS+MAX_EXTRA_PARMS);

				if (QCC_PR_CheckToken ("..."))
				{
					if (optional)
						numparms = optional-1;
					ftype->num_parms = (numparms * -1) - 1;
					break;
				}

				if (QCC_PR_CheckKeyword(keyword_optional, "optional"))
				{
					if (!optional)
						optional = numparms+1;
				}
				else if (optional)
					QCC_PR_ParseWarning(WARN_MISSINGOPTIONAL, "optional not specified on all optional args\n");

				nptype = QCC_PR_ParseType(true, false);

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
						strcpy (pr_parm_names[numparms], name);
				}
				else if (definenames)
					strcpy (pr_parm_names[numparms], "");
				numparms++;
				if (optional)
					ftype->num_parms = optional-1;
				else
					ftype->num_parms = numparms;
			} while (QCC_PR_CheckToken (","));

		QCC_PR_Expect (")");
	}
	recursivefunctiontype--;
	if (newtype)
		return ftype;
	return QCC_PR_FindType (ftype);
}
QCC_type_t *QCC_PR_ParseFunctionTypeReacc (int newtype, QCC_type_t *returntype)
{
	QCC_type_t	*ftype, *ptype, *nptype;
	char	*name;
	char	argname[64];
	int definenames = !recursivefunctiontype;

	recursivefunctiontype++;

	ftype = QCC_PR_NewType(type_function->name, ev_function, false);

	ftype->aux_type = returntype;	// return type
	ftype->num_parms = 0;
	ptype = NULL;


	if (!QCC_PR_CheckToken (")"))
	{
		if (QCC_PR_CheckToken ("..."))
			ftype->num_parms = -1;	// variable args
		else
			do
			{
				if (ftype->num_parms>=MAX_PARMS+MAX_EXTRA_PARMS)
					QCC_PR_ParseError(ERR_TOOMANYTOTALPARAMETERS, "Too many parameters. Sorry. (limit is %i)\n", MAX_PARMS+MAX_EXTRA_PARMS);

				if (QCC_PR_CheckToken ("..."))
				{
					ftype->num_parms = (ftype->num_parms * -1) - 1;
					break;
				}

				if (QCC_PR_CheckName("arg"))
				{
					sprintf(argname, "arg%i", ftype->num_parms);
					name = argname;
					nptype = QCC_PR_NewType("Variant", ev_variant, false);
				}
				else if (QCC_PR_CheckName("vect"))	//this can only be of vector sizes, so...
				{
					sprintf(argname, "arg%i", ftype->num_parms);
					name = argname;
					nptype = QCC_PR_NewType("Vector", ev_vector, false);
				}
				else
				{
					name = QCC_PR_ParseName();
					QCC_PR_Expect(":");
					nptype = QCC_PR_ParseType(true, false);
				}

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

				if (definenames)
					strcpy (pr_parm_names[ftype->num_parms], name);
				ftype->num_parms++;
			} while (QCC_PR_CheckToken (";"));

		QCC_PR_Expect (")");
	}
	recursivefunctiontype--;
	if (newtype)
		return ftype;
	return QCC_PR_FindType (ftype);
}
QCC_type_t *QCC_PR_PointerType (QCC_type_t *pointsto)
{
	QCC_type_t	*ptype, *e;
	ptype = QCC_PR_NewType("ptr", ev_pointer, false);
	ptype->aux_type = pointsto;
	e = QCC_PR_FindType (ptype);
	if (e == ptype)
	{
		char name[128];
		sprintf(name, "ptr to %s", pointsto->name);
		e->name = strdup(name);
	}
	return e;
}
QCC_type_t *QCC_PR_FieldType (QCC_type_t *pointsto)
{
	QCC_type_t	*ptype;
	char name[128];
	sprintf(name, "FIELD TYPE(%s)", pointsto->name);
	ptype = QCC_PR_NewType(name, ev_field, false);
	ptype->aux_type = pointsto;
	ptype->size = ptype->aux_type->size;
	return QCC_PR_FindType (ptype);
}

pbool type_inlinefunction;
/*newtype=true: creates a new type always
  silentfail=true: function is permitted to return NULL if it was not given a type, otherwise never returns NULL
*/
QCC_type_t *QCC_PR_ParseType (int newtype, pbool silentfail)
{
	QCC_type_t	*newparm;
	QCC_type_t	*newt;
	QCC_type_t	*type;
	char	*name;
	int i;

	type_inlinefunction = false;	//doesn't really matter so long as its not from an inline function type

//	int ofs;

	if (QCC_PR_CheckToken (".."))	//so we don't end up with the user specifying '. .vector blah' (hexen2 added the .. token for array ranges)
	{
		newt = QCC_PR_NewType("FIELD TYPE", ev_field, false);
		newt->aux_type = QCC_PR_ParseType (false, false);

		newt->size = newt->aux_type->size;

		newt = QCC_PR_FindType (newt);

		type = QCC_PR_NewType("FIELD TYPE", ev_field, false);
		type->aux_type = newt;

		type->size = type->aux_type->size;

		if (newtype)
			return type;
		return QCC_PR_FindType (type);
	}
	if (QCC_PR_CheckToken ("."))
	{
		newt = QCC_PR_NewType("FIELD TYPE", ev_field, false);
		newt->aux_type = QCC_PR_ParseType (false, false);

		newt->size = newt->aux_type->size;

		if (newtype)
			return newt;
		return QCC_PR_FindType (newt);
	}

	name = QCC_PR_CheckCompConstString(pr_token);

	if (QCC_PR_CheckKeyword (keyword_class, "class"))
	{
//		int parms;
		QCC_type_t *fieldtype;
		char membername[2048];
		char *classname = QCC_PR_ParseName();
		int forwarddeclaration;

		newt = 0;

		/* Don't advance the line number yet */
		forwarddeclaration = pr_token[0] == ';';

		/* Look to see if this type is already defined */
		for(i=0;i<numtypeinfos;i++)
		{
			if (!qcc_typeinfo[i].typedefed)
				continue;
			if (STRCMP(qcc_typeinfo[i].name, classname) == 0)
			{
				newt = &qcc_typeinfo[i];
				break;
			}
		}

		if (newt && forwarddeclaration)
			QCC_PR_ParseError(ERR_REDECLARATION, "Forward declaration of already defined class %s", classname);

		if (newt && newt->num_parms != 0)
			QCC_PR_ParseError(ERR_REDECLARATION, "Redeclaration of class %s", classname);

		if (!newt)
			newt = QCC_PR_NewType(classname, ev_entity, true);

		newt->size=type_entity->size;

		type = NULL;

		if (forwarddeclaration)
		{
			QCC_PR_CheckToken(";");
			return NULL;
		}



		if (QCC_PR_CheckToken(":"))
		{
			char *parentname = QCC_PR_ParseName();
			newt->parentclass = QCC_TypeForName(parentname);
			if (!newt->parentclass)
				QCC_PR_ParseError(ERR_NOTANAME, "Parent class %s was not defined", parentname);
		}
		else
			newt->parentclass = type_entity;


		QCC_PR_Expect("{");
		if (QCC_PR_CheckToken(","))
			QCC_PR_ParseError(ERR_NOTANAME, "member missing name");
		while (!QCC_PR_CheckToken("}"))
		{
//			if (QCC_PR_CheckToken(","))
//				type->next = QCC_PR_NewType(type->name, type->type);
//			else
				newparm = QCC_PR_ParseType(true, false);

			if (newparm->type == ev_struct || newparm->type == ev_union)	//we wouldn't be able to handle it.
				QCC_PR_ParseError(ERR_INTERNAL, "Struct or union in class %s", classname);

			if (!QCC_PR_CheckToken(";"))
			{
				newparm->name = QCC_CopyString(pr_token)+strings;
				QCC_PR_Lex();
				if (QCC_PR_CheckToken("["))
				{
					type->next->size*=atoi(pr_token);
					QCC_PR_Lex();
					QCC_PR_Expect("]");
				}
				QCC_PR_CheckToken(";");
			}
			else
				newparm->name = QCC_CopyString("")+strings;

			sprintf(membername, "%s::"MEMBERFIELDNAME, classname, newparm->name);
			fieldtype = QCC_PR_NewType(newparm->name, ev_field, false);
			fieldtype->aux_type = newparm;
			fieldtype->size = newparm->size;
			QCC_PR_GetDef(fieldtype, membername, pr_scope, 2, 0, false);


			newparm->ofs = 0;//newt->size;
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
	if (QCC_PR_CheckKeyword (keyword_struct, "struct"))
	{
		newt = QCC_PR_NewType("struct", ev_struct, false);
		newt->size=0;
		QCC_PR_Expect("{");

		type = NULL;
		if (QCC_PR_CheckToken(","))
			QCC_PR_ParseError(ERR_NOTANAME, "element missing name");

		newparm = NULL;
		while (!QCC_PR_CheckToken("}"))
		{
			if (QCC_PR_CheckToken(","))
			{
				if (!newparm)
					QCC_PR_ParseError(ERR_NOTANAME, "element missing type");
				newparm = QCC_PR_NewType(newparm->name, newparm->type, false);
			}
			else
				newparm = QCC_PR_ParseType(true, false);

			if (!QCC_PR_CheckToken(";"))
			{
				newparm->name = QCC_CopyString(pr_token)+strings;
				QCC_PR_Lex();
				if (QCC_PR_CheckToken("["))
				{
					newparm->arraysize=QCC_PR_IntConstExpr();
					QCC_PR_Expect("]");
				}
				QCC_PR_CheckToken(";");
			}
			else
				newparm->name = QCC_CopyString("")+strings;
			newparm->ofs = newt->size;
			newt->size += newparm->size*(newparm->arraysize?newparm->arraysize:1);
			newt->num_parms++;

			if (type)
				type->next = newparm;
			else
				newt->param = newparm;
			type = newparm;
		}
		return newt;
	}
	if (QCC_PR_CheckKeyword (keyword_union, "union"))
	{
		newt = QCC_PR_NewType("union", ev_union, false);
		newt->size=0;
		QCC_PR_Expect("{");

		type = NULL;
		if (QCC_PR_CheckToken(","))
			QCC_PR_ParseError(ERR_NOTANAME, "element missing name");
		newparm = NULL;
		while (!QCC_PR_CheckToken("}"))
		{
			int arraysize;
			if (QCC_PR_CheckToken(","))
			{
				if (!newparm)
					QCC_PR_ParseError(ERR_NOTANAME, "element missing type");
				newparm = QCC_PR_NewType(newparm->name, newparm->type, false);
			}
			else
				newparm = QCC_PR_ParseType(true, false);
			if (QCC_PR_CheckToken(";"))
				newparm->name = QCC_CopyString("")+strings;
			else
			{
				newparm->name = QCC_CopyString(pr_token)+strings;
				QCC_PR_Lex();
				if (QCC_PR_CheckToken("["))
				{
					newparm->arraysize=QCC_PR_IntConstExpr();
					QCC_PR_Expect("]");
				}
				QCC_PR_Expect(";");
			}
			newparm->ofs = 0;
			arraysize = newparm->arraysize;
			if (!arraysize)
				arraysize = 1;
			if (newparm->size*arraysize > newt->size)
				newt->size = newparm->size*arraysize;
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
		if (!qcc_typeinfo[i].typedefed)
			continue;
		if (!STRCMP(qcc_typeinfo[i].name, name))
		{
			type = &qcc_typeinfo[i];
			break;
		}
	}

	if (i == numtypeinfos)
	{
		if (!*name)
			return NULL;
		if (!stricmp("Void", name))
			type = type_void;
		else if (!stricmp("Real", name))
			type = type_float;
		else if (!stricmp("Vector", name))
			type = type_vector;
		else if (!stricmp("Object", name))
			type = type_entity;
		else if (!stricmp("String", name))
			type = type_string;
		else if (!stricmp("PFunc", name))
			type = type_function;
		else
		{
			if (silentfail)
				return NULL;

			QCC_PR_ParseError (ERR_NOTATYPE, "\"%s\" is not a type", name);
			type = type_float;	// shut up compiler warning
		}
	}
	QCC_PR_Lex ();

	while (QCC_PR_CheckToken("*"))
		type = QCC_PointerTypeTo(type);

	if (QCC_PR_CheckToken ("("))	//this is followed by parameters. Must be a function.
	{
		type_inlinefunction = true;
		type = QCC_PR_ParseFunctionType(newtype, type);
	}
	else
	{
		if (newtype)
		{
			type = QCC_PR_DuplicateType(type);
		}
	}
	return type;
}

#endif



