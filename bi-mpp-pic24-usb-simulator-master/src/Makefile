CFLAGS=-g -O0 -I ../include
LIBOBJS=utils.o urb.o hcd.o sys.o usbip.o usb_lib_usbip.o usbip_server.o
OBJS=main.o
EXE=main
LIB=libpic24usbsim.a

GCC=gcc

DEPS := $(OBJS:.o=.d) $(LIBOBJS:.o=.d)

all: $(LIB) $(EXE)

-include $(DEPS)

%.d: %.c
	$(GCC) $(CFLAGS) -MM -o $@ $<

%.o: %.c
	$(GCC) $(CFLAGS) -c -o $@ $<

$(EXE): $(OBJS)
	gcc -o $(EXE) $(OBJS)  -L . -l pic24usbsim -l pthread

$(LIB):  $(LIBOBJS)
	ar -rc $(LIB) $(LIBOBJS)

clean:
	rm -f *.o $(EXE) $(LIB)

