/*
 * pcb.c supports 3 different areas of functionality
 *		1) Free list management
 *		2) Queue management
 *		3) Process tree management (or Parent-child management)
 */

#include "../h/types.h"
#include "../h/const.h"

pcb_PTR pcbFree_h;

/* Insert the element onto the pcbFree list */
void freePcb (pcb_PTR p) {
	insertProcQ(pcbFree_h, p);
}

/*
 * Return NULL if pcbFree list is empty. Otherwise, remove an element from the pcbFree list,
 * provide initial values for ALL of the pcbs' fields (i.e. NULL and/or 0) and then return
 * a pointer to the removed element. Pcbs get reused, so it is important that no previous
 * value persist in a pcb when it gets reallocated.
 */
pcb_PTR allocPcb () {
	pcb_PTR gift = removeProcQ(pcbFree_h);
	/* Clean and rewrap present */
	(*gift) = EmptyPcb;

	pcbFree_h = gift->p_next;
	return gift;
}

/*
 * Initialize the pcbFree list to contain all the elements of the static array of MAXPROC pcbs.
 * This methods will be called only once during data structure initialization.
 */
void initPcbs () {
	static pcb_t procTable[MAXPROC];

	pcbFree_h = mkEmptyProcQ(); /* Init pcbFree list */

	for(int i=0; i<MAXPROC; i++) {
		freePcb(&(procTable[i]));
	}
}

/*
 * Initialize a variable to be the tail pointer to a process queue.
 * Return a pointer to the tail of an empty process queue; i.e. NULL.
 */
pcb_PTR mkEmptyProcQ () {
	return NULL;
}

/*
 * An accessor method returns TRUE if the queue is empty, FALSE otherwise
 */
int emptyProcQ (pcb_PTR tp) {
	return (tp==NULL);
}

/*
 * A mutator method that inserts a pcb node to linked list
 */
void insertProcQ (pcb_PTR *tp, pcb_PTR p) {
	if(emptyProcQ(*tp)) {
		(*tp) = p;
		p->p_next = p;
		p->p_prev = p;
	} else {
		/* link p to tail and head */
		p->p_next = (*tp)->p_next;
		p->p_prev = (*tp);
		/* add p behind current tail, and before head */
		(*tp)->p_next = p;
		p->p_next->p_prev = p;
		/* update tp */
		(*tp) = p;
	}
}

/*
 * A mutator method to remove and return the next node in the Queue
 */
pcb_PTR removeProcQ (pcb_PTR *tp) {
	if(emptyProcQ(*tp)) {
		return NULL;
	} else {
		/* Grab head */
		pcb_PTR head = (*tp)->p_next;
		/* Link tail to next head */
		(*tp)->p_next = head->p_next;
		/* Link the next head to tail */
		(*tp)->p_next->p_prev = (*tp);
		return head;
	}
}

/*
 * Mutator method to remove a specific node on the list and gives a pointer to the node
 * Return NULL if p does not exist, otherwise return p
 * TODO: Simplify logic here
 */
pcb_PTR outProcQ (pcb_PTR *tp, pcb_PTR p) {
	/* check if p exists in the list */
	if(emptyProcQ(*tp)) {
		return NULL;
	} else if((*tp) == p) {
		/* remove and return p, and update tp */
		(*tp)->p_next->p_prev = (*tp)->p_prev;
		(*tp)->p_prev->p_next = (*tp)->p_next;
		return (*tp);
	} else {
		/* traverse the list */
		pcb_PTR nomad = (*tp)->p_next; /* temp var to find p */

		while(nomad != NULL) {
			if(nomad == p) {
				/* nomad can finally rest */
				nomad->p_next->p_prev = nomad->p_prev;
				nomad->p_prev->p_next = nomad->p_next;
				return nomad;
			} else if(nomad == (*tp)) {
				/* p not found in list */
				return NULL;
			} else {
				nomad = nomad->p_next;
			}
		}
		/* Fell off the list. This should not happen */
		return NULL;
	}
}

/*
 * Accessor method to return the next to dequeue (the head node)
 */
pcb_PTR headProcQ (pcb_PTR tp) {
	if(emptyProcQ(tp)) {
		return (NULL);
	} else {
		return (tp->p_next);
	}
}

/* Tree management methods */
/* TO-DO: implement */
int emptyChild (pcb_PTR p) {
	return 1;
}

void insertChild (pcb_PTR prnt, pcb_PTR p) {

}

pcb_PTR removeChild (pcb_PTR prnt) {
	return NULL;
}

pcb_PTR outChild (pcb_PTR p) {
	return NULL;
}
