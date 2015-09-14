#ifndef __INCL_LLIST
#define __INCL_LLIST
#include <pthread.h>
#include <semaphore.h>

#include "simulator.h"
#define LL_DATA_TYPE ProcessControlBlock *

typedef struct LLNode {
	struct LLNode *next;
	struct LLNode *prev;
	LL_DATA_TYPE data;
} LLNode;

typedef struct {
	LLNode *head;
	pthread_mutex_t mutex;
	int size;
	sem_t _size;
} LinkedList;

void ll_init(LinkedList *q);
void ll_destroy(LinkedList *q);
void ll_copy(LinkedList *dst, LinkedList *src);
void ll_insert_beginning(LinkedList *q, LL_DATA_TYPE b);
void ll_insert_beginning_batch(LinkedList *q, LL_DATA_TYPE *b, int size);
void ll_insert_last(LinkedList *q, LL_DATA_TYPE b);
void ll_insert_last_batch(LinkedList *q, LL_DATA_TYPE *b, int size);
LLNode *ll_find(LinkedList *q, int id);
int ll_in(LinkedList *q, int id);
LLNode *ll_find_index(LinkedList *q, int index);
LL_DATA_TYPE ll_remove(LinkedList *q, int id);
LL_DATA_TYPE ll_remove_index(LinkedList *q, int index);

void ll_print(LinkedList *q);
#endif
