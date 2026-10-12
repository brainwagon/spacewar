#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/glut.h>
#include <GL/gl.h>
#include <assert.h>

#include "ship.h"
#include "logo.h"

#ifdef WIN32
#define lrand48()	rand()
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

	

int executed ; 	/* number of instructions executed */
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

GLuint logoName ;

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
NewDot(int x, int y)
{
    Dot *dot ;
    int idx ;

    if (dot=FindDot(x, y)) {
	dot->age = 0 ;
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
    idx = Hash(x, y) ;
    dot->next = DotHashTable[idx] ;
    DotHashTable[idx] = dot ;
}

void
AgeDots(int maxage)
{
    int i ;
    Dot *dot, *nextdot ;

    for (i=0; i<HASH_SIZE; i++) {
	dot = DotHashTable[i] ;
        DotHashTable[i] = NULL ;

	while (dot) {
	    nextdot = dot->next ;

	    dot->age++ ;
	    if (dot->age < maxage) {
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

#define REALMAXAGE	(200)

int MaxAge ;
float DotColor[REALMAXAGE][3] ;

#define MAX(a,b)	((a)>(b)?(a):(b))
#define MAX3(a,b,c)	MAX(MAX(a,b),c)

void
InitDotColor(float rd, float gd, float bd, float dotgamma)
{
    float rk, gk, bk ;
    int rm, gm, bm, i ;

    rk = log(0.5) / -rd ;
    gk = log(0.5) / -gd ;
    bk = log(0.5) / -bd ;

    rm = ceil(log(0.03) / -rk) ;
    gm = ceil(log(0.03) / -gk) ;
    bm = ceil(log(0.03) / -bk) ;

    MaxAge = MAX3(rm, gm, bm) ;

    if (MaxAge >= REALMAXAGE) 
	abort() ;

    for (i=0; i<MaxAge; i++) {
	DotColor[i][0] = pow(exp(-rk * i), 1.0/dotgamma) ;
	DotColor[i][1] = pow(exp(-gk * i), 1.0/dotgamma) ;
	DotColor[i][2] = pow(exp(-bk * i), 1.0/dotgamma) ;
    }
}

void
DrawDots() 
{
    Dot *dot;
    int i ;

    glBegin(GL_POINTS) ;
	for (i=0; i<HASH_SIZE; i++) {
	    dot = DotHashTable[i] ;
	    while (dot) {
		glColor4f(DotColor[dot->age][0],
		    DotColor[dot->age][1],
		    DotColor[dot->age][2],
		    DotColor[dot->age][0]) ;
		glVertex2i(dot->x, dot->y) ;
		dot = dot->next ;
	    }
	}
    glEnd() ;
}



void
DisplayPoint(int x, int y)
{
    x >>= 8 ;
    y >>= 8 ;
 
    if (x & 01000) { x |= ~01777; x++; } /* convert to two's complement */
    if (y & 01000) { y |= ~01777; y++; }

    NewDot(x, y) ;
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
    int operator, tmpaddr, i, cond, sign, mb, v ;

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
	    if (opr & 02000) AC = 0 ;
	    if (opr & 00200) AC = 0 ;
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
		/* display a point */
		DisplayPoint(AC, IO) ;
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
#if 0
    /* The original PDP1 implemented these two instructions that
     * helped implement multiply and divide.  Later versions implemented
     * an actual multiply and divide using the same opcodes.  Spacewar! 
     * requires the MUS/DIS version, whereas the Snowflake Demo requires
     * MUL/DIV.
     */
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
#else
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

    switch (PC) {
    case 01130:
	/* Draw the background stars */
	glColor3f(1.0, 1.0, 1.0) ;
	break ;
    case 0650:
	/* Drawing the sun */
	glColor3f(1.0, 1.0, 0.0) ;
	break ;
    }
    executed++ ;
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
    	
	/* draw the logo */
	glPushMatrix() ;
	glTranslatef(logox,  logoy, 0.0) ;
	glEnable(GL_TEXTURE_2D) ;

	    glBindTexture(GL_TEXTURE_2D, logoName) ;

	    glColor4f(1.0, 1.0, 1.0, logobrightness(logoage)) ;

	    glBegin(GL_QUADS) ;
		glTexCoord2f(0.0, 1.0) ;    glVertex2f(-200.0, -50.0) ;
		glTexCoord2f(0.0, 0.0) ;    glVertex2f(-200.0,  50.0) ;
		glTexCoord2f(1.0, 0.0) ;    glVertex2f( 200.0,  50.0) ;
		glTexCoord2f(1.0, 1.0) ;    glVertex2f( 200.0, -50.0) ;
	    glEnd() ;

	glDisable(GL_TEXTURE_2D) ;
	glPopMatrix() ;

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
    int i ;

    if (!FPSvalid || !FPSflag)
    	return ;

    sprintf(cbuf, "%d fps", (int)(FPS+0.5)) ;

    glPushMatrix() ;
	glColor3f(0.1, 1.0, 0.1) ;
	glTranslatef(-400, -400, 0) ;
	glScalef(0.25, 0.25, 1.0) ;
	for (i=0; i<strlen(cbuf); i++) 
	    glutStrokeCharacter(GLUT_STROKE_ROMAN, cbuf[i]) ;
    glPopMatrix() ;
}

void
display(void)
{
    static float theta = 0.0 ;

    glPushMatrix() ;

    if (Spin) {
        theta += 3.3 ;
	if (theta > 360.0)
	    theta = 0.0 ;
	glRotatef(theta, 0, 0, 1) ;
    }


    displaylogo() ;
    displayfps() ;

    /* now, do the rest */
    DrawDots() ;
    AgeDots(MaxAge) ;

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
        
	    glBegin(GL_LINES) ;
		glColor3f(1.0, 0.0, 0.0) ;
		glVertex2i(x, y-10) ;
		glVertex2i(x, y+10) ;
	    glEnd() ;

	    glBegin(GL_LINES) ;
		glColor3f(1.0, 0.0, 0.0) ;
		glVertex2i(x-10, y) ;
		glVertex2i(x+10, y) ;
	    glEnd() ;

	    glBegin(GL_LINES) ;
		glColor3f(0.0, 0.5, 1.0) ;
		glVertex2i(x, y) ;
		glVertex2i(x+dx, y+dy) ;
	    glEnd() ;

	    if (i < 2) {
		float a = (180.0 * Mem[nth+i]) / (3.14159 * 16384.0) ;
		glPushMatrix() ;
		glTranslatef(x, y, 0) ;
		glRotatef(a, 0.0, 0.0, 1.0) ;
		glBegin(GL_POLYGON) ;
		    glColor4f(0.0, 0.5, 0.0, 0.5) ;
		    glVertex2f(0.0, 30.0) ;
		    glVertex2f(-12.0, -10.0) ;
		    glVertex2f( 12.0, -10.0) ;
		glEnd() ;
		glPopMatrix() ;
	    }
	}
    }

    glPopMatrix() ;

    glutSwapBuffers() ;

    glClearColor(0.0, 0.0, 0.0, 0.0) ;
    glClear(GL_COLOR_BUFFER_BIT) ;

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

int timedelay = 0 ;

/* ARGSUSED */ void
idle(int value)
{
    ShipStatus status ;

    if (autoFlag) {
	ComputeShipStatus(&status) ;
	ShipStrategy(0, 1, &status) ;
	ShipStrategy(1, 0, &status) ;
    }

#if 0
    do {
	SingleStep() ;
    } while (PC != 02051) ;
#else
    int cnt ;
    for (cnt=0; cnt<1000;cnt++) {
        SingleStep();
    }
#endif

    glutPostRedisplay() ;
    glutTimerFunc(timedelay, idle, 0) ;

    /* compute frames per second...
     */

#define NTIMES	100	/* average frame times over NTIMES */
    {
    	static int stime[NTIMES], ctime, ftime ;
	static int idx = 0 ;

	if (!FPSvalid) {
	    stime[idx] = glutGet(GLUT_ELAPSED_TIME) ;
	    idx ++ ;
	    if (idx >= NTIMES) {
	    	FPSvalid = 1 ;
		idx = 0 ;
	    }
	} else {
	    ctime = glutGet(GLUT_ELAPSED_TIME) ;
	    ftime = (ctime - stime[idx]) / NTIMES ;
	    FPS = 1000.0 / ftime ;
	    stime[idx] = ctime ;
	    idx ++ ;
	    if (idx >= NTIMES)
	    	idx = 0 ;
	}
    }
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
	if (HeavyDots)
	    glPointSize(2.0) ;
	else
	    glPointSize(1.0) ;
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
FPSFunc(int fps)
{
    switch (fps) {
    case 24:
	timedelay = 20 ;
	break ;
    case 30:
	timedelay = 15 ;
	break ;
    case 60:
	timedelay = 5 ;
	break ;
    default:
        timedelay = 0 ;
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

/*ARGSUSED*/ void
KeyDown(unsigned char key, int x, int y) 
{
    switch (key) {
    case 'a': ShipSteerLeft(1, 1) ; break ;
    case 's': ShipThrust(1, 1) ; break ;
    case 'd': ShipSteerRight(1, 1) ; break ;
    case 'w': ShipFire(1, 1) ; break ;
    case 'j': ShipSteerLeft(0, 1) ; break ;
    case 'k': ShipThrust(0, 1) ; break ;
    case 'l': ShipSteerRight(0, 1) ; break ;
    case 'i': ShipFire(0, 1) ; break ;
    }
}

/*ARGSUSED*/ void
KeyUp(unsigned char key, int x, int y) 
{
    switch (key) {
    case 'a': ShipSteerLeft(1, 0) ; break ;
    case 's': ShipThrust(1, 0) ; break ;
    case 'd': ShipSteerRight(1, 0) ; break ;
    case 'w': ShipFire(1, 0) ; break ;
    case 'j': ShipSteerLeft(0, 0) ; break ;
    case 'k': ShipThrust(0, 0) ; break ;
    case 'l': ShipSteerRight(0, 0) ; break ;
    case 'i': ShipFire(0, 0) ; break ;
    }
}

void
reshape(int width, int height) 
{
    glViewport(0, 0, (GLsizei) width, (GLsizei) height) ;
    glMatrixMode(GL_PROJECTION) ;
    glLoadIdentity() ;
    if (width <= height) {
	glOrtho(-512, 512, -512*(GLfloat)height/(GLfloat)width,
		512*(GLfloat)height/(GLfloat)width, -1.0, 1.0) ;
    } else {
	glOrtho(-512*(GLfloat)width/(GLfloat)height,
		512*(GLfloat)width/(GLfloat)height, -512, 512, -1.0, 1.0) ;
    }
    glMatrixMode(GL_MODELVIEW) ;
    glLoadIdentity() ;
}

main(int argc, char *argv[])
{

    srand48((long)getpid()) ;

    InitDotHashFunc() ;
    InitDotColor(3.0, 8.0, 3.0, 2.0) ;

    /* initialize glut */
    glutInit(&argc, argv) ;
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB) ;
    glutInitWindowPosition(100,100) ;
    glutInitWindowSize(1024, 1024) ;

    glutCreateWindow("Spacewar") ;
    /* glutFullScreen() ; */

#ifdef WIN32
    glEnable(GL_POINT_SMOOTH) ;
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST) ;
#endif
    glPointSize(2.0) ;
    glEnable(GL_BLEND) ;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) ;

    /* initialize the logo texture maps */
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1) ;
    glGenTextures(1, &logoName) ;
    glBindTexture(GL_TEXTURE_2D, logoName) ;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP) ;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP) ;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 
    	GL_NEAREST) ;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 
    	GL_NEAREST) ;
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE) ;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA8, TEX_XSIZE, TEX_YSIZE,
    	0, GL_ALPHA, GL_UNSIGNED_BYTE, logo_image) ;

    {
	int menu = glutCreateMenu(MenuFunc) ;
	int fpsmenu = glutCreateMenu(FPSFunc) ;

	glutSetMenu(fpsmenu) ;
	glutAddMenuEntry("24 fps", 24) ;
	glutAddMenuEntry("30 fps", 30) ;
	glutAddMenuEntry("60 fps", 60) ;
	glutAddMenuEntry("Maximum", 10000) ;

	glutSetMenu(menu) ;
	glutAddMenuEntry("Toggle Auto Play", 5) ;
	glutAddMenuEntry("Toggle Heavy Dots", 3) ;
	glutAddMenuEntry("Toggle Heavy Star", 2) ;
	glutAddMenuEntry("Toggle FPS Display", 7) ;
	glutAddMenuEntry("Toggle HUD", 4) ;
	glutAddMenuEntry("Toggle Ego Mode", 6) ;
	glutAddSubMenu("Set Target FPS", fpsmenu) ;
	glutAddMenuEntry("Quit", 1) ;
	glutAttachMenu(GLUT_RIGHT_BUTTON) ;

    }

    glMatrixMode(GL_PROJECTION) ;
    glLoadIdentity() ;
    glOrtho(-512.0, 512.0, -512.0, 512.0, -1.0, 1.0) ;

    glutReshapeFunc(reshape) ;
    glutDisplayFunc(display) ;
    glutIgnoreKeyRepeat(1) ;
    glutTimerFunc(timedelay, idle, 0) ;
    glutKeyboardFunc(KeyDown) ;
    glutKeyboardUpFunc(KeyUp) ;
    glutMainLoop() ;
}
