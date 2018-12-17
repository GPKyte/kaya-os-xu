/********************** VMIOSUPPORT.C **********************
 * vmIOsupport module implements the VM-I/O support level
 * SYS/Bp and PgmTrap exception handlers, which are
 * SYSCALLs 9-18.
 *
 * AUTHORS: Ploy Sithisakulrat & Gavin Kyte
 * ADVISOR/CONTRIBUTER: Michael Goldweber
 * DATE PUBLISHED:
 ***********************************************************/

#include "../h/types.h"
#include "../h/const.h"
#include "../e/initProc.e"

#include "/usr/local/include/umps2/umps/libumps.e"

/************************ Prototypes ***********************/

HIDDEN int sys9_readFromTerminal(int termNo, char *addr);
HIDDEN int sys10_writeToTerminal(int termNo, char *virtAddr, int len);
HIDDEN void sys11_vVirtSem(int *semAddr);
HIDDEN void sys12_pVirtSem(int *semAddr);
HIDDEN void sys13_delay(int asid, int secondsToDelay);
HIDDEN int sys14_diskPut(memaddr blockAddr, int diskNo, int sectNo);
HIDDEN int sys15_diskGet(memaddr blockAddr, int diskNo, int sectNo);
HIDDEN int sys16_writeToPrinter(int prntNo, char *virtAddr, int len);
HIDDEN cpu_t sys17_getTOD(void);
HIDDEN void sys18_terminate(int asid);
int calcBkgStoreAddr(int asid, int pageOffset);
void disableInterrupts(void);
void enableInterrupts(void);
HIDDEN int findPTEntryIndex(uPgTable_PTR pageTable, int vpn);
int* findMutex(int lineNum, int deviceNum, Bool isReadTerm);
Bool frameInUse(uint frameDescriptor);
uPgTable_PTR getSegmentTableEntry(int segment, int asid);
Bool isDirty(ptEntry_PTR pageDesc);
void nukePageTable(uPgTable_PTR pageTable);
void readPageFromBackingStore(int sectIndex, memaddr destFrameAddr);
int selectFrameIndex(void);
uint engageDiskDevice(int diskNo, int sectIndex, memaddr addr, int readOrWrite);
void invalidate(ptEntry_PTR pte);
void writePageToBackingStore(memaddr srcFrameAddr, int sectIndex);

/********************* External Methods ********************/
/*
 * uPgrmTrapHandler - handles user-level program trap handler,
 * in which users should not be dealing with, so terminate!
 */
void uPgrmTrapHandler(void) {
	sys18_terminate(getASID()); /* No faulty programs here. Off with their head! */
}

/*
 * uTlbHandler - handles the user-level TLB exception handler.
 */
