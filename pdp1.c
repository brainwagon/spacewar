#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#ifndef WIN32
#include <unistd.h>	/* getpid() */
#endif

#include <raylib.h>

#include "ship.h"
#include "logo.h"

/* The simulator uses the POSIX *rand48() family.  Provide portable
 * fallbacks where they are unavailable (e.g. Windows). */
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

/* 
 * +-+-+-+-+ +-+-+-+-+-+-+-+-+-+
 * |p|d|p|1| |s|i|m|u|l|a|t|o|r|
 * +-+-+-+-+ +-+-+-+-+-+-+-+-+-+
 *
 * Tom Duff decided to write a PDP-1 simulator so that he could play
 * the original spacewar.  I thought it would be an interesting 
 * challenge as well, so I decided to do the same.  I won't pretend
 * that mine will be better, but it is a fun project, as this program
 * was actually written before I was born (albeit, not by much).
 */

typedef int PDP1Word ;

/* Each instruction normally consists of five opcode bits, a single
 * indirection bit, and a 12 bit address.  
 */

#define 	OPERATOR(x)		(((x)&(0x3E000))>>12)
#define		INDIRECT(x)		((x)&(0x1000))
#define		ADDRESS(x)		((x)&0xFFF)

#define INSTRUCTIONPART(x)	((x)&(0770000))
#define ADDRESSPART(x)		((x)&(07777))

#define SIGNBIT			(1<<17) 
#define SIGN(x)			(((x)>>17)&1)
#define NEGATIVE(x)		((x)&(1<<17))

/* Now, the opcodes themselves...
 */

#define	ADD	(040)
#define AND	(002)
#define CAL	(016)
#define DAC	(024)
#define	DAP	(026)
#define DIO	(032)
#define DIP	(030)
#define DIV	(056)	/* DIV or... */
#define DIS	(056)	/* DIS divide step */
#define DZM	(034)
#define IDX	(044)
#define IOR	(004)
#define IOT	(072)
#define ISP	(046)
#define JDA	(017)
#define JMP	(060)
#define JSP	(062)
#define LAC	(020)
#define LAW	(070)
#define LIO	(022)
#define MUL	(054)	/* MUL or... */
#define MUS	(054)	/* MUS multiply step */
#define OPR	(076)	/* operation, complex */
#define SAD	(050)
#define SAS	(052)
#define SFT	(066)
#define SKP	(064)
#define SUB	(042)
#define XCT	(010)	/* xct instruction at the address, cool! */
#define XOR	(006)

/* The PDP1 has 4096 words of 18 bit memory. */

#define MEMORY_SIZE	(4096)
PDP1Word Mem[MEMORY_SIZE] = {
#include "spacewar.mem.h"
//#include "snowflake2.h"
} ;

#define DEBUG(x)

/* PDP-1 timing: one memory cycle is 5 microseconds (200,000 cycles/s). */
#define PDP1_HZ			200000.0
#define MAXSTEPS_PER_TICK	200000	/* bound catch-up work per timer tick */

/* Time constant for phosphor hit accumulation (emulated seconds). */
#define PHOS_CHARGE_HALFLIFE	0.10

/* Type 30 display intensity, from the 3-bit field in the dpy instruction
 * (bits 6-8).  It is in 1's complement: 3 is brightest, 0 the default and
 * 7 (-0) barely visible.  These are brightness multipliers. */
static const float DpyBrightness[8] = {
    0.70, 0.85, 0.95, 1.00,	/* 0 (default) .. 3 (brightest) */
    0.55, 0.35, 0.18, 0.06	/* 4 .. 7 (-0, barely visible) */
} ;

int executed ; 	/* number of instructions executed */
double emu_cycles = 0.0 ;	/* PDP-1 cycles executed (drives phosphor timing) */

int InstrCycles(int instruction) ;	/* defined with the timing code below */
double EmuSeconds(void) ;		/* emulated seconds elapsed */
int PC=4 ;	/* program counter */
int AC=0;	/* accumulator */
int OV=0;	/* overflow condition */
int IO=0;	/* IO Register */
int SW = 0x89 ;	/* switches */

int Sense[19] = {0, 0, 0, 0, 0, 0, 0} ;
int Flag[7] = {0, 0, 0, 0, 0, 0, 0} ;

int HeavyDots = 1 ;
int HUD = 0 ;
int autoFlag = 1 ;
int Spin = 0 ;
double FPS ;
int FPSvalid = 0 ;
int FPSflag = 0 ;

Texture2D logoTexture ;

/* View transform: emulator coordinates are [-512,512] with y up and the
 * origin at the centre of the scope; raylib's origin is the top-left with
 * y down.  viewScale maps logical units to pixels. */
float viewScale = 1.0f ;
float viewCX = 512.0f, viewCY = 512.0f ;
float sceneTheta = 0.0f ;	/* radians, for Spin mode */

Vector2
ToScreen(float x, float y)
{
    float rx = x, ry = y ;

    if (sceneTheta != 0.0f) {
	float c = cosf(sceneTheta), s = sinf(sceneTheta) ;
	rx = x * c - y * s ;
	ry = x * s + y * c ;
    }
    return (Vector2){ viewCX + rx * viewScale, viewCY - ry * viewScale } ;
}

int
incpc()
{
    PC ++ ;  PC &= 07777 ;
    return PC ;
}

int 
SenseesZero()
{
    int i ;
    for (i=1;i<=6; i++) if (Sense[i] == 1) return 0 ;
    return 1 ;
}

