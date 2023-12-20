#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
/**
 * Jako parametr flags ve funkci open, použijte O_RDWR. Funkce open vrací číslo typu int,
 * tzv. file descriptor (fd), který je předán jako první argument funkcím read, write a close.
 * 
 * Jako parametr funkce open použijte soubor "/dev/mpp", který vytvoříte příkazem mknod tato:
 * mknod /dev/mpp c <major> 0
 * kde je číslo, které zjistíte v souboru /proc/devices (položka mpp).
 * (alebo automaticky v kode modulu)
 */

#define REQ_CAPITALISE 100
#define REQ_CIPHER     200

int main ( void )
{
    int fd = open ( "/dev/mpp", O_RDWR );
    if ( fd < 0 )
    {
        printf("Error opening file\n");
        return 1;
    }
    
    char read_buf[21];
    read ( fd, read_buf, 21 ); // size is expected amount of returned bytes
    printf("Read from kernel: %s", read_buf);

    char buf[] = "Ahoj ovladac jak sa mas.\n";
    write ( fd, buf, sizeof(buf) );

    ioctl(fd, REQ_CAPITALISE, 1);
    char buf_cap[] = "Ahoj ovladac jak sa mas *kricim*.\n";
    write ( fd, buf_cap, sizeof(buf_cap) );
    
    ioctl(fd, REQ_CAPITALISE, 0);
    char buf_cap2[] = "Ahoj ovladac jak sa mas *uz nekricim*.\n";
    write ( fd, buf_cap2, sizeof(buf_cap2) );

    ioctl(fd, REQ_CIPHER, 1);
    char buf_cipher[] = "Super tajna vec\n";
    write ( fd, buf_cipher, sizeof(buf_cipher) );
    ioctl(fd, REQ_CIPHER, 0);

    if ( close(fd) != 0 )
    {
        printf("Error closing file\n");
        return 1;
    }

    return 0;
}