void uTlbHandler() {
	int storageAddr, asid, segNum, vpn, destPTEIndex, newFrameNum, newFrameAddr;
	int curPageIndex;
	uint cause, *curFPEntry,
	uPgTable_PTR destPgTbl;
	ptEntry_PTR destPageEntry, curPage;

	/* Determine cause */
	state_PTR oldTlb = (state_PTR) TLBOLDAREA;

	cause = oldTlb->s_cause;
	cause = (cause >> 2) & 0x1F; /* Get ExecCode */

	/* Who am I? */
	asid = (oldTlb->s_asid & ASIDMASK) >> 6;
	segNum = (oldTlb->s_asid & SEGMASK) >> 30;
	/* We store "page number" in the offset from the relevant VPN start */
	vpn = (oldTlb->s_asid & VPNMASK) >> 12;

	destPgTbl = getSegmentTableEntry(segNum, asid);
	destPTEIndex = findPTEntryIndex(destPgTbl, vpn);
	destPageEntry = &(destPgTbl->entries[destPTEIndex]);

	if(cause != PAGELOADMISS && cause != PAGESTRMISS)
		/* Enfore zero-tolerance policy rules, only handle missing page */
		sys18_terminate(asid);

	/* Gain mutal exclusion of pager to avoid inconsistent states */
	sys12_pVirtSem(&pager);

	/* Check shared table to see if it's already been brought in */
	if(segNum == KUSEG3 && ((destPageEntry->entryLO & VALID) == TRUE)) {
		sys11_vVirtSem(&pager);
		contextSwitch(oldTlb);
	}

	newFrameNum = selectFrameIndex();
	curFPEntry = &(framePool.frames[newFrameNum]);
	newFrameAddr = framePool.frameAddr[newFrameNum];

	if(frameInUse((int) curFPEntry)) {
		/* Find PTE for the to-be-overwritten page in order
		 * to determine if its a dirty page/

		/ * Note: this may either be an OS or User page table */
		uPgTable_PTR curPageTable = getSegmentTableEntry(
			((*curFPEntry & SEGMASK) >> 30),
			((*curFPEntry & ASIDMASK) >> 6));

		curPageIndex = findPTEntryIndex(curPageTable,
			(*curFPEntry & VPNMASK) >> 12);

		curPage = &(curPageTable->entries[curPageIndex]);
		invalidate(curPage); /* Invalidate curPage to mean "missing" */

		if(isDirty(curPage)) { /* Then write page to backing store */
			/* Clear out TLB to avoid inconsistencies */
			/* TODO Better: Overwrite cached page table entry */
			TLBCLR();

			/* Write frame back to backing store */
			/* TODO: Find where page offset could be found, 1:1 VPN and pageNum?? */
			storageAddr = calcBkgStoreAddr((*curFPEntry & ASIDMASK) >> 6, curPageIndex);
			writePageToBackingStore(newFrameAddr, storageAddr);
		}
	}

	/* Regardless of dirty frame, load in new page from BStore */
	storageAddr = calcBkgStoreAddr(asid, destPTEIndex);
	readPageFromBackingStore(storageAddr, newFrameAddr);

	/* Update Frame Pool to match new Page Entry */
	framePool.frames[newFrameNum] = destPageEntry->entryHI + 1; /* +1 for "in use" */

	/* Resynch TLB Cache */
	TLBCLR();

	/* Update relevant page table entry */
	destPageEntry->entryLO &= ~PFNMASK; /* Erase current PFN */
	destPageEntry->entryLO |= newFrameAddr | VALID; /* newFrameAddr is like PFN << 12 (or * 4096) */

	/* End mutal exclusion of TLB Handling */
	sys11_vVirtSem(&pager);
	contextSwitch(oldTlb);
}


/*
 * uSysCallHandler - handles SYS9 - SYS18 when requested
 */
void uSysCallHandler() {
	int *semAddr, termNo, len, secondsToDelay, diskNo, sectNo, prntNo;
	char *charAddr, *virtCharAddr;
	memaddr blockAddr;
	uint asid = getASID();
	state_PTR oldSys = &(uProcList[asid - 1].up_stateAreas[OLD][SYSTRAP]);

	/* check Cause.ExcCode in uProc's SYS/BP Old area for SYS/BP
	if(((oldSys->s_cause & 127) >> 2) == _BP_)
		??*/

	/* check for invalid SYSCALL */
	/* if invalid, avadaKedavra */
	if(oldSys->s_a0 < 9 || (int) oldSys->s_a0 > 18)
		sys18_terminate(asid);

	/* switch cases for each sysCall */
	switch(oldSys->s_a0) {
		case 9:
			termNo = asid - 1;
			charAddr = (char*) oldSys->s_a1;
			oldSys->s_v0 = sys9_readFromTerminal(termNo, charAddr);

		case 10:
			termNo = asid - 1;
			virtCharAddr = (char*) oldSys->s_a1;
			len = oldSys->s_a2;
			oldSys->s_v0 = sys10_writeToTerminal(termNo, virtCharAddr, len);

		case 11:
			semAddr = (int*) oldSys->s_a1;
			sys11_vVirtSem(semAddr);

		case 12:
			semAddr = (int*) oldSys->s_a1;
			sys12_pVirtSem(semAddr);

		case 13:
			secondsToDelay = oldSys->s_a1;
			sys13_delay(asid, secondsToDelay);

		case 14:
			blockAddr = (memaddr) oldSys->s_a1;
			diskNo = oldSys->s_a2;
			sectNo = oldSys->s_a3;
			oldSys->s_v0 = sys14_diskPut(blockAddr, diskNo, sectNo);

		case 15:
			blockAddr = (memaddr) oldSys->s_a1;
			diskNo = oldSys->s_a2;
			sectNo = oldSys->s_a3;
			oldSys->s_v0 = sys15_diskGet(blockAddr, diskNo, sectNo);

		case 16:
			prntNo = asid - 1;
			virtCharAddr = (char*) oldSys->s_a1;
			len = oldSys->s_a2;
			oldSys->s_v0 = sys16_writeToPrinter(prntNo, virtCharAddr, len);

		case 17:
			oldSys->s_v0 = sys17_getTOD();

		case 18:
			sys18_terminate(asid);

		default: /* Anything else */
			sys18_terminate(asid);
	}

	contextSwitch(oldSys);
}

