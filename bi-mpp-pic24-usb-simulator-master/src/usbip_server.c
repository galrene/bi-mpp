#include "hcd.h"
#include "sys.h"
#include "usbip.h"

#include "usbip_server.h"

static hcd_t usbhcd;
static sys_t usbsys;
static usbip_t usbip;

void __usbip_server_init() {

    // Init HCD
	hcd_init(&usbhcd);

    sys_init(&usbsys, &usbhcd);

    usbip_init(&usbip, &usbsys, "127.0.0.1", 3240);
	
}
