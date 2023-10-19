#ifndef __HCD_H
#define __HCD_H

#include <stdint.h>

struct HCD;
typedef struct HCD hcd_t;

struct HCD_GTD;
typedef struct HCD_GTD hcd_gtd_t;

struct HCD_EPD;
typedef struct HCD_EPD hcd_epd_t;

#include "urb.h"

#define HCD_NOTIFY_RESET      1
#define HCD_NOTIFY_CONTROL    2
#define HCD_NOTIFY_BULK       4
#define HCD_NOTIFY_INTERRUPT  8
#define HCD_NOTIFY_DONE      16

#define __GTD_PID_SETUP 	0b00
#define __GTD_PID_OUT   	0b01
#define __GTD_PID_IN    	0b10

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

// no-thread-save
void __hcd_notify(hcd_t* hcd, int notification);
void __hcd_clear_notification(hcd_t* hcd, int notification);
void __hcd_lock(hcd_t* hcd);
void __hcd_unlock(hcd_t* hcd);

// thread-safe
void hcd_init(hcd_t* hcd);
void hcd_bus_reset(hcd_t* hcd);
void hcd_process_done_list(hcd_t* hcd);
void hcd_wait_for_event(hcd_t* hcd);
void hcd_event_ack(hcd_t* hcd);
void hcd_gtd_link(hcd_t* hcd, int addr, int ep, int dir, int setup, int toggle, int mps, uint8_t* data, int size, urb_t* urb);
void hcd_gtd_unlink(hcd_t* hcd, hcd_gtd_t* gtd);

#endif