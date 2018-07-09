#
# File:
#    Makefile
#
# Description:
#    Makefile for the TS Library using a VME Controller running Linux
#
# SVN: $Rev$
#
# Uncomment DEBUG line, to include some debugging info ( -g and -Wall)
DEBUG=1
#
ifndef ARCH
  ifdef LINUXVME_LIB
	ARCH=Linux
  else
	ARCH=VXWORKSPPC
  endif
endif

# Defs and build for VxWorks
ifeq ($(ARCH),VXWORKSPPC)
VXWORKS_ROOT = /site/vxworks/5.5/ppc/target

# This should be replaced with whichever directories contain jvme.h, tiLib.h, tsLib.h..
VME_INCLUDE             ?= -I$(LINUXVME_INC)

CC			= ccppc
LD			= ldppc
DEFS			= -mcpu=604 -DCPU=PPC604 -DVXWORKS -D_GNU_TOOL -mlongcall \
				-fno-for-scope -fno-builtin -fvolatile -DVXWORKSPPC
INCS			= -I. -I$(VXWORKS_ROOT)/h -I$(VXWORKS_ROOT)/h/rpc -I$(VXWORKS_ROOT)/h/net \
			 $(VME_INCLUDE)
CFLAGS			= $(INCS) $(DEFS)

endif #ARCH=VXWORKSPPC#

# Defs and build for Linux
ifeq ($(ARCH),Linux)
LINUXVME_LIB		?= ../lib
LINUXVME_INC		?= ../include

CC			= gcc
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -L. -L${LINUXVME_LIB}
INCS			= -I. -I${LINUXVME_INC} 

LIBS			= libts.a
endif #ARCH=Linux#

ifdef DEBUG
CFLAGS			+= -Wall -g
else
CFLAGS			+= -O2
endif
SRC			= tsLib.c
HDRS			= $(SRC:.c=.h)
OBJ			= tsLib.o
DEPS			= $(SRC:.c=.d)

ifeq ($(ARCH),Linux)
all: echoarch $(LIBS)
else
all: echoarch $(OBJ) 
endif

$(OBJ): $(SRC)
	@echo "Building $@ from $<"
	$(CC) $(CFLAGS) $(INCS) -c -o $@ $(SRC)

$(LIBS): $(OBJ)
	@echo "Building $@ from $<"
	$(CC) -fpic -shared $(CFLAGS) $(INCS) -o $(@:%.a=%.so) $(SRC)
	$(AR) ruv $@ $<
	$(RANLIB) $@

ifeq ($(ARCH),Linux)
links: $(LIBS)
	@ln -vsf $(PWD)/$< $(LINUXVME_LIB)/$<
	@ln -vsf $(PWD)/$(<:%.a=%.so) $(LINUXVME_LIB)/$(<:%.a=%.so)
	@ln -vsf $(PWD)/tsLib.h $(LINUXVME_INC)/tsLib.h

install: $(LIBS)
	@cp -v $(PWD)/$< $(LINUXVME_LIB)/$<
	@cp -v $(PWD)/$(<:%.a=%.so) $(LINUXVME_LIB)/$(<:%.a=%.so)
	@cp -v $(PWD)/tsLib.h $(LINUXVME_INC)/tsLib.h

%.d: %.c
	@echo "Building $@ from $<"
	@set -e; rm -f $@; \
	$(CC) -MM -shared $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)

endif

clean:
	@rm -vf tsLib.{o,d} libts.{a,so}

echoarch:
	@echo "Make for $(ARCH)"

.PHONY: clean echoarch
