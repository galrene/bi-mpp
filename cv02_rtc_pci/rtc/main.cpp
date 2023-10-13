#include <iostream>
#include <stdio.h>
#include <sys/io.h>
#include <string>

#define DATAPORT 0x71
#define INDEXPORT 0x70

using namespace std;

// read cmos data from address
unsigned char read_cmos_reg ( unsigned char addr ) {
    outb ( addr, INDEXPORT ); // set index register to addr
    return inb ( DATAPORT ); // read data received at addr
}

string getWeekDay ( int dayNumber ) {
    switch (dayNumber) {
        case 1:
            return "Pondelok";
        case 2:
            return "Utorok";
        case 3:
            return "Streda";
        case 4:
            return "Stvrtok";
        case 5:
            return "Piatok";
        case 6:
            return "Sobota";
        case 7:
            return "Nedela";
        default:
            return "Error";
    }
}

int main ( void ) {
    iopl(3); // enable I/O operations

    int secs = 0x00;
    int minutes = 0x02;
    int hours = 0x04;
    int dayInWeek = 0x06;
    int dayInMonth = 0x07;
    int month = 0x08;
    int year = 0x09;
    int regA = 0x0A;

    while ( read_cmos_reg(regA) & 0b01000000 ); // wait until info is valid

    printf ( "Cas: %x:%x:%x\nDatum: %x.%x.%x\n", read_cmos_reg ( hours ) + 2, read_cmos_reg ( minutes ), read_cmos_reg ( secs )
                                              , read_cmos_reg ( dayInMonth ), read_cmos_reg ( month ), read_cmos_reg ( year ) );
    cout << "Je " << getWeekDay ( read_cmos_reg(dayInWeek) ) << endl;
    return 0;
}