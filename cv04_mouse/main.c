#include <stdio.h>
#include <libusb-1.0/libusb.h>


int main(int argc, char const *argv[]) {
    const struct libusb_version *ver;
    libusb_context *ctx;

    libusb_init(&ctx);
    ver = libusb_get_version();
    printf("LIBUSB version %d.%d.%d.%d\n",
    ver->major, ver->minor, ver->micro, ver->nano);
    return 0;
}
