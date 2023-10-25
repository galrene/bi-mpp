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

/**
 * Get address of configuration registers start of a given device by vendor id and device id
 * returns 0xffffffff if device not found.
*/
uint32_t get_cfgbase ( uint16_t D_ID, uint16_t V_ID ) {
    for ( int bus_num = 0; bus_num < 256; bus_num++ ) {
        for ( int dev_num = 0; dev_num < 32; dev_num++ )
            for ( int func_num = 0; func_num < 7; func_num++ ) {
                uint32_t address = 0x80000000 | (bus_num << 16) | (dev_num << 11) | (func_num << 8);
                uint32_t val = pci_cfg_read ( address );
                if ( val == 0xffffffff ) // device not present
                    continue;
                uint16_t vendor_id = (uint16_t) val;
                uint16_t device_id = val >> 16;

                if ( vendor_id == V_ID && device_id == D_ID )
                    return address;
        }
    }
    return 0xffffffff;
}

/* Get bar register 0 address of given device from vendor */
uint32_t get_bar( uint16_t D_ID, uint16_t V_ID ) {
    return get_cfgbase ( D_ID, V_ID ) + 0x10;
}

void test_scratchreg_write ( uint32_t bar0_val ) {
    printf ( "bar0_val: %x\n", bar0_val);
    uint32_t scratchpad_register_addr =  (bar0_val & ~3) + 7; // mask off first two bits of base0 and offset by 7

    uint8_t val_to_write = 0x5A;
    printf ( "Writing 0x%X to scratchpad\n", val_to_write );
    outb ( val_to_write, scratchpad_register_addr );
    printf ( "Read from scratchpad: 0x%X\n", inb ( scratchpad_register_addr ) );
}

uint16_t get_bar_size ( uint32_t bar_addr ) {
    pci_cfg_write ( bar_addr, 0xFFFFFFFF );
    return ~(pci_cfg_read ( bar_addr ) & 0xFFFFFFFC) + 1;
}

/**
 * Napište funkci, která vyhledá PCI zařízení s požadovaným VID a PID
 * (prohledávejte i funkce zařízení) a vrátí bázovou adresu v konfiguračním adresním prostoru.
 * Na manipulaciu sa BAR, pouzijeme pci_config_read a pci_config_write,
 * na manipulaciu so scratch registrom pouzijeme outb a outl

 1. Zapisat hodnotu do scratchregistru na ktory sa ukazuje cez BAR
 2. Precitat ci tam skutocne hodnota je
 3. Premapovat kam ukazuje BAR
 4. Precitat ze tam uz ta hodnota nie je.
*/
int main ( void ) {
    int res;
    if ( (res = iopl(3)) < 0 ) { // allow i/o operations
        perror("errno");
        printf("Must be root %d", res);
        return 1;
    }

    // Set this to values of the Serial Controller using lspci and lspci -n for IDs
    uint16_t vendor_id = 0x8086;
    uint16_t device_id = 0xa13d;
    
    uint32_t bar0_addr = get_bar ( device_id, vendor_id );

    /**
     * PART 1
     * Write 0x5A to scratchpad register of base address register 0 and read the register.
     */
    uint32_t bar0_val = pci_cfg_read ( bar0_addr );
    test_scratchreg_write ( bar0_val );
    /** 
     * PART 2
     * Register count that BAR maps
    */
    printf ( "Bar0 maps %u registers.\n", get_bar_size ( bar0_addr ) );
    /**
     * PART 3
     * Remap BAR and check that it actually changed location
     */
    uint32_t new_bar0_val = 0x2000;
    pci_cfg_write ( bar0_addr, new_bar0_val );
    uint32_t scratchpad_register_addr =  (new_bar0_val & ~3) + 7; // mask off first two bits of base0 and offset by 7
    
    printf ( "Testing previous bar0_val write\n" );
    test_scratchreg_write ( bar0_val );

    // // Shouldn't read 0x5A anymore, since the BAR0 points to a new location
    printf ( "Read from scratchpad afer BAR0 address change: 0x%X\n", inb ( scratchpad_register_addr ) );
    pci_cfg_write ( bar0_addr, bar0_val );
    printf ( "Reverting old BAR0 address\n" );
    return 0;
}
