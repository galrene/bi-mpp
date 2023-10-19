#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>

#include "urb.h"
#include "sys.h"

void* system_loop(void* t);

void sys_init(sys_t* sys, hcd_t* hcd) {
	memset(sys, 0, sizeof(sys_t));	
	sys->hcd = hcd;
    if (pthread_create(&sys->system_thread, NULL, system_loop, sys)  < 0) {
		printf("Error: cannot create SYSTEM thread\n");
		exit(1);
	}
}

void sys_destroy(sys_t* sys) {
    pthread_mutex_destroy(&sys->lock);	
}

void __sys_lock(sys_t* sys) {
	pthread_mutex_lock(&sys->lock);
}

void __sys_unlock(sys_t* sys) {
	pthread_mutex_unlock(&sys->lock);
}

void* system_loop(void* t) {
	int i;
	uint8_t* buf;
	sys_t* sys = (sys_t*)t;
	hcd_t* hcd = sys->hcd;
	config_descriptor_t cfg_dsc;
	usb_device_req_t req_get_device_dsc = 
		{ .bmRequestType = 0x80, 
	  	  .bRequest = 0x06,
	      .wValue = 0x0100,
	      .wIndex = 0,
	      .wLength = 18 };

	usb_device_req_t req_get_config_dsc = 
		{ .bmRequestType = 0x80, 
	  	  .bRequest = 0x06,
	  	  .wValue = 0x0200,
	  	  .wIndex = 0,
	      .wLength = 9 };

	usb_device_req_t req_set_address = 
		{ .bmRequestType = 0x00, 
	  	  .bRequest = 0x05,
	  	  .wValue = 0x0001,
	  	  .wIndex = 0,
	      .wLength = 0 };

	usb_device_req_t req_clear_feature = 
		{ .bmRequestType = 0x00, 
	  	  .bRequest = 0x01,
	  	  .wValue = 0x0000,
	  	  .wIndex = 0,
	      .wLength = 0 };

	usb_device_req_t req_set_feature = 
		{ .bmRequestType = 0x00, 
	  	  .bRequest = 0x03,
	  	  .wValue = 0x0000,
	  	  .wIndex = 0,
	      .wLength = 0 };

	usb_device_req_t req_set_configuration = 
		{ .bmRequestType = 0x00, 
	  	  .bRequest = 0x09,
	  	  .wValue = 0x0000,
	  	  .wIndex = 0,
	      .wLength = 0 };

	usb_device_req_t req_set_interface = 
		{ .bmRequestType = 0x01, 
	  	  .bRequest = 0x0B,
	  	  .wValue = 0x0000,
	  	  .wIndex = 0,
	      .wLength = 0 };

	while (1) {
		if (hcd->attached == 0) {
			if (__usb_is_dev_attached()) {
				hcd->attached = 1;
				for(i = 0; i < 3; i++) {
					hcd_bus_reset(hcd);
					if (sys_usb_control_transfer(sys, 0, 0, &req_get_device_dsc, 8, 
						(byte*)&sys->dev_dsc, sizeof(device_descr_t)) != sizeof(device_descr_t))
						continue;
				    if (sys_usb_control_transfer(sys, 0, 0, &req_get_config_dsc, sys->dev_dsc.bMaxPacketSize, 
							(byte*)&cfg_dsc, sizeof(cfg_dsc)) != sizeof(cfg_dsc))
						continue;
					hcd_bus_reset(hcd);
					if (sys_usb_control_transfer(sys, 0, 0, &req_get_device_dsc, sys->dev_dsc.bMaxPacketSize, 
						(byte*)&sys->dev_dsc, sizeof(device_descr_t)) != sizeof(device_descr_t))
						continue;
				    if (sys_usb_control_transfer(sys, 0, 0, &req_set_address, sys->dev_dsc.bMaxPacketSize, 
							NULL, 0) != 0)
						continue;
					if (sys_usb_control_transfer(sys, 1, 0, &req_get_device_dsc, sys->dev_dsc.bMaxPacketSize, 
						(byte*)&sys->dev_dsc, sizeof(device_descr_t)) != sizeof(device_descr_t))
						continue;
				    if (sys_usb_control_transfer(sys, 1, 0, &req_get_config_dsc, sys->dev_dsc.bMaxPacketSize, 
							(byte*)&cfg_dsc, sizeof(cfg_dsc)) != sizeof(cfg_dsc))
						continue;
					sys->cfg_dsc = (config_descriptor_t*)(calloc(cfg_dsc.wTotalLength, 1));
					req_get_config_dsc.wLength = cfg_dsc.wTotalLength;
				    if (sys_usb_control_transfer(sys, 1, 0, &req_get_config_dsc, sys->dev_dsc.bMaxPacketSize, 
							(byte*)sys->cfg_dsc, cfg_dsc.wTotalLength) != cfg_dsc.wTotalLength)
						continue;

					__sys_lock(sys);
					sys->devlist = create_op_rep_list(&sys->dev_dsc, sys->cfg_dsc, &sys->devlist_size);
					sys->dev_ready = 1;
					__sys_unlock(sys);					
/*						
				    if (sys_usb_control_transfer(sys, 5, 0, &req_clear_feature, sys->dev_dsc.bMaxPacketSize, 
							NULL, 0) != 0)
						continue;
				    if (sys_usb_control_transfer(sys, 5, 0, &req_set_feature, sys->dev_dsc.bMaxPacketSize, 
							NULL, 0) != 0)
						continue;
*/						
					req_set_configuration.wValue = cfg_dsc.bConfigurationValue;	
				    if (sys_usb_control_transfer(sys, 1, 0, &req_set_configuration, sys->dev_dsc.bMaxPacketSize, 
							NULL, 0) != 0)
						continue;
				    if (sys_usb_control_transfer(sys, 1, 0, &req_set_interface, sys->dev_dsc.bMaxPacketSize, 
							NULL, 0) != 0)
						continue;
					break;		
				}	
			}	
		} else {
			sleep(1);
		}

	}
	return t;
}