/********************** Helper Methods *********************/
/*
 * sys9_readFromTerminal - when requested, this service causes
 * the requesting U-proc to be suspended until a line of input
 * (string of chars) has been transmitted from the terminal
 * device associated with the U-Proc.
 *
 * PARAM: termNo - terminal number being read from
 *				*addr - a pointer to string of char address
 * RETURN: the number of characters actually transmitted to
 *					the terminal device, if read successfully.
 *					If not, return the negative of devic's statis value
 */
HIDDEN int sys9_readFromTerminal(int termNo, char *addr) {
	int status;
	int charReadCount = 0;
	int *semAddr = findMutex(TERMINT, termNo, TRUE);
	char rChar;
	Bool isReading = TRUE;

	device_t *termReg = &(((devregarea_t*) RAMBASEADDR)->devreg[DEVINTNUM * TERMINT + termNo]);

	/* Check boundary conditions, no OS access */
	if((int) addr < KUSEG2START)
		sys18_terminate(getASID());

	sys12_pVirtSem(semAddr); /* Gain mutual exclusion of term device */
	while(isReading) {
		/* Prepare terminal and wait for Operation */
		disableInterrupts();
		termReg->t_recv_command = RECEIVECMD;

		status = SYSCALL(WAITIO, TERMINT, termNo, TRUE);
		enableInterrupts();

		/* Respond to results */
		rChar = (status & 0xFF00);
		status = (status & 0xFF);

		if(status == CRECVD && rChar != EOL) {
			*(addr + charReadCount) = rChar;
			charReadCount++;

		} else {
			isReading = FALSE;
		}
	}

	sys11_vVirtSem(semAddr);	/* Release exclusion */

	if(status == CRECVD && charReadCount > 0) /* Return successfully */
		return charReadCount;
	else
		return (-1 * status); /* Return failure */
}

/*
 * sys10_writeToTerminal - when requested, this service causes
 * the requesting U-Proc to be suspended until a line of output
 * (string of chars) has been transmitted to the terminal device
 * associated with the U-Proc
 *
 * PARAM: termNo - terminal number bein written to
 *				*virtAddr - a pointer to string of char address
 * RETURN: the number of character written to the terminal device,
 *					if write sucessfully. If not, return the negative
 *					of the device's status value
 */
HIDDEN int sys10_writeToTerminal(int termNo, char *virtAddr, int len) {
	int status, *semAddr;
	int writeCount = 0;
	char tChar;
	Bool noFailure = TRUE;
	device_t *termReg;

	semAddr = findMutex(TERMINT, termNo, FALSE);
	termReg = &(((devregarea_t*) RAMBASEADDR)->devreg[DEVINTNUM * TERMINT + termNo]);

	/* Check boundary conditions, no OS access */
	if((int) virtAddr < KUSEG2START || len < 0 || len > 128)
		sys18_terminate(getASID());

	sys12_pVirtSem(semAddr); /* Gain mutual exclusion of term device */
	while(noFailure && writeCount < len) {
		/* Prepare terminal and wait for Operation */
		disableInterrupts();
		tChar = *(virtAddr + writeCount);
		termReg->t_transm_command = (tChar << 8 | TRANCMD);
		status = SYSCALL(WAITIO, TERMINT, termNo, FALSE);
		enableInterrupts();

		/* React to results */
		if(((status & 0xFF) == CTRANSD) && ((status & 0xFF00) == tChar)) {
			writeCount++;

		} else {
			noFailure = FALSE;
		}
	}

	sys11_vVirtSem(semAddr);	/* Release exclusion */

	if(noFailure) /* Return count of successfully written characters */
		return writeCount;
	else
		return (-1 * status); /* Return failure code */
}

/*
 * sys11_vVirtSem - when this service is requested, it is
 * interpreted by the nucleus as a request to perform a V op
 * on a virtual semaphore.
 *
 * PARAM: *semAddr - a pointer to a semaphore address
 *					to be signaled
 */
HIDDEN void sys11_vVirtSem(int *semAddr) {

}

/*
 * sys12_pVirtSem - when this service is requested, it is
 * interpreted by the nuclus as a request to perform a P op
 * on a virtual semaphore.
 *
 * PARAM: *semAddr - a pointer to a semaphore address
 *					to be put on wait
 */
