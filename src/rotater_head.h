/*
 * This file is part of the zlog Library.
 *
 * Copyright (C) 2011 by Hardy Simpson <HardySimpson1984@gmail.com>
 *
 * Licensed under the LGPL v2.1, see the file COPYING in base directory.
 */

#ifndef __zlog_rotater_head_h
#define __zlog_rotater_head_h

#include "zc_defs.h"

typedef struct zlog_rotater_s {
	pthread_mutex_t lock_mutex;
	char *lock_file;
	int lock_fd;
	volatile int is_rotating;

	/* single-use members */
	char *base_path;			/* aa.log */
	char *archive_path;			/* aa.#5i.log */
	char glob_path[MAXLEN_PATH + 1];	/* aa.*.log */
	size_t num_start_len;			/* 3, offset to glob_path */
	size_t num_end_len;			/* 6, offset to glob_path */
	int num_width;				/* 5 */
	int mv_type;				/* ROLLING or SEQUENCE */
	int max_count;
	zc_arraylist_t *files;
} zlog_rotater_t;

zlog_rotater_t *zlog_rotater_new(char *lock_file);
void zlog_rotater_del(zlog_rotater_t *a_rotater);

#endif
