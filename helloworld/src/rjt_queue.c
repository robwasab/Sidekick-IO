/*
 * rjt_queue.c
 *
 * Created: 11/8/2020 11:07:04 AM
 *  Author: robbytong
 */ 

 #include "rjt_logger.h"
 #include "rjt_queue.h"
 #include "utils.h"

 #include <string.h>
 #include <asf.h>

 #define CRITICAL_REGION_ENTER()		system_interrupt_enter_critical_section()
 #define CRITICAL_REGION_EXIT()			system_interrupt_leave_critical_section()


 void RJTQueue_reset(RJTQueue * self)
 {
	 CRITICAL_REGION_ENTER();

	 self->size = 0;
	 self->head = 0;
	 self->tail = 0;

	 CRITICAL_REGION_EXIT();
 }


 bool RJTQueue_pop(RJTQueue * self, uint8_t * val)
 {
	 bool success = false;

	 CRITICAL_REGION_ENTER();

	 if(self->size > 0)
	 {
		 *val = self->data[self->head];
		 self->head = (self->head + 1) % self->max_len;
		 self->size -= 1;
		 success = true;
	 }

	 CRITICAL_REGION_EXIT();

	 return success;
 }


 bool RJTQueue_push(RJTQueue * self, uint8_t val)
 {
	 bool success = false;

	 CRITICAL_REGION_ENTER();

	 if(self->size < self->max_len)
	 {
		 self->data[self->tail] = val;
		 self->tail = (self->tail + 1) % self->max_len;
		 self->size += 1;
		 success = true;
	 }

	 CRITICAL_REGION_EXIT();

	 return success;
 }


 bool RJTQueue_dequeue(RJTQueue * self, uint8_t * dst, size_t len)
 {
	 bool success = false;

	 CRITICAL_REGION_ENTER();

	 if(len > 0 && len <= self->size)
	 {
		 size_t new_head = (self->head + len) % self->max_len;

		 if(new_head <= self->head)
		 {
			 // Have to deal with wrap around, ugh...
			 size_t upper_len = self->max_len - self->head;
			 size_t bottm_len = len - upper_len;

			 memcpy(dst, &self->data[self->head], upper_len);

			 // now take into account the bottom portion
			 memcpy(&dst[upper_len], &self->data[0], bottm_len);

			 //RLPRINTF("Dequeue: handling wrap around");
		 }
		 else
		 {
			 // Straight forward!
			 // No need to account for roll over
			 memcpy(dst, &self->data[self->head], len);

			 //RLPRINTF("Dequeue: no roll over");
		 }
		 self->size -= len;
		 self->head = new_head;
		 success = true;
	 }

	 CRITICAL_REGION_EXIT();

	 return success;
 }


 size_t RJTQueue_getSpaceAvailable(RJTQueue * self)
 {
	 return self->max_len - self->size;
 }


 size_t RJTQueue_getNumEnqueued(RJTQueue * self)
 {
	 return self->size;
 }


 bool RJTQueue_enqueue(RJTQueue * self, uint8_t const * buffer, size_t len)
 {
	 bool success = false;

	 if(0 == len) {
		 return true;
	 }

	 CRITICAL_REGION_ENTER();

	 if(len > 0 && len + self->size <= self->max_len)
	 {
		 size_t new_tail = (self->tail + len) % self->max_len;

		 if(new_tail <= self->tail)
		 {
			 // Data wrapped around so we have to account for that
			 // First copy enough data until the end of the array
			 size_t upper_len = self->max_len - self->tail;
			 size_t bottm_len = len - upper_len;

			 memcpy(&self->data[self->tail], buffer, upper_len);

			 // now take into account the bottom portion
			 memcpy(&self->data[0], &buffer[upper_len], bottm_len);

			 //RLPRINTF("Enqueue: handling wrap around");
		 }
		 else
		 {
			 // No need to account for roll over
			 memcpy(&self->data[self->tail], buffer, len);

			 //RLPRINTF("Enqueue: no roll over");
		 }
		 self->size += len;
		 self->tail = new_tail;
		 success = true;
	 }

	 CRITICAL_REGION_EXIT();

	 return success;
 }


 void RJTQueue_init(RJTQueue * self, uint8_t * buffer, size_t len)
 {
	 ASSERT(self != NULL);
	 ASSERT(buffer != NULL);

	 self->data = buffer;
	 self->max_len = len;
	 self->size = 0;
	 self->head = 0;
	 self->tail = 0;
 }


 void RJTQueue_test(void)
 {
	 bool success;
	 RJTQueue test_queue;

	 uint8_t buffer[512];

	 RJTQueue_init(&test_queue, buffer, sizeof(buffer));

	 // Test enqueuing the size of the queue
	 uint8_t test_data[sizeof(buffer)];
	 for(uint32_t k = 0; k < sizeof(test_data); k++) {
		 test_data[k] = k;
	 }
	 success = RJTQueue_enqueue(&test_queue, test_data, sizeof(test_data));
	 ASSERT(true == success);


	 memset(test_data, 0, sizeof(test_data));

	 // Try to enqueue more data
	 success = RJTQueue_enqueue(&test_queue, test_data, sizeof(test_data));
	 ASSERT(false == success);

	 success = RJTQueue_push(&test_queue, 0);
	 ASSERT(false == success);

	 // Read all the data
	 success = RJTQueue_dequeue(&test_queue, test_data, sizeof(test_data));

	 for(uint32_t k = 0; k < sizeof(test_data); k++) {
		 ASSERT((k&0xff) == test_data[k]);
	 }

	 // Enqueue half of the data
	 for(uint32_t k = 0; k < sizeof(test_data)/2; k++) {
		 test_data[k] = k;
	 }

	 success = RJTQueue_enqueue(&test_queue, test_data, sizeof(test_data)/2);
	 ASSERT(true == success);

	 // Dequeue it
	 memset(test_data, 0, sizeof(test_data));
	 success = RJTQueue_dequeue(&test_queue, test_data, sizeof(test_data)/2);
	 ASSERT(true == success);

	 // Do another full size test
	 memset(test_data, 0, sizeof(test_data));
	 
	 for(uint32_t k = 0; k < sizeof(test_data); k++) {
		 test_data[k] = k;
	 }

	 success = RJTQueue_enqueue(&test_queue, test_data, sizeof(test_data));
	 ASSERT(true == success);

	 memset(test_data, 0, sizeof(test_data));

	 success = RJTQueue_dequeue(&test_queue, test_data, sizeof(test_data));
	 ASSERT(true == success);

	 for(uint32_t k = 0; k < sizeof(test_data); k++) {
		 ASSERT(test_data[k] == (k&0xff));
	 }

	 ASSERT(0 == RJTQueue_getNumEnqueued(&test_queue));

	 // Test push and pop
	 for(uint32_t k = 0; k < sizeof(test_data); k++) {
		 success = RJTQueue_push(&test_queue, (k&0xff));
		 ASSERT(true == success);
	 }

	 for(uint32_t k = 0; k < sizeof(test_data); k++) {
		 uint8_t popped_byte;

		 success = RJTQueue_pop(&test_queue, &popped_byte);
		 ASSERT(true == success);
		 
		 ASSERT(popped_byte == (k&0xff));
	 }


	 RJTLogger_print("RLQueue test success!");
	 
 }