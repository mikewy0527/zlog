/*
 * This file is part of the zlog Library.
 *
 * Copyright (C) 2018 by mikewy0527
 *
 * Licensed under the LGPL v2.1, see the file COPYING in base directory.
 */

#ifndef __zlog_fname_fd_h
#define __zlog_fname_fd_h

#include "zc_defs.h"

typedef struct zlog_fname_fd_s zlog_fname_fd_t;
struct zlog_fname_fd_s {
	int fd;
	int level;
	char mdc[MAXLEN_CFG_NAME + 1];
	char time_str[MAXLEN_CFG_NAME + 1];
	volatile int is_reopening;
} ;

void zlog_fname_fd_del(zlog_fname_fd_t * a_fname_fd);

zlog_fname_fd_t * zlog_fname_fd_new(void);

#endif
