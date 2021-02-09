/*************************************************************************
 * atapi.c -- This file is part of OS/0.                                 *
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

#include <libk/libk.h>
#include <sys/ata.h>
#include <sys/io.h>
#include <sys/timer.h>

static unsigned char atapi_packet[12] = {0xa8};

int ide_irq;

int
atapi_read (unsigned char drive, uint32_t lba, unsigned char nsects,
	    void *buffer)
{
  uint32_t channel = ata_devices[drive].id_channel;
  uint32_t slavebit = ata_devices[drive].id_drive;
  uint32_t bus = ata_channels[channel].icr_base;
  uint32_t words = ATAPI_SECTSIZE >> 1;
  int err;
  int i;

  /* Enable IRQs */
  ide_irq = 0;
  ata_channels[channel].icr_noint = ide_irq;
  ata_write (channel, ATA_REG_CONTROL, ata_channels[channel].icr_noint);

  /* Fill SCSI packet */
  atapi_packet[0] = ATAPI_CMD_READ;
  atapi_packet[1] = 0;
  atapi_packet[2] = (lba >> 24) & 0xff;
  atapi_packet[3] = (lba >> 16) & 0xff;
  atapi_packet[4] = (lba >> 8) & 0xff;
  atapi_packet[5] = lba & 0xff;
  atapi_packet[6] = 0;
  atapi_packet[7] = 0;
  atapi_packet[8] = 0;
  atapi_packet[9] = nsects;
  atapi_packet[10] = 0;
  atapi_packet[11] = 0;

  ata_write (channel, ATA_REG_HDDEVSEL, slavebit << 4);
  for (i = 0; i < 4; i++)
    ata_read (channel, ATA_REG_ALTSTAT);

  /* Use PIO mode */
  ata_write (channel, ATA_REG_FEATURES, 0);

  ata_write (channel, ATA_REG_LBA1, (words * 2) & 0xff);
  ata_write (channel, ATA_REG_LBA2, (words * 2) >> 8);

  ata_write (channel, ATA_REG_COMMAND, ATA_CMD_PACKET);
  err = ata_poll (channel, 1);
  if (err != 0)
    return err;
  outsw (bus, atapi_packet, 6);

  for (i = 0; i < nsects; i++)
    {
      ata_await ();
      err = ata_poll (channel, 1);
      if (err != 0)
	return err;
      insw (bus, buffer, words);
      buffer += words * 2;
    }

  ata_await ();
  while (ata_read (channel, ATA_REG_STATUS) & (ATA_SR_BSY | ATA_SR_DRQ))
    ;
  return 0;
}

int
atapi_eject (unsigned char drive)
{
  uint32_t channel = ata_devices[drive].id_channel;
  uint32_t slavebit = ata_devices[drive].id_drive;
  uint32_t bus = ata_channels[channel].icr_base;
  int err;
  int i;

  if (drive > 3 || !ata_devices[drive].id_reserved)
    return 1;
  if (ata_devices[drive].id_type == IDE_ATA)
    return 20;

  /* Enable IRQs */
  ide_irq = 0;
  ata_channels[channel].icr_noint = ide_irq;
  ata_write (channel, ATA_REG_CONTROL, ata_channels[channel].icr_noint);

  /* Fill SCSI packet */
  atapi_packet[0] = ATAPI_CMD_EJECT;
  atapi_packet[1] = 0;
  atapi_packet[2] = 0;
  atapi_packet[3] = 0;
  atapi_packet[4] = 2;
  atapi_packet[5] = 0;
  atapi_packet[6] = 0;
  atapi_packet[7] = 0;
  atapi_packet[8] = 0;
  atapi_packet[9] = 0;
  atapi_packet[10] = 0;
  atapi_packet[11] = 0;

  ata_write (channel, ATA_REG_HDDEVSEL, slavebit << 4);
  for (i = 0; i < 4; i++)
    ata_read (channel, ATA_REG_ALTSTAT);

  ata_write (channel, ATA_REG_COMMAND, ATA_CMD_PACKET);
  err = ata_poll (channel, 1);
  if (err != 0)
    return err;

  outsw (bus, atapi_packet, 6);
  ata_await ();
  err = ata_poll (channel, 1);
  if (err == 3)
    err = 0;
  return ata_perror (drive, err);
}
