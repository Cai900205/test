# Default target.
all:

CC = gcc #/opt/fsl-networking/QorIQ-SDK-V1.6/sysroots/i686-fslsdk-linux/usr/bin/powerpc64-fsl-linux/powerpc64-fsl-linux-gcc

SRCS = IDT_OP.c main.c

OBJS = $(SRCS:%.c=%.o) 

$(OBJS): %.o : %.c
	$(CC) -c -o $@ $< -lreadline    

IDT_OP: IDT_OP.o main.o 
	$(CC) -o $@ $^ $(LIBS) -lreadline

all: $(OBJS) IDT_OP

clean:
	rm -f *.o $(OBJS) IDT_OP

.PHONY: all clean

