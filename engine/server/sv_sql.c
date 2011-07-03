#include "quakedef.h"

#ifdef SQL
#include "sv_sql.h"

static void (VARGS *qmysql_library_end)(void);
static int (VARGS *qmysql_library_init)(int argc, char **argv, char **groups);

static my_ulonglong (VARGS *qmysql_affected_rows)(MYSQL *mysql);
static void (VARGS *qmysql_close)(MYSQL *sock);
static void (VARGS *qmysql_data_seek)(MYSQL_RES *result, my_ulonglong offset);
static unsigned int (VARGS *qmysql_errno)(MYSQL *mysql);
static const char *(VARGS *qmysql_error)(MYSQL *mysql);
static MYSQL_FIELD *(VARGS *qmysql_fetch_field_direct)(MYSQL_RES *res, unsigned int fieldnr);
static MYSQL_ROW (VARGS *qmysql_fetch_row)(MYSQL_RES *result);
static unsigned int (VARGS *qmysql_field_count)(MYSQL *mysql);
static void (VARGS *qmysql_free_result)(MYSQL_RES *result);
static const char *(VARGS *qmysql_get_client_info)(void);
static MYSQL *(VARGS *qmysql_init)(MYSQL *mysql);
static MYSQL_RES *(VARGS *qmysql_store_result)(MYSQL *mysql);
static unsigned int (VARGS *qmysql_num_fields)(MYSQL_RES *res);
static my_ulonglong (VARGS *qmysql_num_rows)(MYSQL_RES *res);
static int (VARGS *qmysql_options)(MYSQL *mysql, enum mysql_option option, const char *arg);
static int (VARGS *qmysql_query)(MYSQL *mysql, const char *q);
static MYSQL *(VARGS *qmysql_real_connect)(MYSQL *mysql, const char *host, const char *user, const char *passwd, const char *db, unsigned int port, const char *unix_socket, unsigned long clientflag);
static unsigned long (VARGS *qmysql_real_escape_string)(MYSQL *mysql, char *to, const char *from, unsigned long length);
static void (VARGS *qmysql_thread_end)(void);
static my_bool (VARGS *qmysql_thread_init)(void);
static unsigned int (VARGS *qmysql_thread_safe)(void);

static dllfunction_t mysqlfuncs[] =
{
	{(void*)&qmysql_library_end, "mysql_server_end"}, /* written as a define alias in mysql.h */
	{(void*)&qmysql_library_init, "mysql_server_init"}, /* written as a define alias in mysql.h */
	{(void*)&qmysql_affected_rows, "mysql_affected_rows"},
	{(void*)&qmysql_close, "mysql_close"},
	{(void*)&qmysql_data_seek, "mysql_data_seek"},
	{(void*)&qmysql_errno, "mysql_errno"},
	{(void*)&qmysql_error, "mysql_error"},
	{(void*)&qmysql_fetch_field_direct, "mysql_fetch_field_direct"},
	{(void*)&qmysql_fetch_row, "mysql_fetch_row"},
	{(void*)&qmysql_field_count, "mysql_field_count"},
	{(void*)&qmysql_free_result, "mysql_free_result"},
	{(void*)&qmysql_get_client_info, "mysql_get_client_info"},
	{(void*)&qmysql_init, "mysql_init"},
	{(void*)&qmysql_store_result, "mysql_store_result"},
	{(void*)&qmysql_num_fields, "mysql_num_fields"},
	{(void*)&qmysql_num_rows, "mysql_num_rows"},
	{(void*)&qmysql_options, "mysql_options"},
	{(void*)&qmysql_query, "mysql_query"},
	{(void*)&qmysql_real_connect, "mysql_real_connect"},
	{(void*)&qmysql_real_escape_string, "mysql_real_escape_string"},
	{(void*)&qmysql_thread_end, "mysql_thread_end"},
	{(void*)&qmysql_thread_init, "mysql_thread_init"},
	{(void*)&qmysql_thread_safe, "mysql_thread_safe"},
	{NULL}
};

