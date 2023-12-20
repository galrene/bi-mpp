obj-m := mpp.o
KDIR := /usr/src/linux-headers-6.2.0-39-generic
PWD := $(shell pwd)
default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	rm -rf *.o *.ko *.mod.* *.symvers *.order .*.cmd *.mod
