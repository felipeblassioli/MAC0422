#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include "linked_list.h"

void ll_init(LinkedList *q){
	LLNode *node = malloc(sizeof(*node));
	node->prev = (LLNode *)NULL;
	node->next = (LLNode *)NULL;
	LL_DATA_TYPE dummy = malloc(sizeof(*dummy));
	dummy->id = -1;
	node->data = dummy;
	q->head = node;
	q->size = 0;

	pthread_mutex_init(&q->mutex, NULL);
	sem_init(&q->_size, 0, 0);
}

void ll_destroy(LinkedList *q){
	/*LLNode *tmp;*/
	/*LLNode *cur = q->head;*/
	/*while(cur->next){*/
		/*cur = cur->next;*/
		/*tmp = cur->next;*/
		/*free(cur);*/
		/*cur = tmp;*/
	/*}*/

	free(q->head->data);
	free(q->head);
}

void _ll_insert_beginning(LinkedList *q, LL_DATA_TYPE b){
	LLNode *first, *second;

	first = malloc(sizeof(*first));
	first->data = b;

	second = q->head->next;
	q->head->next = first;
	first->prev = q->head;
	first->next = second;
	if(second)
		second->prev = first;

	q->size++;
	/*printf("insert size=%d\n", q->size);*/
	sem_post(&q->_size);
}

void ll_insert_beginning(LinkedList *q, LL_DATA_TYPE b){
	pthread_mutex_lock(&q->mutex);
	_ll_insert_beginning(q,b);
	pthread_mutex_unlock(&q->mutex);

	//printf("\t inserted proc_id = %d at address = %p \n", first->data->id, first);
}

void ll_insert_beginning_batch(LinkedList *q, LL_DATA_TYPE *batch, int size){
	pthread_mutex_lock(&q->mutex);
	while(size--)
		_ll_insert_beginning(q, batch[size]);	
	pthread_mutex_unlock(&q->mutex);
}

void _ll_insert_last(LinkedList *q, LL_DATA_TYPE b){
	LLNode *cur;
	LLNode *last = malloc(sizeof(*last));
	last->next = (LLNode *)NULL;
	last->data = b;

	cur = q->head;
	while(cur->next)
		cur = cur->next;

	cur->next = last;
	last->prev = cur;

	q->size++;
	sem_post(&q->_size);
	/*printf("AFTER INSERT");*/
	/*ll_print(q);*/

}

void ll_insert_last(LinkedList *q, LL_DATA_TYPE b){
	pthread_mutex_lock(&q->mutex);
	_ll_insert_last(q,b);
	pthread_mutex_unlock(&q->mutex);
}

void ll_insert_last_batch(LinkedList *q, LL_DATA_TYPE *batch, int size){
	pthread_mutex_lock(&q->mutex);
	while(size--)
		_ll_insert_last(q, batch[size]);	
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
	/*fprintf(stderr, "LL_ERROR: id=%d not found!.\n", id);*/
	/*ll_print(q);*/
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
	fprintf(stderr, "LL_ERROR: index=%d not found!.\n", index);
	ll_print(q);
	return ((LLNode *)NULL);	
}

int ll_in(LinkedList *q, int id){
	if(ll_find(q,id))
		return 1;
	else
		return 0;
}

LL_DATA_TYPE _ll_remove_node(LinkedList *q, LLNode *node){
	LL_DATA_TYPE result;

	/*printf("\t removing node with proc_id = %d\n", node->data->id);*/
	if(node->prev){
		/*printf("\t\t prev proc_id = %d\n", node->prev->data->id);*/
		node->prev->next = node->next;
	}
	if(node->next){
		/*printf("\t\t next proc_id = %d\n", node->next->data->id);*/
		node->next->prev = node->prev;
	}
	result = node->data;

	free(node);
	return result;
}

LL_DATA_TYPE ll_remove_index(LinkedList *q, int index){
	sem_wait(&q->_size);
	pthread_mutex_lock(&q->mutex);

	LL_DATA_TYPE result;

	LLNode *node = ll_find_index(q, index);
	result = _ll_remove_node(q, node);
	q->size--;
	/*printf("remove size=%d\n", q->size);*/

	pthread_mutex_unlock(&q->mutex);
	return result;
}

LL_DATA_TYPE ll_remove(LinkedList *q, int id){
	/*int tmp;*/
	/*sem_getvalue(&q->_size, &tmp);*/
	/*printf("before lock size=%d tmp=%d\n", q->size, tmp);*/
	//sem_wait(&q->_size);
	/*printf("after lock\n");*/
	//pthread_mutex_lock(&q->mutex);

	LL_DATA_TYPE result;

	LLNode *node = ll_find(q, id);
	if(node){
		result = _ll_remove_node(q, node);
		q->size--;
	}

	//pthread_mutex_unlock(&q->mutex);
	return result;
}

void ll_print(LinkedList *q){
	LLNode *cur = q->head;
	printf("\tQUEUE %d = [ ", q->size);
	while(cur->next){
		cur = cur->next;
		printf("{ %d-%s-[cpu=%d, state=%d]} ", cur->data->id, cur->data->info->process_name, cur->data->cpu, cur->data->state);
	}
	printf("]\n");
}

void ll_copy(LinkedList *dst, LinkedList *src){
	ll_init(dst);
	LLNode *cur = src->head;
	while(cur->next){
		cur = cur->next;
		_ll_insert_beginning(dst, cur->data);
	}
}
