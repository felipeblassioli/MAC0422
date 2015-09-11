#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>

#include "simulator.h"
#include "utils.h"

/* GLOBALS */
int g_debug = 1;
int g_line_count;
int g_total_cpus;
int g_context_switch_count;
struct timeval g_start;

double current_time(){
	double ellapsed_time;
	struct timeval now;
	gettimeofday(&now, NULL);
	ellapsed_time = time_diff(g_start, now);

	return ellapsed_time;
}

double current_time_s(){
	double ellapsed_time;
	struct timeval now;
	gettimeofday(&now, NULL);
	ellapsed_time = time_diff(g_start, now);

	return ellapsed_time/1000000;
}

typedef struct {
	TraceInfo **batch;
	int batch_size;
} EventNewPayload;

#include "event_queue.h"
EventQueue g_event_queue, g_dispatch_queue;

#define LL_DATA_TYPE ProcessControlBlock *
#include "linked_list.h"
LinkedList g_ready_processes, g_running_processes, g_finished_processes;

#include <semaphore.h>
sem_t g_available_processors;

#include "algorithms.h"
typedef void (*t_initfunc)(LinkedList *, LinkedList *, EventQueue *);
typedef void (*t_onreadyfunc)(ProcessControlBlock **, int, LinkedList *, LinkedList *, EventQueue *);
typedef void (*t_onexitfunc)(ProcessControlBlock *, LinkedList *, LinkedList *, EventQueue *);
typedef void (*t_oninterruptfunc)(ProcessControlBlock *, LinkedList *, LinkedList *, EventQueue *);

typedef struct{
	t_initfunc init;
	t_oninterruptfunc on_interrupt;
	t_onreadyfunc on_ready;
	t_onexitfunc on_exit;
} SchedulerAlgorithm;

SchedulerAlgorithm g_scheduler_algorithms[] = {
	{ NULL, NULL, fcfs_on_ready, fcfs_on_exit },
	{ NULL, NULL, sjf_on_ready, sjf_on_exit },
	{ NULL, NULL, srtn_on_ready, srtn_on_exit },
	{ rr_init, rr_on_interrupt, rr_on_ready, rr_on_exit },
	{ NULL, NULL, ps_on_ready, ps_on_exit },
	{ NULL, NULL, hds_on_ready, hds_on_exit }
};

SchedulerAlgorithm *g_scheduler;

void realTimeOperation(void *args) {
	ProcessControlBlock *proc = (ProcessControlBlock *)args;

	proc->state = RUNNING;
	if(g_debug) fprintf(stderr, "[%.2f] proc [%d-%s] started using CPU %d.\n", current_time_s(), proc->id, proc->info->process_name, proc->cpu);

	double elapsed, et0;
	double start;

	et0 = proc->et;
	start = current_time_s();
	do {
		elapsed = current_time_s() - start;
		proc->et = et0 + elapsed;
	} while(proc->state == RUNNING && remaining_time(proc) > 0);

	if(proc->state != RUNNING){
		if(g_debug) fprintf(stderr, "[%.2f] proc [%d-%s] released CPU %d. (INTERRUPTED)\n", current_time_s(), proc->id, proc->info->process_name, proc->cpu);
	}else{
		proc->tf = current_time();
		if(g_debug) fprintf(stderr, "[%.2f] proc [%d-%s] released CPU %d.\n", current_time_s(), proc->id, proc->info->process_name, proc->cpu);
		enqueue(&g_event_queue, EVT_EXITED, (void *)proc);
	}
}

#define MAX_SIZE 256
pthread_t g_threads[MAX_SIZE];
int g_running_threads_count = 0;
void dispatch_process(ProcessControlBlock *proc, int cpu, int *context_switch_count, EventType last_evt_type){
	int rc;
	proc->cpu = cpu;
	proc->state = RUNNING;
	ll_remove(&g_ready_processes, proc->id);
	rc = pthread_create(&g_threads[g_running_threads_count++], NULL, (void *)&realTimeOperation, (void *)proc);
	assert( rc == 0 );
	ll_insert_beginning(&g_running_processes, proc);

	if(last_evt_type == EVT_INTERRUPT){
		(*context_switch_count)++;
		if(g_debug) fprintf(stderr, "[%.2f] EVT_DISPATCH: proc [%d-%s] assigned to CPU %d. (context_switch_count=%d)\n", current_time_s(), proc->id, proc->info->process_name, proc->cpu, (*context_switch_count));
	} else if(g_debug){ 
		fprintf(stderr, "[%.2f] EVT_DISPATCH: proc [%d-%s] assigned to CPU %d.\n", current_time_s(), proc->id, proc->info->process_name, proc->cpu);
	}
}

double remaining_time(ProcessControlBlock *b){
	if(!b)
		return RTIME_INFINITY;
	return (b->info->dt - b->et);
}

