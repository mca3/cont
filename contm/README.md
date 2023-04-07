# contm

contm (container main) is the binary that does most of the hard work of
containing something.
It is intentionally designed to be replaceable and to be used by other projects
that may not need the full weight of contd.

Using arguments it is passed, contm sets up a namespace, and after a little bit
of housekeeping will start your program.
