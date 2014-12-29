ifneq ($(KERNELRELEASE),)

obj-m   := ktunnel.o
ktunnel-objs := kmain.o ktap.o kudp.o ksyscall.o knetpoll.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

default:
	@$(MAKE) -C $(KERNELDIR) M=$(PWD) modules


endif

clean:
	@$(RM) -rf *.o *.ko *~ core .depend *.mod.c .*.cmd .tmp_versions .*.o.d *.symvers *.markers *.order


