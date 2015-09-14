#ifndef __INCL_SCHEDULER
#define __INCL_SCHEDULER

#include "simulator.h"
#include "linked_list.h"

#define RTIME_INFINITY 9999.999
double remaining_time(ProcessControlBlock *);

typedef struct {
	LinkedList *ready_processes;
	LinkedList *running_processes;
	LinkedList *finished_processes;
} ProcessControlTable;

void scheduler_start(int);
void scheduler_stop();
void scheduler_join();

void scheduler_new(TraceInfo **, int);
void scheduler_ready(ProcessControlBlock **, int);
void scheduler_dispatch(ProcessControlBlock *, int);
void scheduler_interrupt(ProcessControlBlock *);
void scheduler_exit(ProcessControlBlock *);
#endif
