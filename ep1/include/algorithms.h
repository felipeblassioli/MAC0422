#include "simulator.h"
#include "event_queue.h"
#include "linked_list.h"

void fcfs_on_ready(ProcessControlBlock **, int, LinkedList *, LinkedList *, EventQueue *);
void fcfs_on_exit(ProcessControlBlock *, LinkedList *, LinkedList *, EventQueue *);

void sjf_on_ready(ProcessControlBlock **, int, LinkedList *, LinkedList *, EventQueue *);
void sjf_on_exit(ProcessControlBlock *, LinkedList *, LinkedList *, EventQueue *);

void srtn_on_ready(ProcessControlBlock **, int, LinkedList *, LinkedList *, EventQueue *);
void srtn_on_exit(ProcessControlBlock *, LinkedList *, LinkedList *, EventQueue *);

void rr_on_ready(ProcessControlBlock **, int, LinkedList *, LinkedList *, EventQueue *);
void rr_on_exit(ProcessControlBlock *, LinkedList *, LinkedList *, EventQueue *);
void rr_on_interrupt(ProcessControlBlock *, LinkedList *, LinkedList *, EventQueue *);
void rr_init(LinkedList *, LinkedList *, EventQueue *);

void ps_on_ready(ProcessControlBlock **, int, LinkedList *, LinkedList *, EventQueue *);
void ps_on_exit(ProcessControlBlock *, LinkedList *, LinkedList *, EventQueue *);

void hds_on_ready(ProcessControlBlock **, int, LinkedList *, LinkedList *, EventQueue *);
void hds_on_exit(ProcessControlBlock *, LinkedList *, LinkedList *, EventQueue *);
