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
#include "udi_vendor.h"

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

static uint8_t mTxInProgressBuffer[32];
static bool	mTxInProgress;

static uint8_t  mTxQueueBuffer[256];
static RJTQueue mTxQueue;

// The way this is coded, mRxRingBuf size has to be a power of 2
#define RX_RING_BUF_LOG2_OF_SIZE	10
#define RX_RING_BUF_MASK					((1 << RX_RING_BUF_LOG2_OF_SIZE) - 1)
static uint8_t  mRxRingBuf[(1 << RX_RING_BUF_LOG2_OF_SIZE)];
static size_t   mRxRingBufIndex = 0;


static volatile bool mCDCEnabled = false;
static uint32_t mCurrentTicks;
static uint32_t mLastTicks;


// The DMA channel object
static struct dma_resource mDMA;

// The DMA memory descriptor the UART rx buffer
static COMPILER_ALIGNED(16) 
DmacDescriptor mDMADescriptor SECTION_DMAC_DESCRIPTOR;

// The Event channel for re-triggering the DMAC
// it's event output is re-routed back to it's input
static struct events_resource mDMAEventChannel;

extern DmacDescriptor _write_back_section[CONF_MAX_USED_CHANNEL_NUM];


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
		size_t num2dequeue = 
			MIN(RJTQueue_getNumEnqueued(&mTxQueue), sizeof(mTxInProgressBuffer));
		
		RJTQueue_dequeue(
			&mTxQueue, 
			mTxInProgressBuffer, 
			num2dequeue);

		// Start transmit job
		enum status_code res =
		usart_write_buffer_job(
			&mUart, 
			mTxInProgressBuffer, 
			num2dequeue);
			
		ASSERT(STATUS_OK == res);

		RJTLogger_print("UART: sending %d bytes", num2dequeue);

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

	//RJTLogger_print("uart tx %d bytes", len);
}


/**
 * Returns the index where the DMAC will write to next
 */
static size_t get_rx_ringbuf_pos(void)
{
	/*
		RJTLogger_print(
			"[%d] num trans: %d", 
			DMAC->ACTIVE.bit.ID, 
			DMAC->ACTIVE.bit.BTCNT);
	*/
	// BTCNT represents number of bytes to send until the ring buffer is 
	// depleted.
	// BTCNT starts off at zero, but after the DMAC transfers 1 byte, BTCNT
	// becomes buffer size - 1. After transferring 2 bytes, it 
	// reads buffer size - 2
	return (sizeof(mRxRingBuf) - DMAC->ACTIVE.bit.BTCNT) % sizeof(mRxRingBuf);
}


/**
 * Get the number of bytes available for reading (data received by the uart)
 */
static size_t get_num_readable(void)
{
	size_t ringbuf_pos = get_rx_ringbuf_pos();
	size_t num_avail = (ringbuf_pos - mRxRingBufIndex) & RX_RING_BUF_MASK;

	return num_avail;
}


/**
 * Dequeue data from the read queue (data received by the uart)
 */
static void dequeue_from_read_queue(uint8_t * outdata, size_t num)
{
	ASSERT(num <= get_num_readable());

	for(size_t k = 0; k < num; k++) {
		outdata[k] = mRxRingBuf[mRxRingBufIndex];
		mRxRingBufIndex = (mRxRingBufIndex + 1) & RX_RING_BUF_MASK;
	}
}


struct CachedBuffer {
	uint8_t  buf[64];
	 size_t  buf_pos;
	 size_t  buf_size;
	   bool  dirty;
};


/**
 * Send data to host.
 */
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

			// read from the uart rx buffer
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


/**
 * Retrieve data written to us from the host.
 */
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

	#if 0
	if(mCurrentTicks > mLastTicks + 100)
	{
		mLastTicks = mCurrentTicks;

		RJTUart_testTransmit();
	}
	#endif
}

#pragma GCC pop_options


#if 0
// The Event channel callback object for attaching a callback
// handler to a particular event channel
static struct events_hook     mDMAEventHook;


static void event_callback(struct events_resource * resource)
{
	if(events_is_interrupt_set(resource, EVENTS_INTERRUPT_DETECT))
	{
		events_ack_interrupt(resource, EVENTS_INTERRUPT_DETECT);
		RJTLogger_print("dma beat callback");
	}
}
#endif


static void dma_sof_callback(void)
{
	static uint32_t count = 0;

	count += 1;

	if(count >= 1000) {
		count = 0;

		RJTLogger_print("dma ptr [%d / %d]", mRxRingBufIndex, get_rx_ringbuf_pos());
	}
}

