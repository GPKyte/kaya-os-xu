#ifndef EXCEPTIONS
#define EXCEPTIONS

/************************** EXCEPTIONS.E ******************************
*
*  The externals declaration file for the Exceptions handler file
*
*  Written by Gavin Kyte
*/

extern void copyState(state_PTR orig, state_PTR dest);
extern void sysCallHandler();
extern void tlbHandler();
extern void pgrmTrapHandler();

/***************************************************************/

#endif
