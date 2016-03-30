/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2012 Advanced Micro Devices, Inc.
 * Copyright (c) 2014 Sage Electronic Engineering, Inc.
 * Copyright (c) 2015-2016 PC Engines GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <arch/io.h>
#include "spi.h"
#include <pci.h>

#if defined (CONFIG_SB800_IMC_FWM)
#include "SBPLATFORM.h"
#include <vendorcode/amd/cimx/sb800/ECfan.h>

static int bus_claimed = 0;
#endif

static u32 spibar;

static void reset_internal_fifo_pointer(void)
{
	do {
		writeb(spibar + 2, readb(spibar + 2) | 0x10);
	} while (readb(spibar + 0xD) & 0x7);
}

static void execute_command(void)
{
	writeb(spibar + 2, readb(spibar + 2) | 1);

	while ((readb(spibar + 2) & 1) && (readb(spibar+3) & 0x80));
}

void spi_init(void)
{
	pcidev_t dev  = PCI_DEV(0,0x14,3);
	spibar = pci_read_config32(dev, 0xA0) & ~0x1F;
}

int spi_xfer(struct spi_slave *slave, const void *dout,
		unsigned int bitsout, void *din, unsigned int bitsin)
{
	/* First byte is cmd which can not being sent through FIFO. */
	u32 cmd = *(u32 *)dout++;
	u8 readoffby1;
	u32 readwrite;
	u8 bytesout, bytesin;
	u8 count;

	bitsout -= 8;
	bytesout = bitsout / 8;
	bytesin  = bitsin / 8;

	readoffby1 = bytesout ? 0 : 1;

	readwrite = (bytesin + readoffby1) << 4 | bytesout;
	writeb(spibar + 1, readwrite);
	writeb(spibar + 0, cmd);

	reset_internal_fifo_pointer();
	for (count = 0; count < bytesout; count++, dout++) {
		writeb(spibar + 0x0C, *(u32 *)dout);
	}

	reset_internal_fifo_pointer();
	execute_command();

	reset_internal_fifo_pointer();
	/* Skip the bytes we sent. */
	for (count = 0; count < bytesout; count++) {
		cmd = readb(spibar + 0x0C);
	}

	//reset_internal_fifo_pointer();
	for (count = 0; count < bytesin; count++, din++) {
		*(u8 *)din = readb(spibar + 0x0C);
	}
	return 0;
}

#if defined (CONFIG_SB800_IMC_FWM)

static void ImcSleep(void)
{
	u8	cmd_val = 0x96;		/* Kick off IMC Mailbox command 96 */
	u8	reg0_val = 0;		/* clear response register */
	u8	reg1_val = 0xB4;	/* request ownership flag */

	WriteECmsg (MSG_REG0, AccWidthUint8, &reg0_val);
	WriteECmsg (MSG_REG1, AccWidthUint8, &reg1_val);
	WriteECmsg (MSG_SYS_TO_IMC, AccWidthUint8, &cmd_val);

	WaitForEcLDN9MailboxCmdAck();
}


static void ImcWakeup(void)
{
	u8	cmd_val = 0x96;		/* Kick off IMC Mailbox command 96 */
	u8	reg0_val = 0;;		/* clear response register */
	u8	reg1_val = 0xB5;	/* release ownership flag */

	WriteECmsg (MSG_REG0, AccWidthUint8, &reg0_val);
	WriteECmsg (MSG_REG1, AccWidthUint8, &reg1_val);
	WriteECmsg (MSG_SYS_TO_IMC, AccWidthUint8, &cmd_val);

	WaitForEcLDN9MailboxCmdAck();
}
#endif

int spi_claim_bus(struct spi_slave *slave)
{
#if defined (CONFIG_SB800_IMC_FWM)

	if (slave->rw == SPI_WRITE_FLAG) {
		bus_claimed++;
		if (bus_claimed == 1)
			ImcSleep();
	}
#endif

	return 0;
}

void spi_release_bus(struct spi_slave *slave)
{
#if defined (CONFIG_SB800_IMC_FWM)

	if (slave->rw == SPI_WRITE_FLAG)  {
		bus_claimed--;
		if (bus_claimed <= 0) {
			bus_claimed = 0;
			ImcWakeup();
		}
	}
#endif
}

void spi_cs_activate(struct spi_slave *slave)
{
}

void spi_cs_deactivate(struct spi_slave *slave)
{
}

struct spi_slave *spi_setup_slave(unsigned int bus, unsigned int cs,
		unsigned int max_hz, unsigned int mode)
{
	struct spi_slave *slave = malloc(sizeof(*slave));

	if (!slave) {
		return NULL;
	}

	memset(slave, 0, sizeof(*slave));

	return slave;
}
