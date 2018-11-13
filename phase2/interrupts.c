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
void debugI(int line, int somePtr) {
	int i = line + somePtr;
	i++;
}

/*
 * findDevice - Calculate address of device given interrupt line and device num
 */
HIDDEN device_t* findDevice(int lineNum, int deviceNum) {
	device_t* devRegArray = ((devregarea_t*) RAMBASEADDR)->devreg;
	return &(devRegArray[(lineNum - LINENUMOFFSET) * DEVPERINT + deviceNum]);
}

/*
 * isReadTerm - Decides whether interrupting device is a terminal
 * and whether it should be treated as a read (TRUE) or write (FALSE) terminal
 * Note: Dependent on pre-ACKnowledged status of term device,
 *       We assume either read or write active, but check for improper lineNum
 */
HIDDEN Bool isReadTerm(int lineNum, device_t* dev) {
	/* Check writeStatus because write priortized over read */
	unsigned int transmStatusMask = 0x0F;

	if(lineNum != TERMINT) /* Wrong line, not even a terminal */
		return FALSE;

	if((dev->t_transm_status & transmStatusMask) != READY) /* Write, not read */
		return FALSE;

	return TRUE; /* lineNum == TERMINT && readStatus != READY, i.e. isReadTerm */
}

/* Select device number */
HIDDEN int findDeviceIndex(int intLine) {
	int deviceIndex;
	int deviceMask = 1; /* Traveling bit to select device */

	devregarea_t* devregarea = (devregarea_t*) RAMBASEADDR;
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

HIDDEN unsigned int handleTerminal(device_t* device) {
	/* handle writes then reads */
	unsigned int transmitStatusMask = 0x0F;
	unsigned int status;

	if((device->t_transm_status & transmitStatusMask) != READY) {
		status = device->t_transm_status;
		device->t_transm_command = ACK;

	} else {
		status = device->t_recv_status;
		device->t_recv_command = ACK;
	}

	return status;
}

HIDDEN int ack(int lineNumber, device_t* device) {
	unsigned int status;

	if(lineNumber == TERMINT) {
		status = handleTerminal(device);

	} else {
		status = device->d_status;
		device->d_command = ACK;
	}

	return status;
}

/********************** External Methods *********************/
void intHandler() {
	pcb_PTR p;
	state_t* oldInt;
	device_t* device;
	int* semAdd;
	int lineNumber, deviceNumber;
	Bool isRead;
	unsigned int status;
	cpu_t stopTOD;
	cpu_t endOfInterrupt;
	cpu_t tmpTOD;

	STCK(stopTOD);
	oldInt = (state_t*) INTOLDAREA;
	lineNumber = findLineIndex(oldInt->s_cause);
	debugI(156, lineNumber);

	if(lineNumber == 0) { /* Handle inter-processor interrupt (not now) */
		fuckIt(INTER);

	} else if(lineNumber == 1) { /* Handle Local Timer (End of QUANTUMTIME) */
		/* Timing stuff maybe? Switch to next process */
		debugI(164, stopTOD - startTOD);
		curProc->p_CPUTime += stopTOD - startTOD; /* More or less a QUANTUMTIME */
		copyState(oldInt, &(curProc->p_s)); /* Save off context for reentry */

		putInPool(curProc);
		curProc = NULL;
		scheduler();

	} else if(lineNumber == 2) { /* Handle Interval Timer */
		/* Release all jobs from psuedoClock */
		(*psuedoClock)++;

		if((*psuedoClock) <= 0) {

			while(headBlocked(psuedoClock) != NULL) {
				STCK(tmpTOD);

				putInPool(p = removeBlocked(psuedoClock));
				softBlkCount--;

				STCK(endOfInterrupt);
				debugI(187, p == NULL);
				p->p_CPUTime += (tmpTOD - endOfInterrupt); /* Account for time spent */
			}
		}

		(*psuedoClock) = 0;
		LDIT(INTERVALTIME);

	} else { /* lineNumber >= 3; Handle I/O device interrupt */
		/* Handle external device interrupts */
		deviceNumber = findDeviceIndex(lineNumber);
		device = findDevice(lineNumber, deviceNumber);
		isRead = isReadTerm(lineNumber, device);
		status = ack(lineNumber, device);

		/* Be aware that I/O int can occur BEFORE sys8_waitForIODevice */
		/* V the device's semaphore, once io complete, put back on ready queue */
		semAdd = findSem(lineNumber, deviceNumber, isRead);
		(*semAdd)++;

		if((*semAdd) <= 0) {
			putInPool(p = removeBlocked(semAdd));
			softBlkCount--;
			p->p_s.s_v0 = status;

			STCK(endOfInterrupt);
			p->p_CPUTime += (stopTOD - endOfInterrupt); /* Account for time spent */
		}
	}

	/* General non-accounted time space belongs to OS, not any process */
	if(waiting || curProc == NULL) {
		/* Came back from waiting state, get next job; don't return to WAIT */
		waiting = FALSE;
		scheduler();
	}

	if(stopTOD - startTOD < QUANTUMTIME) /* Return stolen time if remainder > 0 */
		setTIMER(QUANTUMTIME - (stopTOD - startTOD));

	loadState(oldInt);
}
