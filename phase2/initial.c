/*************************************************************
 * Initial.c
 *
 * Start Kaya OS:
 *    Define states of the 4 "New" state vectors (4 Old left empty)
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
#include "/usr/local/include/umps2/umps/libumps.e"

extern void test();

int procCount, softBlkCount;
pcb_PTR curProc;
pcb_PTR readyQ; /* Queue of non-blocked jobs to be executed */

/*
 * Populate the four new areas in low memory. Allocate finite
 * resources for the nucleus to manage.
 * Only runs once. Scheduler takes control after this method
 */
int main() {
  int i;
  unsigned int RAMTOP;
  devregarea_t *devregarea;
  unsigned int baseStatus;

  devregarea = (devregarea_t *) RAMBASEADDR; /* Predefined hardware details */
  RAMTOP = (devregarea->rambase) + (devregarea->ramsize);

  /* Init new processor state areas */
  state_PTR intNewArea = ROMPAGESTART + STATESIZE;
  state_PTR tlbMgntNewArea = ROMPAGESTART + 3 * STATESIZE;
  state_PTR pgrmTrpNewArea = ROMPAGESTART + 5 * STATESIZE;
  state_PTR sysCallNewArea = ROMPAGESTART + 7 * STATESIZE;

  for(i = 0; i < STATEREGNUM; i++) {
    intNewArea->p_s.s_reg[i] = 0;
    tlbMgntNewArea->p_s.s_reg[i] = 0;
    pgrmTrpNewArea->p_s.s_reg[i] = 0;
    sysCallNewArea->p_s.s_reg[i] = 0;
  }

  intNewArea->s_pc = (memaddr) intHandler;
  tlbMgntNewArea->s_pc = (memaddr) tlbHandler;
  pgrmTrpNewArea->s_pc = (memaddr) trapHandler;
  sysCallNewArea->s_pc = (memaddr) sysCallHandler;

  intNewArea->s_t9 = (memaddr) intHandler;
  tlbMgntNewArea->s_t9 = (memaddr) tlbHandler;
  pgrmTrpNewArea->s_t9 = (memaddr) trapHandler;
  sysCallNewArea->s_t9 = (memaddr) sysCallHandler;

  /* Initialize stack pointer  */
  intNewArea->s_sp = RAMTOP;
  tlbMgntNewArea->s_sp = RAMTOP;
  pgrmTrpNewArea->s_sp = RAMTOP;
  sysCallNewArea->s_sp = RAMTOP;

  /* status: VM off, interrupts off, kernal-mode, and local timer on */
  baseStatus = LOCALTIMEON & ~VMpON & ~INTpON & ~USERMODEON);
  intNewArea->status = baseStatus;
  tlbMgntNewArea->status = baseStatus;
  pgrmTrpNewArea->status = baseStatus;
  sysCallNewArea->status = baseStatus;

  initPCBs(); /* initialize ProcBlk Queue */
  initASL(); /* Initialize Active Semaphore List */

  /* initialize Phase 2 global variables */
  procCount = 0;
  softBlkCount = 0;
  curProc = NULL;
  readyQ = mkEmptyProcQ();

  pcb_PTR p = allocPcb();
  /* initialize the process state */
  for(i = 0; i < STATEREGNUM; i++) {
    p->p_s.s_reg[i] = 0;
  }

  /*
   * Setting state for initial process:
   *    VM off, interrupts on, local timer on, user mode off
   *    Stack starts below reserved page
   *    Set PC to start at P2's test
   */
  p->p_s.s_status = INTMASKOFF | INTpON | LOCALTIMEON & ~USERMODEON & ~VMpON;
  p->p_s.s_sp = RAMTOP - PAGESIZE;
  p->p_s.s_pc = (memaddr) test;
  p->p_s.s_t9 = p->p_s.s_pc; /* For technical reasons, setting t9 to pc */

  procCount++;
  insertProcQ(&readyQ, p);
  scheduler();
  return 0;
}
