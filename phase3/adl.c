/************************** ADL.C **************************
 * ADL module implements the Active Delay List module to
 * store and keep track of the "sleeping" U-proc's,
 * which is maintained as a sorted NULL-terminated
 * single linearly linked list. Similarly, the free list,
 * that holds the unused delay-node descriptors, is also
 * maintained as a NULL-terminated single linearly linked
 * list.
 *
 * AUTHORS: Ploy Sithisakulrat & Gavin Kyte
 * ADVISOR/CONTRIBUTER: Michael Goldweber
 * DATE PUBLISHED:
 ***********************************************************/

 #include "../h/const.h"
 #include "../h/types.h"
 #include "testers/h/tconst.h"

 #include "../e/pcb.e"
 #include "../e/initProc.e"
 #include "../e/vmIOsupport.e"

 #include "/usr/local/include/umps2/umps/libumps.e"

delayd_PTR openBeds_h; /* Active Delay Free List */
delayd_PTR nextToWake_h; /* ADL */

/************************ Prototypes ***********************/
HIDDEN void freeBed(delayd_PTR bed);
HIDDEN delayd_PTR allocBed (void);
HIDDEN delayd_PTR searchBeds (int wakeTime);
/********************* External Methods ********************/

/*
 * initADL - initializes the active delay list (ADL) to store
 * and keep track of the "sleeping" U-proc's
 */
void initADL(void) {
	int i;
	static delayd_t bedFactory[MAXUPROC + 2]; /* +2 for dummy nodes */

	openBeds_h = NULL; /* Init delay list */

	for(i = 0; i < MAXUPROC; i++) {
		freeBed(&(bedFactory[i]));
	}

	/* Set boundary beds for ADL to simplify condition checking */
	bedFactory[MAXUPROC].d_wakeTime = 0;
	nextToWake_h = &(bedFactory[MAXUPROC]);
	bedFactory[MAXUPROC + 1].d_wakeTime = (cpu_t) MAXINT;
	nextToWake_h->d_next = &(bedFactory[MAXUPROC + 1]);
	nextToWake_h->d_next->d_next = NULL;
}

/*
 * summonSandmanTheDelayDemon - this process determines if a
 * U-Proc's wake-time has passed. For each delay-node that
 * wake-time has passed, perform a V operation on the
 * indicated U-Proc's private semaphore, remove it from
 * the ADL and return it to the free list.
 */
void summonSandmanTheDelayDemon() {
	cpu_t alarmTime;
	int *semAdd;
	delayd_PTR napNeighbor;

	while(TRUE) {
		STCK(alarmTime);

		/* Find beds to wake up */
		while(nextToWake_h->d_wakeTime <= alarmTime) {
			/* Wake up processes by calling V on their semaphore */
			semAdd = &(uProcList[nextToWake_h->d_asid - 1].up_syncSem);
			SYSCALL(VSEMVIRT, (int)semAdd, 0, 0); /* Wake up proc */

			/* Kick out proc */
			napNeighbor = nextToWake_h->d_next;
			freeBed(nextToWake_h);
			nextToWake_h = napNeighbor;
		}

		/* Take micronap */
		SYSCALL(WAITCLOCK, 0, 0, 0);
	}
}

/*
 * setAlarm - a mutator to insert a process into a delay descriptor
 *   on the ordered ADL. The process should be the only process to sleep on
 *   this descriptor
 *
 * PARAM:  Id of Proc to delay, Time of Day to wake up proc
 * RETURN: TRUE if successfully delayed, FALSE otherwise
 */
Bool setAlarm(int asid, cpu_t wakeTime) {
	delayd_PTR neighborNapper = searchBeds(wakeTime);
	delayd_PTR target = allocBed();

	if(target == NULL) {
		/* Allocation failed, all beds occupied TODO: this shouldn't be a concern.. ?*/
		return (FALSE);
	} else {
		/* Init fields and insert new bed into ADL */
		target->d_occupantID = asid;
		target->d_wakeTime = wakeTime;
		target->d_next = neighborNapper->d_next;
		neighborNapper->d_next = target;
	}

	return (TRUE);
}

/********************** Helper Methods *********************/
/*
 * freeBed - a mutator used to insert a delay-node into the
 * active delay free list.
 *
 * PARAM: bed - a pointer to a delay-node to be added to the
 *              active delay free list.
 */
HIDDEN void freeBed(delayd_PTR bed) {
	bed->d_next = openBeds_h;
	openBeds_h = bed;
}

/*
 * allocBed - an accessor to remove a delay-node from the
 * free list and return to the active delay list.
 *
 * RETURN: NULL - if the free list is empty, else return
 *                a pointer to the removed node
 */
HIDDEN delayd_PTR allocBed (void) {
	if(openBeds_h == NULL) {
		return (NULL);
	} else {
		delayd_PTR cleanBed = openBeds_h;
		openBeds_h = cleanBed->d_next;

		/* Clean bed */
		cleanBed->d_next = NULL;
		cleanBed->d_wakeTime = (cpu_t) NULL;
		return (cleanBed);
	}
}

/*
 * searchBeds - an accessor used to find and return the predecessor/proper
 *		location of the desired semaphore, regardless of whether it exists yet.
 *
 * PARAM:	wakeTime - a pointer to a semaphore address.
 * RETURN:	the predecessor of the given semaphore address in the ASL.
 */
HIDDEN delayd_PTR searchBeds (int wakeTime) {
	delayd_PTR nomad = nextToWake_h;

	while(nomad->d_next->d_wakeTime < wakeTime) {
		nomad = nomad->d_next;
	}
	return (nomad);
}
