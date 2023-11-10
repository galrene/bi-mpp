#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define TIMEOUT 1000

// USB endpoint directions
#define ENDPOINT_IN 0x81
#define ENDPOINT_OUT 0x02

// USB requests
#define REQUEST_GET_MAX_LUN 0xFE
#define REQUEST_BULK_RESET 0xFF

// SCSI commands
#define SCSI_INQUIRY 0x12
#define SCSI_REQUEST_SENSE 0x03
#define SCSI_READ_CAPACITY 0x25
#define SCSI_READ_10 0x28
#define SCSI_WRITE_10 0x2A

#define DATA_IN 0x80
#define DATA_OUT 0x00

// Wrapper structure for USB Mass Storage Bulk-Only commands
typedef struct {
    unsigned char bmCBWSignature[4];
    unsigned int dwCBWTag;
    unsigned int dwCBWDataTransferLength;
    unsigned char bCBWFlags;
    unsigned char bCBWLUN;
    unsigned char bCBWCBLength;
    unsigned char CBWCB[16];
} usb_mass_bulk_cbw;

// Wrapper structure for USB Mass Storage Bulk-Only command status
typedef struct {
    unsigned char dCSWSignature[4];
    unsigned int dwCSWTag;
    unsigned int dwCSWDataResidue;
    unsigned char bCSWStatus;
} usb_mass_bulk_csw;

void fill_inquiry_cbw(usb_mass_bulk_cbw *cbw) {
    // Fill the CBW structure with data for the INQUIRY command
    // Modify this function based on the SCSI command specification
    // Refer to the "spc3r23.pdf" document for details on the INQUIRY command
    // ...

    // Example: SCSI INQUIRY command
    cbw->bmCBWSignature[0] = 'U';
    cbw->bmCBWSignature[1] = 'S';
    cbw->bmCBWSignature[2] = 'B';
    cbw->bmCBWSignature[3] = 'C';
    cbw->dwCBWTag = 0x12345678;
    cbw->dwCBWDataTransferLength = 0;
    cbw->bCBWFlags = DATA_IN; // Data transfer direction (bit 7 = 1 for IN)
    cbw->bCBWLUN = 0;
    cbw->bCBWCBLength = 6;
    cbw->CBWCB[0] = SCSI_INQUIRY;
    cbw->CBWCB[1] = 0x00; // Additional parameters
    cbw->CBWCB[2] = 0x00;
    cbw->CBWCB[3] = 0x00;
    cbw->CBWCB[4] = 0xFF; // Allocation length
    cbw->CBWCB[5] = 0x00;
}

void fill_request_sense_cbw(usb_mass_bulk_cbw *cbw) {
    // Fill the CBW structure with data for the REQUEST SENSE command
    // Modify this function based on the SCSI command specification
    // Refer to the appropriate documentation for details on the command
    // ...
}

void fill_read_capacity_cbw(usb_mass_bulk_cbw *cbw) {
    // Fill the CBW structure with data for the READ CAPACITY command
    // Modify this function based on the SCSI command specification
    // Refer to the appropriate documentation for details on the command
    // ...
}

void fill_read_10_cbw(usb_mass_bulk_cbw *cbw) {
    // Fill the CBW structure with data for the READ (10) command
    // Modify this function based on the SCSI command specification
    // Refer to the appropriate documentation for details on the command
    // ...
}

void fill_write_10_cbw(usb_mass_bulk_cbw *cbw) {
    // Fill the CBW structure with data for the WRITE (10) command
    // Modify this function based on the SCSI command specification
    // Refer to the appropriate documentation for details on the command
    // ...
}

void print_endpoint_descriptors ( libusb_device * device ) {
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
}

unsigned char get_max_lun ( struct libusb_device_handle *device ) {
    unsigned char max_lun;
    libusb_control_transfer(device,
                            LIBUSB_ENDPOINT_IN
                            | LIBUSB_REQUEST_TYPE_CLASS
                            | LIBUSB_RECIPIENT_INTERFACE,
                            0xFE,
                            0,
                            0,
                            &max_lun,
                            1,
                            1000);
    return max_lun;
}
//
//void send_inquiry ( struct libusb_device_handle *device,
//                    unsigned char *inquiry, unsigned int size,
//                    unsigned char * inquiry_received, int size_received ) {
//    libusb_control_transfer(device,
//                            LIBUSB_ENDPOINT_IN
//                            | LIBUSB_REQUEST_TYPE_CLASS
//                            | LIBUSB_RECIPIENT_INTERFACE,
//                            0x12,
//                            0,
//                            0,
//                            inquiry,
//                            size,
//                            1000);
//    // receive data
//    libusb_bulk_transfer(device,
//                         0x81,
//                         inquiry_received,
//                         size_received,
//                         NULL,
//                         1000);
//}

#define DEVICEID 0x1666
#define VENDORID 0x0951

void print_hex(unsigned char *data, int length) {
    for (int i = 0; i < length; ++i) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

int configure_device ( struct libusb_device_handle *device ) {
    if ( libusb_set_auto_detach_kernel_driver(device, 1) != LIBUSB_SUCCESS ) {
        fprintf(stderr, "Unable to set auto detach kernel driver\n");
        return 0;
    }

    // set configuration
    int r = 0;
    if ( ( r = libusb_set_configuration(device, 1) ) != 0 ) {
        fprintf (stderr, "Unable to set configuration - error: %s\n", libusb_strerror(r));
        return 0;
    }
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

int main ( void ) {
    libusb_init(NULL);
    libusb_device_handle *device_handle =
            libusb_open_device_with_vid_pid(NULL,VENDORID, DEVICEID);
//    libusb_device * device =  libusb_get_device(device_handle);
    if ( ! device_handle ) {
        fprintf(stderr, "Unable to open device\n");
        return 1;
    }
    if ( ! configure_device(device_handle) ) {
        destroy_device(device_handle);
        return 1;
    }

    printf ("Max LUN: %d\n", get_max_lun(device_handle));

    usb_mass_bulk_cbw cbw_inquiry;
    fill_inquiry_cbw(&cbw_inquiry);

    // send cbw_inqury
    int r = libusb_bulk_transfer(device_handle,
                                 ENDPOINT_OUT,
                                 (unsigned char *)&cbw_inquiry,
                                 sizeof(cbw_inquiry),
                                 NULL,
                                 TIMEOUT);
    if ( r < 1 ) {
        fprintf(stderr, "Unable to send CBW\n");
        fprintf (stderr, "Error: %s\n", libusb_strerror(r));
        destroy_device(device_handle);
        return 1;
    }
    usb_mass_bulk_csw csw_inquiry;
    // receive data
    r = libusb_bulk_transfer(device_handle,
                             ENDPOINT_IN,
                             (unsigned char *) &csw_inquiry,
                             sizeof(csw_inquiry),
                             NULL,
                             TIMEOUT);
    if ( r < 1 ) {
        fprintf(stderr, "Unable to receive data\n");
        fprintf (stderr, "Error: %s\n", libusb_strerror(r));
        destroy_device(device_handle);
        return 1;
    }
    fprintf(stderr, "CSW status: %d\n", csw_inquiry.bCSWStatus);
    fprintf(stderr, "CSW data signature: %s\n", csw_inquiry.dCSWSignature);


    destroy_device(device_handle);
    return 0;
}
