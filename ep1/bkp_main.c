#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>

typedef struct t_traceinfo {
	float t0, dt, deadline;
	int priority;
	char process_name[1024];
} TraceInfo;

typedef void (*t_acceptfunc)(TraceInfo **, int);
typedef void (*t_runfunc)(void *);

typedef struct t_scheduler {
	t_acceptfunc accept;
	t_runfunc run;
} Scheduler;

typedef struct {
	int id;
	int state;
	TraceInfo *info;
} ProcessControlBlock;

struct timeval g_start, g_wait;

double time_diff(struct timeval x, struct timeval y){
	double x_us, y_us, diff;

	x_us = (double)x.tv_sec*1000000 + (double)x.tv_usec;
	y_us = (double)y.tv_sec*1000000 + (double)y.tv_usec;
	diff = (double)y_us - (double)x_us;

	return diff;
}

typedef enum {EVT_NEW, EVT_INTERRUPTED, EVT_EXITED, EVT_READY, EVT_DISPATCH} EventType;
typedef struct {
	EventType type;
	void *payload;
} Event;

#define EVENT_QUEUE_MAX_SIZE 1024
typedef struct {
	Event *buffer[EVENT_QUEUE_MAX_SIZE];
	sem_t items, slots;
	pthread_mutex_t mutex;
	int insert_index;
	int remove_index;
} EventQueue;

EventQueue g_event_queue, g_dispatch_queue;
sem_t g_available_processors;

void EventQueue_init(EventQueue *q){
	sem_init(&q->items, 1, 0);
	sem_init(&q->slots, 1, EVENT_QUEUE_MAX_SIZE);
	pthread_mutex_init(&q->mutex, NULL);
	q->insert_index = 0;
	q->remove_index = 0;
}

void enqueue(EventQueue *q, EventType type, void *payload){
	Event *data = (Event *)malloc(sizeof(*data));
	data->type = type;
	data->payload = payload;

	sem_wait(&q->slots);
	pthread_mutex_lock(&q->mutex);
	q->buffer[q->insert_index] = data;
	q->insert_index = (q->insert_index + 1) % EVENT_QUEUE_MAX_SIZE;
	pthread_mutex_unlock(&q->mutex);
	sem_post(&q->items);

	//printf("\tevt_queue insert_index = %d\n", q->insert_index);
}

Event *dequeue(EventQueue *q){
	Event *result;

	sem_wait(&q->items);
	pthread_mutex_lock(&q->mutex);
	result = q->buffer[q->remove_index];
	q->remove_index = ((q->remove_index + 1) % EVENT_QUEUE_MAX_SIZE);
	pthread_mutex_unlock(&q->mutex);
	sem_post(&q->slots);

	//printf("\tevt_queue remove_index = %d\n", q->remove_index);
	return result;
}

void fcfs_accept(TraceInfo *new_processes_info[], int size){
	static int count = 0;
	int i;

	printf("Accepting count at %d\n", count);
	for(i=0 ; i < size; i++){
		printf("\tfcfs_accepting %s (%f)\n", new_processes_info[i]->process_name, new_processes_info[i]->t0);
		enqueue(&g_event_queue, EVT_NEW, new_processes_info[i]);
	}
}

typedef struct LLNode {
	struct LLNode *next;
	struct LLNode *prev;
	ProcessControlBlock *data;
} LLNode;

typedef struct {
	LLNode *head;
	pthread_mutex_t mutex;
	int size;
	sem_t _size;
} LinkedList;

LinkedList g_ready_processes, g_running_processes;

void ll_init(LinkedList *q){
	LLNode *node = (LLNode *)malloc(sizeof(*node));
	node->prev = (LLNode *)NULL;
	node->next = (LLNode *)NULL;
	ProcessControlBlock *dummy = (ProcessControlBlock *)malloc(sizeof(*dummy));
	dummy->id = -1;
	node->data = dummy;
	q->head = node;
	q->size = 0;

	pthread_mutex_init(&q->mutex, NULL);
	sem_init(&q->_size, 1, 0);
}

