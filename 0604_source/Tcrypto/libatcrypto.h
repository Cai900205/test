/** 
  *   Help routine for AT88SC0808CA at application level.
  *
  *   We just operate the CryptoMemory device(multi-address) though
  *   the device node: "/dev/i2c-%d". 
  *
  *   For more information about the driver you can refer to the source:
  *   1. drivers/i2c/i2c-dev.c
  *   2. drivers/i2c/busses/i2c-tile.c  (i2c master on tilera)
  *
  *   For more information about the device you can refer to the pdf:
  *      CryptoMemory Low Density Full Specification Complete
  *      <http://www.atmel.com/devices/at88sc0808ca.aspx?tab=documents>
  *
  */
#ifndef __AT88SC0808CA_H_
#define __AT88SC0808CA_H_


#include <stdint.h>

#define AT808
#ifdef AT808
#define AT808_NAME 		"AT808"
#define AT808_DUMMY_I2C_ADDR	0xFE
#endif


/** Length(bytes) of the word address, when operation the device. */
#define AT808_LWORD_ADDRESS               0x02

/** The maximum number bytes can be write once. */
#define AT808_MWRITE_BYTES                0x400

/** The type number of the CryptoMemory. */
#define AT808_TYPE_NUM              0x04

/** The maximum number of user zone. */
#define AT808_ZONE_NUM              0x08  

/** We should sleep some seconds after verify the password.
  * [FIXME]: The actual number of seconds should sleep. */
#define AT808_VERIFY_DELAY          0x05

/** R/W Answer to reset defined by Atmel, RO after blowing fuse. */
#define AT808_RW_ID_ATR    0x00

/** R/W Fab code defined by Atmel, RO after blowing fuse. */
#define AT808_RW_ID_FCB    0x08

/** R/W Memory test zone for testing basic communication. */
#define AT808_RW_ID_MTZ    0x0a

/** R/W Card Manufacturer Code defined by customer, RO after blowing fuse. */
#define AT808_RW_ID_CMC    0x0c

/** Read Lot history code defined by Atmel */
#define AT808_R_ID_LHC     0x10 

/** R/W Device configuration register, default value 0xff. */
#define AT808_RW_ACCESS_DCR    0x18 
#define AT808_RW_ACCESS_DCR__SHIFT_SME  0x07 /**< Supervisor mode enable */
#define AT808_RW_ACCESS_DCR__SHIFT_UCR  0x06 /**< Unlimited checksum reads */
#define AT808_RW_ACCESS_DCR__SHIFT_UAT  0x05 /**< Unlimited approve trials */  
#define AT808_RW_ACCESS_DCR__SHIFT_ETA  0x04 /**< Eight trials allowed */
#define AT808_RW_ACCESS_DCR__SHIFT_CS0  0x03 /**< Programmable chip select:0 */
#define AT808_RW_ACCESS_DCR__SHIFT_CS1  0x02 /**<                         :1 */
#define AT808_RW_ACCESS_DCR__SHIFT_CS2  0x01 /**<                         :2 */
#define AT808_RW_ACCESS_DCR__SHIFT_CS3  0x00 /**<                         :3 */

/** R/W Access control register for user zone[0..7], default value 0xff. */
#define AT808_RW_ACCESS_AR0    0x20 
#define AT808_RW_ACCESS_AR0__SHIFT_PM1    0x07 /**< Password mode:1 */
#define AT808_RW_ACCESS_AR0__SHIFT_PM0    0x06 /**<              :0 */
#define AT808_RW_ACCESS_AR0__SHIFT_AM1    0x05 /**< Authentication mode:1 */ 
#define AT808_RW_ACCESS_AR0__SHIFT_AM0    0x04 /**<                    :2 */
#define AT808_RW_ACCESS_AR0__SHIFT_ER     0x03 /**< Encryption required */
#define AT808_RW_ACCESS_AR0__SHIFT_WLM    0x02 /**< Write lock mode */
#define AT808_RW_ACCESS_AR0__SHIFT_MDF    0x01 /**< Modify forbidden */
#define AT808_RW_ACCESS_AR0__SHIFT_PGO    0x00 /**< Program only */

/** R/W Identification number customers defines during personalization. */
#define AT808_RW_ACCESS_INN    0x19 /**< */

