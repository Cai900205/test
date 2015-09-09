#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h> 

#include "libatcrypto.h"


char AT808_SECURE_CODE[3] = { 0x22, 0xe8, 0x3f };

 int 
cryptomem_ranread(int fd, char* buf, uint8_t slave, uint16_t addr, int count);
static int 
cryptomem_write(int fd, char* buf, uint8_t slave, uint16_t addr, int count);
static int 
cryptozone_read(int fd, char* buf, int zone, uint16_t addr, int count);
static int 
cryptozone_write(int fd, char* buf, int zone, uint16_t addr, int count);


/** Start to unlock the configuration zone of AT88SC0808CA.
  * @param: cm structure presented the device
  * @param: grunt the seconds to sleep after verifing the password-7,
  *               if 0, it will sleep for 5 seconds(default)
  * @return: ZERO on success, or one negative error code
  */
int at808_unlock(cm_t* cm, int grunt)
{
    int result = -1;
    int seconds = AT808_VERIFY_DELAY;
    char recv[8];
    int fd = cm->fd;
    
    // Just return if the configuration zone is unlocked.
    if (cm->unlocked)
    {
        return 0;
    }
        
    if (grunt != 0)
        seconds = grunt;
    
    // Get the secure code, which will be used to unlock the configuration zone.
    result = cryptomem_write(fd, AT808_SECURE_CODE, 
                             CVERIFY_PASSWORD, 0x0700, 0x03);
    assert(result != -1);
    
   // sleep(seconds);
    
    // Read back the write password-7 for verification that the configuration
    // zone has been unlocked.
    do {
//		printf("
    	result = cryptomem_ranread(fd, recv, CSYSTEM_WRITE, 0x00e9, 0x03);
		//printf("%d 0x%02x 0x%02x 0x%02x\n", result, recv[0], recv[1], recv[2]);

		if (recv[0] != AT808_SECURE_CODE[0])
		{
			result = -1;
		}
    } while(result == -1);


        
    cm->unlocked = 1;
    
    return 0;
    
}

/** Set the manufactur code for your product
  * @param: cm structure presented the device, which has been unlocked
  * @param: cmc information to set, which is 4 in length
  * @return: NULL
  */
void at808_set_manufactur_code(cm_t* cm, char* cmc)
{
    int result = -1;
    char recv[8];
    int fd = cm->fd;
   
    // Just return if the configuration is not unlocked. 
    assert(cm->unlocked);
    
    // Read the configuration since we should write it at address,
    // which is aligned by eight bytes.
    result = cryptomem_ranread(fd, recv, CSYSTEM_WRITE, 
                               AT808_RW_ID_FCB, 0x08);
    assert(result != -1);

	//printf("####### 0x%02x 0x%02x 0x%02x 0x%02x\n", recv[4],recv[5], recv[6], recv[7]);
    
    memcpy(recv+4, cmc, 4);
    
    result = cryptomem_write(fd, recv, CSYSTEM_WRITE,
                             AT808_RW_ID_FCB, 0x08);   
    assert(result != -1);
    sleep(1);
                                           
}

/** Set the identification number for your product
  * @param: cm structure presented the device, which has been unlocked
  * @param: cmc information to set, which is 7 in length
  * @return: NULL
  */
void at808_set_identification(cm_t* cm, char* identfy)
{
    int result = -1;
    char recv[8];   
    int fd = cm->fd;
    
    assert(cm->unlocked);

    result = cryptomem_ranread(fd, recv, CSYSTEM_WRITE, 
                               AT808_RW_ACCESS_DCR, 0x08);
    assert(result != -1);
    
    memcpy(recv+1, identfy, 7);
    
    result = cryptomem_write(fd, recv, CSYSTEM_WRITE,
                             AT808_RW_ACCESS_DCR, 0x08);   
    assert(result != -1);    
        
}

