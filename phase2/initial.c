/************************ INITIAL.C ***************************
 *
 * Start Kaya OS:
 *    Define states of the 4 "New" state vectors
 *      (4 Old left empty)
 *    Init Queue service for processes
 *    Init Active Semaphore List
 *
 * Status bit definitions are kept in constants file
 * Hardware generates machine specific info and ROM code
 *    like RAMSIZE at RAMBASEADDR before this is run
 *
 * AUTHORS: Ploy Sithisakulrat & Gavin Kyte
 * ADVISOR/CONTRIBUTER: Michael Goldweber
 * DATE PUBLISHED: 10.04.2018
 *************************************************************/

#include "../h/const.h"
#include "../h/types.h"

#include "../e/pcb.e"
#include "../e/asl.e"
#include "../e/scheduler.e"
#include "../e/interrupts.e"
#include "../e/exceptions.e"
#include "/usr/local/include/umps2/umps/libumps.e"

extern void test(); /* To link OS's 1st process to test file location */

int procCount, softBlkCount, semaphores[MAXSEMS];
int *psuedoClock; /* a semaphore */
Bool waiting;
cpu_t startTOD;
pcb_PTR curProc;
pcb_PTR deathRowLine; /* Queue of non-blocked jobs to be executed */

/*
 * findSem - Calculates address of device semaphore
 * This excludes the psuedoClock and non-device semaphores
 * PARAM: int lineNum is the device type, correlates with the interrupt lineNum
 *        int deviceNum is the index of a device within a type group
 *        Bool isReadTerm is FALSE for writeT & non-terminals, TRUE for readT
 * RETURN: int* calculated address of device semaphore
 */
int* findSem(int lineNum, int deviceNum, Bool isReadTerm) {
	int termOffset = (isReadTerm) ? 1 : 0;
	int semGroup = (lineNum - LINENUMOFFSET) + termOffset;
	return &(semaphores[semGroup * DEVPERINT + deviceNum]);
}

/*
 * Populate the four new areas in low memory. Allocate finite
 * resources for the nucleus to manage.
 * Only runs once. Scheduler takes control after this method
 */
int main() {
	int i;
	unsigned int ramtop, baseStatus;
	devregarea_t* devregarea;
	state_PTR intNewArea, tlbMgntNewArea, pgrmTrpNewArea, sysCallNewArea;
	pcb_PTR firstP;

	/* Init semaphores to 0 */
	/* The first semaphore describes device at line 3, 1st device */
	for(i = 0; i < MAXSEMS; i++) {
		semaphores[i] = 0;
	}

	psuedoClock = &(semaphores[MAXSEMS - 1]);

	/* Get ROM defined hardware info */
	devregarea = (devregarea_t*) RAMBASEADDR;
	ramtop = (devregarea->rambase) + (devregarea->ramsize);

	/* Init new processor state areas */
	intNewArea = (state_PTR) INTNEWAREA;
	tlbMgntNewArea = (state_PTR) TLBNEWAREA;
	pgrmTrpNewArea = (state_PTR) PGRMNEWAREA;
	sysCallNewArea = (state_PTR) SYSNEWAREA;

	/* Set t9 to pc for uMIPS2 reasons */
	intNewArea->s_pc = intNewArea->s_t9 = (memaddr) intHandler;
	tlbMgntNewArea->s_pc = tlbMgntNewArea->s_t9 = (memaddr) tlbHandler;
	pgrmTrpNewArea->s_pc = pgrmTrpNewArea->s_t9 = (memaddr) pgrmTrapHandler;
	sysCallNewArea->s_pc = sysCallNewArea->s_t9 = (memaddr) sysCallHandler;

	/* Initialize stack pointer  */
	intNewArea->s_sp = tlbMgntNewArea->s_sp =
		pgrmTrpNewArea->s_sp = sysCallNewArea->s_sp = ramtop;

	/* status: VM off, interrupts off, kernal-mode, and local timer on */
	baseStatus = LOCALTIMEON & ~VMpON & ~INTpON & ~USERMODEON;
	intNewArea->s_status = tlbMgntNewArea->s_status =
		pgrmTrpNewArea->s_status = sysCallNewArea->s_status = baseStatus;

	initPCBs(); /* initialize ProcBlk Queue */
	initASL(); /* Initialize Active Semaphore List */

	/* initialize Phase 2 global variables */
	procCount = 0;
	softBlkCount = 0;
	curProc = NULL;
	deathRowLine = mkEmptyProcQ();
	firstP = allocPcb();

	/*
	 * Setting state for initial process:
	 *    VM off, interrupts on, local timer on, user mode off
	 *    Stack starts below reserved page
	 *    Set PC to start at P2's test
	 */
	firstP->p_s.s_status = baseStatus | INTMASKOFF | INTpON;
	firstP->p_s.s_sp = ramtop - PAGESIZE;
	firstP->p_s.s_pc = firstP->p_s.s_t9 = (memaddr) test;

	waiting = FALSE;
	procCount++;
	putInPool(firstP);
	LDIT(INTERVALTIME);
	nextVictim();
	return 0; /* Will never reach, but this removes the warning */
}
