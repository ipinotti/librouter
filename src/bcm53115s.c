/*
 * bcm53115s.c
 *
 *  Created on: Mar 1, 2011
 *      Author: Igor Kramer Pinotti (ipinotti@pd3.com.br)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>

#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <linux/autoconf.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>

#include "options.h"

#ifdef OPTION_MANAGED_SWITCH

#include "bcm53115s.h"
#include "bcm53115s_etc.h"

#define TIMEOUT_SPI_LIMIT 10
#define ARGS_1 1
#define ARGS_2 2
#define ARGS_3 3
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static uint8_t mode;
static uint8_t bits = 8;
static uint32_t speed = 500000;
static uint16_t delay;
const int timeout_spi_limit = 100;

static uint16_t endian_swap_16bits(uint16_t * val)
{
	return ((* val << 8) | (* val >> 8)) & 0xffff;
}

static uint32_t endian_swap_32bits(uint32_t * val)
{
	 return ((((*val) & 0xff000000) >> 24) |
			(((*val) & 0x00ff0000) >>  8) |
			(((*val) & 0x0000ff00) <<  8) |
			(((*val) & 0x000000ff) << 24));
}

static int clear_buff(uint8_t buff[], int size_of)
{
	memset(buff, 0, size_of);

	return 0;
}

static int clear_tx_rx(uint8_t tx[], uint8_t rx[], int size_of_TxRx)
{
	clear_buff(tx, size_of_TxRx);
	clear_buff(rx, size_of_TxRx);

	return 0;
}

static void printf_buffer_data(uint8_t buff[], int size_of_buff)
{
	int i = 0;

	printf("Size - Buffer data: %d \n", size_of_buff);
	for (i = 0; i < size_of_buff; i++) {
		if (!(i % 6))
			printf("");
		printf("%.2X ", buff[i]);
	}
	printf("\n");
}

static void printf_spi_buffers(uint8_t tx[], uint8_t rx[], int size_of_TxRx)
{
	int i = 0;

	printf("Size - Buffer de envio: %d \n", size_of_TxRx);
	for (i = 0; i < size_of_TxRx; i++) {
		if (!(i % 6))
			printf("");
		printf("%.2X ", tx[i]);
	}

	printf("\nSize - Buffer de resposta: %d \n", size_of_TxRx);
	for (i = 0; i < size_of_TxRx; i++) {
		if (!(i % 6))
			printf("");
		printf("%.2X ", rx[i]);
	}

	printf("\n");
}

static int _bcm53115s_spi_mode (int dev)
{
	uint8_t mode = 0;
	int ret = 0;

	ret = ioctl(dev, SPI_IOC_RD_MODE, &mode);
	if (ret < 0)
		goto end;

	mode |= SPI_CPOL;
	mode |= SPI_CPHA;

	ret = ioctl(dev, SPI_IOC_WR_MODE, &mode);

end:
	if (ret < 0){
		bcm53115s_dbg_syslog("Could not set SPI mode : %s\n", strerror(errno));
		return -1;
	}
	else
		return 0;
}

/* Low level I2C functions */
static int _bcm53115s_spi_transfer(uint8_t tx[], uint8_t rx[], int size_of_TxRx)
{
	int dev, i;

	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = size_of_TxRx,
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};

	dev = open(BCM53115S_SPI_DEV, O_RDWR);
	if (dev < 0){
		bcm53115s_dbg_syslog("error opening %s: %s\n", BCM53115S_SPI_DEV, strerror(errno));
		return -1;
	}

	if (_bcm53115s_spi_mode(dev) < 0){
		bcm53115s_dbg_syslog("Could not read from device / can't send spi message : %s\n", strerror(errno));
		close(dev);
		return -1;
	}

	if (ioctl(dev, SPI_IOC_MESSAGE(1), &tr) < 0){
		bcm53115s_dbg_syslog("Could not read from device / can't send spi message : %s\n", strerror(errno));
		close(dev);
		return -1;
	}

#ifdef BCM53115S_DEBUG_CODE
	printf_spi_buffers(tx, rx, size_of_TxRx);
#endif

	close(dev);
	return 0;
}

static int _bcm53115s_spi_reg_read_raw(uint8_t page, uint8_t offset, uint8_t *buf, int len)
{
	uint8_t tx[3];
	uint8_t rx[3];
	uint8_t tx_5_step[len+2];
	uint8_t rx_5_step[len+2];
	int i;
	int timeout = 0;

	clear_tx_rx(tx,rx,sizeof(tx));
	clear_tx_rx(tx_5_step,rx_5_step,sizeof(rx_5_step));

	/*
	 * Normal SPI Mode (Command Byte)
	 * Bit7		Bit6		Bit5		Bit4		Bit3		Bit2		Bit1		Bit0
	 * 0		1			1			Mode=0		CHIP_ID2	ID1			ID0(lsb)	Rd/Wr(0/1)
	 *
	 */

	/* Normal Read Operation */
	/* 1. Issue a normal read command(0x60) to poll the SPIF bit in the
	      SPI status register(0XFE) to determine the operation can start */
	tx[0] = CMD_SPI_BYTE_RD;
	tx[1] = ROBO_SPI_STATUS_PAGE;
	tx[2] = 0x00;
	do {
		_bcm53115s_spi_transfer(tx, rx, sizeof(tx));
		if ( (timeout++) == timeout_spi_limit)
			return -1;
		usleep(100);
	} while ((rx[2] >> ROBO_SPIF_BIT) & 1); // wait SPI bit to 0

	clear_tx_rx(tx, rx, sizeof(tx));
	timeout = 0;

	/* 2. Issue a normal write command(0x61) to write the register page value
		  into the SPI page register(0xFF) 	 */
	tx[0] = CMD_SPI_BYTE_WR;
	tx[1] = ROBO_PAGE_PAGE;
	tx[2] = page;
	if (_bcm53115s_spi_transfer(tx, rx, sizeof(tx)) < 0)
		return -1;

	clear_tx_rx(tx, rx, sizeof(tx));

	/* 3. Issue a normal read command(0x60) to setup the required RobiSwitch register
		  address 	 */
	tx[0] = CMD_SPI_BYTE_RD;
	tx[1] = offset;
	tx[2] = 0x00;
	if (_bcm53115s_spi_transfer(tx, rx, sizeof(tx)) < 0)
		return -1;

	clear_tx_rx(tx, rx, sizeof(tx));

	/* 4. Issue a normal read command(0x60) to poll the RACK bit in the
	      SPI status register(0XFE) to determine the completion of read 	 */
	do
	{
		tx[0] = CMD_SPI_BYTE_RD;
		tx[1] = ROBO_SPI_STATUS_PAGE;
		tx[2] = 0x00;
		_bcm53115s_spi_transfer(tx, rx, sizeof(tx));
		if ( (timeout++) == timeout_spi_limit)
			return -1;
		usleep(100);
	}while (((rx[2] >> ROBO_RACK_BIT) & 1) == 0); // wait RACK bit to 1

	clear_tx_rx(tx, rx, sizeof(tx));
	timeout = 0;

	/* 5. Issue a normal read command(0x60) to read the specific register's conternt
		  placed in the SPI data I/O register(0xF0) 	 */
	tx_5_step[0] = CMD_SPI_BYTE_RD;
	tx_5_step[1] = ROBO_SPI_DATA_IO_0_PAGE;
	if (_bcm53115s_spi_transfer(tx_5_step, rx_5_step, sizeof(tx_5_step)) < 0)
		return -1;

	/* 6. Copy returned info to buff */
	for (i=0; i<len; i++)
		buf[i] = rx_5_step[2+i];

	return 0;
}


