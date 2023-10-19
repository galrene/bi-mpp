#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

#define MIN(a,b) ((a < b) ? (a) : (b))

#include "usb_lib_usbip.h"

#define USBIP_VERSION 0x0111

#define URB_SHORT_NOT_OK        0x00000001
#define URB_ISO_ASAP            0x00000002 
#define URB_NO_TRANSFER_DMA_MAP 0x00000004 
#define URB_NO_FSBR             0x00000020 
#define URB_ZERO_PACKET         0x00000040 
#define URB_NO_INTERRUPT        0x00000080 
#define URB_FREE_BUFFER         0x00000100 
#define URB_DIR_MASK            0x00000200 

#define HCD_NOTIFY_RESET      1
#define HCD_NOTIFY_CONTROL    2
#define HCD_NOTIFY_BULK       4
#define HCD_NOTIFY_INTERRUPT  8
#define HCD_NOTIFY_DONE      16

#define __GTD_PID_SETUP 	0b00
#define __GTD_PID_OUT   	0b01
#define __GTD_PID_IN    	0b10

struct URB;

typedef struct USBIP_OP_HEADER {
	struct {
		uint16_t version;
		uint16_t command;
	};
	union {
		uint32_t status;	
		uint32_t seqnum;	
	};
} usbip_op_header_t;

typedef struct  __attribute__((packed)) OP_REQ_DEVLIST {
	usbip_op_header_t header;
} usbip_op_req_devlist_t;

typedef struct __attribute__((packed)) USBIP_DEVLIST_INTF {
	uint8_t   bInterfaceClass;
	uint8_t   bInterfaceSubClass;
	uint8_t   bInterfaceProtocol;
	uint8_t   reserved;
} usbip_devlist_intf_t; 

typedef struct __attribute__((packed)) USBIP_DEVLIST_DEV {
	uint8_t   path[256];
	uint8_t   busid[32];
    uint32_t  busnum;
	uint32_t  devnum;
	uint32_t  speed;
	uint16_t  idVendor;
	uint16_t  idProduct;
	uint16_t  bcdDevice;
	uint8_t   bDeviceClass;
	uint8_t   bDeviceSubClass;
	uint8_t   bDeviceProtocol;
	uint8_t   bConfigurationValue;
    uint8_t   bNumConfigurations;
	uint8_t   bNumInterfaces;
	usbip_devlist_intf_t interfaces[];
} usbip_devlist_dev_t;

typedef struct __attribute__((packed)) USBIP_OP_REP_DEVLIST {
	usbip_op_header_t header;
    uint32_t  num_of_exp_devs;
	usbip_devlist_dev_t devices [];
} usbip_op_rep_devlist_t;

typedef struct  __attribute__((packed)) OP_REQ_IMPORT {
	usbip_op_header_t header;
	uint8_t  busid[32];
} usbip_op_req_import_t;

typedef struct  __attribute__((packed)) OP_REP_IMPORT {
	usbip_op_header_t header;
	usbip_devlist_dev_t dev;
} usbip_op_rep_import_t;

typedef struct OP_CMD_SUBMIT {
	usbip_op_header_t header;
	uint32_t devid;
	uint32_t direction;
	uint32_t ep;
	uint32_t flags;
	uint32_t buffer_length;
	uint32_t start_frame;
	uint32_t number_of_packets;
	uint32_t interval;
	union {
		uint64_t as_int;
		usb_device_req_t as_req;
		byte as_barr[sizeof(usb_device_req_t)];
	} setup;
} usbip_op_cmd_submit_t;

typedef struct OP_RET_SUBMIT {
	usbip_op_header_t header;
	uint32_t devid;
	uint32_t direction;
	uint32_t ep;
	uint32_t status;
	uint32_t actual_length;
	uint32_t start_frame;
	uint32_t number_of_packets;
	uint32_t error_count;
	union {
		uint64_t as_int;
		usb_device_req_t as_req;
		byte as_barr[sizeof(usb_device_req_t)];
	} setup;
} usbip_op_ret_submit_t;

typedef void (*urb_handler_t) (struct URB *urb, void* args);

typedef struct USB_PACKET {
	struct USB_PACKET *next;
	int addr;
	int ep;
	int pid;
	int size;
	uint8_t data[8192];	
} usb_packet_t;	

typedef struct HCD_GTD {
	int bufferRounding;
	int pid;
	int interruptDelay;
	int dataToggle;
	int errorCount;
	int conditionCode;
	uint8_t* CurrentBufferPointer;
	struct HCD_GTD *nextTD;
	uint8_t* bufferEnd;
	struct URB *urb;
} hcd_gtd_t;

