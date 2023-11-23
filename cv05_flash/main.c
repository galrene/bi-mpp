#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BULK_TRANSFER_TIMEOUT 1000

#define USBC_SIGNATURE 0x43425355
#define CSW_SIGNATURE  0x53425355

// USB endpoint directions
#define ENDPOINT_IN 0x81
#define ENDPOINT_OUT 0x02

// USB requests
#define REQUEST_GET_MAX_LUN 0xFE

// SCSI commands
#define SCSI_INQUIRY 0x12
#define SCSI_REQUEST_SENSE 0x03
#define SCSI_READ_CAPACITY 0x25
#define SCSI_READ_10 0x28
#define SCSI_WRITE_10 0x2A

#define DATA_IN 0x80
#define DATA_OUT 0x00

#define INQUIRY_DATA_LEN 36
#define REQ_SENSE_DATA_LEN 18
#define READ_CAPACITY_DATA_LEN 8

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
 * @param LBA last block address
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
    cbw->CBWCB[0] = SCSI_WRITE_10;
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
    libusb_free_config_descriptor(config);
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

void decode_inq_data ( unsigned char * data ) {
    printf("======INQUIRY DATA======\n");
    printf("Peripheral Qualifier: 0x%02X\n", data[0]);
    printf("Peripheral Device Type: 0x%02X\n", data[0]);
    printf("RMB: 0x%02X\n", data[1]);
    printf("ISO Version: 0x%02X\n", data[2]);
    printf("ECMA Version: 0x%02X\n", data[3]);
    printf("ANSI Version: 0x%02X\n", data[4]);
    printf("Additional Length: 0x%02X\n", data[5]);
    printf("SCCS: 0x%02X\n", data[6]);
    printf("BQue: 0x%02X\n", data[7]);
    printf("CmdQue: 0x%02X\n", data[8]);
    printf("Vendor ID: ");
    for ( int i = 8; i < 16; i ++ )
        printf("%c", data[i]);
    printf("\n");
    printf("Product ID: ");
    for ( int i = 16; i < 32; i ++ )
        printf("%c", data[i]);
    printf("\n========================\n");
}

int check_csw ( libusb_device_handle * handle ) {
    CSW_t csw_inquiry;
    // receive status
    int r = libusb_bulk_transfer ( handle,                          // dev handle
                               ENDPOINT_IN,                     // endpoint
                               (unsigned char *)&csw_inquiry,   // data
                               sizeof(csw_inquiry),             // length
                               NULL,                            // actual length (not sure if null correct)
                               BULK_TRANSFER_TIMEOUT );         // BULK_TRANSFER_TIMEOUT
    if ( r != LIBUSB_SUCCESS ) {
        fprintf(stderr, "Unable to receive CSW\n");
        fprintf (stderr, "Error: %s\n", libusb_strerror(r));
        return 0;
    }
    
    if ( csw_inquiry.bCSWStatus != LIBUSB_SUCCESS ) {
        fprintf(stderr, "CSW status error\n");
        fprintf(stderr, "CSW status: %d\n", csw_inquiry.bCSWStatus);
        return 0;
    }
    if ( csw_inquiry.dCSWSignature != CSW_SIGNATURE ) {
        fprintf(stderr, "CSW signature mismatch\n");
        fprintf(stderr, "CSW signature: 0x%X\nExpected: 0x%X\n", csw_inquiry.dCSWSignature, CSW_SIGNATURE);
        return 0;
    }
    return 1;
}

int send_cbw ( libusb_device_handle * handle, CBW_t * cbw ) {
    int transferred = 0;
    // send CBW
    int r = libusb_bulk_transfer ( handle,                      // dev handle
                               ENDPOINT_OUT,                    // endpoint
                               (unsigned char *)cbw,            // data
                               sizeof(*cbw),                    // length
                               &transferred,                    // actual length (not sure if null correct)
                               BULK_TRANSFER_TIMEOUT );         // BULK_TRANSFER_TIMEOUT
    if ( r != LIBUSB_SUCCESS ) {
        fprintf(stderr, "Unable to send CBW\n");
        fprintf(stderr, "Transferred bytes: %d\n", transferred);
        fprintf (stderr, "Error: %s\n", libusb_strerror(r));
        return 0;
    }
    return 1;
}

