################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../src/libconfig/acl.o \
../src/libconfig/args.o \
../src/libconfig/crc.o \
../src/libconfig/crc32.o \
../src/libconfig/debug.o \
../src/libconfig/dev.o \
../src/libconfig/device.o \
../src/libconfig/dhcp.o \
../src/libconfig/dns.o \
../src/libconfig/error.o \
../src/libconfig/exec.o \
../src/libconfig/flashsave.o \
../src/libconfig/hash.o \
../src/libconfig/hash_sn.o \
../src/libconfig/ip.o \
../src/libconfig/ipsec.o \
../src/libconfig/lan.o \
../src/libconfig/list-lib.o \
../src/libconfig/lock.o \
../src/libconfig/md5.o \
../src/libconfig/mib.o \
../src/libconfig/ntp.o \
../src/libconfig/nv.o \
../src/libconfig/pam.o \
../src/libconfig/pim.o \
../src/libconfig/ppcio.o \
../src/libconfig/ppp.o \
../src/libconfig/process.o \
../src/libconfig/qos.o \
../src/libconfig/quagga.o \
../src/libconfig/smcroute.o \
../src/libconfig/snmp.o \
../src/libconfig/ssh.o \
../src/libconfig/str.o \
../src/libconfig/system.o \
../src/libconfig/time.o \
../src/libconfig/tunnel.o \
../src/libconfig/version.o \
../src/libconfig/vlan.o 

C_SRCS += \
../src/libconfig/acl.c \
../src/libconfig/args.c \
../src/libconfig/crc.c \
../src/libconfig/crc32.c \
../src/libconfig/debug.c \
../src/libconfig/dev.c \
../src/libconfig/device.c \
../src/libconfig/dhcp.c \
../src/libconfig/dns.c \
../src/libconfig/error.c \
../src/libconfig/exec.c \
../src/libconfig/flashsave.c \
../src/libconfig/getpid.c \
../src/libconfig/hash.c \
../src/libconfig/hash_sn.c \
../src/libconfig/ip.c \
../src/libconfig/ipsec.c \
../src/libconfig/lan.c \
../src/libconfig/list-lib.c \
../src/libconfig/lock.c \
../src/libconfig/md5.c \
../src/libconfig/mib.c \
../src/libconfig/ntp.c \
../src/libconfig/nv.c \
../src/libconfig/pam.c \
../src/libconfig/pim.c \
../src/libconfig/ppcio.c \
../src/libconfig/ppp.c \
../src/libconfig/process.c \
../src/libconfig/qos.c \
../src/libconfig/quagga.c \
../src/libconfig/smcroute.c \
../src/libconfig/snmp.c \
../src/libconfig/ssh.c \
../src/libconfig/str.c \
../src/libconfig/system.c \
../src/libconfig/time.c \
../src/libconfig/tunnel.c \
../src/libconfig/version.c \
../src/libconfig/vlan.c 

OBJS += \
./src/libconfig/acl.o \
./src/libconfig/args.o \
./src/libconfig/crc.o \
./src/libconfig/crc32.o \
./src/libconfig/debug.o \
./src/libconfig/dev.o \
./src/libconfig/device.o \
./src/libconfig/dhcp.o \
./src/libconfig/dns.o \
./src/libconfig/error.o \
./src/libconfig/exec.o \
./src/libconfig/flashsave.o \
./src/libconfig/getpid.o \
./src/libconfig/hash.o \
./src/libconfig/hash_sn.o \
./src/libconfig/ip.o \
./src/libconfig/ipsec.o \
./src/libconfig/lan.o \
./src/libconfig/list-lib.o \
./src/libconfig/lock.o \
./src/libconfig/md5.o \
./src/libconfig/mib.o \
./src/libconfig/ntp.o \
./src/libconfig/nv.o \
./src/libconfig/pam.o \
./src/libconfig/pim.o \
./src/libconfig/ppcio.o \
./src/libconfig/ppp.o \
./src/libconfig/process.o \
./src/libconfig/qos.o \
./src/libconfig/quagga.o \
./src/libconfig/smcroute.o \
./src/libconfig/snmp.o \
./src/libconfig/ssh.o \
./src/libconfig/str.o \
./src/libconfig/system.o \
./src/libconfig/time.o \
./src/libconfig/tunnel.o \
./src/libconfig/version.o \
./src/libconfig/vlan.o 

C_DEPS += \
./src/libconfig/acl.d \
./src/libconfig/args.d \
./src/libconfig/crc.d \
./src/libconfig/crc32.d \
./src/libconfig/debug.d \
./src/libconfig/dev.d \
./src/libconfig/device.d \
./src/libconfig/dhcp.d \
./src/libconfig/dns.d \
./src/libconfig/error.d \
./src/libconfig/exec.d \
./src/libconfig/flashsave.d \
./src/libconfig/getpid.d \
./src/libconfig/hash.d \
./src/libconfig/hash_sn.d \
./src/libconfig/ip.d \
./src/libconfig/ipsec.d \
./src/libconfig/lan.d \
./src/libconfig/list-lib.d \
./src/libconfig/lock.d \
./src/libconfig/md5.d \
./src/libconfig/mib.d \
./src/libconfig/ntp.d \
./src/libconfig/nv.d \
./src/libconfig/pam.d \
./src/libconfig/pim.d \
./src/libconfig/ppcio.d \
./src/libconfig/ppp.d \
./src/libconfig/process.d \
./src/libconfig/qos.d \
./src/libconfig/quagga.d \
./src/libconfig/smcroute.d \
./src/libconfig/snmp.d \
./src/libconfig/ssh.d \
./src/libconfig/str.d \
./src/libconfig/system.d \
./src/libconfig/time.d \
./src/libconfig/tunnel.d \
./src/libconfig/version.d \
./src/libconfig/vlan.d 


# Each subdirectory must supply rules for building sources it contributes
src/libconfig/%.o: ../src/libconfig/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


