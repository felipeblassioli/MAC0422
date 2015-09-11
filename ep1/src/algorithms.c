#include <stdio.h>

#include "simulator.h"
#include "event_queue.h"
#include "linked_list.h"

#include <semaphore.h>
#include <pthread.h>

/* COMMON BEGIN */
extern int g_total_cpus;
extern sem_t g_available_processors;

static int get_non_used_cpu(LinkedList *running_processes){
	int i;
	int seen[MAX_CPU_NUMBER];
	LinkedList *q = running_processes;
	LLNode *cur;

	for(i=0; i<g_total_cpus; i++) seen[i] = 0;

	pthread_mutex_lock(&q->mutex);
	cur = q->head;
	while(cur->next){
		cur = cur->next;
		seen[cur->data->cpu] = 1;
	}
	pthread_mutex_unlock(&q->mutex);
	for(i=0; i<g_total_cpus && seen[i]; i++);
	return i == g_total_cpus? -1 : i;
}

typedef ProcessControlBlock *(*t_choosefunc)(LinkedList *, LinkedList *);

void nonpreemptive_dispatch(LinkedList *ready_processes, LinkedList *running_processes, EventQueue *event_queue, void *choose_func){
	t_choosefunc choose = choose_func;

	ProcessControlBlock *proc;
	int cpu;
	LinkedList _ready_processes;
	LinkedList _running_processes;

	ll_copy(&_ready_processes, ready_processes);
	ll_copy(&_running_processes, running_processes);
			
	cpu = get_non_used_cpu(&_running_processes);
	while(cpu != -1){
		proc = choose(&_ready_processes, &_running_processes);
		proc->cpu = cpu;
		ll_remove(&_ready_processes, proc->id);
		ll_insert_beginning(&_running_processes, proc);

		enqueue(event_queue, EVT_DISPATCH, (void *)proc);

		if(_ready_processes.size == 0)
			break;
		cpu = get_non_used_cpu(&_running_processes);
	}
	ll_destroy(&_ready_processes);
	ll_destroy(&_running_processes);
}

void nonpreemptive_dispatch2(LinkedList *ready_processes, LinkedList *running_processes, EventQueue *event_queue, void *choose_func){
	t_choosefunc choose = choose_func;
	ProcessControlBlock *proc;

	if(ready_processes->size > 0){
		proc = choose(ready_processes, running_processes); 
		proc->cpu = get_non_used_cpu(running_processes);
		enqueue(event_queue, EVT_DISPATCH, (void *)proc);
	}
}

typedef int (*t_ltfunc)(ProcessControlBlock *, ProcessControlBlock *);

ProcessControlBlock *choose_min(LinkedList *q, LinkedList *running_processes, void *lt_func){
	t_ltfunc lt = lt_func;

	ProcessControlBlock *proc = (ProcessControlBlock *)NULL;
	LLNode *cur;
	pthread_mutex_lock(&q->mutex);
	cur = q->head;

	while(cur->next){
		cur = cur->next;
		if(lt(cur->data, proc))
			proc = cur->data;
	}
	pthread_mutex_unlock(&q->mutex);
	return proc;
}

