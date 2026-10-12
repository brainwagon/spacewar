
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "ship.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288419716939937510
#endif

/* Portable fallbacks for the POSIX *rand48() family (e.g. Windows). */
#ifdef WIN32
#ifndef srand48
#define srand48(x)	srand((unsigned int)(x))
#endif
#ifndef lrand48
#define lrand48()	((long)rand())
#endif
#ifndef drand48
#define drand48()	((double)rand() / (double)RAND_MAX)
#endif
#endif /* WIN32 */

static int 
sundiving(int x, int y, int dx, int dy)
{
    float t ;
    float cx, cy ;

    if (dx * dx + dy * dy == 0)
	return 0 ;

    t = -(x * dx + y * dy) / (dx * dx + dy * dy) ;

    cx = x + t * dx ;
    cy = y + t * dy ;
    
    if ((t < 0) || (cx*cx + cy*cy > 10000))
	return 0 ;
    else
	return 1 ;
}

static double
bearing(int x, int y, int tx, int ty)
{
    int dx, dy ;
    double th ;

    dx = tx - x ;
    dy = ty - y ;

    th = atan2(dy, dx) - M_PI / 2.0 ;
    while (th < 0)
	th += 2.0 * M_PI ;
    while (th > 2.0 * M_PI)
	th -= 2.0 * M_PI ;
    return th ;
}

void
ShipFace(int me, double theta, ShipStatus *status)
{
    double th = theta - status->th[me] ;

    while (th < 0.0)
	th += 2.0 * M_PI ;
    while (th > 2.0 * M_PI)
	th -= 2.0 * M_PI ;

    if (th < M_PI) {
	ShipSteerRight(me, 0) ;
	ShipSteerLeft(me, 1) ;
    } else {
	ShipSteerRight(me, 1) ;
	ShipSteerLeft(me, 0) ;
    }
}

static int
jitter(int x)
{
    x += (lrand48() & 63) ;
    return x - 32 ;
}


void
ShipStrategy(int me, int him, ShipStatus *status)
{
    double angle ;
    double dx = status->x[him] - status->x[me] ;
    double dy = status->y[him] - status->y[me] ;
    double distance = sqrt(dx*dx+dy*dy) ;


    if (sundiving(status->x[me], status->y[me], 
		  status->dx[me], status->dy[me])) {
	angle = M_PI / 2 + bearing(status->x[me], status->y[me], 
		jitter(0), jitter(0)) ;
	ShipFace(me, angle, status) ;
	ShipThrust(me, 1) ;
	ShipFire(me, 0) ;
    } else {
	angle = bearing(status->x[me], status->y[me],
			jitter(status->x[him]+distance*0.05*
				(status->dx[him]-status->dx[me])), 
			jitter(status->y[him]+distance*0.05*
				(status->dy[him]-status->dy[me]))) ;
	ShipFace(me, angle, status) ;
	ShipThrust(me, 0) ;
	ShipFire(me, 1) ;
	if (fabs(angle-status->th[me]) < 0.03 && distance < 300.0) {
	    ShipFire(me, 1) ;
	} else {
	    ShipFire(me, 0) ;
	}
    }

}
