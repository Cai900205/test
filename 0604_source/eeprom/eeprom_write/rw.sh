#!/bin/sh
./rom_write --passes $5 --interval 1 --i2c $1 --slave $2 --word_offset --offset $3 --count $4
