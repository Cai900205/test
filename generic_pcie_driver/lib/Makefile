# This is a -*- Makefile -*-

# Default target.
all:

ifdef TILERA_ROOT

include $(TILERA_ROOT)/etc/chip.mk
CC = $(TILERA_ROOT)/bin/tile-cc
AR = $(TILERA_ROOT)/bin/tile-ar

else

TILE_ARCH = $(shell uname -m)
ifeq ($(filter tile%,$(TILE_ARCH)),)
$(error TILERA_ROOT environment variable not set)
endif

endif

RM = rm -f
MKDIR = mkdir -p

INCLUDES =  
EXTRA_CFLAGS ?=

CFLAGS = -std=c99 -O2 -g -ffunction-sections -Wall -Wshadow -Werror \
  -D_GNU_SOURCE $(INCLUDES) $(EXTRA_CFLAGS)

DLL_FLAGS = \
  -Wl,--gc-sections \
  -Wl,--fatal-warnings \
  -Wl,--warn-unresolved-symbols


SOURCES := \
  sirius_pcie.c \

-include $(SOURCES:.c=.d)


%.o: %.c
	$(CC) $(CFLAGS) -MD -MP -fpic -c -o $@ $<

libsirius_pcie.a: sirius_pcie.o 
	rm -f $@
	$(AR) cq $@ $^

libsirius_pcie.so: sirius_pcie.o 
	$(CC) $(CFLAGS) $(DLL_FLAGS) -Wl,-soname,$@ -shared -o $@ $^
		
all: libsirius_pcie.a libsirius_pcie.so 
	@cp lib*.so ../build
	@cp lib*.a ../build

clean:
	$(RM) *.d  *.o lib*.a lib*.so 

.PHONY: all clean