int 
FlagsZero()
{
    int i ;
    for (i=1;i<=6; i++) if (Flag[i] == 1) return 0 ;
    return 1 ;
}


int
Y(int inst)
{
    int cnt=0 ;
    while (INDIRECT(inst)&&cnt<12) {
	inst = Mem[ADDRESS(inst)] ;
	cnt++ ;
    }
    if (cnt > 1) {
	fprintf(stderr, "suspicious multiple indirection, bailing\n") ;
	abort() ;
    }
    return ADDRESS(inst) ;
}

#define NEGATIVE_ZERO	((1<<18)-1)

typedef struct t_phosphordot {
    struct t_phosphordot *next ;
    int x, y, age ;
    double t ;		/* emulated time (seconds) when last painted */
    float level ;	/* brightness of the most recent hit (0..1) */
    float charge ;	/* accumulated exposure, decays over emulated time */
} Dot ;

Dot *FreeDot = NULL ;

#define HASH_SIZE	(2048)
#define HASH_MASK	(HASH_SIZE-1)

int DotHashFunc[16384] ;

Dot *DotHashTable[HASH_SIZE] ;


void
InitDotHashFunc()
{
    int i ;
    for (i=0; i<1024; i++) DotHashFunc[i] = lrand48() & HASH_MASK ;
    for (i=0; i<HASH_SIZE; i++) DotHashTable[i] = NULL ;
}

int
Hash(int x, int y)
{
    return DotHashFunc[x+512]^DotHashFunc[y+512] ;
}

Dot *
FindDot(int x, int y)
{
    int idx = Hash(x, y) ;
    Dot *dot ;

    dot = DotHashTable[idx] ;

    while (dot && (dot->x != x || dot->y != y))
	dot = dot->next ;

    return dot ;
}

void
NewDot(int x, int y, float level)
{
    Dot *dot ;
    int idx ;
    double now = EmuSeconds() ;

    if ((dot=FindDot(x, y))) {
	/* Refresh: let the previous exposure decay, then add this hit. */
	float decay = exp(-(now - dot->t) / PHOS_CHARGE_HALFLIFE) ;
	dot->age = 0 ;
	dot->charge = dot->charge * decay + 1.0 ;
	dot->level = level ;
	dot->t = now ;
	return ;
    }

    /* didn't find it */

    if (FreeDot == NULL) {
	dot = (Dot *) malloc(sizeof(Dot)) ;
    } else {
	dot = FreeDot ;
	FreeDot = FreeDot->next ;
    }

    dot->x = x ;
    dot->y = y ;
    dot->age = 0 ;
    dot->charge = 1.0 ;
    dot->level = level ;
    dot->t = now ;
    idx = Hash(x, y) ;
    dot->next = DotHashTable[idx] ;
    DotHashTable[idx] = dot ;
}

/* Phosphor decay.
 *
 * The Type 30 uses a P7 phosphor: a fast blue flash followed by a
 * long-lived yellow-green afterglow.  A drawn point is stored once and its
 * brightness decays with *emulated* time (not with render frames), so the
 * image looks the same regardless of the host frame rate or the selected
 * emulation speed.  Repainting a point refreshes it to full brightness.
 *
 * Half-lives are in emulated seconds.  Colours are precomputed into a table
 * of MaxAge steps spanning the time it takes the slowest channel to fade to
 * PHOS_FLOOR.
 */

#define REALMAXAGE	(1024)

int MaxAge ;			/* number of colour-table steps */
float DotColor[REALMAXAGE][3] ;
double PhosDecayStep ;		/* emulated seconds per table step */

#define PHOS_R_HALFLIFE	0.05	/* seconds (fast blue flash) */
#define PHOS_G_HALFLIFE	0.15	/* seconds (yellow-green afterglow) */
#define PHOS_B_HALFLIFE	0.05
#define PHOS_GAMMA	2.0
#define PHOS_FLOOR	0.05

#define MAX(a,b)	((a)>(b)?(a):(b))
#define MAX3(a,b,c)	MAX(MAX(a,b),c)

void
InitDotColor(float rh, float gh, float bh, float dotgamma)
{
    float rk, gk, bk, tfloor ;
    int i ;

    rk = log(2.0) / rh ;	/* decay constants, per second */
    gk = log(2.0) / gh ;
    bk = log(2.0) / bh ;

    /* emulated time for the slowest channel to fade to PHOS_FLOOR */
    tfloor = MAX3(log(1.0/PHOS_FLOOR) / rk,
		  log(1.0/PHOS_FLOOR) / gk,
		  log(1.0/PHOS_FLOOR) / bk) ;

    MaxAge = REALMAXAGE ;
    PhosDecayStep = tfloor / (MaxAge - 1) ;

    for (i=0; i<MaxAge; i++) {
	float t = i * PhosDecayStep ;
	DotColor[i][0] = pow(exp(-rk * t), 1.0/dotgamma) ;
	DotColor[i][1] = pow(exp(-gk * t), 1.0/dotgamma) ;
	DotColor[i][2] = pow(exp(-bk * t), 1.0/dotgamma) ;
    }
}

void
AgeDots(double now)
{
    int i ;
    Dot *dot, *nextdot ;

    for (i=0; i<HASH_SIZE; i++) {
	dot = DotHashTable[i] ;
        DotHashTable[i] = NULL ;

	while (dot) {
	    int a ;

	    nextdot = dot->next ;
	    a = (int)((now - dot->t) / PhosDecayStep) ;
	    if (a < 0)
		a = 0 ;
	    dot->age = a ;
	    if (a < MaxAge) {
		dot->next = DotHashTable[i] ;
		DotHashTable[i] = dot ;
	    } else {
		dot->next = FreeDot ;
		FreeDot = dot ;
	    }
	    dot = nextdot ;
	}
    }
}

