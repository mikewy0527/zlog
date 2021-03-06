/*
 * This file is part of the zlog Library.
 *
 * Copyright (C) 2011 by Hardy Simpson <HardySimpson1984@gmail.com>
 *
 * Licensed under the LGPL v2.1, see the file COPYING in base directory.
 */

/**
 * @file rule.h
 * @brief rule decide to output in format by category & level
 */

#ifndef __zlog_rule_h
#define __zlog_rule_h

#include <stdio.h>
#include <pthread.h>

#include "zc_defs.h"
#include "format.h"
#include "thread.h"
#include "rotater.h"
#include "record.h"

typedef struct zlog_rule_s zlog_rule_t;

typedef int (*zlog_rule_output_fn) (zlog_rule_t * a_rule, zlog_thread_t * a_thread);

struct zlog_rule_s {
	char category[MAXLEN_CFG_NAME + 1];
	char compare_char;
	/*
	 * [*] log all level
	 * [.] log level >= rule level, default
	 * [=] log level == rule level
	 * [!] log level != rule level
	 */
	int level;
	unsigned char level_bitmap[32]; /* for category determine whether ouput or not */

	unsigned int file_perms;
	int file_open_flags;

	char *file_path;
	zc_arraylist_t *dynamic_specs;
	int static_fd;
	dev_t static_dev;
	ino_t static_ino;
	int is_reopening;

	pthread_mutex_t lock_mutex;
	zc_arraylist_t *fname_fds;
	int path_spec_flag;

	volatile size_t file_size;
	long archive_max_size;
	int archive_max_count;
	char *archive_path;
	zc_arraylist_t *archive_specs;

	FILE *pipe_fp;
	int pipe_fd;

	size_t fsync_period;
	size_t fsync_count;

	zc_arraylist_t *levels;
	int syslog_facility;

	zlog_format_t *format;
	zlog_rule_output_fn output;

	char *record_name;
	char *record_path;
	zlog_record_fn record_func;
};

zlog_rule_t *zlog_rule_new(char * line,
						   zc_arraylist_t * levels,
						   zlog_format_t * default_format,
						   zc_arraylist_t * formats,
						   unsigned int file_perms,
						   size_t fsync_period,
						   long archive_max_size,
						   int archive_max_count,
						   int * time_cache_count);

void zlog_rule_del(zlog_rule_t * a_rule);
void zlog_rule_profile(zlog_rule_t * a_rule, int flag);
int zlog_rule_match_category(zlog_rule_t * a_rule, char *category);
int zlog_rule_is_wastebin(zlog_rule_t * a_rule);
int zlog_rule_set_record(zlog_rule_t * a_rule, zc_hashtable_t *records);
int zlog_rule_output(zlog_rule_t * a_rule, zlog_thread_t * a_thread);

#endif
