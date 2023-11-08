#include <libusb-1.0/libusb.h>
#include <stdio.h>

#define DEVICEID 0x1666
#define VENDORID 0x0961


int main(int argc, char **argv) {
  libusb_init(NULL);

  struct libusb_device_handle * device = libusb_open_device_with_vid_pid(NULL, VENDORID, DEVICEID);
  if (device == NULL) {
    fprintf(stderr, "Unable to open device\n");
    return 1;
  }

  // Get plug&play info
  struct libusb_device_descriptor desc;
  libusb_get_device_descriptor(device, &desc);
  // get number of endpoints from interface descriptor
  struct libusb_config_descriptor *config;
  libusb_get_config_descriptor(libusb_get_device(device), 0, &config);
  int num_endpoints = config->interface[0].altsetting[0].bNumEndpoints;
  // Pro každý endpoint zjistíme typ a směr
  for (unsigned int i = 0; i < num_endpoints; i++) {
    struct libusb_endpoint_descriptor endpoint;
    libusb_get_endpoint_descriptor(device, i, &endpoint);
    printf("Endpoint %d: typ %d, směr %d\n", i, endpoint.bDescriptorType, endpoint.bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK);
  }

  // Get Max LUN
  unsigned char max_lun;
  libusb_control_transfer(   device,
                             LIBUSB_ENDPOINT_IN
                           | LIBUSB_REQUEST_TYPE_CLASS 
                           | LIBUSB_RECIPIENT_INTERFACE,
                           0xFE,
                           0,
                           0,
                           &max_lun,
                           1,
                           1000);
  printf("Max LUN: %d\n", max_lun);

  // // create control block wrapper
  // struct libusb_control_setup setup;
  // setup.bmRequestType = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
  // setup.bRequest = 0x06;
  // setup.wValue = 0x0200;
  // setup.wIndex = 0;
  // setup.wLength = 0;
  // unsigned char status;
  // libusb_control_transfer(device, setup.bmRequestType, setup.bRequest, setup.wValue, setup.wIndex, &status, 1, 1000);
  // printf("Status: %d\n", status);


  // // Načítáme a zapisujeme sektory
  // for (unsigned int lun = 0; lun < max_lun; lun++) {
  //   // Načtení sektoru
  //   unsigned char buf[512];
  //   libusb_bulk_transfer(device, endpoint.bEndpointAddress | LIBUSB_ENDPOINT_IN, buf, sizeof(buf), &buf[0], 1000);

  //   // Zobrazení načteného sektoru
  //   for (unsigned int i = 0; i < sizeof(buf); i++) {
  //     printf("%02x ", buf[i]);
  //   }
  //   printf("\n");

  //   // Zapsání sektoru
  //   memset(buf, 0xff, sizeof(buf));
  //   libusb_bulk_transfer(device, endpoint.bEndpointAddress | LIBUSB_ENDPOINT_OUT, buf, sizeof(buf), &buf[0], 1000);
  // }

  // Zavřeme zařízení
  libusb_close(device);

  // Ukončíme knihovnu libusb
  libusb_exit(NULL);

  return 0;
}
