#define _GNU_SOURCE

#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>

#define DIE(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

void
usage(char *argv0)
{
	fprintf(stderr, "usage:	%s [PID] sh\n"
			"	%s [PID] exec command...\n",
			argv0, argv0);
	exit(EXIT_FAILURE);
}

void
set_all_ns(pid_t pid)
{
	char buf[PATH_MAX];
	int fd;
	int uid;

	uid = getuid();

#define SETNS(name, cap) do { \
	snprintf(buf, sizeof(buf), "/proc/%d/ns/"name, pid); \
	if ((fd = open(buf, O_RDONLY)) == -1) \
		DIE("open ns/"name); \
	if (setns(fd, cap) == -1) \
		DIE("setns "#cap); \
	close(fd); \
} while (0)

	// The following namespaces are set by contm by default:
	// - user
	// - ipc
	// - mnt (must be done last)
	// - pid
	// - uts (needs root for some reason)

	SETNS("user", CLONE_NEWUSER);
	SETNS("ipc", CLONE_NEWIPC);

	// TODO: We can only set the UTS namespace if root???
	if (uid == 0)
		SETNS("uts", CLONE_NEWUTS);

	SETNS("pid", CLONE_NEWPID);
	SETNS("mnt", CLONE_NEWNS);

#undef SETNS
}

int
main(int argc, char *argv[])
{
	pid_t pid;

	if (argc <= 2)
		usage(argv[0]);

	pid = atoi(argv[1]);

	set_all_ns(pid);

	chdir("/");

	setuid(0);
	setgid(0);

	if (strcmp(argv[2], "sh") == 0) {
		argv[2] = "/bin/sh";
		execvp(argv[2], &argv[2]);
		DIE("execvp");
	} else if (strcmp(argv[2], "exec") == 0 && argc >= 3) {
		execvp(argv[3], &argv[3]);
		DIE("execvp");
	} else
		usage(argv[0]);
}
