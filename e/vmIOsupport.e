#ifndef VMIOSUPPORT
#define VMIOSUPPORT

/***************************** VMIOSUPPORT.E *******************************
*
*
*
*
*
*
*  Written by Gavin Kyte and Ploy Sithisakulrat
*
***************************************************************************/

extern void uTlbHandler(void);
extern void uSysCallHandler(void);
extern void uPgrmTrapHandler(void);
extern int calcBkgStoreAddr(int asid, int pageOffset);
extern int* findMutex(int lineNum, int deviceNum, Bool isReadTerm);
extern uPgTable_PTR getSegmentTableEntry(int segment, int asid);
extern Bool isDirty(ptEntry_PTR pageDesc);
extern void nukePageTable(uPgTable_PTR pageTable);
extern void readPageFromBackingStore(int sectIndex, memaddr destFrameAddr);
extern void engageDiskDevice(int sectIndex, memaddr addr, int readOrWrite);
extern int selectFrameIndex();
extern void writePageToBackingStore(memaddr srcFrameAddr, int sectIndex);

/***************************************************************************/

#endif
