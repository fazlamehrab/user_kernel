obj-m += kernelspace.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc -pthread -o userspace userspace.c

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf userspace