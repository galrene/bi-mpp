#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "utils.h"
#include "urb.h"
#include "usbip.h"

static pthread_t accept_thread;

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

typedef struct {
	sys_t* sys;
	int fd;
} on_urb_complete_args_t;

void on_urb_complete(urb_t* urb, void* args) {
	int r;
	sys_t* sys = ((on_urb_complete_args_t*)args)->sys;
	int fd = ((on_urb_complete_args_t*)args)->fd;
    usbip_op_ret_submit_t op_ret_submit;
	op_ret_submit.header.submit_cmd = htonl(0x00000003);
	op_ret_submit.header.seqnum = htonl(urb->submit.header.seqnum);
	DBG_MSG("RET seqnum: 0x%04X\n", urb->submit.header.seqnum);
	op_ret_submit.devid = htonl(urb->submit.devid);
	op_ret_submit.direction = htonl(urb->submit.direction);
	op_ret_submit.ep = htonl(urb->submit.ep);
	//TODO: ISO PACKETS
	op_ret_submit.number_of_packets = 0;
	op_ret_submit.start_frame = 0;
	// status OK
	op_ret_submit.status = 0;
	// TODO: kontrola, ze td je dealokovano po volani handleru
	//op_ret_submit.error_count = urb->td->errorCount;
	op_ret_submit.error_count = 0;
    op_ret_submit.actual_length = htonl(urb->length);
    r = send(fd, &op_ret_submit, sizeof(op_ret_submit), 0);
	if (r != sizeof(op_ret_submit)) {
		printf("Error: cannot write to the socket");
	}
	if (urb->submit.direction) {
    	r = send(fd, urb->data, urb->length, 0);
		if (r != urb->length) {
			printf("Error: cannot write to the socket");
		}
	}	
	urb_free(urb);
}

