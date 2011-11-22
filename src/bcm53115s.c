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

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>

#include "options.h"

#ifdef OPTION_MANAGED_SWITCH

#include "bcm53115s.h"
#include "bcm53115s_etc.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static uint8_t mode;
static uint8_t bits = 8;
static uint32_t speed = 2000000;
static uint16_t delay;
const int timeout_spi_limit = 100;

/* type, port[NUMBER_OF_SWITCH_PORTS]*/
port_family_switch _switch_bcm_ports[] = {
	{ real_sw,  {0,1,2,3} },
	{ alias_sw, {1,2,3,4} },
	{ non_sw,   {0,0,0,0} }
};

/**
 * Função retorna porta switch "alias" utilizada no cish e web através da
 * porta switch real correspondente passada por parâmetro
 *
 * @param switch_port
 * @return alias_switch_port if ok, -1 if not
 */
int librouter_bcm53115s_get_aliasport_by_realport(int switch_port)
{
	int i;

	for (i=0; i < NUMBER_OF_SWITCH_PORTS; i++){
		if ( _switch_bcm_ports[real_sw].port[i] == switch_port ){
			return _switch_bcm_ports[alias_sw].port[i];
		}
	}
	return -1;
}

/**
 * Função retorna porta switch correspondente através da porta "alias"
 * utilizada no cish e web
 *
 * @param switch_port
 * @return real_switch_port if ok, -1 if not
 */
int librouter_bcm53115s_get_realport_by_aliasport(int switch_port)
{
	int i;

	for (i=0; i < NUMBER_OF_SWITCH_PORTS; i++){
		if ( _switch_bcm_ports[alias_sw].port[i] == switch_port ){
			return _switch_bcm_ports[real_sw].port[i];
		}
	}
	return -1;
}

/**
 * endian_swap_16bits
 *
 * Convert endian 16 bits value
 */
static uint16_t endian_swap_16bits(uint16_t * val)
{
	return ((*val << 8) | (*val >> 8)) & 0xffff;
}

/**
 * endian_swap_32bits
 *
 * Convert endian 32 bits value
 */
static uint32_t endian_swap_32bits(uint32_t * val)
{
	return ((((*val) & 0xff000000) >> 24) | (((*val) & 0x00ff0000) >> 8) | (((*val)
	                & 0x0000ff00) << 8) | (((*val) & 0x000000ff) << 24));
}

/**
 * clear_buff
 *
 * Clear 8 bits buffer
 */
static int clear_buff(uint8_t buff[], int size_of)
{
	memset(buff, 0, size_of);

	return 0;
}

/**
 * clear_tx_rx
 *
 * Clear tx & rx 8 bits buffers by the same size
 */
static int clear_tx_rx(uint8_t tx[], uint8_t rx[], int size_of_TxRx)
{
	clear_buff(tx, size_of_TxRx);
	clear_buff(rx, size_of_TxRx);

	return 0;
}

/**
 * printf_buffer_data
 *
 * Show 8 bits buffer content in hexa
 */
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

/**
 * printf_spi_buffers
 *
 * Show SPI tx & rx 8 bits buffers content in hexa
 */
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

/**
 * _bcm53115s_spi_mode
 *
 * Configure SPI mode for transaction with the BCM53115s
 */
static int _bcm53115s_spi_mode(int dev)
{
	uint8_t mode = 0;
	int ret = 0;

	ret = ioctl(dev, SPI_IOC_RD_MODE, &mode);
	if (ret < 0)
		goto end;

	mode |= SPI_CPOL;
	mode |= SPI_CPHA;

	ret = ioctl(dev, SPI_IOC_WR_MODE, &mode);

	end: if (ret < 0) {
		bcm53115s_dbg_syslog("Could not set SPI mode : %s\n", strerror(errno));
		return -1;
	} else
		return 0;
}

/**
 * _bcm53115s_spi_transfer
 *
 * Sends message (tx & rx SPI buffer) through SPI connection via IOCTL
 */