void
DrawDots() 
{
    Dot *dot;
    int i ;
    float sz = (HeavyDots ? 2.0f : 1.0f) * viewScale ;

    if (sz < 1.0f)
	sz = 1.0f ;

    /* Phosphor emits light: blend additively so overlapping or tightly
     * spaced hits accumulate and bloom. */
    BeginBlendMode(BLEND_ADDITIVE) ;
    for (i=0; i<HASH_SIZE; i++) {
	dot = DotHashTable[i] ;
	while (dot) {
	    /* overall exposure grows toward 1 with repeated hits */
	    float exposure = 1.0f - expf(-dot->charge) ;
	    float r = DotColor[dot->age][0] * dot->level * exposure ;
	    float g = DotColor[dot->age][1] * dot->level * exposure ;
	    float b = DotColor[dot->age][2] * dot->level * exposure ;
	    Color c = {
		(unsigned char)(255.0f * r),
		(unsigned char)(255.0f * g),
		(unsigned char)(255.0f * b),
		255
	    } ;
	    Vector2 p = ToScreen(dot->x, dot->y) ;

	    DrawRectangle((int)(p.x - sz * 0.5f), (int)(p.y - sz * 0.5f),
		(int)sz, (int)sz, c) ;
	    dot = dot->next ;
	}
    }
    EndBlendMode() ;
}



void
DisplayPoint(int x, int y, float level)
{
    x >>= 8 ;
    y >>= 8 ;
 
    if (x & 01000) { x |= ~01777; x++; } /* convert to two's complement */
    if (y & 01000) { y |= ~01777; y++; }

    NewDot(x, y, level) ;
}

int 
add(int a, int b)
{
    int obits ;

    if (a&SIGNBIT) a |= SIGNBIT << 1 ;
    if (b&SIGNBIT) b |= SIGNBIT << 1 ;
    a += b ;
    a += a >> 19 ;	/* the carry bit */
    obits = (a>>17)&3 ;
    if (obits == 1 || obits == 2) OV = 1 ;
    a &= 0777777 ;
    if (a == NEGATIVE_ZERO) a = 0 ;
    return a ;
}

