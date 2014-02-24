#!/bin/sh
hexdump -v -e '16/1 "0x%x," "\n"' mbr
