/*
 * This file is part of the zlog Library.
 *
 * Copyright (C) 2011 by Hardy Simpson <HardySimpson1984@gmail.com>
 *
 * Licensed under the LGPL v2.1, see the file COPYING in base directory.
 */

#include <string.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include "zc_defs.h"
#include "rotater.h"

#define ROLLING  1     /* aa.02->aa.03, aa.01->aa.02, aa->aa.01 */
#define SEQUENCE 2     /* aa->aa.03 */

typedef struct {
	int index;
} zlog_file_t;

void zlog_rotater_profile(zlog_rotater_t * a_rotater, int flag)
{
	zc_assert(a_rotater,);
	zc_profile(flag, "--rotater[%p][%p,%s,%d][%s,%s,%s,%ld,%ld,%d,%d,%d]--",
		a_rotater,

		&(a_rotater->lock_mutex),
		a_rotater->lock_file,
		a_rotater->lock_fd,

		a_rotater->base_path,
		a_rotater->archive_path,
		a_rotater->glob_path,
		(long)a_rotater->num_start_len,
		(long)a_rotater->num_end_len,
		a_rotater->num_width,
		a_rotater->mv_type,
		a_rotater->max_count
		);
	if (a_rotater->files) {
		int i;
		zlog_file_t *a_file;
		zc_arraylist_foreach(a_rotater->files, i, a_file) {
			zc_profile(flag, "[%d]->", a_file->index);
		}
	}
	return;
}

/*******************************************************************************/

static void zlog_file_del(zlog_file_t * a_file)
{
	zc_debug("del onefile[%p]", a_file);

	free(a_file);
}

static zlog_file_t *zlog_file_check_new(zlog_rotater_t * a_rotater, const char *path)
{
	int nread;
	zlog_file_t *a_file;

	/* base_path will not be in list */
	if (STRCMP(a_rotater->base_path, ==, path)) {
		return NULL;
	}

	/* omit dirs */
	if ((path)[strlen(path) - 1] == '/') {
		return NULL;
	}

	a_file = calloc(1, sizeof(zlog_file_t));
	if (!a_file) {
		zc_error("calloc fail, errno[%d]", errno);
		return NULL;
	}

	nread = 0;
	sscanf(path + a_rotater->num_start_len, "%d%n", &(a_file->index), &(nread));

	if (a_rotater->num_width != 0) {
		if (nread < a_rotater->num_width) {
			zc_warn("aa.1.log is not expect, need aa.01.log");
			goto err;
		}
	} /* else all file is ok */

	return a_file;
err:
	zlog_file_del(a_file);
	return NULL;
}


static int zlog_file_cmp(zlog_file_t * a_file_1, zlog_file_t * a_file_2)
{
	return (a_file_1->index > a_file_2->index);
}

static int zlog_rotater_add_archive_files(zlog_rotater_t * a_rotater)
{
	int rc = 0;
	glob_t glob_buf;
	size_t pathc;
	char **pathv;
	zlog_file_t *a_file;

	/* scan file which is aa.*.log and aa */
	rc = glob(a_rotater->glob_path, GLOB_ERR | GLOB_MARK | GLOB_NOSORT, NULL, &glob_buf);
	if (rc == GLOB_NOMATCH) {
		goto exit;
	} else if (rc) {
		zc_error("glob err, rc=[%d], errno[%d]", rc, errno);
		return -1;
	}

	pathv = glob_buf.gl_pathv;
	pathc = glob_buf.gl_pathc;

	a_rotater->files = zc_arraylist_new((zc_arraylist_del_fn)zlog_file_del, pathc);
	if (!a_rotater->files) {
		zc_error("zc_arraylist_new fail");
		goto err;
	}

	/* check and find match aa.[0-9]*.log, depend on num_width */
	for (; pathc-- > 0; pathv++) {
		a_file = zlog_file_check_new(a_rotater, *pathv);
		if (!a_file) {
			zc_warn("not the expect pattern file[%s]", *pathv);
			continue;
		}

		/* file in list aa.00, aa.01, aa.02... */
		rc = zc_arraylist_sortadd(a_rotater->files,
					(zc_arraylist_cmp_fn)zlog_file_cmp, a_file);
		if (rc) {
			zc_error("zc_arraylist_sortadd fail");
			goto err;
		}
	}

	zc_arraylist_reduce_size(a_rotater->files);

exit:
	globfree(&glob_buf);
	return 0;
err:
	globfree(&glob_buf);
	return -1;
}