cvar_t sql_driver = SCVARF("sv_sql_driver", "mysql", CVAR_NOUNSAFEEXPAND);
cvar_t sql_host = SCVARF("sv_sql_host", "127.0.0.1", CVAR_NOUNSAFEEXPAND);
cvar_t sql_username = SCVARF("sv_sql_username", "", CVAR_NOUNSAFEEXPAND);
cvar_t sql_password = SCVARF("sv_sql_password", "", CVAR_NOUNSAFEEXPAND);
cvar_t sql_defaultdb = SCVARF("sv_sql_defaultdb", "", CVAR_NOUNSAFEEXPAND);

void SQL_PushResult(sqlserver_t *server, queryresult_t *qres)
{
	Sys_LockMutex(server->resultlock);
	qres->next = NULL;
	if (!server->resultslast)
		server->results = server->resultslast = qres;
	else
		server->resultslast = server->resultslast->next = qres;
	Sys_UnlockMutex(server->resultlock);
}

queryresult_t *SQL_PullResult(sqlserver_t *server)
{
	queryresult_t *qres;
	Sys_LockMutex(server->resultlock);
	qres = server->results;
	if (qres)
	{
		server->results = qres->next;
		if (!server->results)
			server->resultslast = NULL;
	}
	Sys_UnlockMutex(server->resultlock);

	return qres;
}

void SQL_PushRequest(sqlserver_t *server, queryrequest_t *qreq)
{
	Sys_LockConditional(server->requestcondv);
	qreq->next = NULL;
	if (!server->requestslast)
		server->requests = server->requestslast = qreq;
	else
		server->requestslast = server->requestslast->next = qreq;
	Sys_UnlockConditional(server->requestcondv);
}

queryrequest_t *SQL_PullRequest(sqlserver_t *server, qboolean lock)
{
	queryrequest_t *qreq;
	if (lock)
		Sys_LockConditional(server->requestcondv);
	qreq = server->requests;
	if (qreq)
	{
		server->requests = qreq->next;
		if (!server->requests)
			server->requestslast = NULL;
	}
	Sys_UnlockConditional(server->requestcondv);

	return qreq;
}

sqlserver_t **sqlservers;
int sqlservercount;
qboolean sqlavailable;

int sql_serverworker(void *sref)
{
	sqlserver_t *server = (sqlserver_t *)sref;
	char *error = NULL;
	my_bool reconnect = 1;
	int tinit, i;
	qboolean needlock = false;

	if (tinit = qmysql_thread_init())
		error = "MYSQL thread init failed";
	else if (!(server->mysql = qmysql_init(NULL)))
		error = "MYSQL init failed";
	else if (qmysql_options(server->mysql, MYSQL_OPT_RECONNECT, &reconnect))
		error = "MYSQL reconnect options set failed";
	else
	{	
		int port = 0;
		char *colon;

		colon = strchr(server->connectparams[0], ':');
		if (colon)
		{
			*colon = '\0';
			port = atoi(colon + 1);
		}

		if (!(server->mysql = qmysql_real_connect(server->mysql, server->connectparams[0], server->connectparams[1], server->connectparams[2], server->connectparams[3], port, 0, 0)))
		error = "MYSQL initial connect attempt failed";

		if (colon)
			*colon = ':';
	}

	for (i = SQL_CONNECT_STRUCTPARAMS; i < SQL_CONNECT_PARAMS; i++)
		Z_Free(server->connectparams[i]);

	BZ_Realloc(server->connectparams, sizeof(char *) * SQL_CONNECT_STRUCTPARAMS);

	if (error)
		server->active = false;

	while (server->active)
	{	
		Sys_LockConditional(server->requestcondv);
		if (!server->requests) // this is needed for thread startup and to catch any "lost" changes
			Sys_ConditionWait(server->requestcondv);
		needlock = false; // so we don't try to relock first round

		while (1)
		{
			queryrequest_t *qreq = NULL;
			queryresult_t *qres;
			const char *qerror = NULL;
			MYSQL_RES *mysqlres = NULL;
			int rows = -1;
			int columns = -1;
			int qesize = 0;

			if (!(qreq = SQL_PullRequest(server, needlock)))
				break;

			// pullrequest makes sure our condition is unlocked but we'll need
			// a lock next round
			needlock = true;

			// perform the query and fill out the result structure
			if (qmysql_query(server->mysql, qreq->query))
				qerror = qmysql_error(server->mysql);
			else // query succeeded
			{
				mysqlres = qmysql_store_result(server->mysql);
				if (mysqlres) // result set returned
				{
					rows = qmysql_num_rows(mysqlres);
					columns = qmysql_num_fields(mysqlres);
				}
				else if (qmysql_field_count(server->mysql) == 0) // no result set
				{
					rows = qmysql_affected_rows(server->mysql);
					if (rows < 0)
						rows = 0;
					columns = 0;
				}
				else // error
					qerror = qmysql_error(server->mysql);
			}

			if (qerror)
				qesize = Q_strlen(qerror);
			qres = (queryresult_t *)ZF_Malloc(sizeof(queryresult_t) + qesize);
			if (qres)
			{
				if (qerror)
					Q_strncpy(qres->error, qerror, qesize);
				qres->result = mysqlres;
				qres->rows = rows;
				qres->columns = columns;
				qres->request = qreq;
				qres->eof = true; // store result has no more rows to read afterwards
				qreq->next = NULL;

				SQL_PushResult(server, qres);
			}
			else // we're screwed here so bomb out
			{
				server->active = false;
				error = "MALLOC ERROR! Unable to allocate query result!";
				break;
			}
		}
	}

	if (server->mysql)
		qmysql_close(server->mysql);

	// if we have a server error we still need to put it on the queue
	if (error)
	{ 
		int esize = Q_strlen(error);
		queryresult_t *qres = (queryresult_t *)Z_Malloc(sizeof(queryresult_t) + esize);
		if (qres)
		{ // hopefully the qmysql_close gained us some memory otherwise we're pretty screwed
			qres->rows = qres->columns = -1;
			Q_strncpy(qres->error, error, esize);

			SQL_PushResult(server, qres);
		}
	}

	if (!tinit)
		qmysql_thread_end();

	return 0;
}

