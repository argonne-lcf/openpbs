/*
 * Copyright (C) 1994-2021 Altair Engineering, Inc.
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
 * @file	pbs_orderjob.c
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <string.h>
#include <stdio.h>
#include "libpbs.h"
#include "dis.h"
#include "pbs_ecl.h"


/**
 * @brief
 *	-send order job batch request
 *
 * @param[in] c - connection handler
 * @param[in] job1 - job identifier
 * @param[in] job2 - job identifier
 * @param[in] extend - string to encode req
 *
 * @return      int
 * @retval      0       success
 * @retval      !0      error
 *
 */
int
__pbs_orderjob(int c, const char *job1, const char *job2, const char *extend)
{
	struct batch_reply *reply;
	int rc;

	if ((job1 == NULL) || (*job1 == '\0') ||
		(job2 == NULL) || (*job2 == '\0'))
		return (pbs_errno = PBSE_IVALREQ);

	/* initialize the thread context data, if not already initialized */
	if (pbs_client_thread_init_thread_context() != 0)
		return pbs_errno;

	/* lock pthread mutex here for this connection */
	/* blocking call, waits for mutex release */
	if (pbs_client_thread_lock_connection(c) != 0)
		return pbs_errno;

	/* setup DIS support routines for following DIS calls */
	DIS_tcp_funcs();

	if ((rc = encode_DIS_ReqHdr(c, PBS_BATCH_OrderJob, pbs_current_user)) ||
		(rc = encode_DIS_MoveJob(c, job1, job2)) ||
		(rc = encode_DIS_ReqExtend(c, extend))) {
		if (set_conn_errtxt(c, dis_emsg[rc]) != 0)
			pbs_errno = PBSE_SYSTEM;
		else
			pbs_errno = PBSE_PROTOCOL;

		pbs_client_thread_unlock_connection(c);
		return pbs_errno;
	}

	if (dis_flush(c)) {
		pbs_errno = PBSE_PROTOCOL;
		pbs_client_thread_unlock_connection(c);
		return pbs_errno;
	}

	/* read reply */
	reply = PBSD_rdrpy(c);
	PBSD_FreeReply(reply);
	rc = get_conn_errno(c);

	/* unlock the thread lock and update the thread context data */
	if (pbs_client_thread_unlock_connection(c) != 0)
		return pbs_errno;

	return rc;
}
