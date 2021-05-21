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
 * @file	job_route.c
 * @brief
 * 		job_route.c - functions to route a job to another queue
 *
 * Included functions are:
 *
 *	job_route() - attempt to route a job to a new destination.
 *	add_dest()	- Add an entry to the list of bad destinations for a job.
 *	is_bad_dest()	- Check the job for a match of dest in the list of rejected destinations.
 *	default_router()	- basic function for "routing" jobs.
 *	queue_route()	- route any "ready" jobs in a specific queue
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <stdio.h>
#include <sys/types.h>

#include "pbs_ifl.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "pbs_error.h"
#include "list_link.h"
#include "attribute.h"
#include "server_limits.h"
#include "work_task.h"
#include "server.h"
#include "log.h"
#include "credential.h"
#include "libpbs.h"
#include "batch_request.h"
#include "resv_node.h"
#include "queue.h"

#include "job.h"
#include "reservation.h"
#include "pbs_nodes.h"
#include "svrfunc.h"
#include "libutil.h"
#include <memory.h>


/* Local Functions */


/* Global Data */

extern char	*msg_badstate;
extern char	*msg_routexceed;
extern char	*msg_routebad;
extern char	*msg_err_malloc;
extern time_t	 time_now;

/**
 * @brief
 * 		Add an entry to the list of bad destinations for a job.
 *
 * @see
 * 		default_router and post_routejob.
 *
 *	@param[in]	jobp - pointer to job structure
 *
 * @return	void
 */

void
add_dest(job *jobp)
{
	badplace	*bp;
	char	*baddest = jobp->ji_qs.ji_destin;

	bp = (badplace *)malloc(sizeof(badplace));
	if (bp == NULL) {
		log_err(errno, __func__, msg_err_malloc);
		return;
	}
	CLEAR_LINK(bp->bp_link);

	strcpy(bp->bp_dest, baddest);

	append_link(&jobp->ji_rejectdest, &bp->bp_link, bp);
	return;
}

/**
 * @brief
 * 		Check the job for a match of dest in the list of rejected destinations.
 *
 * @see
 * 		default_router
 *
 * @param[in]	jobp - pointer to job structure
 * @param[in]	dest - destination which needs to be matched.
 *
 *	Return: pointer if found, NULL if not.
 */

badplace *
is_bad_dest(job	*jobp, char *dest)
{
	badplace	*bp;

	bp = (badplace *)GET_NEXT(jobp->ji_rejectdest);
	while (bp) {
		if (strcmp(bp->bp_dest, dest) == 0)
			break;
		bp = (badplace *)GET_NEXT(bp->bp_link);
	}
	return (bp);
}


/**
 * @brief
 * 		default_router - basic function for "routing" jobs.
 *		Does a round-robin attempt on the destinations as listed,
 *		job goes to first destination that takes it.
 *
 *		If no destination will accept the job, PBSE_ROUTEREJ is returned,
 *		otherwise 0 is returned.
 *
 * @see
 * 		site_alt_router and job_route.
 *
 * @param[in,out]	jobp - pointer to job structure
 * @param[in]	qp - PBS queue.
 * @param[in]	retry_time - retry time before each attempt.
 *
 * @return	int
 * @retval	0	- success
 * @retval	PBSE_ROUTEREJ	- If no destination will accept the job
 */

int
default_router(job *jobp, struct pbs_queue *qp, long retry_time)
{
	struct array_strings *dests = NULL;
	char		     *destination;
	int		      last;

	if (is_qattr_set(qp, QR_ATR_RouteDestin)) {
		dests = get_qattr_arst(qp, QR_ATR_RouteDestin);
		last = dests->as_usedptr;
	} else
		last = 0;


	/* loop through all possible destinations */

	while (1) {
		if (jobp->ji_lastdest >= last) {
			jobp->ji_lastdest = 0;	/* have tried all */
			if (jobp->ji_retryok == 0) {
				log_event(PBSEVENT_JOB, PBS_EVENTCLASS_JOB,
					LOG_DEBUG,
					jobp->ji_qs.ji_jobid, msg_routebad);
				return (PBSE_ROUTEREJ);
			} else {

				/* set time to retry job */
				jobp->ji_qs.ji_un.ji_routet.ji_rteretry = retry_time;
				jobp->ji_retryok = 0;
				return (0);
			}
		}

		destination = dests->as_string[jobp->ji_lastdest++];

		if (is_bad_dest(jobp, destination))
			continue;

		switch (svr_movejob(jobp, destination, NULL)) {

			case -1:		/* permanent failure */
				add_dest(jobp);
				break;

			case 0:		/* worked */
			case 2:		/* deferred */
				return (0);

			case 1:		/* failed, but try destination again */
				jobp->ji_retryok = 1;
				break;
		}
	}
}

