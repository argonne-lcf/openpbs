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
 * @file	qorder.c
 * @brief
 * 	qorder - change the order of two batch jobs in a queue
 */
#include <pbs_config.h>   /* the master config generated by configure */

#include <string.h>
#include "cmds.h"
#include <pbs_version.h>
#include "pbs_ifl.h"
#include "net_connect.h"


int
main(int argc, char **argv, char **envp)
{
	char job_id1[PBS_MAXCLTJOBID+1];		/* from the command line */
	char job_id2[PBS_MAXCLTJOBID+1];		/* from the command line */
	char job_id1_out[PBS_MAXCLTJOBID+1];
	char job_id2_out[PBS_MAXCLTJOBID+1];
	char *pn;
	int  port1 = 0;
	int  port2 = 0;
	char server_out1[MAXSERVERNAME+1];
	char server_out2[MAXSERVERNAME+1];
	char svrtmp[MAXSERVERNAME+1];
	int connect;
	int stat=0;
	int rc = 0;

	/*test for real deal or just version and exit*/

	PRINT_VERSION_AND_EXIT(argc, argv);

	if (initsocketlib())
		return 1;

	if (argc != 3) {
		static char usage[]="usage: qorder job_identifier job_identifier\n";
		static char usag2[]="       qorder --version\n";
		fprintf(stderr, "%s", usage);
		fprintf(stderr, "%s", usag2);
		exit(2);
	}

	pbs_strncpy(job_id1, argv[1], sizeof(job_id1));
	pbs_strncpy(job_id2, argv[2], sizeof(job_id2));
	svrtmp[0] = '\0';
	if (get_server(job_id1, job_id1_out, svrtmp)) {
		fprintf(stderr, "qorder: illegally formed job identifier: %s\n", job_id1);
		exit(1);
	}
	if (*svrtmp == '\0') {
		if ((pn = pbs_default()) != NULL) {
			pbs_strncpy(svrtmp, pn, sizeof(svrtmp));
		} else {
			fprintf(stderr, "qorder: could not get default server: %s\n", job_id1);
			exit(1);
		}
	}

	if ((pn = strchr(svrtmp, (int)':')) != 0) {
		*pn = '\0';
		port1 = atoi(pn+1);
	}
	if (get_fullhostname(svrtmp, server_out1, MAXSERVERNAME) != 0) {
		fprintf(stderr, "qorder: invalid server name: %s\n", job_id1);
		exit(1);
	}

	svrtmp[0] = '\0';
	if (get_server(job_id2, job_id2_out, svrtmp)) {
		fprintf(stderr, "qorder: illegally formed job identifier: %s\n", job_id2);
		exit(1);
	}
	if (*svrtmp == '\0') {
		if ((pn = pbs_default()) != NULL) {
			pbs_strncpy(svrtmp, pn, sizeof(svrtmp));
		} else {
			fprintf(stderr, "qorder: could not get default server: %s\n", job_id1);
			exit(1);
		}
	}
	if ((pn = strchr(svrtmp, (int)':')) != 0) {
		*pn = '\0';
		port2 = atoi(pn+1);
	}
	if (get_fullhostname(svrtmp, server_out2, MAXSERVERNAME) != 0) {
		fprintf(stderr, "qorder: invalid server name: %s\n", job_id2);
		exit(1);
	}
	if ((strcmp(server_out1, server_out2) != 0) || (port1 != port2)) {
		fprintf(stderr, "qorder: both jobs ids must specify the same server\n");
		exit(1);
	}
	if (pn)
		*pn = ':';	/* restore : if it was present */

	/*perform needed security library initializations (including none)*/

	if (CS_client_init() != CS_SUCCESS) {
		fprintf(stderr, "qorder: unable to initialize security library.\n");
		exit(1);
	}

	connect = cnt2server(svrtmp);
	if (connect <= 0) {
		fprintf(stderr, "qorder: cannot connect to server %s (errno=%d)\n",
			pbs_server, pbs_errno);
		exit(1);;
	}

	stat = pbs_orderjob(connect, job_id1_out, job_id2_out, NULL);
	if (stat) {

		char job_id_both[PBS_MAXCLTJOBID + PBS_MAXCLTJOBID + 3];

		strcpy(job_id_both, job_id1_out);
		strcat(job_id_both, " or ");
		strcat(job_id_both, job_id2_out);
		prt_job_err("qorder", connect, job_id_both);
		rc = pbs_errno;
	}

	pbs_disconnect(connect);

	/*cleanup security library initializations before exiting*/
	CS_close_app();

	exit(rc);
}
