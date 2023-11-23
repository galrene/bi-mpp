#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define TIMEOUT 1000

#define USBC_SIGNATURE 0x43425355
#define CSW_SIGNATURE  0x53425355

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
typedef struct __attribute__((packed)) {
uint32_t dCBWSignature;
uint32_t dCBWTag;
uint32_t dCBWDataTransferLength;
unsigned char bmCBWFlags;
unsigned char bCBWLUN;  // pouze bity 0-3
unsigned char bCBWCBLength; // pouze bity 0-5
unsigned char CBWCB[16];
} CBW_t;

// Wrapper structure for USB Mass Storage Bulk-Only command status
typedef struct __attribute__((packed)) {
    uint32_t dCSWSignature;
    uint32_t dCSWTag;
    uint32_t dCSWDataResidue;
    unsigned char bCSWStatus;
} CSW_t;

void fill_inquiry_cbw(CBW_t *cbw) {
    // Refer to the "spc3r23.pdf" document for details on the INQUIRY command
    /**
     * Pro realizaci tohoto příkazu budete volat třikrát funkci libusb_bulk_transfer. 
     * - na endpoint out, buffer: &cbw, size: sizeof(cbw) 
     * - na endpoint in, buffer: pro data, size: 36 
     * - na endpoint in, buffer: &csw, size: sizeof(csw).
     */
    cbw->dCBWSignature = USBC_SIGNATURE;
    cbw->dCBWTag = 0x12345678;
    cbw->dCBWDataTransferLength = 36;
    cbw->bmCBWFlags = DATA_IN;
    cbw->bCBWLUN = 0;
    cbw->bCBWCBLength = 6;
    cbw->CBWCB[0] = SCSI_INQUIRY;
    cbw->CBWCB[1] = 0x00; 
    cbw->CBWCB[2] = 0x00;
    cbw->CBWCB[3] = 0x00; // AH
    cbw->CBWCB[4] = 0x24; // Allocation length
    cbw->CBWCB[5] = 0x00;
    // AH * 256 * AL = 36 Bytes to read
}

void fill_request_sense_cbw(CBW_t *cbw) {
    /**
     * Pro realizaci tohoto příkazu budete volat třikrát funkci libusb_bulk_transfer.
     * - na endpoint out, buffer: &cbw, size: sizeof(cbw) 
     * - na endpoint in, buffer: pro data, size: 18 
     * - na endpoint in, buffer: &csw, size: sizeof(csw).
     */
    cbw->dCBWSignature = USBC_SIGNATURE;
    cbw->dCBWTag = 0x02345678;
    cbw->dCBWDataTransferLength = 18;
    cbw->bmCBWFlags = DATA_IN;
    cbw->bCBWLUN = 0;
    cbw->bCBWCBLength = 6;
    cbw->CBWCB[0] = SCSI_REQUEST_SENSE;
    cbw->CBWCB[1] = 0x00;
    cbw->CBWCB[2] = 0x00;
    cbw->CBWCB[3] = 0x00;
    cbw->CBWCB[4] = 0x18; // Allocation length - how much we want to read
    cbw->CBWCB[5] = 0x00;
}

void fill_read_capacity_cbw(CBW_t *cbw) {
    /**
     * Pro realizaci tohoto příkazu budete volat třikrát funkci libusb_bulk_transfer.
     * - na endpoint out, buffer: &cbw, size: sizeof(cbw)
     * - na endpoint in, buffer: pro data, size: 8
     * - na endpoint in, buffer: &csw, size: sizeof(csw).
     * mlba = (mlba3 « 24) | (mlba2 « 16) | (mlba1 « 8) | mlba0; adresa posledního bloku.
     * blen = (blen3 « 24) | (blen2 « 16) | (blen1 « 8) | blen0; velikost bloku v bytech.
     * Kapacita media: size = (mlba+1)*blen;
     */
    cbw->dCBWSignature = USBC_SIGNATURE;
    cbw->dCBWTag = 0x00345678;
    cbw->dCBWDataTransferLength = 8;
    cbw->bmCBWFlags = DATA_IN;
    cbw->bCBWLUN = 0;
    cbw->bCBWCBLength = 10;
    cbw->CBWCB[0] = SCSI_READ_CAPACITY;
    cbw->CBWCB[1] = 0x00;
    cbw->CBWCB[2] = 0x00;
    cbw->CBWCB[3] = 0x00;
    cbw->CBWCB[4] = 0x00;
    cbw->CBWCB[5] = 0x00;
    cbw->CBWCB[6] = 0x00;
    cbw->CBWCB[7] = 0x00;
    cbw->CBWCB[8] = 0x00;
    cbw->CBWCB[9] = 0x00;
}

