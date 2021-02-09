/*************************************************************************
 * ata.h -- This file is part of OS/0.                                   *
 * Copyright (C) 2021 XNSC                                               *
 *                                                                       *
 * OS/0 is free software: you can redistribute it and/or modify          *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation, either version 3 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * OS/0 is distributed in the hope that it will be useful,               *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with OS/0. If not, see <https://www.gnu.org/licenses/>.         *
 *************************************************************************/

#ifndef _SYS_ATA_H
#define _SYS_ATA_H

#include <sys/cdefs.h>
#include <stdint.h>

#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DF   0x20
#define ATA_SR_DSC  0x10
#define ATA_SR_DRQ  0x08
#define ATA_SR_CORR 0x04
#define ATA_SR_IDX  0x02
#define ATA_SR_ERR  0x01

#define ATA_ER_BBK  0x80
#define ATA_ER_UNC  0x40
#define ATA_ER_MC   0x20
#define ATA_ER_IDNF 0x10
#define ATA_ER_MCR  0x08
#define ATA_ER_ABRT 0x04
#define ATA_ER_T0NF 0x02
#define ATA_ER_AMNF 0x01

#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_READ_PIO_EXT    0x24
#define ATA_CMD_READ_DMA        0xc8
#define ATA_CMD_READ_DMA_EXT    0x25
#define ATA_CMD_WRITE_PIO       0x30
#define ATA_CMD_WRITE_PIO_EXT   0x34
#define ATA_CMD_WRITE_DMA       0xca
#define ATA_CMD_WRITE_DMA_EXT   0x35
#define ATA_CMD_CACHE_FLUSH     0xe7
#define ATA_CMD_CACHE_FLUSH_EXT 0xea
#define ATA_CMD_PACKET          0xa0
#define ATA_CMD_IDENTIFY_PACKET 0xa1
#define ATA_CMD_IDENTIFY        0xec

#define ATAPI_CMD_READ  0xa8
#define ATAPI_CMD_EJECT 0x1b

#define ATA_IDENT_DEVICETYPE   0
#define ATA_IDENT_CYLINDERS    2
#define ATA_IDENT_HEADS        6
#define ATA_IDENT_SECTORS      12
#define ATA_IDENT_SERIAL       20
#define ATA_IDENT_MODEL        54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID   106
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200

#define IDE_ATA   0
#define IDE_ATAPI 1

#define ATA_MASTER 0
#define ATA_SLAVE  1

#define ATA_REG_DATA     0x00
#define ATA_REG_ERROR    0x01
#define ATA_REG_FEATURES 0x01
#define ATA_REG_SECCNT0  0x02
#define ATA_REG_LBA0     0x03
#define ATA_REG_LBA1     0x04
#define ATA_REG_LBA2     0x05
#define ATA_REG_HDDEVSEL 0x06
#define ATA_REG_COMMAND  0x07
#define ATA_REG_STATUS   0x07
#define ATA_REG_SECCNT1  0x08
#define ATA_REG_LBA3     0x09
#define ATA_REG_LBA4     0x0a
#define ATA_REG_LBA5     0x0b
#define ATA_REG_CONTROL  0x0c
#define ATA_REG_ALTSTAT  0x0c
#define ATA_REG_DEVADDR  0x0d

#define ATA_PRIMARY   0
#define ATA_SECONDARY 1

#define ATA_READ  0
#define ATA_WRITE 1

#define IDE_LBA48 2
#define IDE_LBA28 1
#define IDE_CHS   0

#define PATA_BAR0 0x1f0
#define PATA_BAR1 0x3f6
#define PATA_BAR2 0x170
#define PATA_BAR3 0x376
#define PATA_BAR4 0x000

#define ATA_SECTSIZE   512
#define ATAPI_SECTSIZE 2048

typedef struct
{
  uint16_t icr_base;
  uint16_t icr_ctrl;
  uint16_t icr_bmide;
  unsigned char icr_noint;
} IDEChannelRegisters;

typedef struct
{
  unsigned char id_reserved;
  unsigned char id_channel;
  unsigned char id_drive;
  uint16_t id_type;
  uint16_t id_sig;
  uint16_t id_cap;
  uint32_t id_cmdset;
  uint32_t id_size;
  char id_model[41];
} IDEDevice;

__BEGIN_DECLS

extern IDEChannelRegisters ata_channels[2];
extern IDEDevice ata_devices[4];
extern int ide_irq;

void ata_init (uint32_t bar0, uint32_t bar1, uint32_t bar2, uint32_t bar3,
	       uint32_t bar4);

unsigned char ata_read (unsigned char channel, unsigned char reg);
void ata_write (unsigned char channel, unsigned char reg, unsigned char data);
void ata_readbuf (unsigned char channel, unsigned char reg, void *buffer,
		  uint32_t quads);
int ata_poll (unsigned char channel, unsigned char chkerr);
int ata_perror (unsigned char drive, int err);
int ata_access (unsigned char op, unsigned char drive, uint32_t lba,
		unsigned char nsects, void *buffer);
void ata_await (void);

int atapi_read (unsigned char drive, uint32_t lba, unsigned char nsects,
		void *buffer);
int atapi_eject (unsigned char drive);

int ata_read_sectors (unsigned char drive, unsigned char nsects, uint32_t lba,
		      void *buffer);
int ata_write_sectors (unsigned char drive, unsigned char nsects, uint32_t lba,
		       void *buffer);

__END_DECLS

#endif
