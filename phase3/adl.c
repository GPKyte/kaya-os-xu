/************************** ADL.C **************************
 *
 *
 *
 *
 *
 *
 *
 *
 *
 * AUTHORS: Ploy Sithisakulrat & Gavin Kyte
 * ADVISOR/CONTRIBUTER: Michael Goldweber
 * DATE PUBLISHED:
 ***********************************************************/

#include

delayd_PTR openBeds_h;
delayd_PTR nextToWake_h;

/************************ Prototypes ***********************/

typedef struct delayd_t {
	delayd_PTR   d_next;
	pcb_PTR      d_occupant;
	cpu_t        d_wakeTime;
}


/********************* External Methods ********************/


void initADL(void) {
	int i;
	static semd_t bedFactory[MAXPROC + 2]; /* +2 for dummy nodes */

	openBeds_h = NULL; /* Init delay list */

	for(i = 0; i < MAXPROC; i++) {
		freeBed(&(bedFactory[i]));
	}

	/* Set boundary beds for ADL to simplify condition checking */
	bedFactory[MAXPROC].s_wakeTime = 0;
	nextToWake_h = &(bedFactory[MAXPROC]);
	bedFactory[MAXPROC + 1].d_wakeTime = (cpu_t) MAXINT;
	nextToWake_h->d_next = &(bedFactory[MAXPROC + 1]);
	nextToWake_h->d_next->d_next = NULL;
}

void summonSandmanTheDelayDemon() {
	cpu_t alarmTime;
	int *semAdd;
	delayd_PTR napNeighbor;

	while(TRUE) {
		STCK(alarmTime);

		/* Find beds to wake up */
		while(nextToWake_h->d_wakeTime <= alarmTime) {
			/* Wake up processes by calling V on their semaphore */
			semAdd = &(uProcList[nextToWake_h->d_asid - 1]->up_syncSem);
			SYSCALL(VMVERHOGEN, semAdd); /* Wake up proc */

			/* Kick out proc */
			napNeighbor = nextToWake_h->d_next;
			freeBed(nextToWake_h);
			nextToWake_h = napNeighbor;
		}

		/* Take micronap */
		SYSCALL(WAITCLOCK);
	}
}

/*
 * delayProcess - a mutator to insert a process into a delay descriptor
 *   on the ordered ADL. The process should be the only process to sleep on
 *   this descriptor
 *
 * PARAM:  Time of Day to wake up proc, and proc to delay
 * RETURN: TRUE if successfully delayed, FALSE otherwise
 */
Bool delayProcess(cpu_t wakeTime, pcb_PTR sleepyProc) {
	delayd_PTR neighborNapper = searchBeds(wakeTime);
	delayd_PTR target = allocBed();

	if(target == NULL) {
		/* Allocation failed, all beds occupied TODO: this shouldn't be a concern.. ?*/
		return (FALSE);
	} else {
		/* Init fields and insert new bed into ADL */
		target->d_occupant = sleepyProc;
		target->d_wakeTime = wakeTime;
		target->d_next = neighborNapper->d_next;
		neighborNapper->d_next = target;
	}

	return (TRUE);
}

/********************** Helper Methods *********************/
HIDDEN void freeBed(delayd_t bed) {
	bed->d_next = openBeds_h;
	openBeds_h = bed;
}

HIDDEN delayd_PTR allocBed (void) {
	if(bedFree_h == NULL) {
		return (NULL);
	} else {
		delayd_PTR cleanBed = bedFree_h;
		bedFree_h = cleanBed->d_next;

		/* Clean bed */
		cleanBed->d_next = NULL;
		cleanBed->d_procQ = mkEmptyProcQ();
		cleanBed->d_wakeTime = NULL;
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