typedef struct HCD_EPD {
	int FunctionAddress;
	int EndpointNumber;
	int Direction;
//	int Speed;
	int Skip;
//	int Format;
	int MaximumPacketSize;
	hcd_gtd_t* TDQueueTailPointer;
	hcd_gtd_t* TDQueueHeadPointer;
	int toggleCarry;
	int Halted;
	struct HCD_EPD *nextEPD; 
} hcd_epd_t;

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
	hcd_t* hcd;
    struct URB *next;
	struct URB *prev;
} urb_t;

typedef struct HCD {
	int attached;
	pthread_mutex_t lock;
	pthread_t hcd_thread;
	int notify;
	pthread_cond_t notify_cond;
	hcd_epd_t* control_list;
	hcd_epd_t* bulk_list;
	hcd_gtd_t* done_list;
	hcd_epd_t* current_control_epd;
	hcd_epd_t* current_bulk_epd;
} hcd_t;

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

typedef struct USBIP {
	sys_t* sys;
	int fd;
} usbip_t;

static hcd_t usbhcd;
static sys_t usbsys;
static usbip_t usbip;

static pthread_t accept_thread;

double get_time() {
	double t = (1.0 * clock()) / CLOCKS_PER_SEC;
	return t;
}

void wait_for(double start, double delay) {
	while (get_time() - start < delay) {
		usleep(5);
	}
}	

void urb_wait(urb_t* urb, int timeout);

void hcd_append_gtd(hcd_t* hcd, int addr, int ep, int dir, int setup, int toggle, int mps, uint8_t* data, int size, urb_t* urb);
void hcd_send_gtd(hcd_t* hcd, hcd_epd_t* list, int notification);
void hcd_send_bus_reset(hcd_t* hcd);
void hcd_bus_reset(hcd_t* hcd);
void hcd_process_done_list(hcd_t* hcd);
void hcd_wait_for_event(hcd_t* hcd);
void hcd_event_ack(hcd_t* hcd);
void __hcd_notify(hcd_t* hcd, int notification);
void __hcd_clear_notification(hcd_t* hcd, int notification);
void __hcd_lock(hcd_t* hcd);
void __hcd_unlock(hcd_t* hcd);
int sys_usb_control_transfer(sys_t* sys, int addr, int ep, usb_device_req_t *req, int max_packet_size, uint8_t *data, int size);
int sys_usb_bulk_transfer(sys_t* sys, int addr, int ep, usb_device_req_t *req,  int max_packet_size, uint8_t *data, int size);

usbip_op_rep_devlist_t* create_op_rep_list(device_descr_t* devdsc, config_descriptor_t* cfgdsc, size_t* devlist_size) {
	
	int i;
	byte* cfg;

	size_t size = sizeof(usbip_op_rep_devlist_t) +
		sizeof(usbip_devlist_dev_t) +
		sizeof(usbip_devlist_intf_t) * cfgdsc->bNumInterfaces;

	usbip_op_rep_devlist_t* devlist = calloc(1, size);

	assert(devlist_size != NULL);
	*devlist_size = size;

	devlist->header.version  = htons(USBIP_VERSION);
    devlist->header.command  = htons(0x0005);
	devlist->header.status   = htonl(0x0000);
    devlist->num_of_exp_devs = htonl(1);

    memset(&devlist->devices[0], 0, 256);
    strcpy(devlist->devices[0].path, "/sys/devices/pic24/usb1/1-1");
    memset(devlist->devices[0].busid,0, sizeof(devlist->devices[0].busid));
    strcpy(devlist->devices[0].busid,"1-1");

    devlist->devices[0].busnum=htonl(1);
    devlist->devices[0].devnum=htonl(1);
    devlist->devices[0].speed=htonl(2);
    devlist->devices[0].idVendor=htons(devdsc->idVendor);
    devlist->devices[0].idProduct=htons(devdsc->idProduct);
    devlist->devices[0].bcdDevice=htons(devdsc->bcdDevice);
    devlist->devices[0].bDeviceClass=devdsc->bDeviceClass;
    devlist->devices[0].bDeviceSubClass=devdsc->bDeviceSubClass;
    devlist->devices[0].bDeviceProtocol=devdsc->bDeviceProtocol;
    devlist->devices[0].bConfigurationValue=cfgdsc->bConfigurationValue;
    devlist->devices[0].bNumConfigurations=devdsc->bNumConfigurations; 
    devlist->devices[0].bNumInterfaces=cfgdsc->bNumInterfaces;
	
    cfg = (byte*)cfgdsc + ((byte*)cfgdsc)[0];

	for(i = 0; i < devlist->devices[0].bNumInterfaces; i++) {

		while (cfg < ((byte*)cfgdsc + cfgdsc->wTotalLength)) {
			if (cfg[1] == 4)
				break;
			cfg += cfg[0];
		}

		devlist->devices[0].interfaces[i].bInterfaceClass = ((interf_descriptor_t*)cfg)->bInterfaceClass;
		devlist->devices[0].interfaces[i].bInterfaceSubClass = ((interf_descriptor_t*)cfg)->bInterfaceSubClass;
		devlist->devices[0].interfaces[i].bInterfaceProtocol = ((interf_descriptor_t*)cfg)->bInterfaceProtocol;
		devlist->devices[0].interfaces[i].reserved = 0; 
		cfg += cfg[0];

	}

	return devlist;
}

