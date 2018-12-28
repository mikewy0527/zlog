/*
 * This file is part of the zlog Library.
 *
 * Copyright (C) 2011 by Hardy Simpson <HardySimpson1984@gmail.com>
 *
 * Licensed under the LGPL v2.1, see the file COPYING in base directory.
 */

#ifndef __zlog_rotater_h
#define __zlog_rotater_h

#include "zc_defs.h"
#include "rotater_head.h"

/*
 * return
 * -1	fail
 * 0	no rotate, or rotate and success
 */
int zlog_rotater_rotate(zlog_rotater_t *a_rotater,
						char *base_path,
						char *archive_path,
						int archive_max_count,
						int file_open_flags,
						unsigned int file_perms,
						int *orig_fd);

void zlog_rotater_profile(zlog_rotater_t *a_rotater, int flag);

#endif
