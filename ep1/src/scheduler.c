#include <stdio.h>
#include <assert.h>
#include <pthread.h>

#include "event_queue.h"
#include "scheduler.h"
#include "utils.h"
#include "algorithm.h"

#ifdef DEBUG
int g_debug = 1;
#else
extern int g_debug;
#endif

/* Static Prototypes */

static void run_loop(void *);
static void dispatch_process(ProcessControlBlock *);
static void handle_evt_new(Event *);
static void handle_evt_ready(Event *);
static void handle_evt_dispatch(Event *);
static void handle_evt_interrupt(Event *);
static void handle_evt_interrupted(Event *);
static void handle_evt_exit(Event *);

static void realTimeOperation(void *);

/* Static Variables */

/*typedef void (*t_evthandler_func)(Event *);*/
/*static t_evthandler_func evt_handlers = {*/
	/*handle_evt_new,*/
	/*handle_evt_dispatch,*/
	/*handle_evt_interrupt,*/
	/*handle_evt_exit*/
/*};*/

typedef struct {
	TraceInfo **batch;
	int batch_size;
} EventNewPayload;

typedef struct{
	ProcessControlBlock **batch;
	int batch_size;
} ProcBatchPayload;

typedef struct {
	ProcessControlBlock *proc;
	int cpu;
} EventDispatchPayload;

static pthread_t loop_thread;

static ProcessControlTable process_table;

static EventQueue event_queue;
static EventType last_evt_type;

static int arrived_process_count;
static int context_switch_count;
static int process_threads_count;

static int processors_in_use;
static int loop_running;

/* Implementations */

void scheduler_start(int algorithm){
	EventQueue_init(&event_queue);
	algorithm_init(algorithm);
	thread_manager_init();
	pthread_create(&loop_thread, NULL, (void *)run_loop, NULL);
}

void scheduler_stop(){
	enqueue(&event_queue, EVT_STOP, NULL);
}

void scheduler_join(){
	pthread_join(loop_thread, NULL);
}

static void run_loop(void *args){
	Event *evt;

	arrived_process_count = 0;
	context_switch_count = 0;
	process_threads_count = 0;
	processors_in_use = 0;

	process_table.ready_processes = malloc(sizeof(LinkedList));
	ll_init(process_table.ready_processes);
	process_table.running_processes = malloc(sizeof(LinkedList));
	ll_init(process_table.running_processes);
	process_table.finished_processes = malloc(sizeof(LinkedList));
	ll_init(process_table.finished_processes);

	last_evt_type = EVT_NONE;
	loop_running = 1;
	while(loop_running){
		evt = dequeue(&event_queue);
		switch(evt->type){
			case EVT_NEW:
				handle_evt_new(evt);
				break;
			case EVT_READY:
				handle_evt_ready(evt);
				break;
			case EVT_DISPATCH:
				handle_evt_dispatch(evt);
				break;
			case EVT_INTERRUPT:
				handle_evt_interrupt(evt);
				break;
			case EVT_INTERRUPTED:
				handle_evt_interrupted(evt);
				break;
			case EVT_EXIT:
				handle_evt_exit(evt);
				break;
			case EVT_STOP:
				break;
			default:
				break;
		}
		last_evt_type = evt->type;
	}

	ll_destroy(process_table.ready_processes);
	free(process_table.ready_processes);
	ll_destroy(process_table.running_processes);
	free(process_table.running_processes);
	ll_destroy(process_table.finished_processes);
	free(process_table.finished_processes);
}

static void handle_evt_new(Event *evt){
	int i;
	EventNewPayload *payload;
	ProcessControlBlock *proc;	
	ProcessControlBlock **proc_batch;
	TraceInfo *trace_info;

	payload = evt->payload;

	proc_batch = malloc(payload->batch_size * sizeof(ProcessControlBlock *));
	for(i=0; i<payload->batch_size; i++){
		trace_info = payload->batch[i];

		proc = malloc(sizeof(*proc));
		proc->id = arrived_process_count++; 
		proc->state = NEW;
		proc->info = trace_info;
		proc->et = 0.0;
		proc->interrupted = 0;

		if(g_debug) fprintf(stderr, "[%.2f] EVT_NEW: proc [%d-%s] from line [%d] arrived.\n", current_time_s(), proc->id, proc->info->process_name, proc->info->line);

		proc_batch[i] = proc;
	}

	scheduler_ready(proc_batch, payload->batch_size);

	free(proc_batch);
	free(payload);
}

static void handle_evt_ready(Event *evt){
	int i;
	ProcBatchPayload *payload = evt->payload;
	ProcessControlBlock *proc;

	for(i=0; i<payload->batch_size; i++){
		proc = payload->batch[i];
		if( proc->state == FINISHED ) continue;
		assert( proc->state == NEW || proc->state == RUNNING);
		if(proc->state == RUNNING){
			context_switch_count++;
		}
		proc->state = READY;
		proc->cpu = -1;
		
		if(g_debug) fprintf(stderr, "[%.2f] EVT_READY: proc [%d-%s] is ready.\n", current_time_s(), proc->id, proc->info->process_name);
	}

	ll_insert_beginning_batch(process_table.ready_processes, payload->batch, payload->batch_size);

	algorithm_on_ready(payload->batch, payload->batch_size);

	free(payload->batch);
	free(payload);
}

static void dispatch_process(ProcessControlBlock *proc){
	int rc;
	rc = pthread_create(&proc->thread, NULL, (void *)&realTimeOperation, (void *)proc);
	assert( rc == 0 );
	process_threads_count++;
}