/**
 * @brief UNFINISHED, CBWCB not filled
 * 
 * @param cbw 
 * @param blen 
 * @param BC BC = BC1«8+BC0 je délka přenášených dat v blocích.
 */
void fill_read_10_cbw(CBW_t *cbw, int blen, int BC, char * LBA) {
    /**
     * Pro realizaci tohoto příkazu budete volat třikrát funkci libusb_bulk_transfer.
     * - na endpoint out, buffer: &cbw, size: sizeof(cbw)
     * - na endpoint in, buffer: pro data, size: blen * BC
     * - na endpoint in, buffer: &csw, size: sizeof(csw).
     */
    cbw->dCBWSignature = USBC_SIGNATURE;
    cbw->dCBWTag = 0x00045678;
    cbw->dCBWDataTransferLength = blen * BC;
    cbw->bmCBWFlags = DATA_IN;
    cbw->bCBWLUN = 0;
    cbw->bCBWCBLength = 10;
    cbw->CBWCB[0] = SCSI_READ_10;
    cbw->CBWCB[1] = 0x00;
    cbw->CBWCB[2] = 0x00; // LBA3
    cbw->CBWCB[3] = 0x00; // LBA2
    cbw->CBWCB[4] = 0x00; // LBA1
    cbw->CBWCB[5] = 0x00; // LBA0
    cbw->CBWCB[6] = 0x00;
    cbw->CBWCB[7] = 0x00; // BC1 - transfer length in blocks
    cbw->CBWCB[8] = 0x00; // BC0
    cbw->CBWCB[9] = 0x00;
}

void fill_write_10_cbw(CBW_t *cbw, int blen, int BC) {
    cbw->dCBWSignature = USBC_SIGNATURE;
    cbw->dCBWTag = 0x00045678;
    cbw->dCBWDataTransferLength = blen * BC;
    cbw->bmCBWFlags = DATA_IN;
    cbw->bCBWLUN = 0;
    cbw->bCBWCBLength = 10;
    cbw->CBWCB[0] = SCSI_READ_10;
    cbw->CBWCB[1] = 0x00;
    cbw->CBWCB[2] = 0x00; // LogicalBlocakAddress3
    cbw->CBWCB[3] = 0x00; // LBA2
    cbw->CBWCB[4] = 0x00; // LBA1
    cbw->CBWCB[5] = 0x00; // LBA0
    cbw->CBWCB[6] = 0x00;
    cbw->CBWCB[7] = 0x00; // BC1 - transfer length in blocks
    cbw->CBWCB[8] = 0x00; // BC0
    cbw->CBWCB[9] = 0x00;
}

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
}

unsigned char get_max_lun ( struct libusb_device_handle *device ) {
    unsigned char max_lun;
    int r = libusb_control_transfer(device,
                                    LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                    REQUEST_GET_MAX_LUN,
                                    0,
                                    0,
                                    &max_lun,
                                    sizeof(max_lun),
                                    1000);

    if ( r < 1 ) {
        fprintf(stderr, "Unable to get max lun - error: %s\n", libusb_strerror(r));
        return 0;
    }

    return max_lun;
}

#define DEVICEID 0x1666
#define VENDORID 0x0951

