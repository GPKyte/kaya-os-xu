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
int* devSemList[loopVar];
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
		newProcDesc->up_bkgStoreAddr = calcBkgStoreAddr(asid);

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
	newFrameNumber = nextFrameIndex();
	curFPEntry = framePool[newFrameNumber];
	newFrame = curFPEntry->fp_frameAddr;

	if(isDirty(newFrame)) {
		/* Get info for next steps from Frame Pool */
		oldFramePgTbl = curFPEntry->fp_pgTableAddr;

		/* Clear out frame (?) cache to avoid inconsistencies */
		/* TODO Better: Update page table entry as invalid to mean missing */
		nukePageTable(oldFramePgTbl);

		/* Write frame back to backing store */
		storageAddr = calcBkgStoreAddr(curFPEntry->fp_asid, someUnknownPageOffset);
		writeToBackingStore(newFrame, storageAddr);
	}

	/* Regardless of dirty frame, load in new page from BStore */
	storageAddr = calcBkgStoreAddr(asid, someUnknownPageOffset);
	readFromBackingStore(storageAddr, newFrame);

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
HIDDEN ptEntry_t* findPageTableEntry() {

}

int getSegmentTableEntry(int segment, int asid) {
	return segTable[segment][asid];
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
