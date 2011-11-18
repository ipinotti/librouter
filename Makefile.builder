include ../../common.mk

CFLAGS += -I$(ROOTDIR)/include -I$(ROOTDIR)/fs/include
CFLAGS += -DCONFIG_PD3
LDFLAGS += -L$(ROOTDIR)/lib -L$(ROOTDIR)/fs/lib

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
	#HACK FIXME!
	find -iname "*.so*" -exec cp -af {} $(ROOTDIR)/lib \;

clean:
	$(MAKE) clean

distclean:
	$(MAKE) distclean
