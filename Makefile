# Portable Makefile for the PDP-1 simulator.
#
#   make           build the simulator (./pdp1)
#   make clean     remove build products
#
# Override defaults as needed, e.g.
#   make CFLAGS='-O0 -g -Wall'
#
# Requires spacewar.mem.h (the PDP-1 Spacewar! memory image) in this
# directory.  It is generated data and is not tracked by git.

UNAME := $(shell uname -s)

CFLAGS   ?= -O2 -Wall
LDLIBS   ?= -lm

ifeq ($(UNAME),Darwin)
# macOS ships GLUT/OpenGL as frameworks.
LDLIBS += -framework GLUT -framework OpenGL
else
# Linux/BSD: freeglut + Mesa.  The X11 libraries are linked transitively.
LDLIBS += -lglut -lGLU -lGL
endif

OBJS = pdp1.o ship.o logo.o

pdp1: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

pdp1.o: pdp1.c ship.h logo.h spacewar.mem.h
ship.o: ship.c ship.h
logo.o: logo.c logo.h

clean:
	$(RM) $(OBJS) pdp1

.PHONY: clean