void ll_insert_beginning(LinkedList *q, ProcessControlBlock *b){
	pthread_mutex_lock(&q->mutex);

	LLNode *first, *second;

	first = (LLNode *)malloc(sizeof(*first));
	first->data = b;

	second = q->head->next;
	q->head->next = first;
	first->prev = q->head;
	first->next = second;
	if(second)
		second->prev = first;

	q->size++;
	printf("insert size=%d\n", q->size);
	sem_post(&q->_size);
	pthread_mutex_unlock(&q->mutex);
}

LLNode *ll_find(LinkedList *q, int id){
	LLNode *cur = q->head;
	while(cur->next){
		cur = cur->next;
		if(cur->data && cur->data->id == id){
			return cur;
		}
	}
	return ((LLNode *)NULL);	
}

LLNode *ll_find_index(LinkedList *q, int index){
	int count = 0;
	LLNode *cur = q->head;
	while(cur->next){
		cur = cur->next;
		if(count++ == index){
			return cur;
		}
	}
	return ((LLNode *)NULL);	
}

ProcessControlBlock *ll_remove_node(LinkedList *q, LLNode *node){
	ProcessControlBlock *result;

	printf("\t removing node with proc_id = %d\n", node->data->id);
	if(node->prev){
		printf("\t\t prev proc_id = %d\n", node->prev->data->id);
		node->prev->next = node->next;
	}
	if(node->next){
		printf("\t\t next proc_id = %d\n", node->next->data->id);
		node->next->prev = node->prev;
	}
	result = node->data;
	free(node);
	return result;
}

ProcessControlBlock *ll_remove_index(LinkedList *q, int id){
	sem_wait(&q->_size);
	pthread_mutex_lock(&q->mutex);

	ProcessControlBlock *result;

	LLNode *node = ll_find_index(q, id);
	result = ll_remove_node(q, node);
	q->size--;
	printf("remove size=%d\n", q->size);

	LLNode *cur = q->head;
	printf("\tQUEUE = [ ");
	while(cur->next){
		cur = cur->next;
		printf("%d ", cur->data->id);
	}
	printf("]\n");
	pthread_mutex_unlock(&q->mutex);
	return result;
}

ProcessControlBlock *fcfs_choose(){
	ProcessControlBlock *proc;
	
	proc = ll_remove_index(&g_ready_processes, 0); 
	if(!proc)
		fprintf(stderr, "FATAL: this shouldnt ever happen.\n");
	printf("GOT proc_id = %d\n", proc->id);
	return proc;
}

void *realTimeOperation(void *args) {
	ProcessControlBlock *proc = (ProcessControlBlock *)args;
	printf("Hello wolrd!\n");
	/*double time, elapsed;*/

	/*clock_t start, end;*/

	/*int id = proc->id;*/
	/*time = proc->info->dt;*/
	/*start = clock();*/
	/*printf("Rodando %d %lf\n", id, time);*/
	/*while (1) {*/
		/*end = clock();*/
		/*elapsed = ((double)end - (double)start) / CLOCKS_PER_SEC;*/
		/*if(elapsed >= time) {*/
			/*//printf("Thread %d terminou - instante de encerramento: %lf\n", id, (((double)end - (double)gstart) / CLOCKS_PER_SEC));*/
			/*//procs[id].ftime = ((double)end - (double)gstart) / CLOCKS_PER_SEC;*/
			/*printf("finished\n");*/
			/*break;*/
		/*}*/
	/*}*/

	enqueue(&g_event_queue, EVT_EXITED, (void *)proc);
	return NULL;
}

#define MAX_SIZE 1024
void dispatcher_run(void *args){
	int running;
	int rc;
	ProcessControlBlock *proc;

	int thread_args[MAX_SIZE];
	pthread_t threads[MAX_SIZE];
	int i = 0;
	
	running = 1;
	while(running){
		printf("DISPATCHER: waiting for available_processors...\n");
		sem_wait(&g_available_processors);
		//printf("DISPATCHER: processor available.\n");

		proc = fcfs_choose();

		printf("dispatching proc %d to CPU\n", proc->id);
		rc = pthread_create(&threads[i++], NULL, (void *)&realTimeOperation, NULL);
		assert(0 == rc);
	}
}

