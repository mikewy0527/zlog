/*
 * This file is part of the zlog Library.
 *
 * Copyright (C) 2011 by Hardy Simpson <HardySimpson1984@gmail.com>
 *
 * Licensed under the LGPL v2.1, see the file COPYING in base directory.
 */

#include "fmacros.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "zc_profile.h"
#include "zc_xplatform.h"

static void zc_time(char *time_str, size_t time_str_size)
{
	time_t t_sec;
	time_t t_millisec;
	time_t t_microsec;
	struct tm local_time;
	struct timeval tv;
	int len;

	if (gettimeofday(&tv, NULL) == 0) {
		t_sec = tv.tv_sec;
		t_millisec = tv.tv_usec / 1000;
		t_microsec = tv.tv_usec % 1000;
	} else {
		t_sec = time(NULL);
		t_millisec = 0;
		t_microsec = 0;
	}

	localtime_r(&t_sec, &local_time);
	strftime(time_str, time_str_size, "%m-%d %T", &local_time);
	len = strlen(time_str);
	snprintf(time_str + len, time_str_size - len, ",%03ld.%03ld (%lu)",
			t_millisec, t_microsec, syscall(SYS_gettid));

	return;
}

int zc_profile_inner(int flag, const char *file, const long line, const char *fmt, ...)
{
	va_list args;
	char time_str[36 + 1];
	FILE *fp = NULL;

	static char *debug_log = NULL;
	static char *error_log = NULL;
	static size_t init_flag = 0;

	if (!init_flag) {
		init_flag = 1;
		debug_log = getenv("ZLOG_PROFILE_DEBUG");
		error_log = getenv("ZLOG_PROFILE_ERROR");
	}

	switch (flag) {
	case ZC_DEBUG:
 		if (debug_log == NULL) return 0;
		fp = fopen(debug_log, "a");
		if (!fp) return -1;
		zc_time(time_str, sizeof(time_str));
		fprintf(fp, "%s DEBUG (%d:%s:%ld) ", time_str, getpid(), file, line);
		break;
	case ZC_WARN:
 		if (error_log == NULL) return 0;
		fp = fopen(error_log, "a");
		if (!fp) return -1;
		zc_time(time_str, sizeof(time_str));
		fprintf(fp, "%s WARN  (%d:%s:%ld) ", time_str, getpid(), file, line);
		break;
	case ZC_ERROR:
 		if (error_log == NULL) return 0;
		fp = fopen(error_log, "a");
		if (!fp) return -1;
		zc_time(time_str, sizeof(time_str));
		fprintf(fp, "%s ERROR (%d:%s:%ld) ", time_str, getpid(), file, line);
		break;
	}

	/* writing file twice(time & msg) is not atomic
	 * may cause cross
	 * but avoid log size limit */
	va_start(args, fmt);
	vfprintf(fp, fmt, args);
	va_end(args);
	fprintf(fp, "\n");

	fclose(fp);
	return 0;
}

