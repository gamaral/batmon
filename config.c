/*
 * Copyright (c) Guillermo A. Amaral <g@maral.me>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

struct config_threshold_node_t
{
	struct config_threshold_t entry;
	struct config_threshold_node_t *next;
};

struct config_handle_t
{
	int fd;
	struct config_threshold_node_t *cursor;
	struct config_threshold_node_t *root;
};

static int _config_parse_threshold(struct config_handle_t *, const char *);
static int _config_read_entries(struct config_handle_t *);
static void _config_clear_nodes(struct config_handle_t *);

/*****************************************************************************/

struct config_handle_t *
config_open(int fd)
{
	struct config_handle_t *h;

	assert(fd != -1);

	h = calloc(1, sizeof(struct config_handle_t));
	h->fd = fd;

	if (_config_read_entries(h)) {
		_config_clear_nodes(h);
		free(h), h = NULL;
	}

	return h;
}

void
config_close(struct config_handle_t *h)
{
	_config_clear_nodes(h);
	free(h);
}

struct config_threshold_t *
config_next(struct config_handle_t *h)
{
	if (h->cursor)
		h->cursor = h->cursor->next;
	else
		h->cursor = h->root;

	return (struct config_threshold_t *)h->cursor;
}

/*****************************************************************************/

int
_config_read_entries(struct config_handle_t *h)
{
	char line[128];
	char buf[64];
	char *cline;
	const char *eline;
	char *cbuf;
	const char * const ebuf = buf + sizeof(buf);
	ssize_t rc;
	ssize_t sbuf;

	cbuf = &buf[0];
	cline = &line[0];

	do {
		if (cbuf == ebuf)
			cbuf = &buf[0];

		sbuf = ebuf - cbuf;
		assert(sbuf >= 0);

		rc = read(h->fd, cbuf, sbuf);
		if (-1 == rc)
			return -1;

		eline = cbuf + rc;
		do {
			/* EOL check */
			if (*cbuf == '\n' || *cbuf == '\0') {
				*cline = '\0';
				cline = &line[0];

				if (strlen(line) > 0 &&
				    _config_parse_threshold(h, line))
					return -1;

				continue;
			}

			*cline++ = *cbuf;
		} while (++cbuf < eline);
	} while(rc == sbuf);

	return 0;
}

int
_config_parse_threshold(struct config_handle_t *h, const char *str)
{
	char buf[4];
	struct config_threshold_node_t *node;
	char *lend;
	const char *sep;
	unsigned short level;

	sep = index(str, ':');
	if (!sep || (ssize_t) sizeof(buf) < (sep - str))
		return -1;

	strncpy(buf, str, sep - str);
	buf[sep - str] = '\0';

	lend = NULL;
	level = strtoul(buf, &lend, 10);

	/* abort if threshold is contains invalid chars */
	if (!lend || *lend != '\0')
		return -1;

	++sep; /* move past separator */

	/* find first non-space character */
	while (*sep != '\0' && isspace(*sep))
		++sep;

	/* abort if command is empty */
	if (*sep == '\0')
		return -1;

	node = calloc(1, sizeof(struct config_threshold_node_t));
	node->next = h->root;
	node->entry.level = level;
	node->entry.cmd = strndup(sep, strlen(str) - (sep - str));
	h->root = node;

	return 0;
}

void
_config_clear_nodes(struct config_handle_t *h)
{
	struct config_threshold_node_t *node;

	assert(h);

	while ((node = h->root)) {
		h->root = node->next;
		free((void *)node->entry.cmd);
		free(node);
	}
}