void urb_init(urb_t* urb) {
	memset(urb, 0, sizeof(urb_t));
	assert(pthread_mutex_init(&urb->lock, NULL) == 0);
	assert(pthread_cond_init(&urb->done_cond, NULL) == 0);
}

void urb_destroy(urb_t* urb) {
	assert(pthread_mutex_destroy(&urb->lock) == 0);
	assert(pthread_cond_destroy(&urb->done_cond) == 0);
}

void urb_free(urb_t* urb) {
	urb_destroy(urb);
	free(urb);
}	

void urb_wait(urb_t* urb, int timeout_ms) {
	pthread_mutex_lock(&urb->lock);
	while (!urb->done) {
		int r;
		struct timespec end_time = {0, 0};
		clock_gettime(CLOCK_REALTIME, &end_time);
		end_time.tv_nsec += timeout_ms * 1000;
		if (end_time.tv_nsec >= 1000000000L) {
			end_time.tv_nsec -= 1000000000L;
			end_time.tv_sec  += 1;
		}
		r = pthread_cond_timedwait(&urb->done_cond, &urb->lock, &end_time);
		if (r < 0) {
			break;
		}
	}
	pthread_mutex_unlock(&urb->lock);
}

urb_t* urb_create(usbip_op_cmd_submit_t* submit, uint8_t* data) {
	urb_t* urb = (urb_t*)malloc(sizeof(urb_t));
	urb_init(urb);
	if (submit != NULL) {
		memcpy(&urb->submit, submit, sizeof(submit));
	}
	if (data != NULL) {	
		urb->data = data;
	}	
	return urb;
}

void urb_set_done_handler(urb_t* urb, urb_handler_t done_handler, void* done_args) {
	__urb_lock(urb);
	urb->done_handler = done_handler;
	urb->done_arg = done_args;
	__urb_unlock(urb);
}

void urb_clear(urb_t* urb) {
	pthread_mutex_lock(&urb->lock);
	urb->done = 0;	
	pthread_mutex_unlock(&urb->lock);
}

void urb_done(urb_t* urb) {
	if (urb == NULL) {
		return;
	}
	pthread_mutex_lock(&urb->lock);
	urb->done = 1;
	if (urb->done_handler != NULL) {
		urb_handler_t handler = urb->done_handler;
		void *arg = urb->done_arg;
		pthread_mutex_unlock(&urb->lock);
		handler(urb, arg);
		pthread_mutex_lock(&urb->lock);
		pthread_cond_signal(&urb->done_cond);		
	}
	pthread_mutex_unlock(&urb->lock);
}

//////////////////////////////////////////////

void* hcd_loop(void* t) {
	hcd_t* hcd = (hcd_t*)t;
	hcd_gtd_t* td;
	int state = 0;
	while (1) {

		hcd_wait_for_event(hcd);

		switch (state) {
			case 0: 
				hcd_send_gtd(hcd, hcd->control_list, HCD_NOTIFY_CONTROL);
				state++;
				break;
			case 1: 
				hcd_send_gtd(hcd, hcd->control_list, HCD_NOTIFY_CONTROL);
				state++;
				break;
			case 2: 
				hcd_send_gtd(hcd, hcd->control_list, HCD_NOTIFY_CONTROL);
				state++;
				break;
			case 3: 
				hcd_send_gtd(hcd, hcd->control_list, HCD_NOTIFY_CONTROL);
				state = -1;
				break;
			case -1:	
				hcd_send_gtd(hcd, hcd->bulk_list, HCD_NOTIFY_BULK);
				state = -2;
				break;
			case -2:	
//				printf("Processing done list ...\n");
				hcd_process_done_list(hcd);
				state = 0;
				break;
		}	
	}
	return t;
}

