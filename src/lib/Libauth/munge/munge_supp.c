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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <munge.h>
#include "pbs_config.h"
#include "libauth.h"
#include "pbs_ifl.h"

#if defined(MUNGE_PBS_SOCKET_PATH)
static char * munge_socket_path = MUNGE_PBS_SOCKET_PATH;
#else
static char * munge_socket_path = NULL;
#endif

static void (*logger)(int type, int objclass, int severity, const char *objname, const char *text);

#define __MUNGE_LOGGER(e, c, s, m)                                        \
	do {                                                              \
		if (logger == NULL) {                                     \
			if (s != LOG_DEBUG)                               \
				fprintf(stderr, "%s: %s\n", __func__, m); \
		} else {                                                  \
			logger(e, c, s, __func__, m);                     \
		}                                                         \
	} while (0)
#define MUNGE_LOG_ERR(m) __MUNGE_LOGGER(PBSEVENT_ERROR | PBSEVENT_FORCE, PBS_EVENTCLASS_SERVER, LOG_ERR, m)
#define MUNGE_LOG_DBG(m) __MUNGE_LOGGER(PBSEVENT_DEBUG | PBSEVENT_FORCE, PBS_EVENTCLASS_SERVER, LOG_DEBUG, m)

static char *munge_get_auth_data(char *, size_t);
static int munge_validate_auth_data(void *, char *, size_t);

/**
 * @brief
 *	munge_get_auth_data - Call Munge encode API's to get the authentication data for the current user
 *
 * @param[in] ebuf - buffer to hold error msg if any
 * @param[in] ebufsz - size of ebuf
 *
 * @return char *
 * @retval !NULL - success
 * @retval  NULL - failure
 *
 */
static char *
munge_get_auth_data(char *ebuf, size_t ebufsz)
{
	char *cred = NULL;
	uid_t myrealuid;
	struct passwd *pwent;
	struct group *grp;
	char payload[PBS_MAXUSER + PBS_MAXGRPN + 1] = {'\0'};
	munge_ctx_t munge_ctx = NULL;
	int munge_err = 0;

	/*
	 * ebuf passed to this function is initialized with nulls all through
	 * and ebufsz value passed is sizeof(ebuf) - 1
	 * So, we don't need to null terminate the last byte in the below
	 * all snprintf
	 *
	 * see pbs_auth_process_handshake_data()
	 */

	myrealuid = getuid();
	pwent = getpwuid(myrealuid);
	if (pwent == NULL) {
		snprintf(ebuf, ebufsz, "Failed to obtain user-info for uid = %d", myrealuid);
		MUNGE_LOG_ERR(ebuf);
		goto err;
	}

	grp = getgrgid(pwent->pw_gid);
	if (grp == NULL) {
		snprintf(ebuf, ebufsz, "Failed to obtain group-info for gid=%d", pwent->pw_gid);
		MUNGE_LOG_ERR(ebuf);
		goto err;
	}

	snprintf(payload, PBS_MAXUSER + PBS_MAXGRPN, "%s:%s", pwent->pw_name, grp->gr_name);

	if (munge_socket_path) {
		munge_ctx = munge_ctx_create();
		if (munge_ctx == NULL) {
			snprintf(ebuf, ebufsz, "Failed to create a MUNGE context");
			MUNGE_LOG_ERR(ebuf);
			goto err;
		}
		munge_ctx_set(munge_ctx, MUNGE_OPT_SOCKET, munge_socket_path);
	}

	munge_err = munge_encode(&cred, munge_ctx, payload, strlen(payload));
	if (munge_err != 0) {
		snprintf(ebuf, ebufsz, "MUNGE user-authentication on encode failed with `%s`", munge_strerror(munge_err));
		MUNGE_LOG_ERR(ebuf);
		goto err;
	}

fn_return:
	if (munge_ctx) {
		munge_ctx_destroy(munge_ctx);
	}
	return cred;

err:
	free(cred);
	cred = NULL;
	goto fn_return;
}

/**
 * @brief
 *	munge_validate_auth_data - validate given munge authentication data
 *
 * @param[in] auth_data - auth data to be verified
 * @param[in] ebuf - buffer to hold error msg if any
 * @param[in] ebufsz - size of ebuf
 *
 * @return int
 * @retval 0 - Success
 * @retval -1 - Failure
 *
 */