/** Set the device configuration 
  * @param: cm structure presented the device, which has been unlocked
  * @param: mode the mode that the device operate in, you should refer
  *         to the pdf
  * @return: NULL
  */
void at808_set_configuration(cm_t* cm, int* mode)
{
    int result = -1;
    char recv[8];
    int fd = cm->fd;
    
    assert(cm->unlocked);   
    
    result = cryptomem_ranread(fd, recv, CSYSTEM_WRITE, 
                               AT808_RW_ACCESS_DCR, 0x08);
    assert(result != -1); 
    
    memcpy(recv, mode, 1);
    
    result = cryptomem_write(fd, recv, CSYSTEM_WRITE,
                             AT808_RW_ACCESS_DCR, 0x08);   
    assert(result != -1); 
    
    cm->mode = *mode;
    
}

void at808_set_pac(cm_t* cm, int idx, char acc)
{
	int result = -1;
    char recv[8];
    int fd = cm->fd;
    
    assert(cm->unlocked);
    

        result = cryptomem_ranread(fd, recv, CSYSTEM_WRITE, 
                                   AT808_RW_PASSWORD_PACW0 + 0x08 * idx, 0x08);
        assert(result != -1);
    
        recv[0] = acc;
		recv[4] = acc;
        
        result = cryptomem_write(fd, recv, CSYSTEM_WRITE,
                                 AT808_RW_PASSWORD_PACW0 + 0x08 * idx, 0x08);   
        assert(result != -1);


}

/** Set the user zone[0..7] access mode.
  * @param: cm structure presented the device, which has been unlocked
  * @param: zone zone index, which you want to set
  * @param: acc access mode, which you should refer to the pdf
  * @return: NULL
  */
void at808_set_access(cm_t* cm, int zone, unsigned char* acc)
{
    int result = -1;
    char recv[8];
    int fd = cm->fd;
    
    assert(cm->unlocked);
    
    if (0 <= zone && zone <= 3)
    {
        result = cryptomem_ranread(fd, recv, CSYSTEM_WRITE, 
                                   AT808_RW_ACCESS_AR0, 0x08);
        assert(result != -1);
    
        memcpy((recv + zone * 2), acc, 1);
        
        result = cryptomem_write(fd, recv, CSYSTEM_WRITE,
                                 AT808_RW_ACCESS_AR0, 0x08);   
        assert(result != -1);
    }
    else if (4 <= zone && zone <= 7)
    {
        result = cryptomem_ranread(fd, recv, CSYSTEM_WRITE, 
                                   AT808_RW_ACCESS_AR4, 0x08);
        assert(result != -1);
    
        memcpy((recv + zone * 2), acc, 1);
        
        result = cryptomem_write(fd, recv, CSYSTEM_WRITE,
                                 AT808_RW_ACCESS_AR4, 0x08);   
        assert(result != -1);      
    } 
    
    cm->access[zone] = *acc;
            
}

/** Bind the password set to the zone[0..7].
  * @param: cm structure presented the device, which has been unlocked
  * @param: zone zone index, which you want to set
  * @param: pkey the bind manner you expected, which you should refer to 
  *         the pdf
  * @return: NULL
  */
void at808_set_pkey(cm_t* cm, int zone, unsigned char * pkey)
{
    int result = -1;
    char recv[8];
    int fd = cm->fd;
    
    assert(cm->unlocked);    
    
    if (0 <= zone && zone <= 3)
    {
        result = cryptomem_ranread(fd, recv, CSYSTEM_WRITE, 
                                   AT808_RW_ACCESS_AR0, 0x08);
        assert(result != -1);
    
        memcpy((recv + zone * 2)+1, pkey, 1);
        
        result = cryptomem_write(fd, recv, CSYSTEM_WRITE,
                                 AT808_RW_ACCESS_AR0, 0x08);   
        assert(result != -1);
    }
    else if (4 <= zone && zone <= 7)
    {
        result = cryptomem_ranread(fd, recv, CSYSTEM_WRITE, 
                                   AT808_RW_ACCESS_AR4, 0x08);
        assert(result != -1);
    
        memcpy((recv + zone * 2)+1, pkey, 1);
        
        result = cryptomem_write(fd, recv, CSYSTEM_WRITE,
                                 AT808_RW_ACCESS_AR4, 0x08);   
        assert(result != -1);          
    }
    
    cm->pkey[zone] = *pkey;
    
}

