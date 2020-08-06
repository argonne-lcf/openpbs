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
 * @file	get_ttr.c
 * @brief
 *      Locate an attribute (attrl) by name (and resource).
 */

#include <pbs_config.h>   /* the master config generated by configure */
#include "cmds.h"
#include "pbs_ifl.h"
#include "libutil.h"


/**
 * @brief
 *      Locate an attribute (attrl) by name (and resource).
 *
 * @param[in] pattrl    - Attribute list.
 * @param[in] name      - name to find in attribute list.
 * @param[in] resc      - resource to find in attribute list.
 *
 * @return	pointer to string
 * @retval      value of the located name and resource from attribute list,
 * @retval 	othewise NULL.
 */

char *
get_attr(struct attrl *pattrl, char *name, char *resc)
{
	while (pattrl) {
		if (strcmp(name, pattrl->name) == 0) {
			if (resc) {
				if (strcmp(resc, pattrl->resource) == 0) {
					return (pattrl->value);
				}
			} else {
				return (pattrl->value);
			}
		}
		pattrl = pattrl->next;
	}
	return NULL;
}

/*
 * @brief
 *	check_max_job_sequence_id - retrieve the max_job_sequence_id attribute value
 *
 *	@param[in]server_attrs - Batch status
 *
 *	@retval  1	success
 *	@retval  0	error/attribute is not set
 *
 */
int
check_max_job_sequence_id(struct batch_status *server_attrs)
{
	char * value;
	value = get_attr(server_attrs->attribs, ATTR_max_job_sequence_id, NULL);
	if (value == NULL) {
		/* if server is not configured for max_job_sequence_id
		* or attribute is unset */
		return 0;
	} else {
		/* if attribute is set set */
		long long seq_id = 0;
		seq_id = strtoul(value, NULL, 10);
		if (seq_id > PBS_DFLT_MAX_JOB_SEQUENCE_ID) {
			return 1;
		}
		return 0;
	}
}
