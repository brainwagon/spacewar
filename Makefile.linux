#

LDFLAGS=-L/usr/X11R6/lib
CFLAGS=-g -I/usr/X11R6/include
LIBS=-lglut -lGLU -lGL -lXmu -lXi -lX11 -lm 

pdp1:	pdp1.o ship.o logo.o
	$(CC) -o pdp1 pdp1.o ship.o logo.o $(LDFLAGS) $(LIBS)

pdp1.o: logo.h