HIDDEN void sys12_pVirtSem(int *semAddr) {

}

/*
 * sys13_delay - causes the executing U-Proc to be delayed
 * for at least n seconds and not substantially longer.
 * Since the nucleus controls low-level scheduling devicions,
 * all the VM-I/O support level can ensure is that the
 * requesting U-Proc not be "sechdulable" until n seconds
 * has elapsed and that it becomes schedulable shortly thereafter
 *
 * PARAM: asid - id of the requesting U-Proc
 *				secondsToDelay - an integer of senconds to delay
 *						the executing U-Proc
 */
HIDDEN void sys13_delay(int asid, int secondsToDelay) {
	cpu_t startTime, alarmTime;
	STCK(startTime);

	alarmTime = startTime + secondsToDelay;
	if(alarmTime <= 0) /* Invalid operation, "delay" forever */
		sys18_terminate(asid);

	/* TODO: setAlarm(asid, alarmTime); */
	sys12_pVirtSem(&(uProcList[asid - 1].up_syncSem));
}

/*
 * sys14_diskPut - the requesting U-Proc is suspended until
 * the disk write operation has ended
 *
 * PARAM: *blockAddr - a pointer to a block address being written to
 *				diskNo - disk number being written to
 *				sectNo - sector number being written to
 * RETURN: if write successfully, returns a completion status.
 *				If not, return the negative of the completion status
 */
HIDDEN int sys14_diskPut(memaddr blockAddr, int diskNo, int sectNo) {
	uint wordIndex;
	memaddr bufferAddr = (memaddr) DISKBUFFERSTART;
	int *semAddr = findMutex(DISKINT, diskNo, FALSE);

	/* check for invalid blockAddr. if addr is in ksegOS, terminate */
	/* TODO: check for diskNo, if writing to disk0, terminate */
	if(((int) bufferAddr <= KSEGOSEND) || diskNo == 0)
		sys18_terminate(getASID());

	sys12_pVirtSem(semAddr);
	/* Transfer data from virtual address into physically located buffer */
	for(wordIndex = 0; wordIndex < PAGESIZE; wordIndex++) {
		*(int*)(bufferAddr + wordIndex) = *(int*)(blockAddr + wordIndex);
	}

	engageDiskDevice(diskNo, sectNo, bufferAddr, WRITE);
	sys11_vVirtSem(semAddr);
}

/*
 * sys15_diskGet - the requesting U-Proc is suspended until
 * the disk read operation has ended
 *
 * PARAM: *blockAddr - a pointer to a block address being read from
 *				diskNo - disk number being read from
 *				sectNo - sector number being read from
 * RETURN: if read successfully, returns a completion status.
 *				If not, return the negative of the completion status
 */
HIDDEN int sys15_diskGet(memaddr blockAddr, int diskNo, int sectNo) {
	uint wordIndex;
	memaddr bufferAddr = (memaddr) DISKBUFFERSTART;
	int *semAddr = findMutex(DISKINT, diskNo, FALSE);

	if((int) blockAddr < KUSEG2START || diskNo == 0)
		sys18_terminate(getASID()); /* illegal access, kill process */

	sys12_pVirtSem(semAddr);
	engageDiskDevice(diskNo, sectNo, bufferAddr, READ);

	/* Transfer data from physically located buffer into virtual address */
	for(wordIndex = 0; wordIndex < PAGESIZE; wordIndex++) {
		*(int*)(blockAddr + wordIndex) = *(int*)(bufferAddr + wordIndex);
	}
	sys11_vVirtSem(semAddr);
}

/*
 * sys16_writeToPrinter - the requesting U-Proc is suspended until
 * a line of output (string of char) has been written to the printer
 * device associated with the U-Proc successfully.
 *
 * PARAM: prntNo - printer device number
 *				*virtAddr - a pointer to virtual address
 *				len - length of the string of characters
 * RETURN: If write successfully, return the number of characters
 *					actually being written to the printer device.
 *					It not, return the negative of the device's status value
 */