static int _bcm53115s_spi_reg_write_raw(uint8_t page, uint8_t offset, uint8_t *buf, int len)
{
	uint8_t tx[3];
	uint8_t rx[3];
	uint8_t tx_3_step[len+2];
	uint8_t rx_3_step[len+2];
	int i;
	int timeout = 0;

	clear_tx_rx(tx,rx,sizeof(tx));
	clear_tx_rx(tx_3_step,rx_3_step,sizeof(tx_3_step));

	/*
	 * Normal SPI Mode (Command Byte)
	 * Bit7		Bit6		Bit5		Bit4		Bit3		Bit2		Bit1		Bit0
	 * 0		1			1			Mode=0		CHIP_ID2	ID1			ID0(lsb)	Rd/Wr(0/1)
	 *
	 */

	/* Normal Write Operation */
	/* 1. Issue a normal read command(0x60) to poll the SPIF bit in the
	      SPI status register(0XFE) to determine the operation can start */
	tx[0] = CMD_SPI_BYTE_RD;
	tx[1] = ROBO_SPI_STATUS_PAGE;
	tx[2] = 0x00;
	do {
		_bcm53115s_spi_transfer(tx,rx, sizeof(tx));
		if ( (timeout++) == timeout_spi_limit)
			return -1;
		usleep(100);
	} while ((rx[2] >> ROBO_SPIF_BIT) & 1); // wait SPI bit to 0

	clear_tx_rx(tx,rx,sizeof(tx));
	timeout = 0;

	/* 2. Issue a normal write command(0x61) to write the register page value
		  into the SPI page register(0xFF) 	 */
	tx[0] = CMD_SPI_BYTE_WR;
	tx[1] = ROBO_PAGE_PAGE;
	tx[2] = page;
	if (_bcm53115s_spi_transfer(tx,rx, sizeof(tx)) < 0)
		return -1;

	clear_tx_rx(tx,rx,sizeof(tx));

	/* 3. Issue a normal write command(0x61) and write the address of the accessed
		  register followed by the write content starting from a lower byte */
	tx_3_step[0] = CMD_SPI_BYTE_WR;
	tx_3_step[1] = offset;

	for(i=0; i < len; i++)
		tx_3_step[i+2] = buf[i];

	if (_bcm53115s_spi_transfer(tx_3_step, rx_3_step, sizeof(tx_3_step)) < 0)
		return -1;

	return 0;
}

static int _bcm53115s_reg_read(uint8_t page, uint8_t offset, void *data_buf, int len)
{
	uint32_t data = 0;

	if (_bcm53115s_spi_reg_read_raw(page, offset, (uint8_t *) &data, len) < 0)
		return -1;

	if (len == sizeof(uint32_t)) {
		uint32_t data32 = endian_swap_32bits((uint32_t *) &data);
		memcpy(data_buf, (void *) &data32, len);
	} else if (len == sizeof(uint16_t)) {
		uint16_t data16 = endian_swap_16bits((uint16_t *) &data);
		memcpy(data_buf, (void *) &data16, len);
	} else
		memcpy(data_buf, (void *) &data, len);

	return 0;
}

static int _bcm53115s_reg_write(uint8_t page, uint8_t offset, void *data_buf, int len)
{
	void *data;

	if (len == sizeof(uint32_t)) {
		uint32_t data32 = endian_swap_32bits((uint32_t *)data_buf);
		data = (void *) &data32;
	} else if (len == sizeof(uint16_t)) {
		uint16_t data16 = endian_swap_16bits((uint16_t *)data_buf);
		data = (void *) &data16;
	} else
		data = data_buf;

	if (_bcm53115s_spi_reg_write_raw(page, offset, (uint8_t *) data, len) < 0)
		return -1;

	return 0;
}

/******************************************************/
/********** Exported functions ************************/
/******************************************************/

/**
 * _set_storm_protect_defaults
 *
 * Set storm protect registers to the desired default
 */
