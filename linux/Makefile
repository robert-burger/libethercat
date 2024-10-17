KERNEL_VERSION=$(shell uname -r)
KERNEL_SOURCE_DIR=/lib/modules/${KERNEL_VERSION}/build/
DRIVER_PATH=drivers/$(shell bash guess_dist_kernel.sh)

all:
	make -C ${KERNEL_SOURCE_DIR} M=$(shell pwd) DRIVER_PATH=$(DRIVER_PATH)

clean:
	make -C ${KERNEL_SOURCE_DIR} M=$(shell pwd) DRIVER_PATH=$(DRIVER_PATH) clean

modules_install:
	make -C ${KERNEL_SOURCE_DIR} M=$(shell pwd) DRIVER_PATH=$(DRIVER_PATH) modules_install
	depmod ${KERNEL_VERSION}