sqlserver_t *SQL_GetServer (int serveridx, qboolean inactives)
{
	if (serveridx < 0 || serveridx >= sqlservercount)
		return NULL;
	if (!inactives && sqlservers[serveridx]->active == false)
		return NULL;
	return sqlservers[serveridx];
}

queryresult_t *SQL_GetQueryResult (sqlserver_t *server, int queryidx)
{
	queryresult_t *qres;

	qres = server->currentresult;
	if (qres && qres->request && qres->request->num == queryidx)
		return qres;

	for (qres = server->persistresults; qres; qres = qres->next)
		if (qres->request && qres->request->num == queryidx)
			return qres;

	return NULL;
}

void SQL_DeallocResult(queryresult_t *qres)
{
	// deallocate current result
	if (qres->result)
		qmysql_free_result(qres->result);
	if (qres->request)
		Z_Free(qres->request);

	Z_Free(qres);
}

void SQL_ClosePersistantResult(sqlserver_t *server, queryresult_t *qres)
{
	queryresult_t *prev, *cur;

	prev = server->persistresults;
	if (prev == qres)
	{
		server->persistresults = prev->next;
		SQL_DeallocResult(prev);
		return;
	}

	for (cur = prev->next; cur; prev = cur, cur = prev->next)
	{
		if (cur == qres)
		{
			prev = cur->next;
			SQL_DeallocResult(cur);
			return;
		}
	}
}

void SQL_CloseResult(sqlserver_t *server, queryresult_t *qres)
{
	if (!qres)
		return;
	if (qres == server->currentresult)
	{
		SQL_DeallocResult(server->currentresult);
		server->currentresult = NULL;
		return;
	}
	// else we have a persistant query
	SQL_ClosePersistantResult(server, qres);
}

void SQL_CloseAllResults(sqlserver_t *server)
{
	queryresult_t *oldqres, *qres;

	// close orphaned results (we assume the lock is active or non-existant at this point)
	qres = server->results;
	while (qres)
	{
		oldqres = qres;
		qres = qres->next;
		SQL_DeallocResult(oldqres);
	}
	// close current
	if (server->currentresult)
	{
		SQL_DeallocResult(server->currentresult);
		server->currentresult = NULL;
	}
	// close persistant results
	qres = server->persistresults;
	while (qres)
	{
		oldqres = qres;
		qres = qres->next;
		SQL_DeallocResult(oldqres);
	}
	server->persistresults = NULL;
	// close server result
	if (server->serverresult)
	{
		SQL_DeallocResult(server->serverresult);
		server->serverresult = NULL;
	}
}

