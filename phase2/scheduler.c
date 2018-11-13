/***********************SCHEDULER.C***************************
 *
 * Decides the next process to run for Kaya OS
 * Pre-emptive, no starvation, favors long-jobs
 *
 * Use Round-Robin selection algorithm with the "Ready Queue"
 * Decide some time-slice value for timer interrupt
 *
 * When ready queue is empty detect:
 *    deadlock: procCount > 0 && softBlkCount == 0
 *    termination: procCount == 0
 *    waiting: procCount > 0 && softBlkCount > 0
 *
 * AUTHORS: Ploy Sithisakulrat & Gavin Kyte
 * ADVISOR/CONTRIBUTER: Michael Goldweber
 * DATE PUBLISHED: 10.22.2018
 *************************************************************/
#include "../h/const.h"
#include "../h/types.h"

#include "../e/pcb.e"
#include "../e/asl.e"
#include "../e/initial.e"
#include "/usr/local/include/umps2/umps/libumps.e"

/********************* Helper methods ***********************/
/*
 * Select next process to be scheduled as curProc
 * RETURN: pcb_PTR to ready process for execution
 */
HIDDEN pcb_PTR removeFromPool() {
	/* In Round-Robin style, grab next process */
	return removeProcQ(&readyQ);
}

/*
 * Put process in a ready state
 * PARAM: pointer to PCB to be returned to pool
 */
void putInPool(pcb_PTR p) {
	if(p != NULL)
		insertProcQ(&readyQ, p);
}
/*************************** External methods *****************************/
/*
 * loadState - An abstraction of LDST() to improve context info and encapsulation
 */
void loadState(state_PTR statep) {
	STCK(startTOD);
	LDST(statep);
}

/*
 * fuckIt - A wrapper function to provide a psuedo status code
 *   to the PANIC() operation.
 * PARAM: file location as defined in const
 */
void fuckIt(int location) {
	int a = i;
	a++;
	PANIC();
}

/*
* scheduler - Mutator method that decides the currently running process
* and manages the associated queues and meta data.
*
* Pre: Any context switch
* Post: curProc points to next job and starts or if no available
*   jobs, system will HALT, PANIC, or WAIT appropriately
*/
void scheduler() {
	state_t waitState;
	curProc = removeFromPool();

	if(curProc != NULL) {
		/* Prepare state for next job */
		/* Put time on clock */
		STCK(startTOD);
		setTIMER(QUANTUMTIME);
		loadState(&(curProc->p_s));
	}

	if(procCount == 0) /* Finished all jobs so HALT system */
		HALT();

	if(softBlkCount == 0) /* Detected deadlock so PANIC */
		fuckIt(SCHED);

	waiting = TRUE;
	waitState.s_status = (getSTATUS() | INTMASKOFF | INTcON);
	setTIMER((int) MAXINT);

	/* No ready jobs, so we WAIT for next interrupt */
	setSTATUS(waitState.s_status); /* turn interrupts on */
	WAIT();
}
