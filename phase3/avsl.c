/************************* AVSL.C **************************
 * AVSL module implements the Active Virtual Semaphore List
 * module, where the list is managed as a double circularly
 * linked list, and the free list is managed as a single
 * linearly linked list. It acts as the virtual P and V service.
 *
 * Similar to ASL, the AVSL is used to keep track of blocked
 * user processes when the P operation is called on that
 * specific process. For a U-Proc to be blocked, P operation
 * is to be performed on the U-Proc's sema4, and a virtSemd-node
 * needs to be allocated from the free list into the AVSL.
 * For a U-Proc to be unblocked, a search is made on the AVSL
 * for a virtSemd-node (vs_semd)
 *
 * AUTHORS: Ploy Sithisakulrat & Gavin Kyte
 * ADVISOR/CONTRIBUTER: Michael Goldweber
 * DATE PUBLISHED:
 ***********************************************************/

#include "../h/const.h"
#include "../h/types.h"

#include "../e/pcb.e"
#include "../e/initProc.e"
#include "../e/adl.e"
#include "../e/vmIOsupport.e"

virtSemd_PTR openRoutes_h; /* pointer to the head of virtSemdFree list */
virtSemd_PTR trafficJam_h; /* pointer to the head of AVSL;
                              holds blocked U-Proc's*/

/************************ Prototypes ***********************/
HIDDEN void freeTraffic (virtSemd_PTR car);
HIDDEN virtSemd_PTR allocTraffic (void);
HIDDEN virtSemd_PTR searchTraffic (int *nextToRelease, virtSemd_PTR vs);
void addCarToTraffic (virtSemd_PTR *head, virtSemd_PTR vs);
virtSemd_PTR removeCarFromTraffic (virtSemd_PTR *head);

/********************* External Methods ********************/
/*
 * initAVSL - initializes the Active Virtual Semaphore List
 * (AVSL) to store blocked user processes
 */
void initAVSL (void) {
  int i;
  static virtSemd_t accidents[MAXPROC + 2]; /* +2 for dummy nodes */

  openRoutes_h = NULL; /* Init virtSemdFree list */

  for(i=0; i < MAXPROC; i++) {
  freeTraffic(&(accidents[i]));
  }

  /* Set crash boundary for AVSL */
  accidents[MAXPROC].vs_semd = 0;
  trafficJam_h = &(accidents[MAXPROC]);
  accidents[MAXPROC + 1].vs_semd = MAXINT;
  trafficJam_h->vs_next = &(accidents[MAXPROC + 1]);
  trafficJam_h->vs_next->vs_next = NULL;
}

/*
 * anotherCrash - a mutator method to insert/block a U-proc
 * sema4 descriptor into the ordered double circularly
 * Active Virtual Sema4 Linked list (once the P op is called)
 *
 * PARAM: driverID - a pointer to the virtSemd address
 *        asid - an id for a blocked U-Proc to be added to AVSL
 * RETURN:  TRUE - if a U-proc is blocked successfully on the AVSL.
 *          Otherwise, return FALSE.
 */
Bool anotherCrash (int *driverID, int asid, virtSemd_PTR vs) {
  /* search for appropriate location to insert the blocked vs */
  virtSemd_PTR nearbyDriver = searchTraffic(driverID, vs);
  virtSemd_PTR target;

  if(nearbyDriver->vs_prev->vs_semd == driverID && nearbyDriver->vs_prev->vs_asid == asid) {
    target = nearbyDriver->vs_prev;
  } else {
    /* get vs from the free list */
    target = allocTraffic();

    if(target == NULL) {
      /* check if freelist is empty, if yes return FALSE */
      return (FALSE);
    } else {
      /* allocate the blocked vs to AVSL */
      target->vs_asid = asid;
      target->vs_semd = driverID;
      target->vs_prev = nearbyDriver->vs_prev;
      nearbyDriver->vs_prev = target;
    }
  }

  vs->vs_semd = driverID;
  addCarToTraffic(&target, vs);
  return (TRUE);
}

