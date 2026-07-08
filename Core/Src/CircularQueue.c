#include "CircularQueue.h"
#include <string.h>
#include <stdlib.h>

void CQ_init(CircQueue *queue, void* buffer, size_t size, size_t elem_size){
	queue->buff = buffer;
	queue->lenght = size;
	queue->elem_size = elem_size;
	queue->testa = 0;
	queue->coda = 0;
	queue->count = 0;
}

void CQ_push(CircQueue *queue, const void* buffer){
	
	void * dest = (void *)queue->buff + (queue->testa * queue->elem_size);
	memcpy(dest, buffer, queue->elem_size);

	queue->testa = (queue->testa + 1) % queue->lenght;

	if (queue->count == queue->lenght)
		return;
	
	queue->count++;
	
}

uint8_t CQ_pop(CircQueue *queue, void *dest){
	if (queue->count == 0)
		return 0;
	
	void *src = (void*)queue->buff + (queue->coda * queue->elem_size);
	memcpy(dest,src,queue->elem_size);

    queue->coda = (queue->coda + 1) % queue->lenght;
    queue->count--;
	
	return 1;
}