void
PerformInstruction(int instruction)
{
    int operator, tmpaddr, i, cond, sign ;
#ifdef PDP1_MUL_DIV
    int mb, v ;
#endif

    switch (operator=OPERATOR(instruction)) {
    case ADD:
	DEBUG(ADD) ;
	tmpaddr = Y(instruction) ;
	AC = add(AC, Mem[tmpaddr]) ;
	break ;
    case SUB:
        DEBUG(SUB);
	tmpaddr = Y(instruction) ;
	if (AC == 0 && Mem[tmpaddr] == NEGATIVE_ZERO) 
	    AC = NEGATIVE_ZERO ;
	else
	    AC = add(AC, Mem[tmpaddr]^0777777) ;
	break ;

    /* Logical operators, AND, IOR, and XOR */
    case AND:
	DEBUG(AND);
	tmpaddr = Y(instruction) ;
	AC &= Mem[tmpaddr] ;
	break ;
    case IOR:
 	DEBUG(IOR);
	tmpaddr = Y(instruction) ;
	AC |= Mem[tmpaddr] ;
	break ;
    case XOR:
	DEBUG(IOR);
	tmpaddr = Y(instruction) ;
	AC ^= Mem[tmpaddr] ;
	break ;

    /* General instructions */
    case LAC:	/* Load accumulator */
	DEBUG(LAC);
	tmpaddr = Y(instruction) ;
	AC = Mem[tmpaddr] ;
	break ;
    case DAC:	/* Deposit accumulator */
	DEBUG(DAC);
	tmpaddr = Y(instruction) ;
	Mem[tmpaddr] = AC ;
	break ;
    case DAP:	/* Deposit address part */
	DEBUG(DAP);
	tmpaddr = Y(instruction) ;
	Mem[tmpaddr] ^= ADDRESSPART(Mem[tmpaddr]) ;
	Mem[tmpaddr] |= ADDRESSPART(AC) ;
	break ;
    case DIP:
	DEBUG(DIP);
	tmpaddr = Y(instruction) ;
	Mem[tmpaddr] ^= INSTRUCTIONPART(Mem[tmpaddr]) ;
	Mem[tmpaddr] |= INSTRUCTIONPART(AC) ;
	break ;
    case DZM:	
	DEBUG(DZM);
	tmpaddr = Y(instruction) ;
	Mem[tmpaddr] = 0 ;
	break ;
	
    case IDX:
	DEBUG(IDX);
	tmpaddr = Y(instruction) ;
	AC = Mem[tmpaddr] + 1;
	if (AC == NEGATIVE_ZERO)
	   AC = 0 ;
	Mem[tmpaddr] = AC ;
	break ;

    case ISP:
	DEBUG(ISP);
	tmpaddr = Y(instruction) ;
	AC = Mem[tmpaddr] + 1;
	if (AC == NEGATIVE_ZERO)
	   AC = 0 ;
	Mem[tmpaddr] = AC ;
	if (!NEGATIVE(AC))
	    incpc() ;
	break ;

    case LAW:
	DEBUG(LAW);
	if (INDIRECT(instruction)) 
	    AC = ADDRESS(instruction) ^ 0777777 ;
	else
	    AC = ADDRESS(instruction) ;
	break ;

    case XCT:
	DEBUG(XCT);
	tmpaddr = Y(instruction) ;
	PerformInstruction(Mem[tmpaddr]) ;
	break ;

    case JMP:
	DEBUG(JMP);
	PC = ADDRESSPART(Y(instruction)) ;
	break ;

    case SKP:
	DEBUG(SKP);
	tmpaddr = ADDRESSPART(instruction) ;
	cond = 
	   (((tmpaddr & 00100) && (AC == 0)) ||
	    ((tmpaddr & 00200) && (SIGN(AC)==0)) ||
	    ((tmpaddr & 00400) && (SIGN(AC))) ||
	    ((tmpaddr & 01000) && (OV == 0)) ||
	    ((tmpaddr & 02000) && (SIGN(IO) == 0)) ||
	    (((tmpaddr&00070)==00010) && (Sense[1] == 0)) ||
	    (((tmpaddr&00070)==00020) && (Sense[2] == 0)) ||
	    (((tmpaddr&00070)==00030) && (Sense[3] == 0)) ||
	    (((tmpaddr&00070)==00040) && (Sense[4] == 0)) ||
	    (((tmpaddr&00070)==00050) && (Sense[5] == 0)) ||
	    (((tmpaddr&00070)==00060) && (Sense[6] == 0)) ||
	    (((tmpaddr&00070)==00070) && (SenseesZero())) ||
	    (((tmpaddr&00007)==00001) && (Flag[1] == 0)) ||
	    (((tmpaddr&00007)==00002) && (Flag[2] == 0)) ||
	    (((tmpaddr&00007)==00003) && (Flag[3] == 0)) ||
	    (((tmpaddr&00007)==00004) && (Flag[4] == 0)) ||
	    (((tmpaddr&00007)==00005) && (Flag[5] == 0)) ||
	    (((tmpaddr&00007)==00006) && (Flag[6] == 0)) ||
	    (((tmpaddr&00007)==00007) && (FlagsZero()))) ;

	if (INDIRECT(instruction))
	    cond = !cond ;

	if (cond)
	    incpc() ;
	break ;

    case SFT:
	switch (instruction&0777000) {
	case 0671000:	/* RAR - Rotate Accumulator Right */
	    DEBUG(RAR);
	    for (i=0; i<9;i++) {
		if (instruction & (1<<i)) {
		    AC = ((AC&1)<<17)|(AC>>1) ;
		}
	    }
	    break ;
	case 0673000: /* RCR - Rotate Combined Right */
	    DEBUG(RCR);
	    for (i=0; i<9;i++) {
		if (instruction & (1<<i)) {
		    if (AC & 1)
			IO |= (1<<18) ;
		    if (IO & 1)
			AC |= (1<<18) ;
		    AC >>= 1 ;
		    IO >>= 1 ;
		    AC &= 0777777 ;
		    IO &= 0777777 ;
		}
	    }
	    break ;
	case 0663000: /* RCL - Rotate Combined Left */
	    DEBUG(RCL);
	    for (i=0; i<9;i++) {
		if (instruction & (1<<i)) {
		    AC <<= 1 ;
		    IO <<= 1 ;
		    if (AC & (1 << 18))
			IO |= 1 ;
		    if (IO & (1 << 18))
			AC |= 1 ;
		    AC &= 0777777 ;
		    IO &= 0777777 ;
		}
	    }
	    break ;
	case 0662000: /* RIL - Rotate IO Register Left */
	    DEBUG(RIL);
	    for (i=0; i<9;i++) {
		if (instruction & (1<<i))
		    IO = ((IO&SIGNBIT)>>17) | (IO<<1) ;
	    }
	    break ;
	case 0661000: /* RAL - Rotate Accumulator Left */
	    DEBUG(RAL);
	    for (i=0; i<9;i++) {
		if (instruction & (1<<i)) {
		    AC <<= 1 ;
		    if (AC & 01000000) AC ^= 01000001 ;
		}
	    }
	    break ;
	case 0667000: /* SCL - Shift Combined Left */ 
	    DEBUG(SCL) ;
	    for (i=0; i<9;i++) {
		if (instruction & (1<<i)) {
		    IO <<= 1 ;
		    AC <<= 1 ;
		    if (AC & 01000000) {
			IO |= 1 ;
			AC = (AC & 0777777) | SIGNBIT ;
		    } else {
			AC &= 0377777 ;
		    }
		    if (IO & 01000000) {
			IO &= 0777777 ;
			AC |= 1 ;
		    }
		}
	    }
	    break ;
	case 0677000: /* SCR - Shift Combined Right */ /* XXX */
	    DEBUG(SCR) ;
	    for (i=0; i<9;i++) {
		if (instruction & (1<<i)) {
		    IO >>= 1 ;
		    if (AC & 1) IO |= (1 << 17) ;
		    AC >>= 1 ;
		    if (AC & (1<<16)) AC |= (1<<17) ;
		}
	    }
	    break ;
	case 0665000: /* SAL - Shift Accumulator Left */
	    DEBUG(SAL) ;
	    for (i=0; i<9;i++) {
		if (instruction & (1<<i)) {
		    if (AC&SIGNBIT)
			AC = ((AC << 1)|0400001) & 0777777 ;
		    else
			AC = (AC << 1) & 0377777 ;
		}
	    } 
	    break ;
	case 0675000: /* SAR - Shift Accumulator Right */ /* XXX */
	    DEBUG(SAR) ;
	    for (i=0; i<9;i++) {
		if (instruction & (1<<i)) {
		    AC >>= 1 ;
	            if (AC & (1<<16)) AC |= (1<<17) ;
		}
	    }
	    break ;
	case 0676000: /* SIR - Shift IO Register Right */
	    DEBUG(SIR) ;
	    for (i=0; i<9;i++) {
		if (instruction & (1<<i)) {
		    IO >>= 1 ;
	            if (IO & (1<<16)) IO |= (1<<17) ;
		}
	    }
	    break ;
	case 0672000: /* RIR - Rotate IO Register Right */
	    DEBUG(RIR) ;
	    for (i=0; i<9;i++) {
		if (instruction & (1<<i)) {
		    IO = ((IO&1)<<17)|(IO>>1) ;
		}
	    } 
	    break ;
	    
	default:
	    fprintf(stderr, "unknown shift code 0%06o\n", instruction) ;
	    abort() ;
	}
	break ;

    case OPR:
	DEBUG(OPR) ;
	{ 
	    int opr = instruction&07777 ;
	    if (opr & 04000) IO = 0 ;
	    if (opr & 02000)
		AC = SW ;		/* lat: load the Test Word switches */
	    else if (opr & 00200)
		AC = 0 ;		/* cla */
	    if (opr & 01000) AC ^= 0777777 ;
	    if (opr & 00100) {
		abort() ;
		AC |= (PC|(OV<<17)) ;
	    }
	    if (opr & 00007) {
		int bit ;
		if (opr & 010)
		    bit = 1 ;
		else 
		    bit = 0 ;
		if ((opr & 00007) == 7) 
		    for (i=1;i<=6;i++) Flag[i] = bit ;
		else {
		    Flag[(opr&00007)] = bit ;
		}
	    }
	}
	break ;
    case SAD:
	DEBUG(SAD) ;
	tmpaddr = Y(instruction) ;
	if (AC != Mem[tmpaddr])
	    incpc() ;
	break ;
    case SAS:
	DEBUG(SAS) ;
	tmpaddr = Y(instruction) ;
	if (AC == Mem[tmpaddr])
	    incpc() ;
	break ;
    case LIO:
	DEBUG(LIO);
	tmpaddr = Y(instruction) ;
	IO = Mem[tmpaddr] ;
	break ;
    case DIO:
	DEBUG(DIO);
	tmpaddr = Y(instruction) ;
	Mem[tmpaddr] = IO ;
	break ;

    case CAL:	/* subroutine call */
	if (INDIRECT(instruction)) {
	    DEBUG(JDA) ;
	} else {
	    DEBUG(CAL) ;
	}
	tmpaddr= INDIRECT(instruction) ? ADDRESS(instruction) : 0100 ;
	Mem[tmpaddr] = AC ;
	AC = (OV << 17) | PC ;
	PC = (tmpaddr+1) & 07777 ;
	break ;

    case JSP:
	DEBUG(JSP) ;
	tmpaddr = Y(instruction) ;
	AC = (OV<<17) | ADDRESSPART(PC) ;
	PC = tmpaddr ;
	break ;
    case IOT:
	DEBUG(IOT) ;
	switch (instruction&077) {
	case 07:
		/* display a point (intensity in bits 6-8) */
		DisplayPoint(AC, IO, DpyBrightness[(instruction >> 6) & 07]) ;
		break ;
	case 011:
		/* read the control panel, needs to be filled in */
		IO = SW ;
		break ;
	case 0:
		/* sync, I think */
		break ;
	default:
		abort() ;
	} 
	break ;
#ifdef PDP1_MUL_DIV
    /* Later PDP-1s implemented a hardware multiply and divide using the
     * same opcodes.  Select it with -DPDP1_MUL_DIV (needed by e.g. the
     * Snowflake demo).  Spacewar! needs the original step instructions. */
    case MUL:
	DEBUG(MUL) ;
        tmpaddr = Y(instruction) ;
        mb = Mem[tmpaddr] ;
        sign = SIGN(AC) ^ SIGN(mb) ;
        IO = SIGN(AC) ? (AC^0777777) : AC ;
        v = SIGN(mb) ? (mb^0777777) : mb ;
        for (i= AC = 0; i<17; i++) {
            if (IO & 1)
                AC = AC + v ;
            IO = (IO >> 1) | ((AC & 1) << 17) ;
            AC = AC >> 1 ;
        }
        if (sign && (AC | IO)) {
            AC = AC ^ 0777777 ;
            IO = IO ^ 0777777 ;
        }
	break ;
    case DIV:
	DEBUG(DIV) ;
        abort() ;
	break ;
#else
    /* The original PDP-1 implemented multiply and divide as repeated
     * steps.  Spacewar! requires this version. */
    case MUS:	/* multiply step */
	DEBUG(MUS) ;
	tmpaddr = Y(instruction) ;
	if (IO & 1)  {
	    AC = AC + Mem[tmpaddr] ;
	    AC = (AC + (AC >>18))&0777777 ;
	    if (AC == NEGATIVE_ZERO) AC = 0 ;
	}
	IO = ((IO >> 1) | (AC<<17)) & 0777777 ;
	AC >>= 1 ;
	break ;

    case DIS: /* divide step */
	DEBUG(DIS) ;
	tmpaddr = Y(instruction) ;
	sign = SIGN(AC) ;
	AC = ((AC << 1)|(IO>>17)) & 0777777 ;
	IO=(((IO<<1)|sign)^1) & 0777777;
	if (IO & 1) 
	    AC += (Mem[tmpaddr]^0777777) ;
	else
	    AC += 1 + Mem[tmpaddr] ;
	AC = (AC + (AC >> 18)) & 0777777 ;
	if (AC == NEGATIVE_ZERO) AC = 0 ;
	break ;
#endif

    default:
	fprintf(stderr, "PC=0%06o, unknown opcode 0%o, instruction = 0%o\n", 
		PC, operator, instruction) ;
	fprintf(stderr, "%d instructions executed\n", executed) ;
	abort() ;
    }
}


