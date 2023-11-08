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

  // Check if device if USB flash memory
  if (desc.bDeviceClass != LIBUSB_CLASS_MASS_STORAGE) {
    fprintf(stderr, "Device not a USB flash memory\n");
    return 1;
  }

  // Zjistime počet endpointů
  unsigned int num_endpoints = desc.bNumEndpoints;

  // Pro každý endpoint zjistíme typ a směr
  for (unsigned int i = 0; i < num_endpoints; i++) {
    struct libusb_endpoint_descriptor endpoint;
    libusb_get_endpoint_descriptor(device, i, &endpoint);

    printf("Endpoint %d: typ %d, směr %d\n", i, endpoint.bEndpointType, endpoint.bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK);
  }

  // Zjistime počet logických zařízení
  unsigned int max_lun = 0;
  libusb_control_transfer(device, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_DEVICE, USB_REQ_GET_MAX_LUN, 0, 0, &max_lun, sizeof(max_lun), 1000);

  printf("Počet logických zařízení: %d\n", max_lun);

  // Načítáme a zapisujeme sektory
  for (unsigned int lun = 0; lun < max_lun; lun++) {
    // Načtení sektoru
    unsigned char buf[512];
    libusb_bulk_transfer(device, endpoint.bEndpointAddress | LIBUSB_ENDPOINT_IN, buf, sizeof(buf), &buf[0], 1000);

    // Zobrazení načteného sektoru
    for (unsigned int i = 0; i < sizeof(buf); i++) {
      printf("%02x ", buf[i]);
    }
    printf("\n");

    // Zapsání sektoru
    memset(buf, 0xff, sizeof(buf));
    libusb_bulk_transfer(device, endpoint.bEndpointAddress | LIBUSB_ENDPOINT_OUT, buf, sizeof(buf), &buf[0], 1000);
  }

  // Zavřeme zařízení
  libusb_close(device);

  // Ukončíme knihovnu libusb
  libusb_exit(NULL);

  return 0;
}
