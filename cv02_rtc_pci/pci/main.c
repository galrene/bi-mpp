#include <stdio.h>
#include <stdint.h>
#include <sys/io.h>

#define INDEXPORT 0x0CF8
#define DATAPORT 0x0CFC

void pci_cfg_write ( uint32_t addr, uint32_t val ) {
    outl ( addr, INDEXPORT ); // set index register to addr - expose register
    outl ( val, DATAPORT ); // write val to dataport - write to exposed register
}

uint32_t pci_cfg_read ( uint32_t addr ) {
    outl ( addr, INDEXPORT ); // set index register to addr
    return inl ( DATAPORT );
}
/* Print all device functions of device of dev_num at bus bus_num */
void print_pci_dev_funcs ( int bus_num, int dev_num ) {
    for ( int func_num = 0; func_num < 7; func_num++ ) {
        uint32_t address = 0x80000000 | (bus_num << 16) | (dev_num << 11) | (func_num << 8);
        uint32_t val = pci_cfg_read ( address );
        if ( val == 0xffffffff )
            continue;
        uint16_t vendor_id = (uint16_t) val;
        uint16_t device_id = val >> 16;
    printf ( "VendorID: %x - DeviceID: %x at 0x%x \n", vendor_id, device_id, address );
    }
}
/* Print all PCI devices at bus_num*/
void print_pci_devs ( int bus_num ) {
    for ( int dev_num = 0; dev_num < 32; dev_num++ )
        print_pci_dev_funcs ( bus_num, dev_num );
}
/* Print all PCI Device IDs and Vendor IDs*/
void print_all_pci_ids ( void ) {
    for ( int bus_num = 0; bus_num < 256; bus_num++ )
        print_pci_devs ( bus_num );
}

void print_vendor_and_device ( uint32_t addr ) {
    uint32_t val = pci_cfg_read(addr);
    uint16_t vendor_id = (uint16_t) val;
    uint16_t device_id = val >> 16;
    printf("Vendor: %x Device: %x\n", vendor_id, device_id);
}

int main ( int argc, char const *argv[] ) {
    int res;
    if ( (res = iopl(3)) < 0 ) { // allow i/o operations
        perror("errno");
        printf("Must be root %d", res);
        return 1;
    }
    print_all_pci_ids();
    
    // uint32_t addr_to_write = 0x80000000;
    
    // print_vendor_and_device ( addr_to_write ); // list device before change

    // uint32_t val_to_write = 0x59141234;
    // printf ( "Writing %x to %x\n", val_to_write, addr_to_write );
    // pci_cfg_write ( addr_to_write, val_to_write );
    
    // print_vendor_and_device ( addr_to_write ); // list device after change

    // val_to_write = 0x59148086;
    // printf ( "Reverting\n" );
    // pci_cfg_write ( addr_to_write, val_to_write );
    
    // print_vendor_and_device ( addr_to_write ); // list device after revert


    return 0;
}