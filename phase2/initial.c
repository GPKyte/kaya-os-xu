/*************************************************************
 * Initial.c
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

extern void test();

Bool waiting;
cpu_t startTOD;
int procCount, softBlkCount;
pcb_PTR curProc;
pcb_PTR readyQ; /* Queue of non-blocked jobs to be executed */
int *psuedoClock; /* a semaphore */
int semaphores[MAXSEMS];

void debug(int a, int b) {
 int i;
 i = a+b;
 i++;
}

/*
 * findSem - Calculates address of device semaphore
 * This excludes the psuedoClock and non-device semaphores
 * PARAM: int lineNum is the device type, correlates with the interrupt lineNum
 *        int deviceNum is the index of a device within a type group
 *        Bool isReadTerm is FALSE for writeT & non-terminals, TRUE for readT
 * RETURN: int* calculated address of device semaphore
 */
int* findSem(int lineNum, int deviceNum, Bool isReadTerm) {
	int semGroup = (lineNum - LINENUMOFFSET) + isReadTerm;
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
  devregarea_t *devregarea;
  state_PTR intNewArea, tlbMgntNewArea, pgrmTrpNewArea, sysCallNewArea;
  pcb_PTR firstProc;

  /* Init semaphores to 0 */
  /* The first semaphore describes device at interrupt line 3, 1st device */
  for(i = 0; i < MAXSEMS; i++) {
    semaphores[i] = 0;
  }
  psuedoClock = &(semaphores[MAXSEMS - 1]);

  devregarea = (devregarea_t *) RAMBASEADDR; /* ROM defined hardware info */
  ramtop = (devregarea->rambase) + (devregarea->ramsize);

  /* Init new processor state areas */
  intNewArea = (state_t *) INTNEWAREA;
  tlbMgntNewArea = (state_t *) TLBNEWAREA;
  pgrmTrpNewArea = (state_t *) PGRMNEWAREA;
  sysCallNewArea = (state_t *) SYSNEWAREA;

  intNewArea->s_pc = (memaddr) intHandler;
  tlbMgntNewArea->s_pc = (memaddr) tlbHandler;
  pgrmTrpNewArea->s_pc = (memaddr) pgrmTrapHandler;
  sysCallNewArea->s_pc = (memaddr) sysCallHandler;

  intNewArea->s_t9 = (memaddr) intHandler;
  tlbMgntNewArea->s_t9 = (memaddr) tlbHandler;
  pgrmTrpNewArea->s_t9 = (memaddr) pgrmTrapHandler;
  sysCallNewArea->s_t9 = (memaddr) sysCallHandler;

  /* Initialize stack pointer  */
  intNewArea->s_sp = ramtop;
  tlbMgntNewArea->s_sp = ramtop;
  pgrmTrpNewArea->s_sp = ramtop;
  sysCallNewArea->s_sp = ramtop;

  /* status: VM off, interrupts off, kernal-mode, and local timer on */
  baseStatus = LOCALTIMEON & ~VMpON & ~INTpON & ~USERMODEON;
  intNewArea->s_status = baseStatus;
  tlbMgntNewArea->s_status = baseStatus;
  pgrmTrpNewArea->s_status = baseStatus;
  sysCallNewArea->s_status = baseStatus;

  initPCBs(); /* initialize ProcBlk Queue */
  initASL(); /* Initialize Active Semaphore List */

  /* initialize Phase 2 global variables */
  procCount = 0;
  softBlkCount = 0;
  curProc = NULL;
  readyQ = mkEmptyProcQ();
  firstProc = allocPcb();

  /*
   * Setting state for initial process:
   *    VM off, interrupts on, local timer on, user mode off
   *    Stack starts below reserved page
   *    Set PC to start at P2's test
   */
  firstProc->p_s.s_status = (INTMASKOFF | INTpON | LOCALTIMEON) & ~USERMODEON & ~VMpON;
  debug(125, (int) firstProc->p_s.s_status); 
  firstProc->p_s.s_sp = ramtop - PAGESIZE;
  firstProc->p_s.s_pc = (memaddr) test;
  firstProc->p_s.s_t9 = firstProc->p_s.s_pc; /* For technical reasons, setting t9 to pc */

  waiting = FALSE;
  procCount++;
  putInPool(firstProc);
  LDIT(INTERVALTIME);
  scheduler();
  return 0; /* Will never reach, but this will remove the pointless warning */
}
