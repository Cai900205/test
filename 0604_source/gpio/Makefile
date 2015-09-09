
# To build modules outside of the kernel tree, we run "make"
# in the kernel source tree; the Makefile these then includes this
# Makefile once again.
# This conditional selects whether we are being included from the
# kernel Makefile or not.
ifeq ($(KERNELRELEASE),)

    # Assume the source tree is where the running kernel was built
    # You should set KERNELDIR in the environment if it's elsewhere
#    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
#    KERNELDIR = /home/sjk/Compass/src/aspeed-2300-compass/kernel/linux-2.6.28.9
#	zhou.rr 20140126 fix path
    KERNELDIR = /usr/src/linux/
#    KERNELDIR = /root/NOVA/aspeed/kernel/linux-2.6.28.9
    # The current directory is passed to sub-makes as argument
    PWD := $(shell pwd)
#	LDFLAGS =

#    echo  " #### $(LDFLAGS) ####"


modules:
	echo "##### abcd $(LDFLAGS) ####"
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

.PHONY: modules modules_install clean

else
    # called from kernel build system: just declare what our modules are
    obj-m := gpio_copy.o
endif


