/*
  serial port handler, include init, send, receive

 */

typedef struct netboard_command_t{
  uint8_t head;
  uint8_t command;
}netboard_command_t;

typedef struct netboard_status_t{
  uint8_t head; // 0x
  int8_t status; 
  int8_t work_type; // 0 pass, 1 business
  int8_t backend_link_status; // 0 ok, 1 error
  int8_t manager_link_status; // 0 ok, 1 error
  int8_t temperature;
}netboard_status_t;

int send_status(int8_t board_status, int8_t backend_link_status, int8_t manager_link_status, int8_t temperature);




/* init serial port */
void init_serial(char* serial_dev);


/* send data to serial port 
   serial_fd: serial port file desciption, created when init serial port
   data: the data to send
   size: the size of data
*/
int send(int serial_fd, char* data, int size);


/*
  receive data from serial port
  int serial_fd: serial port file desciption, created when init serial port
  char* data: the buf which store received data
  int size: the data length which you want receive from serial port
  return : -1 when get error. 0 when good luck
 */
int recv(int serial_fd, char* data, int size);


