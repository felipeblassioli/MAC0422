#ifndef _INCL_EVTQUEUE
#define _INCL_EVTQUEUE

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

typedef enum {EVT_NONE, EVT_NEW, EVT_CONTEXT_SWITCH, EVT_INTERRUPT, EVT_EXITED, EVT_STOP, EVT_DISPATCH} EventType;
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

#include "simulator.h"
typedef struct{
	ProcessControlBlock *old;
	ProcessControlBlock *new;
	int cpu;
} EventContextSwitchPayload;

void EventQueue_init(EventQueue *q);
void enqueue(EventQueue *q, EventType type, void *payload);
Event *dequeue(EventQueue *q);

#endif
