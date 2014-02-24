## -----------------------------------------------------------------------
##
##   Copyright 2001-2008 H. Peter Anvin - All Rights Reserved
##   Copyright 2009 Intel Corporation; author: H. Peter Anvin
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
##   Boston MA 02110-1301, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

##
## Disk Wipe Tool
##

topdir = ../..
MAKEDIR = $(topdir)/mk
include $(MAKEDIR)/com32.mk

LIBS      = ../cmenu/libmenu/libmenu.a ../libupload/libcom32upload.a

MODULES	  = dwipe.c32
TESTFILES =

OBJS	  = $(patsubst %.c,%.o,$(wildcard *.c))

all: $(MODULES) $(TESTFILES)

dwipe.elf : $(OBJS) $(LIBS) $(C_LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

clean:
	rm -rf *.o *.elf *.c32 *.d
-include .*.d