/** R/W Password/Key register for user zone[0..7], default value 0xff. */
#define AT808_RW_ACCESS_PR0    0x21 
#define AT808_RW_ACCESS_PR0__SHIFT_AK1    0x07 /**< Authentication key:1 */
#define AT808_RW_ACCESS_PR0__SHIFT_AK0    0x06 /**<                   :0 */
#define AT808_RW_ACCESS_PR0__SHIFT_POK1   0x05 /**< Program only key:1 */
#define AT808_RW_ACCESS_PR0__SHIFT_POK0   0x04 /**<                 :0 */
#define AT808_RW_ACCESS_PR0__SHIFT_PW2    0x02 /**< Password set:2 */
#define AT808_RW_ACCESS_PR0__SHIFT_PW1    0x01 /**<             :1 */
#define AT808_RW_ACCESS_PR0__SHIFT_PW0    0x00 /**<             :0 */

#define AT808_RW_ACCESS_AR1    0x22 /**< */
#define AT808_RW_ACCESS_PR1    0x23 /**< */
#define AT808_RW_ACCESS_AR2    0x24 /**< */
#define AT808_RW_ACCESS_PR2    0x25 /**< */
#define AT808_RW_ACCESS_AR3    0x26 /**< */
#define AT808_RW_ACCESS_PR3    0x27 /**< */
#define AT808_RW_ACCESS_AR4    0x28 /**< */
#define AT808_RW_ACCESS_PR4    0x29 /**< */
#define AT808_RW_ACCESS_AR5    0x2a /**< */
#define AT808_RW_ACCESS_PR5    0x2b /**< */
#define AT808_RW_ACCESS_AR6    0x2c /**< */
#define AT808_RW_ACCESS_PR6    0x2d /**< */
#define AT808_RW_ACCESS_AR7    0x2e /**< */
#define AT808_RW_ACCESS_PR7    0x2f /**< */

/** R/W Issuer code defined by customer, RO after blowing fuse. */
#define AT808_RW_ACCESS_ISC    0x40 

#define AT808_RW_CRYPTO_AAC0   0x50 /**< R/W Authentication attempts counters */
#define AT808_RW_CRYPTO_CC0    0x51 /**< R/W cryptogram for authentication */
#define AT808_RW_CRYPTO_SEK0   0x58 /**< R/W session key for encryption */
#define AT808_RW_CRYPTO_AAC1   0x60 /**< */
#define AT808_RW_CRYPTO_CC1    0x61 /**< */
#define AT808_RW_CRYPTO_SEK1   0x68 /**< */
#define AT808_RW_CRYPTO_AAC2   0x70 /**< */
#define AT808_RW_CRYPTO_CC2    0x71 /**< */
#define AT808_RW_CRYPTO_SEK2   0x78 /**< */
#define AT808_RW_CRYPTO_AAC3   0x80 /**< */
#define AT808_RW_CRYPTO_CC3    0x81 /**< */
#define AT808_RW_CRYPTO_SEK3   0x88 /**< */

#define AT808_RW_SECRET_SSG0   0x90 /**< R/W 64-bit secret seed */
#define AT808_RW_SECRET_SSG1   0x98 /**< */
#define AT808_RW_SECRET_SSG2   0xa0 /**< */
#define AT808_RW_SECRET_SSG3   0xa8 /**< */

#define AT808_RW_PASSWORD_PACW0  0xb0 /**< R/W Attempts counters for write */
#define AT808_RW_PASSWORD_W0     0xb1 /**< R/W Password for write */
#define AT808_RW_PASSWORD_PACR0  0xb4 /**< R/W Attempts counters for read */
#define AT808_RW_PASSWORD_R0     0xb5 /**< R/W Password for read */
#define AT808_RW_PASSWORD_PACW1  0xb8 /**< */
#define AT808_RW_PASSWORD_W1     0xb9 /**< */
#define AT808_RW_PASSWORD_PACR1  0xbc /**< */
#define AT808_RW_PASSWORD_R1     0xbf /**< */
#define AT808_RW_PASSWORD_PACW2  0xc0 /**< */
#define AT808_RW_PASSWORD_W2     0xc1 /**< */
#define AT808_RW_PASSWORD_PACR2  0xc4 /**< */
#define AT808_RW_PASSWORD_R2     0xc5 /**< */
#define AT808_RW_PASSWORD_PACW3  0xc8 /**< */
#define AT808_RW_PASSWORD_W3     0xc9 /**< */
#define AT808_RW_PASSWORD_PACR3  0xcc /**< */
#define AT808_RW_PASSWORD_R3     0xcf /**< */
#define AT808_RW_PASSWORD_PACW4  0xd0 /**< */
#define AT808_RW_PASSWORD_W4     0xd1 /**< */
#define AT808_RW_PASSWORD_PACR4  0xd4 /**< */
#define AT808_RW_PASSWORD_R4     0xd5 /**< */
#define AT808_RW_PASSWORD_PACW5  0xd8 /**< */
#define AT808_RW_PASSWORD_W5     0xd9 /**< */
#define AT808_RW_PASSWORD_PACR5  0xdc /**< */
#define AT808_RW_PASSWORD_R5     0xdf /**< */
#define AT808_RW_PASSWORD_PACW6  0xe0 /**< */
#define AT808_RW_PASSWORD_W6     0xe1 /**< */
#define AT808_RW_PASSWORD_PACR6  0xe4 /**< */
#define AT808_RW_PASSWORD_R6     0xe5 /**< */
#define AT808_RW_PASSWORD_PACW7  0xe8 /**< */
#define AT808_RW_PASSWORD_W7     0xe9 /**< */
#define AT808_RW_PASSWORD_PACR7  0xec /**< */
#define AT808_RW_PASSWORD_R7     0xef /**< */

