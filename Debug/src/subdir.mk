################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../src/acl.o \
../src/args.o \
../src/crc.o \
../src/crc32.o \
../src/debug.o \
../src/dev.o \
../src/device.o \
../src/dhcp.o \
../src/dns.o \
../src/error.o \
../src/exec.o \
../src/flashsave.o \
../src/hash.o \
../src/hash_sn.o \
../src/ip.o \
../src/ipsec.o \
../src/lan.o \
../src/libtime.o \
../src/list-lib.o \
../src/lock.o \
../src/md5.o \
../src/mib.o \
../src/modem3G.o \
../src/ntp.o \
../src/nv.o \
../src/pam.o \
../src/pim.o \
../src/ppcio.o \
../src/ppp.o \
../src/process.o \
../src/qos.o \
../src/quagga.o \
../src/smcroute.o \
../src/snmp.o \
../src/ssh.o \
../src/str.o \
../src/tunnel.o \
../src/version.o \
../src/vlan.o 

C_SRCS += \
../src/acl.c \
../src/args.c \
../src/crc.c \
../src/crc32.c \
../src/debug.c \
../src/dev.c \
../src/device.c \
../src/dhcp.c \
../src/dns.c \
../src/error.c \
../src/exec.c \
../src/flashsave.c \
../src/hash.c \
../src/hash_sn.c \
../src/ip.c \
../src/ipsec.c \
../src/lan.c \
../src/libtime.c \
../src/list-lib.c \
../src/lock.c \
../src/md5.c \
../src/mib.c \
../src/modem3G.c \
../src/ntp.c \
../src/nv.c \
../src/pam.c \
../src/pim.c \
../src/ppcio.c \
../src/ppp.c \
../src/process.c \
../src/qos.c \
../src/quagga.c \
../src/smcroute.c \
../src/snmp.c \
../src/ssh.c \
../src/str.c \
../src/tunnel.c \
../src/version.c \
../src/vlan.c 

OBJS += \
./src/acl.o \
./src/args.o \
./src/crc.o \
./src/crc32.o \
./src/debug.o \
./src/dev.o \
./src/device.o \
./src/dhcp.o \
./src/dns.o \
./src/error.o \
./src/exec.o \
./src/flashsave.o \
./src/hash.o \
./src/hash_sn.o \
./src/ip.o \
./src/ipsec.o \
./src/lan.o \
./src/libtime.o \
./src/list-lib.o \
./src/lock.o \
./src/md5.o \
./src/mib.o \
./src/modem3G.o \
./src/ntp.o \
./src/nv.o \
./src/pam.o \
./src/pim.o \
./src/ppcio.o \
./src/ppp.o \
./src/process.o \
./src/qos.o \
./src/quagga.o \
./src/smcroute.o \
./src/snmp.o \
./src/ssh.o \
./src/str.o \
./src/tunnel.o \
./src/version.o \
./src/vlan.o 

C_DEPS += \
./src/acl.d \
./src/args.d \
./src/crc.d \
./src/crc32.d \
./src/debug.d \
./src/dev.d \
./src/device.d \
./src/dhcp.d \
./src/dns.d \
./src/error.d \
./src/exec.d \
./src/flashsave.d \
./src/hash.d \
./src/hash_sn.d \
./src/ip.d \
./src/ipsec.d \
./src/lan.d \
./src/libtime.d \
./src/list-lib.d \
./src/lock.d \
./src/md5.d \
./src/mib.d \
./src/modem3G.d \
./src/ntp.d \
./src/nv.d \
./src/pam.d \
./src/pim.d \
./src/ppcio.d \
./src/ppp.d \
./src/process.d \
./src/qos.d \
./src/quagga.d \
./src/smcroute.d \
./src/snmp.d \
./src/ssh.d \
./src/str.d \
./src/tunnel.d \
./src/version.d \
./src/vlan.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


