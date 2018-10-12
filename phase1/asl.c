/*
 * asl.c supports active semaphore list (ASL) and its operations.
 *
 * AUTHORS: Gavin Kyte & Ploy Sithisakulrat
 * CONTRIBUTOR/ADVISOR: Michael Goldweber
 * DATE PUBLISHED: 9.24.2018
 * LAST UPDATED: 10.8.2018
 */

#include "../h/const.h"
#include "../h/types.h"

#include "../e/pcb.e"
#include "../e/asl.e"


/* Prototypes */
HIDDEN void freeSemd (semd_PTR);
HIDDEN semd_PTR allocSemd(void);
HIDDEN semd_PTR searchSemd(int*);

semd_PTR semdFree_h; /* pointer to the head of semdFree list */
semd_PTR semd_h; /* pointer to the active head list */

/*
 * freeSemd - A mutator used to insert a sema4 descriptor into semdFree list
 * PARAM:	s - semaphore descriptor to be added to the free list.
 */
HIDDEN void freeSemd (semd_PTR s) {
	s->s_next = semdFree_h;
	semdFree_h = s;
}

/*
 * allocSemd - an accessor used to remove a semaphore descriptor from
 *		the free list (to add to active list).
 *
 * Returns NULL if semdFree list is empty.
 * Otherwise, remove an element from the semdFree list, and then return
 * a pointer to the removed element.
 *
 * Note: it is up to the caller to add the Semd to the ASL when applicable
 * RETURN:	NULL - if the semdFree list is empty; else
 *		a pointer to the removed semaphore descriptor.
 */
HIDDEN semd_PTR allocSemd (void) {
	if(semdFree_h == NULL) {
		return (NULL);
	} else {
		semd_PTR gift = semdFree_h;
		semdFree_h = gift->s_next;

		/* Clean semd */
		gift->s_next = NULL;
		gift->s_procQ = mkEmptyProcQ();
		gift->s_semAdd = NULL;
		return (gift);
	}
}

/*
 * searchSemd - an accessor used to find and return the predecessor/proper
 *		location of the desired semaphore, regardless of whether it exists yet.
 *
 * PARAM:		*semAdd - a pointer to a semaphore address.
 * RETURN:	the predecessor of the given semaphore address in the ASL.
 */
HIDDEN semd_PTR searchSemd (int *semAdd) {
	semd_PTR nomad = semd_h;

	while(nomad->s_next->s_semAdd < semAdd) {
		nomad = nomad->s_next;
	}
	return (nomad);
}

/*
 * insertBlocked - a mutator to insert the ProcBlk at the tail of the process
 *		queue associated with the semaphore whose physical address is semAdd
 * 		and set the semaphore address of p to semAdd.
 *
 * PARAM:	*semAdd - a pointer to a semaphore address
 *		p - a pointer to a process block to be inserted to process queue
 * RETURN:	TRUE if a new semaphore descriptor needs to be allocated and
 *		the free list is empty. Otherwise, return FALSE
 */
int insertBlocked (int *semAdd, pcb_PTR p) {
	semd_PTR predecessor = searchSemd(semAdd);
	semd_PTR target; /* object to insert p into */

	/* Verify if sema4 already in place or needs allocated
	 * If a new semaphore descriptor needs to be allocated and the semdFree
	 * list is empty, return TRUE, otherwise return FALSE. */
	if(predecessor->s_next->s_semAdd == semAdd) {
		target = predecessor->s_next;
	} else {
		target = allocSemd();
		if(target == NULL) {
			/* Allocation failed, empty free list */
			return (TRUE);
		} else {
			/* Init fields and insert new semd into ASL */
			target->s_procQ = mkEmptyProcQ();
			target->s_semAdd = semAdd;
			target->s_next = predecessor->s_next;
			predecessor->s_next = target;
		}
	}

	p->p_semAdd = semAdd;
	insertProcQ(&(target->s_procQ), p);
	return (FALSE);
}

/*
 * removeBlocked - a mutator to remove the descriptor for given semaphore
 *		from the ASL
 *
 * If none is found, return NULL; otherwise, remove the first (i.e. head) PCB
 * from the procQueue of the found sema4 descriptor and return its reference
 * If the process queue for this semaphore becomes empty, remove the semaphore
 * descriptor from the ASL and return it to the semdFree list.
 *
 * PARAM:		*semAdd - a pointer to a semaphore address
 * RETURN: 	a head pointer to a removed ProcBlc from the process queue
 *		of the found semaphore; NULL if the semaphore is not found.
 */
pcb_PTR removeBlocked (int *semAdd) {
	pcb_PTR result;
	semd_PTR predecessor = searchSemd(semAdd);
	if(predecessor->s_next->s_semAdd == semAdd) {
		semd_PTR target = predecessor->s_next;

		result = removeProcQ(&(target->s_procQ)); /* This should NOT be NULL */
		/* When ProcQ is empty, we clear up the semd */
		if(emptyProcQ(target->s_procQ)) {
			predecessor->s_next = target->s_next;
			freeSemd(target);
		}

		return (result);
	} else {
		return (NULL);
	}
}

/*
 * outBlocked - a mutator to remove the ProcBlk pointed to by p from the
 *		process queue associated with p’s semaphore (p→ p semAdd) on the ASL.
 *
 * If ProcBlk pointed to by p
 * does not appear in the process queue associated with p’s semaphore,
 * which is an error condition, return NULL; otherwise, return p.
 *
 * PARAM:		p - a process block in a process queue associated with its
 *		semaphore on the active list.
 * RETURN:	p if found; otherwise, return NULL (error)
 */
pcb_PTR outBlocked (pcb_PTR p) {
	semd_PTR predecessor = searchSemd(p->p_semAdd);

	if(predecessor->s_next->s_semAdd == p->p_semAdd) {
		/* Yay. Let outProcQ return successfully or report the error w/ NULL */
		return (outProcQ( &(predecessor->s_next->s_procQ), p));
	} else {
		/* p's associated semd is missing from ASL */
		return (NULL);
	}
}

/*
 * headBlocked - an accessor to return a pointer to the ProcBlk that is at
 *		the head of the process queue associated with the semaphore semAdd.
 *
 * Return NULL if semAdd is not found on
 *		the ASL or if the process queue associated with semAdd is empty.
 *
 * PARAM:		*semAdd - a pointer to a semaphore address
 * RETURN:	the pointer to the process block if found; otherwise, return NULL.
 */
pcb_PTR headBlocked (int *semAdd) {
	semd_PTR predecessor = searchSemd(semAdd);

	if(predecessor->s_next->s_semAdd == semAdd) {
		return (headProcQ(predecessor->s_next->s_procQ));
	} else {
		return (NULL);
	}
}

/*
 * initAS: - a method used to initialize the semdFree list to contain
 *		all the elements of the static array of MAXPROC semaphores.
 *
 * This method will be only called once during data structure initialization.
 */
void initASL (void) {
	int i;
	static semd_t semdTable[MAXPROC + 2]; /* +2 for dummy nodes */

	semdFree_h = mkEmptySemdList(); /* Init semdFree list */

	for(i=0; i < MAXPROC; i++) {
		freeSemd(&(semdTable[i]));
	}

	/* Set ASL dummy nodes */
	semdTable[MAXPROC].s_semAdd = 0;
	semd_h = &(semdTable[MAXPROC]);
	semdTable[MAXPROC + 1].s_semAdd = MAXINT;
	semd_h->s_next = &(semdTable[MAXPROC + 1]);
	semd_h->s_next->s_next = NULL;
}
