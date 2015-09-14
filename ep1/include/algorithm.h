#ifndef __INCL_ALGORITHM
#define __INCL_ALGORITHM

#include "simulator.h"
void algorithm_init(int n);
void algorithm_on_ready(ProcessControlBlock **, int);
void algorithm_on_interrupt(ProcessControlBlock *, int);
void algorithm_on_exit(ProcessControlBlock *, int);
#endif
