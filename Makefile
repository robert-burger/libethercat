KERNEL_VERSION=$(shell uname -r)
ETHERCAT_DEVICE_BASE=$(shell pwd)

all:
	make -C /lib/modules/${KERNEL_VERSION}/build/ M=$(shell pwd)

clean:
	make -C /lib/modules/${KERNEL_VERSION}/build/ M=$(shell pwd) clean

module_install:
	make -C /lib/modules/${KERNEL_VERSION}/build/ M=$(shell pwd) modules_install

