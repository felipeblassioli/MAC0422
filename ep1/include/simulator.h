#ifndef __INCL_SIMULATOR
#define __INCL_SIMULATOR

#include <pthread.h>
#define MAX_CPU_NUMBER 64

typedef struct t_traceinfo {
	double t0, dt, deadline;
	int priority;
	char process_name[256];
	int line;
} TraceInfo;

#define NEW 0
#define READY 1
#define RUNNING 2
#define FINISHED 3
typedef struct {
	int id;
	int state;
	double tf;
	TraceInfo *info;
	int cpu;
	double et;
	pthread_t thread;
	int interrupted;
} ProcessControlBlock;

#endif
