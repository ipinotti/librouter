include ../../common.mk

#
# standard CFLAGS
#

CFLAGS = -O2 -fpic -Wall -Wstrict-prototypes
CFLAGS += -I$(TOPDIR)/include
CFLAGS += -I$(TOPDIR)/arch/powerpc/include
CFLAGS += -I$(TOPDIR)/drivers/net
CFLAGS += -I$(ROOTDIR)/include
CFLAGS += -I$(ROOTDIR)/packages/u-boot/include
CFLAGS += -I$(ROOTDIR)/packages/iproute2/include
CFLAGS += -I$(ROOTDIR)/packages/net-snmp/include
LDFLAGS += -L$(ROOTDIR)/lib

OBJS =	acl.o \
	args.o \
	crc.o \
	crc32.o \
	debug.o \
	dev.o \
	device.o \
	dhcp.o \
	dns.o \
	error.o \
	exec.o \
	hash.o \
	hash_sn.o \
	flashsave.o \
	ip.o \
	ipsec.o \
	lan.o \
	list-lib.o \
	lock.o \
	md5.o \
	mib.o \
	ntp.o \
	nv.o \
	pam.o \
	pim.o \
	ppcio.o \
	ppp.o \
	process.o \
	qos.o \
	quagga.o \
	smcroute.o \
	ssh.o \
	str.o \
	time.o \
	tunnel.o \
	version.o \
	modem3G.o \
	vlan.o

OBJSNMP = snmp.o

LIBS = ../iproute2/lib/libnetlink.a ../pam/libpam/libpam.a ../pam/libpam_misc/libpam_misc.a

all: libconfig.so libconfigsnmp.so

libconfig.so: $(OBJS) $(LIBS)
	$(CC) -shared -Wl,-soname,libconfig.so -o libconfig.so $(OBJS) $(LDFLAGS) $(LIBS) $(LIBS_PAM) -lc -ldl

libconfigsnmp.so: $(OBJSNMP)
	$(CC) -shared -Wl,-soname,libconfigsnmp.so -o libconfigsnmp.so $(OBJSNMP) $(LDFLAGS) -lc -lnetsnmp -lcrypto

install: all
	cp -avf libconfig.* $(ROOTDIR)/lib/
	cp -avf libconfigsnmp.* $(ROOTDIR)/lib/
	cp -avf libconfig.so $(ROOTDIR)/$(FSDIR)/lib/
	cp -avf libconfigsnmp.so $(ROOTDIR)/$(FSDIR)/lib/

clean:
	rm -f $(OBJS) $(OBJSNMP) libconfig* *~
