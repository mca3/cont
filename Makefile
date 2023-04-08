CC ?= cc
CFLAGS ?= -O2 -std=c99
LDFLAGS ?= -static

all: doc \
	contm/contm

doc: doc/contm.1

%.o: %.c
	$(CC) -c -o $@ $(CFLAGS) $^

doc/%: doc/%.scd
	scdoc < $^ > $@

contm/contm: contm/main.o
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $^

.PHONY: clean

clean:
	rm -f contm/main.o contm/contm
	rm -f doc/contm.1
