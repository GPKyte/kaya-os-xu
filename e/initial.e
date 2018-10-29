#ifndef INITIAL
#define INITIAL

/************************ INITIAL.E **************************
*
*  The externals declaration file for the Initial module.
*
*  Written by Ploy Sithisakulrat and Gavin Kyte
*/

extern cpu_t startTOD;
extern int procCount
extern softBlkCount;

extern pcb_PTR curProc;
extern pcb_PTR readyQ;

extern int *psuedoClock;
extern static int[MAXSEM] semaphores;

/***************************************************************/

#endif