void preemptive_dispatch(LinkedList *ready_processes, LinkedList *running_processes, EventQueue *event_queue, void *lt_func){
	t_ltfunc lt = lt_func;

	int cpu;
	ProcessControlBlock *proc1;
	ProcessControlBlock *proc2;
	LinkedList _ready_processes;
	LinkedList _running_processes;

	ll_copy(&_ready_processes, ready_processes);
	ll_copy(&_running_processes, running_processes);

	/*printf("RUNNING PROCESSES"); ll_print(&_running_processes);*/
	/*printf("READY PROCESSES"); ll_print(&_ready_processes);*/
	cpu = get_non_used_cpu(&_running_processes);
	/*printf("FREE CPU = %d\n", cpu);*/
	while(cpu != -1){
		//proc1 = srtn_choose_min(&_ready_processes);
		proc1 = choose_min(&_ready_processes, &_running_processes, lt_func);
		proc1->cpu = cpu;
		ll_remove(&_ready_processes, proc1->id);
		ll_insert_beginning(&_running_processes, proc1);

		/*printf("\tchosen [%d-%s] to cpu %d.\n", proc1->id, proc1->info->process_name, proc1->cpu);*/
		/*ll_print(&_ready_processes);*/
		enqueue(event_queue, EVT_DISPATCH, (void *)proc1);

		if(_ready_processes.size == 0)
			break;
		cpu = get_non_used_cpu(&_running_processes);
	}

	LLNode *cur;
	EventContextSwitchPayload *context_switch_args;
	ProcessControlBlock *best_candidate;

	/*printf("\nReplace someone?\n");*/
	do{
		/*proc1 = srtn_choose_min(&_ready_processes);*/
		best_candidate = (ProcessControlBlock *)NULL;
		proc1 = choose_min(&_ready_processes, &_running_processes, lt_func);
		if(proc1)
			ll_remove(&_ready_processes, proc1->id);
		else
			break;
		cur = _running_processes.head;
		/*printf("\ttesting [%d-%s] with rt = %f\n", proc1->id, proc1->info->process_name, remaining_time(proc1));*/
		/*printf("\ttesting [%d-%s]\n", proc1->id, proc1->info->process_name);*/
		while(cur->next){
			cur = cur->next;
			proc2 = cur->data;
			/*printf("\t\tvs [%d-%s] with rt = %f\n", proc2->id, proc2->info->process_name, remaining_time(proc2));*/
			if(lt(proc1,proc2)){
				//proc1 can replace proc2. Is proc2 the best candidate?
				if(!best_candidate){
					best_candidate = proc2;
				} else if(!lt(proc2, best_candidate)){
					best_candidate = proc2;
				}
			}
		}
		if(best_candidate){
			/*printf("\t\t\treplacing [%d-%s]\n", best_candidate->id, best_candidate->info->process_name);*/
			context_switch_args = malloc(sizeof(*context_switch_args));
			context_switch_args->old = proc2;
			context_switch_args->new = proc1;
			context_switch_args->cpu = proc2->cpu;

			enqueue(event_queue, EVT_CONTEXT_SWITCH, (void *)context_switch_args);
			ll_remove(&_running_processes, best_candidate->id);
		}
	} while(1);
	/*printf("SRTN_ON_READY END\n");*/
	ll_destroy(&_ready_processes);
	ll_destroy(&_running_processes);
}

void preemptive_dispatch2(LinkedList *ready_processes, LinkedList *running_processes, EventQueue *event_queue, void *lt_func){
	ProcessControlBlock *proc = (ProcessControlBlock *)NULL;

	if(ready_processes->size > 0){
		proc = choose_min(ready_processes, running_processes, lt_func);
		proc->cpu = get_non_used_cpu(running_processes);
		/*printf("\tCHOSEN process [%d-%s] and CPU=[%d].\n", proc->id, proc->info->process_name, proc->cpu);*/
		enqueue(event_queue, EVT_DISPATCH, (void *)proc);
	}
}
/* COMMON END */

/* First Come First Served */
ProcessControlBlock *fcfs_choose(LinkedList *ready_processes, LinkedList *running_processes){
	return ready_processes->head->next->data; 
}

void fcfs_on_ready(ProcessControlBlock **proc_batch, int batch_size, LinkedList *ready_processes, LinkedList *running_processes, EventQueue *event_queue){
	nonpreemptive_dispatch(ready_processes, running_processes, event_queue, (void *)&fcfs_choose);
}

void fcfs_on_exit(ProcessControlBlock *evt_proc, LinkedList *ready_processes, LinkedList *running_processes, EventQueue *event_queue){
	nonpreemptive_dispatch2(ready_processes, running_processes, event_queue, (void *)fcfs_choose);
}