/**
 * @brief
 * 		job_route - route a job to another queue
 *
 * 		This is only called for jobs in a routing queue.
 * 		Loop over all the possible destinations for the route queue.
 * 		Check each one to see if it is ok to try it.  It could have been
 * 		tried before and returned a rejection.  If so, skip to the next
 * 		destination.  If it is ok to try it, look to see if it is a local
 * 		queue.  If so, it is an internal procedure to try/do the move.
 * 		If not, a child process is created to deal with it in the
 * 		function net_route(), see svr_movejob.c
 *
 * @see
 * 		queue_route
 *
 * @param[in]	jobp - pointer to job structure
 *
 *@return	int
 *@retval	0	- success
 *@retval	non-zero	- failure
 */

int
job_route(job *jobp)
{
	int			 bad_state = 0;
	time_t			 life;
	struct pbs_queue	*qp;
	long			 retry_time;

	/* see if the job is able to be routed */

	switch (get_job_state(jobp)) {

		case JOB_STATE_LTR_TRANSIT:
			return (0);		/* already going, ignore it */

		case JOB_STATE_LTR_QUEUED:
			break;			/* ok to try */

		case JOB_STATE_LTR_HELD:
			bad_state = !get_qattr_long(jobp->ji_qhdr, QR_ATR_RouteHeld);
			break;

		case JOB_STATE_LTR_WAITING:
			bad_state = !get_qattr_long(jobp->ji_qhdr, QR_ATR_RouteWaiting);
			break;

		case JOB_STATE_LTR_MOVED:
		case JOB_STATE_LTR_FINISHED:
			/*
			 * If the job in ROUTE_Q is already deleted (ji_state ==
			 * JOB_STATE_LTR_FINISHED) or routed (ji_state == JOB_STATE_LTR_MOVED)
			 * and kept for history purpose, then ignore it until being
			 * cleaned up by SERVER.
			 */
			return (0);

		default:
			log_eventf(PBSEVENT_DEBUG, PBS_EVENTCLASS_JOB, LOG_DEBUG,
				jobp->ji_qs.ji_jobid, "(%s) %s, state=%d",
				__func__, msg_badstate, get_job_state(jobp));
			return (0);
	}

	/* check the queue limits, can we route any (more) */

	qp = jobp->ji_qhdr;
	if (get_qattr_long(qp, QA_ATR_Started) == 0)
		return (0);	/* queue not started - no routing */

	if (is_qattr_set(qp, QA_ATR_MaxRun) && get_qattr_long(qp, QA_ATR_MaxRun) <= qp->qu_njstate[JOB_STATE_TRANSIT])
		return (0);	/* max number of jobs being routed */

	/* what is the retry time and life time of a job in this queue */

	if (is_qattr_set(qp, QR_ATR_RouteRetryTime))
		retry_time = (long)time_now + get_qattr_long(qp, QR_ATR_RouteRetryTime);
	else
		retry_time = (long)time_now + PBS_NET_RETRY_TIME;

	if (is_qattr_set(qp, QR_ATR_RouteLifeTime))
		life = jobp->ji_qs.ji_un.ji_routet.ji_quetime + get_qattr_long(qp, QR_ATR_RouteLifeTime);
	else
		life = 0;	/* forever */

	if (life && (life < time_now)) {
		log_event(PBSEVENT_JOB, PBS_EVENTCLASS_JOB, LOG_DEBUG,
			jobp->ji_qs.ji_jobid, msg_routexceed);
		return (PBSE_ROUTEEXPD);   /* job too long in queue */
	}

	if (bad_state) 	/* not currently routing this job */
		return (0);		   /* else ignore this job */

	if (get_qattr_long(qp, QR_ATR_AltRouter) == 0)
		return (default_router(jobp, qp, retry_time));
	else
		return (site_alt_router(jobp, qp, retry_time));
}


/**
 * @brief
 * 		queue_route - route any "ready" jobs in a specific queue
 *
 *		look for any job in the queue whose route retry time has
 *		passed.

 *		If the queue is "started" and if the number of jobs in the
 *		Transiting state is less than the max_running limit, then
 *		attempt to route it.
 *
 * @see
 * 		main
 *
 * @param[in]	pque	- PBS queue.
 *
 * @return	void
 */

void
queue_route(pbs_queue *pque)
{
	job *nxjb;
	job *pjob;
	int  rc;


	pjob = (job *)GET_NEXT(pque->qu_jobs);
	while (pjob) {
		nxjb = (job *)GET_NEXT(pjob->ji_jobque);
		if (pjob->ji_qs.ji_un.ji_routet.ji_rteretry <= time_now) {
			if ((rc = job_route(pjob)) == PBSE_ROUTEREJ)
				job_abt(pjob, msg_routebad);
			else if (rc == PBSE_ROUTEEXPD)
				job_abt(pjob, msg_routexceed);
		}
		pjob = nxjb;
	}
}
