obj-m += proc_module.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) CC=x86_64-linux-gnu-gcc-13 modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean