#ifndef INITPROC
#define INITPROC

/************************ INITPROC.E ***************************
*
*
*
*
*
*
*  Written by Gavin Kyte and Ploy Sithisakulrat
*
****************************************************************/
extern fpTable_t framePool;
extern int masterSem;
extern int pager; /* Mutex for page swaping mechanism */
extern int mutexSems[MAXSEMS];
extern uProcEntry_t uProcList[MAXUPROC];
extern segTable_t* segTable;

extern void test();
extern void initUProc();

/***************************************************************/

#endif
