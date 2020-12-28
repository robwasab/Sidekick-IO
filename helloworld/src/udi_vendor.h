/*
 * udi_vendor.h
 *
 * Created: 11/8/2020 12:57:40 PM
 *  Author: robbytong
 */ 


#ifndef UDI_VENDOR_H_
#define UDI_VENDOR_H_


#include "conf_usb.h"
#include "usb_protocol.h"
#include "udd.h"
#include "udc_desc.h"
#include "udi.h"

extern UDC_DESC_STORAGE udi_api_t udi_api_vendor;

/**
 * Vendor Descriptor:
 * 1 Interface with
 * 1 OUT endpoint for writing to the function
 * 1 IN  endpoint for reading from the function
 * 1 IN Interrupt for sending notifications from the function
 */
typedef struct {
	usb_iface_desc_t  iface;
	   usb_ep_desc_t  ep_write;
	   usb_ep_desc_t  ep_read;
	   usb_ep_desc_t  ep_notify;
} udi_vendor_desc_t;

#define UDI_VENDOR_EP_SIZE				64

#define UDI_VENDOR_DESC {		  														\
	.iface.bLength            = sizeof(usb_iface_desc_t),		\
  .iface.bDescriptorType    = USB_DT_INTERFACE,						\
	.iface.bAlternateSetting  = 0,													\
	.iface.bNumEndpoints		  = 3,													\
	.iface.bInterfaceClass    = 0xff,												\
  .iface.bInterfaceSubClass = 0xff,												\
	.iface.bInterfaceProtocol = 0xff,											  \
  .iface.iInterface         = 0,                          \
  .iface.bInterfaceNumber   = UDI_VENDOR_IFACE_NUMBER,    \
  .ep_write.bLength         = sizeof(usb_ep_desc_t),      \
  .ep_write.bDescriptorType = USB_DT_ENDPOINT,            \
  .ep_write.bEndpointAddress= UDI_VENDOR_EP_WRITE_ADDR,   \
  .ep_write.bmAttributes    = USB_EP_TYPE_BULK,						\
  .ep_write.bInterval       = 0,                          \
	.ep_write.wMaxPacketSize  = LE16(UDI_VENDOR_EP_SIZE),   \
  .ep_read.bLength         = sizeof(usb_ep_desc_t),       \
  .ep_read.bDescriptorType = USB_DT_ENDPOINT,             \
  .ep_read.bEndpointAddress= UDI_VENDOR_EP_READ_ADDR,     \
  .ep_read.bmAttributes    = USB_EP_TYPE_BULK,						\
  .ep_read.bInterval       = 0,                           \
  .ep_read.wMaxPacketSize  = LE16(UDI_VENDOR_EP_SIZE),    \
  .ep_notify.bLength         = sizeof(usb_ep_desc_t),     \
  .ep_notify.bDescriptorType = USB_DT_ENDPOINT,           \
  .ep_notify.bEndpointAddress= UDI_VENDOR_EP_NOTIFY_ADDR, \
  .ep_notify.bmAttributes    = USB_EP_TYPE_INTERRUPT,		  \
  .ep_notify.bInterval       = 100,                       \
  .ep_notify.wMaxPacketSize  = LE16(UDI_VENDOR_EP_SIZE)   \
}


#endif /* UDI_VENDOR_H_ */