char *SQL_ReadField (sqlserver_t *server, queryresult_t *qres, int row, int col, qboolean fields)
{
	if (!qres->result) // TODO: partial resultset logic not implemented yet
		return NULL;
	else
	{ // store_result query
		if (qres->rows < row || qres->columns < col || col < 0)
			return NULL;

		if (row < 0)
		{ // fetch field name
			if (fields) // but only if we asked for them
			{
				MYSQL_FIELD *field;

				field = qmysql_fetch_field_direct(qres->result, col);

				if (!field)
					return NULL;
				else
					return field->name;
			}
			else
				return NULL;
		}
		else
		{ // fetch data
			MYSQL_ROW sqlrow;

			qmysql_data_seek(qres->result, row);
			sqlrow = qmysql_fetch_row(qres->result);
			if (!sqlrow || !sqlrow[col])
				return NULL;
			else
				return sqlrow[col];
		}
	}
}

int SQL_NewServer(char *driver, char **paramstr)
{
	sqlserver_t *server;
	int serverref;
	int drvchoice;
	int paramsize[SQL_CONNECT_PARAMS];
	int i, tsize;

	for (i = 0; i < SQL_CONNECT_PARAMS; i++)
		paramsize[i] = Q_strlen(paramstr[i]);

	if (Q_strcasecmp(driver, "mysql") == 0)
		drvchoice = SQLDRV_MYSQL;
	else if (Q_strcasecmp(driver, "sqlite") == 0)
		return -1; // drvchoice = SQLDRV_SQLITE;
	else // invalid driver choice so we bomb out
		return -1;

	// alloc or realloc sql servers array
	if (sqlservers == NULL)
	{
		serverref = 0;
		sqlservercount = 1;
		sqlservers = (sqlserver_t **)BZ_Malloc(sizeof(sqlserver_t *));
	}
	else
	{
		serverref = sqlservercount;
		sqlservercount++;
		sqlservers = (sqlserver_t **)BZ_Realloc(sqlservers, sizeof(sqlserver_t *) * sqlservercount);
	}

	// assemble server structure
	tsize = 0;
	for (i = 0; i < SQL_CONNECT_STRUCTPARAMS; i++)
		tsize += paramsize[i] + 1;	// allocate extra space for host and user only

	server = (sqlserver_t *)Z_Malloc(sizeof(sqlserver_t) + tsize);
	server->connectparams = (char **)BZ_Malloc(sizeof(char *) * SQL_CONNECT_PARAMS);

	tsize = 0;
	for (i = 0; i < SQL_CONNECT_STRUCTPARAMS; i++)
	{
		server->connectparams[i] = ((char *)(server + 1)) + tsize;
		Q_strncpy(server->connectparams[i], paramstr[i], paramsize[i]);
		// string should be null-terminated due to Z_Malloc
		tsize += paramsize[i] + 1;
	}
	for (i = SQL_CONNECT_STRUCTPARAMS; i < SQL_CONNECT_PARAMS; i++)
	{
		server->connectparams[i] = (char *)Z_Malloc(sizeof(char) * (paramsize[i] + 1));
		Q_strncpy(server->connectparams[i], paramstr[i], paramsize[i]);
		// string should be null-terminated due to Z_Malloc
	}

	sqlservers[serverref] = server;

	server->driver = (sqldrv_t)drvchoice;
	server->querynum = 1;
	server->active = true;
	server->requestcondv = Sys_CreateConditional();
	server->resultlock = Sys_CreateMutex();

	if (!server->requestcondv || !server->resultlock)
	{
		if (server->requestcondv)
			Sys_DestroyConditional(server->requestcondv);
		if (server->resultlock)
			Sys_DestroyMutex(server->resultlock);
		Z_Free(server);
		sqlservercount--;
		return -1;
	}

	server->thread = Sys_CreateThread(sql_serverworker, (void *)server, THREADP_NORMAL, 1024);
	
	if (!server->thread)
	{
		Z_Free(server);
		sqlservercount--;
		return -1;
	}

	return serverref;
}

