ifeq ($(ARCH),)
ARCH=$(shell uname -m)
endif
ifeq ($(ARCH),powerpc)
ARCH=ppc64
endif
ifeq ($(CC),)
CC = gcc
AR = ar
endif
INC_ROOT=../../include
LIB_ROOT=../../lib/$(ARCH)

OPT = -O0
INC = -I$(INC_ROOT)
CFLAGS = -D_GNU_SOURCE -DARCH_$(ARCH) -std=gnu99 -g -Wall -fno-strict-aliasing -Wno-unused-function -g $(INC) $(OPT)
LIBS = -L$(LIB_ROOT) -lpthread -lzlog

