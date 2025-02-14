KERNEL_SOURCE_DIR=/lib/modules/${shell uname -r}/build/
KERNEL_VERSION=$(shell make -s -C $(KERNEL_SOURCE_DIR) kernelversion 2>/dev/null)
DISTRIBUTOR_ID=$(shell lsb_release -i | awk '{ print tolower($$3) }')
RELEASE=$(shell lsb_release -r | awk '{ print $$2 }')
CODENAME=$(shell lsb_release -c | awk '{ print tolower($$2) }')

DEBIAN_KERNEL_VERSION=$(shell uname -r)

ifeq ($(CODENAME), n/a)
DRIVER_PATH=drivers/$(DISTRIBUTOR_ID)/$(RELEASE)/$(KERNEL_VERSION)
else
ifeq ($(DISTRIBUTOR_ID), debian)
DRIVER_PATH=drivers/$(DISTRIBUTOR_ID)/$(CODENAME)/$(DEBIAN_KERNEL_VERSION)
else
DRIVER_PATH=drivers/$(DISTRIBUTOR_ID)/$(CODENAME)/$(KERNEL_VERSION)
endif
endif

all:
	make -C ${KERNEL_SOURCE_DIR} M=$(shell pwd) DRIVER_PATH=$(DRIVER_PATH)

clean:
	make -C ${KERNEL_SOURCE_DIR} M=$(shell pwd) DRIVER_PATH=$(DRIVER_PATH) clean

modules_install:
	make -C ${KERNEL_SOURCE_DIR} M=$(shell pwd) DRIVER_PATH=$(DRIVER_PATH) modules_install