static int
munge_validate_auth_data(void *auth_data, char *ebuf, size_t ebufsz)
{
	uid_t uid;
	gid_t gid;
	int recv_len = 0;
	struct passwd *pwent = NULL;
	struct group *grp = NULL;
	void *recv_payload = NULL;
	munge_ctx_t munge_ctx = NULL;
	int munge_err = 0;
	char *p;
	int rc = -1;

	/*
	 * ebuf passed to this function is initialized with nulls all through
	 * and ebufsz value passed is sizeof(ebuf) - 1
	 * So, we don't need to null terminate the last byte in the below
	 * all snprintf
	 *
	 * see pbs_auth_process_handshake_data()
	 */

	if (munge_socket_path) {
		munge_ctx = munge_ctx_create();
		if (munge_ctx == NULL) {
			snprintf(ebuf, ebufsz, "Failed to create a MUNGE context");
			MUNGE_LOG_ERR(ebuf);
			goto err;
		}
		munge_ctx_set(munge_ctx, MUNGE_OPT_SOCKET, munge_socket_path);
	}

	munge_err = munge_decode(auth_data, munge_ctx, &recv_payload, &recv_len, &uid, &gid);
	if (munge_err != 0) {
		snprintf(ebuf, ebufsz, "MUNGE user-authentication on decode failed with `%s`", munge_strerror(munge_err));
		MUNGE_LOG_ERR(ebuf);
		goto err;
	}

	if ((pwent = getpwuid(uid)) == NULL) {
		snprintf(ebuf, ebufsz, "Failed to obtain user-info for uid = %d", uid);
		MUNGE_LOG_ERR(ebuf);
		goto err;
	}

	if ((grp = getgrgid(pwent->pw_gid)) == NULL) {
		snprintf(ebuf, ebufsz, "Failed to obtain group-info for gid=%d", gid);
		MUNGE_LOG_ERR(ebuf);
		goto err;
	}

	p = strtok((char *) recv_payload, ":");

	if (p && (strncmp(pwent->pw_name, p, PBS_MAXUSER) == 0)) /* inline with current pbs_iff we compare with username only */
		rc = 0;
	else {
		snprintf(ebuf, ebufsz, "User credentials do not match");
		MUNGE_LOG_ERR(ebuf);
	}

err:
	if (recv_payload)
		free(recv_payload);
	if (munge_ctx) {
		munge_ctx_destroy(munge_ctx);
	}
	return rc;
}

/********* START OF EXPORTED FUNCS *********/

/** @brief
 *	pbs_auth_set_config - Set config for this lib
 *
 * @param[in] config - auth config structure
 *
 * @return void
 *
 */
void
pbs_auth_set_config(const pbs_auth_config_t *config)
{
	logger = config->logfunc;
}

/** @brief
 *	pbs_auth_create_ctx - allocates external auth context structure for MUNGE authentication
 *
 * @param[in] ctx - pointer to external auth context to be allocated
 * @param[in] mode - AUTH_SERVER or AUTH_CLIENT
 * @param[in] conn_type - AUTH_USER_CONN or AUTH_SERVICE_CONN
 * @param[in] hostname - hostname of other authenticating party
 *
 * @note
 * 	Currently munge doesn't require any context data, so just return 0
 *
 * @return	int
 * @retval	0 - success
 * @retval	1 - error
 */
int
pbs_auth_create_ctx(void **ctx, int mode, int conn_type, const char *hostname)
{
	*ctx = NULL;
	return 0;
}

/** @brief
 *	pbs_auth_destroy_ctx - destroy external auth context structure for MUNGE authentication
 *
 * @param[in] ctx - pointer to external auth context
 *
 * @note
 * 	Currently munge doesn't require any context data, so just return 0
 *
 * @return void
 */
void
pbs_auth_destroy_ctx(void *ctx)
{
	ctx = NULL;
}

/** @brief
 *	pbs_auth_get_userinfo - get user, host and realm from authentication context
 *
 * @param[in] ctx - pointer to external auth context
 * @param[out] user - username assosiate with ctx
 * @param[out] host - hostname/realm assosiate with ctx
 * @param[out] realm - realm assosiate with ctx
 *
 * @return	int
 * @retval	0 on success
 * @retval	1 on error
 *
 * @note
 * 	Currently munge doesn't have context, so just return 0
 */
int
pbs_auth_get_userinfo(void *ctx, char **user, char **host, char **realm)
{
	*user = NULL;
	*host = NULL;
	*realm = NULL;
	return 0;
}

/** @brief
 *	pbs_auth_process_handshake_data - do Munge auth handshake
 *
 * @param[in] ctx - pointer to external auth context
 * @param[in] data_in - received auth token data (if any)
 * @param[in] len_in - length of received auth token data (if any)
 * @param[out] data_out - auth token data to send (if any)
 * @param[out] len_out - lenght of auth token data to send (if any)
 * @param[out] is_handshake_done - indicates whether handshake is done (1) or not (0)
 *
 * @return	int
 * @retval	0 on success
 * @retval	!0 on error
 */
int
pbs_auth_process_handshake_data(void *ctx, void *data_in, size_t len_in, void **data_out, size_t *len_out, int *is_handshake_done)
{
	int rc = -1;
	char ebuf[LOG_BUF_SIZE] = {'\0'};

	*len_out = 0;
	*data_out = NULL;
	*is_handshake_done = 0;

	if (len_in > 0) {
		char *data = (char *) data_in;
		/* enforce null char at given length of data */
		data[len_in - 1] = '\0';
		rc = munge_validate_auth_data(data, ebuf, sizeof(ebuf) - 1);
		if (rc == 0) {
			*is_handshake_done = 1;
			return 0;
		} else if (ebuf[0] != '\0') {
			*data_out = strdup(ebuf);
			if (*data_out != NULL)
				*len_out = strlen(ebuf);
		}
	} else {
		*data_out = (void *) munge_get_auth_data(ebuf, sizeof(ebuf) - 1);
		if (*data_out) {
			*len_out = strlen((char *) *data_out) + 1; /* +1 to include null char also in data_out */
			*is_handshake_done = 1;
			return 0;
		} else if (ebuf[0] != '\0') {
			*data_out = strdup(ebuf);
			if (*data_out != NULL)
				*len_out = strlen(ebuf);
		}
	}

	return 1;
}

/********* END OF EXPORTED FUNCS *********/
