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

}


/********************** Helper Methods *********************/
HIDDEN findPageTableEntry() {

}
