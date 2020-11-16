/*
 * rjt_queue.h
 *
 * Created: 11/8/2020 11:10:55 AM
 *  Author: robbytong
 */ 


#ifndef RJT_QUEUE_H_
#define RJT_QUEUE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

struct RJTQueue
{
	uint8_t* data;
	size_t  size;
	size_t  max_len;
	size_t  head;
	size_t  tail;
};

typedef struct RJTQueue RJTQueue;

bool RJTQueue_pop(RJTQueue * self, uint8_t * val);
bool RJTQueue_push(RJTQueue * self, uint8_t val);
bool RJTQueue_dequeue(RJTQueue * self, uint8_t * dst, size_t len);
size_t RJTQueue_getSpaceAvailable(RJTQueue * self);
size_t RJTQueue_getNumEnqueued(RJTQueue * self);
bool RJTQueue_enqueue(RJTQueue * self, uint8_t const * buffer, size_t len);
void RJTQueue_init(RJTQueue * self, uint8_t * buffer, size_t len);
void RJTQueue_test(void);
void RJTQueue_reset(RJTQueue * self);


#endif /* RJT_QUEUE_H_ */