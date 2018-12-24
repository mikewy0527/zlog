/*
 * This file is part of the zlog Library.
 *
 * Copyright (C) 2018 by mikewy0527
 *
 * Licensed under the LGPL v2.1, see the file COPYING in base directory.
 */

#include <unistd.h>
#include <errno.h>

#include "fname_fd.h"
#include "zc_defs.h"

void zlog_fname_fd_del(zlog_fname_fd_t * a_fname_fd)
{
	if (!a_fname_fd)
		return;

	zc_debug("a_fname_fd->fd[%d]", a_fname_fd->fd);
	if (a_fname_fd->fd > 0) {
		fsync(a_fname_fd->fd);
		close(a_fname_fd->fd);
	}

	zc_debug("del fname_fd[%p]", a_fname_fd);
	free(a_fname_fd);
}

zlog_fname_fd_t * zlog_fname_fd_new(void)
{
	zlog_fname_fd_t *a_fname_fd;

	a_fname_fd = calloc(1, sizeof(zlog_fname_fd_t));
	if (!a_fname_fd) {
		zc_error("calloc fail, errno[%d]", errno);
		return NULL;
	}

	return a_fname_fd;
}
