include ../../common.mk

CFLAGS += -I$(ROOTDIR)/include              
LDFLAGS += -L$(ROOTDIR)/lib

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
	autoreconf -i

install:
	$(MAKE) install

clean:
	$(MAKE) clean

distclean:
	$(MAKE) distclean
