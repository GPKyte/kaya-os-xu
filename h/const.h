#ifndef CONSTS
#define CONSTS

/****************************************************************************
 *
 * This header file contains utility constants & macro definitions.
 *
 ****************************************************************************/

/* File location descriptors for PANIC psuedo status code */
#define EXCEP	0
#define INIT	1
#define INTER	2
#define SCHED	3
#define TEST	4

/* Hardware & software constants */
#define PAGESIZE		4096	/* page size in bytes */
#define WORDLEN			4		/* word size in bytes */
#define PTEMAGICNO		0x2A

#define ROMPAGESTART	0x20000000	 /* ROM Reserved Page */
#define INTOLDAREA	0x20000000
#define INTNEWAREA	0x2000008c
#define TLBOLDAREA	0x20000118
#define TLBNEWAREA	0x200001a4
#define PGRMOLDAREA	0x20000230
#define PGRMNEWAREA	0x200002bc
#define SYSOLDAREA	0x20000348
#define SYSNEWAREA	0x200003d4

/* timer, timescale, TOD-LO and other bus regs */
#define RAMBASEADDR   0x10000000
#define TODLOADDR     0x1000001C
#define INTERVALTMR   0x10000020
#define TIMESCALEADDR 0x10000024
#define QUANTUMTIME   50000000 /* microseconds, 5 milliseconds */
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
#define GETTIME					17

/* utility constants */
#define TRUE		1
#define FALSE		0
#define ON		1
#define OFF		0
#define OLD		0
#define NEW		1
#define CHILD		0
#define NOCHILD	-1
#define HIDDEN		static
#define Bool		int
#define EOS		'\0'
#define EOL		0x0A
#define NULL ((void *) 0xFFFFFFFF)
#define MAXINT ((int *) 0x7FFFFFFF)
#define READ    0
#define WRITE   1

/* Bitwise masks and constants */
/* Turn 1 and 2 On, but 3 off: 1ON | 2ON & ~3ON */
#define ALLOFF		0
#define VMpON		(1 << 25)
#define INTpON		(1 << 2)
#define INTcON		(1)
#define INTMASKOFF	(255 << 8) /* Not masking means interrupts on */
#define USERMODEON	(1 << 3)
#define LOCALTIMEON	(1 << 27)

 /* Cause register */
#define NOCAUSE		~(124) /* 0b1111100 */
#define RESERVEDINSTERR (10 << 2) /* 0b101000 */
#define INTPENDMASK 	(255 << 8)

#define TRANSMITSTATUSMASK 0x0F /* For Term Read Status */

/* page table operations */
#define OSFRAMES      30
#define MAXPAGES      32

#define KSEGOS        0
#define KSEGOSSTART   ROMPAGESTART
#define KSEGOSVPN     ROMPAGESTART / PAGESIZE
#define KSEGOSSIZE    OSFRAMES * PAGESIZE
#define KUSEG2        2
#define KUSEG2START   0x80000000
#define KUSEG2VPN     KUSEG2START / PAGESIZE
#define KUSEG3        3
#define KUSEG3START   0xC0000000
#define KUSEG3VPN     KUSEG3START / PAGESIZE
#define KSEGOSEND     KUSEG2START - 1

#define DIRTY         (1 << 10)
#define GLOBAL        (1 << 9)
#define VALID         (1 << 8)
#define MAGICNUM      (42 << 24)

#define MAGICNUMMASK  0xFF000000 /* Bits 24-31 On */
#define ENTRYCNTMASK  0x000FFFFF /* Bits 0-19 On */
#define PFNMASK       0xFFFFF000
#define VPNMASK       0x3FFFF000
#define ASIDMASK      0x00000fc0
#define SEGMASK       0xB0000000

#define UPROCSTACKSIZE  (2 * PAGESIZE)
#define DISKBUFFERSTART  (KSEGOSSTART + KSEGOSSIZE)
#define TAPEBUFFERSSTART  (DISKBUFFERSTART + PAGESIZE)

/* Segment Table */
#define SEGMENTS      3
#define MAXPROCID     64
#define MAXUPROC      8

/* vectors number and type */
#define VECTSNUM	4

#define TLBTRAP		0
#define PROGTRAP	1
#define SYSTRAP		2

#define TRAPTYPES	3

/* device interrupts */
#define LINENUMOFFSET	3
#define DISKINT		3
#define TAPEINT		4
#define NETWINT		5
#define PRNTINT		6
#define TERMINT		7

#define INTDEVREGSTART  0x10000050
#define INTDEVBITMAP    0x1000003c

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

/* terminal success STATUS codes */
#define CRECVD    5
#define CTRANSD   5

/* device common STATUS codes */
#define UNINSTALLED	0
#define READY		1
#define BUSY		3

/* device common COMMAND codes */
#define RESET		0
#define ACK			1
#define PRINTCHR	2

/* disk device COMMAND codes */
#define SEEKCYL   2
#define READBLK   3
#define WRITBLK   4

/* terminal COMMAND codes */
#define RECEIVECMD	2
#define TRANCMD		2

/* TLB ERROR codes */
#define PAGELOADMISS	2
#define PAGESTRMISS		3

/* tape device marker */
#define EOT   0
#define EOF   1
#define EOB   2
#define TS    3

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
