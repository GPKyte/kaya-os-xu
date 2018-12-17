/************************** ADL.C **************************
 * ADL module implements the Active Delay List module.
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

 #include "../h/const.h"
 #include "../h/types.h"

 #include "../e/pcb.e"
 #include "../e/initProc.e"
 #include "../e/vmIOsupport.e"

delayd_PTR openBeds_h;
delayd_PTR nextToWake_h;

/************************ Prototypes ***********************/

/********************* External Methods ********************/


void initADL(void) {
	int i;
	static delayd_t bedFactory[MAXUPROC + 2]; /* +2 for dummy nodes */

	openBeds_h = NULL; /* Init delay list */

	for(i = 0; i < MAXUPROC; i++) {
		freeBed(&(bedFactory[i]));
	}

	/* Set boundary beds for ADL to simplify condition checking */
	bedFactory[MAXUPROC].s_wakeTime = 0;
	nextToWake_h = &(bedFactory[MAXUPROC]);
	bedFactory[MAXUPROC + 1].d_wakeTime = (cpu_t) MAXINT;
	nextToWake_h->d_next = &(bedFactory[MAXUPROC + 1]);
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
