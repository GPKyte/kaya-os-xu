/************************ INITPROC.C ************************
 * initProc module is used to make kernel-level
 *   specifications to new processes and the main
 *   method, initUProc, should generally be the
 *   starting PC value of user processes.
 *
 * This module also implements initUProc to hold two major
 * steps in launching a U-Proc:
 *
 *   step 1 is variable initialization where each U-Proc
 *   is assigned a unique ID (ASID), as well as U-Proc's
 *   kUseg2 PTE, private sema4, segment table, and three new
 *   areas are initialized, and then perform SYS1.
 *
 *	 In step 2, SYS5 is issued three times (one for each
 *   new area) and writing to backing store for every block
 *   of tape read.
 *
 * AUTHORS: Ploy Sithisakulrat & Gavin Kyte
 * ADVISOR/CONTRIBUTER: Michael Goldweber
 * DATE PUBLISHED:
 ***********************************************************/

#include "../h/const.h"
#include "../h/types.h"
#include "../e/vmIOsupport.e"
/* #include "../e/adl.e" */

#include "/usr/local/include/umps2/umps/libumps.e"

uPgTable_PTR kUseg3_pte; /* shared variable among all users */

fpTable_t framePool;
int masterSem = 0;
int pager = 0; /* Mutex for page swaping mechanism */
int mutexSems[MAXSEMS];
uProcEntry_t uProcList[MAXUPROC];
segTable_t* segTable = (segTable_t*) 0x20000500;

/************************ Prototypes ***********************/
uint getASID();
void test(void);
HIDDEN void initUProc();
void contextSwitch(state_PTR newContext);
int newAreaSPforSYS5(int trapType);

HIDDEN int debugIP(int a, int b, int c, int d) {
	int debugVarToKeepFromDisappearing = a + b + c + d;
	return debugVarToKeepFromDisappearing;
}

/********************* External Methods ********************/
/*
 * test - Set up the page and segment tables for all 8 user processes
 */
void test(void) {
	static osPgTable_t osPgTable;
	static uPgTable_t sharedPgTable, uPgTableList[MAXUPROC];
	state_t delayDaemonState, *newState;
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
		newPTEntry->entryHI = ((KUSEG3VPN - 1) << 12) | (asid << 6);

		/* Fill entry for user process tracking */
		newProcDesc = &(uProcList[asid - 1]);
		newProcDesc->up_syncSem = 0;
		newProcDesc->up_pgTable = &(uPgTableList[asid - 1]);
		newProcDesc->up_bkgStoreAddr = calcBkgStoreAddr(asid, 0);

		STST(newState);
		debugIP(151, newState->s_sp, 0, 0);
		/* Create default kernel level state starting in init code, fill in s_sp later */
		newState = &(newStateList[asid - 1]);
		newState->s_asid = asid << 6;
		newState->s_pc = newState->s_t9 = (memaddr) initUProc;

		/* Interrupts on, Local Timer On, VM Off, Kernel mode on */
		newState->s_status = (INTMASKOFF | INTpON | LOCALTIMEON)
			& ~VMpON & ~USERMODEON;

		debugIP(159, (int) initUProc, (int) getASID, newState->s_sp);
		if(masterSem >= 6) debugIP(160,masterSem,newState->s_asid,newState->s_status);
		/* SYSCALL(PASSEREN, (int) &masterSem, 0, 0); */
		masterSem++;
		SYSCALL(CREATEPROCESS, (int) newState, 0, 0); /* SYSCALLs are Main reason for kernel mode */
	}
}

/*****************************************************************************
 * initUProc - assigns each process a unique ASID, set up new areas, and
 * loads a user process from the tape drive and copy it to the backing store
 * on disk 0, then start that specific user process.
 ****************************************************************************/
HIDDEN void initUProc() {
	int status, trapNo, pageNo, bufferAddr, destAddr;
	state_PTR newArea; /* TODO: either this or will have to specify area */
	state_t uProcState; /* used to update user process' new state */
	device_t* tape;
	uint asid;
	
	debugIP(177, 0,0,0);
	asid = getASID();

	/* the tape the data is read from */
	tape = (device_t*) (INTDEVREGSTART + ((TAPEINT-3) * DEVREGSIZE * DEVPERINT) + ((asid-1) * DEVREGSIZE));

	debugIP(182, asid ,0,0);
	/* Set up the three new areas for Pass up or die */
	for(trapNo = 0; trapNo < TRAPTYPES; trapNo++) {
		newArea = &(uProcList[asid-1].up_stateAreas[NEW][trapNo]);
		newArea->s_status =
			INTpON | INTMASKOFF | LOCALTIMEON | VMpON | USERMODEON;
		newArea->s_asid = getENTRYHI();

		debugIP(190, newArea->s_status, newArea->s_asid, asid);
		/* TODO: pgrmTrapHandler for P3 */
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

	/* Read the contents of the tape device onto the backing store device */
	pageNo = 0;
	/* TODO: Skip header space */
	while((tape->d_data1 != EOT) && (tape->d_data1 != EOF)) {
		/* read contents until data1 is no longer EOB */
		bufferAddr = TAPEBUFFERSSTART + (PAGESIZE * (asid-1));
		tape->d_data1 = bufferAddr;
		tape->d_command = READBLK; /* issue read block command */

		status = SYSCALL(WAITIO, TAPEINT, asid-1, 0); /* wait to be read */

		/* check status: if tape not ready, terminate */
		if(status != READY)
			SYSCALL(TERMINATEPROCESS, 0, 0, 0);

		/* write page to backing store */
		destAddr = calcBkgStoreAddr(asid, pageNo);
		writePageToBackingStore(bufferAddr, destAddr);
		pageNo++;
	}

	/*  Set up a new state for the user process */
	uProcState.s_status =
			ALLOFF | INTpON | INTMASKOFF | LOCALTIMEON | VMpON | USERMODEON;
	uProcState.s_asid = getENTRYHI();
	uProcState.s_sp = KUSEG3START;
	uProcState.s_pc = uProcState.s_t9 = (memaddr) KUSEG2START;

	contextSwitch(&uProcState);
}

/*************************** Helper Methods ****************************/
/*
 * getASID - returns the ASID of the currently running process
 *
 * RETURN: ASID - Address Space Identifier for current process
 */
uint getASID() {
	uint asid = getENTRYHI();
	asid = (asid & 0x00000FC0) >> 6;

	return (asid);
}

/*
 * contextSwitch - generate a context switch. Must be in Kernel mode to use!
 */
void contextSwitch(state_PTR newContext) {
	LDST(newContext);
}

/*
 * newAreaSPforSYS5 - calculate memory page for SYS5 stack page for new area
 *
 * PARAM:		trapType - type of exception handler
 * RETURN:	memory stack page for SYS5 for new area
 */
int newAreaSPforSYS5(int trapType) {
	/* Calculate address of page in OS memory to act as stack for SYS 5 handling */
	int topStackPageNo = TAPEBUFFERSSTART / PAGESIZE;
	int downwardOffset = (TRAPTYPES * (getASID() - 1)) + trapType;
	return (topStackPageNo - downwardOffset) * PAGESIZE;
}
