/*************************************************************
 * Interrupts.c
 *
 * This module handles the interrupts generated from I/O
 * devices when previously intitaited I/O requests finish
 * or when the Local or Interval Timers transition
 * from 0x0000.0000 -> 0xFFFF.FFFF
 *
 * The Cause register in the OLDINTAREA describes current ints
 *
 * There are 49 managed semaphores in total: 1 psuedo-clock
 * & 8 ea. for disks, tapes, printers, netw cards,
 * and read/write terminals which have two semaphores for r/w,
 * but use the same ROM device-register space.
 * Start at managed devices at interrupt line 3
 *
 * We use a contiguous block of 48 semaphores (int's) to represent
 * all devices and a special variable for the psuedo-clock
 *
 * Note: Multiple interrupts can be active at once,
 * we prioritize for the lower interrupt line and device #;
 * & prioritize terminal write > terminal read.
 *
 * Currently, we only handle one interrupt per context switch
 * Later, it would be desireable to optimize this.
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
HIDDEN void ack(device_t*) {}
HIDDEN int findDeviceIndex() {}
HIDDEN int findLineIndex() {}

/********************** External Methods *********************/
void intHandler() {
	/* Check Processor Local Timer and Interval Timers */
	/* Determine line number of highest priority interrupt */
	/* Select device number */
	/* Save status for special handling */
	/* Acknowledge interrupt to turn it off */
	/* Handle case for timer, use psuedo-clock timer sema4 */
		/* Perform V operation on psuedo-clock timer */
		/* LDIT(INTERVALTIME); /* Arbitrary interval time */
	/* Handle weird timing issues surrounding sys8-wait command */
		/* Use result of V operation to determine race condition */
		/* If waiting for this I/O, store status in process */
		/* If I/O before waiting, store the interrupting status somewhere */
}
