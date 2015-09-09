# Default target.
all:

CC = /opt/fsl-networking/QorIQ-SDK-V1.6/sysroots/i686-fslsdk-linux/usr/bin/powerpc64-fsl-linux/powerpc64-fsl-linux-gcc

SRCS = Tcrypto.c libatcrypto.c

OBJS = $(SRCS:%.c=%.o) 

$(OBJS): %.o : %.c
	$(CC) -c -o $@ $<    

Tcrypto: Tcrypto.o libatcrypto.o
	$(CC) -o $@ $^ $(LIBS)

all: $(OBJS) Tcrypto

clean:
	rm -f *.o $(OBJS) Tcrypto

.PHONY: all clean

