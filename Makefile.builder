include ../../common.mk

#CFLAGS = -O2 -fpic -Wall -Wstrict-prototypes
#CFLAGS += -I$(TOPDIR)/include               
#CFLAGS += -I$(TOPDIR)/arch/powerpc/include  
#CFLAGS += -I$(TOPDIR)/drivers/net           
CFLAGS += -I$(ROOTDIR)/include              
#CFLAGS += -I$(ROOTDIR)/packages/u-boot/include
#CFLAGS += -I$(ROOTDIR)/packages/iproute2/include
#CFLAGS += -I$(ROOTDIR)/packages/net-snmp/include
LDFLAGS += -L$(ROOTDIR)/lib
#LDFLAGS += -L$(ROOTDIR)/packages/iproute2/lib

all: config
	$(MAKE)

config: configure
	if [ ! -f Makefile ]; then \
		export CFLAGS="$(CFLAGS)"; \
		export LDFLAGS="$(LDFLAGS)"; \
		./configure --prefix='$(ROOTDIR)/fs' \
			--host='powerpc-hardhat-linux' \
			--build='i386'; \
	fi

configure:
	./autogen.sh

install:
	$(MAKE) install

clean:
	$(MAKE) clean

distclean:
	$(MAKE) clean
