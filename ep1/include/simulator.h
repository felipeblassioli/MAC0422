#ifndef __INCL_SIMULATOR
#define __INCL_SIMULATOR

#define MAX_CPU_NUMBER 64
#define READY 1
#define RUNNING 2
#define FINISHED 3

typedef struct t_traceinfo {
	double t0, dt, deadline;
	int priority;
	char process_name[256];
	int line;
} TraceInfo;

typedef struct {
	int id;
	int state;
	double tf;
	TraceInfo *info;
	int cpu;
	double et;
} ProcessControlBlock;

#define RTIME_INFINITY 9999.999
double remaining_time(ProcessControlBlock *);
#endif
