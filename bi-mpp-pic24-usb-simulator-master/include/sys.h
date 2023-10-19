#ifndef __SYS_H
#define __SYS_H

#include <pthread.h>

struct SYSTEM;
typedef struct SYSTEM sys_t;

#include "hcd.h"
#include "usbip.h"
#include "usb_lib_usbip.h"

typedef struct SYSTEM {
	pthread_mutex_t lock;
	hcd_t* hcd;
	device_descr_t dev_dsc;
	config_descriptor_t* cfg_dsc;
	usbip_op_rep_devlist_t* devlist;
	size_t devlist_size;
	int dev_ready;
	urb_t* urb_list_head;
	urb_t* urb_list_tail;
	pthread_t system_thread;
} sys_t;

void sys_init(sys_t* sys, hcd_t* hcd);
void sys_destroy(sys_t* sys);
void __sys_lock(sys_t* sys);
void __sys_unlock(sys_t* sys);
void sys_urb_kill(urb_t* urb);
void sys_urb_unlink(urb_t* urb);
void sys_urb_submit(sys_t* sys, urb_t* urb);
int sys_usb_control_transfer(sys_t* sys, int addr, int ep, usb_device_req_t *req, int max_packet_size, uint8_t *data, int size);
int sys_usb_bulk_transfer(sys_t* sys, int addr, int ep, usb_device_req_t *req,  int max_packet_size, uint8_t *data, int size);

#endif