#include <pthread.h>
#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <assert.h>

#include "algorithm.h"
#include "scheduler.h"
#include "linked_list.h"
#include "utils.h"

extern int g_total_cpus;

/* Prototypes */

static void rr_init();
static void rr_on_ready();
static void rr_on_interrupt();
static void rr_on_exit();

static ProcessControlBlock *rr_next();

static pthread_t rr_loop_thread[MAX_CPU_NUMBER];
static LinkedList rr_queue;

static sem_t rr_cpu_available[MAX_CPU_NUMBER];

typedef struct {
	int cpu;
} RRThreadArgs;

RRThreadArgs rr_thread_args[MAX_CPU_NUMBER];

/* Implementations */

static ProcessControlBlock *rr_next(){
	ProcessControlBlock *proc = ll_remove_index(&rr_queue, 0);
	while(proc->state != READY)
		proc = ll_remove_index(&rr_queue, 0);
	return proc;
}

static void rr_loop(void *args){
	RRThreadArgs *thread_args = args;
	int cpu = thread_args->cpu;

	ProcessControlBlock *proc;
	int quantum_ms = 1000;

	while(1){
		sem_wait(&rr_cpu_available[cpu]);

		proc =  rr_next();
		assert( proc->state == READY );

		scheduler_dispatch(proc, cpu);

		sleep_ms(quantum_ms);	
		if(proc->state == RUNNING)
			scheduler_interrupt(proc);
	}
}

static void rr_init(){
	int i;
	ll_init(&rr_queue);

	for(i=0; i<g_total_cpus; i++){
		rr_thread_args[i].cpu = i;
		sem_init(&rr_cpu_available[i], 0, 1);
		pthread_create(&rr_loop_thread[i], NULL, (void *)&rr_loop, (void *)&rr_thread_args[i]);
	}
}

static void rr_on_ready(ProcessControlBlock **proc_batch, int batch_size){
	ll_insert_last_batch(&rr_queue, proc_batch, batch_size);
}

static void rr_on_interrupt(ProcessControlBlock *proc, int cpu){
	sem_post(&rr_cpu_available[cpu]);
}

static void rr_on_exit(ProcessControlBlock *proc, int cpu){
	sem_post(&rr_cpu_available[cpu]);
}

/*static int loop_running;*/
/*void loop(void *args){*/
	/*Event *evt;*/
	/*loop_running = 1;*/
	/*while(loop_running){*/
		/*evt = dequeue(&event_queue);*/
		/*switch(evt->type){*/
			/*case EVT_READY:*/
				/*break;*/
			/*case EVT_INTERRUPT:*/
				/*break;*/
			/*case EVT_EXIT:*/
				/*break;*/
			/*default:*/
				/*break;*/
		/*}*/
	/*}*/
/*}*/

void algorithm_init(int n){
	rr_init();
}

void algorithm_on_ready(ProcessControlBlock **proc_batch, int batch_size){
	int i;
	ProcessControlBlock **batch_copy = malloc( batch_size * sizeof(ProcessControlBlock *));
	for(i=0;i<batch_size;i++)
		batch_copy[i] = proc_batch[i];

	rr_on_ready(batch_copy, batch_size);
}

void algorithm_on_interrupt(ProcessControlBlock *proc, int cpu){
	/*enqueue(&event_queue, EVT_INTERRUPT, proc);*/
	rr_on_interrupt(proc, cpu);
}

void algorithm_on_exit(ProcessControlBlock *proc, int cpu){
	/*nqueue(&event_queue, EVT_EXIT, (void *)proc);*/
	rr_on_exit(proc, cpu);
}