/** We just use four fuse to solodify our configure.
  * Once blown, these fused can never be reset.
  * 
  * We must blow the fuse according the sequence:  
  *     SEC:  Atmel blows the SEC fuse to lock the lot history code 
  *           before the device leaves the factory.
  *     FAB:  To lock the ATR and the fab code portions of the 
  *           configuration memory.
  *     CMA:  To lock the card manufacturer code of the configuration 
  *           memory.
  *     PER:  To lock the remainder of the configuration memory.
  */
#define SECURITY_FUSES_NUM                (4)
#define SECURITY_FUSES_SEC                0x08
#define SECURITY_FUSES_PER                0x04
#define SECURITY_FUSES_CMA                0x02
#define SECURITY_FUSES_FAB                0x01

/** CryptoMemory supports two application areas with different communication 
  * protocols:
  *     1. Two-wire serial communication for embedded applications.
  *     2. ISO 7816 asynchronous T=0 smart card interface.
  * The power-up sequence determines what mode it shall operate in.
  * [FIXME]: The difference between the two protocols, and which protocol we
  *          should operation in.
  */
#define PROTOCOLS_NUM                     (2)
#define PROTOCOLS_TWSERIAL                0x01
#define PROTOCOLS_ISO7816                 0x02

/** The concept of commands on AT88SC0808CA is same with the slave address.
  * The normal read command(0xB6) is not compatible with the i2c master on 
  * tilera because of the normal read command should be followed by one byte
  * of length. 
  * We just use the random read command to read the configuration memory, user
  * zone and the fuse byte.
  */
/** Command number of the CryptoMemory. */
#define CRYPTO_MEM_CNUM             0x07

#define CWRITE_USER_ZONE            0xB0
#define CRANDOM_READ                0xB1 
#define CNORMAL_READ                0xB2
#define CSYSTEM_WRITE               0xB4
#define CSYSTEM_READ                0xB6
#define CVERIFY_CRYPTO              0xB8
#define CVERIFY_PASSWORD            0xBA

extern char AT808_SECURE_CODE[3]; // = { 0x22, 0xe8, 0x3f };

typedef struct {
    int fd;
    int unlocked;
    int mode;
    int access[AT808_ZONE_NUM];
    int pkey[AT808_ZONE_NUM];
} cm_t;

int at808_unlock(cm_t* cm, int grunt);
void at808_set_manufactur_code(cm_t* cm, char* cmc);
void at808_set_identification(cm_t* cm, char* identfy);
void at808_set_configuration(cm_t* cm, int* mode);
void at808_set_access(cm_t* cm, int zone, unsigned char* acc);
void at808_set_pkey(cm_t* cm, int zone, unsigned char* pkey);
void at808_set_issuer_code(cm_t* cm, char* iss_code);
void at808_set_password(cm_t* cm, int idx, char* password, int rw);
void at808_verify_password(cm_t* cm, int idx, char* password, int rw, int grunt);
int at808_write_zone(cm_t* cm, int zone, char* buf, int addr);
int at808_read_zone(cm_t* cm, int zone, char* buf, int addr);

int 
cryptomem_ranread(int fd, char* buf, uint8_t slave, uint16_t addr, int count);

void at808_set_pac(cm_t* cm, int zone, char acc);

#endif /* __AT88SC0808CA_H_ */
