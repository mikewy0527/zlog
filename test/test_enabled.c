/*
 * This file is part of the zlog Library.
 *
 * Copyright (C) 2018 by Teracom Telemática S/A
 *
 * Licensed under the LGPL v2.1, see the file COPYING in base directory.
 */

#include <stdio.h>

#include "zlog.h"

enum {
	ZLOG_LEVEL_TRACE = 30,
	/* must equals conf file setting */
};

#define zlog_trace(cat, format, args...) \
	zlog(cat, __FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ZLOG_LEVEL_TRACE, format, ##args)

#define zlog_trace_enabled(cat) zlog_level_enabled(cat, ZLOG_LEVEL_TRACE)

int main(int argc, char** argv)
{
	int rc;
	zlog_category_t *zc;

	rc = zlog_init("test_enabled.conf");
	if (rc) {
		printf("init failed\n");
		return -1;
	}

	zc = zlog_get_category("my_cat");
	if (!zc) {
		printf("get cat fail\n");
		zlog_fini();
		return -2;
	}

	if (zlog_trace_enabled(zc)) {
		/* do something heavy to collect data */
		zlog_trace(zc, "hello, zlog - trace");
	}

	if (zlog_debug_enabled(zc)) {
		/* do something heavy to collect data */
		zlog_debug(zc, "hello, zlog - debug");
	}

	if (zlog_info_enabled(zc)) {
		/* do something heavy to collect data */
		zlog_info(zc, "hello, zlog - info");
	}

	zlog_fini();

	return 0;
}
