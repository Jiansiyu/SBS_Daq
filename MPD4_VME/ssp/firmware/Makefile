# File:
#    Makefile
#
# Description:  
#	Makefile for SSP firmware updating code (Linux ony)
#
#
ARCH=Linux
DEBUG=1

#Check Operating system we are using
ifndef OSNAME
  OSNAME := $(subst -,_,$(shell uname))
endif

FWFILE = $(shell cat .currentfw)

LINUXVME_LIB	?= ${CODA}/linuxvme/lib
LINUXVME_INC	?= ${CODA}/linuxvme/include
LINUXVME_BIN    ?= ${CODA}/linuxvme/bin
LINUXVME_FW     ?= ${CODA}/linuxvme/firmware

CODALIB = $(CODA)/$(OSNAME)/lib
CODAINC = $(CODA)/common/include
CFLAGS	= -O $(DEFS) \
		-I${LINUXVME_INC} -I. \
		-L${LINUXVME_LIB} -L. -DJLAB
ifdef DEBUG
CFLAGS	+= -Wall -g
else
CFLAGS	+= -O2
endif

PROGS	= sspFirmwareUpdate

all: echoarch $(PROGS)


sspFirmwareUpdate: sspFirmwareUpdate.c
	$(CC) $(CFLAGS) -o $@ $(@:%=%.c) -lrt -ljvme -lssp

install: $(PROGS)
	@echo "Installing $(PROGS)"
	@install -v -d $(LINUXVME_BIN)
	@install -v -p $(PROGS) -D $(LINUXVME_BIN)/

fwinstall:
	@ln -sf $(FWFILE) ssp_current_firmware.mcs
	@install -v -d $(LINUXVME_FW)/ssp
	@install -p -m 664 -v $(FWFILE) \
		ssp_ctrl_current_firmware.mcs \
		-D $(LINUXVME_FW)/ssp/

clean:
	rm -f *~ $(PROGS)

echoarch:
	@echo "Make for $(ARCH)"
