# Default target.
all:

CC = /opt/fsl-networking/QorIQ-SDK-V1.6/sysroots/i686-fslsdk-linux/usr/bin/powerpc64-fsl-linux/powerpc64-fsl-linux-gcc

SRCS = gpio_app.c 

OBJS = $(SRCS:%.c=%.o) 

$(OBJS): %.o : %.c
	$(CC) -c -o $@ $<    

gpio_app: gpio_app.o 
	$(CC) -o $@ $^ $(LIBS)

all: $(OBJS) gpio_app

clean:
	rm -f *.o $(OBJS) gpio_app

.PHONY: all clean

