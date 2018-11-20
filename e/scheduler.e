#ifndef SCHEDULER
#define SCHEDULER

/************************ SCHEDULER.E ***************************
*
*  The externals declaration file for the Scheduler module.
*
*  Scheduler provides methods to interact with process
*  scheduling and context switching.
*
*  Written by Gavin Kyte and Ploy Sithisakulrat
****************************************************************/

extern void putInPool(pcb_PTR p);
extern void loadState(state_PTR state);
extern void gameOver(int fileOrigin);
extern void nextVictim();

/***************************************************************/

#endif
