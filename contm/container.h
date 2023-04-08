struct child_args {
	char **argv;

	char *hostname;
	char *root;

	int pipe[2];
	int flags;
};

int container_main(void *arg);
pid_t new_container(struct child_args *args);

void deny_setgroups(pid_t p);
void setmap(char *file, int from, int to, int range);