void hcd_init(hcd_t* hcd) {
  	int r;
	pthread_mutexattr_t attr;

//	printf("HCD init ...\n");  
	memset(hcd, 0, sizeof(hcd_t));
	r = pthread_mutex_init(&hcd->lock, NULL);
//	printf("hcd lock init: %d\n", r);
	r = pthread_cond_init(&hcd->notify_cond, NULL);
//	printf("hcd cond init: %d\n", r);
    r = pthread_create(&hcd->hcd_thread, NULL, hcd_loop, hcd);
	if (r < 0) {
//		printf("Error: cannot create HCD thread (%d)\n", r);
		exit(1);
	}
}   

hcd_epd_t* hcd_epd_create(int addr, int ep, int mps) {
	hcd_epd_t* p = (hcd_epd_t*)calloc(sizeof(hcd_epd_t), 1);
	assert(0 <= ep && ep <= 15);
	assert(0 <= mps && mps <= 1024);
	assert(0 <= addr && addr <= 127);
	p->FunctionAddress = addr;
	p->MaximumPacketSize = mps;
	p->Direction = 0;
	return p;
}

hcd_gtd_t* hcd_gtd_create(int pid, int toggle, uint8_t* data, int size, urb_t* urb) {
	hcd_gtd_t* p = (hcd_gtd_t*)calloc(sizeof(hcd_gtd_t), 1);
	p->CurrentBufferPointer = data;
	p->bufferEnd = data + size - 1;
	p->pid = pid;
	p->urb = urb;
	return p;
}

void hcd_append_gtd(hcd_t* hcd, int addr, int ep, int dir, int setup, int toggle, int mps, uint8_t* data, int size, urb_t* urb) {
	int pid;
	hcd_epd_t* p;
	hcd_gtd_t* t;
	int notification = 0;
	__hcd_lock(hcd);
	ep &= 0xF; 
	if (setup || ep == 0) {
		notification = HCD_NOTIFY_CONTROL;
		p = hcd->control_list;
		if (p == NULL) {
			p = hcd_epd_create(addr, ep, mps);
			hcd->control_list = p;
		}
	} else {
		notification = HCD_NOTIFY_BULK;
		p = hcd->bulk_list;
		if (p == NULL) {
			p = hcd_epd_create(addr, ep, mps);
			hcd->bulk_list = p;
		}
	}
	// find EP
	while (p) { 
		if (p->FunctionAddress == addr && p->EndpointNumber == ep) {
			break;
		}
		if (p->nextEPD == NULL) {
			p->nextEPD = hcd_epd_create(addr, ep, mps);
			p = p->nextEPD;
			break;
		}
		p = p->nextEPD; 
	}

	if (setup) {
		assert(dir == 0);
		pid = __GTD_PID_SETUP;
	} else 
	if (dir) {
		pid = __GTD_PID_IN;
	} else {
		pid = __GTD_PID_OUT;
	}

	t = hcd_gtd_create(pid, toggle, data, size, urb);
	if (p->TDQueueTailPointer != NULL) {
		p->TDQueueTailPointer->nextTD = t;
	}
	if (p->TDQueueHeadPointer == NULL) {
		p->TDQueueHeadPointer = t;
	}
	p->TDQueueTailPointer = t;
	p->Skip = 0;
	p->Halted = 0;
	__hcd_notify(hcd, notification);
	__hcd_unlock(hcd);
	return;
}

void __append_to_done_list(hcd_t* hcd, hcd_gtd_t* gtd) {
	if (hcd->done_list == NULL) {
		hcd->done_list = gtd;
		gtd->nextTD = NULL;
	} else {
		gtd->nextTD = hcd->done_list;
		hcd->done_list = gtd;
	}
	__hcd_notify(hcd, HCD_NOTIFY_DONE);
}

void __hcd_unlink_gtd(hcd_t* hcd, hcd_epd_t* epd, hcd_gtd_t* gtd) {
	hcd_gtd_t* list;
    if (epd->TDQueueHeadPointer == gtd) {
		epd->TDQueueHeadPointer = gtd->nextTD;
		if (epd->TDQueueTailPointer == gtd) {
			epd->TDQueueTailPointer = NULL;
		}
		__append_to_done_list(hcd, gtd);
		return;
	}
	list = epd->TDQueueHeadPointer;
	while (list != NULL) {
		if (list->nextTD == gtd) {
			list->nextTD = gtd->nextTD;
			__append_to_done_list(hcd, gtd);
			return;
		}
		list = list->nextTD;
	}
}

