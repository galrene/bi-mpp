#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

#include "utils.h"
#include "hcd.h"

#include "usb_lib_usbip.h"

void* hcd_loop(void* t);

void hcd_init(hcd_t* hcd) {
  	int r;
	pthread_mutexattr_t attr;
	DBG_MSG("HCD init ...\n");  
	memset(hcd, 0, sizeof(hcd_t));
	r = pthread_mutex_init(&hcd->lock, NULL);
    DBG_MSG("hcd lock init: %d\n", r);
	r = pthread_cond_init(&hcd->notify_cond, NULL);
	DBG_MSG("hcd cond init: %d\n", r);
    r = pthread_create(&hcd->hcd_thread, NULL, hcd_loop, hcd);
	if (r < 0) {
    	DBG_MSG("Error: cannot create HCD thread (%d)\n", r);
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
	p->EndpointNumber = ep;
	return p;
}

hcd_gtd_t* hcd_gtd_create(int pid, int toggle, uint8_t* data, int size, urb_t* urb) {
	hcd_gtd_t* p = (hcd_gtd_t*)calloc(sizeof(hcd_gtd_t), 1);
	p->CurrentBufferPointer = data;
	p->bufferEnd = data + size - 1;
	p->pid = pid;
	p->urb = urb;
	p->dataToggle = toggle;
	return p;
}

void hcd_gtd_link(hcd_t* hcd, int addr, int ep, int dir, int setup, int toggle, int mps, uint8_t* data, int size, urb_t* urb) {
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
	DBG_MSG("hcd_lock %d\n", r);
	assert(r == 0);
}

void __hcd_unlock(hcd_t* hcd) {
	int r = pthread_mutex_unlock(&hcd->lock);
	DBG_MSG("hcd_unlock %d\n", r);
	assert(r == 0);
}

void hcd_gtd_send(hcd_t* hcd, hcd_epd_t* epd, int notification) {
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
	DBG_MSG("Sending usb packet ... [%d, 0x%X, 0x%X]\n", epd->FunctionAddress, ep,  pid);
	r = __usb_transfer(epd->FunctionAddress, ep, pid, p->CurrentBufferPointer, sz, &rlen);
	wait_for(start, 8*(sz+4)/12000000.0);

	switch (r) {
		case __RET_ACK:
			DBG_MSG("USB ep: %d, ACK\n", epd->EndpointNumber);
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
					epd->toggleCarry = p->dataToggle & 1;
				}
			}	
			break;
		case __RET_NAK:
			DBG_MSG("USB ep: %d, NAK\n", epd->EndpointNumber);
			usleep(1000);
			break;
		case __RET_ERR:
			DBG_MSG("USB ep: %d, ERR\n", epd->EndpointNumber);
			if (p->errorCount == 2) {
				__hcd_unlink_gtd(hcd, epd, p);
			} else {
				p->errorCount += 1;
			}
			break;
		case __RET_STALL:
			DBG_MSG("USB ep: %d, STALL\n", epd->EndpointNumber);
			epd->Halted = 1;
			__hcd_unlink_gtd(hcd, epd, p);
			break;
		default:
			DBG_MSG("Error: invalid device response %d\n", r);	
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
	urb->status = td->errorCount != 0;
	urb->length = urb->submit.buffer_length - ((td->bufferEnd + 1) - td->CurrentBufferPointer); 
	free(td);
	urb->td = NULL;
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

void hcd_gtd_unlink(hcd_t* hcd, hcd_gtd_t* gtd) {
	assert(hcd != NULL);
	if (gtd == NULL) {
		return;
	}
	__hcd_lock(hcd);
	hcd_epd_t* e = hcd->control_list;
	while (e != NULL) {
		hcd_gtd_t* t = e->TDQueueHeadPointer;
		if (t == gtd) {
			e->TDQueueHeadPointer = t->nextTD;
			return;
		}
		while (t != NULL) {
			if (t->nextTD == gtd) {
				t->nextTD = gtd->nextTD;
				return;
			}
			t = t->nextTD;
		}
	}
	__hcd_unlock(hcd);
}

void* hcd_loop(void* t) {
	hcd_t* hcd = (hcd_t*)t;
	hcd_gtd_t* td;
	int state = 0;
	while (1) {

		hcd_wait_for_event(hcd);

		switch (state) {
			case 0: 
				hcd_gtd_send(hcd, hcd->control_list, HCD_NOTIFY_CONTROL);
				state++;
				break;
			case 1: 
				hcd_gtd_send(hcd, hcd->control_list, HCD_NOTIFY_CONTROL);
				state++;
				break;
			case 2: 
				hcd_gtd_send(hcd, hcd->control_list, HCD_NOTIFY_CONTROL);
				state++;
				break;
			case 3: 
				hcd_gtd_send(hcd, hcd->control_list, HCD_NOTIFY_CONTROL);
				state = -1;
				break;
			case -1:	
				hcd_gtd_send(hcd, hcd->bulk_list, HCD_NOTIFY_BULK);
				state = -2;
				break;
			case -2:	
				DBG_MSG("Processing done list ...\n");
				hcd_process_done_list(hcd);
				state = 0;
				break;
		}	
	}
	return t;
}

