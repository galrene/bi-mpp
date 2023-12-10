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
#define ENDPOINT_OUT        0x02

void print_endpoint_descriptors ( libusb_device_handle * handle ) {
    libusb_device * device = libusb_get_device(handle);
    struct libusb_device_descriptor dev_dsc; libusb_get_device_descriptor( device, &dev_dsc);
    struct libusb_config_descriptor *config; libusb_get_config_descriptor(device, 0, &config);
    // Loop through the endpoints
    for (int i = 0; i < dev_dsc.bNumConfigurations; i++) {
        const struct libusb_interface *interface = &config->interface[i];
        for (int j = 0; j < interface->num_altsetting; j++) {
            const struct libusb_interface_descriptor *altsetting = &interface->altsetting[j];
            for (int k = 0; k < altsetting->bNumEndpoints; k++) {
                const struct libusb_endpoint_descriptor *ep_desc = &altsetting->endpoint[k];
                printf("Endpoint address: 0x%02X\n", ep_desc->bEndpointAddress);
                // Access the endpoint descriptor fields
                // For example, ep_desc->bEndpointAddress, ep_desc->bmAttributes, etc.
            }
        }
    }
    libusb_free_config_descriptor(config);
}

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
    unsigned char data[64];
    int r;
    int actual_transferred = 0;
    
    while ( 1 ) {
        if ( (r = libusb_bulk_transfer ( device, ENDPOINT_IN, data, sizeof(data),
                                        &actual_transferred, 5000 )) != LIBUSB_SUCCESS )
        {
            fprintf (stderr, "Error: %s\n", libusb_strerror(r));
            break;
        }
        printf("Received %d bytes\n", actual_transferred);
        for ( int i = 0; i < actual_transferred; i++ )
            printf("%02x ", data[i]);
        printf("\n");
    }
}

void send_data ( libusb_device_handle * device, int * value ) {
    int r;
    unsigned char data[32];
    int actual_transferred;
    snprintf ( (char *) data, sizeof(data) - 1, "%d", *value );
    (*value)++;
    if ( (r = libusb_bulk_transfer ( device, ENDPOINT_OUT, data, sizeof(data),
                                &actual_transferred, 3000 )) != LIBUSB_SUCCESS )
    {
        fprintf (stderr, "Error: %s\n", libusb_strerror(r));
    }
}

int main ( void ) {
    libusb_init(NULL);
    libusb_device_handle *device_handle =
        libusb_open_device_with_vid_pid(NULL, 0x1111, 0x2222);
    if (device_handle == NULL) {
        printf("Cannot open device\n");
        return 1;
    }
    if ( ! configure_device(device_handle) ) {
        destroy_device(device_handle);
        return 1;
    }

    print_endpoint_descriptors(device_handle);

    char choice;
    int value_to_send = 0;
    printf("Choose action:\n");
    printf("r - read received data from device\n");
    printf("s - send data to device\n");
    while ( scanf(" %c", &choice) == 1 ) {
        switch (choice) {
            case 'r':
                read_data(device_handle);
                break;
            case 's':
                send_data(device_handle, &value_to_send);
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