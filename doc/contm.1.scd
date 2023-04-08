contm(1)

# NAME

contm - runs containerized services in Linux _namespaces_(7)

# SYNOPSIS

*contm* [-imnprU] [-u _hostname_] [-R _root_] _command..._

# DESCRIPTION

*contm* is a utility that sets up and starts a container using Linux
_namespaces_(7).

# OPTIONS

*-i*
	Create a new IPC namespace for the container.
	This isolates the System V IPC and POSIX message queues.

*-m*
	Create a new mount namespace for the container.
	The container will not be able to see existing mounts, rather only
	mounts that are local to the container.

*-n*
	Create a new network namespace for the container.

*-p*
	Create a new PID namespace for the container.
	This isolates the PID number space, so that the command run after
	intitial setup would have a PID number of 1.

*-r*
	Map the current UID and GID to 0.

*-u* _hostname_
	Create a new UTS namespace for the container.

	Essentially, this allows you control over the hostname and the domain
	name.

*-R* _root_
	Root of container.

	_/proc_, _/dev_, and _/sys_ may exist but *should not* have any existing
	virtual filesystems inside of them.

	The root directory is recommended to be backed by _tmpfs_(5) and/or an
	overlay filesystem, such as _overlayfs_, with holes selectively punched
	in it for permanent data storage.

*-U*
	Create a new user namespace for the container.

	This allows user and group IDs to be transliterated between the
	container and the outer system.

	This option is necessary for unprivileged namespaces to work.

# SEE ALSO

_namespaces_(7), _user\_namespaces_(7), _capabilities_(7)
