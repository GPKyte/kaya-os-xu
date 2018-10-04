/*
 * Scheduler.c
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
 * ADVISOR: Michael Goldweber
 */
 #include "../h/const.h"
 #include "../h/types.h"

 #include "../e/pcb.e"
 #include "../e/asl.e"
 #include "/usr/local/include/umps2/umps/libumps.e"

 /* Helper methods */

 /*
  * Mutator method that decides the currently running process
  * and manages the associated queues and meta data.
  *
  * Pre: Any context switch
  * Post: curProc points to next job and starts or if no available
  *   jobs, system will HALT, PANIC, or WAIT appropriately
  */
 void scheduleNext() {
   /* Elevate priority by masking all interrupts */

   /* Finished all jobs so HALT system */
   /* Detected deadlock so PANIC */

   /*
    * Store curProc back in Queue if unfinished
    * Otherwise release process and decrement procCount
    */

   /* Decide which process goes next */
      /* No ready jobs, so we WAIT for next interrupt */

   /* Prepare state for next job */
   /* Put time on clock */
   /* Load context of process and continue execution */
 }
