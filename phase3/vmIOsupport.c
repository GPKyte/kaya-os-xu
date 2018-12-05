/********************** VMIOSUPPORT.C **********************
 *
 *
 *
 *
 *
 *
 *
 *
 *
 * AUTHORS: Ploy Sithisakulrat & Gavin Kyte
 * ADVISOR/CONTRIBUTER: Michael Goldweber
 * DATE PUBLISHED:
 ***********************************************************/

#include "../h/types.h"
#include "../h/const.h"
#include "../e/initProc.e"

#include "usr/local/include/umps2/umps/libumps.e"


int* pager; /* Mutex for page swaping mechanism */
int* mutexSems[MAXSEMS];
int segTable[SEGMENTS][MAXPROCID]; /* TODO: find a generic type solution that better describes segTbl */
uProcEntry_t uProcList[MAXUPROC];

/************************ Prototypes ***********************/




/********************* External Methods ********************/
/*
 * test - Set up the page and segment tables for all 8 user processes
 */
void test() {
	static fpTable_t framePool;
	static osPgTable_t osPgTbl;
	static uPgTable_t sharedPgTbl, uPgTblList[MAXUPROC];
	state_t newStateList[MAXUPROC];

	unsigned int ramtop;
	devregarea_t* devregarea;

	/* Set up kSegOS and kuSeg3 Segment Table entries (all the same-ish) */
	for(loopVar = 0; loopVar < MAXPROCID; loopVar++) {
		setSegmentTableEntry(KSEGOS, loopVar, &osPgTbl);
		setSegmentTableEntry(KUSEG3, loopVar, &sharedPgTbl);
		/* kuSeg2 handled for each unique process later */
	}

	/* Set up kSegOS page tables */
	osPgTable.magicPtHeaderWord = MAGICNUM;
	for(loopVar = 0; loopVar < MAXOSPTENTRIES; loopVar++) {
		newPTEntry = &(osPgTable.entries[loopVar]);

		/* TODO: Should we add segment number or asid at all here? (both 0) */
		newPTEntry->entryHI = KSEGOSVPN << 12;
		newPTEntry->entryLO = DIRTY | GLOBAL | VALID;
	}

	/* Set up kSeg3 page tables */
	sharedPgTbl.magicPtHeaderWord = MAGICNUM;
	for(loopVar = 0; loopVar < MAXPTENTRIES; loopVar++) {
		newPTEntry = &(sharedPgTbl.entries[loopVar]);

		newPTEntry->entryHI = (KUSEG3 << 30)
			| (KUSEG3START << 12)
			| (MAXPROCID << 6);
		newPTEntry->entryLO = DIRTY | GLOBAL;
	}

	/* Possible opportunity for optimization is pre-loading frame pool */
	/* Initialize Swap Pool structure to manage Frame Pool */
	devregarea = (devregarea_t*) RAMBASEADDR;
	ramtop = (devregarea->rambase) + (devregarea->ramsize);
	for(loopVar = 0; loopVar < MAXFRAMES; loopVar++) {
		framePool.frames[loopVar] = 0;
		/* Calculate physical location of frame starting address *
		 * Sits beneath the kernel and test stack frames         */
		frameLoc = ramtop - (3 + loopVar) * PAGESIZE;
		framePool.frameAddr[loopVar] = frameLoc;
	}


	pager = 1; /* Init mutex for swap pool pager */
	/* Init device mutex semaphore array; set each to 1 */
	for(loopVar = 0; loopVar < MAXSEMS; loopVar++)
		devSemList[loopVar] = 1;

	masterSem = 0; /* For graceful halting of OS after test fin */

	/* Set up user processes */
	for(asid = 1; asid <= MAXUPROC; asid++) {
		/* Set up new page table for process */
		uPgTblList[asid - 1].magicPtHeaderWord = MAGICNUM;

		setSegmentTableEntry(KUSEG2, asid, &(uPgTblList[asid - 1]));

		/* Fill in default entries */
		for(loopVar = 0; loopVar < MAXPTENTRIES; loopVar++) {
			newPTEntry = &(uPgTblList[asid - 1].entries[loopVar]);

			newPTEntry->entryHI = (KUSEG2 << 30)
				| (KUSEG2VPN << 12)
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

/*
 * tlbHandler -
 *
 *
 */
void tlbHandler() {
	/* Determine cause */
	oldTlb = (state_PTR) TLBOLDAREA;
	cause = oldTlb->s_cause;
	cause = (cause << shiftBitsForCauseOffset) & someCauseMask;

	/* Who am I? */
	asid = oldTlb->s_asid;

	if(cause != PAGELOADMISS && cause != PAGESTRMISS)
		/* Enfore zero-tolerance policy rules, only handle missing page */
		SYSCALL(TERMINATEPROCESS);

	/* Gain mutal exclusion of pager to avoid inconsistent states */
	SYSCALL(PASSEREN, &pager);

	newFrameNumber = selectFrameIndex();
	curFPEntry = framePool.frames[newFrameNumber];
	newFrameAddr = framePool.frameAddr[newFrameNumber];

	/* Find PTE for the to-be-overwritten page in order
	 * to determine if its a dirty page */
	curPageTable = getSegmentTableEntry(curFPEntry >> 30, getASID(curFPEntry));
	curPage = findPageTableEntry(curPageTable, curFPEntry & VPNMASK); /* TODO: make this actually find entry */

	/* TODO: Consider keeping isDirty info in framepool entries... */
	if(isDirty(curPage)) { /* Then write page to backing store */
		/* Get info for next steps from Frame Pool */
		oldFramePgTbl = curFPEntry->fp_pgTableAddr;

		/* Clear out frame (?) cache to avoid inconsistencies */
		/* TODO Better: Update page table entry as invalid to mean missing */
		nukePageTable(oldFramePgTbl);

		/* Write frame back to backing store */
		storageAddr = calcBkgStoreAddr(curFPEntry->fp_asid, someUnknownPageOffset);
		writePageToBackingStore(newFrame, storageAddr);
	}

	/* Regardless of dirty frame, load in new page from BStore */
	storageAddr = calcBkgStoreAddr(asid, someUnknownPageOffset);
	readPageFromBackingStore(storageAddr, newFrameAddr);

	/* Resynch TLB Cache */
	TLBCLEAR();

	/* Update relevant page table entry */
	/* TODO: Q: is this a different page table? Why? Does it belong to this new process? */
	newPTEntry = uProcList[asid - 1]->up_pgTable[someIndexLikeTheLastEntryMaybe];
	/* newPTEntry = findPTEntryAddr(pageTableAddr, virtualPageNumber/frameAddr?) */
	newPTEntry->entryLO |= VALID;

	/* End mutal exclusion of TLB Handling */
	SYSCALL(VERHOGEN, &pager);
	loadState(oldTlb);
}


/********************** Helper Methods *********************/
int calcBkgStoreAddr(int asid, int pageOffset) {
	int pageIndex;
	return pageIndex = (asid * MAXPAGES) + pageOffset;
}

HIDDEN ptEntry_PTR findPageTableEntry(uPgTbl_PTR pageTable, int VPN) {
	int indexOfMatch = 0;
	/* TODO: loop through page table entries to find index */
	return pageTable->entries[indexOfMatch];
}

/* TODO: modify original findSem method if indeed 1:1
 * findMutex - Calculates address of device semaphore, specifically mutex
 * PARAM: int lineNum is the device type, correlates with the interrupt lineNum
 *        int deviceNum is the index of a device within a type group
 *        Bool isReadTerm is FALSE for writeT & non-terminals, TRUE for readT
 * RETURN: int* calculated address of device semaphore
 */
int* findMutex(int lineNum, int deviceNum, Bool isReadTerm) {
	int termOffset = (isReadTerm) ? 1 : 0;
	int semGroup = (lineNum - LINENUMOFFSET) + termOffset;
	return &(mutexSems[semGroup * DEVPERINT + deviceNum]);
}

int getSegmentTableEntry(int segment, int asid) {
	return segTable[segment][asid];
}

Bool isDirty(ptEntry_PTR pageDesc) {
	return TRUE; /* (*pageDesc & DIRTY); */
}

void readPageFromBackingStore(int sectIndex, memaddr destFrameAddr) {
	engageDiskDevice(sectIndex, destFrameAddr, READBLK);
}

void engageDiskDevice(int sectIndex, memaddr addr, int readOrWrite) {
	int head, sect, cyl, maxHeads, maxSects, maxCyls;
	devregtr* diskDev = ((devregarea_t*) RAMBASEADDR)->devreg[DEVINTNUM * DISKINT];

	int sectMask = 0x000000FF;
	int headMask = 0x0000FF00;
	int cylMask = 0xFFFF0000;

	SYSCALL(PASSEREN, findMutex(DISKINT, 0, FALSE));

	maxSects = (diskDev->d_data1 & sectMask) >> 0;
	maxHeads = (diskDev->d_data1 & headMask) >> 8;
	maxCyls = (diskDev->d_data1 & cylMask) >> 16;

	/* Calc cylinder */
	cyl = sectIndex / (maxHeads * maxSects);
	if(cyl >= maxCyls) { SYSCALL(TERMINATEPROCESS); }

	/* Move boom to the correct disk cylinder */
 	desDev->d_command = cyl << 8 | SEEKCYL;
	status = SYSCALL(WAITIO, DISKINT, 0, 0);
	if(status != ACK) { SYSCALL(TERMINATEPROCESS); }

	/* Calc position within cylinder */
	head = (sectIndex / maxSects) % maxHeads;
	sect = sectIndex % maxSects;

	/* Position and Prepare device */
	diskDev->d_command = head << 16 | sect << 8 | readOrWrite;
	diskDev->d_data0 = destFrameAddr; /* Move destFrameAddr into DATA0 */

	status = SYSCALL(WAITIO, DISKINT, 0, 0); /* Wait for job to complete */
	if(status != ACK)
		SYSCALL(TERMINATEPROCESS);

	SYSCALL(VERHOGEN, findMutex(DISKINT, 0, FALSE));
}

int selectFrameIndex() {
	framePool.indexOfLastFrameReplaced += 1;
	framePool.indexOfLastFrameReplaced %= MAXFRAMES;
	return (framePool.indexOfLastFrameReplaced);
}

void setSegmentTableEntry(int segment, int asid, uPgTable_PTR addr) {
	/* First, boundary check segment and asid, but not address */
	/*	TLB exception already covers bad address to some extent */
	if(segment < 0 || segment > 3)
		gameOver(99);

	if(asid < 0 || asid > 63)
		gameOver(99);

	/* Is it actually a page table? */
	if((addr->magicPtHeaderWord & MAGICNUMMASK) != MAGICNUM)
		gameOver(99); /* Magic is a lie */

	/* Overwrite current entry of segTable with address of page table */
	segTable[segment][asid] = (int) addr;
}

void setSegmentTableEntry(int segment, int asid, osPgTable_PTR addr) {
	setSegmentTableEntry(segment, asid, (uPgTable_PTR) addr);
}

void writePageToBackingStore(memaddr srcFrameAddr, int sectIndex) {
	engageDiskDevice(sectIndex, srcFrameAddr, WRITBLK);
}
