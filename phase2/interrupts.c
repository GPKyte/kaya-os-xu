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
	devregarea_t devregarea;
	int lineIndex; /* 0 is line 3 in deviceRegisterArea */
	int deviceIndex;
	int deviceMask;
	int interruptingSemaphore;
	device_t interruptingDevice;
	unsigned int interruptStatus;
	/* Check Processor Local Timer and Interval Timers */

	/* Determine line number of highest priority interrupt */
	devregarea = (devregarea_t *) RAMBASEADDR;
	lineIndex = 0;
	while (devregarea->interrupt_dev[lineIndex] == 0) {
		++lineIndex;
	}

	/* Select device number */
	deviceIndex = 0;
	deviceMask = 1;
	while (devregarea->interrupt_dev[lineIndex] & deviceMask) == 0) {
		deviceIndex++;
		deviceMask = 1 << deviceIndex;
	}

	interruptingDevice = devregarea->devreg[lineIndex * 8 + deviceIndex];
	/* Save status for special handling */
	interruptStatus = interruptingDevice->d_status;

	/* Acknowledge interrupt to turn it off */
	ack(&interruptingDevice);

	interruptingSemaphore = /* Find semaphore */;
	SYSCALL(VERHOGEN, &interruptingSemaphore);
	/* Handle case for timer, use psuedo-clock timer sema4 */
		/* Perform V operation on psuedo-clock timer */
		/* LDIT(INTERVALTIME); /* Arbitrary interval time */
	/* Handle weird timing issues surrounding sys8-wait command */
		/* Use result of V operation to determine race condition */
		/* If waiting for this I/O, store status in process */
		/* If I/O before waiting, store the interrupting status somewhere */
}
