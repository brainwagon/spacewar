#define PDP1_DEMO_MUNCHING
#define main pdp1_gui_main
#include "pdp1.c"
#undef main

int
main(void)
{
    int step, count = 0;
    int minx = 0, maxx = 0, miny = 0, maxy = 0;
    int bucket;

    srand48(1);
    InitDotHashFunc();
    ResetEmulator();
    for (step=0; step<200000; step++)
        SingleStep();

    for (bucket=0; bucket<HASH_SIZE; bucket++) {
        Dot *dot;
        for (dot=DotHashTable[bucket]; dot; dot=dot->next) {
            if (count == 0) {
                minx = maxx = dot->x;
                miny = maxy = dot->y;
            }
            if (dot->x < minx) minx = dot->x;
            if (dot->x > maxx) maxx = dot->x;
            if (dot->y < miny) miny = dot->y;
            if (dot->y > maxy) maxy = dot->y;
            count++;
        }
    }

    printf("Munching Squares: steps=%d dots=%d "
        "bounds=(%d..%d,%d..%d) increment=%06o\n",
        step, count, minx, maxx, miny, maxy, Mem[7]);
    if (count < 1000 || Mem[7] == 0 || minx == maxx || miny == maxy ||
        maxx > 512 || minx < -512 || maxy > 512 || miny < -512)
        return 1;

    SW = 1;
    ResetEmulator();
    for (step=0; step<1000; step++)
        SingleStep();
    if (Mem[7] != 111) {
        fprintf(stderr, "Test Word 000001: increment is %06o, expected 000157\n",
            Mem[7]);
        return 1;
    }
    return 0;
}
