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

/************************ Prototypes ***********************/




/********************* External Methods ********************/
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
	asid = (oldTlb->s_asid & ASIDMASK) >> 6;
	segNum = (oldTlb->s_asid & SEGMASK) >> 30;
	/* We store "page number" in the offset from the relevant VPN start */
	vpn = (oldTlb->s_asid & VPNMASK) >> 12;

	destPgTbl = getSegmentTableEntry(segNum, asid);
	destPTEIndex = findPTEntryIndex(destPgTbl, vpn);

	if(cause != PAGELOADMISS && cause != PAGESTRMISS)
		/* Enfore zero-tolerance policy rules, only handle missing page */
		SYSCALL(TERMINATEPROCESS);

	/* Gain mutal exclusion of pager to avoid inconsistent states */
	SYSCALL(PASSEREN, &pager);

	/* Check shared table to see if it's already been brought in */
	if(segNum == KUSEG3 && ((destPageEntry->entryLO & VALID) == TRUE)) {
		SYSCALL(VERHOGEN, &pager);
		loadState(oldTlb);
	}

	newFrameNum = selectFrameIndex();
	curFPEntry = framePool.frames[newFrameNum];
	newFrameAddr = framePool.frameAddr[newFrameNum];

	if(frameInUse(curFPEntry)) {
		/* Find PTE for the to-be-overwritten page in order
		 * to determine if its a dirty page */
		curPageTable = getSegmentTableEntry(
			(curFPEntry & SEGMASK) >> 30,
			(curFPEntry & ASIDMASK) >> 6);

		curPageIndex = findPageTableEntry(curPageTable,
			(curFPEntry & VPNMASK) >> 12);

		curPage = curPageTable->entries[curPageIndex];
		curPage &= ~VALID; /* Invalidate curPage to mean "missing" */

		/* TODO: Consider keeping isDirty info in framepool entries... */
		if(isDirty(curPage)) { /* Then write page to backing store */
			/* Clear out TLB to avoid inconsistencies */
			/* TODO Better: Overwrite cached page table entry */
			TLBCLEAR();

			/* Write frame back to backing store */
			/* TODO: Find where page offset could be found, 1:1 VPN and pageNum?? */
			storageAddr = calcBkgStoreAddr(curFPEntry->fp_asid, curPageIndex);
			writePageToBackingStore(newFrame, storageAddr);
		}
	}

	/* Regardless of dirty frame, load in new page from BStore */
	storageAddr = calcBkgStoreAddr(asid, destPTEIndex);
	readPageFromBackingStore(storageAddr, newFrameAddr);

	/* Update Frame Pool to match new Page Entry */
	framePool.frames[newFrameNum] = destPageEntry->entryHI + 1; /* +1 for "in use" */

	/* Resynch TLB Cache */
	TLBCLEAR();

	/* Update relevant page table entry */
	newPTEntry->entryLO &= ~PFNMASK /* Erase current PFN */
	newPTEntry->entryLO |= (newFrameAddr / PAGESIZE) | VALID;

	/* End mutal exclusion of TLB Handling */
	SYSCALL(VERHOGEN, &pager);
	loadState(oldTlb);
}


/*
 * sysCallHandler handles SYS9 - SYS18
 *
 */
void sysCallHandler() {
	unsigned int asid = getASID(getENTRYHI());
	state_PTR oldSys = &(uProcList[asid - 1].up_stateAreas[OLD][SYSTRAP]);

	/* check Cause.ExcCode in uProc's SYS/BP Old area for SYS/BP */
	if(((oldSys->s_cause & 127) >> 2) == _BP_)
		??

	/* check for invalid SYSCALL */
	/* if invalid, avadaKedavra */
	if(oldSys <= 0 || oldSys > 18)
		sys18_terminate();

	/* switch cases for each sysCall */
	switch(oldSys->s_a0) {
		case 9:
			int termNo = asid - 1;
			char *addr = oldSys->s_a1;
			oldSys->s_v0 = sys9_readFromTerminal(termNo, addr);

		case 10:
			char *virtAddr = oldSys->s_a1;
			int len = oldSys->s_a2;
			oldSys->s_v0 = sys10_writeToTerminal(virtAddr, len);

		case 11:
			int *semaddr = oldSys->s_a1;
			sys11_vVirtSem(semaddr);

		case 12:
			semaddr = oldSys->s_a1;
			sys12_pVirtSem(semaddr);

		case 13:
			int secondsToDelay = oldSys->s_a1;
			sys13_delay(asid, secondsToDelay);

		case 14:
			int *blockAddr = oldSys->s_a1;
			int diskNo = oldSys->s_a2;
			int sectNo = oldSys->s_a3;
			oldSys->s_v0 = sys14_diskPut(blockAddr, diskNo, sectNo);

		case 15:
			blockAddr = oldSys->s_a1;
			diskNo = oldSys->s_a2;
			sectNo = oldSys->s_a3;
			oldSys->s_v0 = sys15_diskGet(blockAddr, diskNo,sectNo);

		case 16:
			virtAddr = oldSys->s_a1;
			len = oldSys->s_a2;
			oldSys->s_v0 = sys16_writeToPrinter(virtAddr, len);

		case 17:
			oldSys->s_v0 = sys17_getTOD();

		case 18:
			sys18_terminate();

		default: /* Anything else */
			sys18_terminate();
	}

	loadState(oldSys);
}