int receive_data ( libusb_device_handle * device_handle, unsigned char * rec_data, int length ) {
    // receive data
    int r = libusb_bulk_transfer (  device_handle,                   // dev handle
                                    ENDPOINT_IN,                     // endpoint
                                    rec_data,                        // data
                                    length,                          // length
                                    NULL,                            // actual transfer length
                                    BULK_TRANSFER_TIMEOUT );         // BULK_TRANSFER_TIMEOUT
    if ( r != LIBUSB_SUCCESS ) {
        fprintf(stderr, "Unable to receive data\n");
        fprintf (stderr, "Error: %s\n", libusb_strerror(r));
        return 0;
    }
    return 1;
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
    if ( ! send_cbw ( device_handle, &cbw_inquiry ) )
        return 0;
    unsigned char rec_data[INQUIRY_DATA_LEN];
    if ( ! receive_data ( device_handle, rec_data, INQUIRY_DATA_LEN ) )
        return 0;
    if ( ! check_csw ( device_handle ) )
        return 0;
    decode_inq_data(rec_data);
    return 1;
}

void decode_req_sense_data ( unsigned char * data ) {
    printf("====REQUEST SENSE DATA====\n");
    printf("Response code: 0x%02X\n", data[0] & 0x7F);
    printf("Filemark: %s\n", (data[2] & 0x80) ? "yes" : "no");
    printf("EOM: %s\n", (data[2] & 0x40) ? "yes" : "no");
    printf("ILI: %s\n", (data[2] & 0x20) ? "yes" : "no");
    printf("Sense key: 0x%02X\n", data[2] & 0x0F);
    printf("Information: ");
    for ( int i = 3 ; i < 7; i ++ )
        printf("0x%02X ", data[i]);
    printf("\n");
    printf("Additional sense length: 0x%02X\n", data[7]);
    printf("Command specific information: ");
    for ( int i = 8 ; i < 12; i ++ )
        printf("0x%02X ", data[i]);
    printf("\n");
    printf("Additional sense code: 0x%02X\n", data[12]);
    printf("Additional sense code qualifier: 0x%02X\n", data[13]);
    printf("Field replaceable unit code: 0x%02X\n", data[14]);
    printf("Sense key specific: ");
    for ( int i = 15 ; i < 18; i ++ )
        printf("0x%02X ", data[i]);
    printf("\n==========================\n");
}

int req_sense ( libusb_device_handle * device_handle ) {
    /**
     * Pro realizaci tohoto příkazu budete volat třikrát funkci libusb_bulk_transfer.
     * - na endpoint out, buffer: &cbw, size: sizeof(cbw) 
     * - na endpoint in, buffer: pro data, size: 18 
     * - na endpoint in, buffer: &csw, size: sizeof(csw).
     */
    /**
     * Pomocí funkcí Libusb pošlete CBW(REQUEST SENSE) do USB flash paměti a přečtěte dodaná data a CSW blok.
     * Zkontrolujte status v CSW bloku a dekodujte datovou část.    
     *
     */
    CBW_t cbw_req_sense;
    fill_request_sense_cbw(&cbw_req_sense);
    if ( ! send_cbw ( device_handle, &cbw_req_sense ) )
        return 0;
    unsigned char rec_data[REQ_SENSE_DATA_LEN];
    if ( ! receive_data ( device_handle, rec_data, REQ_SENSE_DATA_LEN ) )
        return 0;
    if ( ! check_csw ( device_handle ) )
        return 0;
    decode_req_sense_data(rec_data);
    return 1;
}

typedef struct read_capacity_data {
    uint32_t m_Mlba;     // address of the last logical block
    uint32_t m_Blength;  // block length in bytes
    uint64_t m_Capacity; // disk capacity in bytes
} TReadCapacityData;

TReadCapacityData decode_read_capacity_data ( unsigned char * data ) {
    printf("====READ CAPACITY DATA====\n");
    uint32_t mlba = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    uint32_t blength = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
    uint64_t capacity = (uint64_t)(mlba+1) * blength;
    printf("Last logical block address: 0x%X\n", mlba);
    printf("Block length: %d\n", blength);
    printf("Disk capacity:\t%ldB\n\t\t%ldGiB\n", capacity, capacity/1024/1024/1024);
    printf("==========================\n");
    return {mlba, blength, capacity};
}

