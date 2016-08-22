#Default target.
all:

CC = /opt/fsl-networking/QorIQ-SDK-V1.6/sysroots/i686-fslsdk-linux/usr/bin/powerpc64-fsl-linux/powerpc64-fsl-linux-gcc#gcc
OPT = -O0
LIB_ROOT = ../lib
LIBS = -L$(LIB_ROOT)   -lpthread -lm 
LIBS +=  -L$(LIB_ROOT)/usdpaa -lusdpaa_helper -lusdpaa_syscfg -lusdpaa_qbman \
	-lusdpaa_fman -lusdpaa_of -lusdpaa_dma_mem -lusdpaa_process
INCS = -I../include
CFLAGS = -std=gnu99 -Wall -g $(OPT) $(INCS) -DDEF_PCD_PATH="../app_config/usdpaa_policy_hash_ipv4.xml" \
	                                    -DDEF_CFG_PATH="../app_config/usdpaa_config_t4_serdes_1_1_6_6.xml" 

SRCS = helper_reflector.c

OBJS = $(SRCS:%.c=%.o)

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c -o  $@ $< -lreadline

helper_reflector: helper_reflector.o
	$(CC) $(LDFLAGS) $(XFLAGS) -o $@ $^ $(LIBS) ../lib/libreadline.a  ../lib/libhistory.a  ../lib/libxml2.a  ../lib/libtermcap.a -ltermcap


all: clean helper_reflector 
#	@cp -f $(EXEC)  ../../../bin

clean:
	rm -f *.o *.d helper_reflector

.PHONY: all clean
