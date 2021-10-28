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
 * @file    pbs_release_nodes.c
 *
 * @brief
 *
 * 	Send release nodes request to batch job.
 *
 */

#include <pbs_config.h>

#include <errno.h>
#include "cmds.h"
#include "pbs_ifl.h"
#include <pbs_config.h>   /* the master config generated by configure */
#include <pbs_version.h>

#define USAGE	\
	"usage: pbs_release_nodes [-j job_identifier] host_or_vnode1 host_or_vnode2 ...\n" \
	"       pbs_release_nodes [-j job_identifier] -a\n" \
	"       pbs_release_nodes [-j job_identifier] -k <select string>\n" \
	"       pbs_release_nodes [-j job_identifier] -k <node count>\n" \
	"       pbs_release_nodes --version\n"

int
main(int argc, char **argv, char **envp) /* pbs_release_nodes */
{
	int c;
	int errflg=0;
	int any_failed=0;

	char job_id[PBS_MAXCLTJOBID];       /* from the command line */
	char job_id_out[PBS_MAXCLTJOBID];
	char server_out[MAXSERVERNAME];
	char rmt_server[MAXSERVERNAME];
	char *keep_opt = NULL;
	int  len;
	char *node_list = NULL;
	int connect;
	int stat=0;
	int k;
	int all_opt = 0;

#define GETOPT_ARGS "j:k:a"

	/*test for real deal or just version and exit*/
	PRINT_VERSION_AND_EXIT(argc, argv);

	if (initsocketlib())
		return 1;

	job_id[0] = '\0';
	while ((c = getopt(argc, argv, GETOPT_ARGS)) != EOF) {
		switch (c) {
			case 'j':
				pbs_strncpy(job_id, optarg, sizeof(job_id));
				break;
			case 'k':
				keep_opt = optarg;
				break;
			case 'a':
				all_opt = 1;
				break;
			default :
				errflg++;
		}
	}
	if (job_id[0] == '\0') {
		char *jid;
		jid = getenv("PBS_JOBID");
		pbs_strncpy(job_id, jid?jid:"", sizeof(job_id));
	}

	if (all_opt && keep_opt) {
		errflg++;
		fprintf(stderr, "pbs_release_nodes: -a and -k options cannot be used together\n");
	}

	if ((optind != argc) && keep_opt) {
		errflg++;
		fprintf(stderr, "pbs_release_nodes: cannot supply node list with -k option\n");
	}

	if (errflg ||
		((optind == argc) && !(all_opt || keep_opt)) ||
		((optind != argc) && all_opt) ) {
		fprintf(stderr, "%s", USAGE);
		exit(2);
	}

	if (job_id[0] == '\0') {
		fprintf(stderr, "pbs_release_nodes: No jobid given\n");
		exit(2);
	}

	/*perform needed security library initializations (including none)*/

	if (CS_client_init() != CS_SUCCESS) {
		fprintf(stderr, "pbs_release_nodes: unable to initialize security library.\n");
		exit(2);
	}

	len = 0;
	for (k = optind; k < argc; k++) {
		len += (strlen(argv[k]) + 1);	/* +1 for space */
	}

	node_list = (char *)malloc(len + 1);
	if (node_list == NULL) {
		fprintf(stderr, "failed to malloc to store data (error %d)", errno);
		exit(2);
	}
	node_list[0] = '\0';

	for (k = optind; k < argc; k++) {
		if (k != optind)
			strcat(node_list, "+");
		strcat(node_list, argv[k]);
	}
	if (get_server(job_id, job_id_out, server_out)) {
		fprintf(stderr, "pbs_release_nodes: illegally formed job identifier: %s\n", job_id);
		free(node_list);
		exit(2);
	}

	pbs_errno = 0;
	stat = 0;
	while(1) {
		connect = cnt2server(server_out);
		if (connect <= 0) {
			fprintf(stderr,
				"pbs_release_nodes: cannot connect to server %s (errno=%d)\n",
							pbs_server, pbs_errno);
			break;
		}
		

		stat = pbs_relnodesjob(connect, job_id_out, node_list, keep_opt);
		if (stat && (pbs_errno == PBSE_UNKJOBID)) {
			if (locate_job(job_id_out, server_out, rmt_server)) {
				/*
				 * job located at a different server
				 * retry connect on the new server
				 */
				pbs_disconnect(connect);
				strcpy(server_out, rmt_server);
			} else {
				prt_job_err("pbs_release_nodes", connect, job_id_out);
				break;
			}
		} else {
			char *info_msg;

			if (stat && (pbs_errno != PBSE_UNKJOBID)) {
				prt_job_err("pbs_release_nodes", connect, "");
			} else if ((info_msg = pbs_geterrmsg(connect)) != NULL) {
				/* print potential warning message */
				printf("pbs_release_nodes: %s\n", info_msg);
			}
			break;
		}
	}
	any_failed = pbs_errno;

	pbs_disconnect(connect);

	/*cleanup security library initializations before exiting*/
	CS_close_app();

	exit(any_failed);
}
