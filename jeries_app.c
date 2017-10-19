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
void write_string_ioctl(int fd, char *buffer){
	int buff_inc = 0;
	while(buffer[buff_inc] != '\0'){
		ioctl(fd, JERIES_DEV_WRITE, buffer[buff_inc]);
		buff_inc++;
	}

	ioctl(fd, JERIES_DEV_WRITE, buffer[buff_inc]);
}
void read_string_ioctl(int fd, int len){
	ioctl(fd, JERIES_DEV_READ, 0);
	while(len >= 0){
		ioctl(fd, JERIES_DEV_READ, 1);
		len--;
	}
}
int main(){
	int fd, sel;
	int buff_size = 0;
	char *buff = malloc(sizeof(char));
	fd = open("/dev/jerdev", O_RDWR);
	if(fd < 0){
		printf("Can't open device file: %s\n", DEVICE_NAME);
		exit(FAIL_OPEN_DEVF);
	}
	//fd = 0;

	printf("Enter your desired command: 0-Wake up device\n 1-Get the device's state\n 2-Put the device to sleep\n 3- Enter a string to be reversed\n");
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
		case 3:
			printf("Enter Your Desired String: \n");
			getchar();
			scanf("%c", &buff[buff_size]);
			while(buff[buff_size] != '\n' &&  buff[buff_size] != '\r'){
				buff = realloc(buff, (++buff_size+1)*sizeof(char));
				scanf("%c", &buff[buff_size]);
			}
			buff[buff_size] = '\0';
			write_string_ioctl(fd, buff);
			read_string_ioctl(fd, buff_size);
			free(buff);
			break;
		default:
			printf("invalid input, exitting!");
			exit(INVALID_INPUT);
			break;
	}
	return 0;
}
