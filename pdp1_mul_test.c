#include <stdint.h>

#define PDP1_DEMO_SNOWFLAKE
#define PDP1_MUL_DIV
#define main pdp1_gui_main
#include "pdp1.c"
#undef main

static int
CheckMultiply(PDP1Word lhs, PDP1Word rhs)
{
    int negative = SIGN(lhs) ^ SIGN(rhs) ;
    uint64_t left = SIGN(lhs) ? lhs ^ 0777777 : lhs ;
    uint64_t right = SIGN(rhs) ? rhs ^ 0777777 : rhs ;
    uint64_t product = left * right ;
    PDP1Word expected_ac = (PDP1Word)(product >> 17) & 0777777 ;
    PDP1Word expected_io = (PDP1Word)((product & 0377777) << 1) ;

    if (negative && (expected_ac | expected_io)) {
        expected_ac ^= 0777777 ;
        expected_io ^= 0777777 ;
    }

    AC = lhs ;
    IO = 0 ;
    Mem[0100] = rhs ;
    PerformInstruction(0540100) ;

    if (AC != expected_ac || IO != expected_io) {
        fprintf(stderr,
            "MUL %06o * %06o: got %06o:%06o, expected %06o:%06o\n",
            lhs, rhs, AC, IO, expected_ac, expected_io) ;
        return 1 ;
    }
    return 0 ;
}

int
main(void)
{
    int failed = 0 ;

    failed |= CheckMultiply(0520063, 0342643) ;
    failed |= CheckMultiply(0350735, 0342643) ;
    failed |= CheckMultiply(0520063, 0435134) ;
    failed |= CheckMultiply(0123456, 0765432) ;

    return failed ? 1 : 0 ;
}
