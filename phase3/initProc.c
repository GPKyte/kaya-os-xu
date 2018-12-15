/*********************** INITPROC.C ************************
 * Initialize process
 *
 * This module is used to make kernel-level
 *   specifications to new processes and the main
 *   method, ******, should generally be the
 *   starting PC value.
 *
 * One major kernal action is 3x sys5 calls.
 *
 * AUTHORS: Ploy Sithisakulrat & Gavin Kyte
 * ADVISOR/CONTRIBUTER: Michael Goldweber
 * DATE PUBLISHED:
 ***********************************************************/

#include "../h/const.h"
#include "../h/types.h"
#include "../e/scheduler.e"
#include "../e/vmiosupport.e"
#include "../e/adl.e"

#include "usr/local/include/umps2/umps/libumps.e"

uPgTable_PTR kUseg3_pte; /* shared variable among all users */

static int masterSem;
static int pager; /* Mutex for page swaping mechanism */
static int mutexSems[MAXSEMS];
static uProcEntry_t uProcList[MAXUPROC];
segTable_t* segTable = 0x20000500;

/************************ Prototypes ***********************/
unsigned int getASID();

/********************* External Methods ********************/
/*
 * test - Set up the page and segment tables for all 8 user processes
 */
void test() {
	static fpTable_t framePool;
	static osPgTable_t osPgTbl;
	static uPgTable_t sharedPgTbl, uPgTblList[MAXUPROC];
	state_t delayDaemonState;
	state_t newStateList[MAXUPROC];

	devregarea_t* devregarea = (devregarea_t*) RAMBASEADDR;
	unsigned int ramtop = (devregarea->rambase) + (devregarea->ramsize);
	masterSem = 0; /* For graceful halting of OS after test fin */

	/* Set up kSegOS and kuSeg3 Segment Table entries (all the same-ish) */
	for(loopVar = 0; loopVar < MAXPROCID; loopVar++) {
		segTable->kSegOS[loopVar] = &osPgTbl);
		segTable->kuSeg3[loopVar] = &sharedPgTbl);
		/* kuSeg2 handled for each unique process later */
	}

	/* Set up kSegOS page table entries */
	osPgTable.magicPtHeaderWord = MAGICNUM;
	for(loopVar = 0; loopVar < MAXOSPTENTRIES; loopVar++) {
		newPTEntry = &(osPgTable.entries[loopVar]);

		/* TODO: Should we add segment number or asid at all here? (both 0) */
		newPTEntry->entryHI = (KSEGOSVPN + loopVar) << 12;
		newPTEntry->entryLO = (KSEGOSVPN + loopVar) << 12 | DIRTY | GLOBAL | VALID;
	}

	/* Set up kSeg3 page table entries */
	sharedPgTbl.magicPtHeaderWord = MAGICNUM;
	for(loopVar = 0; loopVar < MAXPTENTRIES; loopVar++) {
		newPTEntry = &(sharedPgTbl.entries[loopVar]);

		newPTEntry->entryHI = (KUSEG3 << 30)
			| ((KUSEG3VPN + loopVar) << 12)
			| (MAXPROCID << 6);
		newPTEntry->entryLO = DIRTY | GLOBAL;
	}

	/* TODO: pre-load frame pool */
	/* Initialize Swap Pool structure to manage Frame Pool */
	pager = 1; /* Init mutex for swap pool pager */
	for(loopVar = 0; loopVar < MAXFRAMES; loopVar++) {
		framePool.frames[loopVar] = 0;
		/* Calculate physical location of frame starting address *
		 * Sits beneath the kernel and test stack frames         */
		frameLoc = ramtop - (3 + loopVar) * PAGESIZE;
		framePool.frameAddr[loopVar] = frameLoc;
	}

	/* Init device mutex semaphore array; set each to 1 */
	for(loopVar = 0; loopVar < MAXSEMS; loopVar++) {
		devSemList[loopVar] = 1;
	}

	/* Set up the delay daemon:
	 *   Daemon will have unique ASID in kernel-mode
	 *   with VMc on and all interrupts enabled */
	initADL();
	delayDaemonState.s_asid = delayDaemonID; /* TODO: decide ASID for DD */
	delayDaemonState.s_pc = (memaddr) summonSandmanTheDelayDemon();
 	delayDaemonState.s_status = (VMpON | INTpON | INTMASKOFF) & ~USERMODEON;
	SYSCALL(CREATEPROCESS, &delayDaemonState);

	/* Set up user processes */
	for(asid = 1; asid < MAXUPROC; asid++) {
		/* Set up new page table for process */
		uPgTblList[asid - 1].magicPtHeaderWord = MAGICNUM;

		/* Make Segment Table Entry for uProc's new Page Table */
		segTable->kuSeg2[asid] = &(uPgTblList[asid - 1]));

		/* Fill in default page table entries */
		for(loopVar = 0; loopVar < MAXPTENTRIES; loopVar++) {
			newPTEntry = &(uPgTblList[asid - 1].entries[loopVar]);

			newPTEntry->entryHI = (KUSEG2 << 30)
				| ((KUSEG2VPN + loopVar) << 12)
				| (asid << 6);
			newPTEntry->entryLO = DIRTY;
		}

		/* Correct last entry to act as a stack page */
		newPTEntry->entryHI = (KUSEG3VPN << 12) | (asid << 6)

		/* Fill entry for user process tracking */
		newProcDesc = uProcList[asid - 1];
		newProcDesc->up_syncSem = 0;
		newProcDesc->up_pgTable = &(uPgTblList[asid - 1]);
		newProcDesc->up_bkgStoreAddr = calcBkgStoreAddr(asid, 0);

		/* Create default kernel level state starting in init code */
		newState = &(newStateList[asid - 1]);
		newState->s_asid = asid;
		newState->s_sp = NULL; /* TODO: fill in later, maybe in other code block */
		newState->s_pc = (memaddr) initProc();

		/* Interrupts on, Local Timer On, VM Off, Kernel mode on */
		newState->s_status = (INTMASKOFF | INTpON | LOCALTIMEON)
			& ~VMpON & ~USERMODEON;

		SYSCALL(PASSEREN, &masterSem);
		SYSCALL(CREATEPROCESS, newState); /* SYSCALLs are Main reason for kernel mode */
	}
}