/** Set the issuer code for your product
  * @param: cm structure presented the device, which has been unlocked
  * @param: iss_code information to set, which is 16 in length
  * @return: NULL
  */
void at808_set_issuer_code(cm_t* cm, char* iss_code)
{
    int result = -1;
    int fd = cm->fd;
    
    assert(cm->unlocked);  
    
    // Since the issuer code consists of 16 bytes, we should
    // write it in two times.
    result = cryptomem_write(fd, iss_code+8, CSYSTEM_WRITE,
                             AT808_RW_ACCESS_ISC, 0x08);   
    assert(result != -1);  
    
    result = cryptomem_write(fd, iss_code, CSYSTEM_WRITE,
                             AT808_RW_ACCESS_ISC+8, 0x08);   
    assert(result != -1);      
        
}

/** Initialize the password[0..7](read or write) in configuration.
  * @param: cm structure presented the device, which has been unlocked
  * @param: idx index of the password range
  * @param: password password to set, which is 3 in length
  * @param: rw the password to set is read-password or write-password
  * @return: NULL
  */
void at808_set_password(cm_t* cm, int idx, char* password, int rw)
{
    int result = -1;
    char recv[8];
    int fd = cm->fd;
    
    assert(cm->unlocked);
    
    result = cryptomem_ranread(fd, recv, CSYSTEM_WRITE, 
                               AT808_RW_PASSWORD_PACW0 + 0x08 * idx,
                               0x08);
    assert(result != -1);    
        
    memcpy(recv+1+4*rw, password, 3);

    result = cryptomem_write(fd, recv, CSYSTEM_WRITE,
                             AT808_RW_PASSWORD_PACW0 + 0x08 * idx,
                             0x08);   
    assert(result != -1);      
}

/** Verify the password before reading or writing the user zone 
  * in password mode.
  * @param: cm structure presented the device, which has been unlocked
  * @param: idx index of the password bound to the zone, which you want
  *         to operate
  * @param: password password to verify
  * @param: rw verify for the reading or writing operation
  * @param: grunt the seconds to sleep after verifing
  * @return: NULL
  */
void at808_verify_password(cm_t* cm, int idx, char* password, int rw, int grunt)
{
    int result = -1;
    int seconds = AT808_VERIFY_DELAY;
    int fd = cm->fd;
    
//    assert(cm->unlocked);
    
    if (grunt != 0)
        seconds = grunt;    
    
    if (rw == 1)
    {
        result = cryptomem_write(fd, password, CVERIFY_PASSWORD,
                                 0x1000 + 0x100 * idx, 0x03);
        assert(result != -1);
    }
    else if (rw == 0)
    {
        result = cryptomem_write(fd, password, CVERIFY_PASSWORD,
                                 0x0000 + 0x100 * idx, 0x03);
        assert(result != -1);        
    }
   
	char recv[100];
    do {
    	result = cryptomem_ranread(fd, recv, CSYSTEM_WRITE, 0x00e9, 0x03);
//		printf("%d 0x%02x 0x%02x 0x%02x\n", result, recv[0], recv[1], recv[2]);

//		if (recv[0] != AT808_SECURE_CODE[0])
//		{
	//		result = -1;
//		}
    } while(result == -1);

    
}

/** Write th data to the user zone[0..7]
  * @param: cm structure presented the device, which has been unlocked
  * @param: zone index of the zone, which to write
  * @param: buf eight bytes data to write
  * @param: addr the offset in the zone, which should be aligned by
  *         eight bytes
  * @return: the number of bytes written successfully, or -1 on failure
  */
