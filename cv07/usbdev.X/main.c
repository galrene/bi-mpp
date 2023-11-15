#include "led.h"
#include "cpu.h"
#include "display.h"
#include "touchpad.h"
#include "log.h"
#include "usb_lib.h"

#define MIN(a, b) ((a < b) ? (a) : (b))


// -- device decsriptor --
#define ST_INIT 0
#define ST_DEVDSC_DATA 1
#define ST_DEVDSC_STATUS 2
#define ST_SETADDR_STATUS 3


const device_descr_t devdsc __attribute__((space(auto_psv))) = {
    sizeof(device_descr_t),
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

const struct config {
    config_descriptor_t config_descr;
    interf_descriptor_t interf_descr;
    endpoint_descriptor_t endp_descr1;
} CONFIG __attribute__((space(auto_psv))) = {
    {
        sizeof(config_descriptor_t),
        2,               // bDescriptorType
        sizeof(CONFIG),
        1,               // bNumInterfaces
        0x01,            // bConfigurationValue
        0,               // iConfiguration
        0x80,            // bmAttributes
        50               // bMaxPower
    },
    {
        sizeof(interf_descriptor_t),
        4,               // bDescriptorType
        0,               // bInterfaceNumber
        0x0,             // bAlternateSetting
        1,               // bNumEndpoints
        0xFF,            // bInterfaceClass
        0x0,            // bInterfaceSubClass
        0x0,            // bInterfaceProtocol
        0                // iInterface
    },
    {
        sizeof(endpoint_descriptor_t),
        5,               // bDescriptorType
        0x81,             // bEndpointAddress
        3,               // bmAttributes
        10,              // wMaxPacketSize
        1                // bInterval
    }
};
// -- device decsriptor end --

#define EP0_OUT_BUF_SIZE 64
#define EP0_IN_BUF_SIZE 64

DECLARE_EP_BEGIN
    DECLARE_EP(ep0);   // IN/OUT Control EP
DECLARE_EP_END

DECLARE_BUFFER_BEGIN
    DECLARE_BUFFER(ep0_buf_out, EP0_OUT_BUF_SIZE);
    DECLARE_BUFFER(ep0_buf_in, EP0_IN_BUF_SIZE);
DECLARE_BUFFER_END

void init_basics ( void ) {
    cpu_init();
    led_init();
    disp_init();
    touchpad_init();
    log_init();
    disp_clear();
}

#define DETACHED  0
#define ATTACHED  1
#define POWERED   2
#define DEFAULT   3
#define ADDRESSED 4
#define CONFIGURED 5
int device_state = DETACHED;

void init_usb_dev ( void ) {
    // init USB subsystem
    usb_init();
    // wait until dev is attached to PC
    while (!is_attached());
    // start USB subsystem
    usb_enable();
    device_state = ATTACHED;
    // wait until dev is powered
    while (!is_powered());
    device_state = POWERED;
}

#define CTS_SETUP   0
#define CTS_DEVDSC  1
#define CTS_ACKIN   2
#define CTS_ACKAD   3
#define CTS_CFGDSC  4
#define CTS_ACKSC   5

int configuration = 0;
void process_control_transfer ( int ep ) {
    static int state = CTS_SETUP;
    static int addr = 0;
    usb_device_req_t req;
    
    
    int setup = is_setup(ep, EP(ep0));
    if ( setup ) {
        PRINT("SETUP");
        copy_from_buffer(ep0_buf_out, &req, sizeof(req));
        PRINTF("Req type: 0x%02X\n", req.bmRequestType);
        PRINTF("Request : 0x%02X\n", req.bRequest);
        PRINTF("Index   : 0x%04X\n", req.wIndex);
        PRINTF("Value   : 0x%04X\n", req.wValue);
        PRINTF("Length  : 0x%04X\n", req.wLength);

        switch ( req.bRequest ) {
            // GET DESCRIPTOR
            case 6:
                switch ( (req.wValue >> 8) & 0xFF) {
                    // DEVICE DESCRIPTOR
                    case 1:
                        copy_to_buffer ( ep0_buf_in, &devdsc, sizeof(devdsc) );
                        usb_ep_transf_start(EP(ep0), USB_TRN_DATA1_IN,
                                            ep0_buf_in, MIN(sizeof(devdsc),req.wLength));
                        state = CTS_DEVDSC;
                        break;
                    case 2:
                        copy_to_buffer ( ep0_buf_in, &CONFIG, sizeof(CONFIG) );
                        usb_ep_transf_start(EP(ep0), USB_TRN_DATA1_IN,
                                            ep0_buf_in, MIN(sizeof(CONFIG),req.wLength));
                        state = CTS_CFGDSC;
                        break;
                    default:
                        PRINTF("Unknown desc: %d\n", (req.wValue >> 8) & 0xFF);
                }
                break;
            // Set address
            case 5:
                addr = req.wValue & 0xFF;
                usb_ep_transf_start(EP(ep0), USB_TRN_DATA1_IN, ep0_buf_in, 0);
                state = CTS_ACKAD;
                break;
            // Set configuration
            case 9:
                configuration = req.wValue & 0xFF;
                // TODO
                usb_ep_transf_start(EP(ep0), USB_TRN_DATA1_IN, ep0_buf_in, 0);
                state = CTS_ACKSC;
                break;
            default:
                PRINTF("Unknown req: %d\n", req.bRequest);
        }
        return;
    }

    switch (state) {
        case CTS_DEVDSC:
            usb_ep_transf_start(EP(ep0), USB_TRN_DATA1_OUT,
                    ep0_buf_out, EP0_OUT_BUF_SIZE);
            state = CTS_ACKIN;
            break;
        case CTS_CFGDSC:
            usb_ep_transf_start(EP(ep0), USB_TRN_DATA1_OUT,
                    ep0_buf_out, EP0_OUT_BUF_SIZE);
            state = CTS_ACKIN;
            break;
        case CTS_ACKIN:
            usb_ep_transf_start(EP(ep0), USB_TRN_SETUP, ep0_buf_out, EP0_OUT_BUF_SIZE);
            state = CTS_SETUP;
            break;
        case CTS_ACKAD:
            usb_set_address(addr);
            usb_ep_transf_start(EP(ep0), USB_TRN_SETUP, ep0_buf_out, EP0_OUT_BUF_SIZE);
            break;
        case CTS_ACKSC:
            usb_ep_transf_start(EP(ep0), USB_TRN_SETUP, ep0_buf_out, EP0_OUT_BUF_SIZE);
            break;
    }

}

void process_ep_transfer ( int ep ) {
    
}


int main(int argc, char** argv) {
    init_basics();
    PRINT("START");
    init_usb_dev();
    led_on(LED_R);
    
    int count = 0;
    while (1) {
        // testujte USB reset
    	if (is_reset()) {
            PRINT("RESET");
    		usb_reset();
            // init endpoint
            usb_init_ep(0, EP_SETUP_INOUT, EP(ep0));
            usb_ep_transf_start(EP(ep0), USB_TRN_SETUP, ep0_buf_out, EP0_OUT_BUF_SIZE);
            // start tarnsfer through ep0 (fill-in)
    		device_state = DEFAULT;
    	}
        // test IDLE (power saving mode)
    	if (is_idle())
    		// reportujte do logu
    		continue;

    	if (is_sof()) {
            // reportujte do logu
//            PRINT("SOF");
    		continue;
    	}

        // test until transfer is done
    	if (is_transfer_done()) {
            // get number of endpoint where transfer ended
    		int ep_num = get_trn_status();
            PRINTF("TRN DONE [0x%02X]\n", ep_num);
    		if (ep_num == 0 || ep_num == 0x80) { // endpoint in alebo out
                // function for servicing endpoint tranfsers
                process_control_transfer(ep_num);
                continue;
    		}
            // zpracujte prenosy na ostatnych koncovych bodoch
    		process_ep_transfer(ep_num);
    		continue;
    	}
        if ( count == 10 ) {    
            log_main_loop();
            count = 0;
        } 
        else count ++;
    }
    return 1;
}

