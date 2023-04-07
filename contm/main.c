#define _GNU_SOURCE

#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define STACK_SIZE 4096

#define DIE(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

struct child_args {
	char **argv;

	char *hostname;

	int pipe[2];
	int flags;
};

static char child_stack[STACK_SIZE];

void
usage(char *argv0)
{
	fprintf(stderr, "usage: %s [-imnpU] [-u hostname] command [args...]\n"
			"-i		New IPC namespace\n"
			"-m		New mount namespace\n"
			"-n		New network namespace\n"
			"-p		New PID namespace\n"
			"-u <hostname>	New UTS namespace\n"
			"-U		New user namespace (needed if not UID 0)\n",
			argv0);
	exit(EXIT_FAILURE);
}

int
container_main(void *arg)
{
	struct child_args *args = arg;
	char trash;

	// Wait for the parent to signal that we're ready
	close(args->pipe[1]);

	if (read(args->pipe[0], &trash, 1) != 0) {
		fprintf(stderr, "Read from signal pipe failed\n");
		perror("read");
		exit(EXIT_FAILURE);
	}

	close(args->pipe[0]);

	// Do our setup now.

	// Set a hostname if it was requested.
	if (args->flags & CLONE_NEWUTS && args->hostname) {
		if (sethostname(args->hostname, strlen(args->hostname)) == -1)
			DIE("sethostname");
	}
	
	// Setup finished. Execute the program.
	if (!execvp(args->argv[0], args->argv))
		DIE("execvp");
}

int
new_container(struct child_args *args)
{
	// Don't even try if non-root without CLONE_NEWUSER
	if (getuid() != 0 && !(args->flags & CLONE_NEWUSER)) {
		fprintf(stderr, "cannot make new namespace without root unless -U is specified\n");
		exit(EXIT_FAILURE);
	}

	if (pipe(args->pipe) == -1)
		DIE("pipe");

	pid_t child = clone(container_main, child_stack + STACK_SIZE, args->flags | SIGCHLD, args);
	if (child == -1)
		DIE("clone");

	return child;
}

int
main(int argc, char *argv[])
{
	int flags = 0, opt;
	char *hostname = NULL;
	pid_t child;
	struct child_args args = {0};

	while ((opt = getopt(argc, argv, "imnpu:U")) != -1) {
		switch (opt) {
		case 'i': flags |= CLONE_NEWIPC; break;
		case 'm': flags |= CLONE_NEWNS; break;
		case 'n': flags |= CLONE_NEWNET; break;
		case 'p': flags |= CLONE_NEWPID; break;
		case 'u': flags |= CLONE_NEWUTS; args.hostname = optarg; break;
		case 'U': flags |= CLONE_NEWUSER; break;
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

	/* TODO: UID/GID mappings, network setup */

	fprintf(stderr, "Child PID is %d\n", child);

	// Signal the child and say we are ready.
	close(args.pipe[1]);

	if (waitpid(child, NULL, 0) == -1)
		DIE("waitpid");
}
