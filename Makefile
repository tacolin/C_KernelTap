
ifneq ($(KERNELRELEASE),)

obj-m   := ktunnel.o
ktunnel-objs := kmain.o ktap.o ktx.o ksyscall.o krx.o

else

# ARCH ?= mips
# CROSS_COMPILE ?=
# KERNELDIR ?=

# ARCH ?= arm
# CROSS_COMPILE ?=
# KERNELDIR ?=

ARCH ?= x86
CROSS_COMPILE ?=
KERNELDIR ?= /lib/modules/$(shell uname -r)/build

PWD       := $(shell pwd)

default:
	@$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNELDIR) M=$(PWD) modules


endif

clean:
	@$(RM) -rf *.o *.ko *~ core .depend *.mod.c .*.cmd .tmp_versions .*.o.d *.symvers *.markers *.order


