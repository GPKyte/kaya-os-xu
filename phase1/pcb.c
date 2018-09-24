/*
 * pcb.c supports 3 different areas of functionality
 *		1) Free list management
 *		2) Queue management
 *		3) Process tree management (or Parent-child management)
 *
 * AUTHORS: Gavin Kyte & Ploy Sithisakulrat
 * CONTRIBUTOR/ADVISOR: Michael Goldweber
 * DATE PUBLISHED: 9.24.2018
 */

#include "../h/types.h"
#include "../h/const.h"

#include "../e/pcb.e"

pcb_PTR pcbFree_h; /* head pointer to the pcbFree list */

void debugA (int a, int b) {
	int i;
	i = a + b;
	i++;
}

/* 
 * cleanPcb - used to set up default states of variables
 * PARAM:	p - pointer to a process block
 */
HIDDEN void cleanPcb(pcb_PTR p) {
	int i;

	/* TO-DO: clarify what default state should be */
	/* Set state fields */
	p->p_s.s_asid = 0;
	p->p_s.s_cause = 0;
	p->p_s.s_status = 0;
	p->p_s.s_pc = 0;
	i = 0;
	while(i < STATEREGNUM) {
		p->p_s.s_reg[i] = 0;
		i++;
	}

	p->p_next = NULL;
	p->p_prev = NULL;
	p->p_prnt = NULL;
	p->p_child = NULL;
	p->p_old = NULL;
	p->p_yng = NULL;
	p->p_semAdd = NULL;
}

/* 
 * freePcb - a mutator used to insert the element onto the pcbFree list.
 * PARAM:	p - a pcb pointer to be added to the pcbFree list.
 */
void freePcb (pcb_PTR p) {
	insertProcQ(&pcbFree_h, p);
}

/*
 * allocPcb - a mutator to remove an element from the pcbFreelist, provide initial values 
 * for ALL of the pcbs' fields (i.e. NULL and/or 0) and then return a pointer to the removed element. 
 * Pcbs get reused, so it is important that no previous value persist in a pcb when it gets reallocated.
 * RETURN: 	NULL if the pcbFree list is empty; otherwise, return a pointer to the removed element from pcbFree list.
 */
pcb_PTR allocPcb (void) {
	pcb_PTR gift = removeProcQ(&pcbFree_h); /* Update of tail pointer handled by method */
	
	if(gift != NULL) { 
		cleanPcb(gift); 
	}
	return (gift);
}

/*
 * initPCBs - initialize the pcbFree list to contain all the elements of the static array of MAXPROC pcbs.
 * This methods will be called only once during data structure initialization.
 */
void initPcbs (void) {
	static pcb_t procTable[MAXPROC];
	int i = 0;

	pcbFree_h = mkEmptyProcQ(); /* Init pcbFree list */

	while(i < MAXPROC) {
		freePcb(&(procTable[i]));
		i++;
	}
}

/*
 * mkEmptyProcQ - an accessor initializes a variable to be the tail pointer to a process queue.
 * RETURN: 	a tail pointer of an empty process queue, i.e. NULL.
 */
pcb_PTR mkEmptyProcQ (void) {
	return (NULL);
}

/*
 * emptyProcQ - an accessor method to check whether the process queue is empty
 * RETURN:	TRUE if the queue is empty, FALSE otherwise.
 */
int emptyProcQ (pcb_PTR tp) {
	return (tp == NULL);
}

/*
 * insertProcQ - a mutator method to insert a process block into the process queue whose
 * tail pointer is pointed to by tp. Note: the double indirection through tp to allow for
 * the possible updating of the tail pointer as well.
 * PARAM:	*tp - a tail pointer of the process queue.
 * 		p - a process block to be inserted to process queue.
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
 * removeProcQ - a mutator method to remove and return the first (head) element from the proceses queue.
 * PARAM:	*tp - tail pointer to the process queue.
 * RETURN:	NULL if process queue is initally empty. Otherwise, return the first element that has been removed.
 */
pcb_PTR removeProcQ (pcb_PTR *tp) {
	if(emptyProcQ(*tp)) {
		return (NULL);
	} else {
		/* Grab head */
		pcb_PTR head = (*tp)->p_next;
		if((*tp) == head) {
			/* Handle case of disappearing snake */
			(*tp) = NULL;
		} else {
			/* Link tail to next head */
			(*tp)->p_next = head->p_next;
			/* Link the next head to tail */
			(*tp)->p_next->p_prev = (*tp);
		}

		return (head);
	}
}

