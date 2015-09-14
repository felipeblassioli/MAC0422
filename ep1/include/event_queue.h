#ifndef _INCL_EVTQUEUE
#define _INCL_EVTQUEUE

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

typedef enum { EVT_NONE, EVT_NEW, EVT_READY, EVT_DISPATCH, EVT_INTERRUPT, EVT_INTERRUPTED, EVT_EXIT, EVT_STOP } EventType;
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

void EventQueue_init(EventQueue *q);
void enqueue(EventQueue *q, EventType type, void *payload);
Event *dequeue(EventQueue *q);

#endif