void
SingleStep()
{
    int instruction ;

    instruction = Mem[PC] ;

#if 0
    printf("pc=%04o ac=%06o io=%06o ov=%1d sw=%06o mem[pc]=%06o\n", 
	PC, AC, IO, OV, SW, Mem[PC]) ;
#endif

    incpc() ;

    PerformInstruction(instruction) ;

    executed++ ;
    emu_cycles += InstrCycles(instruction) ;
}

enum {
	LOGO_DISABLED,
	LOGO_DISPLAYED,
	LOGO_HIDDEN
} logostatus = LOGO_DISABLED ;

int logoage = 50 ;
float logox, logoy ;

float
logobrightness(int logoage)
{
    if (logoage >= 40) 
    	return (50-logoage) / 10.0 ;
    else
    	return logoage / 40.0 ;
    	
}

void
displaylogo()
{
    if (logostatus == LOGO_HIDDEN) {
    	logoage -- ;
	if (logoage == 0) {
	    logostatus = LOGO_DISPLAYED ;
	    logoage = 50 ;
	    logox = drand48() * 800.0 - 400.0 ;
	    logoy = drand48() * 800.0 - 400.0 ;
	}
    } else if (logostatus == LOGO_DISPLAYED) {

	/* draw the logo (a 400x100 logical-unit quad centred at logox,logoy) */
	Vector2 tl = ToScreen(logox - 200.0f, logoy + 50.0f) ;
	Rectangle src = { 0.0f, 0.0f, (float)logoTexture.width,
			  (float)logoTexture.height } ;
	Rectangle dst = { tl.x, tl.y, 400.0f * viewScale, 100.0f * viewScale } ;
	Color tint = { 255, 255, 255,
		       (unsigned char)(255.0f * logobrightness(logoage)) } ;

	DrawTexturePro(logoTexture, src, dst, (Vector2){ 0.0f, 0.0f },
	    0.0f, tint) ;

	logoage -- ;

	if (logoage <= 0) {
	    logostatus = LOGO_HIDDEN ;
	    logoage = 500 ;
	} 
    }
}

