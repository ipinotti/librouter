include ../../common.mk

all: config
	$(MAKE)

config:
	mkdir -p $(ROOTDIR)/include/net-snmp/
	cp -avf $(ROOTDIR)/packages/libconfig/net-snmp-config.h $(ROOTDIR)/include/net-snmp/net-snmp-config.h
	mkdir -p $(ROOTDIR)/include/cish
	cp -avf $(ROOTDIR)/packages/cish/*.h $(ROOTDIR)/include/cish
	mkdir -p $(ROOTDIR)/include/libconfig
	cp -avf $(ROOTDIR)/packages/libconfig/*.h $(ROOTDIR)/include/libconfig

install:
	$(MAKE) install

clean:
	$(MAKE) clean

distclean:
	$(MAKE) clean
