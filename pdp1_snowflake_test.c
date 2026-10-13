#include <limits.h>

#define PDP1_DEMO_SNOWFLAKE
#define PDP1_MUL_DIV
#define main pdp1_gui_main
#include "pdp1.c"
#undef main

int
main(void)
{
    int step, count = 0, near = 0 ;
    int minx = INT_MAX, maxx = INT_MIN ;
    int miny = INT_MAX, maxy = INT_MIN ;
    int bucket ;

    srand48(1) ;
    InitDotHashFunc() ;
    ResetEmulator() ;
    for (step=0; step<200000; step++)
        SingleStep() ;

    for (bucket=0; bucket<HASH_SIZE; bucket++) {
        Dot *dot ;
        for (dot=DotHashTable[bucket]; dot; dot=dot->next) {
            count++ ;
            if (dot->x < minx) minx = dot->x ;
            if (dot->x > maxx) maxx = dot->x ;
            if (dot->y < miny) miny = dot->y ;
            if (dot->y > maxy) maxy = dot->y ;
            if (abs(dot->x) <= 16 && abs(dot->y) <= 16)
                near++ ;
        }
    }

    printf("Snowflake: steps=%d dots=%d bounds=(%d..%d,%d..%d) "
        "center16=%d\n", step, count, minx, maxx, miny, maxy, near) ;
    return near == 0 ;
}
