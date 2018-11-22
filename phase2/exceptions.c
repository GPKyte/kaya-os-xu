/********************* EXCEPTIONS.C **************************
 *
 * Covers three types of exceptions:
 *    1) Program Traps
 *    2) System Calls & Breakpoints
 *    3) Table Management
 *
 * Reference the *OLDAREA as defined in const for state context
 * Time spent in a SYSCALL belongs to the executing job.
 * Exceptions may return to the same process or a new process
 *   depending on if curProc was terminated or blocked
 *
 * AUTHORS: Ploy Sithisakulrat & Gavin Kyte
 * ADVISOR/CONTRIBUTER: Michael Goldweber
 * DATE PUBLISHED: 10.04.2018
 *************************************************************/

#include "../h/const.h"
#include "../h/types.h"

#include "../e/pcb.e"
#include "../e/asl.e"
#include "../e/initial.e"
#include "../e/scheduler.e"
#include "/usr/local/include/umps2/umps/libumps.e"

/************************* Prototypes ************************/
void copyState(state_PTR orig, state_PTR dest);
void pgrmTrapHandler();
void tlbHandler();
void sysCallHandler();

HIDDEN void avadaKedavra(pcb_PTR p);
HIDDEN void blockCurProc(int* semAdd);
HIDDEN void innocentOrNoose(int exceptionType, state_PTR oldState);
HIDDEN int sys1_createProcess(state_PTR birthState);
HIDDEN void sys2_terminateProcess();
HIDDEN void sys3_verhogen(int* mutex);
HIDDEN void sys4_passeren(int* mutex);
HIDDEN void sys5_specExceptionState(int type, state_PTR old, state_PTR new);
HIDDEN cpu_t sys6_getCPUTime();
HIDDEN void sys7_waitForClock();
HIDDEN void sys8_waitForIODevice(int lineNum, int deviceNum, Bool isReadTerm);

/********************* External Methods *********************/
/*
 * copyState - A utility method to deep copy states from orig to dest
 */
void copyState(state_PTR orig, state_PTR dest) {
	int i;
	dest->s_asid = orig->s_asid;
	dest->s_cause = orig->s_cause;
	dest->s_status = orig->s_status;
	dest->s_pc = orig->s_pc;

	for(i = 0; i < STATEREGNUM; i++)
		dest->s_reg[i] = orig->s_reg[i];
}

/*
 * Method called in event that a process performs an illegal
 * or undefined action. The cause will be set in the PgmTrap
 * old state vector's Cause.ExcCode
 *
 * Either passes up the offending process or terminates it (sys2) per
 * the existence of a specified exception state vector (sys5)
 */
void pgrmTrapHandler() {
	innocentOrNoose(PROGTRAP, (state_PTR) PGRMOLDAREA);
}

/*
 * Called when TLB Management Exception occurs,
 * i.e. when virtual -> physical mem address translation fails for
 * any of the following reasons:
 *    TLB-Modification, TLB-Invalid, Bad-PgTbl, or PTE-MISS
 *
 * Either passes up the offending process or terminates it (sys2) per
 * the existence of a specified exception state vector (sys5)
 */
void tlbHandler() {
	innocentOrNoose(TLBTRAP, (state_PTR) TLBOLDAREA);
}

/*
 * Offer 255 system calls; 1-8 are privileged & the rest are passed up
 * See helper methods for description of each system call.
 *
 * PARAM: a0 = int for system call number
 */
void sysCallHandler() {
	state_PTR oldSys;
	Bool userModeOn;
	oldSys = (state_PTR) SYSOLDAREA;

	/* Increment PC regardless of whether process lives after this call */
	oldSys->s_pc = oldSys->s_pc + 4;
	copyState(oldSys, &(curProc->p_s)); /* Set re-entry context */

	/* Check for reserved instruction error pre-emptively for less code */
	userModeOn = (oldSys->s_status & USERMODEON) > 0;
	if(userModeOn && oldSys->s_a0 <= 8 && oldSys->s_a0 > 0) {
		/* Set Reserved Instruction in Cause to handle as PROG TRAP */
		copyState(oldSys, (state_PTR) PGRMOLDAREA);
		((state_PTR) PGRMOLDAREA)->s_cause =
			(oldSys->s_cause & NOCAUSE) | RESERVEDINSTERR;
		pgrmTrapHandler();
	}

	/* Let a0 register decide SysCall type and execute appropriate method */
	switch(oldSys->s_a0) {
		case 1:
			oldSys->s_v0 = sys1_createProcess((state_PTR) oldSys->s_a1);
			loadState(oldSys);

		case 2:
			sys2_terminateProcess();

		case 3:
			sys3_verhogen((int*) oldSys->s_a1);
			loadState(oldSys);

		case 4:
			sys4_passeren((int*) oldSys->s_a1);
			loadState(oldSys); /* If not blocked on P: continue */

		case 5:
			sys5_specExceptionState(oldSys->s_a1,
				(state_PTR) oldSys->s_a2,
				(state_PTR) oldSys->s_a3);
			loadState(oldSys);

		case 6:
			oldSys->s_v0 = sys6_getCPUTime();
			loadState(oldSys);

		case 7:
			sys7_waitForClock();

		case 8:
			sys8_waitForIODevice(oldSys->s_a1, oldSys->s_a2, oldSys->s_a3);

		default: /* SYSCALL for unhandled method >= 9 */
			innocentOrNoose(SYSTRAP, oldSys);
	}
}

