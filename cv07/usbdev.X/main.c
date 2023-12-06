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
#define EP1_IN_BUF_SIZE 64
#define EP2_OUT_BUF_SIZE 64

DECLARE_EP_BEGIN
    DECLARE_EP(ep0);   // IN/OUT Control EP
    DECLARE_EP(ep1);   // IN EP
    DECLARE_EP(ep2);   // OUT EP
DECLARE_EP_END

DECLARE_BUFFER_BEGIN
    DECLARE_BUFFER(ep0_buf_out, EP0_OUT_BUF_SIZE);
    DECLARE_BUFFER(ep0_buf_in, EP0_IN_BUF_SIZE);
    DECLARE_BUFFER(ep1_buf_in, EP1_IN_BUF_SIZE);
    DECLARE_BUFFER(ep2_buf_out, EP2_OUT_BUF_SIZE);
DECLARE_BUFFER_END

#define EP1_IN  1
#define EP2_OUT 2

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
unsigned int DEBUG = 0x00000A04;
#define CTS_SETUP   0
#define CTS_DEVDSC  1
#define CTS_ACKIN   2
#define CTS_ACKAD   3
#define CTS_CFGDSC  4
#define CTS_ACKSC   5
#define CTS_DBGDSC  6

int configuration = 0;

int out_data = 0;
int in_data  = 0;
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
                    // CONFIG DESCRIPTOR
                    case 2:
                        copy_to_buffer ( ep0_buf_in, &CONFIG, sizeof(CONFIG) );
                        usb_ep_transf_start(EP(ep0), USB_TRN_DATA1_IN,
                                            ep0_buf_in, MIN(sizeof(CONFIG),req.wLength));
                        state = CTS_CFGDSC;
                        break;
                    // DEBUG DESC
                    case 0xA:
                        copy_to_buffer ( ep0_buf_in, &DEBUG, sizeof(DEBUG) );
                        usb_ep_transf_start(EP(ep0), USB_TRN_DATA1_IN,
                                            ep0_buf_in, MIN(sizeof(DEBUG),req.wLength));
                        state = CTS_DBGDSC;
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
                // TODO - initialise endpoints
                usb_init_ep(1, EP_IN, EP(ep1));
                usb_init_ep(2, EP_OUT, EP(ep2));
                in_data = 0;
                out_data = 0;
                
                usb_ep_transf_start( EP(ep2),
                                     USB_TRN_DATA0_OUT,
                                     ep2_buf_out,
                                     0
                                    );
                out_data = ( out_data == 0 ? 1 : 0 ); // striedaj 1 a 0
                usb_ep_transf_start( EP(ep1),
                                     in_data == 1 ? USB_TRN_DATA1_IN : USB_TRN_DATA0_IN,
                                     ep1_buf_in,
                                     0
                                    );
                in_data = ( in_data == 0 ? 1 : 0 ); // striedaj 1 a 0
                
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
        case CTS_DBGDSC:
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
        // case CTS_ACKSC:
        //     usb_ep_transf_start(EP(ep0), USB_TRN_SETUP, ep0_buf_out, EP0_OUT_BUF_SIZE);
        //     break;
        
            
    }

}
/**
  Funkce bude obsahovat následující funkcionalitu

    Podle čísla koncového bodu rozhodněte, zda data odeslat nebo přijmout.
    Pro koncový bod 1 (IN) uložte data (stav tlačítek klávesnice, 1 byte) do bufferu a funkcí
    ep_tranfer_start naplánujte odeslání dat.
    Pro koncový bod 2 (OUT) vyzvedněte data z bufferu a připravte k zobrazení na displeji.
    Naplánujte další příjem dat z koncového bodu 2.

Pro každý z koncových bodů deklarujte proměnnou, která bude indikovat jaký typ datového paketu se
odešle/přijme: DATA0/DATA1. Typ paketu se musí střídat např. DATA0, DATA1, DATA0, DATA1, …​

Data přijatá z koncového bodu 2 nezobrazujte ve funkci process_ep_transfer,
protože zobrazení (pokud nepoužíváte funkce logování) může trvat dlouho a způsobit vypršení
některých timeoutů vázaných na přenosy dat. Lepším (a také rychlejším) řešením je data
uchovat v proměnné typu pole a zobrazení provést až v hlavní smyčce programu.
*/
void process_ep_transfer ( int ep, byte * buf_to_receive,
                           byte * buf_to_send, unsigned int buf_to_send_size ) {
    switch (ep)
    {
    case EP1_IN:
        copy_to_buffer(ep1_buf_in, buf_to_send, 1);
        usb_ep_transf_start( EP(ep1),
                               in_data == 1 ? USB_TRN_DATA1_IN : USB_TRN_DATA0_IN,
                               ep1_buf_in, buf_to_send_size
                             );
        in_data = ( in_data == 0 ? 1 : 0 ); // striedaj 1 a 0
        break;
    case EP2_OUT:
        copy_from_buffer(ep2_buf_out, buf_to_receive, EP2_OUT_BUF_SIZE );
        usb_ep_transf_start( EP(ep2),
                             out_data == 1 ? USB_TRN_DATA1_OUT : USB_TRN_DATA0_OUT,
                             ep2_buf_out, EP2_OUT_BUF_SIZE
                            );
        out_data = ( out_data == 0 ? 1 : 0 ); // striedaj 1 a 0
        break;
    default:
        PRINT("Unknown EP");
        break;
    }
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
            //  PRINT("SOF");
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
            else if (ep_num == EP1_IN ) {
                byte key = get_touchpad_key(); // TODO: not sure if the correct func
                while ( get_touchpad_key() == key ); // debounce
                byte buf_to_send[1] = key;
    		    process_ep_transfer(ep_num, NULL, buf_to_send, 1 );
            }
            else if (ep_num == EP2_OUT ) {
                byte buf_to_receive[EP2_OUT_BUF_SIZE];
    		    process_ep_transfer(ep_num, buf_to_receive, NULL, 0 );
                PRINTF("Received: %s", buf_to_receive);
            }
            else
                PRINT("Unknown EP");
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

