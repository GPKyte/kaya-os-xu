#ifndef CONSTS
#define CONSTS

/****************************************************************************
 *
 * This header file contains utility constants & macro definitions.
 *
 ****************************************************************************/

/* Hardware & software constants */
#define PAGESIZE		4096	/* page size in bytes */
#define WORDLEN			4		/* word size in bytes */
#define PTEMAGICNO		0x2A

#define ROMPAGESTART	0x20000000	 /* ROM Reserved Page */
#define INTOLDAREA		ROMPAGESTART
#define TLBOLDAREA		ROMPAGESTART + 2 * STATESIZE
#define PGRMOLDAREA		ROMPAGESTART + 4 * STATESIZE
#define SYSOLDAREA		ROMPAGESTART + 6 * STATESIZE

/* timer, timescale, TOD-LO and other bus regs */
#define RAMBASEADDR   0x10000000
#define TODLOADDR     0x1000001C
#define INTERVALTMR   0x10000020
#define TIMESCALEADDR 0x10000024
#define QUANTUMTIME   5000 /* microseconds, 5 milliseconds */
#define INTERVALTIME  100000

/* System call constants */
#define CREATEPROCESS				1
#define TERMINATEPROCESS			2
#define VERHOGEN				3
#define PASSEREN				4
#define SPECTRAPVEC				5
#define GETCPUTIME				6
#define WAITCLOCK				7
#define WAITIO					8

/* utility constants */
#define	TRUE		1
#define	FALSE		0
#define	ON			1
#define	OFF			0
#define	OLD			0
#define	NEW			1
#define	HIDDEN	static
#define	Bool		int
#define	EOS		'\0'

/* Bitwise masks and constants */
/* Turn 1 and 2 On, but 3 off: 1ON | 2ON & ~3ON */
#define VMpON		(1 << 25)
#define INTpON	(1 << 2)
#define INTMASKOFF 	(255 << 8) /* Not masking means interrupts on */
#define USERMODEON	(1 << 3)
#define LOCALTIMEON	(1 << 27)
/* Cause register */
#define INTPENDMASK (255 << 8)

#define NULL ((void *)0xFFFFFFFF)
#define MAXINT ((void *)0x7FFFFFFF)

/* vectors number and type */
#define VECTSNUM	4

#define TLBTRAP		0
#define PROGTRAP	1
#define SYSTRAP		2

#define TRAPTYPES	3


/* device interrupts */
#define DISKINT		3
#define TAPEINT		4
#define NETWINT		5
#define PRNTINT		6
#define TERMINT		7

#define DEVREGLEN	4	/* device register field length in bytes & regs per dev */
#define DEVREGSIZE	16	/* device register size in bytes */

/* device register field number for non-terminal devices */
#define STATUS		0
#define COMMAND		1
#define DATA0		2
#define DATA1		3

/* device register field number for terminal devices */
#define RECVSTATUS		0
#define RECVCOMMAND		1
#define TRANSTATUS		2
#define TRANCOMMAND		3


/* device common STATUS codes */
#define UNINSTALLED	0
#define READY		1
#define BUSY		3

/* device common COMMAND codes */
#define RESET		0
#define ACK			1

/* operations */
#define	MIN(A,B)	((A) < (B) ? A : B)
#define MAX(A,B)	((A) < (B) ? B : A)
#define	ALIGNED(A)	(((unsigned)A & 0x3) == 0)

/* Useful operations */
/* Accesses time of day clock at that moment, this TOD clock has no interrupt */
#define STCK(T) ((T) = ((* ((cpu_t *) TODLOADDR)) / (* ((cpu_t *) TIMESCALEADDR))))
/* Sets count-down timer, used for giving a process a QUANTUMTIME to execute */
#define LDIT(T)	((* ((cpu_t *) INTERVALTMR)) = (T) * (* ((cpu_t *) TIMESCALEADDR)))


#endif