static void handle_evt_dispatch(Event *evt){
	EventDispatchPayload *payload = evt->payload;
	ProcessControlBlock *proc;
	int cpu;

	proc = payload->proc;
	cpu = payload->cpu;
	assert( proc->state == READY );
	
	ll_remove(process_table.ready_processes, proc->id);
	proc->cpu = cpu;
	dispatch_process(proc);
	ll_insert_beginning(process_table.running_processes, proc);
	
	if(g_debug){ 
		fprintf(stderr, "[%.2f] EVT_DISPATCH: proc [%d-%s] assigned to CPU %d.\n", current_time_s(), proc->id, proc->info->process_name, proc->cpu);
	}

	proc->state = RUNNING;

	processors_in_use++;

	free(payload);
}

static void handle_evt_interrupt(Event *evt){
	ProcessControlBlock *proc;

	proc = evt->payload;
	if(proc->state == FINISHED){
		printf("cannot interrupt: already finished.\n");
		return;
	}
	assert( proc->state == RUNNING && proc->cpu != -1 );

	/*interrupt_process(proc);*/
	if(g_debug) fprintf(stderr, "[%.2f] EVT_INTERRUPT: proc [%d-%s] must release CPU %d.\n", current_time_s(), proc->id, proc->info->process_name, proc->cpu);
	proc->interrupted = 1;
}

static void handle_evt_interrupted(Event *evt){
	int cpu;
	ProcessControlBlock *proc = evt->payload;
	cpu = proc->cpu;

	/*printf("handle_evt_interrupted: [%d-%s] cpu=%d state=%d\n",proc->id, proc->info->process_name, proc->cpu, proc->state);*/
	assert( proc->state == RUNNING );
	/*ll_print(process_table.running_processes);*/
	ll_remove(process_table.running_processes, proc->id);

	ProcessControlBlock **proc_batch = malloc(sizeof(ProcessControlBlock *));
	proc_batch[0] = proc;

	scheduler_ready(proc_batch, 1);

	processors_in_use--;
	algorithm_on_interrupt(proc, cpu);

	free(proc_batch);
}

extern int g_trace_file_read;
extern int g_line_count;

static void handle_evt_exit(Event *evt){
	int cpu;
	ProcessControlBlock *proc;

	proc = evt->payload;
	/*ll_print(process_table.ready_processes);*/
	/*ll_print(process_table.running_processes);*/
	/*ll_print(process_table.finished_processes);*/
	assert( proc->state == RUNNING && proc->cpu != -1 );
	cpu = proc->cpu;

	ll_remove(process_table.running_processes, proc->id);
	ll_insert_beginning(process_table.finished_processes, proc);

	if(g_debug) fprintf(stderr, "[%.2f] EVT_EXITED: proc [%d-%s] released CPU %d.\n", current_time_s(), proc->id, proc->info->process_name, proc->cpu);

	proc->cpu = -1;
	proc->state = FINISHED;

	loop_running = !(g_trace_file_read && (g_line_count == process_table.finished_processes->size));

	/*if(g_scheduler->on_exit && running)*/
		/*g_scheduler->on_exit(proc, &g_ready_processes, &g_running_processes, &g_&event_queue);*/

	processors_in_use--;

	if(loop_running)
		algorithm_on_exit(proc, cpu);
}

void scheduler_new(TraceInfo **batch, int size){
	EventNewPayload *payload = malloc(sizeof(*payload));
	payload->batch = batch;
	payload->batch_size = size;

	enqueue(&event_queue, EVT_NEW, (void *)payload);
}

void scheduler_ready(ProcessControlBlock **proc_batch, int batch_size){
	int i;
	ProcBatchPayload *payload = malloc(sizeof(*payload));
	ProcessControlBlock **batch_copy = malloc(batch_size * sizeof(ProcessControlBlock *));
	for(i=0; i<batch_size; i++)
		batch_copy[i] = proc_batch[i];
	payload->batch = batch_copy;
	payload->batch_size = batch_size;
	enqueue(&event_queue, EVT_READY, (void *)payload);
}

void scheduler_dispatch(ProcessControlBlock *proc, int cpu){
	EventDispatchPayload *payload = malloc(sizeof(*payload));
	payload->proc = proc;
	payload->cpu = cpu;
	enqueue(&event_queue, EVT_DISPATCH, (void *)payload);
}

void scheduler_interrupt(ProcessControlBlock *proc){
	enqueue(&event_queue, EVT_INTERRUPT, (void *)proc);
}

void scheduler_exit(ProcessControlBlock *proc){
	enqueue(&event_queue, EVT_EXIT, (void *)proc);
}

double remaining_time(ProcessControlBlock *b){
	if(!b)
		return RTIME_INFINITY;
	return (b->info->dt - b->et);
}

void realTimeOperation(void *args) {
	ProcessControlBlock *proc = (ProcessControlBlock *)args;

	proc->state = RUNNING;
	proc->interrupted = 0;
	if(g_debug) fprintf(stderr, "[%.2f] proc [%d-%s] started using CPU %d.\n", current_time_s(), proc->id, proc->info->process_name, proc->cpu);

	double elapsed, et0;
	double start;

	et0 = proc->et;
	start = current_time_s();
	do {
		elapsed = current_time_s() - start;
		proc->et = et0 + elapsed;
	} while(remaining_time(proc) > 0 && !proc->interrupted);

	if(proc->interrupted){
		if(g_debug) fprintf(stderr, "[%.2f] proc [%d-%s] released CPU %d. (INTERRUPTED)\n", current_time_s(), proc->id, proc->info->process_name, proc->cpu);
		enqueue(&event_queue, EVT_INTERRUPTED, (void *)proc);
	}else{
		proc->tf = current_time();
		if(g_debug) fprintf(stderr, "[%.2f] proc [%d-%s] released CPU %d.\n", current_time_s(), proc->id, proc->info->process_name, proc->cpu);
		//proc->state = FINISHED;
		scheduler_exit(proc);
	}
}