/********************** Helper Methods *********************/
HIDDEN int sys9_readFromTerminal(int termNo, char *addr) {
	int status;
	int charReadCount = 0;
	int semAddr = findMutex(TERMINT, termNo, TRUE);
	Bool isReading = TRUE;

	devregtr *termReg = ((devregarea_t*) RAMBASEADDR)->devreg[DEVINTNUM * TERMINT + termNo];

	/* Check boundary conditions, no OS access */
	if(addr < KUSEG2START)
		sys18_terminate();

	sys12_pVirtSem(semAddr); /* Gain mutual exclusion of term device */
	while(isReading) {
		/* Prepare terminal and wait for Operation */
		disableInterrupts();
		termReg->t_recv_command = RECEIVECHAR;
		status = SYSCALL(WAITIO, TERMINT, termNo, TRUE);
		enableInterrupts();

		/* Respond to results */
		rChar = (status & 0xFF00);
		status = (status & 0xFF);

		if(status == RECVD) {
			*(addr + charReadCount) = rChar;
			charReadCount++;

		} else {
			isReading = FALSE;
		}
	}

	sys11_vVirtSem(semAddr)	/* Release exclusion */

	if(charReadCount > 0) /* Return successfully */
		return charReadCount;
	else
		return (-1 * status); /* Return failure */
}

HIDDEN int sys10_writeToTerminal(char *virtAddr, int len) {

}

HIDDEN void sys11_vVirtSem(int *semaddr) {

}

HIDDEN void sys12_pVirtSem(int *semaddr) {

}

HIDDEN void sys13_delay(int asid, int secondsToDelay) {
	cpu_t startTime;
	STCK(startTime);
	alarmTime = startTime + secondsToDelay;

	if(alarmTime <= 0) /* Invalid operation, "delay" forever */
		sys18_terminate();

	setAlarm(asid, alarmTime);
	sys12_pVirtSem(&(uProcList[asid - 1]->up_syncSem));
}

/*
 * sys14_diskPut - write to disk
 */
HIDDEN int sys14_diskPut(int *blockAddr, int diskNo, int sectNo) {
	int *bufferAddr = DISKBUFFERSTART;
	int *semAddr = findMutex(DISKINT, diskNo, FALSE);

	/* check for invalid blockAddr. if addr is in ksegOS, terminate */
	/* TODO: check for diskNo, if writing to disk0, terminate */
	if((bufferAddr <= KSEGOSEND) || disk0 == 0)
		sys18_terminate();

	sys12_pVirtSem(semAddr);
	/* Transfer data from virtual address into physically located buffer */
	for(wordIndex = 0; wordIndex < PAGESIZE; wordIndex++) {
		*(blockAddr + wordIndex) = *(bufferAddr + wordIndex);
	}

	engageDiskDevice(diskNo, sectNo, bufferAddr, WRITE);
	sys11_vVirtSem(semAddr);
}

/*
 * sys15_diskGet - read from disk
 */
HIDDEN int sys15_diskGet(int *blockAddr, int diskNo, int sectNo) {
	int *bufferAddr = DISKBUFFERSTART;
	int *semAddr = findMutex(DISKINT, diskNo, FALSE);

	if(blockAddr < KUSEG2START || diskNo == 0)
		sys18_terminate(); /* illegal access, kill process */

	sys12_pVirtSem(semAddr);
	engageDiskDevice(diskNo, sectNo, bufferAddr, READ);

	/* Transfer data from physically located buffer into virtual address */
	for(wordIndex = 0; wordIndex < PAGESIZE; wordIndex++) {
		*(blockAddr + wordIndex) = *(bufferAddr + wordIndex);
	}
	sys11_vVirtSem(semAddr);
}

HIDDEN int sys16_writeToPrinter(char *virtAddr, int len) {

}

HIDDEN unsigned int sys17_getTOD() {

}

HIDDEN void sys18_terminate() {

}

int calcBkgStoreAddr(int asid, int pageOffset) {
	int pageIndex;
	return pageIndex = (asid * MAXPAGES) + pageOffset;
}

