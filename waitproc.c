/*
 * waitproc - wait for an arbitrary process to exit
 * Copyright © 2010 Calvin Walton <calvin.walton@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <errno.h>
#include <popt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define WAITPROC_RET_NOTRUNNING 127

#define WAITPROC_RET_SIGMASK 0x80

#define WAITPROC_RET_TIMEOUT 255
#define WAITPROC_RET_USAGE 254
#define WAITPROC_RET_PERM 253
#define WAITPROC_RET_INTERNAL 252

static int timeout = 0;

static const struct poptOption options[] = {
	{
		"timeout",
		't',
		POPT_ARG_INT,
		&timeout,
		0,
		"The amount of time to wait for the process to exit",
		"SECONDS"
	},
	POPT_AUTOHELP
	POPT_TABLEEND
};

static void alrm_handler(int signal)
{
	return;
};

int main(int argc, const char *argv[])
{
	poptContext opt_ctx;
	long i;
	const char *pid_string;
	char *pid_parse_end;
	pid_t pid;
	int retcode = -1;

	/* Parse command-line options */
	opt_ctx = poptGetContext(NULL, argc, argv, options, 0);
	poptSetOtherOptionHelp(opt_ctx, "PID");

	i = poptGetNextOpt(opt_ctx);
	if (i < -1) {
		fprintf(stderr, "%s: %s\n",
		        poptBadOption(opt_ctx, 0), poptStrerror(i));
		poptPrintUsage(opt_ctx, stderr, 0);
		exit(WAITPROC_RET_USAGE);
	}

	pid_string = poptGetArg(opt_ctx);
	if (pid_string == NULL) {
		fprintf(stderr, "PID not specified\n");
		poptPrintUsage(opt_ctx, stderr, 0);
		exit(WAITPROC_RET_USAGE);
	}

	errno = 0;
	pid = strtoul(pid_string, &pid_parse_end, 0);
	if (errno) {
		perror("PID");
		poptPrintUsage(opt_ctx, stderr, 0);
		exit(WAITPROC_RET_USAGE);
	}
	if (*pid_parse_end != '\0') {
		fprintf(stderr, "Error parsing PID\n");
		poptPrintUsage(opt_ctx, stderr, 0);
		exit(WAITPROC_RET_USAGE);
	}
	if (pid < 1) {
		fprintf(stderr, "PID must be greater than 0\n");
		poptPrintUsage(opt_ctx, stderr, 0);
		exit(WAITPROC_RET_USAGE);
	}

	poptFreeContext(opt_ctx);

	/* We have the pid, now attempt to attach to the process */
	i = ptrace(PTRACE_ATTACH, pid, NULL, NULL);
	if (i == -1) {
		if (errno == ESRCH) {
			exit(WAITPROC_RET_NOTRUNNING);
		} else {
			fprintf(stderr, "Attach to pid %d: %m\n", pid);
			exit(WAITPROC_RET_PERM);
		}
	}

	/* Set up a timer, if a timeout is desired */
	if (timeout) {
		struct sigaction alrm_action = {
			.sa_handler = alrm_handler
		};
		sigaction(SIGALRM, &alrm_action, NULL);
		alarm(timeout);
	}

	while (retcode == -1) {
		int status;
		
		i = waitpid(pid, &status, 0);
		if (i == -1) {
			if (errno == ECHILD) {
				exit(WAITPROC_RET_NOTRUNNING);
			} else if (errno == EINTR) {
				/*
				 * Either the timeout expired, or we've been
				 * interrupted (Ctrl-C), etc. Exit cleanly.
				 */
				retcode = WAITPROC_RET_TIMEOUT;
				break;
			} else {
				/* An unexpected error */
				perror("Failed to wait for process");
				retcode = WAITPROC_RET_INTERNAL;
				break;
			}
		}

		if (WIFEXITED(status)) {
			retcode = WEXITSTATUS(status) & 0x7f;
		} else if (WIFSIGNALED(status)) {
			retcode = WTERMSIG(status) | 0x80;
		} else if (WIFSTOPPED(status)) {
			i = ptrace(PTRACE_CONT, pid, NULL,
			           (void *) (long) WSTOPSIG(status));
			if (i == -1) {
				perror("Failed to resume after signal");
				retcode = WAITPROC_RET_INTERNAL;
				break;
			}
		}
	}

	/* Attempt to detach from the still-running process */
	i = ptrace(PTRACE_DETACH, pid, NULL, NULL);
	if (i == -1) {
		if (errno == ECHILD || errno == ESRCH) {
			/* Whoops, it's already gone. Normal case */
		} else {
			/* An unexpected error */
			perror("Failed to detach process");
		}
	}

	return retcode;
}
