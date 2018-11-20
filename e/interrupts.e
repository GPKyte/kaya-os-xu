#ifndef INTERRUPTS
#define INTERRUPTS

/************************** INTERRUPTS.E ***********************
*
*  The externals declaration file for the Interrupt handler
*  module.
*
*  Interrupts handles interrupts from clocks, and all peripheral
*  devices. Depending on the device, the interrupt handler will
*  switch context at the end of a job quantum, unblock jobs
*  every pseudo-clock tick, and acknowledge device interrupts.
*
*  Written by Gavin Kyte and Ploy Sithisakulrat
****************************************************************/

extern void intHandler();

/***************************************************************/

#endif
