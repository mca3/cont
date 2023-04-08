#include <errno.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include "util.h"

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