/*****************************************************************************
 * initUProc - loads a user process from the tape drive
 * and copy it to the backing store on disk 0, then start
 * that specific user process.
 *
 * Set up the three new areas for Pass up or die
 *  - status: all INTs ON | LOCTIMERON | VMpON | USERMODEON
 *  - ASID = your asid value
 *  - stack page: to be filled in later
 *  - PC = t9 = address of your P3 handler
 *
 *  Read the contents of the tape device (asid-1) onto the
 *  backing store device (disk0)
 *  - keep reading until the tape block marker (data1) is
 *    no longer ENDOFBLOCK
 *    - read block from tape and then immediately write
 *      it out to disk0
 *
 *  Set up a new state for the user process
 *    - status: all INTs ON | LOCALTIMEON | VMpON | USERMODEON
 *    - asid = your asid
 *    - stack page = last page of kUseg2 (0xC000.0000)
 *    - PC = well known address from the start of kUseg2
 *
 ****************************************************************************/
HIDDEN void initUProc() {
	int status, i;
	state_PTR newArea; /* TODO: either this or will have to specify area */
	state_t uProcState; /* used to update user process' new state */
	device_PTR tape;
	unsigned int asid = getASID();

	/* the tape the data is read from */
	tape = (device_t*) (INTDEVREGSTART + ((TAPEINT-3) *DEVREGSIZE * DEVPERINT) + ((asid-1) * DEVREGSIZE));

	/* Set up the three new areas for Pass up or die
	 *  - status: all INTs ON | LOCTIMERON | VMpON | USERMODEON
	 *  - ASID = your asid value
	 *  - TODO: stack page: to be filled in later
	 *  - PC = t9 = address of your P3 handler
	 */
	 for(i = 0; i < TRAPTYPES; i++) {
		newArea = (&uProcList[asid-1].up_stateAreas[NEW][i]);
		newArea->s_status =
				INTpON | INTMASKOFF | LOCALTIMEON | VMpON | ~USERMODEON;
		newArea->s_asid = getENTRYHI();

		/* TODO: pgrmTrapHandler for P3 */
		/* TODO: stack page for New area*/
		switch (i) {
			case (TLBTRAP):
				newArea->s_pc = newArea->s_t9 = (memaddr) tlbHandler;
				/* memory page for sys5 sp */
				newArea->s_sp = (int)newAreaSPforSYS5(TLBTRAP);
				break;

			case (PROGTRAP):
				newArea->s_pc = newArea->s_t9 = (memaddr) pgrmTrapHandler;
				newArea->s_sp = (int)newAreaSPforSYS5(PROGTRAP);
				break;

			case (SYSTRAP):
				newArea->s_pc = newArea->s_t9 = (memaddr) sysCallHandler;
				newArea->s_sp = (int)newAreaSPforSYS5(SYSTRAP);
				break;
		}
		/* call SYS 5 for every trap type (3 times) */
		SYSCALL(SPECTRAPVEC, i, (int)(&uProcList[asid-1].up_stateAreas[NEW][i]));
	 }

	/*  Read the contents of the tape device (asid-1) onto the
	 *  backing store device (disk0)
	 *  - keep reading until the tape block marker (data1) is
	 *    no longer EOB
	 *    - read block from tape and then immediately write
	 *      it out to disk0 (backing store)
	 */
	 while((tape->d_data1 != EOT) && (tape->d_data1 != EOF)) {
		 /* read contents until data1 is no longer EOB */
		 tape->d_data1 = TAPEBUFFERSSTART + (PAGESIZE * (asid-1));
		 tape->d_command = READBLK; /* issue read block command */

		 status = SYSCALL(WAITIO, TAPEINT, asid-1, 0); /* wait to be read */

		 /* check status. if Device not ready, terminate*/
		 if(status != READY)
		 	SYSCALL(TERMINATE, 0, 0, 0);
	 }

/*  Set up a new state for the user process
 *    - status: all INTs ON | LOCALTIMEON | VMpON | USERMODEON
 *    - asid = your asid
 *    - stack page = last page of kUseg2 (0xC000.0000)
 *    - PC = well known address from the start of kUseg2
 */
	uProcState.s_status =
			ALLOFF | INTpON | INTMASKOFF | LOCALTIMEON | VMpON | VMcON | ~USERMODEON;
	uProcState.s_asid = getENTRYHI();
	uProcState.s_sp = KUSEG3START;
	uProcState.s_pc = uProcState.s_t9 = (memaddr) KUSEG2START;

	loadState(&uProcState);
}

/*************************** Helper Methods ****************************/
/*
 *  getASID - returns the ASID of the currently running process
 */
unsigned int getASID() {
	unsigned int asid = getENTRYHI();
	asid = (asid & 0x00000FC0) >> 6;

	return (asid);
}

/*
 * newAreaSPforSYS5 - get memory page for SYS5 stack page for new area
 */
state_PTR newAreaSPforSYS5(int trapType) {
	return
		(TAPEBUFFERSSTART - (((TRAPTYPES-1) * PAGESIZE * (asid-1)) + (PAGESIZE * trapType)));
}
