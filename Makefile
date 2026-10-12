# Portable Makefile for the PDP-1 simulator (raylib front end).
#
#   make           build the simulator (./pdp1)
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

OBJS = pdp1.o ship.o logo.o

pdp1: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

pdp1.o: pdp1.c ship.h logo.h spacewar.mem.h
ship.o: ship.c ship.h
logo.o: logo.c logo.h

clean:
	$(RM) $(OBJS) pdp1

.PHONY: clean