#define READY 1
#define RUNNING 2
void fcfs_run(void *args){
	pthread_t dispatcher_thread;
	int running;
	Event *evt;
	ProcessControlBlock *proc;
	TraceInfo *trace_info;
	
	printf("fcfs_run\n");
	int rc;
	rc = pthread_create(&dispatcher_thread, NULL, (void *)&dispatcher_run, NULL);
	assert( rc == 0);
	running = 1;
	while(running){
		evt = dequeue(&g_event_queue);
		switch(evt->type){
			case EVT_NEW:
				printf("EVT_NEW\n");
				trace_info = (TraceInfo *)evt->payload;

				proc = (ProcessControlBlock *)malloc(sizeof(proc));
				proc->id = g_ready_processes.size + 1;
				proc->state = READY;
				proc->info = trace_info;

				printf("INSERTING proc_id = %d\n", proc->id);
				ll_insert_beginning(&g_ready_processes, proc);

				/*enqueue(&g_event_queue, EVT_READY, 0);*/
				break;
			case EVT_INTERRUPTED:
				printf("EVT_INTERRUPTED\n");
				/*remove(&running_processes, proc_id);*/
				sem_post(&g_available_processors);
				break;
			case EVT_EXITED:
				printf("EVT_EXITED\n");
				/*remove(&running_processes, proc_id);*/
				sem_post(&g_available_processors);
				break;
		}
	}
	// TODO: stop dispatcher
	printf("stopping dispatcher\n");
}


void sjb_choose(){
	// choose the minium burst time in the ready_processes
}

void srtn_choose(){
	// choose the minimum burst time from the ready_processes and the running_processes
}

void rr_choose(){

}

Scheduler schedulers[] = {
	{ fcfs_accept, fcfs_run }
};

TraceInfo *read_traceinfo(FILE *fp){
	float t0,dt,deadline;
	int priority;
	char process_name[1024];

	TraceInfo *info;
	if(fscanf(fp, "%f %s %f %f %d", &t0, process_name, &dt, &deadline, &priority) == 5){
		info = (TraceInfo *)malloc(sizeof(*info));	
		info->t0 = t0;
		strcpy(info->process_name, process_name);
		info->dt = dt;
		info->deadline = deadline;
		info->priority = priority;

		return info;
	}
	return ((TraceInfo *)NULL);
}

void simulate_arrivals(Scheduler *scheduler, TraceInfo *new_processes_info[], int size){
	struct timeval now;
	double target_arrival_time, wait_time, ellapsed_time;
	target_arrival_time = (double)g_start.tv_sec*1000000+(double)g_start.tv_usec+(double)new_processes_info[0]->t0 * 1000000;

	gettimeofday(&now, NULL);
	wait_time = target_arrival_time - ((double)now.tv_sec*1000000+(double)now.tv_usec);
	printf("wait_time = %f\n", wait_time);
	usleep(wait_time) ;

	gettimeofday(&now, NULL);
	ellapsed_time = time_diff(g_start, now);
	printf("elapsed time %f us\n", ellapsed_time);

	scheduler->accept(new_processes_info, size);
}

void run_simulation(char *filename, Scheduler *scheduler) {
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
					simulate_arrivals(scheduler, new_processes, index);
					index = 0;
					new_processes[index++] = cur;
				}
				prev = cur;
				cur = read_traceinfo(fp);
			} while(cur);
			if(index > 0)
					simulate_arrivals(scheduler, new_processes, index);
		} else{
			simulate_arrivals(scheduler, new_processes, index);
		}
		fclose(fp);
	} else {
		fprintf(stderr, "FATAL: couldnt open [%s]\n", filename);
	}
}

int main(int argc, char **argv){
	int scheduler_type = atoi(argv[1]);
	char *input_filename = argv[2];
	char *output_filename = argv[3];

	pthread_t scheduler_thread;
	
	EventQueue_init(&g_event_queue);
	ll_init(&g_ready_processes);
	ll_init(&g_running_processes);
	//sem_init(&g_available_processors, 1, sysconf(_SC_NPROCESSORS_ONLN)); 
	sem_init(&g_available_processors, 1, 1); 

	gettimeofday(&g_start, NULL);
	Scheduler scheduler = schedulers[scheduler_type];

	pthread_create(&scheduler_thread, NULL, (void *)scheduler.run, NULL);
	run_simulation(input_filename, &scheduler);

	if(pthread_join(scheduler_thread, NULL)){
		fprintf(stderr, "Error joining thread\n");
		return 2;
	}
	
	return 0;
}