static int zlog_rotater_seq_files(zlog_rotater_t * a_rotater,
		int file_open_flags, unsigned int file_perms, int *orig_fd)
{
	int rc = 0;
	int nwrite = 0;
	int i, j;
	zlog_file_t *a_file;
	char new_path[MAXLEN_PATH + 1];
	int fd;
	int min_idx = 0;

	memcpy(new_path, a_rotater->glob_path, a_rotater->num_start_len);

	if (a_rotater->files && zc_arraylist_len(a_rotater->files) > 0) {
		a_file = zc_arraylist_get(a_rotater->files, zc_arraylist_len(a_rotater->files) - 1);
		if (!a_file) {
			zc_error("zc_arraylist_get fail");
			return -1;
		}

		j = zc_max(zc_arraylist_len(a_rotater->files) - 1, a_file->index) + 1;
	} else {
		j = 0;
	}

	/* do the base_path mv  */
	nwrite = snprintf(new_path + a_rotater->num_start_len,
		sizeof(new_path) - a_rotater->num_start_len, "%0*d%s",
		a_rotater->num_width, j,
		a_rotater->glob_path + a_rotater->num_end_len);
	if (nwrite < 0 ||
		nwrite + a_rotater->num_start_len >= sizeof(new_path)) {
		zc_error("nwirte[%d], overflow or errno[%d]",
			nwrite + a_rotater->num_start_len, errno);
		return -1;
	}

	if (rename(a_rotater->base_path, new_path)) {
		zc_error("rename[%s]->[%s] fail, errno[%d]", a_rotater->base_path, new_path, errno);
		return -1;
	}

	fd = open(a_rotater->base_path,
		file_open_flags | O_WRONLY | O_APPEND | O_CREAT,
		file_perms);
	if (fd < 0) {
		zc_error("open file[%s] fail, errno[%d]", a_rotater->base_path, errno);
		return -1;
	}

	dup2(fd, *orig_fd);
	close(fd);

	if (!a_rotater->files || a_rotater->max_count <= 0) {
		return 0;
	}

	min_idx = 0;
	if (zc_arraylist_len(a_rotater->files) > a_rotater->max_count) {
		min_idx = zc_arraylist_len(a_rotater->files) - a_rotater->max_count;
	}

	for (i = 0; i < min_idx; i++) {
		a_file = zc_arraylist_get(a_rotater->files, i);
		if (!a_file) {
			zc_error("zc_arraylist_get fail");
			return -1;
		}

		/* unlink aa.0 aa.1 .. aa.(n-c) */
		nwrite = snprintf(new_path + a_rotater->num_start_len,
			sizeof(new_path) - a_rotater->num_start_len, "%0*d%s",
			a_rotater->num_width, a_file->index,
			a_rotater->glob_path + a_rotater->num_end_len);
		if (nwrite < 0 ||
			nwrite + a_rotater->num_start_len >= sizeof(new_path)) {
			zc_error("nwirte[%d], overflow or errno[%d]",
				nwrite + a_rotater->num_start_len, errno);
			return -1;
		}

		rc = unlink(new_path);
		if (rc) {
			zc_error("unlink[%s] fail, errno[%d]", new_path, errno);
			return -1;
		}
	}

	return 0;
}