int SQL_NewQuery(sqlserver_t *server, int callfunc, int type, int self, float selfid, int other, float otherid, char *str)
{
	int qsize = Q_strlen(str);
	queryrequest_t *qreq;
	int querynum;

	qreq = (queryrequest_t *)ZF_Malloc(sizeof(queryrequest_t) + qsize);
	if (qreq)
	{
		qreq->persistant = (type == 1);
		qreq->callback = callfunc;

		qreq->selfent = self;
		qreq->selfid = selfid;
		qreq->otherent = other;
		qreq->otherid = otherid;

		querynum = qreq->num = server->querynum;
		// prevent the reference num from getting too big to prevent FP problems
		if (++server->querynum > 1000000)
			server->querynum = 1; 
				
		Q_strncpy(qreq->query, str, qsize);

		SQL_PushRequest(server, qreq);
		Sys_ConditionSignal(server->requestcondv);

		return querynum;
	}

	return -1;
}

void SQL_Disconnect(sqlserver_t *server)
{
	server->active = false;

	// force the threads to reiterate requests and hopefully terminate
	Sys_ConditionBroadcast(server->requestcondv);
}

void SQL_Escape(sqlserver_t *server, char *src, char *dst, int dstlen)
{
	int srclen;

	switch (server->driver)
	{
	case SQLDRV_MYSQL:
		srclen = strlen(dst);
		if (srclen > (dstlen / 2 - 1))
			dst[0] = '\0';
		qmysql_real_escape_string(server->mysql, dst, src, srclen);
		break;
	default:
		dst[0] = '\0';
	}
}

char *SQL_Info(sqlserver_t *server)
{
	switch (server->driver)
	{
	case SQLDRV_MYSQL:
		return va("mysql: %s", qmysql_get_client_info());
		break;
	default:
		return "unknown";
	}
}

qboolean SQL_Available(void)
{
	return sqlavailable;
}

/* SQL related commands */
void SQL_Status_f(void)
{
	int i;

	Con_Printf("%i connections\n", sqlservercount);
	for (i = 0; i < sqlservercount; i++)
	{
		int reqnum = 0;
		int resnum = 0;
		queryrequest_t *qreq;
		queryresult_t *qres;

		sqlserver_t *server = sqlservers[i];

		Sys_LockMutex(server->resultlock);
		Sys_LockConditional(server->requestcondv);
		for (qreq = server->requests; qreq; qreq = qreq->next)
			reqnum++;
		for (qres = server->results; qres; qres = qres->next)
			resnum++;

		Con_Printf("#%i %s@%s: %s\n",
			i,
			server->connectparams[1],
			server->connectparams[0],
			server->active ? "active" : "inactive");

		if (reqnum)
		{
			Con_Printf ("- %i requests\n");
			for (qreq = server->requests; qreq; qreq = qreq->next)
			{
				Con_Printf ("  query #%i: %s\n",
					qreq->num,
					qreq->query);
				// TODO: function lookup?
			}
		}

		if (resnum)
		{
			Con_Printf ("- %i results\n");
			for (qres = server->results; qres; qres = qres->next)
			{
				Con_Printf ("  * %i rows, %i columns", 
					qres->rows,
					qres->columns);
				if (qres->error[0])
					Con_Printf(", error %s\n", qres->error);
				else
					Con_Printf("\n");
				// TODO: request info?
			}
		}

		if (server->serverresult)
			Con_Printf ("server result: error %s\n", server->serverresult->error);

		// TODO: list all requests, results here
		Sys_UnlockMutex(server->resultlock);
		Sys_UnlockConditional(server->requestcondv);
	}
}

void SQL_Kill_f (void)
{
	sqlserver_t *server;

	if (Cmd_Argc() < 2)
	{
		Con_Printf ("Syntax: %s serverid\n", Cmd_Argv(0));
		return;
	}

	server = SQL_GetServer(atoi(Cmd_Argv(1)), false);
	if (server)
	{
		server->active = false;
		Sys_ConditionBroadcast(server->requestcondv);
		return;
	}
}

void SQL_Killall_f (void)
{
	SQL_KillServers();
}

