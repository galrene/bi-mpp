#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "usb_lib.h"

#define ST_INIT             0
#define ST_DEVDSC_DATA      1
#define ST_DEVDSC_STATUS    2
#define ST_SETADDR_STATUS   3
#define ENDPOINT_IN         0x81

const device_descr_t devdsc __attribute__((space(auto_psv))) = {
    sizeof(device_descr_t),
    1,
    0x0110,
    0,
    0,
    0,
    64,
    0x1111,
    0x2222,
    0x0100,
    0,
    0,
    0,
    1
};

const struct config {
    config_descriptor_t config_descr;
    interf_descriptor_t interf_descr;
    endpoint_descriptor_t endp_descr1;
} CONFIG __attribute__((space(auto_psv))) = {
    {
        sizeof(config_descriptor_t),
        2,               // bDescriptorType
        sizeof(CONFIG),
        1,               // bNumInterfaces
        0x01,            // bConfigurationValue
        0,               // iConfiguration
        0x80,            // bmAttributes
        50               // bMaxPower
    },
    {
        sizeof(interf_descriptor_t),
        4,               // bDescriptorType
        0,               // bInterfaceNumber
        0x0,             // bAlternateSetting
        3,               // bNumEndpoints
        0xFF,            // bInterfaceClass
        0x0,            // bInterfaceSubClass
        0x0,            // bInterfaceProtocol
        0                // iInterface
    },
    {
        sizeof(endpoint_descriptor_t),
        5,               // bDescriptorType
        0x81,             // bEndpointAddress
        3,               // bmAttributes
        10,              // wMaxPacketSize
        1                // bInterval
    }
};

int configure_device ( libusb_device_handle * device ) {
    if ( libusb_set_auto_detach_kernel_driver(device, 1) != LIBUSB_SUCCESS ) {
        fprintf(stderr, "Unable to set auto detach kernel driver\n");
        return 0;
    }
    int r = 0;
    // claim interface
    if ( ( r = libusb_claim_interface(device, 0) ) != 0 ) {
        fprintf(stderr, "Unable to claim interface - error: %s\n", libusb_strerror(r));
        return 0;
    }
    return 1;
}

void destroy_device ( struct libusb_device_handle *device ) {
    // release interface
    libusb_release_interface(device, 0);
    // close device
    libusb_close(device);
    libusb_exit(NULL);
}

void read_data ( libusb_device_handle *device ) {
    pritnf ("Press CTRL-D to stop reading\n");
    // todo    
}

int main ( void ) {
    libusb_init(NULL);
    libusb_device_handle *device_handle = libusb_open_device_with_vid_pid(NULL, 0x1111, 0x2222);
    if (device_handle == NULL) {
        printf("Cannot open device\n");
        return 1;
    }
    if ( ! configure_device(device_handle) ) {
        destroy_device(device_handle);
        return 1;
    }

    print_endpoint_descriptors(device_handle);

    int choice;
    printf("Choose action:\n");
    printf("r - read received data from device\n");
    printf("s - send data to device\n");
    while ( scanf(" %c", &choice) == 1 ) {
        switch (choice) {
            case 'r':
                read_data(device_handle);
                break;
            case 's':
                send_data(device_handle);
                break;
            default:
                printf("Unknown action\n");
                break;
        }
        printf("Choose action:\n");
        printf("r - read received data from device\n");
        printf("s - send data to device\n");
    }

    return 0;
}