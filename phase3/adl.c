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
	pcb_PTR      d_occupants;
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



/*
 * delayProcess - a mutator to insert a process into a delay descriptor
 *   on the ordered ADL. The process should be the only process to sleep on
 *   this descriptor
 *
 * PARAM:  Time of Day to wake up proc, and proc to delay
 * RETURN: TRUE if successfully delayed, FALSE otherwise
 * TODO: Evaluate TRUE/FALSE mapping
 */
Bool delayProcess(cpu_t wakeTime, pcb_PTR sleepyProc) {
	delayd_PTR napNeighbor = searchBeds(wakeTime);
	delayd_PTR target;

	/* Verify if bed already in place or needs allocated */
	if(napNeighbor->d_next->d_wakeTime == wakeTime) {
		target = napNeighbor->d_next;

	} else {
		target = allocBed();

		if(target == NULL) {
			/* Allocation failed, all beds occupied */
			return (FALSE);

		} else {
			/* Init fields and insert new bed into ADL */
			target->d_wakeTime = wakeTime;
			target->d_next = neighborNapper->d_next;
			neighborNapper->d_next = target;
		}
	}

	insertProcQ(&(target->d_occupants), sleepyProc);
	return (TRUE);
}

/********************** Helper Methods *********************/
HIDDEN void freeBed(delayd_t bed) {
	bed->d_next = openBeds_h;
	openBeds_h = bed;
}

HIDDEN delayd_PTR allocBed (void) {
	if(freeBed_h == NULL) {
		return (NULL);
	} else {
		/* Take bed and update available bed list */
		delayd_PTR cleanBed = freeBed_h;
		freeBed_h = cleanBed->d_next;

		/* Clean bed */
		cleanBed->d_next = NULL;
		cleanBed->d_wakeTime = NULL;
		cleanBed->d_occupants = mkEmptyProcQ();
		return (cleanBed);
	}
}

/*
 * searchBeds - an accessor used to find and return the predecessor/proper
 *		location of the desired Bed
 *		regardless of whether it exists yet.
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