void
displayfps()
{
    char cbuf[80] ;

    if (!FPSvalid || !FPSflag)
    	return ;

    sprintf(cbuf, "%d fps", (int)(FPS+0.5)) ;

    Vector2 p = ToScreen(-400.0f, -400.0f) ;
    DrawText(cbuf, (int)p.x, (int)p.y, (int)(30.0f * viewScale) + 1,
	(Color){ 26, 255, 26, 255 }) ;
}

void
display(void)
{
    static float theta = 0.0 ;

    if (Spin) {
        theta += 3.3 ;
	if (theta > 360.0)
	    theta = 0.0 ;
    }
    sceneTheta = Spin ? (theta * PI / 180.0f) : 0.0f ;


    displaylogo() ;
    displayfps() ;

    /* now, do the rest */
    DrawDots() ;
    AgeDots(EmuSeconds()) ;

    if (HUD) {
	int nx1 = 03415 ;
	int ny1 = 03445 ;
	int ndx = 03555 ;
	int ndy = 03605 ;
	int nth = 03637 ;

     	int x, y, dx, dy, i ;

	for (i=0; i<24; i++) {
	    x = Mem[nx1+i] >> 8 ;
	    if (x & 01000) { x |= ~01777; x++; } 
	    y = Mem[ny1+i] >> 8 ;
	    if (y & 01000) { y |= ~01777; y++; } 

	    dx = Mem[ndx+i] >> 8 ;
	    if (dx & 01000) { dx |= ~01777; dx++; } 
	    dy = Mem[ndy+i] >> 8 ;
	    if (dy & 01000) { dy |= ~01777; dy++; } 
        
	    DrawLineV(ToScreen(x, y-10), ToScreen(x, y+10),
		(Color){ 255, 0, 0, 255 }) ;
	    DrawLineV(ToScreen(x-10, y), ToScreen(x+10, y),
		(Color){ 255, 0, 0, 255 }) ;
	    DrawLineV(ToScreen(x, y), ToScreen(x+dx, y+dy),
		(Color){ 0, 128, 255, 255 }) ;

	    if (i < 2) {
		float a = (180.0 * Mem[nth+i]) / (PI * 16384.0) ;
		float ar = a * PI / 180.0f ;
		float ca = cosf(ar), sa = sinf(ar) ;
		float vx[3] = { 0.0f, -12.0f, 12.0f } ;
		float vy[3] = { 30.0f, -10.0f, -10.0f } ;
		Vector2 v[3] ;
		int k ;

		for (k=0; k<3; k++) {
		    float rx = vx[k]*ca - vy[k]*sa ;
		    float ry = vx[k]*sa + vy[k]*ca ;
		    v[k] = ToScreen(x+rx, y+ry) ;
		}
		/* screen y is flipped, so wind the triangle the other way */
		DrawTriangle(v[0], v[2], v[1], (Color){ 0, 128, 0, 128 }) ;
	    }
	}
    }
}

#define NX1 (03415)
#define NY1 (03445)
#define NDX (03555) 
#define NDY (03605)
#define	NTH (03637)

static int
remapCoord(int x)
{
    x >>= 8 ;

    if (x & 01000) { x |= ~01777; x++; } 
    return x ;
}