void __hcd_lock(hcd_t* hcd) {
	int r = pthread_mutex_lock(&hcd->lock);
//	printf("hcd_lock %d\n", r);
	assert(r == 0);
}

void __hcd_unlock(hcd_t* hcd) {
	int r = pthread_mutex_unlock(&hcd->lock);
//	printf("hcd_unlock %d\n", r);
	assert(r == 0);
}

void hcd_send_gtd(hcd_t* hcd, hcd_epd_t* epd, int notification) {
	double start;
	int sz, r, mpsz, buflen, rlen, ep, pid;
	hcd_gtd_t* p;
	__hcd_lock(hcd);
	while (epd != NULL && (epd->Skip || epd->Halted || epd->TDQueueHeadPointer == NULL)) {
		epd = epd->nextEPD;
	}
	if (epd == NULL) {
		__hcd_clear_notification(hcd, notification);
		__hcd_unlock(hcd);
		return;
	}
	p = epd->TDQueueHeadPointer;
	mpsz = epd->MaximumPacketSize;
	buflen = p->bufferEnd - p->CurrentBufferPointer + 1;
	if ((p->dataToggle & 2) != 0) {
		p->dataToggle = epd->toggleCarry;
	}
    switch (p->pid) {
		case __GTD_PID_SETUP:
		    ep = epd->EndpointNumber;
			pid = __USB_PACKET_SETUP;
			assert(buflen == 8);
			sz = buflen;
			break;
		case __GTD_PID_OUT:
			ep = epd->EndpointNumber;
			pid =  ((p->dataToggle & 1)	== 0) ? __USB_PACKET_DATA0 : __USB_PACKET_DATA1;
			sz = MIN(mpsz, buflen);
			break;
		case __GTD_PID_IN:
			ep = epd->EndpointNumber | 0x80;
			pid =  ((p->dataToggle & 1)	== 0) ? __USB_PACKET_DATA0 : __USB_PACKET_DATA1;
			sz = buflen;
			break;
		default:
			assert(0);
			break;	
	}

	start = get_time();
	//printf("Sending usb packet ... [%d, 0x%X, 0x%X]\n", epd->FunctionAddress, ep,  pid);
	r = __usb_transfer(epd->FunctionAddress, ep, pid, p->CurrentBufferPointer, sz, &rlen);
	wait_for(start, 8*(sz+4)/12000000.0);

	switch (r) {
		case __RET_ACK:
//			printf("USB ep: %d, ACK\n", epd->EndpointNumber);
			p->CurrentBufferPointer += rlen;
			p->dataToggle ^= 1;
			if ((ep & 0x80) == 0) {
				if (p->CurrentBufferPointer >= p->bufferEnd + 1) {
					__hcd_unlink_gtd(hcd, epd, p);
					epd->toggleCarry = p->dataToggle & 1;
				}
			} else {
				if (p->CurrentBufferPointer >= p->bufferEnd  + 1 || rlen < epd->MaximumPacketSize) {
					__hcd_unlink_gtd(hcd, epd, p);
				}
			}	
			break;
		case __RET_NAK:
//			printf("USB ep: %d, NAK\n", epd->EndpointNumber);
			usleep(1000);
			break;
		case __RET_ERR:
//			printf("USB ep: %d, ERR\n", epd->EndpointNumber);
			if (p->errorCount == 2) {
				__hcd_unlink_gtd(hcd, epd, p);
			} else {
				p->errorCount += 1;
			}
			break;
		case __RET_STALL:
//			printf("USB ep: %d, STALL\n", epd->EndpointNumber);
			epd->Halted = 1;
			__hcd_unlink_gtd(hcd, epd, p);
			break;
		default:
			printf("Error: invalid device response %d\n", r);	
	}
	__hcd_unlock(hcd);
}

void hcd_bus_reset(hcd_t* hcd) {
	__hcd_lock(hcd);
	__usb_transfer(0,0,__USB_RESET, NULL, 0, NULL);
	__hcd_unlock(hcd);
	usleep(15000);
}

