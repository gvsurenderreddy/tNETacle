/*
 * Copyright (c) 2011 Tristan Le Guern <leguern AT medu DOT se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tnetacle.h"
#include "tntexits.h"
#include "log.h"

/* imsg specific includes */
#include <sys/uio.h>
#include <sys/queue.h>
#include <imsg.h>

int debug;
volatile sig_atomic_t sigchld;
volatile sig_atomic_t quit;

static void usage(void);
static int dispatch_imsg(struct imsgbuf *);

void
sighdlr(int sig) {
	switch (sig) {
	case SIGTERM:
	case SIGINT:
		quit = 1;
		break;
	case SIGCHLD:
		sigchld = 1;
		break;
	/* TODO: SIGHUP */
	}
}

/* 
 * Added here for initial convenience, but the main will
 * move under sys/unix later.
 */
int
main(int argc, char *argv[]) {
	int ch;
	pid_t chld_pid;
	struct passwd *pw;
	int imsg_fds[2];
	struct imsgbuf ibuf;
	/* XXX: To remove when Mota will bring his network code */
	fd_set masterfds;
	int fd_max;

	while ((ch = getopt(argc, argv, "dh")) != -1) {
		switch(ch) {
		case 'd':
			debug = 1;
			break;
		case 'h':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	log_init();

	if (geteuid()) {
		(void)fprintf(stderr, "need root privileges\n");
		return 1;
	}

	if ((pw = getpwnam(TNETACLE_USER)) == NULL) {
		(void)fprintf(stderr, "unknown user " TNETACLE_USER "\n");
		return TNT_NOUSER;
	}

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, imsg_fds) == -1) {
		perror("socketpair");
		return 1;
	}

	signal(SIGCHLD, sighdlr);
	chld_pid = tnt_fork(imsg_fds, pw);

	signal(SIGTERM, sighdlr);
	signal(SIGINT, sighdlr);

	if (close(imsg_fds[1]))
		log_notice("[priv] close");

	imsg_init(&ibuf, imsg_fds[0]);

	fd_max = ibuf.fd;
	FD_ZERO(&masterfds);
	FD_SET(ibuf.fd, &masterfds);

	while (quit == 0) {
		fd_set readfds = masterfds;
		if ((select(fd_max + 1, &readfds, NULL, NULL, NULL)) == -1)
			log_err(1, "[priv] select");

		if (FD_ISSET(ibuf.fd, &readfds)) {
			if (dispatch_imsg(&ibuf) == -1)	
				quit = 1;
		}

		if (sigchld == 1) {
			quit = 1;
			chld_pid = 0;
			sigchld = 0;
		}
	}

	signal(SIGCHLD, SIG_DFL);

	if (chld_pid != 0)
		kill(chld_pid, SIGTERM);

	msgbuf_clear(&ibuf.w);
	log_info("Terminating");
	return TNT_OK;
}

static void
usage(void) {
	char *progname = tnt_getprogname();

	(void)fprintf(stderr, "%s: [-dh]\n", progname);
	exit(TNT_USAGE);
}

static int
dispatch_imsg(struct imsgbuf *ibuf) {
	struct imsg imsg;
	int n;

	n = imsg_read(ibuf);
	if (n == -1) {
		log_warnx("[priv] loose some imsgs");
		imsg_clear(ibuf);
		return 0;
	}

	if (n == 0) {
		log_warnx("[priv] pipe closed");
		return -1;
	}

	for (;;) {
		/* Loops through the queue created by imsg_read */
		n = imsg_get(ibuf, &imsg);
		if (n == -1) {
			log_warnx("[priv] imsg_get");
			return -1;
		}

		/* Nothing was ready */
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			break;
		}
		imsg_free(&imsg);
	}
	return 0;
}
