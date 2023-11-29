    obj-m := mpp.o
    KDIR := /usr/src/linux-headers-6.2.0-33-generic
    PWD := $(shell pwd)
    default:
    	$(MAKE) -C $(KDIR) M=$(PWD) modules