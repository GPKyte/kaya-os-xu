/*
 * asl.c supports active semaphore list (ASL).
 */

#include "../h/const.h"
#include "../h/types.h"
#include "../e/pcb.e"

semd_PTR semdFree_h;

/*
 *
 */
HIDDEN void freeSemd (pcb_PTR p) {
	insertBlocked(semdFree_h, p);
}

/*
 *
 */
HIDDEN pcb_PTR allocSemd () {

}

/*
 * find the given sema4 desciptor and returns its the predecessor if it exists
 * in the list. If not, return NULL
 */
HIDDEN pcb_PTR searchSemd (int *semAdd) {


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
 *
 */
pcb_PTR removeBlocked (int *semAdd) {

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
	static semd_t semdTable[MAXPROC];

	semdFree_h = mkEmptyProcQ(); /* Init semdFree list */

	for(int i=0; i<MAXPROC; i++) {
		freeSemd(&(semdTable[i]));
	}
}