void interrupt_process(ProcessControlBlock *proc){
	assert( proc->state == RUNNING );

	if(g_debug) fprintf(stderr, "[%.2f] EVT_INTERRUPT: proc [%d-%s] must release CPU %d.\n", current_time_s(), proc->id, proc->info->process_name, proc->cpu);
	proc->state = READY;
	ll_remove(&g_running_processes, proc->id);
	ll_insert_beginning(&g_ready_processes, proc);

	if(g_scheduler->on_interrupt)
		g_scheduler->on_interrupt(proc, &g_ready_processes, &g_running_processes, &g_event_queue);
}

void run_scheduler(void *args){
	static int arrived_process_count = 0;
	EventType last_evt_type = EVT_NONE;

	double cur_time;
	
	int trace_file_read = 0;
	int running = 1;
	Event *evt;
	ProcessControlBlock *proc;
	TraceInfo *trace_info;
	
	int i;
	EventNewPayload *payload;
	EventContextSwitchPayload *cs_payload;
	ProcessControlBlock **proc_batch;

	if(g_scheduler->init)
		g_scheduler->init(&g_ready_processes, &g_running_processes, &g_event_queue);
	while(running){
		evt = dequeue(&g_event_queue);
		cur_time = current_time_s();
		switch(evt->type){
			case EVT_NONE:
				break;
			case EVT_NEW:
				payload = evt->payload;

				proc_batch = malloc(payload->batch_size * sizeof(ProcessControlBlock *));
				for(i=0; i<payload->batch_size; i++){
					trace_info = payload->batch[i];

					proc = malloc(sizeof(*proc));
					proc->id = arrived_process_count++; 
					proc->state = READY;
					proc->info = trace_info;
					proc->et = 0.0;

					if(g_debug) fprintf(stderr, "[%.2f] EVT_NEW: proc [%d-%s] from line [%d] arrived.\n", cur_time, proc->id, proc->info->process_name, proc->info->line);

					proc_batch[i] = proc;
				}

				ll_insert_beginning_batch(&g_ready_processes, proc_batch, payload->batch_size);

				if(g_scheduler->on_ready)
					g_scheduler->on_ready(proc_batch, payload->batch_size, &g_ready_processes, &g_running_processes, &g_event_queue);

				free(proc_batch);
				free(payload);
				break;
			case EVT_INTERRUPT:
				proc = evt->payload;
				if(proc->state == FINISHED)
					break;
				interrupt_process(proc);

				proc_batch = malloc(1 * sizeof(ProcessControlBlock *));
				proc_batch[0] = proc;
				if(g_scheduler->on_ready)
					g_scheduler->on_ready(proc_batch, 1, &g_ready_processes, &g_running_processes, &g_event_queue);
				free(proc_batch);
				break;
			case EVT_CONTEXT_SWITCH:
				cs_payload = evt->payload;

				interrupt_process(cs_payload->old);
				last_evt_type = EVT_INTERRUPT;
				dispatch_process(cs_payload->new, cs_payload->cpu, &g_context_switch_count, last_evt_type);

				free(cs_payload);
				break;
			case EVT_EXITED:
				proc = evt->payload;
				proc->state = FINISHED;

				ll_remove(&g_running_processes, proc->id);
				ll_insert_beginning(&g_finished_processes, proc);
				if(g_debug) fprintf(stderr, "[%.2f] EVT_EXITED: proc [%d-%s] released CPU %d.\n", cur_time, proc->id, proc->info->process_name, proc->cpu);

				running = !(trace_file_read && (g_line_count == g_finished_processes.size));

				if(g_scheduler->on_exit && running)
					g_scheduler->on_exit(proc, &g_ready_processes, &g_running_processes, &g_event_queue);

				sem_post(&g_available_processors);
				break;
			case EVT_DISPATCH:
				proc = evt->payload;

				dispatch_process(proc, proc->cpu, &g_context_switch_count, last_evt_type);
				break;
			case EVT_STOP:
				//running = 0;
				trace_file_read = 1;
				break;
		}
		last_evt_type = evt->type;
	}
}

TraceInfo *read_traceinfo(FILE *fp){
	double t0,dt,deadline;
	int priority;
	char process_name[1024];

	TraceInfo *info;
	if(fscanf(fp, "%lf %s %lf %lf %d", &t0, process_name, &dt, &deadline, &priority) == 5){
		info = malloc(sizeof(*info));	
		if(!info){
			fprintf(stderr, "ERROR: couldnt malloc traceinfo.\n");
			exit(9);
		}
		info->t0 = t0;
		strcpy(info->process_name, process_name);
		info->dt = dt;
		info->deadline = deadline;
		info->priority = priority;
		info->line = g_line_count++; 

		return info;
	}
	return ((TraceInfo *)NULL);
}

