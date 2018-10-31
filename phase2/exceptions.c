/*************************************************************
 * Exceptions.c
 *
 * Covers three types of exceptions:
 *    1) Program Traps
 *    2) System Calls & Breakpoints
 *    3) Table Management
 *
 * State decided by SYSOLDAREA context, this module provides
 * system services and manages TRAP and TLB Exceptions.
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

state_t *oldSys; /* TODO: Make this local and change methods to match */

/********************** Helper methods **********************/
/*
 * A utility to copy over states
 */
HIDDEN void copyState(state_PTR orig, state_PTR dest) {
  int i;
  dest->s_asid = orig->s_asid;
  dest->s_cause = orig->s_cause;
  dest->s_status = orig->s_status;
  dest->s_pc = orig->s_pc;
  for(i = 0; i < STATEREGNUM; i++) {
    dest->s_reg[i] = orig->s_reg[i];
  }
}

/*
 * Mutator method to recursively kill the given pcb_PTR and all of its
 * progeny. Leaves parent and siblings unaffected. Removes from any Queue/semd_t
 * Used for sys2 abstraction
 */
HIDDEN void avadaKedavra(pcb_PTR p) {
  /* top-down method */
  while(!emptyChild(p)) {
    avadaKedavra(removeChild(p));
  }

  /* bottom-up: dealing with each individual PCB */
  /* TODO: If semaphore value negative, increment the semaphore? */
  /* If terminating a blocked process, do NOT adjust semaphore. Because the
   * semaphore will get V'd by the interrupt handler */
  if(outProcQ(&readyQ, p) == p) {
    /* Know p was on Ready Queue, do nothing more */
  } else if(outBlocked(p) == p) {
    /* TODO: make test to decide if was blocked on device sema4 */
    /* if p is a device sema4: soft block-- & leave sema4++ to intHandler */
    /* if NOT a device sema4: sema4++ */
  } else {
    /* Unexpected behavior */
    PANIC();
  }

  /* Adjust procCount */
  freePcb(p);
  procCount--;
}

/*
 * only for testing - ignore this
 */
HIDDEN void sys2_terminateProcess() {
  avadaKedavra(outChild(curProc));
  curProc = NULL;
  scheduler();
}

/*
 * Decides whether to kill process for exception,
 *    or to fulfill specified exception behaviour.
 * PARAM: exceptionType {0: TLB, 1: PgrmTrap, 2: SYS/Bp}
 */
HIDDEN void genericExceptionTrapHandler(int exceptionType) {
  /* Check exception type and existence of a specified excep state vector */
  if(curProc->p_exceptionConfig[OLD][exceptionType] == NULL
    || curProc->p_exceptionConfig[NEW][exceptionType] == NULL) {
    /* Default, exception state not specified */
    sys2_terminateProcess();
    scheduler();
  }

  /*
   * Pass up the processor state from old area into the process blk's
   * Specified old area address. Then load the pcb's specified new area into
   * the Process block.
   * TODO: clarify what is stored and loaded, i.e. STST vs copyState
   */
  copyState(oldSys, curProc->p_exceptionConfig[OLD][exceptionType]);
  copyState(curProc->p_exceptionConfig[NEW][exceptionType], &(curProc->p_s));
  loadState(&(curProc->p_s));
}

/*
 * Creates new process with given state as child of executing process
 *
 * EX: int SYSCALL (CREATEPROCESS, state_t *statep)
 *    Where CREATEPROCESS has the value of 1.
 * PARAM: a1 = physical address of processor state area
 * RETURN: v0 = 0 (CHILD) on success, -1 (NOCHILD) on failure
 */
HIDDEN void sys1_createProcess() {
  /* Birth new process as child of executing pcb */
  pcb_PTR child = allocPcb();

  if(child == NULL) {
    oldSys->s_v0 = NOCHILD;
    scheduler();
  }

  copyState(oldSys->s_a1, &(child->p_s));
  insertChild(curProc, child);

  oldSys->s_v0 = CHILD;
  insertProcQ(&readyQ, child); /* insert child to the readyQ */
  /* Return control to calling process */
  loadState(oldSys);
}

/*
 * Kills the executing process and all descendents
 *
 * Orphan curProc from its parent and siblings
 * If a terminated process is blocked on a semaphore,
 * increment the semaphore to indicate a loss of a blocked job.
 * Else, if a terminated process is blocked on a device semaphore, the semaphore
 * should NOT be adjusted because when the I/O eventually occurs,
 * the semaphore will get V’ed by the interrupt handler.
 *
 * A ProcBlk is either the current process, sitting on the ready queue,
 * blocked on a device semaphore, or blocked on a non-device semaphore.
 *
 * EX: void SYSCALL (TERMINATEPROCESS)
 *   Where TERMINATEPROCESS has the value of 2.
 */
// HIDDEN void sys2_terminateProcess() {
//   avadaKedavra(outChild(curProc));
//   curProc = NULL;
//   scheduler();
// }

/*
 * Perform V (Increase) operation on a Semaphore
 *
 * EX: void SYSCALL (VERHOGEN, int *semaddr)
 *    Where the mnemonic constant VERHOGEN has the value of 3.
 * PARAM: a1 = semaphore address
 */
