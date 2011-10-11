/*
 * data.c
 *
 *  Created on: Oct 6, 2011
 *      Author: tgrande
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LIBROUTER_DATA_PATH	"/var/run/librouter_data/"

static int _data_save(const char *filename, void *data, int len)
{
	int fd, n;
	char *buf;

	fd = open(filename, O_WRONLY | O_CREAT, 0660);

	if (fd < 0) {
		perror("Could not open data file\n");
		return -1;
	}

	buf = (char *) data;
	n = write(fd, buf, len);
	if (n < 0) {
		perror("Could not save data\n");
		return -1;
	}

	close(fd);

	return 0;
}

static int _data_load(const char *filename, void *data, int maxlen)
{
	int fd, n;
	char *buf;

	fd = open(filename, O_RDONLY);

	if (fd < 0) {
		if (errno != ENOENT)
			perror("Could not open data file\n");
		return -1;
	}

	buf = (char *) data;
	n = read(fd, buf, maxlen);
	if (n < 0) {
		perror("Could not load data\n");
		return -1;
	}

	close(fd);

	return 0;
}

int librouter_data_save(const char *name, void *data, int len)
{
	char filename[64];

	snprintf(filename, sizeof(filename), "%s%s", LIBROUTER_DATA_PATH, name);

	return _data_save(filename, data, len);
}

int librouter_data_load(const char *name, char *data, int maxlen)
{
	char filename[64];

	snprintf(filename, sizeof(filename), "%s%s", LIBROUTER_DATA_PATH, name);

	return _data_load(filename, data, maxlen);
}
