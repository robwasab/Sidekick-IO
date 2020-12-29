/*
 * rjt_uart.c
 *
 * Created: 12/28/2020 6:04:09 PM
 *  Author: robbytong
 */ 

#include <string.h>
#include <usart.h>
#include <asf.h>

#include "rjt_logger.h"
#include "rjt_queue.h"
#include "utils.h"
#include "rjt_uart.h"

/**
 * PB00 - SERCOM5/PAD[2]
 * PB01 - SERCOM5/PAD[3]
 * PB02 - SERCOM5/PAD[0]
 * PB03 - SERCOM5/PAD[1]
 * PB04
 * PB05
 * PB06
 * PB07
 * PB08 - SERCOM4/PAD[0]
 * PB09 - SERCOM4/PAD[1]
 * PB10 - SERCOM4/PAD[2]
 * PB11 - SERCOM4/PAD[3]
 */

static struct usart_module mUart;
static uint16_t mRxByte;

static uint8_t mTxInProgressBuffer[32];
static bool	mTxInProgress;

static uint8_t  mTxQueueBuffer[128];
static RJTQueue mTxQueue;

static uint8_t  mRxQueueBuffer[128];
static RJTQueue mRxQueue;


static void dequeue_and_transmit(void)
{
	system_interrupt_enter_critical_section();

	if(0 < RJTQueue_getNumEnqueued(&mTxQueue))
	{
		// Start transmission
			
		// Dequeue data
		size_t num2dequeue = MIN(RJTQueue_getNumEnqueued(&mTxQueue), sizeof(mTxInProgressBuffer));
		RJTQueue_dequeue(&mTxQueue, mTxInProgressBuffer, num2dequeue);

		// Start transmit job
		enum status_code res =
		usart_write_buffer_job(&mUart, mTxInProgressBuffer, num2dequeue);
			
		ASSERT(STATUS_OK == res);

		//RJTLogger_print("UART: sending %d bytes", num2dequeue);

		mTxInProgress = true;
	}
	else {
		mTxInProgress = false;
	}

	system_interrupt_leave_critical_section();
}


static void callback_transmitted(struct usart_module * module)
{
	//RJTLogger_print("UART: transmitted");

	dequeue_and_transmit();
}

static void callback_received(struct usart_module * module)
{
	//RJTLogger_print("UART: received");

	if(0 < RJTQueue_getSpaceAvailable(&mRxQueue))
	{
		RJTQueue_enqueue(&mRxQueue, (uint8_t *) &mRxByte, sizeof(uint8_t));
	}
	else {
		//RJTLogger_print("UART: rx dropped byte");
	}

	// Start reception again
	enum status_code res;
	res = usart_read_job(&mUart, &mRxByte);
	ASSERT(STATUS_OK == res);
}

static void callback_error(struct usart_module * module)
{
	//RJTLogger_print("UART: error");
}

static void callback_receive_started(struct usart_module * module)
{
	RJTLogger_print("UART: receive started");
}


size_t RJTUart_getWriteQueueAvailableSpace(void)
{
	return RJTQueue_getSpaceAvailable(&mTxQueue);
}


void RJTUart_enqueueIntoWriteQueue(const uint8_t * indata, size_t len)
{
	// Enter critical section, we don't want the queue growing unexpectedly in this code
	system_interrupt_enter_critical_section();

	size_t space_avail = RJTQueue_getSpaceAvailable(&mTxQueue);
	ASSERT(space_avail >= len);

	bool success = RJTQueue_enqueue(&mTxQueue, indata, len);
	ASSERT(true == success);

	if(false == mTxInProgress)
	{
		dequeue_and_transmit();
	}

	system_interrupt_leave_critical_section();
}


size_t RJTUart_getNumReadable(void)
{
	return RJTQueue_getNumEnqueued(&mRxQueue);
}

void RJTUart_dequeueFromReadQueue(uint8_t * outdata, size_t num)
{
	RJTQueue_dequeue(&mRxQueue, outdata, num);
}


void RJTUart_init(void)
{
	// Initialize Tx and Rx Queues
	RJTQueue_init(&mTxQueue, mTxQueueBuffer, sizeof(mTxQueueBuffer));
	RJTQueue_init(&mRxQueue, mRxQueueBuffer, sizeof(mRxQueueBuffer));

	// Initialize UART, use SERCOM4
	struct usart_config config;
	usart_get_config_defaults(&config);

	config.baudrate = 115200;
	config.mux_setting = USART_RX_1_TX_0_XCK_1;
	config.pinmux_pad0 = PINMUX_PB08D_SERCOM4_PAD0;
	config.pinmux_pad1 = PINMUX_PB09D_SERCOM4_PAD1;
	config.pinmux_pad2 = PINMUX_UNUSED;
	config.pinmux_pad3 = PINMUX_UNUSED;

	while(usart_init(&mUart, SERCOM4, &config) != STATUS_OK);

	// Register Callbacks
	usart_register_callback(&mUart, 
		callback_receive_started, USART_CALLBACK_START_RECEIVED);

	usart_register_callback(&mUart,
		callback_received, USART_CALLBACK_BUFFER_RECEIVED);

	usart_register_callback(&mUart,
		callback_transmitted, USART_CALLBACK_BUFFER_TRANSMITTED);

	usart_register_callback(&mUart,
		callback_error, USART_CALLBACK_ERROR);

	usart_enable_callback(&mUart, USART_CALLBACK_START_RECEIVED);
	
	usart_enable_callback(&mUart, USART_CALLBACK_BUFFER_RECEIVED);
	
	usart_enable_callback(&mUart, USART_CALLBACK_BUFFER_TRANSMITTED);

	usart_enable_callback(&mUart, USART_CALLBACK_ERROR);


	usart_enable(&mUart);

	// Read a byte
	enum status_code res;
	res = usart_read_job(&mUart, &mRxByte);
	ASSERT(STATUS_OK == res);
}

void RJTUart_testTransmit(void)
{
	// Test to see what happens when we try to enqueue more data
	// than the tx queue can hold

	uint8_t tx_data[32];

	for(size_t k = 0; k < sizeof(tx_data); k++) {
		char c;
		uint8_t hexno = k % 16;

		if(hexno < 10) {
			c = '0' + hexno;
		}
		else if(hexno < 15) {
			c = 'A' + hexno - 10;
		}
		else {
			c = '\n';
		}

		tx_data[k] = c;
	}

	system_interrupt_enter_critical_section();
	while(0 < RJTUart_getWriteQueueAvailableSpace())
	{
		size_t avail_space = RJTUart_getWriteQueueAvailableSpace();
		size_t num2enqueue = MIN(avail_space, sizeof(tx_data));
		RJTUart_enqueueIntoWriteQueue(tx_data, num2enqueue);
	}
	system_interrupt_leave_critical_section();
}

void RJTUart_testReceive(void)
{
	// Test to see what happens when we try to enqueue more data
	// than the rx queue can hold
}