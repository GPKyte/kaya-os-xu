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
HIDDEN status ack(lineNumber, deviceNumber) {
	unsigned int status;
	device_t *device;
	device_t *devRegArray = ((devregarea_t *) RAMBASEADDR)->devreg;

	device = &(devRegArray[(lineNumber - LINENUMOFFSET) * 8 + deviceNumber]);
	if(lineNumber == TERMINT) {
		status = handleTerminal(deviceNumber);
	} else {
		status = device->d_status;
		device->d_status = 1;
	}

	return status;
}

/* Select device number */
HIDDEN int findDeviceIndex(int intLine) {
	int deviceIndex;
	int deviceMask;
	devregarea_t * devregarea = (devregarea_t *) RAMBASEADDR;
	unsigned int deviceBits = devregarea->interrupt_dev[intLine - LINENUMOFFSET];

	for(deviceIndex = 0; deviceIndex < DEVPERINT; deviceIndex++) {
		deviceMask = 1 << deviceIndex;

		if(deviceBits & deviceMask) {
			break;
		}
	}

	return deviceIndex;
}

/* Determine line number of highest priority interrupt */
HIDDEN int findLineIndex(unsigned int causeRegister) {
	int lineIndex;
	int numOfIntLines = 8;
	unsigned int lineMask = 1;
	unsigned int intLineBits = (causeRegister & INTPENDMASK) >> 8;

	for(lineIndex = 0; lineIndex < numOfIntLines; lineIndex++) {
		lineMask = 1 << lineIndex;

		if(intLineBits & lineMask) {
			break;
		}
	}

	return lineIndex;
}

HIDDEN unsigned int handleTerminal(int terminal) {
	/* handle writes then reads */
	unsigned int status;
	unsigned int *writeStatus;
	unsigned int *readStatus;

	writeStatus = device + DEVREGLEN * RECVSTATUS;
	readStatus = device + DEVREGLEN * TRANSTATUS;
	if(*writeStatus != ACK) {
		status = *writeStatus;
		*writeStatus = ACK;
	} else {
		status = *readStatus;
		*readStatus = ACK;
	}

	return status;
}

/********************** External Methods *********************/
void intHandler() {
	pcb_PTR proc;
	int *semAdd;
	int lineNumber, deviceNumber;
	unsigned int status;

	unsigned int stopTOD = STCK();
	state_t *oldInt = (state_t *) OLDINTAREA;
	lineNumber = findLineIndex(oldInt->s_cause);

	if(lineNumber == 0) { /* Handle inter-processor interrupt (not now) */
		PANIC();

	} else if (lineNumber == 1) { /* Handle Local Timer (End of QUANTUMTIME) */
		/* Timing stuff maybe? */
		scheduler();

	} else if (lineNumber == 2) { /* Handle Interval Timer */
		*INTERVALTMR = INTERVALTIME /* Put time on clock */

		/* V the psuedoClock */
		(*psuedoClock)--;
		if((*psuedoClock) <= 0) {
			insertBlocked(psuedoClock, curProc);
			softBlkCount++;
			scheduler();

		} else {
			loadState(&oldInt);
		}

	} else { /* lineNumber >= 3; Handle I/O device interrupt */
		/* Handle external device interrupts */
		deviceNumber = findDeviceIndex(lineNumber);
		status = ack(lineNumber, deviceNumber);

		/* Be aware that I/O int can occur BEFORE sys8_waitForIODevice */
		/* V the device's semaphore, once io complete, put back on ready queue */
		semAdd = &(semaphores[(lineNumber - LINENUMOFFSET) * DEVPERINT + device]);
		(*semAdd)++;
		if((*semAdd) <= 0) {
			proc = removeBlocked(semAdd);
			insertProcQ(&readyQ, proc);
			softBlkCount--;
		}
	}

	oldInt->s_v0 = status;
	loadState(&oldInt);
}
