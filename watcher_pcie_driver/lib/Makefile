# This is a -*- Makefile -*-

# Default target.
all:


CC = gcc #//$(TILERA_ROOT)/bin/tile-cc



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
  vga_pcie.c \

-include $(SOURCES:.c=.d)


%.o: %.c
	$(CC) $(CFLAGS) -MD -MP -fpic -c -o $@ $<

libvga_pcie.a: vga_pcie.o 
	rm -f $@
	$(AR) cq $@ $^

libvga_pcie.so: vga_pcie.o 
	$(CC) $(CFLAGS) $(DLL_FLAGS) -Wl,-soname,$@ -shared -o $@ $^
		
all: libvga_pcie.a libvga_pcie.so 
	@cp lib*.so ../build
	@cp lib*.a ../build

clean:
	$(RM) *.d  *.o lib*.a lib*.so 

.PHONY: all clean