/*
 * clearCrash - a mutator method to remove/unblock the requested
 * virtual semaphore from the AVSL.
 *
 * PARAM: *driverID - a pointer to virtSemd address
 *        vs - a pointer to virtSemd-node to be removed from AVSL
 * RETURN: a tail pointer to a removed virtual sema4. Otherwise, NULL
 */
virtSemd_PTR clearCrash (int *driverID, virtSemd_PTR vs) {
  /* search for a vs with a matching vs_semd */
  virtSemd_PTR debris;
  virtSemd_PTR nearbyDriver = searchTraffic(driverID, vs); /* head pointer */

  if(nearbyDriver->vs_prev->vs_semd == driverID) {
    virtSemd_PTR target = nearbyDriver->vs_prev;

    debris = removeCarFromTraffic(&target); /* should not be NULL */
    freeTraffic(debris); /* add the removed node to free list */

    return (debris);
  } else {
    return (NULL);
  }
}

/********************** Helper Methods *********************/
/*
 * freeTraffic - a mutator used to insert a virtual sema4
 * descriptor into the virtual sema4 free list
 *
 * PARAM: car - a pointer to a virtual sema4 needed to be added
 *             to the virtSemdFree List
 */
HIDDEN void freeTraffic (virtSemd_PTR car) {
  car->vs_next = openRoutes_h;
  openRoutes_h = car;
}

/*
 * allocTraffic - an accessor to remove a virtual semaphore
 * from the free list (add to the active list)
 *
 * RETURN:  NULL - if the virtSemdFree List is empty; else
 *                 a pointer to the removed virtual semaphore
 */
HIDDEN virtSemd_PTR allocTraffic (void) {
  if(openRoutes_h == NULL) {
    return (NULL);
  } else {
    virtSemd_PTR car = openRoutes_h;
    openRoutes_h = car->vs_next;

    /* clear traffic */
    car->vs_next = NULL;
    car->vs_semd = NULL;
    return (car);
  }
}

/*
 * searchTraffic - an accessor to find a virtual semaphore
 * and return the predecessor/proper location of the desired
 * virtual semaphore, regardless of whether it exists yet.
 *
 * PARAM: nextToRelease - a pointer to a virtSemd address
 *        vs - a pointer to a virtSemd-node
 */
HIDDEN virtSemd_PTR searchTraffic (int *nextToRelease, virtSemd_PTR vs) {
  virtSemd_PTR crash = trafficJam_h;

  while(crash->vs_next->vs_semd < nextToRelease && crash->vs_semd == vs->vs_semd) {
    crash = crash->vs_next;
  }
  return (crash);
}

/*
 * addCarToTraffic - a mutator method to add a virtual semaphore to
 * a double circularly linked list (AVSL)
 *
 * PARAM: head - a head pointer to the linked list
 *        vs - a pointer to a virtual semaphore
 */
void addCarToTraffic (virtSemd_PTR *head, virtSemd_PTR vs) {
  if((*head) == NULL) {
    /* if the linked list is empty */
    (*head) = vs;
    vs->vs_prev = vs;
    vs->vs_next = vs;
  } else {
    /* link vs to tail and head */
    vs->vs_prev = (*head)->vs_prev;
    vs->vs_next = (*head);

    /* add vs in front of current head, and before tail */
    (*head)->vs_prev = vs;
    vs->vs_prev->vs_next = vs;

    /* update head */
    (*head) = vs;
  }
}

/*
 * removeCarFromTraffic - a mutator method to remove and return
 * the the head pointer from the linked list.
 *
 * PARAM: *head - head pointer to the linked list
 * RETURN: NULL if linked list is initially empty.
 *         Otherwise, return the element removed
 */
virtSemd_PTR removeCarFromTraffic (virtSemd_PTR *head) {
  if((*head) == NULL) {
    return (NULL);
  } else {
    /* grab tail */
    virtSemd_PTR tail = (*head)->vs_prev;

    if((*head) == tail) {
      (*head) == NULL;
    } else {
      /* get rid of tail */
      (*head)->vs_prev = tail->vs_prev;
      (*head)->vs_prev->vs_next = (*head);
    }

    return (tail);
  }
 }