void* accept_loop(void* t) {
	usbip_t* usbip = (usbip_t*)t;
	sys_t* sys = usbip->sys;
	hcd_t* hcd = sys->hcd;
	int sfd;
	struct sockaddr_in cli_addr;
	socklen_t cli_len = sizeof(cli_addr);
	DBG_MSG("listen fd: %d\n", usbip->fd);
	for(;;) {
		if ((sfd = accept(usbip->fd, (struct sockaddr*) &cli_addr, &cli_len)) < 0) {
			printf("Error: accept %d\n", sfd);
			exit(1);
		}
		DBG_MSG("Connected from: %s\n", inet_ntoa(cli_addr.sin_addr));
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
			DBG_MSG("Command: %04X\n", cmd);
			switch (cmd) {
				case 0x0001:
					r = recv(sfd, &op_cmd_submit, sizeof(op_cmd_submit), MSG_WAITALL); 
					if (r < 0 || r != sizeof(op_cmd_submit)) {
						printf("Error: recv [%d/%ld]\n", r, sizeof(op_cmd_submit));
						exit(1);
					}	
					op_cmd_submit.header.submit_cmd = ntohl(op_cmd_submit.header.submit_cmd);
					op_cmd_submit.header.seqnum = ntohl(op_cmd_submit.header.seqnum);
					op_cmd_submit.buffer_length = ntohl(op_cmd_submit.buffer_length);
					op_cmd_submit.devid = ntohl(op_cmd_submit.devid);
					op_cmd_submit.direction = ntohl(op_cmd_submit.direction);
					op_cmd_submit.ep = ntohl(op_cmd_submit.ep);
					op_cmd_submit.flags = ntohl(op_cmd_submit.flags);
					op_cmd_submit.interval = ntohl(op_cmd_submit.interval);
					op_cmd_submit.number_of_packets = ntohl(op_cmd_submit.number_of_packets);
					op_cmd_submit.start_frame = ntohl(op_cmd_submit.start_frame);

					DBG_MSG("submit_cmd: 0x%08X\n", op_cmd_submit.header.submit_cmd);
					DBG_MSG("seqnum: 0x%04X\n", op_cmd_submit.header.seqnum);
					DBG_MSG("buffer length: 0x%08X\n", op_cmd_submit.buffer_length);
					DBG_MSG("devid: 0x%08X\n", op_cmd_submit.devid);
					DBG_MSG("direction: 0x%08X\n", op_cmd_submit.direction);
					DBG_MSG("ep: 0x%08X\n", op_cmd_submit.ep);
					DBG_MSG("flags: 0x%08X\n", op_cmd_submit.flags);
					DBG_MSG("interval: 0x%08X\n", op_cmd_submit.interval);
					DBG_MSG("number of packets: 0x%08X\n", op_cmd_submit.number_of_packets);
					DBG_MSG("start frame: 0x%08X\n", op_cmd_submit.start_frame);
					DBG_MSG("setup: 0x%016lX\n", op_cmd_submit.setup.as_int);

					if (op_cmd_submit.direction != 0) {
						urb = urb_create(&op_cmd_submit, malloc(op_cmd_submit.buffer_length));
						on_urb_complete_args_t* args = calloc(sizeof(on_urb_complete_args_t), 1);
						args->fd = sfd;
						args->sys = sys;
						if (op_cmd_submit.ep == 0) {
							urb->max_packet_size = sys->dev_dsc.bMaxPacketSize;
						} else {
							urb->max_packet_size = sys->cfg_dsc->interfaces[0].endpoints[op_cmd_submit.ep - 1].wMaxPacketSize;
						}	
						urb_set_done_handler(urb, on_urb_complete, (void*)args);
						sys_urb_submit(sys, urb);
					} else {
						size_t size = op_cmd_submit.buffer_length;
						uint8_t* buffer = NULL;
						if (size != 0) {
							buffer = malloc(size);
							r = recv(sfd, buffer, size, MSG_WAITALL); 
							if (r < 0 || r != size) {
								printf("Error: recv [%d/%ld]\n", r, size);
								exit(1);
							}															
						}
						urb = urb_create(&op_cmd_submit, buffer);
						on_urb_complete_args_t* args = calloc(sizeof(on_urb_complete_args_t), 1);
						args->fd = sfd;
						args->sys = sys;
						if (op_cmd_submit.ep == 0) {
							urb->max_packet_size = sys->dev_dsc.bMaxPacketSize;
						} else {
							urb->max_packet_size = sys->cfg_dsc->interfaces[0].endpoints[op_cmd_submit.ep - 1].wMaxPacketSize;
						}	
						urb_set_done_handler(urb, on_urb_complete, (void*)args);
						sys_urb_submit(sys, urb);						
					}	
					break;
				case 0x0002:
					r = recv(sfd, &op_ret_submit, sizeof(op_ret_submit), MSG_WAITALL); 
					if (r < 0 || r != sizeof(op_ret_submit)) {
						printf("Error: recv [%d/%ld]\n", r, sizeof(op_ret_submit));
						exit(1);
					}	
					DBG_MSG("devid: %08X\n", ntohl(op_ret_submit.devid));
					DBG_MSG("direction: %d\n", ntohl(op_ret_submit.direction));
					DBG_MSG("ep: %d\n", ntohl(op_ret_submit.ep));
					DBG_MSG("status: %X\n", ntohl(op_ret_submit.status));
					DBG_MSG("setup 0x%lX\n", op_ret_submit.setup.as_int);
                    break;
				case 0x8003:
					r = recv(sfd, &op_req_import, sizeof(op_req_import), MSG_WAITALL); 
					if (r < 0 || r != sizeof(op_req_import)) {
						printf("Error: recv [%d/%ld]\n", r, sizeof(op_req_import));
						exit(1);
					}	
					DBG_MSG("P: %s\n", op_req_import.busid);
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

					DBG_MSG("Version: 0x%04X\n", op_req_devlist.header.version);
					__sys_lock(sys);
					r = send(sfd, sys->devlist, sys->devlist_size, 0);
		    		if (r < 0 || r != sys->devlist_size) {
						printf("Error: send [%d/%ld]", r, sys->devlist_size);
						exit(1);
					}
					__sys_unlock(sys);
					break;
				default:
					DBG_MSG("Error: unknown command %04X\n", cmd);	
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
