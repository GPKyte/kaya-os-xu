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

int pager; /* Mutex for page swaping mechanism */
int mutexSems[MAXSEMS];
uProcEntry_t uProcList[MAXUPROC];
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
		newPTEntry->entryLO = DIRTY | GLOBAL | VALID;
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

		/* Fill in default entries */
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

		SYSCALL(CREATEPROCESS, newState); /* SYSCALLs are Main reason for kernel mode */
	}
}

/*****************************************************************************
 * initUProc - loads a user process from the tape drive
 * and copy it to the backing store on disk 0, then start
 * that specific user process.
 *
 ****************************************************************************/
HIDDEN void initUProc() {
  int i, newAreaSP;
  state_PTR newArea; /* TODO: either this or will have to specify area */
  state_t procState;
  unsigned int asid = getASID();

  /* 1) Set up the three new areas for Pass up or die
   *  - status: all INTs ON | LOCTIMERON | VMpON | USERMODEON
   *  - ASID = your asid value
   *  - stack page: to be filled in later
   *  - PC = t9 = address of your P3 handler
   */

  for(i = 0; i < TRAPTYPES; i++) {
    newArea->s_status =
        INTpON | INTMASKOFF | LOCALTIMEON | VMpON | ~USERMODEON;
    newArea->s_asid = getENTRYHI();

    /* TODO: init pgrmTrap and sysCall handlers for P3 */
    /* TODO: stack page */
    switch (i) {
      case (TLBTRAP):
        newArea->s_pc = newArea->s_t9 = (memaddr) tlbHandler;
        newAreaSP = ; /* memory page for sys5 sp for each newArea */
        break;

      case (PROGTRAP):
        newArea->s_pc = newArea->s_t9 = (memaddr) pgrmTrapHandler;
        newAreaSP = ; /* memory page for sys5 sp for each newArea */
        break;

      case (SYSTRAP):
        newArea->s_pc = newArea->s_t9 = (memaddr) sysCallHandler;
        newAreaSP = ; /* memory page for sys5 sp for each newArea */
        break;
    }

    /* call SYS 5 for every trap type (3 times) */
    SYSCALL();
  }

  /*
   *  Read the contents of the tape device (asid-1) onto the
   *  backing store device (disk0)
   *  - keep reading until the tape block marker (data1) is
   *    no longer ENDOFBLOCK
   *    - read block from tape and then immediately write
   *      it out to disk0
   */


  /*  Set up a new state for the user process
   *    - status: all INTs ON | LOCALTIMEON | VMpON | USERMODEON
   *    - asid = your asid
   *    - stack page = last page of kUseg2 (0xC000.0000)
   *    - PC = well known address from the start of kUseg2
   */
  procState.s_status =
      INTpON | INTMASKOFF | LOCALTIMEON | VMpON | USERMODEON;
  procState.s_asid = asid;
  procState.s_sp = KUSEG3START;
  procState.s_pc = procState.s_t9 = (memaddr) KUSEG2START;

  loadState(&procState);
}

/***********************************************************************
 *  getASID - returns the ASID of the currently running process
 ***********************************************************************/
unsigned int getASID() {
  unsigned int asid = getENTRYHI();
  asid = (asid & 0x00000FC0) >> 6;

  return (asid);
}

/********************** Helper Methods *********************/