static int _bcm53115s_spi_transfer(uint8_t tx[], uint8_t rx[], int size_of_TxRx)
{
	int dev, i;

	struct spi_ioc_transfer tr = { .tx_buf = (unsigned long)tx, .rx_buf = (unsigned long)rx,
	                .len = size_of_TxRx, .delay_usecs = delay, .speed_hz = speed,
	                .bits_per_word = bits, };

	dev = open(BCM53115S_SPI_DEV, O_RDWR);

	if (dev < 0) {
		bcm53115s_dbg_syslog("error opening %s: %s\n", BCM53115S_SPI_DEV, strerror(errno));
		return -1;
	}

	if (_bcm53115s_spi_mode(dev) < 0) {
		bcm53115s_dbg_syslog("Could not read from device / can't send spi message : %s\n", strerror(errno));
		close(dev);
		return -1;
	}

	if (ioctl(dev, SPI_IOC_MESSAGE(1), &tr) < 0) {
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

/**
 * _bcm53115s_spi_reg_read_raw
 *
 * Read (low level) bcm53115s (SPI) registers by page and offset, returning little endia value
 */
static int _bcm53115s_spi_reg_read_raw(uint8_t page, uint8_t offset, uint8_t *buf, int len)
{
	uint8_t tx[3];
	uint8_t rx[3];
	uint8_t tx_5_step[len + 2];
	uint8_t rx_5_step[len + 2];
	int i;
	int timeout = 0;

	clear_tx_rx(tx, rx, sizeof(tx));
	clear_tx_rx(tx_5_step, rx_5_step, sizeof(rx_5_step));

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
		if ((timeout++) == timeout_spi_limit)
			return -1;
		//usleep(100);
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
	do {
		tx[0] = CMD_SPI_BYTE_RD;
		tx[1] = ROBO_SPI_STATUS_PAGE;
		tx[2] = 0x00;
		_bcm53115s_spi_transfer(tx, rx, sizeof(tx));
		if ((timeout++) == timeout_spi_limit)
			return -1;
		//usleep(100);
	} while (((rx[2] >> ROBO_RACK_BIT) & 1) == 0); // wait RACK bit to 1

	clear_tx_rx(tx, rx, sizeof(tx));
	timeout = 0;

	/* 5. Issue a normal read command(0x60) to read the specific register's conternt
	 placed in the SPI data I/O register(0xF0) 	 */
	tx_5_step[0] = CMD_SPI_BYTE_RD;
	tx_5_step[1] = ROBO_SPI_DATA_IO_0_PAGE;
	if (_bcm53115s_spi_transfer(tx_5_step, rx_5_step, sizeof(tx_5_step)) < 0)
		return -1;

	/* 6. Copy returned info to buff */
	for (i = 0; i < len; i++)
		buf[i] = rx_5_step[2 + i];

	return 0;
}

/**
 * _bcm53115s_spi_reg_write_raw
 *
 * Write (low level) little endian values in bcm53115s (SPI) registers by page and offset
 */
static int _bcm53115s_spi_reg_write_raw(uint8_t page, uint8_t offset, uint8_t *buf, int len)
{
	uint8_t tx[3];
	uint8_t rx[3];
	uint8_t tx_3_step[len + 2];
	uint8_t rx_3_step[len + 2];
	int i;
	int timeout = 0;

	clear_tx_rx(tx, rx, sizeof(tx));
	clear_tx_rx(tx_3_step, rx_3_step, sizeof(tx_3_step));

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
		_bcm53115s_spi_transfer(tx, rx, sizeof(tx));
		if ((timeout++) == timeout_spi_limit)
			return -1;
		//usleep(100);
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

	/* 3. Issue a normal write command(0x61) and write the address of the accessed
	 register followed by the write content starting from a lower byte */
	tx_3_step[0] = CMD_SPI_BYTE_WR;
	tx_3_step[1] = offset;

	for (i = 0; i < len; i++)
		tx_3_step[i + 2] = buf[i];

	if (_bcm53115s_spi_transfer(tx_3_step, rx_3_step, sizeof(tx_3_step)) < 0)
		return -1;

	return 0;
}

/**
 * _bcm53115s_reg_read
 *
 * Read register value by page and offset, returning big endian value
 */
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

/**
 * _bcm53115s_reg_write
 *
 * Write value in register by page and offset
 */
static int _bcm53115s_reg_write(uint8_t page, uint8_t offset, void *data_buf, int len)
{
	void *data;

	if (len == sizeof(uint32_t)) {
		uint32_t data32 = endian_swap_32bits((uint32_t *) data_buf);
		data = (void *) &data32;
	} else if (len == sizeof(uint16_t)) {
		uint16_t data16 = endian_swap_16bits((uint16_t *) data_buf);
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
 * _set_tc_to_cos_map
 *
 * Set Traffic-Class to Transmit queues map default configuration
 */
static int _set_tc_to_cos_map_defaults(void)
{
	uint8_t reg, page;
	uint16_t data;

	page = BCM53115S_QOS_PAGE;
	reg = BCM53115S_TC_TO_COS_MAP_REG;

	/* Make the following map:
	 * - TC 0 and 1: 0
	 * - TC 2 and 3: 1
	 * - TC 4 and 5: 2
	 * - TC 6 and 7: 3
	 */
	data = 0xf000;
	data |= 0x0a00;
	data |= 0x0050;

	if (_bcm53115s_reg_write(page, reg, (void *) &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * _set_qos_defaults
 *
 * Set QOS default values on bcm53115s
 */
static int _set_qos_defaults(void)
{
	uint8_t reg, page;
	uint8_t data;

	page = BCM53115S_QOS_PAGE;
	reg = BCM53115S_QOS_GLOBAL_CTRL_REG;

	if (_bcm53115s_reg_read(page, reg, (void *) &data, sizeof(data)))
		return -1;

	data |= 0x80; /* Aggregation mode: IMP is a data interface */
	data &= ~0x40; /* Disable Port-based QoS */

	data &= ~0x0C;
	data |= 0x08; /* QoS Priority selection: Use DiffServ first, then 802.1p */

	if (_bcm53115s_reg_write(page, reg, (void *) &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * fetch_speed_from_data
 *
 * Retrieve speed 10,100,1000
 */
static int _fetch_speed_from_data(unsigned int speed)
{
	switch (speed) {
		case 0:
			return 10;
			break;
		case 1:
			return 100;
			break;
		case 2:
			return 1000;
			break;
		default:
			return -1;
			break;
	}
	return -1;
}

/**
 * _get_port_speed
 *
 * Retrieve speed from SPI data
 */
static int _get_port_speed(struct port_bcm_speed_status * port_data, int port)
{
	switch (port) {
		case 0:
			return _fetch_speed_from_data(port_data->p0);
			break;
		case 1:
			return _fetch_speed_from_data(port_data->p1);
			break;
		case 2:
			return _fetch_speed_from_data(port_data->p2);
			break;
		case 3:
			return _fetch_speed_from_data(port_data->p3);
			break;
		default:
			return -1;
			break;
	}
	return -1;
}

/**
 * librouter_bcm53115s_get_port_speed
 *
 * Get speed from port
 *
 * @param port: from 0 to 3
 * @return speed (10,100,1000), -1 if error
 */
int librouter_bcm53115s_get_port_speed(int port)
{
	uint8_t page, reg;
	struct port_bcm_speed_status port_data;

	if (port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	memset(&port_data, 0, sizeof(port_data));
	page = BCM53115S_STATUS_REG_PAGE;
	reg = BCM53115S_PORT_SPEED_REG;

	if (_bcm53115s_reg_read(page, reg, (uint32_t *) &port_data, sizeof(uint32_t)))
		return -1;

#if 0
	/*Show das variaveis do port speed*/
	printf("\n PORT SPEED -> port_data %x --- p0 is %x / p1 is %x / p2 is %x / p3 is %x \n\n ]", *((uint32_t *) &port_data), port_data.p0, port_data.p1, port_data.p2, port_data.p3);
#endif

	return _get_port_speed(&port_data, port);
}

/**
 * librouter_bcm53115s_get_port_state
 *
 * Get MII data from port
 *
 * @param port: from 0 to 3
 * @return data if enabled, -1 if error
 */
int librouter_bcm53115s_get_MII_port_data(int port)
{
	uint8_t page, reg;
	uint16_t data;

	if (port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	page = ROBO_PORT0_MII_PAGE;
	reg = ROBO_CTRL_PAGE;
	page += port;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	return data;
}

/**
 * librouter_bcm53115s_set_port_state
 *
 * Enable/Disable switch port
 *
 * @param enable
 * @param port:	from 0 to 3
 * @return 0 on success, -1 otherwise
 */
int librouter_bcm53115s_set_MII_port_enable(int enable, int port)
{
	uint8_t page, reg;
	uint16_t data;

	if (port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	page = ROBO_PORT0_MII_PAGE;
	reg = ROBO_CTRL_PAGE;
	page += port;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	if (enable){
		data &= ~BCM53115S_MII_PORT_POWER_UP;
		data |= BCM53115S_MII_PORT_AUTO_NEGOC_RESTART;
	}
	else
		data |= BCM53115S_MII_PORT_POWER_DOWN;

	if (_bcm53115s_reg_write(page, reg, &data, sizeof(data)))
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
 * @param port: from 0 to 3
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

/**
 * librouter_bcm53115s_set_storm_protect_rate
 *
 * Set Storm protect rate by each port
 *
 * @param rate
 * @param port: from 0 to 3
 * @return 0 if ok, -1 if not
 */
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
 * Get Storm protect rate by each port
 *
 * @param rate
 * @param port: from 0 to 3
 * @return 0 if ok, -1 if not
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
 * Enable or Disable Multicast Storm protection by each port
 *
 * @param enable
 * @param port: from 0 to 3
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
 * Get if Multicast Storm protection is enabled or disabled by each port
 *
 * @param port: from 0 to 3
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


//TODO
#ifdef NOT_IMPLEMENTED_YET
/**
 * librouter_bcm53115s_set_replace_null_vid
 *
 * Enable or Disable NULL VID replacement with port VID
 *
 * @param enable
 * @return 0 on success, -1 otherwise
 */
int librouter_bcm53115s_set_replace_null_vid(int enable)
{
	__u8 data;

	if (_bcm53115s_reg_read(BCM53115SREG_GLOBAL_CONTROL2, &data, sizeof(data)))
		return -1;

	if (enable)
		data |= BCM53115S_ENABLE_REPLACE_NULL_VID_MSK;
	else
		data &= ~BCM53115S_ENABLE_REPLACE_NULL_VID_MSK;

	if (_bcm53115s_reg_write(BCM53115SREG_GLOBAL_CONTROL4, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_bcm53115s_get_replace_null_vid
 *
 * Get NULL VID replacement configuration
 *
 * @param void
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_bcm53115s_get_replace_null_vid(void)
{
	__u8 data;

	if (_bcm53115s_reg_read(BCM53115SREG_GLOBAL_CONTROL4, &data, sizeof(data)))
		return -1;

	return ((data & BCM53115S_ENABLE_REPLACE_NULL_VID_MSK) ? 1 : 0);
}

/**
 * librouter_bcm53115s_set_taginsert
 *
 * Enable or Disable 802.1p/q tag insertion on untagged packets
 *
 * @param enable
 * @param port:	from 0 to 2
 * @return 0 on success, -1 otherwise
 */
int librouter_bcm53115s_set_taginsert(int enable, int port)
{
	__u8 reg, data;

	if (port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = BCM53115SREG_PORT_CONTROL;
	reg += port * 0x10;

	if (_bcm53115s_reg_read(reg, &data, sizeof(data)))
		return -1;

	if (enable)
		data |= BCM53115SREG_ENABLE_TAGINSERT_MSK;
	else
		data &= ~BCM53115SREG_ENABLE_TAGINSERT_MSK;

	if (_bcm53115s_reg_write(reg, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_bcm53115s_get_taginsert
 *
 * Get if 802.1p/q tag insertion on untagged packets is enabled or disabled
 *
 * @param port: from 0 to 2
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_bcm53115s_get_taginsert(int port)
{
	__u8 reg, data;

	if (port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = BCM53115SREG_PORT_CONTROL;
	reg += port * 0x10;

	if (_bcm53115s_reg_read(reg, &data, sizeof(data)))
		return -1;

	return (data | BCM53115SREG_ENABLE_TAGINSERT_MSK) ? 1 : 0;
}

#endif

/**
 * librouter_bcm53115s_set_8021q	Enable/disable 802.1q (VLAN)
 *
 * @param enable
 * @return 0 if success, -1 if error
 */
int librouter_bcm53115s_set_8021q(int enable)
{
	uint8_t data = 0;

	if (_bcm53115s_reg_read(ROBO_VLAN_PAGE, 0x00, &data, 1))
		return -1;

	if (enable)
		data |= BCM53115S_ENABLE_8021Q_MSK;
	else
		data &= ~BCM53115S_ENABLE_8021Q_MSK;

	if (_bcm53115s_reg_write(ROBO_VLAN_PAGE, 0x00, &data, sizeof(data)))
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
	uint8_t data;

	if (_bcm53115s_reg_read(ROBO_VLAN_PAGE, 0x00, &data, sizeof(data)))
		return -1;

	return ((data & BCM53115S_ENABLE_8021Q_MSK) ? 1 : 0);
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

	if (_bcm53115s_reg_read(BCM53115S_QOS_PAGE, BCM53115S_QOS_TXQ_CONTROL_REG, &data,
	                sizeof(data)))
		return -1;

	/* 53115S-DS Page 259: When these bits are cleared, WRR are enabled in all 4 queues */
	if (enable)
		data &= ~BCM53115S_QOS_TXQ_CONTROL_WRR_MSK;
	else
		data |= BCM53115S_QOS_TXQ_CONTROL_WRR_MSK; /* Strict priority */

	if (_bcm53115s_reg_write(BCM53115S_QOS_PAGE, BCM53115S_QOS_TXQ_CONTROL_REG, &data,
	                sizeof(data)))
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

	if (_bcm53115s_reg_read(BCM53115S_QOS_PAGE, BCM53115S_QOS_TXQ_CONTROL_REG, &data,
	                sizeof(data)))
		return -1;

	return ((data & BCM53115S_QOS_TXQ_CONTROL_WRR_MSK) ? 0 : 1);
}

/**
 * librouter_bcm53115s_set_default_vid	Set port default 802.1q VID
 *
 * @param port: from 0 to 3
 * @param vid
 * @return 0 if success, -1 if error
 */
int librouter_bcm53115s_set_default_vid(int port, int vid)
{
	uint8_t reg, page;
	uint16_t data;

	if (port < 0 || port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	if (vid > 4095) {
		printf("%% Invalid 802.1q VID : %d\n", vid);
		return -1;
	}

	page = BCM53115S_VLAN_PAGE;
	reg = BCM53115S_VLAN_TAG_REG;
	reg += 0x2 * port;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	data &= ~BCM53115S_VLAN_DEFAULT_VID_MSK; /* Clear old VID */
	data |= (vid & BCM53115S_VLAN_DEFAULT_VID_MSK);

	if (_bcm53115s_reg_write(page, reg, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_bcm53115s_get_default_vid	Get port's default 802.1q VID by each port
 *
 * @param port: from 0 to 3
 * @return Default VID if success, -1 if error
 */
int librouter_bcm53115s_get_default_vid(int port)
{
	uint8_t reg, page;
	uint16_t data;

	if (port < 0 || port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	page = BCM53115S_VLAN_PAGE;
	reg = BCM53115S_VLAN_TAG_REG;
	reg += 0x2 * port;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	return ((int) (data & BCM53115S_VLAN_DEFAULT_VID_MSK));
}

/**
 * librouter_bcm53115s_set_default_cos	Set port default 802.1p CoS by each port
 *
 * @param port: from 0 to 3
 * @param cos
 * @return 0 if success, -1 if error
 */
int librouter_bcm53115s_set_default_cos(int port, int cos)
{
	uint8_t reg, page;
	uint16_t data;

	if (port < 0 || port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	if (cos > 7) {
		printf("%% Invalid 802.1p CoS : %d\n", cos);
		return -1;
	}

	page = BCM53115S_VLAN_PAGE;
	reg = BCM53115S_VLAN_TAG_REG;
	reg += 0x2 * port;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)) < 0)
		return -1;

	data &= ~BCM53115S_VLAN_DEFAULT_COS_MSK;
	data |= (cos << 13) & BCM53115S_VLAN_DEFAULT_COS_MSK; /* Set bits [15-13] */

	if (_bcm53115s_reg_write(page, reg, &data, sizeof(data)) < 0)
		return -1;

	return 0;
}

/**
 * librouter_bcm53115s_get_default_cos	Get port's default 802.1q CoS by each port
 *
 * @param port: from 0 to 3
 * @return Default CoS if success, -1 if error
 */
int librouter_bcm53115s_get_default_cos(int port)
{
	uint8_t reg, page;
	uint16_t data;

	if (port < 0 || port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	page = BCM53115S_VLAN_PAGE;
	reg = BCM53115S_VLAN_TAG_REG;
	reg += 0x2 * port;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	return ((int) (data >> 13));
}

/**
 * librouter_bcm53115s_set_8021p
 *
 * Enable or Disable 802.1p classification by each port
 *
 * @param enable
 * @param port:	from 0 to 3
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
 * Get if 802.1p classification is enabled or disabled by each port
 *
 * @param port: from 0 to 3
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
 * Enable or Disable Diff Service classification by each port
 *
 * @param enable
 * @param port:	from 0 to 3
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
 * Get if Diff Service classification is enabled or disabled by each port
 *
 * @param port: from 0 to 3
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

/**
 * librouter_bcm53115s_set_drop_untagged
 *
 * Set if untagged packets will be dropped by each port
 *
 * @param enable
 * @param port: from 0 to 3
 * @return 0 if success, -1 if error
 */
int librouter_bcm53115s_set_drop_untagged(int enable, int port)
{
	uint8_t page, reg;
	uint16_t data, mask;

	if (port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	page = BCM53115S_VLAN_PAGE;
	reg = BCM53115S_VLAN_DROPUNTAG_REG;
	mask = 1 << port;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	if (enable)
		data |= mask;
	else
		data &= ~mask;

	if (_bcm53115s_reg_write(page, reg, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_bcm53115s_get_drop_untagged
 *
 * Get if untagged packets will be dropped by each port
 *
 * @param port: from 0 to 3
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_bcm53115s_get_drop_untagged(int port)
{
	uint8_t page, reg;
	uint16_t data, mask;

	if (port > 3) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	page = BCM53115S_VLAN_PAGE;
	reg = BCM53115S_VLAN_DROPUNTAG_REG;
	mask = 1 << port;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	return (data & mask) ? 1 : 0;
}

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
	reg += dscp / 16;

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
	reg += dscp / 16;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	prio = (int) ((data >> (dscp % 16) * 3) & 0x7);

	return prio;
}

/**
 * librouter_bcm53115s_set_cos_prio
 *
 * Set the packet priority based on DSCP value
 *
 * @param cos
 * @param prio
 * @return 0 if success, -1 if error
 */
int librouter_bcm53115s_set_cos_prio(int cos, int prio)
{
	uint8_t page, reg, offset;
	uint32_t data, mask;
	int i;

	if (cos < 0 || cos > 7) {
		printf("%% Invalid CoS value : %d\n", cos);
		return -1;
	}

	if (prio < 0 || prio > 7) {
		printf("%% Invalid priority : %d\n", prio);
		return -1;
	}

	page = BCM53115S_QOS_PAGE;

	/* Set the same CoS map to all ports */
	for (i = 0; i < 7; i++) {
		reg = BCM53115S_QOS_COS_PRIOMAP_REG + 0x4 * i;
		if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
			return -1;
		/*
		 * See BCM53115S Datasheet Page 254  for the register description
		 */
		offset = cos * 3;
		mask = 0x7 << offset;
		data &= ~mask; /* Clear current config */
		data |= (prio << offset);

		if (_bcm53115s_reg_write(page, reg, &data, sizeof(data)))
			return -1;
	}

	return 0;
}

/**
 * librouter_bcm53115s_get_dscp_prio
 *
 * Get the packet priority based on CoS value
 *
 * @param cos
 * @return priority value if success, -1 if error
 */
int librouter_bcm53115s_get_cos_prio(int cos)
{
	uint8_t page, reg, offset;
	uint32_t data, mask;
	int prio;

	if (cos < 0 || cos > 7) {
		printf("%% Invalid DSCP value : %d\n", cos);
		return -1;
	}

	page = BCM53115S_QOS_PAGE;
	reg = BCM53115S_QOS_COS_PRIOMAP_REG;

	if (_bcm53115s_reg_read(page, reg, &data, sizeof(data)))
		return -1;

	prio = (int) ((data >> (cos * 3)) & 0x7);

	return prio;
}

/***********************************************/
/********* VLAN table manipulation *************/
/***********************************************/

/**
 * _set_vlan_table_reg_addr_index
 *
 * Set Vlan Table Address index in register by table entry
 */
static int _set_vlan_table_reg_addr_index(int entry)
{
	uint16_t data = entry;

	if (_bcm53115s_reg_write(BCM53115SREG_PAGE_VLAN_TABLE_INDEX,
	                BCM53115SREG_OFFSET_VLAN_TABLE_INDEX, &data, sizeof(uint16_t)))
		return -1;
	return 0;
}

/**
 * _set_vlan_table_reg_Rd_Wr_Clr
 *
 * Set Vlan Table Read/Write/Clear Table register by operator (0-write; 1-read ; 2-clear table)
 */
static int _set_vlan_table_reg_Rd_Wr_Clr(int operator)
{
	uint8_t data = 0;

	if (_bcm53115s_reg_read(BCM53115SREG_PAGE_VLAN_TABLE_RD_WR_CL,
	                BCM53115SREG_OFFSET_VLAN_TABLE_RD_WR_CL, &data, sizeof(uint8_t)))
		return -1;

	data &= ~VLAN_WR_RD_MASK;

	switch (operator) {
//	case 0:	/*Write*/
//		break;
	case 1:/*Read*/
		data |= VLAN_RD_BIT;
		break;
	case 2:/*Clear Table*/
		data |= VLAN_CLR_BIT;
		break;
	}

	data |= (1 << VLAN_START_BIT);

	if (_bcm53115s_reg_write(BCM53115SREG_PAGE_VLAN_TABLE_RD_WR_CL,
	                BCM53115SREG_OFFSET_VLAN_TABLE_RD_WR_CL, &data, sizeof(uint8_t)))
		return -1;

	return 0;
}

/**
 * _verify_operation_done_vlan_table_reg_Rd_Wr_Clr
 *
 * Verify if Read/Write/Clear Table operation is done
 */
static int _verify_operation_done_vlan_table_reg_Rd_Wr_Clr(void)
{
	uint8_t data_rd_wr_ctrl = 1;

	_bcm53115s_reg_read(BCM53115SREG_PAGE_VLAN_TABLE_RD_WR_CL,
	                BCM53115SREG_OFFSET_VLAN_TABLE_RD_WR_CL, &data_rd_wr_ctrl, sizeof(uint8_t));

	if (((data_rd_wr_ctrl >> VLAN_START_BIT) & 1) == 0)
		return 1;
	else
		return 0;
}

/**
 * _get_vlan_table	Fetch VLAN table from switch
 *
 * When successful, data is written to the structure pointed by vlan_table
 *
 * @param table
 * @param vlan_table
 * @return 0 if success, -1 if failure
 */
static int _get_vlan_table_entry(unsigned int table, struct vlan_bcm_table_t *vlan_table)
{
	int i = 0;

	if (table > BCM53115S_NUM_VLAN_TABLES) {
		printf("%% Invalid VLAN table : %d\n", table);
		return -1;
	}

	if (vlan_table == NULL) {
		printf("%% NULL table pointer\n");
		return -1;
	}

	/* Set VLAN Table Address Index Register */
	if (_set_vlan_table_reg_addr_index(table) < 0)
		return -1;

	/* Set VLAN Table Read/Write/Clear Control Register */
	if (_set_vlan_table_reg_Rd_Wr_Clr(VLAN_READ_CMD) < 0)
		return -1;

	for (i = 0; i < timeout_spi_limit; i++) {
		_bcm53115s_reg_read(BCM53115SREG_PAGE_VLAN_TABLE_RD_WR_CL,
		                BCM53115SREG_OFFSET_VLAN_TABLE_ENTRY, (uint32_t *) vlan_table,
		                sizeof(uint32_t));
		/* Verify if Read command is completed */
		if (_verify_operation_done_vlan_table_reg_Rd_Wr_Clr())
			break;
	}

	if ((vlan_table == 0) && (i == timeout_spi_limit))
		return -1;

	return 0;
}

/**
 * _set_vlan_table	Configure a VLAN table in the switch
 *
 * Configuration is present in the structure pointed by vlan_table
 *
 * @param table
 * @param vlan_table
 * @return 0 if success, -1 if failure
 */
static int _set_vlan_table_entry(unsigned int table, struct vlan_bcm_table_t *vlan_table)
{
	int i = 0, ret = 0;

	if (table > BCM53115S_NUM_VLAN_TABLES) {
		printf("%% Invalid VLAN table : %d\n", table);
		return -1;
	}

	if (vlan_table == NULL) {
		printf("%% NULL table pointer\n");
		return -1;
	}

	/* Set VLAN Table Address Index Register */
	if (_set_vlan_table_reg_addr_index(table) < 0)
		return -1;

	/* Set VLAN Table Entry Register */
	if (_bcm53115s_reg_write(BCM53115SREG_PAGE_VLAN_TABLE_RD_WR_CL,
			BCM53115SREG_OFFSET_VLAN_TABLE_ENTRY, (uint32_t *) vlan_table,
			sizeof(uint32_t)) < 0)
		return -1;

	/* Set VLAN Table Read/Write/Clear Control Register */
	if (_set_vlan_table_reg_Rd_Wr_Clr(VLAN_WRITE_CMD) < 0)
		return -1;

	/* Verify if Write command is completed */
	for (i = 0; i < timeout_spi_limit; i++) {
		if (ret = _verify_operation_done_vlan_table_reg_Rd_Wr_Clr())
			break;
	}

	if ((ret == 0) && (i == timeout_spi_limit))
		return -1;

	return 0;
}

/**
 * _del_vlan_table_entry	Remove a VLAN table in the switch by table entry
 *
 * @param table
 * @return 0 if success, -1 if failure
 */
static int _del_vlan_table_entry(int table)
{
	int i = 0, ret = 0;
	struct vlan_bcm_table_t v_table;

	memset(&v_table, 0, sizeof(v_table));

	if (table > BCM53115S_NUM_VLAN_TABLES) {
		printf("%% Invalid VLAN table : %d\n", table);
		return -1;
	}

	/* Set VLAN Table Address Index Register */
	if (_set_vlan_table_reg_addr_index(table) < 0)
		return -1;

	/* Set VLAN Table Entry Register */
	if (_bcm53115s_reg_write(BCM53115SREG_PAGE_VLAN_TABLE_RD_WR_CL,
			BCM53115SREG_OFFSET_VLAN_TABLE_ENTRY, &v_table,
			sizeof(uint32_t)) < 0)
		return -1;

	/* Set VLAN Table Read/Write/Clear Control Register */
	if (_set_vlan_table_reg_Rd_Wr_Clr(VLAN_WRITE_CMD) < 0)
		return -1;

	/* Verify if Write command is completed */
	for (i = 0; i < timeout_spi_limit; i++) {
		if (ret = _verify_operation_done_vlan_table_reg_Rd_Wr_Clr())
			break;
	}

	if ((ret == 0) && (i == timeout_spi_limit))
		return -1;

	return 0;
}

/**
 * _erase_vlan_table	Flush the entire VLANs table
 *
 * @return 0 if success, -1 if failure
 */
static int _erase_vlan_table(void)
{
	int i = 0, ret = 0;

	/* Set VLAN Table Read/Write/Clear Control Register */
	if (_set_vlan_table_reg_Rd_Wr_Clr(VLAN_CLR_TABLE_CMD) < 0)
		return -1;

	/* Verify if Write command is completed */
	for (i = 0; i < timeout_spi_limit; i++) {
		if (ret = _verify_operation_done_vlan_table_reg_Rd_Wr_Clr())
			break;
	}

	if ((ret == 0) && (i == timeout_spi_limit))
		return -1;

	return 0;
}

/**
 * _init_vlan_table
 *
 * Make all VLAN entries invalid
 *
 * @return 0 if success, -1 if failure
 */
static int _init_vlan_table(void)
{
	if (_erase_vlan_table() < 0)
		return -1;

	return 0;
}

/**
 * _get_vlan_bcm_config_from_raw_struct
 *
 * Get Vlan_bcm_config_struct from the raw SPI bcm53115s struct
 *
 * @param table
 * @param vconfig
 * @return 0 if success, -1 if failure
 */
static int _get_vlan_bcm_config_from_raw_struct(int table, struct vlan_bcm_config_t *vconfig)
{
	struct vlan_bcm_table_t t_raw;

	memset(&t_raw, 0, sizeof(struct vlan_bcm_table_t));

	if (table > BCM53115S_NUM_VLAN_TABLES) {
		printf("%% Invalid VLAN table : %d\n", table);
		return -1;
	}

	if (_get_vlan_table_entry((unsigned int) table, &t_raw) < 0)
		return -1;

	vconfig->membership = t_raw.fwd_map_ports;
	vconfig->membership |= t_raw.fwd_map_cpu_port << VLAN_FWD_MAP_CPU_MII_BIT;
	vconfig->vid = table;

	return 0;
}

/**
 * _add_vlan_entry_file_control
 *
 * Add file control refered by table_entry Vlan
 *
 * @param table_entry
 * @return 0 if success, -1 if failure
 */
static int _add_vlan_entry_file_control(int table_entry)
{
	FILE *fd;
	char vlan_filename[40];

	snprintf(vlan_filename, 40, "%s%d",BCM53115S_VLAN_ENTRY_FILE_CONTROL,table_entry);

	if ((fd = fopen((const char *) vlan_filename, "w+")) < 0) {
		syslog(LOG_ERR, "Could not create vlan switch control file\n");
		return -1;
	}

	fclose(fd);

	return 0;
}

/**
 * _del_vlan_entry_file_control
 *
 * Remove file control refered by table_entry Vlan
 *
 * @param table_entry
 * @return 0 if success, -1 if failure
 */
static int _del_vlan_entry_file_control(int table_entry)
{
	char vlan_filename[40];

	snprintf(vlan_filename, 40, "%s%d",BCM53115S_VLAN_ENTRY_FILE_CONTROL,table_entry);

	unlink(vlan_filename);

	return 0;
}

/**
 * _erase_all_vlan_entry_file_control
 *
 * Flush all files control refered by table_entry Vlans
 *
 * @return 0 if success, -1 if failure
 */
static int _erase_all_vlan_entry_file_control(void)
{
	int i = 0;

	for (i=0; i < BCM53115S_NUM_VLAN_TABLES; i++){
		if (_del_vlan_entry_file_control(i) < 0)
			return -1;
	}

	return 0;
}

/**
 * _verify_vlan_entry_file_control
 *
 * Verify the existence of a file Vlan Control by table_entry
 *
 * @return 1 if exist, 0 if not
 */
static int _verify_vlan_entry_file_control(int table_entry)
{
	FILE *fd;
	char vlan_filename[40];

	snprintf(vlan_filename, 40, "%s%d",BCM53115S_VLAN_ENTRY_FILE_CONTROL,table_entry);

	if ((fd = fopen((const char *)vlan_filename, "r")) == NULL) {
		return 0;
	}

	fclose(fd);

	return 1;
}
/**
 * librouter_bcm53115s_add_table
 *
 * Create/Modify a VLAN in the switch
 *
 * @param vconfig
 * @return 0 if success, -1 if error
 */
int librouter_bcm53115s_add_table(struct vlan_bcm_config_t *vconfig)
{
	struct vlan_bcm_table_t vlan_table;
	int i = 0;

	memset(&vlan_table, 0, sizeof(vlan_table));

	if (vconfig == NULL) {
		printf("%% Invalid VLAN config\n");
		return -1;
	}

	if (vconfig->vid > BCM53115S_NUM_VLAN_TABLES)
		return -1;

	if (_get_vlan_table_entry(vconfig->vid, &vlan_table) < 0)
		return -1;

	/* Write data to table data */
	vlan_table.fwd_map_ports = vconfig->membership;
	vlan_table.fwd_map_cpu_port = (vconfig->membership >> VLAN_FWD_MAP_CPU_MII_BIT) & 1;

	if (_set_vlan_table_entry(vconfig->vid, &vlan_table) < 0)
		return -1;

	if (_add_vlan_entry_file_control(vconfig->vid) < 0)
		return -1;

	return 0;
}

/**
 * librouter_bcm53115s_del_table
 *
 * Delete a table entry for a certain VID
 *
 * @param vconfig
 * @return 0 if success, -1 if error
 */
int librouter_bcm53115s_del_table(struct vlan_bcm_config_t *vconfig)
{
	if (vconfig == NULL) {
		printf("%% Invalid VLAN config\n");
		return -1;
	}

	if (_del_vlan_table_entry(vconfig->vid) < 0)
		return -1;

	if (_del_vlan_entry_file_control(vconfig->vid) < 0)
		return -1;

	return 0;
}

/**
 * librouter_bcm53115s_get_table
 *
 * Get a table entry and fill the data in vconfig
 *
 * @param table_entry
 * @param vconfig
 * @return 0 if success, -1 if error
 */
int librouter_bcm53115s_get_table(int table_entry, struct vlan_bcm_config_t *vconfig)
{
	if (vconfig == NULL) {
		printf("%% Invalid VLAN config\n");
		return -1;
	}

	if (!_verify_vlan_entry_file_control(table_entry))
		return -1;

	if (_get_vlan_bcm_config_from_raw_struct(table_entry, vconfig) < 0)
		return -1;

	//bcm53115s_dbg_syslog("Vlan Vid / Entry %d\n", v->vid);
	//bcm53115s_dbg_syslog(" -- > membership : %d\n", v->membership);

	return 0;
}

/**
 * librouter_bcm53115s_erase_all_tables
 *
 * Flush all entries in Vlan Table
 *
 * @return 0 if success, -1 if error
 */
int librouter_bcm53115s_erase_all_tables(void)
{
	if (_erase_vlan_table() < 0)
		return -1;

	if (_erase_all_vlan_entry_file_control() < 0)
		return -1;

	return 0;
}

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
		_set_qos_defaults();
		_set_tc_to_cos_map_defaults();
		_init_vlan_table();
	}

	return 0;
}

/**
 * _dump_port_config
 *
 * Dump the port configuration
 *
 * @param *out
 * @param port
 * @return
 */
static void _dump_port_config(FILE *out, int port)
{
	int i = 0;
	unsigned char rate = 0;
	int port_alias = librouter_bcm53115s_get_aliasport_by_realport(port);

	fprintf(out, " switch-port %d\n", port_alias);

	if (librouter_bcm53115s_get_8021p(port))
		fprintf(out, "  802.1p\n");

	if (librouter_bcm53115s_get_diffserv(port))
		fprintf(out, "  diffserv\n");

	if (librouter_bcm53115s_get_broadcast_storm_protect(port))
		fprintf(out, "  storm-control\n");

	if (librouter_bcm53115s_get_multicast_storm_protect(port))
		fprintf(out, "  multicast-storm-protect\n");

	if (librouter_bcm53115s_get_storm_protect_rate(&rate, port) == 0)
		fprintf(out, "  storm-protect-rate %d\n", rate);

#if NOT_IMPLEMENTED_YET /* Not implemented on CISH yet */
	i = librouter_bcm53115s_get_default_vid(port);
	if (i)
	fprintf(out, "  vlan-default %d\n", i);
#endif
}

/**
 * librouter_bcm53115s_dump_config
 *
 * Dump switch bcm53115s configuration
 *
 * @param *out
 * @return
 */
int librouter_bcm53115s_dump_config(FILE *out)
{
	int i = 0;
	struct vlan_bcm_config_t v;

	memset(&v, 0, sizeof(struct vlan_bcm_config_t));

	/* Is device present ? */
	if (librouter_bcm53115s_probe() == 0)
		return 0;

	if (librouter_bcm53115s_get_8021q())
		fprintf(out, " switch-config 802.1q\n");

	for (i = 0; i < 8; i++) {
		int prio = librouter_bcm53115s_get_cos_prio(i);
		if (prio != i)
			fprintf(out, " switch-config cos-prio %d %d\n", i, prio);
	}

	for (i = 0; i < 64; i++) {
		int prio = librouter_bcm53115s_get_dscp_prio(i);
		if (prio)
			fprintf(out, " switch-config dscp-prio %d %d\n", i, prio);
	}

//TODO
#ifdef NOT_IMPLEMENTED_YET
	if (librouter_bcm53115s_get_replace_null_vid())
		fprintf(out, " switch-config replace-null-vid\n");
#endif

	if (librouter_bcm53115s_get_wrr())
			fprintf(out, " switch-config wrr\n");

	for (i = 0; i < BCM53115S_NUM_VLAN_TABLES; i++) {
		if (librouter_bcm53115s_get_table(i, &v) == 0){
			if (v.membership != 0)
				fprintf(
						out,
						" switch-config vlan %d %s%s%s%s%s\n",
						v.vid,
						(v.membership & BCM53115SREG_VLAN_MEMBERSHIP_PORT_INTERNAL_MSK) ? "pI " : "0 ",
						(v.membership & BCM53115SREG_VLAN_MEMBERSHIP_PORT1_MSK) ? "p1 " : "0 ",
						(v.membership & BCM53115SREG_VLAN_MEMBERSHIP_PORT2_MSK) ? "p2 " : "0 ",
						(v.membership & BCM53115SREG_VLAN_MEMBERSHIP_PORT3_MSK) ? "p3 " : "0 ",
						(v.membership & BCM53115SREG_VLAN_MEMBERSHIP_PORT4_MSK) ? "p4 " : "0 ");
			memset(&v, 0, sizeof(struct vlan_bcm_config_t));
		}
	}

	for (i = 0; i < 4; i++)
		_dump_port_config(out, i);
}

#endif /* OPTION_MANAGED_SWITCH */
