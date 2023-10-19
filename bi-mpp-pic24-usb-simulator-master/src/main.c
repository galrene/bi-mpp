
#include <usb_lib.h>

#define MIN(a,b) ((a < b) ? (a) : (b))
#define MAX(a,b) ((a > b) ? (a) : (b))

// Délky bufferů?

#define EP0_OUT_BUF_SIZE 64
#define EP0_IN_BUF_SIZE 64

//
// Deklarace endpointů
//
DECLARE_EP_BEGIN
DECLARE_EP(ep0); // IN/OUT Control EP
DECLARE_EP_END

//
// Deklarace bufferů
//
DECLARE_BUFFER_BEGIN
DECLARE_BUFFER(ep0_buf_out, EP0_OUT_BUF_SIZE);
DECLARE_BUFFER(ep0_buf_in, EP0_IN_BUF_SIZE);
DECLARE_BUFFER_END

#define DETACHED   0
#define ATTACHED   1
#define POWERED    2        
#define DEFAULT    3
#define ADDRESSED  4
#define CONFIGURED 5

const device_descr_t devdsc = {
    sizeof (device_descr_t),
    1,
    0x0110,
    0,
    0,
    0,
    64,
    0x1111,
    0x2222,
    0x0100,
    0,
    0,
    0,
    1
};

int device_state = DETACHED;

void process_control_tranfer(int ep) {
}

void process_ep_transfer(int ep) {
}

int main(void) {

    usb_init();
    while (!is_attached());

    usb_enable();
    device_state = ATTACHED;

    while (!is_powered());
    device_state = POWERED;

    while (1) {

        if (is_reset()) {
            continue;
        }

        if (is_sof()) {
        }

        if (is_transfer_done()) {
            int ep = get_trn_status();
            if (ep == 0x80 || ep == 0x00) {
                process_control_tranfer(ep);
            } else {
                process_ep_transfer(ep);
            }
            continue;
        }
    }
    return 0;
}
