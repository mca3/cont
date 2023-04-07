#define _GNU_SOURCE

#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/stat.h>

#define STACK_SIZE 8192

#define DIE(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

struct child_args {
	char **argv;

	char *hostname;
	char *root;

	int pipe[2];
	int flags;
};

static char child_stack[STACK_SIZE];

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
mkdirne(char *path, int flags)
{
	if (mkdir(path, flags) && errno != EEXIST)
		return -1;
	return 0;
}

int
pivot_root(char *new, char *old)
{
	// Mount new root as private.
	if (mount(new, new, "none", MS_BIND | MS_REC, NULL) == -1)
		DIE("mount new root");

	// Make space for old root
	if (mkdirne(old, 0700))
		DIE("mkdir old root");

	// Pivot!
	return syscall(SYS_pivot_root, new, old);
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

	if (args->flags & CLONE_NEWNS && args->root) {
		char buf[PATH_MAX];

		snprintf(buf, sizeof(buf), "%s/.pivot", args->root);

		if (pivot_root(args->root, buf) == -1)
			DIE("pivot_root");
		chdir("/");

		// Mount new /proc
		if (mkdirne("/proc", 0700) == -1)
			DIE("mkdir /proc");
		if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL) == -1)
			DIE("mount /proc");

		// Mount new /dev
		if (mkdirne("/dev", 0700) == -1)
			DIE("mkdir /dev");
		if (mount("/.pivot/dev", "/dev", NULL, MS_BIND | MS_REC, NULL) == -1)
			DIE("mount /dev");

		// Ditch /.pivot
		if (umount2("/.pivot", MNT_DETACH) == -1)
			DIE("umount /.pivot");
		if (rmdir("/.pivot") == -1)
			DIE("rmdir /.pivot");
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

void
deny_setgroups(pid_t p)
{
	char path[512];
	int fd;

	snprintf(path, sizeof(path), "/proc/%d/setgroups", p);

	if ((fd = open(path, O_WRONLY)) == -1)
		DIE("open");

	if (write(fd, "deny", 4) != 4)
		DIE("write");

	close(fd);
}

/* uid_map and gid_map share the same format.
 * setmap deals with the format and writes it; it isn't (yet) efficient since
 * it may be called several times and as such open and close file as many times
 * it is called. TODO.
 *
 * from and to specify the start of a range of UIDs of size range.
 * If range is 1, then the UIDs will be mapped 1:1.
 *
 * from maps from inside the namespace, to maps to outside the namespace.
 */
void
setmap(char *file, int from, int to, int range)
{
	char buf[32] = {0};
	int fd;
	size_t sz;

	// TODO: Could overflow here, very unlikely.
	// sz should be clamped to the size of buf.
	sz = snprintf(buf, sizeof(buf), "%d %d %d", from, to, range);

	if ((fd = open(file, O_WRONLY)) == -1)
		DIE("open");

	if (write(fd, buf, sz) != sz)
		DIE("write");

	close(fd);
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
