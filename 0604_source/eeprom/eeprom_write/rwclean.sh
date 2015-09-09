#!/bin/sh
./rom_write_clean --passes $1 --interval 1 --i2c 2 --slave 0x08 --word_offset --offset $2
