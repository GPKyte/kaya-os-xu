#ifndef SCHEDULER
#define SCHEDULER

/************************ SCHEDULER.E **************************
*
*  The externals declaration file for the Scheduler module.
*
*  Written by Gavin Kyte and Ploy Sithisakulrat
*/

extern void putInPool(pcb_PTR p);
extern void loadState(state_PTR state);
extern void panic(int fileOrigin);
extern void scheduler();

/***************************************************************/

#endif