void hcd_process_done_list(hcd_t* hcd) {
	hcd_gtd_t *td;
	__hcd_lock(hcd);
	if (hcd->done_list == NULL) {
		__hcd_clear_notification(hcd, HCD_NOTIFY_DONE);
		__hcd_unlock(hcd);
		return;
	}
	urb_t* urb = hcd->done_list->urb;
	td = hcd->done_list;
	hcd->done_list = hcd->done_list->nextTD;
	free(td);
	urb_done(urb);
	__hcd_unlock(hcd);
}

void hcd_wait_for_event(hcd_t* hcd) {
	pthread_mutex_lock(&hcd->lock);		
	while (!hcd->notify) {
		pthread_cond_wait(&hcd->notify_cond, &hcd->lock);
	}	
	pthread_mutex_unlock(&hcd->lock);
}

void hcd_wait_for_td(hcd_t* hcd, hcd_gtd_t* gtd) {
	pthread_mutex_lock(&hcd->lock);		
	hcd_gtd_t* list = hcd->done_list;
	while (list) {
		if (list == gtd) {

		}
	}

	while (!hcd->notify) {
		pthread_cond_wait(&hcd->notify_cond, &hcd->lock);
	}	
	pthread_mutex_unlock(&hcd->lock);
}


void __hcd_notify(hcd_t* hcd, int notification) {
	hcd->notify |= notification;
	pthread_cond_signal(&hcd->notify_cond);
}

void __hcd_clear_notification(hcd_t* hcd, int notification) {
	hcd->notify &= ~notification;
}


void* system_loop(void* t);

void sys_init(sys_t* sys, hcd_t* hcd) {
	memset(sys, 0, sizeof(sys_t));	
	sys->hcd = hcd;
    if (pthread_create(&sys->system_thread, NULL, system_loop, sys)  < 0) {
		printf("Error: cannot create SYSTEM thread\n");
		exit(1);
	}
}

void __sys_lock(sys_t* sys) {
	pthread_mutex_lock(&sys->lock);
}

void __sys_unlock(sys_t* sys) {
	pthread_mutex_unlock(&sys->lock);
}

void sys_destroy(sys_t* sys) {
	
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
	  	  .wValue = 0x0005,
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
					if (sys_usb_control_transfer(sys, 5, 0, &req_get_device_dsc, sys->dev_dsc.bMaxPacketSize, 
						(byte*)&sys->dev_dsc, sizeof(device_descr_t)) != sizeof(device_descr_t))
						continue;
				    if (sys_usb_control_transfer(sys, 5, 0, &req_get_config_dsc, sys->dev_dsc.bMaxPacketSize, 
							(byte*)&cfg_dsc, sizeof(cfg_dsc)) != sizeof(cfg_dsc))
						continue;
					sys->cfg_dsc = (config_descriptor_t*)(calloc(cfg_dsc.wTotalLength, 1));
					req_get_config_dsc.wLength = cfg_dsc.wTotalLength;
				    if (sys_usb_control_transfer(sys, 5, 0, &req_get_config_dsc, sys->dev_dsc.bMaxPacketSize, 
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
				    if (sys_usb_control_transfer(sys, 5, 0, &req_set_configuration, sys->dev_dsc.bMaxPacketSize, 
							NULL, 0) != 0)
						continue;
				    if (sys_usb_control_transfer(sys, 5, 0, &req_set_interface, sys->dev_dsc.bMaxPacketSize, 
							NULL, 0) != 0)
						continue;
					break;		
				}	
			}	
		}

	}
	return t;
}

void sys_urb_kill(sys_t* sys, urb_t* urb) {
	hcd_t* hcd = sys->hcd;
	__hcd_lock(hcd);

	__hcd_unlock(hcd);
}

void sys_urb_unlink(sys_t* sys, urb_t* urb) {
	urb_t* item = sys->urb_list_head;
	if (item == NULL) {
		return;
	}
	while (item) {
		if (item == urb) {
			// unlink
			pthread_mutex_destroy(&urb->lock);
			pthread_cond_destroy(&urb->done_cond);
			free(urb);
			return;
		} 
	}
	return;
}

