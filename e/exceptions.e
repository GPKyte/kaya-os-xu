#ifndef EXCEPTIONS
#define EXCEPTIONS

/************************** EXCEPTIONS.E ***********************
*
*  The externals declaration file for the Exceptions handler
*  module.
*
*  Exceptions provides access to system calls and handles
*  improper behaviors, i.e. exceptions. This happens either when
*  an illegal action is performed or when privileged instruction,
*  SYSCALLS is requested.
*
*  After leaving exceptions, the current process will either
*  get blocked, get terminated, or continue executing.
*  A blocked or new process can be put on a ready queue.
*
*  Written by Gavin Kyte
****************************************************************/

extern void copyState(state_PTR orig, state_PTR dest);
extern void sysCallHandler();
extern void tlbHandler();
extern void pgrmTrapHandler();

/***************************************************************/

#endif
