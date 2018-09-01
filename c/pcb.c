/*
 * Insert the element onto the pcbFree list
 */
void freePcb (pcb_PTR p) {

} 

/* 
 * 
 */
pcb_PTR allocPcb () {

}

/*
 * 
 */
void initPcbs () {

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
		p->next = p;
		p->prev = p;
	} else {
		/* link p to tail and head */
		p->next = (*tp)->next;
		p->prev = (*tp);
		/* add p behind current tail, and before head */
		(*tp)->next = p;
		p->next->prev = p;
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
		pcb_PTR head = (*tp)->next; 
		/* Link tail to next head */
		(*tp)->next = head->next;
		/* Link the next head to tail */
		(*tp)->next->prev = (*tp);
		return head;
	}
}

/*
 * Mutator method to remove a specific node on the list and gives a pointer to the node
 * Return NULL if p does not exist, otherwise return p
 * TODO: Simplify logic here
 */
pcb_PTR outProcQ (pcb_PTR *tp, pcb_PTR p) {
	// check if p exists in the list
	if(emptyProcQ(*tp)) {
		return NULL;
	} else if((*tp) == p) {
		// remove and return p, and update tp
		(*tp)->next->prev = (*tp)->prev;
		(*tp)->prev->next = (*tp)->next;
		return (*tp);
	} else {
		// traverse the list
		pcb_PTR nomad = (*tp)->next; //temp var to find p
		while(nomad != NULL) {
			if(nomad == p) {
				/* nomad can finally rest */
				nomad->next->prev = nomad->prev;
				nomad->prev->next = nomad->next;
				return nomad;
			} else if(nomad == (*tp)) {
				/* p not found in list */
				return NULL;
			} else {
				nomad = nomad->next;
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
		return (tp->next);
	}
}

/* Tree management methods */
int emptyChild (pcb_PTR p);
void insertChild (pcb_PTR prnt, pcb_PTR p);
pcb_PTR removeChild (pcb_PTR p);
pcb_PTR outChild (pcb_PTR p);
