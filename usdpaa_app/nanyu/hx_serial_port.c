#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <termios.h>
#include <fcntl.h>

#include "hx_serial_port.h"

#define BORTRATE B115200

char* serial_dev = "/dev/ttyA1";
int serial_fd = 0;

struct termios options;


void serial_server(){

  netboard_command_t command;

  while(1){
	int recved = recv2(serial_fd, &command, sizeof(command));
	if (command.head != 0xFE ){
	  printf("get error command head:%d, %02x %02x\n", recved, command.head, command.command);
	  continue;
	}
	if (command.command == 0x01){
	  send_status(0, 0 ,0 ,0);
	}
  }

}





int send_status(int8_t board_status, int8_t backend_link_status, int8_t manager_link_status, int8_t temperature){
  netboard_status_t status;
  status.status = board_status;
  status.work_type = 0;
  status.backend_link_status = backend_link_status;
  status.manager_link_status = manager_link_status;
  status.temperature = temperature;
  send(serial_fd, (char*)&status, sizeof(netboard_status_t));
}


void init_serial(char* serial_dev){
  serial_fd = open(serial_dev, O_RDWR|O_NOCTTY|O_NDELAY);
  tcgetattr(serial_fd, &options);

  cfsetispeed(&options, BORTRATE);
  cfsetospeed(&options, BORTRATE);

  /* set serial to raw mode */
  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); 
  options.c_oflag &= ~OPOST;
  
  tcflush(serial_fd, TCIFLUSH);
  tcsetattr(serial_fd, TCSANOW, &options);
  printf("init serial dev %s : %d\n", serial_dev, serial_fd);
}


int recv2(int serial_fd, char* data, int size){
  char buf[1024];
  int readed = 0;
  while(readed<size){
	int current_readed = recv(serial_fd, data+readed, size-readed);
	if (current_readed == -1 || current_readed == 0 ){
	  continue;
	}else{
	  readed+=current_readed;
	}
  }
  printf("readed %d data: %s \n", size, data);
}


int recv(int serial_fd, char* data, int size){
  int ret, len;
  fd_set fs_read;
  struct timeval tv_timeout;

  FD_ZERO(&fs_read);
  FD_SET(serial_fd, &fs_read);
  tv_timeout.tv_sec = (10*20/115200+2);
  tv_timeout.tv_usec = 0;

  ret = select(serial_fd+1, &fs_read, NULL, NULL, &tv_timeout);
  if (ret==-1){
  	return -1;
  }else if (ret ==0){
  	return -1;
  }

  if (FD_ISSET(serial_fd, &fs_read)){
  	len = read(serial_fd, data, size);
  	return len;
  }else{
  	return -1;
  }
  return 0;
}


int send(int serial_fd, char* data, int size){
  printf("send data: len=%d, %s \t\t ", size, data);
  int len = write(serial_fd, data, size);
  printf("write %d data \n", len);
  if (len == size){
	return len;
  }else{
	tcflush(serial_fd, TCOFLUSH);
	return -1;
  }
}

void close(int serial_fd){
  close(serial_fd);
}


void main(int argc, char* argv[]){
  printf("%d: ", argc);
  int i =0 ;
  for(; i<argc; i++){
	printf("%s ", argv[i]);
  }
  printf("\n");
  serial_dev = argv[1];

  init_serial(serial_dev); 
  if (argc>2){
	for(i=0; i<1000; i++){
	  sleep(1);
	  int rvl = send(serial_fd, argv[2], atoi(argv[3]));
	}
  }else{ 
	serial_server();
  }
}


