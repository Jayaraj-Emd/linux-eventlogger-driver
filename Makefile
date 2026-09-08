obj-m+=logger.o 
KDIR :=/lib/modules/$(shell uname -r)/build
PWD  :=$(shell pwd)
all:
	make -C $(KDIR) M=$(PWD)
clear:
	make -C $(KDIR) M=$(PWD) clean