/********************** Helper methods **********************/
/*
 * avadaKedavra - Mutator method to recursively kill the given pcb_PTR
 * and all of its progeny. Leaves parent and siblings unaffected.
 * pcb can only be executing (curProc), ready (in queue), or waiting (blocked)
 * Used for sys2 abstraction
 */
HIDDEN void avadaKedavra(pcb_PTR p) {
	int* semStart, *semEnd;
	/* Top-down method, kill the children first */
	while(!emptyChild(p)) {
		avadaKedavra(removeChild(p));
	}

	/* bottom-up: dealing with each individual PCB
	 * If terminating a blocked process, do NOT adjust semaphore.
	 * Because the semaphore will get V'd by the interrupt handler */
	if(outProcQ(&deathRowLine, p) != NULL) {
		/* Know p was on Ready Queue, do nothing else */

	} else if(outBlocked(p) != NULL) {
		semStart = &(semaphores[0]);
		semEnd = &(semaphores[MAXSEMS - 1]);

		if(semStart <= p->p_semAdd && p->p_semAdd <= semEnd) {
			/* P blocked on device sema4; sema4++ in intHandler */
			softBlkCount--;

		} else {
			(*(p->p_semAdd))++; /* P blocked on NON device sema4 */
		}
	} /* else it was the curProc which is already handled in sys2 */

	/* Adjust procCount */
	freePcb(p);
	procCount--;
}

/*
 * blockCurProc - Track the current process's cpu time and block it
 *   on the given semaphore before scheduling the next job.
 */
HIDDEN void blockCurProc(int* semAdd) {
	/* Handle timer stuff */
	cpu_t stopTOD;
	STCK(stopTOD);
	curProc->p_CPUTime += stopTOD - startTOD;

	/* Block on sema4 */
	insertBlocked(semAdd, curProc);
	curProc = NULL;
	nextVictim();
}

/*
 * Decides whether to kill process for exception,
 *    or to fulfill specified exception behaviour.
 * PARAM: exceptionType {0: TLB, 1: PgrmTrap, 2: SYS/Bp}
 *        pointer to state that caused exception
 */
HIDDEN void innocentOrNoose(int exceptionType, state_PTR oldState) {
	if(curProc == NULL)
		gameOver(EXCEP); /* NULL ptr exception loop inbound */

	/* Check exception type and for a corresponding excep state vector */
	if(curProc->p_exceptionConfig[OLD][exceptionType] == NULL)
		/* Default case, nexception state not specified */
		sys2_terminateProcess();

	/*
	 * Pass up the processor state from old area into the process blk's
	 * Specified old area address. Then load the pcb's specified
	 * new area into the Process block.
	 */
	copyState(oldState, curProc->p_exceptionConfig[OLD][exceptionType]);
	copyState(curProc->p_exceptionConfig[NEW][exceptionType], &(curProc->p_s));
	loadState(&(curProc->p_s));
}

/*
 * Creates new process with given state as child of executing process
 *
 * EX: int SYSCALL (CREATEPROCESS, state_PTR statep)
 *    Where CREATEPROCESS has the value of 1.
 * PARAM: a1 = physical address of processor state area
 * RETURN: v0 = 0 (CHILD) on success, -1 (NOCHILD) on failure
 */
HIDDEN int sys1_createProcess(state_PTR birthState) {
	/* Birth new process as child of executing pcb */
	pcb_PTR child = allocPcb();

	if(child == NULL)
		return NOCHILD;

	copyState(birthState, &(child->p_s));
	insertChild(curProc, child);

	putInPool(child); /* insert child to the deathRowLine */
	procCount++;
	return CHILD;
}

/*
 * Kills the executing process and all descendents
 *
 * Orphan curProc from its parent and siblings
 * If a terminated process is blocked on a semaphore,
 * increment the semaphore to indicate a loss of a blocked job.
 * Else, if a terminated process is blocked on a device semaphore,
 * the semaphore should NOT be adjusted because when the I/O eventually occurs,
 * the semaphore will get V’ed by the interrupt handler.
 *
 * A ProcBlk is either the current process, sitting on the death row,
 * blocked on a device semaphore, or blocked on a non-device semaphore.
 *
 * EX: void SYSCALL (TERMINATEPROCESS)
 *   Where TERMINATEPROCESS has the value of 2.
 */
