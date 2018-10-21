/*************************************************************
 * Interrupts.c
 *
 * This module handles the interrupts generated from I/O
 * devices when previously intitaited I/O requests finish
 * or when the Local or Interval Timers
 * transition from 0x0000.0000 -> 0xFFFF.FFFF
 *
 * We manage 8 types of devices (interal and external), each type group can
 * have 8 devices and are ordered sequentially by type-priority.
 * The associated 4-word device registers are kept in ROM.
 *
 * There are 48 managed semaphores in total: 8 ea. for
 * disks, tapes, printers, netw cards, and read/write terminals.
 * Terminals have two semaphores for r/w, but use the same register space.
 *
 * We use a contiguous block of 104 semaphores (int's) to represent all
 * possible devices. About half will be unused, but this design
 * will help simplify semaphore referencing related to devices.
 *
 * Note: Multiple interrupts can be active at once,
 * we prioritize for the lower interrupt line and device #;
 * also terminal transmission (W) > terminal receiving (R)
 *
 * AUTHORS: Ploy Sithisakulrat & Gavin Kyte
 * ADVISOR/CONTRIBUTER: Michael Goldweber
 * DATE PUBLISHED: 10.21.2018
 *************************************************************/

#include "../h/const.h"
#include "../h/types.h"

#include "../e/pcb.e"
#include "../e/asl.e"
#include "../e/scheduler.e"
#include "/usr/local/include/umps2/umps/libumps.e"

/*********************** Helper Methods **********************/
HIDDEN void ack(/* Address to interrupting device */)

/********************** External Methods *********************/
void intHandler() {
	/* Save status for special handling */
	/* Acknowledge interrupt to turn it off */
	/* V the semaphore of the interrupting subdevice */
		/* Index of sema4 = line * 8 + device index */
	/* Handle case for timer, use psuedo-clock timer sema4 */
	/* Handle weird timing issues surrounding sys8-wait command */
		/* Use result of V operation to determine race condition */
		/* If waiting for this I/O, store status in process */
		/* If I/O before waiting, store the interrupting status somewhere */
}
