/*************************************************************
 * COURSE WARE ver. 3.0
 * 
 * Permitted to use for educational and research purposes only.
 * NO WARRANTY.
 *
 * Faculty of Information Technology
 * Czech Technical University in Prague
 * Author: Miroslav Skrbek (C)2010,2011,2012
 *         skrbek@fit.cvut.cz
 * 
 **************************************************************
 */

#ifndef __USB_LIB_USBIP_H
#define __USB_LIB_USBIP_H

#include <usb_lib.h>

#define __USB_PACKET_SETUP        0x0D
#define __USB_PACKET_DATA0        0x03
#define __USB_PACKET_DATA1        0x0B
#define __USB_PACKET_SOF          0x05
#define __USB_PACKET_ACK          0x02
#define __USB_PACKET_STALL        0x0E
#define __USB_RESET               0xFF

#define __RET_ACK           0
#define __RET_NAK          -1        
#define __RET_ERR          -2
#define __RET_STALL        -3 

int  __usb_is_dev_attached();
void __usb_reset();
int __usb_transfer(int addr, int ep, int type, byte* data, int size, int *rlen);

#endif

