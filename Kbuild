#KBUILD_CFLAGS   +=  -g -Wall -O0

obj-m := libethercat.o 
libethercat-y := ethercat_device.o

obj-m += drivers/$(shell bash $M/guess_dist_kernel.sh)/
