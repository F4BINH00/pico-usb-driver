obj-m += pico_usb.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
load:
	sudo insmod pico_usb.ko
remove:
	sudo rmmod pico_usb || true
reload: remove load

