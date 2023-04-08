CC ?= cc
CFLAGS ?= -O2 -std=c99
LDFLAGS ?= -static
PREFIX ?= /usr/local
DESTDIR ?=

all: doc \
	contm/contm \
	contc/contc

doc: doc/contm.1 doc/contc.1

%.o: %.c
	$(CC) -c -o $@ $(CFLAGS) $^

doc/%: doc/%.scd
	scdoc < $^ > $@

contm/contm: contm/main.o contm/util.o contm/container.o
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $^

contc/contc: contc/main.o
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $^

.PHONY: clean install

install: all
	install -D contm/contm -m 755 $(DESTDIR)$(PREFIX)/bin/contm
	install -D contc/contc -m 755 $(DESTDIR)$(PREFIX)/bin/contc
	install -D doc/contm.1 -m 644 $(DESTDIR)$(PREFIX)/man/man1/contm.1
	install -D doc/contc.1 -m 644 $(DESTDIR)$(PREFIX)/man/man1/contc.1

clean:
	rm -f contm/main.o contm/contm
	rm -f doc/contm.1
