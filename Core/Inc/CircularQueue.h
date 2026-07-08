#ifndef INC_CIRCULARQUEUE_H_
#define INC_CIRCULARQUEUE_H_

#include "globalVariable.h"

typedef struct{
	void *buff;
	size_t elem_size;
	size_t testa;
	size_t coda;
	size_t count;
	size_t lenght;
}CircQueue;

void CQ_init(CircQueue *, void*, size_t, size_t);
void CQ_push(CircQueue *, const void *);
uint8_t CQ_pop(CircQueue *, void *);

#endif /* INC_CIRCULARQUEUE_H_ */
