#ifndef __USBIP_H
#define __USBIP_H

struct USBIP_OP_REP_DEVLIST;
typedef struct USBIP_OP_REP_DEVLIST usbip_op_rep_devlist_t;

#include "sys.h"
#include "usb_lib_usbip.h"

#define USBIP_VERSION 0x0111

typedef struct USBIP_OP_HEADER {
	union {
		struct {
			uint16_t version;
			uint16_t command;
		};
		uint32_t submit_cmd;
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

#include "sys.h"

typedef struct USBIP {
	sys_t* sys;
	int fd;
} usbip_t;

void usbip_init(usbip_t* usbip, sys_t* sys, const char* ip_addr, int port);
usbip_op_rep_devlist_t* create_op_rep_list(device_descr_t* devdsc, config_descriptor_t* cfgdsc, size_t* devlist_size);

#endif