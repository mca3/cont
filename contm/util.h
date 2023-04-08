#include <stdlib.h>
#include <stdio.h>

#define DIE(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

int mkdirne(char *path, int flags);
int pivot_root(char *new, char *old);