HIDDEN void sys2_terminateProcess() {
	outChild(curProc);
	avadaKedavra(curProc);
	curProc = NULL;
	nextVictim();
}

/*
 * Perform V (Signal/Increase) operation on a Semaphore
 *
 * EX: void SYSCALL (VERHOGEN, int *semaddr)
 *    Where the mnemonic constant VERHOGEN has the value of 3.
 * PARAM: a1 = semaphore address
 */
HIDDEN void sys3_verhogen(int* mutex) {
	(*mutex)++;

	if((*mutex) <= 0) {
		/* Give turn to next waiting process from semaphore */
		if(headBlocked(mutex)) {
			putInPool(removeBlocked(mutex));
		}
	}
}

/*
 * Perform P (Wait/Test) operation on a Semaphore
 *
 * EX: void SYSCALL (PASSEREN, int *semaddr)
 *    Where the mnemonic constant PASSEREN has the value of 4.
 * PARAM: a1 = semaphore address
 */
HIDDEN void sys4_passeren(int* mutex) {
	(*mutex)--;

	if((*mutex) < 0) {
		/* Put process in line to use semaphore and move on */
		blockCurProc(mutex);
	}
}

/*
 * Specify where a process will save and load its state from
 * in the case of an exception
 *
 * Each process may request a SYS5 service at most once for each of the
 * three exception types. An attempt to request a SYS5 service more than
 * once per exception type by any process should be construed as an error
 * and treated as a SYS2.
 *
 * EX: void SYSCALL (SPECTRAPVEC, int type, state t *oldp, state t *newp)
 *    Where the mnemonic constant SPECTRAPVEC has the value of 5.
 * PARAM: a1 = {0: TLB, 1: PgrmTrap, 2: SYS/Bp} (Exception type)
 *        a2 = address of old state vector to be stored in for this process
 *        a3 = address of new state vector to be loaded from after exception
 */
HIDDEN void sys5_specExceptionState(int type, state_PTR old, state_PTR new) {
	if(curProc->p_exceptionConfig[OLD][type] != NULL)
		/* Error, exception state already specified */
		sys2_terminateProcess();

	/* Specify old and new state vectors */
	curProc->p_exceptionConfig[OLD][type] = old;
	curProc->p_exceptionConfig[NEW][type] = new;
}

/*
 * Accesses the processor time (in microseconds) of the requesting process.
 *
 * EX: cpu t SYSCALL (GETCPUTIME)
 *    Where the mnemonic constant GETCPUTIME has the value of 6.
 * RETURN: v0 = processor time in microseconds
 */
HIDDEN cpu_t sys6_getCPUTime() {
	cpu_t stopTOD, partialQuantum;
	STCK(stopTOD);
	partialQuantum = stopTOD - startTOD;

	return curProc->p_CPUTime + partialQuantum;
}

/*
 * Performs a P operation on the nucleus-maintained pseudo-clock timer
 * semaphore. This semaphore is V’ed every 100 milliseconds
 * automatically by the nucleus elsewhere; do not set timer here.
 *
 * EX: void SYSCALL (WAITCLOCK)
 *    Where the mnemonic constant WAITCLOCK has the value of 7.
 */
HIDDEN void sys7_waitForClock() {
	/* Select and P the psuedo-clock timer */
	(*psuedoClock)--;

	if((*psuedoClock) < 0) {
		softBlkCount++;
		blockCurProc(psuedoClock);
	}
}

/*
 * Put process into a blocked / waiting state and perform a P operation
 * on the semaphore associated with the I/O device
 * indicated by the values in a1, a2[, a3]
 *
 * EX: unsigned int SYSCALL (WAITIO, int intlNo, int dnum, Bool isReadTerminal)
 *    Where the mnemonic constant WAITIO has the value of 8.
 * PARAM: a1 = Interrupt line number ([0..7])
 *        a2 = device number ([0..7])
 *        a3 = wait for terminal read operation -> SysCall TRUE / FALSE
 */
HIDDEN void sys8_waitForIODevice(int lineNum, int deviceNum, Bool isReadTerm) {
	/* Choose appropriate semaphore */
	int* semAdd = findSem(lineNum, deviceNum, isReadTerm);
	(*semAdd)--;

	/* Remove curProc and place on semaphore if successful */
	/* Replicate sys4 code for special handling */
	if((*semAdd) < 0) {
		softBlkCount++;
		blockCurProc(semAdd);

	} else {
		/* Unhandled niche case that doesn't appear in testing */
		gameOver(EXCEP);
	}
}