/* Shortest Job First */
ProcessControlBlock *sjf_choose(LinkedList *ready_processes, LinkedList *running_processes){
	LinkedList *q = ready_processes;
	LLNode *min_cur = (LLNode *)NULL;
	LLNode *cur = q->head;

	while(cur->next){
		cur = cur->next;
		if(!min_cur){
			min_cur = cur;
		} else if(cur->data->info->dt < min_cur->data->info->dt){
			min_cur = cur;
		}
	}

	return min_cur->data;
}

void sjf_on_ready(ProcessControlBlock **proc_batch, int batch_size, LinkedList *ready_processes, LinkedList *running_processes, EventQueue *event_queue){
	nonpreemptive_dispatch(ready_processes, running_processes, event_queue, (void *)sjf_choose);
}

void sjf_on_exit(ProcessControlBlock *evt_proc, LinkedList *ready_processes, LinkedList *running_processes, EventQueue *event_queue){
	nonpreemptive_dispatch2(ready_processes, running_processes, event_queue, (void *)sjf_choose);
}

/* Shortest Remaining Time Nex */

int srtn_lt(ProcessControlBlock *proc1, ProcessControlBlock *proc2){
	return remaining_time(proc1) + 0.05 < remaining_time(proc2);
}

void srtn_on_exit(ProcessControlBlock *evt_proc, LinkedList *ready_processes, LinkedList *running_processes, EventQueue *event_queue){
	preemptive_dispatch2(ready_processes, running_processes, event_queue, (void *)srtn_lt);
}

void srtn_on_ready(ProcessControlBlock **proc_batch, int batch_size, LinkedList *ready_processes, LinkedList *running_processes, EventQueue *event_queue){
	preemptive_dispatch(ready_processes, running_processes, event_queue, (void *)srtn_lt);
}

/* Round Robin */
typedef struct {
	LinkedList *ready_processes;
	LinkedList *running_processes;
	EventQueue *event_queue;
} RRLoopArgs;

LinkedList rr_queue;
int rr_cpu[MAX_CPU_NUMBER];
pthread_t rr_thread;
RRLoopArgs rr_thread_args;
sem_t rr_cpu_available;

typedef struct {
	int interval_ms;
	ProcessControlBlock *proc;
	EventQueue *event_queue;
} InterruptArgs;

#include <unistd.h>
#include "utils.h"
void rr_send_interrupt(void *args){
	InterruptArgs *thread_args = args;
	/*ProcessControlBlock *proc = thread_args->proc;*/

	sleep_ms(thread_args->interval_ms);
	/*printf("\tINTERRUPT [%d-%s]\n", proc->id, proc->info->process_name);*/
	if(thread_args->proc->state == RUNNING)
		enqueue(thread_args->event_queue, EVT_INTERRUPT, (ProcessControlBlock *)thread_args->proc);

	free(thread_args);
}

#include <assert.h>
void rr_loop(void *args){
	int i;
	RRLoopArgs *thread_args = args;
	/*LinkedList *ready_processes = thread_args->ready_processes;*/
	/*LinkedList *running_processes = thread_args->running_processes;*/
	EventQueue *event_queue = thread_args->event_queue;
	int quantum_ms = 500;
	pthread_t interrupt_threads[256];
	int threads_count = 0;

	InterruptArgs *interrupt_thread_args;
	
	ProcessControlBlock *proc;
	while(1){
		//wait for cpu
		sem_wait(&rr_cpu_available);		
		do {
			proc = ll_remove_index(&rr_queue, 0);
		} while( proc->state != READY);
		//assert( proc->state == READY );
		/*printf("REMOVE [%d-%s]\n", proc->id, proc->info->process_name);*/

		for(i=0; i<g_total_cpus; i++){
			if(rr_cpu[i] == 1){
				rr_cpu[i] = 0;
				proc->cpu = i;
				break;
			}
		}

		interrupt_thread_args = malloc(sizeof(*interrupt_thread_args));
		interrupt_thread_args->interval_ms = quantum_ms;
		interrupt_thread_args->proc = proc;
		interrupt_thread_args->event_queue = event_queue;
		pthread_create(&interrupt_threads[threads_count++], NULL, (void *)&rr_send_interrupt, (void *)interrupt_thread_args);

		enqueue(event_queue, EVT_DISPATCH, (void *)proc);
	}
}