void
ComputeShipStatus(ShipStatus *status)
{
    status->x[0] = remapCoord(Mem[NX1+0]) ;
    status->x[1] = remapCoord(Mem[NX1+1]) ;
    status->y[0] = remapCoord(Mem[NY1+0]) ;
    status->y[1] = remapCoord(Mem[NY1+1]) ;

    status->dx[0] = remapCoord(Mem[NDX+0]) ;
    status->dx[1] = remapCoord(Mem[NDX+1]) ;
    status->dy[0] = remapCoord(Mem[NDY+0]) ;
    status->dy[1] = remapCoord(Mem[NDY+1]) ;

    status->th[0] = Mem[NTH+0] / 16384.0 ;
    status->th[1] = Mem[NTH+1] / 16384.0 ;
}

/* PDP-1 timing.
 *
 * The PDP-1 memory cycle is 5 microseconds, i.e. 200,000 cycles per second
 * (PDP1_HZ, defined above).  An instruction costs one cycle to fetch, plus
 * one more if it references memory, plus one per level of indirect
 * addressing.  The emulation is throttled to this rate by default so that
 * programs (and Spacewar!) run at their true speed.  A speed factor of 0
 * disables throttling entirely.
 */
double speed_scale = 1.0 ;	/* 1.0 = real PDP-1 speed, 0.0 = unthrottled */
double cycles_owed = 0.0 ;	/* real-time cycle budget carried between frames */

/* Number of PDP-1 memory cycles consumed by an instruction. */
int
InstrCycles(int instruction)
{
    int cycles = 1 ;			/* instruction fetch */

    switch (OPERATOR(instruction)) {
    case LAW:
    case JMP:
    case JSP:
    case SKP:
    case SFT:
    case OPR:
    case IOT:
	break ;				/* internal instruction: no operand fetch */
    default:
	cycles++ ;			/* memory reference */
	break ;
    }
    if (INDIRECT(instruction))
	cycles++ ;			/* each level of indirection */
    return cycles ;
}

/* Emulated PDP-1 time, in seconds. */
double
EmuSeconds(void)
{
    return emu_cycles / PDP1_HZ ;
}

/* Run one slice of emulation (called once per rendered frame). */
void
emulate(void)
{
    ShipStatus status ;

    if (autoFlag) {
	ComputeShipStatus(&status) ;
	ShipStrategy(0, 1, &status) ;
	ShipStrategy(1, 0, &status) ;
    }

    if (speed_scale <= 0.0) {
	/* Unthrottled: run a fixed batch as fast as the host allows. */
	int cnt ;
	for (cnt=0; cnt<1000;cnt++) {
	    SingleStep();
	}
    } else {
	/* Throttle to the real PDP-1 cycle rate, measured against wall
	 * clock time so the average speed is independent of frame rate. */
	static double last = -1.0 ;
	double now = GetTime() ;
	int steps = 0 ;
	double dt ;

	if (last < 0.0)
	    last = now ;
	dt = now - last ;
	last = now ;
	if (dt > 0.25)
	    dt = 0.25 ;			/* don't over-catch-up after a stall */

	cycles_owed += dt * PDP1_HZ * speed_scale ;

	while (cycles_owed >= 1.0 && steps < MAXSTEPS_PER_TICK) {
	    int c = InstrCycles(Mem[PC]) ;
	    if (cycles_owed < c)
		break ;
	    SingleStep() ;
	    cycles_owed -= c ;
	    steps++ ;
	}
    }

    FPS = GetFPS() ;
    FPSvalid = (GetTime() > 0.5) ;
}


void
MenuFunc(int cmd)
{
    switch (cmd) {
    case 1:
	exit(0) ;
	break ;
    case 2:
	Sense[2] = !Sense[2] ;
	break ;
    case 3:
	HeavyDots = !HeavyDots ;
	break ;
    case 4:
	HUD = !HUD ;
	break ;
    case 5:
	autoFlag = !autoFlag ;
	if (autoFlag == 0) {
	    /* disengage automatic controls */
	    ShipSteerLeft(0, 0) ;
	    ShipSteerRight(0, 0) ;
	    ShipThrust(0, 0) ;
	    ShipFire(0, 0) ;
	    ShipSteerLeft(1, 0) ;
	    ShipSteerRight(1, 0) ;
	    ShipThrust(1, 0) ;
	    ShipFire(1, 0) ;
	}
	break ;
    case 6:
	if (logostatus == LOGO_DISABLED) {
	    logoage = 50 ;
	    logostatus = LOGO_HIDDEN ;
	} else {
	    logostatus = LOGO_DISABLED ;
	}
	break ;
    case 7:
    	FPSflag = !FPSflag ;
	break ;
    default:
	abort() ;
    }
}


void
FPSFunc(int speed)
{
    if (speed <= 0) {
	speed_scale = 0.0 ;	/* unthrottled */
	SetTargetFPS(0) ;	/* run the loop as fast as the host allows */
    } else {
	speed_scale = speed / 100.0 ;
	SetTargetFPS(60) ;
    }
}

void
ShipSteerLeft(int ship, int flag) 
{
    switch (ship) {
    case 0:
	if (flag) 
	    SW |= 0400000 ;
	else
	    SW &= ~0400000 ;
	break ;
    case 1:
	if (flag) 
	    SW |= 0000010 ;
	else
	    SW &= ~0000010 ;
	break ;
    }
}