SOF_REGISTER_CALLBACK(dma_sof_callback);

/**
 * Create an event channel to route:
 * DMA event beat transfer -> DMA trigger
 * This way, the DMA re-arms itself everytime it transfers a byte
 */
static void init_beat_event(void)
{
	struct events_config config;

	events_get_config_defaults(&config);

	// the event generator comes from DMAC channel 0
	config.generator    = EVSYS_ID_GEN_DMAC_CH_0;
	config.edge_detect  = EVENTS_EDGE_DETECT_RISING;
	config.path         = EVENTS_PATH_RESYNCHRONIZED;
	config.clock_source = GCLK_GENERATOR_0;
	
	events_allocate(&mDMAEventChannel, &config);

	events_attach_user(&mDMAEventChannel, EVSYS_ID_USER_DMAC_CH_0);

	#if 0
	events_create_hook(&mDMAEventHook, event_callback);

	events_add_hook(&mDMAEventChannel, &mDMAEventHook);

	events_enable_interrupt_source(&mDMAEventChannel, EVENTS_INTERRUPT_DETECT);

	#endif

	while(events_is_busy(&mDMAEventChannel)) {
		__NOP();
	}
}


static void init_dmac(void)
{
	// configure dma resource
	struct dma_resource_config config;
	dma_get_config_defaults(&config);

	config.event_config.input_action = DMA_EVENT_INPUT_CTRIG;
	config.peripheral_trigger = SERCOM4_DMAC_ID_RX;

	config.trigger_action = DMA_TRIGGER_ACTION_BEAT;
	config.event_config.event_output_enable = true;
	
	dma_allocate(&mDMA, &config);

	
	// configure dma descriptor
	struct dma_descriptor_config desc_config;
	dma_descriptor_get_config_defaults(&desc_config);

	desc_config.event_output_selection = DMA_EVENT_OUTPUT_BEAT;
	
	// After a block has been transferred, do nothing...
	// here is where we configure whether or not it should interrupt
	desc_config.block_action = DMA_BLOCK_ACTION_NOACT;
	
	// do not increment source address
	desc_config.src_increment_enable = false;
	desc_config.source_address = (uint32_t) &SERCOM4->USART.DATA.reg;

	
	desc_config.block_transfer_count = sizeof(mRxRingBuf);

	// Still have no idea why we add by the size of the destination buffer
	// just following the example
	desc_config.destination_address = ((uint32_t)mRxRingBuf) + sizeof(mRxRingBuf);
	
	// increment destination address automatically
	desc_config.dst_increment_enable = true;

	// point to ourselves so that the DMA keeps repeating this descriptor transfer
	desc_config.next_descriptor_address = (uint32_t) &mDMADescriptor;

	dma_descriptor_create(&mDMADescriptor, &desc_config);

	dma_add_descriptor(&mDMA, &mDMADescriptor);

	dma_start_transfer_job(&mDMA);
}


void RJTUart_init(void)
{
	// Initialize Tx and Rx Queues
	RJTQueue_init(&mTxQueue, mTxQueueBuffer, sizeof(mTxQueueBuffer));

	// Initialize UART, use SERCOM4
	struct usart_config config;
	usart_get_config_defaults(&config);

	config.baudrate = 115200;
	config.mux_setting = USART_RX_1_TX_0_XCK_1;
	//config.pinmux_pad0 = PINMUX_PB08D_SERCOM4_PAD0;
	config.pinmux_pad0 = PINMUX_PA12D_SERCOM4_PAD0;
	
	//config.pinmux_pad1 = PINMUX_PB09D_SERCOM4_PAD1;
	config.pinmux_pad1 = PINMUX_PA13D_SERCOM4_PAD1;

	config.pinmux_pad2 = PINMUX_UNUSED;
	config.pinmux_pad3 = PINMUX_UNUSED;
	
	NVIC_SetPriority(SERCOM4_IRQn, APP_LOW_PRIORITY);

	while(usart_init(&mUart, SERCOM4, &config) != STATUS_OK);

	// Register Callbacks

	usart_register_callback(&mUart,
		callback_transmitted, USART_CALLBACK_BUFFER_TRANSMITTED);
	
	usart_enable_callback(&mUart, USART_CALLBACK_BUFFER_TRANSMITTED);

	usart_enable_callback(&mUart, USART_CALLBACK_ERROR);

	usart_enable(&mUart);

	init_dmac();

	init_beat_event();

	events_trigger(&mDMAEventChannel);
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