int at808_write_zone(cm_t* cm, int zone, char* buf, int addr)
{  
    int fd = cm->fd;
    
    // The writing bytes should be aligned by eight bytes, and it 
    // should no more than 16.
    return cryptozone_write(fd, buf, zone, addr, 0x08);
    
}

/** Read th data from the user zone[0..7]
  * @param: cm structure presented the device, which has been unlocked
  * @param: zone index of the zone, which to read
  * @param: buf receive buffer for reading
  * @param: addr the offset in the zone, which should be aligned by
  *         eight bytes
  * @return: the number of bytes read successfully, or -1 on failure
  */
int at808_read_zone(cm_t* cm, int zone, char* buf, int addr)
{  
    int fd = cm->fd;
    
    return cryptozone_read(fd, buf, zone, addr, 0x08);        
}

 int 
cryptomem_ranread(int fd, char* buf, uint8_t slave, uint16_t addr, int count)
{
    int result = -1;

#ifdef AT808
    char word_addr[4];
    // Setup the address of random read by system write(0xb4).
    result = ioctl(fd, I2C_SLAVE, AT808_DUMMY_I2C_ADDR >> 1);
    assert(!result);
    
	word_addr[0] = slave;
    word_addr[1] = addr >> 8;
    word_addr[2] = addr;
    word_addr[3] = count;    
    
   result= write(fd, word_addr, 4);
    
    // issue: We just use the "CRANDOM_READ" to read. Slave address will be
    //        0xb0 = "CWRITE_USER_ZONE" after leftshift, we should fix it in
    //        driver.
    result = ioctl(fd, I2C_SLAVE, AT808_DUMMY_I2C_ADDR >> 1);
    assert(!result);
    
    result = read(fd, buf, count);
    return result;


#else
    char word_addr[AT808_LWORD_ADDRESS + 1];
    // Setup the address of random read by system write(0xb4).
    result = ioctl(fd, I2C_SLAVE, slave >> 1);
    assert(!result);
    
    word_addr[0] = addr >> 8;
    word_addr[1] = addr;
    word_addr[2] = count;    
    
    write(fd, word_addr, 3);
    
    // issue: We just use the "CRANDOM_READ" to read. Slave address will be
    //        0xb0 = "CWRITE_USER_ZONE" after leftshift, we should fix it in
    //        driver.
    result = ioctl(fd, I2C_SLAVE, CRANDOM_READ >> 1);
    assert(!result);
    
    result = read(fd, buf, count);
    
    return result;
#endif
            
}

static int 
cryptomem_write(int fd, char* buf, uint8_t slave, uint16_t addr, int count)
{
    int result = -1;

#ifdef AT808
    char* send = malloc(AT808_MWRITE_BYTES + 4);
    assert(send); 
    
    if (count > AT808_MWRITE_BYTES)
    {
        printf("Write too much bytes at once, refused\n");
        return -1;
    }

    // Prepare the data now.
    // FIXME: We should write the number of bytes on the bus?
	send[0] = slave;
    send[1] = addr >> 8;
    send[2] = addr;
    send[3] = count;
    
    memcpy((send + 4), buf, count);

    // Set the slave address, since the CryptoMemory is a multi-address device.
    result = ioctl(fd, I2C_SLAVE, AT808_DUMMY_I2C_ADDR >> 1);
    assert(!result);
    
    result = write(fd, send, count+4);
    free(send);
    sleep(1);    
    return (result == -1)? -1: result-4;

#else
    
    char* send = malloc(AT808_MWRITE_BYTES);
    assert(send); 
    
    if (count > AT808_MWRITE_BYTES)
    {
        printf("Write too much bytes at once, refused\n");
        return -1;
    }
    
    // Prepare the data now.
    // FIXME: We should write the number of bytes on the bus?
    send[0] = addr >> 8;
    send[1] = addr;
    send[2] = count;
    
    memcpy((send + 3), buf, count);
    
    // Set the slave address, since the CryptoMemory is a multi-address device.
    result = ioctl(fd, I2C_SLAVE, slave >> 1);
    assert(!result);
    
    result = write(fd, send, count+3);
    free(send);
    
    return (result == -1)? -1: result-3;
#endif          
}