void rr_init(LinkedList *ready_processes, LinkedList *running_processes, EventQueue *event_queue){
	rr_thread_args.ready_processes = ready_processes;
	rr_thread_args.running_processes = running_processes;
	rr_thread_args.event_queue = event_queue;

	ll_init(&rr_queue);
	int i;
	for(i=0; i<g_total_cpus; i++) rr_cpu[i] = 1;
	sem_init(&rr_cpu_available, 0, g_total_cpus);
	pthread_create(&rr_thread, NULL, (void *)&rr_loop, (void *)&rr_thread_args);
}

void rr_on_ready(ProcessControlBlock **proc_batch, int batch_size, LinkedList *ready_processes, LinkedList *running_processes, EventQueue *event_queue){
	ll_insert_last_batch(&rr_queue, proc_batch, batch_size);
}

void rr_on_exit(ProcessControlBlock *evt_proc, LinkedList *ready_processes, LinkedList *running_processes, EventQueue *event_queue){
	rr_cpu[evt_proc->cpu] = 1;
	sem_post(&rr_cpu_available);
}

void rr_on_interrupt(ProcessControlBlock *evt_proc, LinkedList *ready_processes, LinkedList *running_processes, EventQueue *event_queue){
	rr_cpu[evt_proc->cpu] = 1;
	sem_post(&rr_cpu_available);
}

/* Priority Scheduling */

int pc_priority(ProcessControlBlock *proc){
	if(!proc)
		return 99; // LEAST PRIORITY IS 19
	return proc->info->priority;
}

int ps_lt(ProcessControlBlock *proc1, ProcessControlBlock *proc2){
	printf("\t\t p=%d vs %d\n", pc_priority(proc1), pc_priority(proc2));
	/*printf("\t\t[%d-%s] p=%d vs [%d-%s] p=%d\n", proc1->id, proc1->info->process_name, proc1->info->priority, proc2->id, proc2->info->process_name, proc2->info->priority);*/
	return pc_priority(proc1) < pc_priority(proc2);
}

void ps_on_exit(ProcessControlBlock *evt_proc, LinkedList *ready_processes, LinkedList *running_processes, EventQueue *event_queue){
	preemptive_dispatch2(ready_processes, running_processes, event_queue, (void *)ps_lt);
}

void ps_on_ready(ProcessControlBlock **proc_batch, int batch_size, LinkedList *ready_processes, LinkedList *running_processes, EventQueue *event_queue){
	preemptive_dispatch(ready_processes, running_processes, event_queue, (void *)ps_lt);
}

/* Hard deadline scheduling */

double pc_deadline(ProcessControlBlock *proc){
	if(!proc)
		return 9999.0;
	return proc->info->deadline;
}
int hds_lt(ProcessControlBlock *proc1, ProcessControlBlock *proc2){
	return pc_deadline(proc1) < pc_deadline(proc2);
}

void hds_on_exit(ProcessControlBlock *evt_proc, LinkedList *ready_processes, LinkedList *running_processes, EventQueue *event_queue){
	preemptive_dispatch2(ready_processes, running_processes, event_queue, (void *)hds_lt);
}

void hds_on_ready(ProcessControlBlock **proc_batch, int batch_size, LinkedList *ready_processes, LinkedList *running_processes, EventQueue *event_queue){
	preemptive_dispatch(ready_processes, running_processes, event_queue, (void *)hds_lt);
}