int read_capacity ( libusb_device_handle * device_handle, TReadCapacityData * read_capacity_data ) {
    /* Pomocí funkcí Libusb pošlete CBW(READ CAPACITY) do USB flash
       paměti a přečtěte dodaná data a CSW blok. Zkontrolujte status
        v CSW bloku, zjistěte maximalní hodnotu LBA a velikost bloku. Spočtěte kapacitu disku.
    */
    /**
     * Pro realizaci tohoto příkazu budete volat třikrát funkci libusb_bulk_transfer.
     * - na endpoint out, buffer: &cbw, size: sizeof(cbw)
     * - na endpoint in, buffer: pro data, size: 8
     * - na endpoint in, buffer: &csw, size: sizeof(csw).
     * mlba = (mlba3 « 24) | (mlba2 « 16) | (mlba1 « 8) | mlba0; adresa posledního bloku.
     * blen = (blen3 « 24) | (blen2 « 16) | (blen1 « 8) | blen0; velikost bloku v bytech.
     * Kapacita media: size = (mlba+1)*blen;
     */ 
    CBW_t cbw_read_capacity;
    fill_read_capacity_cbw(&cbw_read_capacity);
    if ( ! send_cbw ( device_handle, &cbw_read_capacity ) )
        return 0;
    unsigned char rec_data[READ_CAPACITY_DATA_LEN];
    if ( ! receive_data ( device_handle, rec_data, READ_CAPACITY_DATA_LEN ) )
        return 0;
    if ( ! check_csw ( device_handle ) )
        return 0;
    *read_capacity_data = decode_read_capacity_data(rec_data);
    return 1;
}

// read usb device list and return device handle of the one with bInterfaceClass = 8
libusb_device_handle * find_mass_storage_device ( void ) {
    libusb_device **devs;
    ssize_t cnt = libusb_get_device_list(NULL, &devs);
    if ( cnt < 0 ) {
        fprintf(stderr, "Unable to get device list\n");
        return NULL;
    }
    libusb_device_handle *device_handle = NULL;
    for ( ssize_t i = 0 ; i < cnt ; i ++ ) {
        libusb_device *device = devs[i];
        struct libusb_device_descriptor desc;
        int r = libusb_get_device_descriptor(device, &desc);
        if ( r < 0 ) {
            fprintf(stderr, "Unable to get device descriptor\n");
            continue;
        }
        
        if ( desc.bDeviceClass == LIBUSB_CLASS_MASS_STORAGE ) {
            r = libusb_open(device, &device_handle);
            if ( r < 0 ) {
                fprintf(stderr, "Unable to open device\n");
                continue;
            }
            break;
        }
        if ( desc.bDeviceClass == LIBUSB_CLASS_PER_INTERFACE ) {
            // open interface and check bInterfaceClass
            struct libusb_config_descriptor *config;
            r = libusb_get_config_descriptor(device, 0, &config);
            if ( r < 0 ) {
                fprintf(stderr, "Unable to get config descriptor\n");
                continue;
            }
            const struct libusb_interface *inter;
            const struct libusb_interface_descriptor *interdesc;
            inter = &config->interface[0];
            for ( int j = 0 ; j < inter->num_altsetting ; j ++ ) {
                interdesc = &inter->altsetting[j];
                if ( interdesc->bInterfaceClass == LIBUSB_CLASS_MASS_STORAGE ) {
                    r = libusb_open(device, &device_handle);
                    if ( r < 0 ) {
                        fprintf(stderr, "Unable to open device\n");
                        continue;
                    }
                    break;
                }
            }
            libusb_free_config_descriptor(config);
        } 
    }
    libusb_free_device_list(devs, 1);
    return device_handle;
}


int main ( void ) {
    libusb_init(NULL);
    libusb_device_handle *device_handle = find_mass_storage_device();
    if ( ! device_handle ) {
        fprintf(stderr, "No mass storage devices found\n");
        return 1;
    }
    if ( ! configure_device(device_handle) ) {
        destroy_device(device_handle);
        return 1;
    }

    print_endpoint_descriptors(device_handle);

    printf ("Max LUN: %d\n", get_max_lun(device_handle));

    TReadCapacityData capacity_data;
    if (   ! inquiry(device_handle)
        || ! req_sense(device_handle)
        || ! read_capacity(device_handle, &capacity_data) ) {
        destroy_device(device_handle);
        return 1;
    }

    /**
     *  Přečtěte masterboot - sektor 0 (povinné) a vytiskněte položky partition table (dobrovolné).
     *  Dobří programátoři se mohou pokusit o vypsání položek hlavního adresáře disku,
     *  pokud bude naformátován na filesystem FAT.
     */

    destroy_device(device_handle);
    return 0;
}
