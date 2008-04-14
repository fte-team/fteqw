#include "qwsvdef.h"
#include "win_mysql.h"

MYSQLDLL_FUNC1(my_ulonglong, mysql_affected_rows, MYSQL *)
MYSQLDLL_FUNC2(my_bool, mysql_autocommit, MYSQL *, my_bool)
MYSQLDLL_FUNC4(my_bool, mysql_change_user, MYSQL *, const char *, const char *, const char *)
MYSQLDLL_NORETFUNC1(mysql_close, MYSQL *)
MYSQLDLL_FUNC1(my_bool, mysql_commit, MYSQL *)
MYSQLDLL_NORETFUNC2(mysql_data_seek, MYSQL_RES *, my_ulonglong)
MYSQLDLL_FUNC1(int, mysql_dump_debug_info, MYSQL *)
MYSQLDLL_FUNC1(unsigned int, mysql_errno, MYSQL *)
MYSQLDLL_FUNC1(const char *, mysql_error, MYSQL *)
MYSQLDLL_FUNC1(MYSQL_FIELD *, mysql_fetch_field, MYSQL_RES *)
MYSQLDLL_FUNC2(MYSQL_FIELD *, mysql_fetch_field_direct, MYSQL_RES *, unsigned int)
MYSQLDLL_FUNC1(MYSQL_FIELD *, mysql_fetch_fields, MYSQL_RES *)
MYSQLDLL_FUNC1(unsigned long *, mysql_fetch_lengths, MYSQL_RES *)
MYSQLDLL_FUNC1(MYSQL_ROW, mysql_fetch_row, MYSQL_RES *)
MYSQLDLL_FUNC2(MYSQL_FIELD_OFFSET, mysql_field_seek, MYSQL_RES *, MYSQL_FIELD_OFFSET)
MYSQLDLL_FUNC1(unsigned int, mysql_field_count, MYSQL *)
MYSQLDLL_FUNC1(MYSQL_FIELD_OFFSET, mysql_field_tell, MYSQL_RES *)
MYSQLDLL_NORETFUNC1(mysql_free_result, MYSQL_RES *)
MYSQLDLL_FUNC0(const char *, mysql_get_client_info)
MYSQLDLL_FUNC0(unsigned long, mysql_get_client_version)
MYSQLDLL_FUNC1(const char *, mysql_get_host_info, MYSQL *)
MYSQLDLL_FUNC1(unsigned long, mysql_get_server_version, MYSQL *)
MYSQLDLL_FUNC1(unsigned int, mysql_get_proto_info, MYSQL *)
MYSQLDLL_FUNC1(const char *, mysql_get_server_info, MYSQL *)
MYSQLDLL_FUNC1(const char *, mysql_info, MYSQL *)
MYSQLDLL_FUNC1(MYSQL *, mysql_init, MYSQL *)
MYSQLDLL_FUNC1(my_ulonglong, mysql_insert_id, MYSQL *)
MYSQLDLL_FUNC2(int, mysql_kill, MYSQL *, unsigned long)
MYSQLDLL_NORETFUNC0(mysql_server_end)
MYSQLDLL_FUNC3(int, mysql_server_init, int, char **, char **)
MYSQLDLL_FUNC2(MYSQL_RES *, mysql_list_dbs, MYSQL *, const char *)
MYSQLDLL_FUNC3(MYSQL_RES *, mysql_list_fields, MYSQL *, const char *, const char *)
MYSQLDLL_FUNC1(MYSQL_RES *, mysql_list_processes, MYSQL *)
MYSQLDLL_FUNC2(MYSQL_RES *, mysql_list_tables, MYSQL *, const char *)
MYSQLDLL_FUNC1(my_bool, mysql_more_results, MYSQL *)
MYSQLDLL_FUNC1(int, mysql_next_result, MYSQL *)
MYSQLDLL_FUNC1(unsigned int, mysql_num_fields, MYSQL_RES *)
MYSQLDLL_FUNC1(my_ulonglong, mysql_num_rows, MYSQL_RES *)
MYSQLDLL_FUNC3(int, mysql_options, MYSQL *, enum mysql_option, const char *)
MYSQLDLL_FUNC1(int, mysql_ping, MYSQL *)
MYSQLDLL_FUNC2(int, mysql_query, MYSQL *, const char *)
MYSQLDLL_FUNC8(MYSQL *, mysql_real_connect, MYSQL *, const char *, const char *, const char *, const char *, unsigned int, const char *, unsigned long)
MYSQLDLL_FUNC4(unsigned long, mysql_real_escape_string, MYSQL *, char *, const char *, unsigned long)
MYSQLDLL_FUNC3(int, mysql_real_query, MYSQL *, const char *, unsigned long)
MYSQLDLL_FUNC2(int, mysql_refresh, MYSQL *, unsigned int)
// MYSQLDLL_FUNC1(int, mysql_reload, MYSQL *)
MYSQLDLL_FUNC1(my_bool, mysql_rollback, MYSQL *)
MYSQLDLL_FUNC2(MYSQL_ROW_OFFSET, mysql_row_seek, MYSQL_RES *, MYSQL_ROW_OFFSET)
MYSQLDLL_FUNC1(MYSQL_ROW_OFFSET, mysql_row_tell, MYSQL_RES *)
MYSQLDLL_FUNC2(int, mysql_select_db, MYSQL *, const char *)
MYSQLDLL_FUNC2(int, mysql_set_server_option, MYSQL *, enum enum_mysql_set_option)
MYSQLDLL_FUNC1(const char *, mysql_sqlstate, MYSQL *)
MYSQLDLL_FUNC2(int, mysql_shutdown, MYSQL *, enum mysql_enum_shutdown_level)
MYSQLDLL_FUNC1(const char *, mysql_stat, MYSQL *)
MYSQLDLL_FUNC1(MYSQL_RES *, mysql_store_result, MYSQL *)
MYSQLDLL_NORETFUNC0(mysql_thread_end)
MYSQLDLL_FUNC1(unsigned long, mysql_thread_id, MYSQL *)
MYSQLDLL_FUNC0(my_bool, mysql_thread_init)
MYSQLDLL_FUNC0(unsigned int, mysql_thread_safe)
MYSQLDLL_FUNC1(MYSQL_RES *, mysql_use_result, MYSQL *)
MYSQLDLL_FUNC1(unsigned int, mysql_warning_count, MYSQL *)

/*
Not doing this:

void mysql_set_local_infile_default(MYSQL *mysql)
void mysql_set_local_infile_handler(MYSQL *mysql,
      int (*local_infile_init)(void **, const char *, void *),
      int (*local_infile_read)(void *, char *, unsigned int),
      void (*local_infile_end)(void *),
      int (*local_infile_error)(void *, char*, unsigned int),
      void *userdata)
*/

int mysql_dll_init()
{
	mysqldll = LoadLibrary("libmysql.dll");

	if (mysqldll == NULL)
		return 0;

	return 1;
}

int mysql_dll_close()
{
	if (mysqldll != NULL)
		FreeLibrary(mysqldll);

	return 1;
}