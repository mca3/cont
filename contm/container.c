#define _GNU_SOURCE

#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include "container.h"
#include "util.h"

#define STACK_SIZE 8192

static char child_stack[STACK_SIZE];

static int
init_dev(char *path)
{
	char tmp[PATH_MAX];

	/* Ideally we could use just plain mknod here but for whatever reason
	 * plain old mknod(..., makedev(...)) doesn't work.
	 *
	 * So, instead we make a file and bind mount from old /dev to new /dev,
	 * and also abuse macros because I don't want to write the same five
	 * lines five times.
	 * Not an ideal solution, no, but it works, even on unpriviledged
	 * namespaces. */

#define MKNOD(name, maj, min) snprintf(tmp, sizeof(tmp), "%s/"name, path); \
	if (mknod(tmp, S_IFCHR|0666, 0) == -1) \
		DIE("mknod "name); \
	if (mount("/.pivot/dev/"name, tmp, NULL, MS_BIND, NULL) == -1) \
		DIE("mount "name);

	MKNOD("null", 1, 3);
	MKNOD("zero", 1, 5);
	MKNOD("full", 1, 7);
	MKNOD("random", 1, 8);
	MKNOD("urandom", 1, 9);
	MKNOD("tty", 5, 0);

#undef MKNOD

	return 1;
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
		do {
			// This is so buf is only allocated in this block.
			// We do not have much stack space, intentionally.
			char buf[PATH_MAX];

			snprintf(buf, sizeof(buf), "%s/.pivot", args->root);

			if (pivot_root(args->root, buf) == -1)
				DIE("pivot_root");
			chdir("/");
		} while (0);

		// Mount new /proc
		if (mkdirne("/proc", 0700) == -1)
			DIE("mkdir /proc");
		if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL) == -1)
			DIE("mount /proc");

		// Mount new /dev
		if (mkdirne("/dev", 0700) == -1)
			DIE("mkdir /dev");
		/* if (mount("/.pivot/dev", "/dev", NULL, MS_BIND | MS_REC, NULL) == -1)
			DIE("mount /dev"); */
		if (mount("contm", "/dev", "tmpfs", MS_NOSUID | MS_NOEXEC | MS_NOATIME, "size=32k,nr_inodes=8,mode=755") == -1)
			DIE("mount /dev");

		if (!init_dev("/dev"))
			DIE("init /dev");

		// Ditch /.pivot
		if (umount2("/.pivot", MNT_DETACH) == -1)
			DIE("umount /.pivot");
		if (rmdir("/.pivot") == -1) /* Why does this fail with root? */
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