static int zlog_rotater_roll_files(zlog_rotater_t * a_rotater,
		int file_open_flags, unsigned int file_perms, int *orig_fd)
{
	int i;
	int rc = 0;
	int nwrite;
	char old_path[MAXLEN_PATH + 1];
	char new_path[MAXLEN_PATH + 1];
	zlog_file_t *a_file;
	int fd;
	int max_idx = 0;

	memcpy(new_path, a_rotater->glob_path, a_rotater->num_start_len);

	if (!a_rotater->files) {
		goto mv_base_path;
	}

	max_idx = zc_arraylist_len(a_rotater->files);
	if (a_rotater->max_count > 0 && max_idx > a_rotater->max_count - 1) {
		max_idx = a_rotater->max_count - 1;
	}

	memcpy(old_path, a_rotater->glob_path, a_rotater->num_start_len);

	/* now in the list, aa.0 aa.1 aa.2 aa.02... */
	for (i = max_idx - 1; i > -1; i--) {
		a_file = zc_arraylist_get(a_rotater->files, i);
		if (!a_file) {
			zc_error("zc_arraylist_get fail");
			return -1;
		}

		nwrite = snprintf(old_path + a_rotater->num_start_len,
			sizeof(old_path) - a_rotater->num_start_len, "%0*d%s",
			a_rotater->num_width, a_file->index,
			a_rotater->glob_path + a_rotater->num_end_len);
		if (nwrite < 0 ||
			nwrite + a_rotater->num_start_len >= sizeof(old_path)) {
			zc_error("nwirte[%d], overflow or errno[%d]",
				nwrite + a_rotater->num_start_len, errno);
			return -1;
		}

		/* begin rename aa.01.log -> aa.02.log , using i, as index in list maybe repeat */
		nwrite = snprintf(new_path + a_rotater->num_start_len,
			sizeof(new_path) - a_rotater->num_start_len, "%0*d%s",
			a_rotater->num_width, i + 1,
			a_rotater->glob_path + a_rotater->num_end_len);
		if (nwrite < 0 ||
			nwrite + a_rotater->num_start_len >= sizeof(new_path)) {
			zc_error("nwirte[%d], overflow or errno[%d]",
				nwrite + a_rotater->num_start_len, errno);
			return -1;
		}

		if (rename(old_path, new_path)) {
			zc_error("rename[%s]->[%s] fail, errno[%d]", old_path, new_path, errno);
			return -1;
		}
	}

mv_base_path:
	/* do the base_path mv  */
	nwrite = snprintf(new_path + a_rotater->num_start_len,
		sizeof(new_path) - a_rotater->num_start_len, "%0*d%s",
		a_rotater->num_width, 0,
		a_rotater->glob_path + a_rotater->num_end_len);
	if (nwrite < 0 ||
		nwrite + a_rotater->num_start_len >= sizeof(new_path)) {
		zc_error("nwirte[%d], overflow or errno[%d]",
			nwrite + a_rotater->num_start_len, errno);
		return -1;
	}

	if (rename(a_rotater->base_path, new_path)) {
		zc_error("rename[%s]->[%s] fail, errno[%d]", a_rotater->base_path, new_path, errno);
		return -1;
	}

	fd = open(a_rotater->base_path,
		file_open_flags | O_WRONLY | O_APPEND | O_CREAT,
		file_perms);
	if (fd < 0) {
		zc_error("open file[%s] fail, errno[%d]", a_rotater->base_path, errno);
		return -1;
	}

	dup2(fd, *orig_fd);
	close(fd);

	if (!a_rotater->files || a_rotater->max_count <= 0) {
		return 0;
	}

	for (i = zc_arraylist_len(a_rotater->files) - 1; i > max_idx; i--) {
		a_file = zc_arraylist_get(a_rotater->files, i);
		if (!a_file) {
			zc_error("zc_arraylist_get fail");
			return -1;
		}

		nwrite = snprintf(old_path + a_rotater->num_start_len,
			sizeof(old_path) - a_rotater->num_start_len, "%0*d%s",
			a_rotater->num_width, a_file->index,
			a_rotater->glob_path + a_rotater->num_end_len);
		if (nwrite < 0 ||
			nwrite + a_rotater->num_start_len >= sizeof(old_path)) {
			zc_error("nwirte[%d], overflow or errno[%d]",
				nwrite + a_rotater->num_start_len, errno);
			return -1;
		}

		rc = unlink(old_path);
		if (rc) {
			zc_error("unlink[%s] fail, errno[%d]", old_path, errno);
			return -1;
		}
	}

	return 0;
}


