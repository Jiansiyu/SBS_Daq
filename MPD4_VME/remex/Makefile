###############################################################################
#  Copyright (c) 2013        Southeastern Universities Research Association,
#                            Thomas Jefferson National Accelerator Facility 
#                                                                           
#    This software was developed under a United States Government license   
#    described in the NOTICE file included as part of this distribution.    
#                                                                           
#    Author:  Bryan Moffit                                                  
#             moffit@jlab.org                   Jefferson Lab, MS-12B3      
#             Phone: (757) 269-5660             12000 Jefferson Ave.        
#             Fax:   (757) 269-5800             Newport News, VA 23606      
#                                                                           
###############################################################################
#
# Description:
#    Makefile for the Remote Execute library and stand-alone client
#
# SVN: $Rev$
#
###############################################################################

#DEBUG=1

LINUXVME_LIB	?= ../lib
LINUXVME_INC	?= ../include
LINUXVME_BIN	?= ../bin

CODA		?= /daqfs/coda/3.04

MACHINE		?= `uname -m`

#CMSG_INC	?= ${CODA}/common/include
#CMSG_LIB	?= ${CODA}/Linux-${MACHINE}/lib/

CMSG_INC	?= ${CMSG_HOME}/include
CMSG_LIB	?= ${CMSG_HOME}/Linux-${MACHINE}/lib/


CC			= gcc
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -I. -I/usr/include -I${LINUXVME_INC} -I${CMSG_INC}
CFLAGS			+= -D_GNU_SOURCE
CFLAGS			+= -lm -lresolv -rdynamic -lrt -ldl
CFLAGS		       += -L. -L${CMSG_LIB} -lcmsg -lcmsgRegex
CFLAGS		       += -L${LINUXVME_LIB}
ifdef DEBUG
	CFLAGS 	       += -Wall -g
else
	CFLAGS 	       += -O2
endif

SRC			= remexLib.c remexClientLib.c
HDRS			= remexLib.h
OBJ			= $(SRC:%.c=%.o)
LIBS			= libremex.a
PROG			= remex

all: $(LIBS) $(PROG) 

%.o: %.c $(HDRS)
	@echo "Building $@"
	$(CC) $(CFLAGS) -c -o $@ $<

$(PROG): remex.c
	@echo "Building $@"
	$(CC) $(CFLAGS) -lremex -o $@ $<

$(LIBS): $(OBJ)
	@echo "Building $(@:%.a=%.so)"
	$(CC) -fpic -shared $(CFLAGS) -o $(@:%.a=%.so) $(SRC)
	@echo "Building $@"
	$(AR) ruv $@ $(OBJ)
	$(RANLIB) $@

linuxvme_install: $(LIBS) $(PROG)
	@cp -v $(LIBS) $(LIBS:%.a=%.so) $(LINUXVME_LIB)/
	@cp -v $(HDRS) $(LINUXVME_INC)/
	@cp -v $(PROG) $(LINUXVME_BIN)/

linuxvme_links: $(LIBS)
	@ln -vsf $(PWD)/*.a $(LINUXVME_LIB)
	@ln -vsf $(PWD)/*.so $(LINUXVME_LIB)
	@ln -vsf ${PWD}/*Lib.h $(LINUXVME_INC)

install: $(LIBS) $(PROG)
	@cp -v $(LIBS) $(LIBS:%.a=%.so) $(CODA)/Linux-$(MACHINE)/lib
	@cp -v $(HDRS) $(CODA)/common/include
	@cp -v $(PROG) $(CODA)/Linux-$(MACHINE)/bin


clean:
	@rm -vf ${PROG} *.{o,a,so} *~

.PHONY: clean