HIDDEN int findPTEntryIndex(uPgTbl_PTR pageTable, int vpn) {
	Bool isAMatch;
	int vpnToMatch;
	int loopVar = 0;
	int indexOfMatch = NULL; /* If not found, this indicates error condition */
	int numEntries = pageTable->magicPtHeaderWord & ENTRYCNTMASK;

	while(loopVar < numEntries && indexOfMatch == NULL) {
		vpnToMatch = ((pageTable->entries[loopVar] & VPNMASK) >> 12);
		isAMatch = (vpn == vpnToMatch);

		indexOfMatch = (isAMatch) ? loopVar : NULL;
		loopVar++;
	}

	return indexOfMatch;
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

/*
 * getSegmentTableEntry - accessor to retrieve the os or user page table ptr
 *   kept in the segment table.
 * Note! User and OS pgTbls are treated as User pg tables
 *   because doing otherwise has little practical advantages
 *   and would complicate otherwise simple logic
 * PARAM: int segment to look in, int asid to choose a process
 * RETURN: ~uPgTbl_PTR to a USER OR OS page table
 */
uPgTbl_PTR getSegmentTableEntry(int segment, int asid) {
	if(segment == KSEGOS)
		return segTable->kSegOS[asid];
	else if (segment == KUSEG2)
		return segTable->KUSEG2[asid];
	else if (segment == KUSEG3)
		return segTable->KUSEG3[asid];
	else
		return NULL; /* This is an error condition */
}

Bool isDirty(ptEntry_PTR pageDesc) {
	return TRUE; /* (*pageDesc & DIRTY); */
}

void nukePageTable(uPgTbl_PTR pageTable) {
	int loopVar, entries;
	/* Is this a page table or small island city? */
	if(pageTable.magicPtHeaderWord & MAGICNUMMASK != MAGICNUM)
		SYSCALL(TERMINATEPROCESS);

	entries = pageTable.magicPtHeaderWord & ENTRYCNTMASK;
	for(loopVar = 0; loopVar < entries; loopVar++) {
		/* TODO: Check if NULL is an appropriate value, is 0 better? */
		pageTable.entries[loopVar]->entryHI = NULL;
		pageTable.entries[loopVar]->entryLO = NULL;
	}

	/* Reset Header Word Entry Count */
	pageTable.magicPtHeaderWord = MAGICNUM;
}

void readPageFromBackingStore(int sectIndex, memaddr destFrameAddr) {
	int *semAddr = findMutex(DISKINT, 0, FALSE);

	sys12_pVirtSem(semAddr);
	engageDiskDevice(0, sectIndex, destFrameAddr, READ);
	sys11_vVirtSem(semAddr);
}

unsigned int engageDiskDevice(int diskNo, int sectIndex, memaddr addr, int readOrWrite) {
	int head, sect, cyl, maxHeads, maxSects, maxCyls;
	devregtr* diskDev = ((devregarea_t*) RAMBASEADDR)->devreg[DEVINTNUM * DISKINT];

	int sectMask = 0x000000FF;
	int headMask = 0x0000FF00;
	int cylMask = 0xFFFF0000;

	maxSects = (diskDev->d_data1 & sectMask) >> 0;
	maxHeads = (diskDev->d_data1 & headMask) >> 8;
	maxCyls = (diskDev->d_data1 & cylMask) >> 16;

	/* Calc cylinder */
	cyl = sectIndex / (maxHeads * maxSects);

	/* TODO: disable interrupts?? */
	if(cyl >= maxCyls) { SYSCALL(TERMINATEPROCESS); }

	/* Move boom to the correct disk cylinder */
 	desDev->d_command = cyl << 8 | SEEKCYL;
	status = SYSCALL(WAITIO, DISKINT, diskNo, FALSE);

	if(status != ACK) { SYSCALL(TERMINATEPROCESS); }

	/* Calc position within cylinder */
	head = (sectIndex / maxSects) % maxHeads;
	sect = sectIndex % maxSects;

	/* Prepare and Engage device */
	diskDev->d_command = head << 16 | sect << 8 | readOrWrite;
	diskDev->d_data0 = destFrameAddr; /* Move destFrameAddr into DATA0 */

	status = SYSCALL(WAITIO, DISKINT, diskNo, FALSE); /* Wait for job to complete */
	if(status != ACK)
		SYSCALL(TERMINATEPROCESS);

	return status;
}

int selectFrameIndex() {
	framePool.indexOfLastFrameReplaced += 1;
	framePool.indexOfLastFrameReplaced %= MAXFRAMES;
	return (framePool.indexOfLastFrameReplaced);
}

/* TODO: Remove the setSegmentTableEntry abstractions for lack of use */
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
	if(segment == KSEGOS)
		segTable->kSegOS[asid] = (osPgTable_PTR) addr;
	else if (segment == KUSEG2)
		segTable->KUSEG2[asid] = addr;
	else /* (segment == KUSEG3) */
		segTable->KUSEG3[asid] = addr;
}

void setSegmentTableEntry(int segment, int asid, osPgTable_PTR addr) {
	setSegmentTableEntry(segment, asid, (uPgTable_PTR) addr);
}

void writePageToBackingStore(memaddr srcFrameAddr, int sectIndex) {
	int *semAddr = findMutex(DISKINT, 0, FALSE);

	sys12_pVirtSem(semAddr);
	engageDiskDevice(0, sectIndex, srcFrameAddr, WRITE);
	sys11_vVirtSem(semAddr);
}