/*
 * outProcQ - a mutator method to remove a specific node on the list and
 * update the tail pointer of the process queue.
 * Note that p can point to any element of the process queue.
 * PARAM:	*tp - tail pointer to the process queue.
 * 		p - the desired node to be removed from the process queue.
 * RETURN:	NULL if p does not exist, otherwise return p.
 */
pcb_PTR outProcQ (pcb_PTR *tp, pcb_PTR p) {
	/* check if p exists in the list */
	if(emptyProcQ(*tp)) {
		return (NULL);
	} else if((*tp) == p && (*tp)->p_next == (*tp) && (*tp)->p_prev == (*tp)) {
		/* Last node in queue */
		(*tp) = NULL;
		return(p);
	} else if((*tp) == p) {
		/* remove and return p, and update tp */
		(*tp)->p_next->p_prev = (*tp)->p_prev;
		(*tp)->p_prev->p_next = (*tp)->p_next;
		(*tp) = (*tp)->p_prev;
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
				return (NULL);
			} else {
				nomad = nomad->p_next;
			}
		}
		/* Fell off the list. This should not happen */
		return (NULL);
	}
}

/*
 * headProcQ - an accessor method to return the next to dequeue (the head node).
 * PARAM:	tp - tail pointer to the process queue.
 * RETURN:	NULL if the process queue is empty, otherwise return the head node.
 */
pcb_PTR headProcQ (pcb_PTR tp) {
	if(emptyProcQ(tp)) {
		return (NULL);
	} else {
		return (tp->p_next);
	}
}

/* 
 * Process Trees Management Methods
 */

/* emptyChild - an accessor methos to check whether or not the node pointerd by p has a child.
 * PARAM:	p - a process tree node.
 * RETURN:	TRUE if the ProcBlk pointed to by p has no children. Return FALSE otherwise. 
 */
int emptyChild (pcb_PTR p) {
	return (p == NULL || p->p_child == NULL);
}

/* insertChild - a mutator to make the ProcBlk pointed to by p a child of the ProcBlk pointed to by prnt. 
 * PARAM:	prnt - a parent node.
 * 		p - a node to make it a child.
 */
void insertChild (pcb_PTR prnt, pcb_PTR p) {
	p->p_prnt = prnt;
	if(emptyChild(prnt)) {
		prnt->p_child = p;
		/* Force clean NULL border, but don't worry about children */
		p->p_old = NULL;
		p->p_yng = NULL;
	} else {
		p->p_old = prnt->p_child;
		p->p_yng = NULL; /* Border control */
		p->p_old->p_yng = p;
		prnt->p_child = p;
	}
}

/*
 * removeChild - a mutator method to make the first child of the ProcBlk pointed to by prnt no longer a child of prnt.
 * PARAM:	prnt - a parent node.
 * RETURN:	NULL if initially there were no children of prnt.
 * 		Otherwise, return a pointer to this removed first child ProcBlk.
 */
pcb_PTR removeChild (pcb_PTR prnt) {
	if(emptyChild(prnt)) {
		return (NULL);
	} else {
		return (outChild(prnt->p_child));
	}
}

/*
 * outChild - a mutator to make the ProcBlk pointed to by p no longer the child of its parent.
 * PARAM:	p - a child node
 * RETURN:	NULL if the ProcBlk pointed to by p has no parent; otherwise, return p.
 * Note that the element pointed to by p need not be the first child of its parent.
 */
pcb_PTR outChild (pcb_PTR p) {
	if(p == NULL || p->p_prnt == NULL || p->p_prnt->p_child == NULL) {
		/* Trolling; invalid input/state */
		return (NULL);
	} else if(p->p_prnt->p_child == p) {
		/* First Child */
		p->p_prnt->p_child = p->p_old;
		if(p->p_old != NULL) { p->p_old->p_yng = NULL; }
	} else {
		/* Middle or last child */
		if(p->p_old != NULL) { p->p_old->p_yng = p->p_yng; } /* In case sibling is NULL */
		p->p_yng->p_old = p->p_old;
	}

	/* Clean up orphan */
	p->p_prnt = NULL;
	p->p_old = NULL;
	p->p_yng = NULL;
	return (p);
}
