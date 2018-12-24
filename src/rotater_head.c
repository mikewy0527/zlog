/*
 * This file is part of the zlog Library.
 *
 * Copyright (C) 2011 by Hardy Simpson <HardySimpson1984@gmail.com>
 *
 * Licensed under the LGPL v2.1, see the file COPYING in base directory.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include <errno.h>

#include "rotater_head.h"
#include "zc_defs.h"

/*******************************************************************************/
void zlog_rotater_del(zlog_rotater_t *a_rotater)
{
	zc_assert(a_rotater,);

	if (a_rotater->lock_file) {
		if (a_rotater->lock_fd) {
			if (close(a_rotater->lock_fd)) {
				zc_error("close fail, errno[%d]", errno);
			}
		}

		if (pthread_mutex_destroy(&(a_rotater->lock_mutex))) {
			zc_error("pthread_mutex_destroy fail, errno[%d]", errno);
		}
	}

	free(a_rotater);
	zc_debug("zlog_rotater_del[%p]", a_rotater);
	return;
}

zlog_rotater_t *zlog_rotater_new(char *lock_file)
{
	int fd = 0;
	zlog_rotater_t *a_rotater;

	a_rotater = calloc(1, sizeof(zlog_rotater_t));
	if (!a_rotater) {
		zc_error("calloc fail, errno[%d]", errno);
		return NULL;
	}

	if (!lock_file) {
		/* no need lock */
		return a_rotater;
	}

	if (pthread_mutex_init(&(a_rotater->lock_mutex), NULL)) {
		zc_error("pthread_mutex_init fail, errno[%d]", errno);
		free(a_rotater);
		return NULL;
	}

	/* depends on umask of the user here
	 * if user A create /tmp/zlog.lock 0600
	 * user B is unable to read /tmp/zlog.lock
	 * B has to choose another lock file except /tmp/zlog.lock
	 */
	fd = open(lock_file, O_RDWR | O_CREAT,
		  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (fd < 0) {
		zc_error("open file[%s] fail, errno[%d]", lock_file, errno);
		goto err;
	}

	a_rotater->lock_fd = fd;
	a_rotater->lock_file = lock_file;

	//zlog_rotater_profile(a_rotater, ZC_DEBUG);
	return a_rotater;
err:
	zlog_rotater_del(a_rotater);
	return NULL;
}

