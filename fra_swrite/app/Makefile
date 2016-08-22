#Default target.
all:

CC = /opt/fsl-networking/QorIQ-SDK-V1.6/sysroots/i686-fslsdk-linux/usr/bin/powerpc64-fsl-linux/powerpc64-fsl-linux-gcc
OPT = -O0
LIB_ROOT = ../lib
LIBS = -L$(LIB_ROOT)   -lpthread -lm 
LIBS +=  -L$(LIB_ROOT)/usdpaa -lusdpaa_srio -lusdpaa_dma -lusdpaa_dma_mem -lusdpaa_of -lusdpaa_process -lfra_cfg_parser -lrman_interface -lfra_fman_port -lfra_fq_interface -lfra_bpool -lusdpaa_syscfg -lusdpaa_qbman -lusdpaa_fman -lusdpaa_dma_mem -lusdpaa_rman
INCS = -I../include
CFLAGS = -std=gnu99 -Wall -g $(OPT) $(INCS) -DDEF_PCD_PATH="../app_config/usdpaa_policy_hash_ipv4.xml" \
	                                    -DDEF_CFG_PATH="../app_config/usdpaa_config_p3_p5_serdes_0x33.xml" \
	                                    -DDEF_FRA_CFG_PATH="../app_config/dbell_tx.xml"
LDFLAGS = -T ./fra.lds

SRCS = main.c fra.c test_speed.c test_port_write.c

OBJS = $(SRCS:%.c=%.o)

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o  $@ $< -lreadline

fra: main.o fra.o test_speed.o test_port_write.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS) ../lib/libreadline.a  ../lib/libhistory.a  ../lib/libxml2.a ../lib/libtestdso.a  ../lib/libtermcap.a -ltermcap


all: clean fra 
#	@cp -f $(EXEC)  ../../../bin

clean:
	rm -f *.o *.d fra

.PHONY: all clean
