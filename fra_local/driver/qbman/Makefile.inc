ARCH=ppc64
CC = /opt/fsl-networking/QorIQ-SDK-V1.6/sysroots/i686-fslsdk-linux/usr/bin/powerpc64-fsl-linux/powerpc64-fsl-linux-gcc
AR = ar

INC_ROOT=../usdpaa_include
LIB_ROOT=../../lib

OPT = -O0
INC = -I$(INC_ROOT)
CFLAGS = -D_GNU_SOURCE -DARCH_$(ARCH) -std=gnu99 -g -Wall -fno-strict-aliasing -Wno-unused-function -g $(INC) $(OPT)
LIBS = -L$(LIB_ROOT) -lpthread -lzlog

