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

extern void tlbHandler();
extern int calcBkgStoreAddr(int asid, int pageOffset);
extern int* findMutex(int lineNum, int deviceNum, Bool isReadTerm);
extern uPgTbl_PTR getSegmentTableEntry(int segment, int asid);
extern Bool isDirty(ptEntry_PTR pageDesc);
extern void nukePageTable(uPgTbl_PTR pageTable);
extern void readPageFromBackingStore(int sectIndex, memaddr destFrameAddr);
extern void engageDiskDevice(int sectIndex, memaddr addr, int readOrWrite);
extern int selectFrameIndex();
extern void setSegmentTableEntry(int segment, int asid, uPgTable_PTR addr);
extern void setSegmentTableEntry(int segment, int asid, osPgTable_PTR addr);
extern void writePageToBackingStore(memaddr srcFrameAddr, int sectIndex);

/***************************************************************************/

#endif
