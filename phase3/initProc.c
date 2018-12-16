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
#include "../e/vmIOsupport.e"
#include "../e/adl.e"

#include "/usr/local/include/umps2/umps/libumps.e"

uPgTable_PTR kUseg3_pte; /* shared variable among all users */

static fpTable_t framePool;
static int masterSem = 0;
static int pager = 0; /* Mutex for page swaping mechanism */
static int mutexSems[MAXSEMS];
static uProcEntry_t uProcList[MAXUPROC];
segTable_t* segTable = (segTable_t*) 0x20000500;

/************************ Prototypes ***********************/
uint getASID();
void test(void);
HIDDEN void initUProc();
int newAreaSPforSYS5(int trapType);

/********************* External Methods ********************/
/*
 * test - Set up the page and segment tables for all 8 user processes
 */
void test(void) {
	static osPgTable_t osPgTable;
	static uPgTable_t sharedPgTable, uPgTableList[MAXUPROC];
	state_t delayDaemonState, newState;
	state_t newStateList[MAXUPROC];
	ptEntry_PTR newPTEntry;
	int loopVar, trapNo, destAddr, asid, delayDaemonID;
	memaddr frameLoc;
	uProcEntry_PTR newProcDesc;



	devregarea_t* devregarea = (devregarea_t*) RAMBASEADDR;
	uint ramtop = (devregarea->rambase) + (devregarea->ramsize);
	masterSem = 0; /* For graceful halting of OS after test fin */

	/* Set up kSegOS and kuSeg3 Segment Table entries (all the same-ish) */
	for(loopVar = 0; loopVar < MAXPROCID; loopVar++) {
		segTable->kSegOS[loopVar] = &osPgTable;
		segTable->kuSeg3[loopVar] = &sharedPgTable;
		/* kuSeg2 handled for each unique process later */
	}

	/* Set up kSegOS page table entries */
	osPgTable.header = MAGICNUM;
	for(loopVar = 0; loopVar < MAXOSPTENTRIES; loopVar++) {
		newPTEntry = &(osPgTable.entries[loopVar]);

		/* TODO: Should we add segment number or asid at all here? (both 0) */
		newPTEntry->entryHI = (KSEGOSVPN + loopVar) << 12;
		newPTEntry->entryLO = (KSEGOSVPN + loopVar) << 12 | DIRTY | GLOBAL | VALID;
	}

	/* Set up kSeg3 page table entries */
	sharedPgTable.header = MAGICNUM;
	for(loopVar = 0; loopVar < MAXPTENTRIES; loopVar++) {
		newPTEntry = &(sharedPgTable.entries[loopVar]);

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
		mutexSems[loopVar] = 1;
	}

	/* Set up the delay daemon:
	 *   Daemon will have unique ASID in kernel-mode
	 *   with VMc on and all interrupts enabled */
	/* Skip for now TODO: initADL();
	delayDaemonState.s_asid = delayDaemonID = MAXASID - 1;
	delayDaemonState.s_pc = (memaddr) summonSandmanTheDelayDemon();
	delayDaemonState.s_status = (VMpON | INTpON | INTMASKOFF) & ~USERMODEON;
	SYSCALL(CREATEPROCESS, &delayDaemonState); */

	/* Set up user processes */
	for(asid = 1; asid < MAXUPROC; asid++) {
		/* Set up new page table for process */
		uPgTableList[asid - 1].header = MAGICNUM;

		/* Make Segment Table Entry for uProc's new Page Table */
		segTable->kuSeg2[asid] = &(uPgTableList[asid - 1]);

		/* Fill in default page table entries */
		for(loopVar = 0; loopVar < MAXPTENTRIES; loopVar++) {
			newPTEntry = &(uPgTableList[asid - 1].entries[loopVar]);

			newPTEntry->entryHI = (KUSEG2 << 30)
				| ((KUSEG2VPN + loopVar) << 12)
				| (asid << 6);
			newPTEntry->entryLO = DIRTY;
		}

		/* Correct last entry to act as a stack page */
		newPTEntry->entryHI = (KUSEG3VPN << 12) | (asid << 6);

		/* Fill entry for user process tracking */
		newProcDesc = &(uProcList[asid - 1]);
		newProcDesc->up_syncSem = 0;
		newProcDesc->up_pgTable = &(uPgTableList[asid - 1]);
		newProcDesc->up_bkgStoreAddr = calcBkgStoreAddr(asid, 0);

		/* Create default kernel level state starting in init code */
		newState = newStateList[asid - 1];
		newState.s_asid = asid;
		newState.s_sp = (int) NULL; /* TODO: fill in later, maybe in other code block */
		newState.s_pc = (memaddr) initUProc;

		/* Interrupts on, Local Timer On, VM Off, Kernel mode on */
		newState.s_status = (INTMASKOFF | INTpON | LOCALTIMEON)
			& ~VMpON & ~USERMODEON;

		SYSCALL(PASSEREN, (int) &masterSem, 0, 0);
		SYSCALL(CREATEPROCESS, (int) &newState, 0, 0); /* SYSCALLs are Main reason for kernel mode */
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
	int status, trapNo, pageNo, bufferAddr, destAddr;
	state_PTR newArea; /* TODO: either this or will have to specify area */
	state_t uProcState; /* used to update user process' new state */
	device_t* tape;
	uint asid = getASID();

	/* the tape the data is read from */
	tape = (device_t*) (INTDEVREGSTART + ((TAPEINT-3) * DEVREGSIZE * DEVPERINT) + ((asid-1) * DEVREGSIZE));

	/* Set up the three new areas for Pass up or die
	 *  - status: all INTs ON | LOCTIMERON | VMpON | USERMODEON
	 *  - ASID = your asid value
	 *  - TODO: stack page: to be filled in later
	 *  - PC = t9 = address of your P3 handler
	 */
	for(trapNo = 0; trapNo < TRAPTYPES; trapNo++) {
		newArea = &(uProcList[asid-1].up_stateAreas[NEW][trapNo]);
		newArea->s_status =
			INTpON | INTMASKOFF | LOCALTIMEON | VMpON | USERMODEON;
		newArea->s_asid = getENTRYHI();

		/* TODO: upgrmTrapHandler for P3 */
		/* TODO: stack page for New area*/
		switch (trapNo) {
			case (TLBTRAP):
				newArea->s_pc = newArea->s_t9 = (memaddr) uTlbHandler;
				newArea->s_sp = newAreaSPforSYS5(TLBTRAP);
				break;

			case (PROGTRAP):
				newArea->s_pc = newArea->s_t9 = (memaddr) uPgrmTrapHandler;
				newArea->s_sp = newAreaSPforSYS5(PROGTRAP);
				break;

			case (SYSTRAP):
				newArea->s_pc = newArea->s_t9 = (memaddr) uSysCallHandler;
				newArea->s_sp = newAreaSPforSYS5(SYSTRAP);
				break;
		}

		/* call SYS 5 for every trap type (3 times) */
		SYSCALL(SPECTRAPVEC, trapNo, (int) &(uProcList[asid-1].up_stateAreas[NEW][trapNo]), 0);
	}

	/*  Read the contents of the tape device (asid-1) onto the
	 *  backing store device (disk0)
	 *  - keep reading until the tape block marker (data1) is
	 *    no longer EOB
	 *    - read block from tape and then immediately write
	 *      it out to disk0 (backing store)
	 */
	pageNo = 0;
	while((tape->d_data1 != EOT) && (tape->d_data1 != EOF)) {
		/* read contents until data1 is no longer EOB */
		bufferAddr = TAPEBUFFERSSTART + (PAGESIZE * (asid-1));
		tape->d_data1 = bufferAddr;
		tape->d_command = READBLK; /* issue read block command */

		status = SYSCALL(WAITIO, TAPEINT, asid-1, 0); /* wait to be read */

		/* check status: if tape not ready, terminate */
		if(status != READY)
			SYSCALL(TERMINATEPROCESS, 0, 0, 0);

		destAddr = calcBkgStoreAddr(asid, pageNo);
		writePageToBackingStore(bufferAddr, destAddr);
		pageNo++;
	}

	/*  Set up a new state for the user process
	 *    - status: all INTs ON | LOCALTIMEON | VMpON | USERMODEON
	 *    - asid = your asid
	 *    - stack page = last page of kUseg2 (0xC000.0000)
	 *    - PC = well known address from the start of kUseg2
	 */
	uProcState.s_status =
			ALLOFF | INTpON | INTMASKOFF | LOCALTIMEON | VMpON | USERMODEON;
	uProcState.s_asid = getENTRYHI();
	uProcState.s_sp = KUSEG3START;
	uProcState.s_pc = uProcState.s_t9 = (memaddr) KUSEG2START;

	loadState(&uProcState);
}

/*************************** Helper Methods ****************************/
/*
 *  getASID - returns the ASID of the currently running process
 */
uint getASID() {
	uint asid = getENTRYHI();
	asid = (asid & 0x00000FC0) >> 6;

	return (asid);
}

/*
 * newAreaSPforSYS5 - calculate memory page for SYS5 stack page for new area
 */
int newAreaSPforSYS5(int trapType) {
	/* Calculate address of page in OS memory to act as stack for SYS 5 handling */
	int topStackPageNo = TAPEBUFFERSSTART / PAGESIZE;
	int downwardOffset = (TRAPTYPES * (getASID() - 1)) + trapType;
	return (topStackPageNo - downwardOffset) * PAGESIZE;
}