static int 
cryptozone_read(int fd, char* buf, int zone, uint16_t addr, int count)
{
    int result = -1;

#ifdef AT808
    char* send = malloc(AT808_MWRITE_BYTES);
    assert(send);

    // Set the user zone.
	send[0] = CSYSTEM_WRITE;
    send[1] = 3;
    send[2] = zone;
    send[3] = 0;
    
    result = ioctl(fd, I2C_SLAVE,  AT808_DUMMY_I2C_ADDR >> 1);
    assert(!result);
    
    result = write(fd, send, 4);
	if (result != 0x4)
	{
		return -1;
	}

	sleep(2);    
    // Setup the read address of the user zone.
    send[0] = CWRITE_USER_ZONE;
    send[1] = addr >> 8;
    send[2] = addr;
    send[3] = count;
    
    result = ioctl(fd, I2C_SLAVE,  AT808_DUMMY_I2C_ADDR >> 1);
    assert(!result);
    
    result = write(fd, send, 4);
	if (result != 0x4)
	{
		return -1;
	}
    sleep(1); 
    // Read the user zone by "CRANDOM READ".
    result = ioctl(fd, I2C_SLAVE, AT808_DUMMY_I2C_ADDR >> 1);
    assert(!result);
    
    result = read(fd, buf, count);
         
    free(send);    
        
    return result; 

#else
    
    char* send = malloc(AT808_MWRITE_BYTES);
    assert(send);

    // Set the user zone.
    send[0] = 0x03;
    send[1] = zone;
    send[2] = 0x00;
    
    result = ioctl(fd, I2C_SLAVE, CSYSTEM_WRITE >> 1);
    assert(!result);
    
    write(fd, send, 0x03);
    
    // Setup the read address of the user zone.
    send[0] = addr >> 8;
    send[1] = addr;
    send[2] = 0x00;
    
    result = ioctl(fd, I2C_SLAVE, CWRITE_USER_ZONE >> 1);
    assert(!result);
    
    write(fd, send, 0x03);
    
    // Read the user zone by "CRANDOM READ".
    result = ioctl(fd, I2C_SLAVE, CRANDOM_READ >> 1);
    assert(!result);
    
    result = read(fd, buf, count);
         
    free(send);    
        
    return result; 
#endif
       
}

static int 
cryptozone_write(int fd, char* buf, int zone, uint16_t addr, int count)
{
    int result = -1;
    
    char* send = malloc(AT808_MWRITE_BYTES+4);
    assert(send);
    
    if (count > AT808_MWRITE_BYTES)
    {
        printf("Write too much bytes at once, refused\n");
        return -1;
    }
    
    // Set the user zone.
    send[0] = CSYSTEM_WRITE;
    send[1] = 3;
    send[2] = zone;
    send[3] = 0;
    
    result = ioctl(fd, I2C_SLAVE, AT808_DUMMY_I2C_ADDR >> 1);
    assert(!result);
    sleep(1);
    result=write(fd, send, 4);
    // Write the user zone.
    send[0] = 0xB0;//CWRITE_USER_ZONE;
    send[1] = addr >> 8;
    send[2] = addr;
    send[3] = count;
    
    result = ioctl(fd, I2C_SLAVE, AT808_DUMMY_I2C_ADDR >> 1);
    assert(!result);
    
    memcpy((send + 4), buf, count);
    
    result = write(fd, send, count+4);
    free(send);
    sleep(1);    
    return (result == -1)? -1: result-4;  
            
}


