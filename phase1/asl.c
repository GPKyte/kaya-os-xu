/*
 * asl.c supports active semaphore list (ASL).
 */

#include "../h/const.h"
#include "../h/types.h"

#include "../e/pcb.e"

semd_PTR semdFree_h; /* pointer to the head of semdFree list */
semd_PTR semd_h; /* pointer to the active head list */

/*
 * Insert a semaphore onto the semdFree list
 */
HIDDEN void freeSemd (pcb_PTR p) {
	insertBlocked(semdFree_h, p);
}

/*
 * return NULL if semdFree list is empty. Otherwise, remove an element from the semdFree list,
 * and then return a pointer to the removed element.
 */
HIDDEN semd_PTR allocSemd (void) {

}

/*
 * find the given sema4 desciptor from active list and returns its predecessor if it exists
 * in the list. If not, return NULL
 */
HIDDEN semd_PTR searchSemd (int *semAdd) {
	semd_PTR nomad = semd_h;
	while ((nomad->s_next != NULL) && (semAdd != nomad->s_next->semAdd)) {
		nomad = nomad->s_next;
	}
	if (nomad->s_next == NULL) {
		return (NULL);
	} else {
		return (nomad);
	}
}

/*
 * A mutator to insert the ProcBlk, p, at the tail of the process queue
 * associated with the sema4 whose physical address is semAdd and set
 * the sema4 address of p to semAdd. If the sema4 is currently not active
 * (i.e. there is no descriptor for it in the ASL), allocate a new descriptor
 * from the semdFree list, insert it in the ASL (at the appropriate position),
 * initialize all of the fields (i.e. set s_semAdd to semAdd, and s_procq to
 * mkEmptyProcQ()), and proceed as above. If a new sema4 descriptor needs to be
 * allocated and the semdFree list is empty, return TRUE, otherwise return FALSE.
 */
int insertBlocked (int *semAdd, pcb_PTR p) {
	p->p_semAdd = semAdd;
	semd_PTR predecessor = searchSemd(semAdd);
	if(predecessor) {
		insertProcQ(predecessor->s_next->s_procQ, p);
		return FALSE;
	} else {
		semd_PTR fromFreeList = allocSemd();

		if(fromFreeList) {
			insertProcQ(fromFreeList->s_procQ, p);
		} else {
			/* Block adding new semd, we are out */
		}
		return TRUE;
	}
}

/*
 * Search the ASL for a descriptor of this semaphore. If none is found, return NULL;
 * otherwise, remove the first (i.e. head) ProcBlk from the process queue of the
 * found semaphore descriptor and return a pointer to it. If the process queue
 * for this semaphore becomes empty (emptyProcQ(s_procq) is TRUE), remove the
 * semaphore descriptor from the ASL and return it to the semdFree list.
 */
pcb_PTR removeBlocked (int *semAdd) {
	pcb_PTR result;
	semd_PTR predecessor = searchSemd(semAdd);
	if(predecessor == NULL || predecessor->s_next == NULL) {
		return (NULL);
	}	else {
		semd_PTR toRemoveFrom = predecessor->s_next;
		result = removeProqQ(toRemoveFrom->s_procQ); /* This may be NULL */
		if(emptyProcQ(toRemoveFrom->s_procQ)) {
			/* When ProcQ is empty, we clear up the semd */
			freeSemd(toRemoveFrom);
		}
		return (result);
	}
}

/*
 *
 */
pcb_PTR outBlocked (pcb_PTR p) {

}

/*
 *
 */
pcb_PTR headBlocked (int *semAdd) {

}

/*
 * Initialize the semdFree list to contain all the elements of the static array
 * of MAXPROC semaphroes. This method will be only called once during data structure
 * initialization.
 */
void initASL (){
	static semd_t semdTable[MAXPROC + 1]; /* +1 for dummy node at head */

	semdFree_h = mkEmptyProcQ(); /* Init semdFree list */

	for(int i=0; i<MAXPROC; i++) {
		freeSemd(&(semdTable[i]));
	}
}
