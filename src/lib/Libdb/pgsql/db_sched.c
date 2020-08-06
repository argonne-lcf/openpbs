/*
 * Copyright (C) 1994-2020 Altair Engineering, Inc.
 * For more information, contact Altair at www.altair.com.
 *
 * This file is part of both the OpenPBS software ("OpenPBS")
 * and the PBS Professional ("PBS Pro") software.
 *
 * Open Source License Information:
 *
 * OpenPBS is free software. You can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * OpenPBS is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Commercial License Information:
 *
 * PBS Pro is commercially licensed software that shares a common core with
 * the OpenPBS software.  For a copy of the commercial license terms and
 * conditions, go to: (http://www.pbspro.com/agreement.html) or contact the
 * Altair Legal Department.
 *
 * Altair's dual-license business model allows companies, individuals, and
 * organizations to create proprietary derivative works of OpenPBS and
 * distribute them - whether embedded or bundled with other software -
 * under a commercial license agreement.
 *
 * Use of Altair's trademarks, including but not limited to "PBS™",
 * "OpenPBS®", "PBS Professional®", and "PBS Pro™" and Altair's logos is
 * subject to Altair's trademark licensing policies.
 */



/**
 *
 * @brief
 *      Implementation of the scheduler data access functions for postgres
 */
#include <pbs_config.h>   /* the master config generated by configure */
#include "pbs_db.h"
#include "db_postgres.h"
#include "libutil.h"

/**
 * @brief
 *	Prepare all the scheduler related sqls. Typically called after connect
 *	and before any other sql exeuction
 *
 * @param[in]	conn - Database connection handle
 *
 * @return      Error code
 * @retval	-1 - Failure
 * @retval	 0 - Success
 *
 */
int
db_prepare_sched_sqls(void *conn)
{
	char conn_sql[MAX_SQL_LENGTH];
	snprintf(conn_sql, MAX_SQL_LENGTH, "insert into "
		"pbs.scheduler( "
		"sched_name, "
		"sched_savetm, "
		"sched_creattm, "
		"attributes "
		") "
		"values ($1, localtimestamp, localtimestamp, hstore($2::text[]))");
	if (db_prepare_stmt(conn, STMT_INSERT_SCHED, conn_sql, 2) != 0)
		return -1;

	/* rewrite all attributes for a FULL update */
	snprintf(conn_sql, MAX_SQL_LENGTH, "update pbs.scheduler set "
		"sched_savetm = localtimestamp, "
		"attributes = attributes || hstore($2::text[]) "
		"where sched_name = $1");
	if (db_prepare_stmt(conn, STMT_UPDATE_SCHED, conn_sql, 2) != 0)
		return -1;

	snprintf(conn_sql, MAX_SQL_LENGTH, "update pbs.scheduler set "
		"sched_savetm = localtimestamp,"
		"attributes = attributes - $2::text[] "
		"where sched_name = $1");
	if (db_prepare_stmt(conn, STMT_REMOVE_SCHEDATTRS, conn_sql, 2) != 0)
		return -1;

	snprintf(conn_sql, MAX_SQL_LENGTH, "select "
		"sched_name, "
		"hstore_to_array(attributes) as attributes "
		"from "
		"pbs.scheduler "
		"where sched_name = $1");
	if (db_prepare_stmt(conn, STMT_SELECT_SCHED, conn_sql, 1) != 0)
		return -1;

	snprintf(conn_sql, MAX_SQL_LENGTH, "select "
		"sched_name, "
		"hstore_to_array(attributes) as attributes "
		"from "
		"pbs.scheduler ");
	if (db_prepare_stmt(conn, STMT_SELECT_SCHED_ALL, conn_sql, 0) != 0)
		return -1;

	snprintf(conn_sql, MAX_SQL_LENGTH, "delete from pbs.scheduler where sched_name = $1");
	if (db_prepare_stmt(conn, STMT_DELETE_SCHED, conn_sql, 1) != 0)
		return -1;

	return 0;
}

/**
 * @brief
 *	Insert scheduler data into the database
 *
 * @param[in]	conn - Connection handle
 * @param[in]	obj  - Information of scheduler to be inserted
 *
 * @return      Error code
 * @retval	-1 - Failure
 * @retval	 0 - Success
 *
 */
int
pbs_db_save_sched(void *conn, pbs_db_obj_info_t *obj, int savetype)
{
	pbs_db_sched_info_t *psch = obj->pbs_db_un.pbs_db_sched;
	char *stmt = NULL;
	int params;
	int rc = 0;
	char *raw_array = NULL;

	SET_PARAM_STR(conn_data, psch->sched_name, 0);

	/* sched does not have a QS area, so ignoring that */

	if ((psch->db_attr_list.attr_count > 0) || (savetype & OBJ_SAVE_NEW)) {
		int len = 0;
		/* convert attributes to postgres raw array format */
		if ((len = attrlist_to_dbarray(&raw_array, &psch->db_attr_list)) <= 0)
			return -1;

		SET_PARAM_BIN(conn_data, raw_array, len, 1);
		stmt = STMT_UPDATE_SCHED;
		params = 2;
	}

	if (savetype & OBJ_SAVE_NEW)
		stmt = STMT_INSERT_SCHED;

	if (stmt)
		rc = db_cmd(conn, stmt, params);

	return rc;
}

