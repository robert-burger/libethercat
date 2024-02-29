KERNEL_VERSION=$(shell uname -r)
DRIVER_PATH=drivers/$(shell bash guess_dist_kernel.sh)

all:
	make -C /lib/modules/${KERNEL_VERSION}/build/ M=$(shell pwd) DRIVER_PATH=$(DRIVER_PATH)

clean:
	make -C /lib/modules/${KERNEL_VERSION}/build/ M=$(shell pwd) DRIVER_PATH=$(DRIVER_PATH) clean

module_install:
	make -C /lib/modules/${KERNEL_VERSION}/build/ M=$(shell pwd) DRIVER_PATH=$(DRIVER_PATH) modules_install

