#ifndef __URB_H
#define __URB_H

struct URB;
typedef struct URB urb_t;
typedef void (*urb_handler_t) (struct URB *urb, void* args);

#include "hcd.h"
#include "usbip.h"

typedef struct URB {
	usbip_op_cmd_submit_t submit;
	pthread_mutex_t lock;
	pthread_cond_t done_cond;
	void* data;
	int length;
    urb_handler_t done_handler;
	int max_packet_size;
	void* done_arg;
	int done;
	int status;
	hcd_gtd_t* td;
	sys_t* sys;
    struct URB *next;
	struct URB *prev;
} urb_t;

void urb_init(urb_t* urb);
urb_t* urb_create(usbip_op_cmd_submit_t* submit, uint8_t* data);
void urb_destroy(urb_t* urb);
void urb_free(urb_t* urb);
void urb_wait(urb_t* urb, int timeout_ms);
void urb_set_done_handler(urb_t* urb, urb_handler_t done_handler, void* done_args);
void urb_clear(urb_t* urb);
void urb_done(urb_t* urb);
void __urb_lock(urb_t* urb);
void __urb_unlock(urb_t* urb);

#endif