/**
 * @brief
 *	Load scheduler data from the row into the scheduler object
 *
 * @param[in]	res - Resultset from a earlier query
 * @param[out]	psch  - Scheduler object to load data into
 * @param[in]	row - The current row to load within the resultset
 *
 * @return      Error code
 * @retval	-1 - On Error
 * @retval	 0 - On Success
 * @retval	>1 - Number of attributes
 */
static int
load_sched(PGresult *res, pbs_db_sched_info_t *psch, int row)
{
	char *raw_array;
	static int sched_name_fnum, attributes_fnum;
	static int fnums_inited = 0;

	if (fnums_inited == 0) {
		sched_name_fnum = PQfnumber(res, "sched_name");
		attributes_fnum = PQfnumber(res, "attributes");
		fnums_inited = 1;
	}

	GET_PARAM_STR(res, row, psch->sched_name, sched_name_fnum);
	GET_PARAM_BIN(res, row, raw_array, attributes_fnum);

	/* convert attributes from postgres raw array format */
	return (dbarray_to_attrlist(raw_array, &psch->db_attr_list));

}

/**
 * @brief
 *	Load scheduler data from the database
 *
 * @param[in]	conn - Connection handle
 * @param[out]	obj  - Load scheduler information into this object
 *
 * @return      Error code
 * @retval	-1 - Failure
 * @retval	 0 - Success
 * @retval	 1 -  Success but no rows loaded
 *
 */
int
pbs_db_load_sched(void *conn, pbs_db_obj_info_t *obj)
{
	PGresult *res;
	int rc;
	pbs_db_sched_info_t *psch = obj->pbs_db_un.pbs_db_sched;

	SET_PARAM_STR(conn_data, psch->sched_name, 0);

	if ((rc = db_query(conn, STMT_SELECT_SCHED, 1, &res)) != 0)
		return rc;

	rc = load_sched(res, psch, 0);

	PQclear(res);
	
	return rc;
}

/**
 * @brief
 *	Find scheduler
 *
 * @param[in]	conn - Connection handle
 * @param[out]	st   - The cursor state variable updated by this query
 * @param[in]	obj  - Information of sched to be found
 * @param[in]	opts - Any other options (like flags, timestamp)
 *
 * @return      Error code
 * @retval	-1 - Failure
 * @retval	 0 - Success
 * @retval	 1 -  Success but no rows found
 *
 */
int
pbs_db_find_sched(void *conn, void *st, pbs_db_obj_info_t *obj,
	pbs_db_query_options_t *opts)
{
	PGresult *res;
	db_query_state_t *state = (db_query_state_t *) st;
	int rc;
	int params;

	if (!state)
		return -1;

	params = 0;
	if ((rc = db_query(conn, STMT_SELECT_SCHED_ALL, params, &res)) != 0)
		return rc;

	state->row = 0;
	state->res = res;
	state->count = PQntuples(res);

	return 0;
}

/**
 * @brief
 *	Deletes attributes of a Scheduler
 *
 * @param[in]	conn - Connection handle
 * @param[in]	obj_id  - Scheduler id
 * @param[in]	attr_list - List of attributes
 *
 * @return      Error code
 * @retval	 0 - Success
 * @retval	-1 - On Failure
 *
 */
int
pbs_db_del_attr_sched(void *conn, void *obj_id, pbs_db_attr_list_t *attr_list)
{
	char *raw_array = NULL;
	int len = 0;
	int rc = 0;

	if ((len = attrlist_to_dbarray_ex(&raw_array, attr_list, 1)) <= 0)
		return -1;

	SET_PARAM_STR(conn_data, obj_id, 0);
	SET_PARAM_BIN(conn_data, raw_array, len, 1);

	rc = db_cmd(conn, STMT_REMOVE_SCHEDATTRS, 2);

	return rc;
}

/**
 * @brief
 *	Get the next scheduler from the cursor
 *
 * @param[in]	conn - Connection handle
 * @param[out]	st   - The cursor state
 * @param[in]	obj  - Scheduler information is loaded into this object
 *
 * @return      Error code
 * @retval	-1 - Failure
 * @retval	 0 - Success
 *
 */
int
pbs_db_next_sched(void *conn, void *st, pbs_db_obj_info_t *obj)
{
	db_query_state_t *state = (db_query_state_t *) st;

	return (load_sched(state->res, obj->pbs_db_un.pbs_db_sched, state->row));

}

/**
 * @brief
 *	Delete the scheduler from the database
 *
 * @param[in]	conn - Connection handle
 * @param[in]	obj  - scheduler information
 *
 * @return      Error code
 * @retval	-1 - Failure
 * @retval	 0 - Success
 *
 */
int
pbs_db_delete_sched(void *conn, pbs_db_obj_info_t *obj)
{
	pbs_db_sched_info_t *sc = obj->pbs_db_un.pbs_db_sched;
	SET_PARAM_STR(conn_data, sc->sched_name, 0);
	return (db_cmd(conn, STMT_DELETE_SCHED, 1));
}
