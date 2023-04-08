#define _GNU_SOURCE

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "container.h"
#include "util.h"

void
usage(char *argv0)
{
	fprintf(stderr, "usage: %s [-imnprU] [-u hostname] [-R root] command [args...]\n"
			"-i		New IPC namespace\n"
			"-m		New mount namespace\n"
			"-n		New network namespace\n"
			"-p		New PID namespace\n"
			"-r		Map the current UID and GID to 0.\n"
			"-u <hostname>	New UTS namespace\n"
			"-R <dir>	Root of container\n"
			"-U		New user namespace (needed if not UID 0)\n"
			"		Implies -r.\n",
			argv0);
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	int flags = 0, map_root = 0, opt;
	char *hostname = NULL;
	pid_t child;
	struct child_args args = {0};
	char buf[512];
	char realroot[PATH_MAX];

	while ((opt = getopt(argc, argv, "imnpru:R:U")) != -1) {
		switch (opt) {
		case 'i': flags |= CLONE_NEWIPC; break;
		case 'm': flags |= CLONE_NEWNS; break;
		case 'n': flags |= CLONE_NEWNET; break;
		case 'p': flags |= CLONE_NEWPID; break;
		case 'r': map_root = 1; break;
		case 'u': flags |= CLONE_NEWUTS; args.hostname = optarg; break;
		case 'R': args.root = realpath(optarg, realroot); break;
		case 'U': flags |= CLONE_NEWUSER; map_root = 1; break;
		default: usage(argv[0]);
		}

		/* TODO: UID/GID mapping */
	}

	/* TODO:
	 * - Determine if user can even make namespaces
	 * - ...
	 */

	if (optind >= argc) {
		fprintf(stderr, "expected command line after options\n");
		exit(EXIT_FAILURE);
	}

	args.argv = &argv[optind];
	args.flags = flags;

	if (!(child = new_container(&args))) {
		perror("failed to create container");
		exit(EXIT_FAILURE);
	}

	if (map_root) {
		deny_setgroups(child);

		snprintf(buf, sizeof(buf), "/proc/%d/uid_map", child);
		setmap(buf, 0, getuid(), 1);
		snprintf(buf, sizeof(buf), "/proc/%d/gid_map", child);
		setmap(buf, 0, getgid(), 1);
	}

	/* TODO: UID/GID mappings, network setup */

	fprintf(stderr, "Child PID is %d\n", child);

	// Signal the child and say we are ready.
	close(args.pipe[1]);

	if (waitpid(child, NULL, 0) == -1)
		DIE("waitpid");
}
