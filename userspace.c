#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>

#define READ_IOCTL _IOR('G', 0, int)
#define WRITE_IOCTL _IOW('G', 1, int)

#define PAGE_SIZE 512

char * address = NULL;
int quit_thread = 0;

void* print_long()
{	
	unsigned long curr_value, prev_value = 0; 

	curr_value = prev_value = address[0];
	printf("First value = %d\n", curr_value);	

	do{		
		if(curr_value != prev_value)
		{
			printf("First value = %d\n", curr_value);
			prev_value = curr_value;
		}
		curr_value = address[0];
	}while(!quit_thread);

}

int main(){
	char buf[20];
	int fd = -1;
	int option, handle, major=0, quit=0;
	
	pthread_t print_thread;	
	FILE *myFile;

	char *cdev = "/dev/my_char_device";
	
	if ((fd = open(cdev, O_RDWR)) < 0) {
		perror("open");
		return -1;
	}

	system("clear");

while(!quit){
	
	printf("1. Allocate Memory at kernel space\n");
	printf("2. Map Kernel space Memory\n");
	//printf("3. Read Data\n");
	//printf("0. Exit\n");
	printf("Enter your choice :: ");
	scanf("%d",&option);

	switch(option){
		case 1:	
			if(ioctl(fd, WRITE_IOCTL, "1") < 0)
				perror("Allocate Command\n");
			
			handle = open("/sys/kernel/debug/memory_map", O_RDWR);
			if(handle < 0) {
				perror("File Open\n");
				return -1;
			}

			break;
		case 2:	
			if(ioctl(fd, WRITE_IOCTL, "2") < 0)
				perror("Map Command");
			
			address = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, handle, 0);
			if (address == MAP_FAILED) {
				perror("Memory Map");
				return -1;
			}
		//	break;
		//case 3:
			pthread_create(&print_thread, NULL, print_long, NULL); 
			sleep(60);	
			quit_thread = 1;
			//break;
		//case 0:
			if(ioctl(fd, WRITE_IOCTL, "0") < 0)
				perror("Exit Command");
			close(handle);
			close(fd);			
			quit = 1;			
			break;
	}
}

	return 0;
}