static int zlog_rotater_parse_archive_path(zlog_rotater_t * a_rotater)
{
	int nread;
	int nwrite;
	char *p;
	size_t len;

	/* no archive path is set */
	if (!a_rotater->archive_path || a_rotater->archive_path[0] == '\0') {
		len = strlen(a_rotater->base_path);
		nwrite = snprintf(a_rotater->glob_path, sizeof(a_rotater->glob_path),
					"%s.*", a_rotater->base_path);
		if (nwrite < 0 || nwrite > sizeof(a_rotater->glob_path)) {
			zc_error("nwirte[%d], overflow or errno[%d]", nwrite, errno);
			return -1;
		}

		a_rotater->mv_type = ROLLING;
		a_rotater->num_width = 0;
		a_rotater->num_start_len = len + 1;
		a_rotater->num_end_len = len + 2;
	} else {
		/* find the 1st # */
		p = strchr(a_rotater->archive_path, '#');
		if (!p) {
			zc_error("no # in archive_path[%s]", a_rotater->archive_path);
			return -1;
		}

		nread = 0;
		sscanf(p, "#%d%n", &(a_rotater->num_width), &nread);
		if (nread == 0) nread = 1;
		if (*(p+nread) == 'r') {
			a_rotater->mv_type = ROLLING;
		} else if (*(p+nread) == 's') {
			a_rotater->mv_type = SEQUENCE;
		} else {
			zc_error("#r or #s not found");
			return -1;
		}

		/* copy and substitue #i to * in glob_path*/
		len = p - a_rotater->archive_path;
		if (len > sizeof(a_rotater->glob_path) - 1) {
			zc_error("sizeof glob_path not enough,len[%ld]", (long) len);
			return -1;
		}

		if (len > 0) {
			memcpy(a_rotater->glob_path, a_rotater->archive_path, len);
			nwrite = snprintf(a_rotater->glob_path + len, sizeof(a_rotater->glob_path) - len,
							"*%s", p + nread + 1);
			if (nwrite < 0 || nwrite > sizeof(a_rotater->glob_path) - len) {
				zc_error("nwirte[%d], overflow or errno[%d]", nwrite, errno);
				return -1;
			}

			a_rotater->num_start_len = len;
			a_rotater->num_end_len = len + 1;
		} else {
			/* compatible with archive_path only configured with #r or #s */
			len = strlen(a_rotater->base_path);
			nwrite = snprintf(a_rotater->glob_path, sizeof(a_rotater->glob_path),
						"%s.*", a_rotater->base_path);
			if (nwrite < 0 || nwrite > sizeof(a_rotater->glob_path)) {
				zc_error("nwirte[%d], overflow or errno[%d]", nwrite, errno);
				return -1;
			}

			a_rotater->num_start_len = len + 1;
			a_rotater->num_end_len = len + 2;
		}
	}

	return 0;
}

static void zlog_rotater_clean(zlog_rotater_t *a_rotater)
{
	a_rotater->base_path = NULL;
	a_rotater->archive_path = NULL;
	a_rotater->max_count = 0;
	a_rotater->mv_type = 0;
	a_rotater->num_width = 0;
	a_rotater->num_start_len = 0;
	a_rotater->num_end_len = 0;

	if (a_rotater->files) {
		zc_arraylist_del(a_rotater->files);
		a_rotater->files = NULL;
	}
}

