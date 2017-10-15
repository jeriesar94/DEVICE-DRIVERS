# If called directly from the command line, invoke the kernel build system.
ifeq ($(KERNELRELEASE),)
 
    KERNEL_SOURCE := /d2/works/out/kernel
    PWD := $(shell pwd)
default: module static

static:
	$(CC) -static -o jeries_app jeries_app.c 
 
module:
	$(MAKE) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) modules
 
clean:
	$(MAKE) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) clean
	${RM} jeries_app
 
# Otherwise KERNELRELEASE is defined; we've been invoked from the
# kernel build system and can use its language.
else
 
    obj-m := jeries_driver.o
 
endif
