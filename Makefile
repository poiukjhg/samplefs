ifneq ($(KERNELRELEASE),)
	obj-m += samplefs.o
	samplefs-objs := super.o file.o
else
	KERNELDIR:=/lib/modules/$(shell uname -r)/build
	PWD:=$(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR)  M=$(PWD) modules
clean:
	rm -rf *.o *.mod.c *.mod.o *.ko
endif