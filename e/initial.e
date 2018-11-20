#ifndef INITIAL
#define INITIAL

/************************ INITIAL.E ****************************
*
*  The externals declaration file for the Initial module.
*
*  Initial defines states of the 4 new state areas in lower
*  memory and initializes the global variables and data
*  structures for Kaya OS.
*
*  It creates new semaphores for peripheral devices and starts
*  a process queue service.
*
*  Written by Ploy Sithisakulrat and Gavin Kyte
****************************************************************/

extern Bool waiting;
extern cpu_t startTOD;
extern int procCount;
extern int softBlkCount;

extern pcb_PTR curProc;
extern pcb_PTR readyQ;

extern int *psuedoClock;
extern int semaphores[MAXSEMS];

extern int* findSem(int lineNum, int deviceNum, Bool isReadTerm);

/***************************************************************/

#endif
