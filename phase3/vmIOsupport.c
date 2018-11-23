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

#include

type? segments_g[MAXSEGMENTS];
uProcEntry_t uProcList[MAXUPROC];

/************************ Prototypes ***********************/




/********************* External Methods ********************/
/*
 * test - Set up the page and segment tables for all 8 user processes
 */
void test() {
	static uPgTable_t uPageTables[MAXUPROC];
	state_t newStateList[MAXUPROC];

	/* TODO: Create default entries for 2D Segment table */
		/* kuSegOs covers 0x0000.0000 - 0x7FFF.FFFF */
		/* kuSeg2 0x8000.0000 - 0xBFFF.FFFF */
		/* kuSeg3 0xC000.0000 - 0xFFFF.FFFF */
	for(loopVar = 0; loopVar < MAXSEGMENTS; loopVar++) {

	}

	/* Set up user processes */
	for(asid = 1; asid <= MAXUPROC; asid++) {
		/* Make a new page table for this process */
		uPageTables[asid - 1].magicPtHeaderWord = MAGIC;

		/* Fill in default entries */
		for(loopVar = 0; loopVar < MAXPTENTRIES; loopVar++) {
			newPTEntry = &(uPageTables[asid - 1][loopVar]);

			newPTEntry->entryHI = KUSEG2START | asid; /* TODO: shift these bits properly */
			newPTEntry->entryLO = DIRTY;
		}

		/* Correct last entry to become a stack page */
		newPTEntry->entryHI = (KUSEG3START - PAGESIZE) | asid /* TODO: confirm const and bit shift */

		/* Fill entry for user process tracking */
		newProcDesc = uProcList[asid - 1];
		newProcDesc->up_syncSem = 0;
		newProcDesc->up_pgTable = &(uPageTables[asid -1]);
		newProcDesc->up_bkgStoreAddr = calcBkgStoreAddr(asid);
		/* Probably don't need to set these here, will be handled in uProcInit();
		newProcDesc->up_stateAreas[OLD][TLBTRAP];
		newProcDesc->up_stateAreas[OLD][PROGTRAP];
		newProcDesc->up_stateAreas[OLD][SYSTRAP];
		newProcDesc->up_stateAreas[NEW][TLBTRAP];
		newProcDesc->up_stateAreas[NEW][PROGTRAP];
		newProcDesc->up_stateAreas[NEW][SYSTRAP]; */

		/* Create default kernel level state starting in init code */
		newState = &(newStateList[asid - 1]);
		newState->s_asid = asid;
		newState->s_sp = NULL; /* TODO: fill in later, maybe in other code block */
		newState->s_pc = (memaddr) uProcInit();
		/* Interrupts on, Local Timer On, VM Off, Kernel mode on */
		newState->s_status = (INTMASKOFF | INTpON | LOCALTIMEON)
			& ~VMpON & ~USERMODEON;

		SYSCALL(CREATEPROCESS, newState); /* SYSCALLs are Main reason for kernel mode */
	}
}

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
	/* Q: is this a different page table? Why? Does it belong to this new process? */
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
