#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "simulator.h"
#include "utils.h"
#include "scheduler.h"

/* GLOBALS */
int g_debug = 1;
int g_line_count;
int g_total_cpus;
int g_trace_file_read;
struct timeval g_start;

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

	scheduler_new(batch, size);
}

void parse_tracefile(char *filename) {
	int index;
	TraceInfo *cur, *prev, *new_processes[1028];
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
	g_trace_file_read = 1;
	/*enqueue(&g_event_queue, EVT_STOP, NULL);*/
}

void init_globals(){
	g_line_count = 0;
	gettimeofday(&g_start, NULL);
	if(g_total_cpus == 0)
		g_total_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	assert( g_total_cpus <= MAX_CPU_NUMBER);
	g_trace_file_read = 0;
	printf("CPU_COUNT=%d\n", g_total_cpus);
}

/*void write_output_trace_file(char *filename){*/
	/*FILE *fp = fopen(filename, "w");*/
	/*if(fp){*/
		/*ProcessControlBlock *proc;*/
		/*LinkedList *q = &g_finished_processes;*/
		/*LLNode *cur = q->head;*/
		/*double tr, tf;*/
		/*while(cur->next){*/
			/*cur = cur->next;*/
			/*proc = cur->data;*/
			/*tf = proc->tf/1000000.0;*/
			/*tr = tf - proc->info->t0;*/
			/*fprintf(fp, "%s %.2f %.2f\n", proc->info->process_name, tf, tr);*/
		/*}*/
		/*fprintf(fp, "%d\n", g_context_switch_count);*/
		/*fclose(fp);*/
	/*}else{*/
		/*fprintf(stderr, "ERROR: couldnt write output file to %s.\n", filename);*/
		/*exit(1);*/
	/*}*/
/*}*/

void run_simulation(int scheduler_type, char *input_filename, char *output_filename){
	init_globals();

	scheduler_start(scheduler_type-1);

	parse_tracefile(input_filename);

	scheduler_join();
	/*write_output_trace_file(output_filename);*/
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
