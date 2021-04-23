ifneq (${KERNELRELEASE},)

	obj-m  := gpiots.o
    gpiots-y := gpiots_stamp.o gpiots_fifo.o

else

	MODULE_DIR := $(shell pwd)
	KERNEL_VERSION := $(shell uname -r)
	KERNEL_DIR ?= /lib/modules/${KERNEL_VERSION}/build

#	KERNEL_DIR = /usr/local/src/linux-rpi-3.6.11
#	ARCH       = arm
#	CROSS_COMPILE = /usr/local/cross/rpi/bin/arm-linux-

	CFLAGS := -std=gnu99 -Wall -g 

all: modules test

modules:
#	${MAKE} -C ${KERNEL_DIR} SUBDIRS=${MODULE_DIR}  modules 
	${MAKE} -C ${KERNEL_DIR} M=${MODULE_DIR}  modules 

clean:
	rm -f *.o *.ko *.mod.c .*.o .*.ko .*.mod.c .*.cmd *~ test
	rm -f Module.symvers Module.markers modules.order
	rm -rf .tmp_versions

test: gpiots_test.c fifo.c
	$(CC) -o test gpiots_test.c fifo.c -lm

module-install: modules
	mkdir -p /lib/modules/${KERNEL_VERSION}/extra
	cp gpiots.ko /lib/modules/${KERNEL_VERSION}/extra/
	depmod

endif