HIDDEN int sys16_writeToPrinter(int prntNo, char *virtAddr, int len) {
	int status, prntCount;
	int prntChar = 0;
	int *semAddr = findMutex(PRNTINT, prntNo, FALSE);
	Bool noFailure = TRUE;

	device_t *prntReg = &(((devregarea_t*) RAMBASEADDR)->devreg[DEVINTNUM * PRNTINT + prntNo]);

	/* Check boundary conditions, no OS access */
	if((int) virtAddr < KUSEG2START || len < 0 || len > 128)
		sys18_terminate(getASID());

	sys12_pVirtSem(semAddr); /* Gain mutual exclusion of printer device */
	while(noFailure && prntCount < len) {
		/* Prepare printer and wait for Operation */
		disableInterrupts();
		prntChar = *(virtAddr + prntCount);
		prntReg->d_data0 = prntChar;
		prntReg->d_command = PRINTCHR;
		status = SYSCALL(WAITIO, PRNTINT, prntNo, FALSE);
		enableInterrupts();

		if(status == READY) {
			prntCount++;

		} else {
			noFailure = FALSE;
		}
	}

	sys11_vVirtSem(semAddr);	/* Release exclusion */

	if(noFailure) /* Return successfully */
		return prntCount;
	else
		return (-1 * status); /* Return failure */
}

/*
 * sys17_getTOD - place/return the number of microseconds since
 * the system was last botted/reset into the U-Proc's v0
 *
 * RETURN: time in microseconds since the system was last booted
 */
HIDDEN cpu_t sys17_getTOD(void) {
	cpu_t tod;
	STCK(tod);
	return tod;
}

/*
 * sys18_terminate - terminated the executing U-Proc, which will
 * cause Kaya to "shut down." This means all the system processes
 * created in the VM-I/O support level will be terminated as well.
 * Eventually, the nucleus scheduler will invoke HALT ROM service
 *
 * PARAM: asid - ID of the U-Proc to terminate
 */
HIDDEN void sys18_terminate(int asid) {
	uProcEntry_PTR upe = &(uProcList[asid - 1]);
	disableInterrupts(); /* New process ldst() will recover ints */
	/* Clear page table, segment table entry & remove from the ADL & AVSL */
	nukePageTable(upe->up_pgTable);
	segTable->kuSeg2[asid] = NULL;
	/* Could be sleeping or blocked on Mutex semaphore */

	/* Halt Delay Daemon when approaching the end */
	/* if(masterSem == 1) { banishDaemon(); } */
	/* Count down to death */
	SYSCALL(VERHOGEN, (int) &masterSem, 0, 0);
	SYSCALL(TERMINATEPROCESS, 0, 0, 0);
}

/*
 * calcBkgStoreAddr - calculates the address of the backing store
 *
 * PARAM: asid - id of a page entry
 *				pageOffset - page offset
 * RETURN: address of backing store
 */
int calcBkgStoreAddr(int asid, int pageOffset) {
	return ((asid * MAXPAGES) + pageOffset);
}

void disableInterrupts() {
	uint status = getSTATUS();
	status = status & ~INTMASKOFF & ~INTcON;
	setSTATUS(status);
}

void enableInterrupts() {
	uint status = getSTATUS();
	status = status | INTMASKOFF | INTcON;
	setSTATUS(status);
}

/*
 * findPTEntryIndex - finds index of page table entry
 *
 * PARAM: pageTable - a pointer to a page table entry to search for
 *				vpn - virtual page number
 * RETURN: an index of the page table entry
 */
HIDDEN int findPTEntryIndex(uPgTable_PTR pageTable, int vpn) {
	int vpnToMatch, loopVar = 0;
	int indexOfMatch = -1; /* If not found, this indicates error condition */
	int numEntries = pageTable->header & ENTRYCNTMASK;

	while((loopVar < numEntries) && (indexOfMatch == -1)) {
		vpnToMatch = ((pageTable->entries[loopVar].entryHI & VPNMASK) >> 12);

		indexOfMatch = (vpn == vpnToMatch) ? loopVar : -1;
		loopVar++;
	}

	return indexOfMatch;
}

/*
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
 * RETURN: ~uPgTable_PTR to a USER OR OS page table
 */
uPgTable_PTR getSegmentTableEntry(int segment, int asid) {
	if((segment == KSEGOS) || (segment == 1))
		return (uPgTable_PTR) segTable->kSegOS[asid];

	else if (segment == KUSEG2)
		return segTable->kuSeg2[asid];

	else if (segment == KUSEG3)
		return segTable->kuSeg3[asid];

	else
		return NULL; /* This is an error condition */
}

Bool frameInUse(uint frameDescriptor) {
	Bool isUsed = (frameDescriptor & 1) ? TRUE : FALSE;
	return isUsed;
}

