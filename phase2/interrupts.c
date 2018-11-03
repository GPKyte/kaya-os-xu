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
#include "../e/initial.e"
#include "../e/scheduler.e"
#include "/usr/local/include/umps2/umps/libumps.e"

/*********************** Helper Methods **********************/
/*
 * findDevice - Calculate address of device given interrupt line and device num
 */
HIDDEN device_t* findDevice(int lineNum, int deviceNum) {
	device_t *devRegArray = ((devregarea_t *) RAMBASEADDR)->devreg;
	return &(devRegArray[(lineNum - LINENUMOFFSET) * DEVPERINT + deviceNum]);
}

/*
 * isReadTerm - Decides whether interrupting device is a terminal
 * and whether it should be treated as a read (TRUE) or write (FALSE) terminal
 * Note: Dependent on pre-ACKnowledged status of term device,
 *       We assume either read or write active, but check for improper lineNum
 */
HIDDEN Bool isReadTerm(int lineNum, device_t *dev) {
	/* Check writeStatus because write priortized over read */
	unsigned int *writeStatus;

	writeStatus = (unsigned int *) ((unsigned int) dev + DEVREGLEN * RECVSTATUS);
	if((lineNum != TERMINT) || ((*writeStatus) != ACK)) {
		return FALSE;
	}
	/* Thus, lineNum == TERMINT && readStatus != ACK, i.e. isReadTerm */
	return TRUE;
}

/* Select device number */
HIDDEN int findDeviceIndex(int intLine) {
	int deviceIndex;
	int deviceMask = 1; /* Traveling bit to select device */

	devregarea_t * devregarea = (devregarea_t *) RAMBASEADDR;
	unsigned int deviceBits = devregarea->interrupt_dev[intLine - LINENUMOFFSET];

	/* Find active bit */
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

HIDDEN unsigned int handleTerminal(device_t *device) {
	/* handle writes then reads */
	unsigned int status;
	unsigned int *writeStatus;
	unsigned int *readStatus;

	writeStatus = (unsigned int *) ((unsigned int) device + DEVREGLEN * RECVSTATUS);
	readStatus = (unsigned int *) ((unsigned int) device + DEVREGLEN * TRANSTATUS);
	if(*writeStatus != ACK) {
		status = *writeStatus;
		*writeStatus = ACK;
	} else {
		status = *readStatus;
		*readStatus = ACK;
	}

	return status;
}

HIDDEN int ack(int lineNumber, device_t* device) {
	unsigned int status;
	if(lineNumber == TERMINT) {
		status = handleTerminal(device);
	} else {
		status = device->d_status;
		device->d_status = ACK;
	}

	return status;
}

/********************** External Methods *********************/
void intHandler() {
	state_t *oldInt;
	device_t *device;
	pcb_PTR proc;
	int *semAdd;
	int lineNumber, deviceNumber;
	Bool isReadTerm;
	unsigned int status;
	unsigned int stopTOD;

	STCK(stopTOD);
	oldInt = (state_t *) INTOLDAREA;
	lineNumber = findLineIndex(oldInt->s_cause);

	if(lineNumber == 0) { /* Handle inter-processor interrupt (not now) */
		PANIC();

	} else if (lineNumber == 1) { /* Handle Local Timer (End of QUANTUMTIME) */
		/* Timing stuff maybe? */
		scheduler();

	} else if (lineNumber == 2) { /* Handle Interval Timer */
		/* Release all jobs from psuedoClock */
		(*psuedoClock)++;
		if((*psuedoClock) < 0) {

			while(headBlocked(psuedoClock)) {
	      insertProcQ(&readyQ, removeBlocked(psuedoClock));
				softBlkCount--;
	    }

			*psuedoClock = 0;
			LDIT(INTERVALTIME);
			scheduler();
		}

	} else { /* lineNumber >= 3; Handle I/O device interrupt */
		/* Handle external device interrupts */
		deviceNumber = findDeviceIndex(lineNumber);

		device = findDevice(lineNumber, deviceNumber);
		isReadTerm = isReadTerm(lineNumber, device);
		status = ack(lineNumber, device);

		/* Be aware that I/O int can occur BEFORE sys8_waitForIODevice */
		/* V the device's semaphore, once io complete, put back on ready queue */
		semAdd = findSem(lineNumber, deviceNumber, isReadTerm);
		(*semAdd)++;
		if((*semAdd) <= 0) {
			proc = removeBlocked(semAdd);
			insertProcQ(&readyQ, proc);
			softBlkCount--;
		}
	}

	oldInt->s_v0 = status;
	/* TODO: Handle timing stuff */
	loadState(oldInt);
}
