/*
 * rjt_uart.c
 *
 * Created: 12/28/2020 6:04:09 PM
 *  Author: robbytong
 */ 

#include <string.h>
#include <usart.h>
#include <asf.h>
#include <conf_usb.h>

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

static volatile bool mCDCEnabled = false;
static uint32_t mCurrentTicks;
static uint32_t mLastTicks;

/************************************************************************
 * USB CDC Callbacks
 ************************************************************************/


bool user_callback_cdc_enable(void)
{
	RJTLogger_print("cdc enabled");
	 
	mCDCEnabled = true;

	mCurrentTicks = 0;
	mLastTicks = 0;
	return true;
}


 void user_callback_cdc_disable(uint8_t port)
 {
	 (void) port;
	 mCDCEnabled = false;
 }


 void user_callback_cdc_set_line_coding(uint8_t port, usb_cdc_line_coding_t * cfg)
 {
	 RJTLogger_print("setting line encoding...");
	 RJTLogger_print("baud: %d", cfg->dwDTERate);
 }


 /**
  * Called after USB host sent us data
	*/
 void user_callback_cdc_rx_notify(uint8_t port)
 {
	 //RJTLogger_print("rx notify!");
 }

/************************************************************************
 * UART Callbacks
 ************************************************************************/

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


/************************************************************************
 * Private
 ************************************************************************/

/**
 * Return the available space in the uart tx queue (data we send out)
 */
static size_t get_write_queue_available_space(void)
{
	return RJTQueue_getSpaceAvailable(&mTxQueue);
}


/**
 * Enqueue data in the write queue (data we send out)
 */
static void enqueue_into_write_queue(const uint8_t * indata, size_t len)
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


/**
 * Get the number of bytes available for reading (data received by the uart)
 */
static size_t get_num_readable(void)
{
	return RJTQueue_getNumEnqueued(&mRxQueue);
}


/**
 * Dequeue data from the read queue (data received by the uart)
 */
static void dequeue_from_read_queue(uint8_t * outdata, size_t num)
{
	RJTQueue_dequeue(&mRxQueue, outdata, num);
}

struct CachedBuffer {
	uint8_t  buf[64];
	 size_t  buf_pos;
	 size_t  buf_size;
	   bool  dirty;
};


static void process_cdc_tx(void)
{
	static struct CachedBuffer write_buf = {0};

	// Send the UART RX Data (to the host)
	if(false == udi_cdc_is_tx_ready()) {
		return;
	}

	if(false == write_buf.dirty)
	{
		// load data into write buffer
		size_t num_readable = get_num_readable();
				
		if(0 < num_readable) {
			size_t num2read;

			num2read = MIN(sizeof(write_buf.buf), num_readable);

			dequeue_from_read_queue(write_buf.buf, num2read);
					
			// reset buffer to beginning
			write_buf.buf_size = num2read;
			write_buf.buf_pos = 0;
			write_buf.dirty = true;
		}
	}
			
	if(true == write_buf.dirty)
	{
		iram_size_t num_written = 0;

		uint8_t * buf_ptr = &write_buf.buf[write_buf.buf_pos];
		size_t buf_len = write_buf.buf_size - write_buf.buf_pos;
				
		enum UDI_CDC_STATUS status =
		udi_cdc_multi_write_buf_no_block(
			0,
			buf_ptr,
			buf_len,
			&num_written);

		if(UDI_CDC_STATUS_OK == status) {
			// advance buffer pointer using num_written variable
			write_buf.buf_pos += num_written;

			ASSERT(write_buf.buf_pos <= write_buf.buf_size);

			if(write_buf.buf_pos == write_buf.buf_size) {
				// we've consumed all the data
				write_buf.dirty = false;
			}
		}
		else {
			// An error occurred, try again later
		}
	}
}


static void process_cdc_rx(void)
{
	static struct CachedBuffer read_buf = {0};

	// Read the data the host sent us, so we can send it (via UART tx)
	
	if(false == udi_cdc_is_rx_ready()) {
		return;
	}
	

	if(false == read_buf.dirty)
	{
		iram_size_t num_avail = udi_cdc_get_nb_received_data();

		if(0 < num_avail)
		{
			size_t num2read;

			// read the received data
			num2read = MIN(num_avail, sizeof(read_buf.buf));

			iram_size_t num_read = 0;
			enum UDI_CDC_STATUS status = 
				udi_cdc_multi_read_no_block(0, read_buf.buf, num2read, &num_read);
	
			if(UDI_CDC_STATUS_OK == status) {
				ASSERT(0 < num_read);

				read_buf.buf_pos = 0;
				read_buf.buf_size = num_read;
				read_buf.dirty = true;
			}
			else {
				// an error occurred
			}
		}
	}

	if(true == read_buf.dirty)
	{
		size_t write_queue_space = get_write_queue_available_space();
		
		size_t buf_len = read_buf.buf_size - read_buf.buf_pos;
		uint8_t * buf_ptr = &read_buf.buf[read_buf.buf_pos];

		size_t num2enqueue = MIN(buf_len, write_queue_space);
		
		enqueue_into_write_queue(buf_ptr, num2enqueue);
		
		read_buf.buf_pos += num2enqueue;

		ASSERT(read_buf.buf_pos <= read_buf.buf_size);

		if(read_buf.buf_pos == read_buf.buf_size) {
			// consumed all the data
			read_buf.dirty = false;
		}
	}
}


/************************************************************************
 * Public
 ************************************************************************/

void RJTUart_sofCallback(void)
{
	++mCurrentTicks;
}


/**
 * This could be optimized
 */
#pragma GCC push_options
#pragma GCC optimize("O0")

void RJTUart_processCDC(void)
{
	if(false == mCDCEnabled) {
		return;
	}

	process_cdc_tx();

	process_cdc_rx();

	//system_interrupt_leave_critical_section();

	if(mCurrentTicks > mLastTicks + 100)
	{
		mLastTicks = mCurrentTicks;

		RJTUart_testTransmit();
	}
}

#pragma GCC pop_options


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
	while(0 < get_write_queue_available_space())
	{
		size_t avail_space = get_write_queue_available_space();
		size_t num2enqueue = MIN(avail_space, sizeof(tx_data));
		enqueue_into_write_queue(tx_data, num2enqueue);
	}
	system_interrupt_leave_critical_section();
}
