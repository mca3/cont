contm(1)

# NAME

contm - runs containerized services in Linux _namespaces_(7)

# SYNOPSIS

*contm* [-n] [-u _hostname_] [-R _root_] _command..._

# DESCRIPTION

*contm* is a utility that sets up and starts a container using Linux
_namespaces_(7).

All containers started come stock with a new IPC, mount, PID, and user
namespace, with the current user ID being mapped to 0 (root).

# OPTIONS

*-n*
	Create a new network namespace for the container.

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

# SEE ALSO

_namespaces_(7), _user\_namespaces_(7), _capabilities_(7), _contc_(1)
