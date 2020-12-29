/*
 * rjt_uart.h
 *
 * Created: 12/28/2020 9:53:17 PM
 *  Author: robbytong
 */ 

void RJTUart_init(void);

void RJTUart_dequeueFromReadQueue(uint8_t * outdata, size_t num);

size_t RJTUart_getNumReadable(void);

void RJTUart_enqueueIntoWriteQueue(const uint8_t * indata, size_t len);

size_t RJTUart_getWriteQueueAvailableSpace(void);

void RJTUart_testTransmit(void);

