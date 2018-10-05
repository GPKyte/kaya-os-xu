/*************************************************************
 * Exceptions.c
 *
 * Covers three types of exceptions:
 *    1) Program Traps
 *    2) System Calls & Breakpoints
 *    3) Table Management
 *
 * Based on context from old-state vectors, decides course of action
 * loads the corresponding and appropriate new context.
 *
 * Uses oldState globally for simplicity and speed of reference
 * parameters and return values are handled at psuedo-register level
 * by accessing and mutating stored context.
 *
 * AUTHORS: Ploy Sithisakulrat & Gavin Kyte
 * ADVISOR/CONTRIBUTER: Michael Goldweber
 * DATE PUBLISHED: 10.04.2018
 *************************************************************/

state_t *oldState;

/********************** Private methods **********************/

/*
 * Creates new child process with given state
 *
 * EX: int SYSCALL (CREATEPROCESS, state_t *statep)
 *    Where CREATEPROCESS has the value of 1.
 * PARAM: a1 = physical address of processor state area
 * RETURN: v0 = 0 on success, -1 (NULL) on failure
 */
HIDDEN void sys1_createProcess() {}

/*
 * Kills the executing process and all descendents
 *
 * EX: void SYSCALL (TERMINATEPROCESS)
 *   Where TERMINATEPROCESS has the value of 2.
 */
HIDDEN void sys2_terminateProcess() {}

/*
 * Perform V operation on a Semaphore
 *
 * EX: void SYSCALL (VERHOGEN, int *semaddr)
 *    Where the mnemonic constant VERHOGEN has the value of 3.
 * PARAM: a1 = semaphore address
 */
HIDDEN void sys3_verhogen() {}

/*
 * Perform P operation on a Semaphore
 *
 * EX: void SYSCALL (PASSEREN, int *semaddr)
 *    Where the mnemonic constant PASSEREN has the value of 4.
 * PARAM: a1 = semaphore address
 */
HIDDEN void sys4_passeren() {}

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
HIDDEN void sys5_specifyExceptionStateVector() {}

/*
 * Accesses the processor time (in microseconds) of the requesting process.
 *
 * EX: cpu t SYSCALL (GETCPUTIME)
 *    Where the mnemonic constant GETCPUTIME has the value of 6.
 * RETURN: v0 = processor time in microseconds
 */
HIDDEN void sys6_getCPUTime() {}

/*
 * Performs a P operation on the nucleus-maintained pseudo-clock timer
 * semaphore. This semaphore is V’ed every 100 milliseconds
 * automatically by the nucleus.
 *
 * EX: void SYSCALL (WAITCLOCK)
 *    Where the mnemonic constant WAITCLOCK has the value of 7.
 */
HIDDEN void sys7_waitForClock() {}

/*
 * Put process into a blocked / waiting state and perform a P operation
 * on the semaphore associated with the I/O device
 * indicated by the values in a1, a2[, a3]
 *
 * EX: unsigned int SYSCALL (WAITIO, int intlNo, int dnum, int waitForTermRead)
 *    Where the mnemonic constant WAITIO has the value of 8.
 * PARAM: a1 = Interrupt line number
 *        a2 = device number ([0..7])
 *        a3 = wait for terminal read operation -> SysCall TRUE / FALSE
 */
HIDDEN void sys8_waitForIODevice() {}


/***************** Start of external methods *****************/
/*
 * Offer 255 system calls; 1-8 are privileged; 9 is for breakpoints
 * The rest are passed up. See helper methods for description of
 * each system call.
 */
void sysCallHandler() {
  /* Load in context from old-state vector */

  /* Check for reserved instruction error pre-emptively for less code */
  /* Let a0 register decide SysCall type and execute appropriate method */
}

/*
 * Method called in event that a process performs an illegal
 * or undefined action. The cause will be set in the PgmTrap
 * old state vector's Cause.ExcCode
 *
 * Either passes up the offending process or terminates it (sys2) per
 * the existence of a specified exceptiont state vector (sys5)
 */
void trapHandler() {
  /* Check excptn type and existence of a specified exception state vector */
    /* Terminate */

    /*
     * Pass up the processor state from old area into the process blk's
     * Specified old area address. Then load the pcb's specified new area into
     * the Process block.
     */
}

/*
 * Called when TLB Management Exception occurs,
 * i.e. when virtual -> physical mem address translation fails for
 * any of the following reasons:
 *    TLB-Modification, TLB-Invalid, Bad-PgTbl, or PTE-MISS
 *
 * Either passes up the offending process or terminates it (sys2) per
 * the existence of a specified exceptiont state vector (sys5)
 */
void tlbHandler() {
  /* Same as trapHandler, delete this comment once implemented */
}