void sys_urb_submit(sys_t* sys, urb_t* urb) {
	int pid = 0;
	if (sys->urb_list_tail == NULL) {
		sys->urb_list_head = urb;
		sys->urb_list_tail = urb;
	} else {
		sys->urb_list_tail->next = urb;
		urb->prev = sys->urb_list_tail;
		sys->urb_list_tail = urb;
	}
	if (urb->submit.setup.as_int != 0) {
		urb_t __urb;
		urb_init(&__urb);		
    	hcd_append_gtd(sys->hcd, urb->submit.devid, urb->submit.ep, 
		    0, // direction OUT
			1, // setup
			0, // toggle 
			urb->max_packet_size, urb->submit.setup.as_barr, 
			sizeof(urb->submit.setup), &__urb);
		urb_wait(&__urb, 100);
		urb_destroy(&__urb);
		urb->length = 0;
		if (urb->submit.buffer_length > 0) {
			urb_init(&__urb);
			__urb.data = urb->data;
			__urb.submit.buffer_length = urb->submit.buffer_length;
			hcd_append_gtd(sys->hcd, urb->submit.devid, urb->submit.ep, 
		    	1, // direction IN
				0, // setup
				1, // toggle 
				urb->max_packet_size, 
				urb->data, 
				urb->submit.buffer_length, urb);
			urb_wait(urb, 100);
			urb->length = __urb.length;
			urb_destroy(&__urb);
		}	
		urb_init(&__urb);
		hcd_append_gtd(sys->hcd, 
			urb->submit.devid, 
			urb->submit.ep, 
		  	0, // direction
			0, // setup
			1, // toggle 
			urb->max_packet_size, 
			NULL, 0, &__urb);	  
		urb_wait(&__urb, 100);
		urb_destroy(&__urb);
		urb_done(urb);		
	} else {
    	hcd_append_gtd(sys->hcd, urb->submit.devid, urb->submit.ep, 
			((urb->submit.ep & 0x80) != 0), // direction
			0, // setup
			2, // toggle from EPD
			urb->max_packet_size, urb->data, 
			sizeof(urb->submit.buffer_length), urb);        		
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

////////////////////////////////////////////////////////////////////////////

void on_urb_complete(urb_t* urb, void* args) {
	int r;
	usbip_t* usbip = (usbip_t*)args;
	sys_t* sys = usbip->sys;
    usbip_op_ret_submit_t op_ret_submit;
	op_ret_submit.header.version = htons(0x0000);
	op_ret_submit.header.command = htons(0x0003);
	op_ret_submit.header.seqnum = urb->submit.header.seqnum;
	op_ret_submit.devid = urb->submit.devid;
	op_ret_submit.direction = urb->submit.direction;
	op_ret_submit.ep = urb->submit.ep;
	//TODO: ISO PACKETS
	op_ret_submit.number_of_packets = 0;
	op_ret_submit.start_frame = 0;
	// status OK
	op_ret_submit.status = 0;
	// TODO: kontrola, ze td je dealokovano po volani handleru
	op_ret_submit.error_count = urb->td->errorCount;
    op_ret_submit.actual_length = urb->length;
    r = send(usbip->fd, &op_ret_submit, sizeof(op_ret_submit), 0);
	if (r != sizeof(op_ret_submit)) {
		printf("Error: cannot wrtie to socket");
		exit(1);
	}
	sys_urb_unlink(urb);
}


void* accept_loop(void* t) {
	usbip_t* usbip = (usbip_t*)t;
	sys_t* sys = usbip->sys;
	hcd_t* hcd = sys->hcd;
	int sfd;
	struct sockaddr_in cli_addr;
	socklen_t cli_len = sizeof(cli_addr);
	printf("listen fd: %d\n", usbip->fd);
	for(;;) {
		if ((sfd = accept(usbip->fd, (struct sockaddr*) &cli_addr, &cli_len)) < 0) {
			printf("Error: accept %d\n", sfd);
			exit(1);
		}
		printf("Connected from: %s\n", inet_ntoa(cli_addr.sin_addr));
		while (1) {
			int r, cmd;
			size_t size;
			usbip_op_header_t header;
			usbip_op_req_devlist_t op_req_devlist;
			usbip_op_rep_devlist_t op_rep_devlist;
			usbip_op_req_import_t  op_req_import;
			usbip_op_rep_import_t  op_rep_import;
			usbip_op_cmd_submit_t  op_cmd_submit;
			usbip_op_ret_submit_t  op_ret_submit;

			urb_t* urb;

			r = recv(sfd, &header, sizeof(header), MSG_PEEK); 
			if (r < 0) {
				printf("Error: recv [%d/%ld]\n", r, sizeof(header));
				exit(1);
			}	
			if (r == 0) {
				close(sfd);
				break;
			}
			cmd = ntohs(header.command);
			printf("Command: %04X\n", cmd);
			switch (cmd) {
				case 0x0001:
					r = recv(sfd, &op_cmd_submit, sizeof(op_cmd_submit), MSG_WAITALL); 
					if (r < 0 || r != sizeof(op_cmd_submit)) {
						printf("Error: recv [%d/%ld]\n", r, sizeof(op_cmd_submit));
						exit(1);
					}	
					printf("seqnum: 0x%04X\n", ntohl(op_cmd_submit.header.seqnum));
					if (op_cmd_submit.direction != 0) {
						urb = urb_create(&op_cmd_submit, malloc(op_cmd_submit.buffer_length));
						urb_set_done_handler(urb, on_urb_complete, (void*)usbip);
						sys_urb_submit(sys, urb);
					} else {
						
					}	
					break;
				case 0x0002:
					r = recv(sfd, &op_ret_submit, sizeof(op_ret_submit), MSG_WAITALL); 
					if (r < 0 || r != sizeof(op_ret_submit)) {
						printf("Error: recv [%d/%ld]\n", r, sizeof(op_ret_submit));
						exit(1);
					}	
					printf("devid: %08X\n", ntohl(op_ret_submit.devid));
					printf("direction: %d\n", ntohl(op_ret_submit.direction));
					printf("ep: %d\n", ntohl(op_ret_submit.ep));
					printf("status: %X\n", ntohl(op_ret_submit.status));
					printf("setup 0x%lX\n", op_ret_submit.setup.as_int);
                    break;
				case 0x8003:
					r = recv(sfd, &op_req_import, sizeof(op_req_import), MSG_WAITALL); 
					if (r < 0 || r != sizeof(op_req_import)) {
						printf("Error: recv [%d/%ld]\n", r, sizeof(op_req_import));
						exit(1);
					}	
					printf("P: %s\n", op_req_import.busid);
					op_rep_import.header.version = htons(USBIP_VERSION);
					op_rep_import.header.command = htons(0x0003);
					if (strcmp(op_req_import.busid, sys->devlist->devices[0].busid) != 0) {
						op_rep_import.header.status = htonl(0x0000001);
						size = sizeof(usbip_op_header_t);
					} else {
						size = sizeof(op_rep_import);
						op_rep_import.header.status = htonl(0x0000000);
						op_rep_import.header.version = htons(USBIP_VERSION);
						op_rep_import.header.command = htons(0x0003);
						memcpy(&op_rep_import.dev, &sys->devlist->devices[0], sizeof(usbip_devlist_dev_t));
					}	
						
					r = send(sfd, &op_rep_import, size, 0);
		    		if (r < 0 || r != size) {
						printf("Error: send [%d/%ld]", r, size);
						exit(1);
					}
					break;

				case 0x8005:
					r = recv(sfd, &op_req_devlist, sizeof(op_req_devlist), MSG_WAITALL); 
					if (r < 0 || r != sizeof(op_req_devlist)) {
						printf("Error: recv [%d/%ld]\n", r, sizeof(op_req_devlist));
						exit(1);
					}	

					printf("Version: 0x%04X\n", op_req_devlist.header.version);
					__sys_lock(sys);
					r = send(sfd, sys->devlist, sys->devlist_size, 0);
		    		if (r < 0 || r != sys->devlist_size) {
						printf("Error: send [%d/%ld]", r, sys->devlist_size);
						exit(1);
					}
					__sys_unlock(sys);
					break;
				default:
					printf("Error: unknown command %04X\n", cmd);	
			}	
		}
	}
	pthread_exit(t);
}

void usbip_init(usbip_t* usbip, sys_t* sys, const char* ip_addr, int port) {
	struct sockaddr_in listen_address;
	memset(usbip, 0, sizeof(usbip_t));
	usbip->sys = sys;

	if ((usbip->fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Error: cannot create socket\n");
		exit(1);
	}

        memset(&listen_address, 0, sizeof(listen_address));
	listen_address.sin_family = AF_INET;
	listen_address.sin_addr.s_addr = inet_addr(ip_addr);
	listen_address.sin_port = htons(3240);

    if (bind(usbip->fd, (const struct sockaddr*) &listen_address, sizeof(listen_address)) < 0) {
		printf("Error: bind error\n");
		exit(1);
	}

	if (listen(usbip->fd, SOMAXCONN) < 0) {
		printf("Error: resource busy\n");
		exit(1);
	}

    pthread_create(&accept_thread, NULL, accept_loop, (void*)usbip);

}

void usbip_destroy(usbip_t* usbip) {
}

void __usbip_server_init() {

    // Init HCD
	hcd_init(&usbhcd);

    sys_init(&usbsys, &usbhcd);

    usbip_init(&usbip, &usbsys, "127.0.0.1", 3240);
	
}
