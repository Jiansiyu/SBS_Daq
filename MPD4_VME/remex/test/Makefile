#
# File:
#    Makefile
#
# Description:
#    Makefile for the testing remex
#
#
# $Date$
# $Rev$
#

LINUXVME_LIB	?= ${CODA}/extensions/linuxvme/libs
LINUXVME_INC	?= ${CODA}/extensions/linuxvme/include

ifndef CMSG_INC
	CMSG_INC	= ${CMSG_HOME}/include
endif
ifndef CMSG_LIB
	CMSG_LIB	= ${CMSG_HOME}/Linux-i686/lib/
endif

CROSS_COMPILE		=
CC			= $(CROSS_COMPILE)gcc
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -Wall -O2 -I${LINUXVME_INC} -I. -I/usr/include \
			  -L${LINUXVME_LIB} -L.
CFLAGS		       += -I${CMSG_INC} -L${CMSG_LIB} -lcmsg -lcmsgRegex
CFLAGS                 += -ldl
PROGS			= remexLibTest remexSendTest remexClientLibTest

all: $(PROGS)

clean distclean:
	@rm -f $(PROGS) *~ *.so

%: %.c
	echo "Making $@"
	$(CC) $(CFLAGS) -o $@ $(@:%=%.c) $(LIBS_$@) -lrt -lpthread -lremex 



.PHONY: all clean distclean
