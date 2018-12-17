#ifndef AVSL
#define AVSL

/*************************** AVSL.E *****************************
*
*  The externals declaration file for the Active Virtual Semaphore
*  List Module.
*
*  Written by Gavin Kyte and Ploy Sithisakulrat
*
*****************************************************************/

extern void initAVSL (void);
extern Bool anotherCrash (int *driverID, int asid, virtSemd_PTR vs);
extern virtSemd_PTR clearCrash (int *driverID, virtSemd_PTR vs);

/****************************************************************/

#endif
