#include <stdio.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>


/**
 * @brief Find usb device
 * 
 * @param id_vendor 
 * @param id_product 
 * @param verbose if non-zero print device and their count else be quiet
 * @return libusb_device found device or NULL if no such device found
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

int main ( void ) {
    /* Initialise libusb */
    libusb_context *ctx;
    libusb_init(&ctx);

    struct libusb_device **list;
    ssize_t count = libusb_get_device_list(NULL, &list);
    printf("Number of devices: %ld\n", count);

    libusb_device * searched_dev = find_dev ( 0x1af3, 0001, count, list, 1 );

    if ( ! searched_dev ) {
        printf ( "ERROR: given device not found\n" );
        libusb_free_device_list(list, 1);
        libusb_exit(ctx);
        return 1;
    }

    struct libusb_device_descriptor dev_dsc;
    int r = 0;
    r = libusb_get_device_descriptor(searched_dev, &dev_dsc);

    if (r < 0) {
        printf("ERROR: %s\n", libusb_strerror(r));
        libusb_free_device_list(list, 1);
        libusb_exit(ctx);
        return 1;
    }

    /* Vyčtěte deskriptor a vytiskněte jeho obsah */


    libusb_free_device_list(list, 1);
    libusb_exit(ctx);
    return 0;
}
