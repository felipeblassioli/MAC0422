#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#define FIRST_COME_FIRST_SERVED 1
#define SHORTEST_JOB_FIRST 2
#define SHORTEST_REMAINING_TIME_NEXT 3
#define ROUND_ROBIN 4
#define PRIORITY_SCHEDULING 5
#define REALTIME_HARD_DEADLINE 6

#define TRACE_LINE_FMT "%f %s %f %f %d"

typedef struct {
	float t0;
	char process_name[1024];
	float dt;
	float deadline;
	int priority;
} TraceInfo;


void fcfs_scheduler()
{
	printf("bla]\n");
}

void (*schedulers[]) (void) = {
	fcfs_scheduler
};

float g_time;

void simulate(TraceInfo info, SCHEDULER *scheduler)
{
	/* Simulate the history written in the trace file */
	/* Basically at time t simulate the arrival of all processes with the same t0 == t */
	/* The scheduler should deal with it (i.e handle CPU allocation) */

	printf("arrival of [%s] at time [%f].\n", info.process_name, info.t0);
	g_time = info.t0;

	while(1)
	{
		new_processes = get_new_process();
		scheduler->enqueue(new_processes);
	}
}


int main(int argc, char **argv)
{
	int scheduler_type = atoi(argv[1]);
	char *input_filename = argv[2];
	char *output_filename = argv[3];
	
	printf("read [%d] [%s] [%s]\n", scheduler_type, input_filename, output_filename);
	FILE *fp;
	TraceInfo info;

	g_time = 0.0;
	fp = fopen(input_filename, "r");
	if(fp)
	{
		while(fscanf(fp, TRACE_LINE_FMT, &info.t0, info.process_name, &info.dt, &info.deadline, &info.priority) == 5)
		{
			simulate(info);
		}
		fclose(fp);
	}

	schedulers[0]();
	return 0;
}
