#include <stdio.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>

#include <signal.h>

static volatile int keepRunning = 1;

void intHandler(int dummy) {
    keepRunning = 0;
}

void print_endpoint_descriptors ( struct libusb_device_descriptor * dev_dsc, struct libusb_config_descriptor * config ) {
    // Loop through the endpoints
    for (int i = 0; i < dev_dsc->bNumConfigurations; i++) {
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
}

/**
 * @brief Find usb device
 * 
 * @param id_vendor 
 * @param id_product 
 * @param verbose if non-zero print device and their count else be quiet
 * @return found device or NULL if no such device found
 */
libusb_device * find_dev ( uint16_t id_vendor, uint16_t id_product, ssize_t device_count, libusb_device ** device_list, unsigned char verbose ) {
    struct libusb_device_descriptor devdscr;
    libusb_device * found_device = NULL;
    for ( ssize_t i = 0; i < device_count; i++ ) {
        libusb_get_device_descriptor(device_list[i], &devdscr);
        if ( verbose )
            printf("%04X:%04X (%d)\n", devdscr.idVendor, devdscr.idProduct,
                libusb_get_device_address(device_list[i]));
        if ( devdscr.idVendor  == id_vendor
          && devdscr.idProduct == id_product )
          found_device = device_list[i];
    }
    return found_device;
}

// Gets the active endpoint descriptor from config
const struct libusb_endpoint_descriptor * get_active_endpoint_descriptor ( struct libusb_config_descriptor * config ) {
    const struct libusb_interface *interface = &config->interface[0];
    const struct libusb_interface_descriptor *altsetting = &interface->altsetting[0];
    const struct libusb_endpoint_descriptor *ep_desc = &altsetting->endpoint[0];
    return ep_desc;
}

#define MOUSE_VENDOR 0x1A81
#define MOUSE_PRODUCT 0x2205

int main ( void ) {
    /* Initialise interrupt handler*/
    signal(SIGINT, intHandler);
    /* Initialise libusb */
    libusb_context *ctx;
    libusb_init(&ctx);

    struct libusb_device **list;
    ssize_t count = libusb_get_device_list(NULL, &list);
    printf("Number of devices: %ld\n", count);

    libusb_device * found_dev = find_dev ( MOUSE_VENDOR, MOUSE_PRODUCT, count, list, 1 );

    if (! found_dev) {
        printf ( "ERROR: given device not found\n" );
        libusb_free_device_list(list, 1);
        libusb_exit(ctx);
        return 1;
    }
    //-------------------------------------------------------
    // Get device descriptor
    struct libusb_device_descriptor dev_dsc;
    int r = 0;
    r = libusb_get_device_descriptor(found_dev, &dev_dsc);

    if (r < 0) {
        printf("ERROR: %s\n", libusb_strerror(r));
        libusb_free_device_list(list, 1);
        libusb_exit(ctx);
        return 1;
    }

    //-------------------------------------------------------
    // Get endpoint descriptor
    struct libusb_config_descriptor *config;   signal(SIGINT, intHandler);
    r = libusb_get_active_config_descriptor(found_dev, &config);
    if (r < 0) {
        printf("ERROR: %s\n", libusb_strerror(r));
        libusb_free_config_descriptor(config);
        libusb_free_device_list(list, 1);
        libusb_exit(ctx);
        return 1;
    }
    // Prints all endpoints descriptors of the device
    print_endpoint_descriptors ( &dev_dsc, config );

    const struct libusb_endpoint_descriptor *ep_desc = get_active_endpoint_descriptor ( config );
    printf("Endpoint descriptor type: %d\n", ep_desc->bDescriptorType);
    //-------------------------------------------------------
    // Open device
    libusb_device_handle *handle;
    r = libusb_open(found_dev, &handle);
    if (r < 0) {
        printf("ERROR: %s\n", libusb_strerror(r));
        libusb_free_device_list(list, 1);
        libusb_free_config_descriptor(config);
        libusb_exit(ctx);
        return 1;
    }
    // Set auto detach kernel driver
    libusb_set_auto_detach_kernel_driver(handle, 1);
    //-------------------------------------------------------
    // Claim interface
    r = libusb_claim_interface(handle, 0);
    if (r < 0) {
        printf("ERROR: %s\n", libusb_strerror(r));
        libusb_close(handle);
        libusb_free_device_list(list, 1);
        libusb_free_config_descriptor(config);
        libusb_exit(ctx);
        return 1;
    }
    //-------------------------------------------------------
    // Read data
    #define BUFFER_SIZE 8 // wMaxPacketSize
    unsigned char data[BUFFER_SIZE];
    int actual_length;
    while ( keepRunning ) {
        r = libusb_interrupt_transfer(handle, ep_desc->bEndpointAddress, data, BUFFER_SIZE, &actual_length, 0);
        if (r == 0) {
            printf("Data: ");
            for (int i = 0; i < actual_length; i++)
                printf("%02X ", data[i]);
            printf("\n");
        }
        else
            printf("ERROR: %s\n", libusb_strerror(r));
        if ( feof(stdin) )
            break;
    }
    //-------------------------------------------------------
    // Release interface
    r = libusb_release_interface(handle, 0);
    if (r < 0) {
        printf("ERROR: %s\n", libusb_strerror(r));
        libusb_close(handle);
        libusb_free_device_list(list, 1);
        libusb_free_config_descriptor(config);
        libusb_exit(ctx);
        return 1;
    }
    //-------------------------------------------------------
    // Close device
    libusb_close(handle);
    
    libusb_free_config_descriptor(config);
    libusb_free_device_list(list, 1);
    libusb_exit(ctx);
    return 0;
}