void
ShipSteerRight(int ship, int flag) 
{
    switch (ship) {
    case 0:
	if (flag) 
	    SW |=  0200000 ;
	else
	    SW &= ~0200000 ;
	break ;
    case 1:
	if (flag) 
	    SW |=  0000004 ;
	else
	    SW &= ~0000004 ;
	break ;
    }
}

void
ShipThrust(int ship, int flag)
{
    switch(ship) {
    case 0:
	if (flag) SW |=  0100000 ;
	else	  SW &= ~0100000 ;
	break ;
    case 1:
	if (flag) SW |=  0000002 ;
	else	  SW &= ~0000002 ;
	break ;
    }
}

void
ShipFire(int ship, int flag)
{
    switch(ship) {
    case 0:
	if (flag) SW |=  0040000 ;
	else	  SW &= ~0040000 ;
	break ;
    case 1:
	if (flag) SW |=  0000001 ;
	else	  SW &= ~0000001 ;
	break ;
    }
}

/* Keyboard: ship 1 uses a/s/d/w, ship 0 uses j/k/l/i; the old right-click
 * menu items are mapped to single keys. */
void
HandleInput(void)
{
    ShipSteerLeft(1, IsKeyDown(KEY_A)) ;
    ShipThrust(1, IsKeyDown(KEY_S)) ;
    ShipSteerRight(1, IsKeyDown(KEY_D)) ;
    ShipFire(1, IsKeyDown(KEY_W)) ;
    ShipSteerLeft(0, IsKeyDown(KEY_J)) ;
    ShipThrust(0, IsKeyDown(KEY_K)) ;
    ShipSteerRight(0, IsKeyDown(KEY_L)) ;
    ShipFire(0, IsKeyDown(KEY_I)) ;

    if (IsKeyPressed(KEY_P)) MenuFunc(5) ;	/* auto play */
    if (IsKeyPressed(KEY_M)) MenuFunc(3) ;	/* heavy dots */
    if (IsKeyPressed(KEY_V)) MenuFunc(2) ;	/* heavy star */
    if (IsKeyPressed(KEY_F)) MenuFunc(7) ;	/* fps display */
    if (IsKeyPressed(KEY_H)) MenuFunc(4) ;	/* HUD */
    if (IsKeyPressed(KEY_G)) MenuFunc(6) ;	/* ego mode (logo) */

    if (IsKeyPressed(KEY_ONE))   FPSFunc(100) ;	/* real PDP-1 */
    if (IsKeyPressed(KEY_TWO))   FPSFunc(50) ;	/* half speed */
    if (IsKeyPressed(KEY_THREE)) FPSFunc(200) ;	/* double speed */
    if (IsKeyPressed(KEY_FOUR))  FPSFunc(0) ;	/* maximum */
}

void
DrawHelp(void)
{
    static const char *lines[] = {
	"Spacewar! / PDP-1 emulator",
	"",
	"Ship 1:  A S D rotate/thrust,  W fire",
	"Ship 0:  J K L rotate/thrust,  I fire",
	"",
	"P auto play     M heavy dots     V heavy star",
	"F fps display   H HUD            G logo (ego mode)",
	"1 real speed    2 half   3 double   4 maximum",
	"TAB hide help   ESC quit",
    } ;
    int n = sizeof(lines) / sizeof(lines[0]) ;
    int i, fs = (int)(18.0f * viewScale) + 1 ;
    int x = (int)(20.0f * viewScale) + 10 ;
    int y = (int)(20.0f * viewScale) + 10 ;

    for (i=0; i<n; i++)
	DrawText(lines[i], x, y + i*(fs+4), fs,
	    (Color){ 170, 220, 170, 230 }) ;
}

int
main(int argc, char *argv[])
{
    int showHelp = 1 ;
    int i ;
    Image img ;

    (void)argc ; (void)argv ;

    srand48((long)getpid()) ;

    InitDotHashFunc() ;
    InitDotColor(PHOS_R_HALFLIFE, PHOS_G_HALFLIFE, PHOS_B_HALFLIFE, PHOS_GAMMA) ;

    InitWindow(1024, 1024, "Spacewar") ;
    SetTargetFPS(60) ;		/* display refresh; the emulation is
				 * paced independently by wall-clock time */

    /* build the logo texture from the alpha-only image */
    img.width = TEX_XSIZE ;
    img.height = TEX_YSIZE ;
    img.mipmaps = 1 ;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 ;
    img.data = MemAlloc(TEX_XSIZE * TEX_YSIZE * 4) ;
    for (i=0; i<TEX_XSIZE*TEX_YSIZE; i++) {
	((unsigned char *)img.data)[i*4+0] = 255 ;
	((unsigned char *)img.data)[i*4+1] = 255 ;
	((unsigned char *)img.data)[i*4+2] = 255 ;
	((unsigned char *)img.data)[i*4+3] = logo_image[i] ;
    }
    logoTexture = LoadTextureFromImage(img) ;
    SetTextureFilter(logoTexture, TEXTURE_FILTER_POINT) ;
    UnloadImage(img) ;

    while (!WindowShouldClose()) {
	int w = GetScreenWidth(), h = GetScreenHeight() ;

	viewScale = (float)(w < h ? w : h) / 1024.0f ;
	viewCX = w * 0.5f ;
	viewCY = h * 0.5f ;

	if (IsKeyPressed(KEY_TAB))
	    showHelp = !showHelp ;

	HandleInput() ;
	emulate() ;

	BeginDrawing() ;
	    ClearBackground(BLACK) ;
	    display() ;
	    if (showHelp)
		DrawHelp() ;
	EndDrawing() ;
    }

    UnloadTexture(logoTexture) ;
    CloseWindow() ;
    return 0 ;
}
