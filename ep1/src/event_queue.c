#include <stdio.h>
#include "event_queue.h"

void EventQueue_init(EventQueue *q){
	sem_init(&q->items, 0, 0);
	sem_init(&q->slots, 0, EVENT_QUEUE_MAX_SIZE);
	pthread_mutex_init(&q->mutex, NULL);
	q->insert_index = 0;
	q->remove_index = 0;
}

void enqueue(EventQueue *q, EventType type, void *payload){
	Event *data = malloc(sizeof(*data));
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