static int zlog_rotater_lsmv(zlog_rotater_t *a_rotater,
		char *base_path, char *archive_path, int archive_max_count,
		int file_open_flags, unsigned int file_perms, int *orig_fd)
{
	int rc = 0;

	a_rotater->base_path = base_path;
	a_rotater->archive_path = archive_path;
	a_rotater->max_count = archive_max_count;
	rc = zlog_rotater_parse_archive_path(a_rotater);
	if (rc) {
		zc_error("zlog_rotater_parse_archive_path fail");
		goto err;
	}

	rc = zlog_rotater_add_archive_files(a_rotater);
	if (rc) {
		zc_error("zlog_rotater_add_archive_files fail");
		goto err;
	}

	if (a_rotater->mv_type == ROLLING) {
		rc = zlog_rotater_roll_files(a_rotater, file_open_flags, file_perms, orig_fd);
		if (rc) {
			zc_error("zlog_rotater_roll_files fail");
			goto err;
		}
	} else if (a_rotater->mv_type == SEQUENCE) {
		rc = zlog_rotater_seq_files(a_rotater, file_open_flags, file_perms, orig_fd);
		if (rc) {
			zc_error("zlog_rotater_seq_files fail");
			goto err;
		}
	}

	zlog_rotater_clean(a_rotater);
	return 0;
err:
	zlog_rotater_clean(a_rotater);
	return -1;
}

/*******************************************************************************/
static int zlog_rotater_trylock(zlog_rotater_t *a_rotater)
{
	struct flock fl;

	if (!a_rotater->lock_file)
		return 0;

	if (!ATOM_CASB(&(a_rotater->is_rotating), 0, 1)) {
		return -1;
	}

	fl.l_type = F_WRLCK;
	fl.l_start = 0;
	fl.l_whence = SEEK_SET;
	fl.l_len = 0;

	if (fcntl(a_rotater->lock_fd, F_SETLK, &fl)) {
		if (errno == EAGAIN || errno == EACCES) {
			/* lock by other process, that's right, go on */
			/* EAGAIN on linux */
			/* EACCES on AIX */
			zc_warn("fcntl lock fail, as file is lock by other process");
		} else {
			zc_error("lock fd[%d] fail, errno[%d]", a_rotater->lock_fd, errno);
		}

		ATOM_CASB(&(a_rotater->is_rotating), 1, 0);

		return -1;
	}

	return 0;
}

static int zlog_rotater_unlock(zlog_rotater_t *a_rotater)
{
	int rc = 0;
	struct flock fl;

	if (!a_rotater->lock_file)
		return 0;

	fl.l_type = F_UNLCK;
	fl.l_start = 0;
	fl.l_whence = SEEK_SET;
	fl.l_len = 0;

	if (fcntl(a_rotater->lock_fd, F_SETLK, &fl)) {
		rc = -1;
		zc_error("unlock fd[%s] fail, errno[%d]", a_rotater->lock_fd, errno);
	}

	if (!ATOM_CASB(&(a_rotater->is_rotating), 1, 0)) {
		rc = -1;
	}

	return rc;
}

int zlog_rotater_rotate(zlog_rotater_t *a_rotater,
						char *base_path,
						char *archive_path,
						int archive_max_count,
						int file_open_flags,
						unsigned int file_perms,
						int *orig_fd)
{
	int rc = 0;

	zc_assert(base_path, -1);

	if (zlog_rotater_trylock(a_rotater)) {
		zc_warn("zlog_rotater_trylock fail, maybe lock by other process or threads");
		return 0;
	}

	/* begin list and move files */
	rc = zlog_rotater_lsmv(a_rotater, base_path, archive_path, archive_max_count,
		file_open_flags, file_perms, orig_fd);
	if (rc) {
		zc_error("zlog_rotater_lsmv [%s] fail, return", base_path);
		rc = -1;
	} /* else if (rc == 0) */

	/* unlock file */
	if (zlog_rotater_unlock(a_rotater)) {
		zc_error("zlog_rotater_unlock fail");
	}

	return rc;
}

/*******************************************************************************/
