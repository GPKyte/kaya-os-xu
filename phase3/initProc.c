/*********************** INITPROC.C ************************
 * Initialize process
 *
 * This module is used to make kernel-level
 *   specifications to new processes and the main
 *   method, ******, should generally be the
 *   starting PC value.
 *
 * One major kernal action is 3x sys5 calls.
 *
 * AUTHORS: Ploy Sithisakulrat & Gavin Kyte
 * ADVISOR/CONTRIBUTER: Michael Goldweber
 * DATE PUBLISHED:
 ***********************************************************/

#include "../h/const.h"
#include "../h/types.h"
#include "../e/scheduler.e"
#include "../e/vmIOsupport.e"

#include "usr/local/include/umps2/umps/libumps.e"

/************************ Prototypes ***********************/
unsigned int getASID();


/********************* External Methods ********************/

/*
 * initProc - loads a user process from the tape drive
 * and copy it to the backing store on disk 0, then start
 * that specific user process.
 */
HIDDEN void initProc() {
  int i, newAreaSP;
  state_PTR newArea; /* TODO: either this or will have to specify area */
  state_t procState;
  unsigned int asid = getASID();

  /* 1) Set up the three new areas for Pass up or die
   *  - status: all INTs ON | LOCTIMERON | VMpON | USERMODEON
   *  - ASID = your asid value
   *  - stack page: to be filled in later
   *  - PC = t9 = address of your P3 handler
   */

  for(i = 0; i < TRAPTYPES; i++) {
    newArea->s_status =
    INTpON | INTMASKOFF | LOCALTIMEON | VMpON | ~USERMODEON;
    newArea->s_asid = getENTRYHI();

    /* TODO: init pgrmTrap and sysCall handlers for P3 */
    /* TODO: stack page */
    switch (i) {
      case (TLBTRAP):
        newArea->s_pc = newArea->s_t9 = (memaddr) tlbHandler;
        newAreaSP = ; /* memory page for sys5 sp for each newArea */
        break;

      case (PROGTRAP):
        newArea->s_pc = newArea->s_t9 = (memaddr) pgrmTrapHandler;
        newAreaSP = ; /* memory page for sys5 sp for each newArea */
        break;

      case (SYSTRAP):
        newArea->s_pc = newArea->s_t9 = (memaddr) sysCallHandler;
        newAreaSP = ; /* memory page for sys5 sp for each newArea */
        break;
    }

    /* call SYS 5 for every trap type (3 times) */
    SYSCALL();
  }

  /*
   *  Read the contents of the tape device (asid-1) onto the
   *  backing store device (disk0)
   *  - keep reading until the tape block marker (data1) is
   *    no longer ENDOFBLOCK
   *    - read block from tape and then immediately write
   *      it out to disk0
   */


  /*  Set up a new state for the user process
   *    - status: all INTs ON | LOCALTIMEON | VMpON | USERMODEON
   *    - asid = your asid
   *    - stack page = last page of kUseg2 (0xC000.0000)
   *    - PC = well known address from the start of kUseg2
   */
  procState.s_status =
      INTpON | INTMASKOFF | LOCALTIMEON | VMpON | USERMODEON;
  procState.s_asid = asid;
  procState.s_sp = KUSEG3START;
  procState.s_pc = procState.s_t9 = (memaddr) KUSEG2START;

  loadState(&procState);
}

/*
 *  getASID - returns the ASID of the currently running process
 */

unsigned int getASID() {
  unsigned int asid = getENTRYHI();
  asid = (asid & 0x00000FC0) >> 6;

  return (asid);
}

/********************** Helper Methods *********************/
