KERNEL_VERSION:=$(shell uname -r)
MODULE_DIR    :=$(dir $(abspath $(firstword $(MAKEFILE_LIST))))

all: 
	@echo Building ethercat_device
	make -C /lib/modules/$(KERNEL_VERSION)/build/ M=$(MODULE_DIR)/ethercat_device
	@echo Building intel igb driver
	make -C /lib/modules/$(KERNEL_VERSION)/build/ M=$(MODULE_DIR)/intel/igb
	@echo Building intel realtek driver
	make -C /lib/modules/$(KERNEL_VERSION)/build/ M=$(MODULE_DIR)/realtek

install:
	make -C /lib/modules/$(KERNEL_VERSION)/build/ M=$(MODULE_DIR)/ethercat_device modules_install
	make -C /lib/modules/$(KERNEL_VERSION)/build/ M=$(MODULE_DIR)/intel/igb modules_install
	make -C /lib/modules/$(KERNEL_VERSION)/build/ M=$(MODULE_DIR)/realtek modules_install

clean:
	make -C /lib/modules/$(KERNEL_VERSION)/build/ M=$(MODULE_DIR)/ethercat_device clean
	make -C /lib/modules/$(KERNEL_VERSION)/build/ M=$(MODULE_DIR)/intel/igb clean
	make -C /lib/modules/$(KERNEL_VERSION)/build/ M=$(MODULE_DIR)/realtek clean
