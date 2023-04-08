contc(1)

# NAME

contc - manage Linux _namespaces_(7) (containers) from the outside.

# SYNOPSIS

*contc* [pid] sh [args...]
	Enter a shell via _/bin/sh_.

*contc* [pid] exec [command...]
	Execute an arbitrary file.	

# DESCRIPTION

*contc* is a utility that allows control of a container by allowing users to
enter namespaces and execute as if they were already in it.

*contc* assumes _/bin/sh_ exists when using the *sh* subcommand.

# SEE ALSO

_contm_(1)
