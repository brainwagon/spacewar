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
    if (SW != 06000) {
        fprintf(stderr, "Munching Squares default Test Word is %06o, "
            "expected 006000\n", SW);
        return 1;
    }

    SW = 0;
    ResetEmulator();
    InitDotColor(PHOS_R_HALFLIFE, PHOS_G_HALFLIFE,
        PHOS_B_HALFLIFE, PHOS_GAMMA);

    for (step=0; step<9; step++)
        SingleStep();
    if (Mem[7] != 02345 || PC != 0111) {
        fprintf(stderr, "Test Word 000000: first increment is %06o, "
            "expected 002345\n", Mem[7]);
        return 1;
    }

    SW = 06000;
    ResetEmulator();
    for (step=0; step<500000; step++)
        SingleStep();
    AgeDots(EmuSeconds());

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

    printf("Munching Squares: Test Word=%06o steps=%d live-dots=%d "
        "bounds=(%d..%d,%d..%d)\n",
        SW,
        step, count, minx, maxx, miny, maxy);
    if (count < 30000 || Mem[7] == 0 || minx == maxx || miny == maxy ||
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
