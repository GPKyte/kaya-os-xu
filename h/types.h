#ifndef TYPES
#define TYPES

/****************************************************************************
 *
 * This header file contains utility types definitions.
 *
 ****************************************************************************/


typedef signed int cpu_t;


typedef unsigned int memaddr;


typedef struct device_t {
	unsigned int d_status;
	unsigned int d_command;
	unsigned int d_data0;
	unsigned int d_data1;
} device_t;

#define t_recv_status		d_status
#define t_recv_command		d_command
#define t_transm_status		d_data0
#define t_transm_command	d_data1

#define DEVINTNUM 5
#define DEVPERINT 8
typedef struct {
	unsigned int rambase;
	unsigned int ramsize;
	unsigned int execbase;
	unsigned int execsize;
	unsigned int bootbase;
	unsigned int bootsize;
	unsigned int todhi;
	unsigned int todlo;
	unsigned int intervaltimer;
	unsigned int timescale;
	unsigned int inst_dev[DEVINTNUM];
	unsigned int interrupt_dev[DEVINTNUM];
	device_t devreg[DEVINTNUM * DEVPERINT];
} devregarea_t;

#define STATEREGNUM	31
typedef struct state_t {
	unsigned int	s_asid;
	unsigned int	s_cause;
	unsigned int	s_status;
	unsigned int	s_pc;
	int		s_reg[STATEREGNUM];
} state_t, *state_PTR;

#define	s_at	s_reg[0]
#define	s_v0	s_reg[1]
#define s_v1	s_reg[2]
#define s_a0	s_reg[3]
#define s_a1	s_reg[4]
#define s_a2	s_reg[5]
#define s_a3	s_reg[6]
#define s_t0	s_reg[7]
#define s_t1	s_reg[8]
#define s_t2	s_reg[9]
#define s_t3	s_reg[10]
#define s_t4	s_reg[11]
#define s_t5	s_reg[12]
#define s_t6	s_reg[13]
#define s_t7	s_reg[14]
#define s_s0	s_reg[15]
#define s_s1	s_reg[16]
#define s_s2	s_reg[17]
#define s_s3	s_reg[18]
#define s_s4	s_reg[19]
#define s_s5	s_reg[20]
#define s_s6	s_reg[21]
#define s_s7	s_reg[22]
#define s_t8	s_reg[23]
#define s_t9	s_reg[24]
#define s_gp	s_reg[25]
#define s_sp	s_reg[26]
#define s_fp	s_reg[27]
#define s_ra	s_reg[28]
#define s_HI	s_reg[29]
#define s_LO	s_reg[30]

#define MAXPROC	20
typedef struct pcb_t {
	/* process queue fields */
	struct pcb_t	*p_next,	/* pointer to next entry */
					*p_prev,	/* pointer to previous entry */

	/* process tree fields */
					*p_prnt,	/* pointer to parent */
					*p_child,	/* pointer to 1st child */
					*p_old,		/* pointer to next older sibling */
					*p_yng;		/* pointer to next younger sibling */

	/* state exception vector array [OLD/NEW][exceptionType] */
	state_t	*p_exceptionConfig[2][3];

	state_t 	p_s;		/* processor state */
	int 			*p_semAdd;	/* ptr to sema4 where pcb blocked */
	unsigned int p_CPUTime; /* total exec time in μ seconds */
} pcb_t, *pcb_PTR;

/* We use 49 sem's; 32normal + 2*8terminal (r/w) + 1timer */
#define MAXSEMS 49
typedef struct semd_t {
/* semaphore descriptor type */
	struct semd_t		*s_next;		/* next element on the ASL */
	int							*s_semAdd;		/* pointer to the semaphore */
	pcb_t						*s_procQ;		/* tail pointer to a process queue */
} semd_t, *semd_PTR;

/* Generic entry in page table */
typedef struct ptEntry_t {
	unsigned int entryHI; /* segNo - Virtual Page Number - asid */
	unsigned int entryLO; /* Physical Frame Number - N - D - V - G */
} ptEntry_t, *ptEntry_PTR;

/* User type page table */
#define MAXPTENTRIES 32
typedef struct uPgTable_t {
	int magicPtHeaderWord; /* Cache to ID object as PTbl and current entry # */
	ptEntry_t entries[MAXPTENTRIES];
} uPgTable_t, *uPgTable_PTR;

#define MAXOSPTENTRIES (2 * MAXPTENTRIES)
typedef struct osPgTable_t {

} osPgTable_t, *osPgTable_PTR;

#define MAXFRAMES 10 /* Less than 2 * MAXUPROC to force paging */
typedef struct fpTable_t {
	int indexOfLastFrameReplaced; /* TODO: Idea for cached info */
	/* Descriptor for frame entry in frame pool */
	/* Use same format as EntryHI + 0/1 for "in use" */
	unsigned int frames[MAXFRAMES];
	memaddr      frameAddr[MAXFRAMES]; /* TODO: Idea for cached info */
} fpTable_t;

#endif
