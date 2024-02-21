KERNEL_VERSION=$(shell uname -r)

all:
	make -C /lib/modules/${KERNEL_VERSION}/build/ M=$(shell pwd)
	make -C /lib/modules/${KERNEL_VERSION}/build/ M=$(shell pwd)/drivers/$(shell bash guess_dist_kernel.sh)

clean:
	make -C /lib/modules/${KERNEL_VERSION}/build/ M=$(shell pwd) clean
	make -C /lib/modules/${KERNEL_VERSION}/build/ M=$(shell pwd)/drivers/$(shell bash guess_dist_kernel.sh) clean

module_install:
	make -C /lib/modules/${KERNEL_VERSION}/build/ M=$(shell pwd) modules_install
	make -C /lib/modules/${KERNEL_VERSION}/build/ M=$(shell pwd)/drivers/$(shell bash guess_dist_kernel.sh) modules_install

