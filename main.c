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

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"

#define BATMON_UNUSED(x) (void)x
#define BATMON_CONFIG "/etc/batmon.conf"
#define BATMON_BATTERY "BAT0"

enum status_t
{
	STATUS_UNKNOWN,
	STATUS_AC,
	STATUS_BATTERY
};

static int _check_status(int);
static int _read_charge(int);
static void _signal_handler(int);
static void _setup_sigaction(void);

static int s_working = 1;

/*****************************************************************************/

int
main(int argc, char *argv[])
{
	enum status_t cstatus;
	enum status_t pstatus = STATUS_UNKNOWN;
	int charge_fd;
	int config_fd;
	int status_fd;
	int level;
	int plevel;
	int zlevel;
	int charge_full = 0;
	int charge_now = 0;
	int timeout = 0;
	int rc;
	struct config_handle_t *config;
	struct config_threshold_t *hit;
	struct config_threshold_t *threshold;

	BATMON_UNUSED(argc);
	BATMON_UNUSED(argv);

	/* configuration file */
	config_fd = open(BATMON_CONFIG, O_RDONLY);
	if (-1 == config_fd) {
		fprintf(stderr, "batmon: Failed to open configuration file (%s).\n",
		    BATMON_CONFIG);
		return EXIT_FAILURE;
	}
	config = config_open(config_fd);

	if (!config) {
		fprintf(stderr, "batmon: Failed to process configuration file.\n",
		    BATMON_CONFIG);
		return EXIT_FAILURE;
	}

	/* get full charge value */
	charge_fd = open("/sys/class/power_supply/" BATMON_BATTERY "/charge_full",
	    O_RDONLY);
	if (-1 == charge_fd) {
		fprintf(stderr, "batmon: Failed to access battery information.\n");
		return EXIT_FAILURE;
	}
	charge_full = _read_charge(charge_fd);
	close(charge_fd);

	if (charge_full <= 0) {
		fprintf(stderr, "batmon: Unable to read full charge value.\n");
		return EXIT_FAILURE;
	}

	status_fd = open("/sys/class/power_supply/" BATMON_BATTERY "/status",
	    O_RDONLY);
	charge_fd = open("/sys/class/power_supply/" BATMON_BATTERY "/charge_now",
	    O_RDONLY);

	_setup_sigaction();

	do {
		sleep(timeout);

		cstatus = _check_status(status_fd);
		if (pstatus != cstatus) {
			switch (cstatus) {
			case STATUS_AC:
				fprintf(stderr, "batmon: AC mode\n");
				timeout = 30;
				break;
			case STATUS_BATTERY:
				fprintf(stderr, "batmon: Battery mode\n");
				level = 0;
				break;
			case STATUS_UNKNOWN:
				fprintf(stderr, "batmon: Unknown state!\n");
				timeout = 5;
				break;
			}
			pstatus = cstatus;
		}

		switch (cstatus) {
		case STATUS_BATTERY:
			plevel = level;
			charge_now = _read_charge(charge_fd);
			level = (charge_now * 100) / charge_full;
			hit = NULL;
			zlevel = 100;

			fprintf(stderr, "batmon: Level %d (%d)\n", level, plevel);

			/*
			 * Search for closest threshold
			 */
			while ((threshold = config_next(config))) {
				if (zlevel > abs(threshold->level - level)) {
					zlevel = abs(threshold->level - level);
					hit = threshold;
				}
			}

			/*
			 * Execute closest threshold hit if just transitioned
			 * past it.
			 */
			if (hit &&
			    plevel > hit->level && level <= hit->level) {
				fprintf(stderr, "batmon: Threshold hit (%d) executing command: %s\n",
				    hit->level, hit->cmd);
				if (0 == (rc = fork()))
					execl("/bin/sh", "sh", "-c", hit->cmd, (char *)NULL);
				else if (-1 == rc)
					fprintf(stderr, "batmon: Failed to execute command!\n");
			}

			timeout = 5 + level;
			break;

		case STATUS_AC:
		case STATUS_UNKNOWN:
			break;
		}

	} while(s_working);

	close(charge_fd);
	close(status_fd);

	config_close(config);
	close(config_fd);

	return EXIT_SUCCESS;
}

/*****************************************************************************/

int
_check_status(int fd)
{
	char buf[6];

	if (-1 == lseek(fd, 0, SEEK_SET) ||
	     0 >= read(fd, buf, sizeof(buf)))
		return STATUS_UNKNOWN;

	if (strncmp(buf, "Char", 4) == 0 ||
	    strncmp(buf, "Full", 4) == 0)
		return STATUS_AC;

	return STATUS_BATTERY;
}

int
_read_charge(int fd)
{
	char buf[12];

	if (-1 == lseek(fd, 0, SEEK_SET) ||
	     0 >= read(fd, buf, sizeof(buf)))
		return STATUS_UNKNOWN;

	return atoi(buf);
}

void
_signal_handler(int signum)
{
	switch (signum) {
	case SIGINT:
	case SIGQUIT:
	case SIGTERM:
		s_working = 0;
		break;

	case SIGHUP:
		/* ignored - only used to wake up from sleep */
		break;
	}
}

void
_setup_sigaction(void)
{
	struct sigaction batmon_action;

	batmon_action.sa_handler = _signal_handler;
	sigemptyset(&batmon_action.sa_mask);
	batmon_action.sa_flags = 0;

	sigaction(SIGHUP, &batmon_action, NULL);
	sigaction(SIGINT, &batmon_action, NULL);
	sigaction(SIGQUIT, &batmon_action, NULL);
	sigaction(SIGTERM, &batmon_action, NULL);
	signal(SIGCHLD, SIG_IGN);
}