HIDDEN void sys3_verhogen() {
  int *mutex = oldSys->s_a1;
  (*mutex)--;
  if((*mutex) < 0) {
    /* Put process in line to use semaphore and move on */
    insertBlocked(mutex, curProc);
    scheduler();
  } else {
    /* Ready to go right now */
    loadState(&oldSys);
  }
}

/*
 * Perform P (Test) operation on a Semaphore
 *
 * EX: void SYSCALL (PASSEREN, int *semaddr)
 *    Where the mnemonic constant PASSEREN has the value of 4.
 * PARAM: a1 = semaphore address
 */
HIDDEN void sys4_passeren() {
  int *mutex = oldSys->s_a1;
  (*mutex)++;
  if((*mutex) <= 0) {
    /* Give turn to next waiting process from semaphore */
    if(headBlocked(mutex)) {
      insertProcQ(&readyQ, removeBlocked(mutex));
    }
  }
  loadState(&oldSys);
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
HIDDEN void sys5_specifyExceptionStateVector() {
  /* Check wether exception state vector is already specified (error) */
  if(curProc->p_exceptionConfig[OLD][oldSys->s_a1] != NULL
    || curProc->p_exceptionConfig[NEW][oldSys->s_a1] != NULL) {
    /* Error, exception state already specified */
    sys2_terminateProcess();
    scheduler();
  }

  /* Specify old and new state vectors */
  curProc->p_exceptionConfig[OLD][oldSys->s_a1] = (state_t *) oldSys->s_a2;
  curProc->p_exceptionConfig[NEW][oldSys->s_a1] = (state_t *) oldSys->s_a3;

  /* Return control to process (LDST) */
  loadState(oldSys);
}

/*
 * Accesses the processor time (in microseconds) of the requesting process.
 *
 * EX: cpu t SYSCALL (GETCPUTIME)
 *    Where the mnemonic constant GETCPUTIME has the value of 6.
 * RETURN: v0 = processor time in microseconds
 */
HIDDEN void sys6_getCPUTime() {
  int partialQuantum = stopTOD - startTOD;
  oldSys->s_v0 = partialQuantum + curProc->p_CPUTime; /* set return for LDST */
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
  (*psuedoClock)++;
  if((*psuedoClock) <= 0) {
    if(headBlocked(psuedoClock)) {
      insertProcQ(&readyQ, removeBlocked(psuedoClock));
      softBlkCount--;
    }
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
HIDDEN void sys8_waitForIODevice() {
  /* Choose appropriate semaphore */
  int lineNumber = oldSys->s_a1 - LINENUMOFFSET;
  int deviceNumber = oldSys->s_a2;
  Bool isReadTerminal = oldSys->s_a3;

  int* semAdd = &(semaphores[(lineNumber + isReadTerminal) * DEVPERINT + deviceNumber]);

  /* Remove curProc and place on semaphore if successful */
  if(outProcQ(&readyQ, curProc) == NULL) {
    PANIC();
  }

  /* Replicate sys3 code for special handling */
  (*semAdd)--;
  if((*semAdd) < 0) {
    insertBlocked(semAdd, curProc);
    softBlkCount++;
  } else {
    PANIC(); /* Error condition, will explore later */
  }
}


/***************** Start of external methods *****************/
/*
 * Offer 255 system calls; 1-8 are privileged & the rest are passed up
 * See helper methods for description of each system call.
 *
 * PARAM: a0 = int for system call number
 */
void sysCallHandler() {
  /* Stop counting time for curProc */
  STCK(stopTOD);
  oldSys = (state_t *) SYSOLDAREA;
  /* Increment PC regardless of whether process lives after this call */
  oldSys->s_pc = oldSys->s_pc + 4;
  /* Check for reserved instruction error pre-emptively for less code */
  Bool isUserMode = (oldSys->s_status & USERMODEON);
  if(isUserMode && oldSys->s_a0 <= 8 && oldSys->s_a0 > 0) {
    /* Set Reserved Instruction in Cause register and handle as PROG TRAP */
    oldSys->s_cause = (oldSys->s_cause & NOCAUSE) | RESERVEDINSTERR;
    copyState(oldSys, (state_t *) PGRMOLDAREA);
    pgrmTrapHandler();
  }
  /* Let a0 register decide SysCall type and execute appropriate method */
  switch (oldSys->s_a0) {
    case 1:
      sys1_createProcess(); /* Keeps control */
      break;
    case 2:
      sys2_terminateProcess(); /* Changes control */
      break;
    case 3:
      sys3_verhogen(); /* ? control */
      break;
    case 4:
      sys4_passeren(); /* ? control */
      break;
    case 5:
      sys5_specifyExceptionStateVector(); /* Keeps control */
      break;
    case 6:
      sys6_getCPUTime(); /* Keeps control */
      break;
    case 7:
      sys7_waitForClock(); /* ? control */
      break;
    case 8:
      sys8_waitForIODevice(); /* ? control */
      break;
    default:
      genericExceptionTrapHandler(SYSTRAP); /* Changes control */
      break;
  }

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
  oldSys = (state_t *) PGRMOLDAREA;
  genericExceptionTrapHandler(PROGTRAP);
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
  oldSys = (state_t *) TLBOLDAREA;
  genericExceptionTrapHandler(TLBTRAP);
}