static int _set_storm_protect_defaults(void)
{
	uint8_t reg, page;
	uint32_t data;

	page = BCM53115S_STORM_PROTECT_PAGE;
	reg = BCM53115S_STORM_PROTECT_INGRESS_RATE_CTRL_REG;

	if (_bcm53115s_reg_read(page, reg, (void *) &data, sizeof(data)))
		return -1;

	data &= BCM53115S_STORM_PROTECT_RESERVED_MASK;
	data |= BCM53115S_STORM_PROTECT_MC_SUPP_EN;
	data |= BCM53115S_STORM_PROTECT_BC_SUPP_EN;
	data |= BCM53115S_STORM_PROTECT_BC_SUPP_NORMALIZED;

	if (_bcm53115s_reg_write(page, reg, (void *) &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_bcm53115s_set_broadcast_storm_protect
 *
 * Enable or Disable Broadcast Storm protection
 *
 * @param enable
 * @param port:	from 0 to 3
 * @return 0 on success, -1 otherwise
 */
int librouter_bcm53115s_set_broadcast_storm_protect(int enable, int port)
{
	uint8_t reg, page;
	uint32_t data;

	if (port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	page = BCM53115S_STORM_PROTECT_PAGE;
	reg = BCM53115S_STORM_PROTECT_PORT_CTRL_REG;
	reg += port * 0x4;

	if (_bcm53115s_reg_read(page, reg, (void *) &data, sizeof(data)))
		return -1;

	if (enable)
		data |= BCM53115S_STORM_PROTECT_PORT_CTRL_BC_SUPR_EN;
	else
		data &= ~BCM53115S_STORM_PROTECT_PORT_CTRL_BC_SUPR_EN;

	if (_bcm53115s_reg_write(page, reg, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_bcm53115s_get_broadcast_storm_protect
 *
 * Get if Broadcast Storm protection is enabled or disabled
 *
 * @param port: from 0 to 2
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_bcm53115s_get_broadcast_storm_protect(int port)
{
	uint8_t reg, page;
	uint32_t data;

	if (port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	page = BCM53115S_STORM_PROTECT_PAGE;
	reg = BCM53115S_STORM_PROTECT_PORT_CTRL_REG;
	reg += port * 0x4;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	return (data & BCM53115S_STORM_PROTECT_PORT_CTRL_BC_SUPR_EN) ? 1 : 0;
}



int librouter_bcm53115s_set_storm_protect_rate(unsigned char rate, int port)
{
	uint8_t reg, page;
	uint32_t data;

	if (port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	page = BCM53115S_STORM_PROTECT_PAGE;
	reg = BCM53115S_STORM_PROTECT_PORT_CTRL_REG;
	reg += port * 0x4;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	data &= ~BCM53115S_STORM_PROTECT_PORT_CTRL_RATE_MSK; /* Clear old limit */
	data |= rate & BCM53115S_STORM_PROTECT_PORT_CTRL_RATE_MSK;

	if (_bcm53115s_reg_write(page, reg, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_bcm53115s_get_storm_protect_rate
 *
 * Get acceptable incoming rate for bcast packets in kbps
 */
int librouter_bcm53115s_get_storm_protect_rate(unsigned char *rate, int port)
{
	uint8_t reg, page;
	uint32_t data;

	if (port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	page = BCM53115S_STORM_PROTECT_PAGE;
	reg = BCM53115S_STORM_PROTECT_PORT_CTRL_REG;
	reg += port * 0x4;


	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	*rate = (unsigned char) (data & BCM53115S_STORM_PROTECT_PORT_CTRL_RATE_MSK);

	return 0;
}

/**
 * librouter_bcm53115s_set_multicast_storm_protect
 *
 * Enable or Disable Multicast Storm protection
 *
 * @param enable
 * @return 0 on success, -1 otherwise
 */
int librouter_bcm53115s_set_multicast_storm_protect(int enable, int port)
{
	uint8_t reg, page;
	uint32_t data;

	if (port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	page = BCM53115S_STORM_PROTECT_PAGE;
	reg = BCM53115S_STORM_PROTECT_PORT_CTRL_REG;
	reg += port * 0x4;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	if (enable)
		data |= BCM53115S_STORM_PROTECT_PORT_CTRL_MC_SUPR_EN;
	else
		data &= ~BCM53115S_STORM_PROTECT_PORT_CTRL_MC_SUPR_EN;

	if (_bcm53115s_reg_write(page, reg, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_bcm53115s_get_multicast_storm_protect
 *
 * Get if Multicast Storm protection is enabled or disabled
 *
 * @param void
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_bcm53115s_get_multicast_storm_protect(int port)
{
	uint8_t reg, page;
	uint32_t data;

	if (port > 3) {
		printf("%% No such port : %d\n", port);

		return -1;
	}

	page = BCM53115S_STORM_PROTECT_PAGE;
	reg = BCM53115S_STORM_PROTECT_PORT_CTRL_REG;
	reg += port * 0x4;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	return ((data & BCM53115S_STORM_PROTECT_PORT_CTRL_MC_SUPR_EN) ? 1 : 0);
}
//
///**
// * librouter_bcm53115s_set_replace_null_vid
// *
// * Enable or Disable NULL VID replacement with port VID
// *
// * @param enable
// * @return 0 on success, -1 otherwise
// */
//int librouter_bcm53115s_set_replace_null_vid(int enable)
//{
//	__u8 data;
//
//	if (_bcm53115s_reg_read(BCM53115SREG_GLOBAL_CONTROL2, &data, sizeof(data)))
//		return -1;
//
//	if (enable)
//		data |= BCM53115S_ENABLE_REPLACE_NULL_VID_MSK;
//	else
//		data &= ~BCM53115S_ENABLE_REPLACE_NULL_VID_MSK;
//
//	if (_bcm53115s_reg_write(BCM53115SREG_GLOBAL_CONTROL4, &data, sizeof(data)))
//		return -1;
//
//	return 0;
//}
//
///**
// * librouter_bcm53115s_get_replace_null_vid
// *
// * Get NULL VID replacement configuration
// *
// * @param void
// * @return 1 if enabled, 0 if disabled, -1 if error
// */
//int librouter_bcm53115s_get_replace_null_vid(void)
//{
//	__u8 data;
//
//	if (_bcm53115s_reg_read(BCM53115SREG_GLOBAL_CONTROL4, &data, sizeof(data)))
//		return -1;
//
//	return ((data | BCM53115S_ENABLE_REPLACE_NULL_VID_MSK) ? 1 : 0);
//}
//
///**
// * librouter_bcm53115s_set_egress_rate_limit
// *
// * Set rate limit for a certain port and priority queue.
// * The rate will be rounded to multiples of 1Mbps or
// * multiples of 64kbps if values are lower than 1Mbps.
// *
// * @param port	From 0 to 2
// * @param prio	From 0 to 3
// * @param rate	In Kbps
// * @return 0 on success, -1 otherwise
// */
//int librouter_bcm53115s_set_egress_rate_limit(int port, int prio, int rate)
//{
//	__u8 reg, data;
//
//	if (port < 0 || port > 3) {
//		printf("%% No such port : %d\n", port);
//		return -1;
//	}
//
//	if (prio > 3) {
//		printf("%% No such priority : %d\n", prio);
//		return -1;
//	}
//
//	/* Calculate reg offset */
//	reg = BCM53115SREG_INGRESS_RATE_LIMIT;
//	reg += 0x4 * port;
//	reg += prio;
//
//	if (!rate) {
//		data = (__u8) 0;
//	} else if (rate >= 1000) {
//		data = (__u8) rate/1000;
//	} else {
//		data = 0x65;
//		data += (__u8) (rate/64 - 1);
//	}
//
//	return _bcm53115s_reg_write(reg, &data, sizeof(data));
//}
//
///**
// * librouter_bcm53115s_get_egress_rate_limit
// *
// * Get rate limit from a certain port and priority queue
// *
// * @param port
// * @param prio
// * @return The configured rate limit or -1 if error
// */
//int librouter_bcm53115s_get_egress_rate_limit(int port, int prio)
//{
//	__u8 reg, data;
//	int rate;
//
//	if (port < 0 || port > 3) {
//		printf("%% No such port : %d\n", port);
//		return -1;
//	}
//
//	if (prio > 3) {
//		printf("%% No such priority : %d\n", prio);
//		return -1;
//	}
//
//	/* Calculate reg offset */
//	reg = BCM53115SREG_INGRESS_RATE_LIMIT;
//	reg += 0x4 * port;
//	reg += prio;
//
//	if (_bcm53115s_reg_read(reg, &data, sizeof(data) < 0))
//		return -1;
//	if (!data)
//		rate = 100000;
//	else if (data < 0x65)
//		rate = data * 1000;
//	else
//		rate = data * 64;
//
//	return rate;
//}
//
///**
// * librouter_bcm53115s_set_ingress_rate_limit
// *
// * Set rate limit for a certain port and priority queue.
// * The rate will be rounded to multiples of 1Mbps or
// * multiples of 64kbps if values are lower than 1Mbps.
// *
// * @param port	From 0 to 2
// * @param prio	From 0 to 3
// * @param rate	In Kbps
// * @return 0 on success, -1 otherwise
// */
//int librouter_bcm53115s_set_ingress_rate_limit(int port, int prio, int rate)
//{
//	__u8 reg, data;
//
//	if (port < 0 || port > 3) {
//		printf("%% No such port : %d\n", port);
//		return -1;
//	}
//
//	if (prio > 3) {
//		printf("%% No such priority : %d\n", prio);
//		return -1;
//	}
//
//	/* Calculate reg offset */
//	reg = BCM53115SREG_INGRESS_RATE_LIMIT;
//	reg += 0x10 * port;
//	reg += prio;
//
//	if (!rate) {
//		data = (__u8) 0;
//	} else if (rate >= 1000) {
//		data = (__u8) rate/1000;
//	} else {
//		data = 0x65;
//		data += (__u8) (rate/64 - 1);
//	}
//
//	return _bcm53115s_reg_write(reg, &data, sizeof(data));
//}
//
///**
// * librouter_bcm53115s_get_ingress_rate_limit
// *
// * Get ingress rate limit from a certain port and priority queue
// *
// * @param port
// * @param prio
// * @return The configured rate limit or -1 if error
// */
//int librouter_bcm53115s_get_ingress_rate_limit(int port, int prio)
//{
//	__u8 reg, data;
//	int rate;
//
//	if (port < 0 || port > 3) {
//		printf("%% No such port : %d\n", port);
//		return -1;
//	}
//
//	if (prio > 3) {
//		printf("%% No such priority : %d\n", prio);
//		return -1;
//	}
//
//	/* Calculate reg offset */
//	reg = BCM53115SREG_INGRESS_RATE_LIMIT;
//	reg += 0x10 * port;
//	reg += prio;
//
//	if (_bcm53115s_reg_read(reg, &data, sizeof(data) < 0))
//		return -1;
//	if (!data)
//		rate = 100000;
//	else if (data < 0x65)
//		rate = data * 1000;
//	else
//		rate = data * 64;
//
//	return rate;
//}

/**
 * librouter_bcm53115s_set_8021q	Enable/disable 802.1q (VLAN)
 *
 * @param enable
 * @return 0 if success, -1 if error
 */
int librouter_bcm53115s_set_8021q(int enable)
{
	uint32_t data = 0;

	if (_bcm53115s_reg_read(ROBO_VLAN_PAGE, 0x00, &data,1))
		return -1;

	if (enable)
		data |= BCM53115S_ENABLE_8021Q_MSK;
	else
		data &= ~BCM53115S_ENABLE_8021Q_MSK;

	if (_bcm53115s_reg_write(ROBO_VLAN_PAGE, 0x00, &data,1))
		return -1;

	return 0;
}

/**
 * librouter_bcm53115s_get_8021q	Get if 802.1q is enabled or disabled
 *
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_bcm53115s_get_8021q(void)
{
	uint32_t data;

	if (_bcm53115s_reg_read(ROBO_VLAN_PAGE, 0x00, &data, sizeof(data)))
		return -1;

	return ((data | BCM53115S_ENABLE_8021Q_MSK) ? 1 : 0);
}

/**
 * librouter_bcm53115s_set_wrr	Enable/disable Weighted Round Robin
 *
 * @param enable
 * @return 0 if success, -1 otherwise
 */
int librouter_bcm53115s_set_wrr(int enable)
{
	uint8_t data;

	if (_bcm53115s_reg_read(BCM53115S_QOS_PAGE, BCM53115S_QOS_TXQ_CONTROL_REG, &data, sizeof(data)))
		return -1;

	/* 53115S-DS Page 259: When these bits are cleared, WRR are enabled in all 4 queues */
	if (enable)
		data &= ~BCM53115S_QOS_TXQ_CONTROL_WRR_MSK;
	else
		data |= BCM53115S_QOS_TXQ_CONTROL_WRR_MSK; /* Strict priority */

	if (_bcm53115s_reg_write(BCM53115S_QOS_PAGE, BCM53115S_QOS_TXQ_CONTROL_REG, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_bcm53115s_get_wfq	Get if WRR is enabled or disabled
 *
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_bcm53115s_get_wrr(void)
{
	uint8_t data;

	if (_bcm53115s_reg_read(BCM53115S_QOS_PAGE, BCM53115S_QOS_TXQ_CONTROL_REG, &data, sizeof(data)))
		return -1;

	return ((data & BCM53115S_QOS_TXQ_CONTROL_WRR_MSK) ? 0 : 1);
}

///**
// * librouter_bcm53115s_set_default_vid	Set port default 802.1q VID
// *
// * @param port
// * @param vid
// * @return 0 if success, -1 if error
// */
//int librouter_bcm53115s_set_default_vid(int port, int vid)
//{
//	__u8 vid_l, vid_u;
//	__u8 reg, data;
//
//	if (port < 0 || port > 3) {
//		printf("%% No such port : %d\n", port);
//		return -1;
//	}
//
//	if (vid > 4095) {
//		printf("%% Invalid 802.1q VID : %d\n", vid);
//		return -1;
//	}
//
//	reg = BCM53115SREG_PORT_CONTROL3;
//	reg += 0x10 * port;
//
//	vid_l = (__u8) (vid | 0xff);
//	vid_u = (__u8) ((vid >> 8) | BCM53115SREG_DEFAULT_VID_UPPER_MSK);
//
//	if (_bcm53115s_reg_read(reg, &data, sizeof(data)))
//		return -1;
//
//	/* Set upper part of VID */
//	vid_u |= (data & (~BCM53115SREG_DEFAULT_VID_UPPER_MSK));
//
//	if (_bcm53115s_reg_write(reg, &vid_u, sizeof(vid_u)) < 0)
//		return -1;
//
//	if (_bcm53115s_reg_write(reg+1, &vid_l, sizeof(vid_l)) < 0)
//		return -1;
//
//	return 0;
//}
//
///**
// * librouter_bcm53115s_get_default_vid	Get port's default 802.1q VID
// *
// * @param port
// * @return Default VID if success, -1 if error
// */
//int librouter_bcm53115s_get_default_vid(int port)
//{
//	__u8 vid_l, vid_u;
//	__u8 reg, data;
//
//	if (port < 0 || port > 3) {
//		printf("%% No such port : %d\n", port);
//		return -1;
//	}
//
//	reg = BCM53115SREG_PORT_CONTROL3;
//	reg += 0x10 * port;
//
//	if (_bcm53115s_reg_read(reg, &data, sizeof(data)))
//		return -1;
//
//	vid_u = data & BCM53115SREG_DEFAULT_VID_UPPER_MSK;
//
//	if (_bcm53115s_reg_read(++reg, &data, sizeof(data)))
//		return -1;
//
//	vid_l = data;
//
//	return ((int) (vid_u << 8 | vid_l));
//}
//
///**
// * librouter_bcm53115s_set_default_cos	Set port default 802.1p CoS
// *
// * @param port
// * @param cos
// * @return 0 if success, -1 if error
// */
//int librouter_bcm53115s_set_default_cos(int port, int cos)
//{
//	__u8 reg, data;
//
//	if (port < 0 || port > 3) {
//		printf("%% No such port : %d\n", port);
//		return -1;
//	}
//
//	if (cos > 7) {
//		printf("%% Invalid 802.1p CoS : %d\n", cos);
//		return -1;
//	}
//
//	reg = BCM53115SREG_PORT_CONTROL3;
//	reg += 0x10 * port;
//
//	if (_bcm53115s_reg_read(reg, &data, sizeof(data)))
//		return -1;
//
//	data &= ~BCM53115SREG_DEFAULT_COS_MSK;
//	data |= (cos << 5) & BCM53115SREG_DEFAULT_COS_MSK; /* Set bits [7-5] */
//
//	if (_bcm53115s_reg_write(reg, &data, sizeof(data)) < 0)
//		return -1;
//
//	return 0;
//}
//
///**
// * librouter_bcm53115s_get_default_cos	Get port's default 802.1q CoS
// *
// * @param port
// * @return Default CoS if success, -1 if error
// */
//int librouter_bcm53115s_get_default_cos(int port)
//{
//	__u8 reg, data;
//
//	if (port < 0 || port > 3) {
//		printf("%% No such port : %d\n", port);
//		return -1;
//	}
//
//	reg = BCM53115SREG_PORT_CONTROL3;
//	reg += 0x10 * port;
//
//	if (_bcm53115s_reg_read(reg, &data, sizeof(data)))
//		return -1;
//
//	return ((int) (data >> 5));
//}

/**
 * librouter_bcm53115s_set_8021p
 *
 * Enable or Disable 802.1p classification
 *
 * @param enable
 * @param port:	from 0 to 2
 * @return 0 on success, -1 otherwise
 */
int librouter_bcm53115s_set_8021p(int enable, int port)
{
	uint8_t page, reg;
	uint16_t data;

	if (port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	page = BCM53115S_QOS_PAGE;
	reg = BCM53115S_QOS_8021P_ENABLE_REG;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	if (enable)
		data |= 1 << port;
	else
		data &= ~(1 << port);

	if (_bcm53115s_reg_write(page, reg, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_bcm53115s_get_8021p
 *
 * Get if 802.1p classification is enabled or disabled
 *
 * @param port: from 0 to 2
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_bcm53115s_get_8021p(int port)
{
	uint8_t page, reg;
	uint16_t data;

	if (port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	page = BCM53115S_QOS_PAGE;
	reg = BCM53115S_QOS_8021P_ENABLE_REG;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	return (data & (1 << port)) ? 1 : 0;
}

/**
 * librouter_bcm53115s_set_diffserv
 *
 * Enable or Disable Diff Service classification
 *
 * @param enable
 * @param port:	from 0 to 2
 * @return 0 on success, -1 otherwise
 */
int librouter_bcm53115s_set_diffserv(int enable, int port)
{
	uint8_t page, reg;
	uint16_t data;

	if (port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	page = BCM53115S_QOS_PAGE;
	reg = BCM53115S_QOS_DIFFSERV_ENABLE_REG;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	if (enable)
		data |= 1 << port;
	else
		data &= ~(1 << port);

	if (_bcm53115s_reg_write(page, reg, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_bcm53115s_get_diffserv
 *
 * Get if Diff Service classification is enabled or disabled
 *
 * @param port: from 0 to 2
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_bcm53115s_get_diffserv(int port)
{
	uint8_t page, reg;
	uint16_t data;

	if (port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	page = BCM53115S_QOS_PAGE;
	reg = BCM53115S_QOS_DIFFSERV_ENABLE_REG;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	return (data & (1 << port)) ? 1 : 0;
}
//
//
///**
// * librouter_bcm53115s_set_taginsert
// *
// * Enable or Disable 802.1p/q tag insertion on untagged packets
// *
// * @param enable
// * @param port:	from 0 to 2
// * @return 0 on success, -1 otherwise
// */
//int librouter_bcm53115s_set_taginsert(int enable, int port)
//{
//	__u8 reg, data;
//
//	if (port > 3) {
//		printf("%% No such port : %d\n", port);
//		return -1;
//	}
//
//	reg = BCM53115SREG_PORT_CONTROL;
//	reg += port * 0x10;
//
//	if (_bcm53115s_reg_read(reg, &data, sizeof(data)))
//		return -1;
//
//	if (enable)
//		data |= BCM53115SREG_ENABLE_TAGINSERT_MSK;
//	else
//		data &= ~BCM53115SREG_ENABLE_TAGINSERT_MSK;
//
//	if (_bcm53115s_reg_write(reg, &data, sizeof(data)))
//		return -1;
//
//	return 0;
//}
//
///**
// * librouter_bcm53115s_get_taginsert
// *
// * Get if 802.1p/q tag insertion on untagged packets is enabled or disabled
// *
// * @param port: from 0 to 2
// * @return 1 if enabled, 0 if disabled, -1 if error
// */
//int librouter_bcm53115s_get_taginsert(int port)
//{
//	__u8 reg, data;
//
//	if (port > 3) {
//		printf("%% No such port : %d\n", port);
//		return -1;
//	}
//
//	reg = BCM53115SREG_PORT_CONTROL;
//	reg += port * 0x10;
//
//	if (_bcm53115s_reg_read(reg, &data, sizeof(data)))
//		return -1;
//
//	return (data | BCM53115SREG_ENABLE_TAGINSERT_MSK) ? 1 : 0;
//}
//
///**
// * librouter_bcm53115s_set_txqsplit
// *
// * Enable or Disable TXQ Split into 4 queues
// *
// * @param enable
// * @param port:	from 0 to 2
// * @return 0 on success, -1 otherwise
// */
//int librouter_bcm53115s_set_txqsplit(int enable, int port)
//{
//	__u8 reg, data;
//
//	if (port > 3) {
//		printf("%% No such port : %d\n", port);
//		return -1;
//	}
//
//	reg = BCM53115SREG_PORT_CONTROL;
//	reg += port * 0x10;
//
//	if (_bcm53115s_reg_read(reg, &data, sizeof(data)))
//		return -1;
//
//	if (enable)
//		data |= BCM53115SREG_ENABLE_TXQSPLIT_MSK;
//	else
//		data &= ~BCM53115SREG_ENABLE_TXQSPLIT_MSK;
//
//	if (_bcm53115s_reg_write(reg, &data, sizeof(data)))
//		return -1;
//
//	return 0;
//}
//
///**
// * librouter_bcm53115s_get_txqsplit
// *
// * Get if TXQ split configuration
// *
// * @param port: from 0 to 2
// * @return 1 if enabled, 0 if disabled, -1 if error
// */
//int librouter_bcm53115s_get_txqsplit(int port)
//{
//	__u8 reg, data;
//
//	if (port > 3) {
//		printf("%% No such port : %d\n", port);
//		return -1;
//	}
//
//	reg = BCM53115SREG_PORT_CONTROL;
//	reg += port * 0x10;
//
//	if (_bcm53115s_reg_read(reg, &data, sizeof(data)))
//		return -1;
//
//	return (data | BCM53115SREG_ENABLE_TXQSPLIT_MSK) ? 1 : 0;
//}
//
/**
 * librouter_bcm53115s_set_dscp_prio
 *
 * Set the packet priority based on DSCP value
 *
 * @param dscp
 * @param prio
 * @return 0 if success, -1 if error
 */
int librouter_bcm53115s_set_dscp_prio(int dscp, int prio)
{
	uint8_t page, reg, offset;
	uint32_t data, mask;

	if (dscp < 0 || dscp > 63) {
		printf("%% Invalid DSCP value : %d\n", dscp);
		return -1;
	}

	if (prio < 0 || prio > 7) {
		printf("%% Invalid priority : %d\n", prio);
		return -1;
	}

	page = BCM53115S_QOS_PAGE;
	reg = BCM53115S_QOS_DIFFSERV_PRIOMAP_REG;
	reg += dscp/16;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	/* See 5311S-DS Page 255 for DiffServ mapping:
	 * - 3 priority bits per DSCP priority
	 * - 4 registers, 48bit each
	 */

	offset = (dscp % 16) * 3;
	mask = 0x7 << offset;

	data &= ~mask; /* Clear current config */
	data |= (prio << offset);

	if (_bcm53115s_reg_write(page, reg, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_bcm53115s_get_dscp_prio
 *
 * Get the packet priority based on DSCP value
 *
 * @param dscp
 * @return priority value if success, -1 if error
 */
int librouter_bcm53115s_get_dscp_prio(int dscp)
{
	uint8_t page, reg, offset;
	uint32_t data, mask;
	int prio;

	if (dscp < 0 || dscp > 63) {
		printf("%% Invalid DSCP value : %d\n", dscp);
		return -1;
	}

	page = BCM53115S_QOS_PAGE;
	reg = BCM53115S_QOS_DIFFSERV_PRIOMAP_REG;
	reg += dscp/16;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	prio = (int) ((data >> (dscp % 16) * 3) & 0x7);

	return prio;
}

///**
// * librouter_bcm53115s_set_cos_prio
// *
// * Set the packet priority based on DSCP value
// *
// * @param cos
// * @param prio
// * @return 0 if success, -1 if error
// */
//int librouter_bcm53115s_set_cos_prio(int cos, int prio)
//{
//	__u8 reg, data, mask;
//	int offset;
//
//	if (cos < 0 || cos > 7) {
//		printf("%% Invalid CoS value : %d\n", cos);
//		return -1;
//	}
//
//	if (prio < 0 || prio > 3) {
//		printf("%% Invalid priority : %d\n", prio);
//		return -1;
//	}
//
//	reg = BCM53115SREG_GLOBAL_CONTROL10;
//	reg += cos/4;
//
//	if (_bcm53115s_reg_read(reg, &data, sizeof(data)))
//		return -1;
//
//	/*
//	 * See BCM53115S Datasheet Page 53  for the register description
//	 * (CoS mod 4) will give the offset:
//	 * 	- mod 0: bits [1-0]
//	 * 	- mod 1: bits [3-2]
//	 * 	- mod 2: bits [5-4]
//	 * 	- mod 3: bits [7-6]
//	 */
//	offset = (cos % 4) * 2;
//	mask = 0x3 << offset;
//	data &= ~mask; /* Clear current config */
//	data |= (prio << offset);
//
//	if (_bcm53115s_reg_write(reg, &data, sizeof(data)))
//		return -1;
//
//	return 0;
//}
//
///**
// * librouter_bcm53115s_get_dscp_prio
// *
// * Get the packet priority based on CoS value
// *
// * @param cos
// * @return priority value if success, -1 if error
// */
//int librouter_bcm53115s_get_cos_prio(int cos)
//{
//	__u8 reg, data;
//	int prio;
//
//	if (cos < 0 || cos > 7) {
//		printf("%% Invalid DSCP value : %d\n", cos);
//		return -1;
//	}
//
//	reg = BCM53115SREG_GLOBAL_CONTROL10;
//	reg += cos/4;
//
//	if (_bcm53115s_reg_read(reg, &data, sizeof(data)))
//		return -1;
//
//	prio = (int) ((data >> (cos % 4) * 2) & 0x3);
//
//	return prio;
//}
//
/***********************************************/
/********* VLAN table manipulation *************/
/***********************************************/

/**
 * _get_vlan_table	Fetch VLAN table from switch
 *
 * When successful, data is written to the structure pointed by t
 *
 * @param table
 * @param t
 * @return 0 if success, -1 if failure
 */
//static int _get_vlan_table(unsigned int entry, struct vlan_table_t *t)
//{
//	__u8 data;
//
//	if (entry > 15) {
//		printf("%% Invalid VLAN table : %d\n", entry);
//		return -1;
//	}
//
//	if (t == NULL) {
//		printf("%% NULL table pointer\n");
//		return -1;
//	}
//
//	data = BCM53115S3REG_READ_OPERATION | BCM53115S3REG_VLAN_TABLE_SELECT;
//	if (_bcm53115s_reg_write(BCM53115S3REG_INDIRECT_ACCESS_CONTROL0, &data, sizeof(data)))
//		return -1;
//
//	data = (__u8) entry;
//	if (_bcm53115s_reg_write(BCM53115S3REG_INDIRECT_ACCESS_CONTROL1, &data, sizeof(data)))
//		return -1;
//
//
//	if (_bcm53115s_reg_read(BCM53115S3REG_INDIRECT_DATA0, &data, sizeof(data)))
//		return -1;
//	t->vid = data;
//
//	if (_bcm53115s_reg_read(BCM53115S3REG_INDIRECT_DATA1, &data, sizeof(data)))
//		return -1;
//	t->vid |= (data & 0x0F) << 8;
//	t->fid = (data & 0xF0) >> 4;
//
//	if (_bcm53115s_reg_read(BCM53115S3REG_INDIRECT_DATA2, &data, sizeof(data)))
//		return -1;
//	t->membership = data & 0x07;
//	t->valid = (data & 0x08) >> 3;
//
//	return 0;
//}
//
/**
 * _set_vlan_table	Configure a VLAN table in the switch
 *
 * Configuration is present in the structure pointed by t
 *
 * @param table
 * @param t
 * @return 0 if success, -1 if failure
 */
//static int _set_vlan_table(unsigned int table, struct vlan_table_t *t)
//{
//	__u8 data;
//
//	if (table > 15) {
//		printf("%% Invalid VLAN table : %d\n", table);
//		return -1;
//	}
//
//	if (t == NULL) {
//		printf("%% NULL table pointer\n");
//		return -1;
//	}
//
//	data = t->vid;
//	if (_bcm53115s_reg_write(BCM53115S3REG_INDIRECT_DATA0, &data, sizeof(data)))
//		return -1;
//
//	data = t->vid >> 8;
//	data |= t->fid << 4;
//	if (_bcm53115s_reg_write(BCM53115S3REG_INDIRECT_DATA1, &data, sizeof(data)))
//		return -1;
//
//	data = t->membership;
//	data |= t->valid << 3;
//	if (_bcm53115s_reg_write(BCM53115S3REG_INDIRECT_DATA2, &data, sizeof(data)))
//		return -1;
//
//	data = BCM53115S3REG_WRITE_OPERATION | BCM53115S3REG_VLAN_TABLE_SELECT;
//	if (_bcm53115s_reg_write(BCM53115S3REG_INDIRECT_ACCESS_CONTROL0, &data, sizeof(data)))
//		return -1;
//
//	data = (__u8) table;
//	if (_bcm53115s_reg_write(BCM53115S3REG_INDIRECT_ACCESS_CONTROL1, &data, sizeof(data)))
//	return -1;
//
//	return 0;
//}

/**
 * _init_vlan_table
 *
 * Make all VLAN entries invalid
 *
 * @return 0 if success, -1 if failure
 */
//static int _init_vlan_table(void)
//{
//	int i;
//	struct vlan_table_t vlan;
//
//	vlan.valid = 0;
//	vlan.vid = 0;
//
//	/* Search for the same VID in a existing entry */
//	for (i = 0; i < BCM53115S3_NUM_VLAN_TABLES; i++)
//		_set_vlan_table(i, &vlan);
//
//	return 0;
//}

///**
// * librouter_bcm53115s_add_table
// *
// * Create/Modify a VLAN in the switch
// *
// * @param vconfig
// * @return 0 if success, -1 if error
// */
//int librouter_bcm53115s_add_table(struct vlan_config_t *vconfig)
//{
//	struct vlan_table_t new_t, exist_t;
//	int active = 0, i;
//
//	if (vconfig == NULL) {
//		printf("%% Invalid VLAN config\n");
//		return -1;
//	}
//
//	/* Initiate table data */
//	new_t.valid = 1;
//	new_t.membership = vconfig->membership;
//	new_t.vid = vconfig->vid;
//
//	/* Search for the same VID in a existing entry */
//	for (i = 0; i < BCM53115S3_NUM_VLAN_TABLES; i++) {
//		_get_vlan_table(i, &exist_t);
//
//		if (!exist_t.valid)
//			continue;
//
//		if (new_t.vid == exist_t.vid) {
//			/* Found the same VID, just update this entry */
//			new_t.fid = i;
//			return _set_vlan_table(i, &new_t);
//		}
//
//		active++;
//	}
//
//	/* No more free entries ? */
//	if (active == BCM53115S3_NUM_VLAN_TABLES) {
//		printf("%% Already reached maximum number of entries\n");
//		return -1;
//	}
//
//	/* The same VID was not found, just add in the first entry available */
//	for (i = 0; i < BCM53115S3_NUM_VLAN_TABLES; i++) {
//		if (_get_vlan_table(i, &exist_t) < 0)
//			return -1;
//
//		if (exist_t.valid)
//			continue;
//
//		new_t.fid = i;
//		return _set_vlan_table(i, &new_t);
//	}
//
//	/* Should not be reached */
//	return -1;
//}
//
///**
// * librouter_bcm53115s_del_table
// *
// * Delete a table entry for a certain VID
// *
// * @param vconfig
// * @return 0 if success, -1 if error
// */
//int librouter_bcm53115s_del_table(struct vlan_config_t *vconfig)
//{
//	struct vlan_table_t t;
//	int i, vid;
//
//	if (vconfig == NULL) {
//		printf("%% Invalid VLAN config\n");
//		return -1;
//	}
//
//	vid = vconfig->vid;
//
//	/* Search for an existing entry */
//	for (i = 0; i < BCM53115S3_NUM_VLAN_TABLES; i++) {
//		if (_get_vlan_table(i, &t) < 0)
//			return -1;
//
//		if (!t.valid)
//			continue;
//
//		if (t.vid == vid) {
//			t.valid = 0; /* Make this entry invalid */
//			return _set_vlan_table(i, &t);
//		}
//	}
//
//	return 0;
//}
//
///**
// * librouter_bcm53115s_get_table
// *
// * Get a table entry and fill the data in vconfig
// *
// * @param entry
// * @param vconfig
// * @return 0 if success, -1 if error
// */
//int librouter_bcm53115s_get_table(int entry, struct vlan_table_t *v)
//{
//	if (v == NULL) {
//		printf("%% Invalid VLAN config\n");
//		return -1;
//	}
//
//	if (_get_vlan_table(entry, v) < 0)
//		return -1;
//
//	bcm53115s_dbg("Getting entry %d\n", entry);
//	bcm53115s_dbg(" -- > valid : %d\n", v->valid);
//	bcm53115s_dbg(" -- > membership : %d\n", v->membership);
//	bcm53115s_dbg(" -- > vid : %d\n", v->vid);
//	bcm53115s_dbg(" -- > fid : %d\n", v->fid);
//
//	return 0;
//}



/*********************************************/
/******* Initialization functions ************/
/*********************************************/

/**
 * librouter_bcm53115s_probe	Probe for the BCM53115S Chip
 *
 * Read ID registers to determine if chip is present
 *
 * @return 1 if chip was detected, 0 if not, -1 if error
 */
int librouter_bcm53115s_probe(void)
{
	uint32_t id = 0;

	if (_bcm53115s_reg_read(0x02, 0x30, &id, 4) < 0)
		return -1;

	if (id == BCM53115S_ID)
		return 1;

	return 0;
}

/**
 * librouter_bcm53115s_set_default_config
 *
 * Configure switch to system default
 *
 * @return 0 if success, -1 if error
 */
int librouter_bcm53115s_set_default_config(void)
{
	if (librouter_bcm53115s_probe()) {
		/* Storm protect starts with Broadcasts only */
		_set_storm_protect_defaults();
	}
	//	_init_vlan_table();

	return 0;
}

#endif /* OPTION_MANAGED_SWITCH */
