/*
 * initial.c - Initialize the primary data structures for OS
 * Assign interrupt handlers and load main program into memory.
 */

#include "../h/const.h"
#include "../h/types.h"

#include "../e/pcb.e"
#include "../e/asl.e"
#include "/usr/local/include/umps2/umps/libumps.e"

int procCount, softBlkCount;
pcb_PTR curProc; /* current running process */
pcb_PTR readyQ; /* 'ready' status waiting for the scheduler to pick them to run */

/*
 * main() populates the four new areas in low memory
 *  - set the sp (last page of physical memory)
 *  - set PC
 *  - set the status: VM OFF
 *                    Interrupts masked (disabled)
 *                    Supervisor mode on (kernel-mode)
 * It is being called only once.
 */
int main() {
  int i;
  unsigned int RAMTOP;
  devregarea_t *devregarea;
  unsigned int localTimeStatusBit = 1 << 27;
  unsigned int VMStatusBit = 0 << 24;
  unsigned int kernalModeStatusBit = 0 << 1;
  unsigned int intStatusBit = 0;
  unsigned int baseStatus = localTimeStatusBit | VMStatusBit | kernalModeStatusBit | intStatusBit;

  devregarea = (devregarea_t *) RAMBASEADDR;
  /* initialize RAMTOP */
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

  /* status: VM OFF, Interrupts disabled, kernel-mode, and Local Timer enabled */
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

  /* set a single process status to: VM off, Interrupts enabled, kernel-mode, and Local Timer enabled */
  /* p->p_s.s_status = 0; */
  p->p_s.s_status = baseStatus | 1;
  p->p_s.s_sp = RAMTOP - PAGESIZE;
  p->p_s.s_pc = (memaddr) test;
  p->p_s.s_t9 = p->p_s.s_pc; /* For technical reasons, setting t9 to pc */

  procCount++;
  insertProcQ(&readyQ, p);
  return 0;
}