void print_hex(unsigned char *data, int length) {
    for (int i = 0; i < length; ++i) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

int configure_device ( libusb_device_handle *device ) {
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

#define INQUIRY_DATA_LEN 36

void decode_inq_data ( unsigned char * data ) {
    printf("Vendor ID: ");
    for ( int i = 8; i < 16; i ++ )
        printf("%c", data[i]);
    printf("\n");
    printf("Product ID: ");
    for ( int i = 16; i < 32; i ++ )
        printf("%c", data[i]);
    printf("\n");
}

int inquiry ( libusb_device_handle * device_handle ) {
    /* Pomocí funkcí Libusb pošlete CBW(INQUIRY) do USB flash
       paměti a přečtěte dodaná data a CSW blok. Zkontrolujte status
       v CSW bloku a dekodujte datovou část. Významné části
       vytiskněte */
    /**
     * Pro realizaci tohoto příkazu budete volat třikrát funkci libusb_bulk_transfer. 
     * - na endpoint out, buffer: &cbw, size: sizeof(cbw) 
     * - na endpoint in, buffer: pro data, size: 36 
     * - na endpoint in, buffer: &csw, size: sizeof(csw).
     */ 
    CBW_t cbw_inquiry;
    fill_inquiry_cbw(&cbw_inquiry);
    int transferred = 0;
    // send an inqury
    int r = libusb_bulk_transfer ( device_handle,                   // dev handle
                                   ENDPOINT_OUT,                    // endpoint
                                   (unsigned char *)&cbw_inquiry,   // data
                                   sizeof(cbw_inquiry),             // length
                                   &transferred,                    // actual transfer length
                                   TIMEOUT );                       // timeout
    if ( r != LIBUSB_SUCCESS ) {
        fprintf(stderr, "Unable to send CBW\n");
        fprintf(stderr, "Transferred bytes: %d\n", transferred );
        fprintf(stderr, "Error: %s\n", libusb_strerror(r));
        return 1;
    }
    unsigned char rec_data[INQUIRY_DATA_LEN];
    // receive data
    r = libusb_bulk_transfer ( device_handle,                   // dev handle
                               ENDPOINT_IN,                     // endpoint
                               rec_data,                        // data
                               sizeof(rec_data),                // length
                               NULL,                            // actual transfer length
                               TIMEOUT );                       // timeout
    if ( r != LIBUSB_SUCCESS ) {
        fprintf(stderr, "Unable to receive data\n");
        fprintf (stderr, "Error: %s\n", libusb_strerror(r));
        return 1;
    }
    CSW_t csw_inquiry;
    // receive status
    r = libusb_bulk_transfer ( device_handle,                   // dev handle
                               ENDPOINT_IN,                     // endpoint
                               (unsigned char *)&csw_inquiry,   // data
                               sizeof(csw_inquiry),             // length
                               NULL,                            // actual length (not sure if null correct)
                               TIMEOUT );                       // timeout
    if ( r != LIBUSB_SUCCESS ) {
        fprintf(stderr, "Unable to receive CSW\n");
        fprintf (stderr, "Error: %s\n", libusb_strerror(r));
        return 1;
    }
    
    if ( csw_inquiry.bCSWStatus != LIBUSB_SUCCESS ) {
        fprintf(stderr, "CSW status error\n");
        fprintf(stderr, "CSW status: %d\n", csw_inquiry.bCSWStatus);
        return 1;
    }
    if ( csw_inquiry.dCSWSignature != CSW_SIGNATURE ) {
        fprintf(stderr, "CSW signature mismatch\n");
        fprintf(stderr, "CSW signature: 0x%X\nExpected: 0x%X\n", csw_inquiry.dCSWSignature, CSW_SIGNATURE);
        return 1;
    }
    decode_inq_data(rec_data);
    return 0;
}

int main ( void ) {
    libusb_init(NULL);
    libusb_device_handle *device_handle =
            libusb_open_device_with_vid_pid(NULL,VENDORID, DEVICEID);
    if ( ! device_handle ) {
        fprintf(stderr, "Unable to open device\n");
        return 1;
    }
    if ( ! configure_device(device_handle) ) {
        destroy_device(device_handle);
        return 1;
    }

    print_endpoint_descriptors(device_handle);

    printf ("Max LUN: %d\n", get_max_lun(device_handle));

    if ( ! inquiry(device_handle) ) {
        destroy_device(device_handle);
        return 1;
    }


    destroy_device(device_handle);
    return 0;
}
