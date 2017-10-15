#include <fcntl.h>
#include <unistd.h>
#include "query_jeries_dev.h"

#define FAIL_OPEN_DEVF -1
#define FAIL_ALERT_DEV -2
#define FAIL_GET_STATE_DEV -3
#define FAIL_SLEEP_DEV -4
#define INVALID_INPUT -5

void wake_ioctl(int fd){
	if(ioctl(fd, JERIES_DEV_ALERT)){
		printf("Can't alert device\n");
		exit(FAIL_ALERT_DEV);
	}
}
void get_state_ioctl(int fd){
	if(ioctl(fd, JERIES_DEV_GET_STATE)){
		printf("Can't get device's state\n");
		exit(FAIL_GET_STATE_DEV);
	}
}
void sleep_ioctl(int fd){
	if(ioctl(fd, JERIES_DEV_SLEEP)){
		printf("Can't put device to sleep\n");
		exit(FAIL_SLEEP_DEV);
	}
}
int main(){
	int fd, sel;
	fd = open("/dev/jerdev", O_RDWR);
	if(fd < 0){
		printf("Can't open device file: %s\n", DEVICE_NAME);
		exit(FAIL_OPEN_DEVF);
	}
	//fd = 0;

	printf("Enter your desired command: 0-Wake up device\n 1-Get the device's state\n 2-Put the device to sleep\n ");
	scanf("%d", &sel);
	printf("\n");
	switch(sel){
		case 0:
			wake_ioctl(fd);
			break;
		case 1:
			get_state_ioctl(fd);
			break;
		case 2:
			sleep_ioctl(fd);
			break;
		default:
			printf("invalid input, exitting!");
			exit(INVALID_INPUT);
			break;
	}
	return 0;
}