void simulate_arrivals(TraceInfo *new_processes_info[], int size){
	struct timeval now;
	double target_arrival_time, wait_time;
	target_arrival_time = (double)g_start.tv_sec*1000000+(double)g_start.tv_usec+(double)new_processes_info[0]->t0 * 1000000;

	gettimeofday(&now, NULL);
	wait_time = target_arrival_time - ((double)now.tv_sec*1000000+(double)now.tv_usec);
	/*printf("wait_time = %f\n", wait_time);*/
	usleep(wait_time) ;

	gettimeofday(&now, NULL);
	/*ellapsed_time = time_diff(g_start, now);*/
	/*printf("elapsed time %f us\n", ellapsed_time);*/

	/* Simulate arrival at time t */
	/*static int count = 0;*/
	int i;

	TraceInfo **batch = malloc(size * sizeof(TraceInfo *));
	/*printf("Accepting count at %d\n", count);*/
	for(i=0 ; i < size; i++){
		/*printf("\tarrival of %s (t0=%f, dt=%f, deadline=%f priority=%d)\n", new_processes_info[i]->process_name, new_processes_info[i]->t0, new_processes_info[i]->dt, new_processes_info[i]->deadline, new_processes_info[i]->priority);*/
		batch[i] = new_processes_info[i];
	}

	EventNewPayload *payload = malloc(sizeof(*payload));
	payload->batch = batch;
	payload->batch_size = size;

	enqueue(&g_event_queue, EVT_NEW, (void *)payload);
}

void parse_tracefile(char *filename) {
	int index;
	TraceInfo *cur, *prev, *new_processes[2056];
	FILE *fp;
	
	fp = fopen(filename, "r");
	index = 0;
	if(fp) {
		prev = read_traceinfo(fp);
		if(prev)
			new_processes[index++] = prev;
		cur = read_traceinfo(fp);
		if(cur){
			do {
				if(cur->t0 == prev->t0){
					new_processes[index++] = cur;
				} else{
					simulate_arrivals(new_processes, index);
					index = 0;
					new_processes[index++] = cur;
				}
				prev = cur;
				cur = read_traceinfo(fp);
			} while(cur);
			if(index > 0)
					simulate_arrivals(new_processes, index);
		} else{
			simulate_arrivals(new_processes, index);
		}
		fclose(fp);
	} else {
		fprintf(stderr, "FATAL: couldnt open [%s]\n", filename);
	}
	enqueue(&g_event_queue, EVT_STOP, NULL);
}

void init_globals(){
	g_line_count = 0;
	g_context_switch_count = 0;
	if(g_total_cpus == 0)
		g_total_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	assert( g_total_cpus <= MAX_CPU_NUMBER);
	printf("CPU_COUNT=%d\n", g_total_cpus);

	EventQueue_init(&g_event_queue);

	ll_init(&g_ready_processes);
	ll_init(&g_running_processes);
	ll_init(&g_finished_processes);
	//sem_init(&g_available_processors, 1, sysconf(_SC_NPROCESSORS_ONLN)); 
	sem_init(&g_available_processors, 0, g_total_cpus); 

	gettimeofday(&g_start, NULL);
}

void write_output_trace_file(char *filename){
	FILE *fp = fopen(filename, "w");
	if(fp){
		ProcessControlBlock *proc;
		LinkedList *q = &g_finished_processes;
		LLNode *cur = q->head;
		double tr, tf;
		while(cur->next){
			cur = cur->next;
			proc = cur->data;
			tf = proc->tf/1000000.0;
			tr = tf - proc->info->t0;
			fprintf(fp, "%s %.2f %.2f\n", proc->info->process_name, tf, tr);
		}
		fprintf(fp, "%d\n", g_context_switch_count);
		fclose(fp);
	}else{
		fprintf(stderr, "ERROR: couldnt write output file to %s.\n", filename);
		exit(1);
	}
}

void run_simulation(int scheduler_type, char *input_filename, char *output_filename){
	pthread_t scheduler_thread;
	g_scheduler = &g_scheduler_algorithms[scheduler_type-1];
	
	init_globals();
	pthread_create(&scheduler_thread, NULL, (void *)run_scheduler, NULL);

	parse_tracefile(input_filename);

	pthread_join(scheduler_thread, NULL);
	write_output_trace_file(output_filename);
}

int main(int argc, char **argv){
	int scheduler_type = atoi(argv[1]);
	char *input_filename = argv[2];
	char *output_filename = argv[3];
	if(argc >= 5){
		g_debug = (argv[4][0] == 'd');
	} else{
		g_debug = 0;
	}
	if(argc >= 6)
		g_total_cpus = atoi(argv[5]);
	else
		g_total_cpus = 0;

	run_simulation(scheduler_type, input_filename, output_filename);

	return 0;
}
