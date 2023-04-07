CC ?= cc
CFLAGS ?= -O2 -std=c99
LDFLAGS ?= -static

all: contm/contm

%.o: %.c
	$(CC) -c -o $@ $(CFLAGS) $^

contm/contm: contm/main.o
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $^
