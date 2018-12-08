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
	int          d_procQ;
	unsigned int d_wakeTime;
}


/********************* External Methods ********************/




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