void SQL_ServerCycle (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;

	for (i = 0; i < sqlservercount; i++)
	{
		sqlserver_t *server = sqlservers[i];
		queryresult_t *qres;

		while (qres = SQL_PullResult(server))
		{
			qres->next = NULL;
			if (qres->request && qres->request->callback)
			{
				if (server->active)
				{ // only process results to callback if server is active
					edict_t *ent;

					server->currentresult = qres;
					G_FLOAT(OFS_PARM0) = i;
					G_FLOAT(OFS_PARM1) = qres->request->num;
					G_FLOAT(OFS_PARM2) = qres->rows;
					G_FLOAT(OFS_PARM3) = qres->columns;
					G_FLOAT(OFS_PARM4) = qres->eof;

					// recall self and other references
					ent = PROG_TO_EDICT(prinst, qres->request->selfent);
					if (ent->isfree || ent->xv->uniquespawnid != qres->request->selfid)
						pr_global_struct->self = pr_global_struct->world;
					else
						pr_global_struct->self = qres->request->selfent;
					ent = PROG_TO_EDICT(prinst, qres->request->otherent);
					if (ent->isfree || ent->xv->uniquespawnid != qres->request->otherid)
						pr_global_struct->other = pr_global_struct->world;
					else
						pr_global_struct->other = qres->request->otherent;

					PR_ExecuteProgram(prinst, qres->request->callback);

					if (qres->eof)
					{
						if (server->currentresult)
						{
							if (server->currentresult->request && server->currentresult->request->persistant)
							{
								// move into persistant list
								server->currentresult->next = server->persistresults;
								server->persistresults = server->currentresult;
							}
							else // just close the query
								SQL_CloseResult(server, server->currentresult);
						}
					}
					// TODO: else we move a request back into the queue?
				}
			}
			else // error or server-only result
			{
				if (server->serverresult)
					Z_Free(server->serverresult);
				server->serverresult = qres;
			}
		}
		server->currentresult = NULL;
	}
}

void SQL_MYSQLInit(void)
{
	if (!(mysqlhandle = Sys_LoadLibrary("libmysql", mysqlfuncs)))
	{
		Con_Printf("mysql client didn't load\n");
		return;
	}

	if (qmysql_thread_safe())
	{
		if (!qmysql_library_init(0, NULL, NULL))
		{
			Con_Printf("MYSQL backend loaded\n");
			sqlavailable = true;
			return;
		}
		else
			Con_Printf("MYSQL library init failed!\n");
	}
	else
		Con_Printf("MYSQL client is not thread safe!\n");

	Sys_CloseLibrary(mysqlhandle);
	sqlavailable = false;
}

void SQL_Init(void)
{
	Cmd_AddCommand ("sqlstatus", SQL_Status_f);
	Cmd_AddCommand ("sqlkill", SQL_Kill_f);
	Cmd_AddCommand ("sqlkillall", SQL_Killall_f);

	Cvar_Register(&sql_driver, SQLCVAROPTIONS);
	Cvar_Register(&sql_host, SQLCVAROPTIONS);
	Cvar_Register(&sql_username, SQLCVAROPTIONS);
	Cvar_Register(&sql_password, SQLCVAROPTIONS);
	Cvar_Register(&sql_defaultdb, SQLCVAROPTIONS);

	SQL_MYSQLInit();
}

void SQL_KillServers(void)
{
	int i;
	for (i = 0; i < sqlservercount; i++)
	{
		sqlserver_t *server = sqlservers[i];
		queryrequest_t *qreq, *oldqreq;

		server->active = false; // set thread to kill itself
		Sys_ConditionBroadcast(server->requestcondv); // force condition check
		Sys_WaitOnThread(server->thread); // wait on thread to die

		// server resource deallocation (TODO: should this be done in the thread itself?)
		Sys_DestroyConditional(server->requestcondv);
		Sys_DestroyMutex(server->resultlock);
		
		// close orphaned requests
		qreq = server->requests;
		while (qreq)
		{
			oldqreq = qreq;
			qreq = qreq->next;
			Z_Free(oldqreq);
		}

		SQL_CloseAllResults(server);

		// the alloc'ed connect params should get deallocated by the thread
		if (server->connectparams)
			BZ_Free(server->connectparams);

		Z_Free(server);
	}
	if (sqlservers)
		Z_Free(sqlservers);
	sqlservers = NULL;
	sqlservercount = 0;
}

void SQL_DeInit(void)
{
	sqlavailable = false;

	SQL_KillServers();

	qmysql_library_end();

	Sys_CloseLibrary(mysqlhandle);
}

#endif
