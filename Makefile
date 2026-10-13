# Portable Makefile for the PDP-1 simulator (raylib front end).
#
#   make           build all programs (./spacewar, ./minskytron, ./snowflake)
#   make minskytron build the Minskytron demo
#   make snowflake  build the Snowflake demo
#   make check     run PDP-1 arithmetic and Snowflake display checks
#   make clean     remove build products
#
# Override defaults as needed, e.g.
#   make CFLAGS='-O0 -g -Wall'
#
# Requires raylib (https://www.raylib.com) and spacewar.mem.h (the PDP-1
# Spacewar! memory image) in this directory.  The memory image is generated
# data and is not tracked by git.

UNAME := $(shell uname -s)

CFLAGS ?= -O2 -Wall

ifeq ($(UNAME),Darwin)
# macOS: use pkg-config if raylib was installed via Homebrew.
RAYLIB_CFLAGS ?= $(shell pkg-config --cflags raylib 2>/dev/null)
RAYLIB_LIBS   ?= $(shell pkg-config --libs raylib 2>/dev/null)
else
# Linux/BSD: raylib's static library also needs OpenGL and the usual
# X11/pthread/dl/rt bits.
RAYLIB_CFLAGS ?= -I/usr/local/include
RAYLIB_LIBS   ?= -L/usr/local/lib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
endif

CPPFLAGS += $(RAYLIB_CFLAGS)
LDLIBS   += $(RAYLIB_LIBS)

OBJS = spacewar.o ship.o logo.o

all: spacewar minskytron snowflake

spacewar: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

minskytron: pdp1-minskytron.o ship.o logo.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

snowflake: pdp1-snowflake.o ship.o logo.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

pdp1_mul_test: pdp1_mul_test.c pdp1.c ship.c logo.c demo_images.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ pdp1_mul_test.c ship.c logo.c $(LDLIBS)

pdp1_snowflake_test: pdp1_snowflake_test.c pdp1.c ship.c logo.c demo_images.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ pdp1_snowflake_test.c ship.c logo.c $(LDLIBS)

check: pdp1_mul_test pdp1_snowflake_test
	./pdp1_mul_test
	./pdp1_snowflake_test

spacewar.o: pdp1.c ship.h logo.h spacewar.mem.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ pdp1.c
ship.o: ship.c ship.h
logo.o: logo.c logo.h
pdp1-minskytron.o: pdp1.c ship.h logo.h demo_images.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -DPDP1_DEMO_MINSKYTRON -c -o $@ $<
pdp1-snowflake.o: pdp1.c ship.h logo.h demo_images.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -DPDP1_DEMO_SNOWFLAKE -DPDP1_MUL_DIV -c -o $@ $<

clean:
	$(RM) $(OBJS) pdp1.o pdp1-minskytron.o pdp1-snowflake.o \
		pdp1_mul_test pdp1_snowflake_test \
		pdp1 spacewar minskytron snowflake

.PHONY: all check clean