/*
 * isDirty - returns TRUE if the page descriptor is dirty
 *
 * PARAM: pageDesc - a pointer to a page descriptor
 */
Bool isDirty(ptEntry_PTR pageDesc) {
	return TRUE; /* (*pageDesc & DIRTY) ? TRUE : FALSE; */
}

/*
 * nukePageTable -
 *
 * PARAM: pageTable - a pointer to page table entry
 */
void nukePageTable(uPgTable_PTR pageTable) {
	int loopVar, entries;

	/* Is this a page table or small island city? */
	if((pageTable->header & MAGICNUMMASK) != MAGICNUM)
		sys18_terminate(getASID());

	entries = pageTable->header & ENTRYCNTMASK;
	for(loopVar = 0; loopVar < entries; loopVar++) {
		pageTable->entries[loopVar].entryHI = -1;
		pageTable->entries[loopVar].entryLO = -1;
	}

	/* Reset Header Word Entry Count */
	pageTable->header = MAGICNUM;
}

/*
 * readPageFromBackingStore - reads page from the backing store
 *
 * PARAM: sectIndex - sector index
 *				destFrameAddr - memory address to read from
 */
void readPageFromBackingStore(int sectIndex, memaddr destFrameAddr) {
	int *semAddr = findMutex(DISKINT, 0, FALSE);

	sys12_pVirtSem(semAddr);
	engageDiskDevice(0, sectIndex, destFrameAddr, READ);
	sys11_vVirtSem(semAddr);
}

/*
 * writePageToBackingStore - writes page to the backing store
 *
 * PARAM: srcFrameAddr - memory address to write to
 *				sectIndex - sector index
 */
void writePageToBackingStore(memaddr srcFrameAddr, int sectIndex) {
	int *semAddr = findMutex(DISKINT, 0, FALSE);

	sys12_pVirtSem(semAddr);
	engageDiskDevice(0, sectIndex, srcFrameAddr, WRITE);
	sys11_vVirtSem(semAddr);
}

/*
 * engageDiskDevice -
 *
 * PARAM: diskNo - disk number
 *				sectIndex - sector index
 *				addr - memory address
 *				readOrWrite - read or write command code
 */
uint engageDiskDevice(int diskNo, int sectIndex, memaddr addr, int readOrWrite) {
	int head, sect, cyl, maxHeads, maxSects, maxCyls, status;
	device_t *diskDev = &(((devregarea_t*) RAMBASEADDR)->devreg[DEVINTNUM * DISKINT + diskNo]);

	int sectMask = 0x000000FF;
	int headMask = 0x0000FF00;
	int cylMask = 0xFFFF0000;

	maxSects = (diskDev->d_data1 & sectMask) >> 0;
	maxHeads = (diskDev->d_data1 & headMask) >> 8;
	maxCyls = (diskDev->d_data1 & cylMask) >> 16;

	/* Calc cylinder */
	cyl = sectIndex / (maxHeads * maxSects);

	/* TODO: disable interrupts?? */
	if(cyl >= maxCyls) { sys18_terminate(getASID()); }

	/* Move boom to the correct disk cylinder */
 	diskDev->d_command = cyl << 8 | SEEKCYL;
	status = SYSCALL(WAITIO, DISKINT, diskNo, FALSE);

	if(status != ACK) { sys18_terminate(getASID()); }

	/* Calc position within cylinder */
	head = (sectIndex / maxSects) % maxHeads;
	sect = sectIndex % maxSects;

	/* Prepare and Engage device */
	diskDev->d_command = head << 16 | sect << 8 | readOrWrite;
	diskDev->d_data0 = addr; /* Move srcFrameAddr or destFrameAddr into DATA0 */

	status = SYSCALL(WAITIO, DISKINT, diskNo, FALSE); /* Wait for job to complete */
	if(status != ACK)
		sys18_terminate(getASID());

	return status;
}

/*
 * invalidate - mutator used to mark a page as missing from memory
 *
 * PARAM: pte - page table entry to be marked as missing
 */
void invalidate(ptEntry_PTR pte) {
	pte->entryLO = pte->entryLO & ~VALID;
}

/*
 * selectFrameIndex -
 *
 * RETURN:
 */
int selectFrameIndex() {
	framePool.indexOfLastFrameReplaced += 1;
	framePool.indexOfLastFrameReplaced %= MAXFRAMES;
	return (framePool.indexOfLastFrameReplaced);
}
