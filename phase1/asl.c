/*
 * asl.c supports active semaphore list (ASL).
 */

#include "../h/const.h"
#include "../h/types.h"
#include "../e/pcb.e"

semd_PTR semdFree_h;
semd_PTR semd_h;

/*
 *
 */
HIDDEN void freeSemd (semd_PTR p) {
	insertBlocked(semdFree_h, p);
}

/*
 *
 */
HIDDEN pcb_PTR allocSemd ();

/*
 * find the given sema4 desciptor and returns its the predecessor if it exists
 * in the list. If not, return NULL
 */
HIDDEN pcb_PTR searchSemd (int *semAdd);

/*
 * Initialize the semdFree list to contain all the elements of the static array
 * of MAXPROC semaphroes. This method will be only called once during data structure
 * initialization.
 */
void initASL (){
	static semd_t semdTable[MAXPROC];

	semdFree_h = mkEmptyProcQ(); /* Init semdFree list */

	for(int i=0; i<MAXPROC; i++) {
		freeSemd(&(semdTable[i]));
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

}

/*
 * Search the ASL for a descriptor of this semaphore. If none is found, return NULL;
 * otherwise, remove the fisrt (i.e. head) ProcBlk from the process qurur of the found
 * semaphore descriptor and return a pointer to it. If the process queue for this
 * semaphore becomes empty (emptyProcQ(s_procq) is TRUE), remove the semaphore
 * descriptor from the ASL and return it to the semdFree list
 */
pcb_PTR removeBlocked (int *semAdd) {

}

/*
 * Remove the ProcBlk pointed to by p from the process queue associated with p's
 * semaphore (p-> p_semAdd) on the ASL. If ProcBlk pointed to by p does not appear
 * in the process queue as associted with p's semaphore, which is an error condition,
 * return NULL; otherwise, return p.
 */
pcb_PTR outBlocked (pcb_PTR p) {
  if(emptyProc(p)) {
    return NULL;
  }
}

/*
 * Return a pointer to the ProcBlk
 */
pcb_PTR headBlocked (int *semAdd) {
  headProcQ(semAdd);
}