void sys_urb_kill(urb_t* urb) {
}

void sys_urb_unlink(urb_t* urb) {
	sys_t* sys = urb->sys;
	if (sys == NULL) {
		return;
	}
	__sys_lock(sys);
	if (urb == sys->urb_list_head) {
		if (urb == sys->urb_list_tail) {
			sys->urb_list_tail = NULL;
		}
		sys->urb_list_head = urb->next;
	} else {
		urb->prev->next = urb->next;
		urb->next->prev = urb->prev;
	}
	__sys_unlock(sys);
	hcd_gtd_unlink(sys->hcd, urb->td);
	urb->sys = NULL;
	urb->td = NULL;
	return;
}

void sys_urb_submit(sys_t* sys, urb_t* urb) {
	int pid = 0;
	assert(sys != NULL);
	__sys_lock(sys);
	if (sys->urb_list_tail == NULL) {
		sys->urb_list_head = urb;
		sys->urb_list_tail = urb;
	} else {
		sys->urb_list_tail->next = urb;
		urb->prev = sys->urb_list_tail;
		sys->urb_list_tail = urb;
	}
	urb->sys = sys;
	__sys_unlock(sys);
	if (urb->submit.setup.as_int != 0) {
		urb_t __urb;
		int direction = (urb->submit.setup.as_req.bmRequestType & 0x80) != 0;

		urb_init(&__urb);
		__urb.submit.buffer_length = sizeof(urb->submit.setup);		
    	hcd_gtd_link(sys->hcd, urb->submit.devid & 0xFFFF, urb->submit.ep, 
		    0, // direction OUT
			1, // setup
			0, // toggle 
			urb->max_packet_size, urb->submit.setup.as_barr, 
			sizeof(urb->submit.setup), &__urb);
		urb_wait(&__urb, 100);
		urb_destroy(&__urb);
		urb->length = 0;
		if (urb->submit.setup.as_req.wLength > 0) {
			urb_init(&__urb);
			__urb.data = urb->data;
			__urb.submit.buffer_length = urb->submit.buffer_length;
			hcd_gtd_link(sys->hcd, urb->submit.devid & 0xFFFF, urb->submit.ep, 
		    	direction, // direction
				0, // setup
				1, // toggle 
				urb->max_packet_size, 
				urb->data, 
				urb->submit.buffer_length, &__urb);
			urb_wait(&__urb, 100);
			urb->length = __urb.length;
			urb_destroy(&__urb);
		}	
		urb_init(&__urb);
		__urb.submit.buffer_length = 0;		
		hcd_gtd_link(sys->hcd, 
			urb->submit.devid & 0xFFFF, 
			urb->submit.ep, 
		  	!direction, // direction
			0, // setup
			1, // toggle 
			urb->max_packet_size, 
			NULL, 0, &__urb);	  
		urb_wait(&__urb, 100);
		urb_destroy(&__urb);
		urb_done(urb);		
	} else {
    	hcd_gtd_link(sys->hcd, urb->submit.devid & 0xFFFF, urb->submit.ep, 
		    urb->submit.direction, // direction
			0, // setup
			2, // toggle from EPD
			urb->max_packet_size, urb->data, 
			urb->submit.buffer_length, urb);        		
	}
}

int sys_usb_control_transfer(sys_t* sys, int addr, int ep, usb_device_req_t *req, int max_packet_size, uint8_t *data, int size) {
	urb_t* urb = urb_create(NULL, NULL);
	urb->submit.devid = addr;
	urb->submit.ep = ep;
	urb->submit.buffer_length = size;
	urb->submit.direction = ((ep & 0x80) == 0) ? 0 : 1;
	urb->max_packet_size = max_packet_size;
	urb->data = data;
	memcpy(&urb->submit.setup, req, sizeof(urb->submit.setup));	
	sys_urb_submit(sys, urb);
	urb_wait(urb, 100);
	urb_free(urb);
	// TODO: skutecne prijatou delku.
	return size;
}

int sys_usb_bulk_transfer(sys_t* sys, int addr, int ep, usb_device_req_t *req,  int max_packet_size, uint8_t *data, int size) {
	urb_t* urb = urb_create(NULL, NULL);
	urb->submit.devid = addr;
	urb->submit.ep = ep;
	urb->submit.buffer_length = size;
	urb->submit.direction = ((ep & 0x80) == 0) ? 0 : 1;
	urb->max_packet_size = max_packet_size;
	sys_urb_submit(sys, urb);
	urb_wait(urb, 100);
	return urb->status;
}
