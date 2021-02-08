/*************************************************************************
 * ata.c -- This file is part of OS/0.                                   *
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

static u8 ata_buffer[2048];
static u8 ide_irq;
static u8 atapi_packet[12] = {0xa8};

static const char *ide_channel_names[] = {
  "primary",
  "secondary"
};

static const char *ide_drive_names[] = {
  "master",
  "slave"
};

static const char *ide_type_names[] = {
  "ATA",
  "ATAPI"
};

IDEChannelRegisters ata_channels[2];
IDEDevice ata_devices[4];

u8
ata_read (u8 channel, u8 reg)
{
  u8 result;
  if (reg > 0x07 && reg < 0x0c)
    ata_write (channel, ATA_REG_CONTROL,
	       0x80 | ata_channels[channel].icr_noint);
  if (reg < 0x08)
    result = inb (ata_channels[channel].icr_base + reg);
  else if (reg < 0x0c)
    result = inb (ata_channels[channel].icr_base + reg - 0x06);
  else if (reg < 0x0e)
    result = inb (ata_channels[channel].icr_ctrl + reg - 0x0a);
  else if (reg < 0x16)
    result = inb (ata_channels[channel].icr_bmide + reg - 0x0e);
  if (reg > 0x07 && reg < 0x0c)
    ata_write (channel, ATA_REG_CONTROL, ata_channels[channel].icr_noint);
  return result;
}

void
ata_write (u8 channel, u8 reg, u8 data)
{
  if (reg > 0x07 && reg < 0x0c)
    ata_write (channel, ATA_REG_CONTROL,
	       0x80 | ata_channels[channel].icr_noint);
  if (reg < 0x08)
    outb (data, ata_channels[channel].icr_base + reg);
  else if (reg < 0x0c)
    outb (data, ata_channels[channel].icr_base + reg - 0x06);
  else if (reg < 0x0e)
    outb (data, ata_channels[channel].icr_ctrl + reg - 0x0a);
  else if (reg < 0x16)
    outb (data, ata_channels[channel].icr_bmide + reg - 0x0e);
  if (reg > 0x07 && reg < 0x0c)
    ata_write (channel, ATA_REG_CONTROL, ata_channels[channel].icr_noint);
}

void
ata_readbuf (u8 channel, u8 reg, void *buffer, u32 quads)
{
  if (reg > 0x07 && reg < 0x0c)
    ata_write (channel, ATA_REG_CONTROL,
	       0x80 | ata_channels[channel].icr_noint);
  __asm__ volatile ("push %es; mov %ds, %ax; mov %ax, %es");
  if (reg < 0x08)
    insl (ata_channels[channel].icr_base + reg, buffer, quads);
  else if (reg < 0x0c)
    insl (ata_channels[channel].icr_base + reg - 0x06, buffer, quads);
  else if (reg < 0x0e)
    insl (ata_channels[channel].icr_ctrl + reg - 0x0a, buffer, quads);
  else if (reg < 0x16)
    insl (ata_channels[channel].icr_bmide + reg - 0x0e, buffer, quads);
  __asm__ volatile ("pop %es");
  if (reg > 0x07 && reg < 0x0c)
    ata_write (channel, ATA_REG_CONTROL, ata_channels[channel].icr_noint);
}

u8
ata_poll (u8 channel, u8 chkerr)
{
  int i;
  for (i = 0; i < 4; i++)
    ata_read (channel, ATA_REG_ALTSTAT);
  while (ata_read (channel, ATA_REG_STATUS) & ATA_SR_BSY)
    ;

  if (chkerr)
    {
      u8 state = ata_read (channel, ATA_REG_STATUS);
      if (state & ATA_SR_ERR)
	return 1;
      if (state & ATA_SR_DF)
	return 2;
      if ((state & ATA_SR_DRQ) == 0)
	return 3;
    }
  return 0;
}

u8
ata_perror (u8 drive, u8 err)
{
  u8 st;
  if (err == 0)
    return 0;

  printk ("Error in IDE %s %s [%s]:\n",
	  ide_channel_names[ata_devices[drive].id_channel],
	  ide_drive_names[ata_devices[drive].id_drive],
	  ata_devices[drive].id_model);
  switch (err)
    {
    case 1:
      printk ("* Device Fault\n");
      err = 19;
      break;
    case 2:
      st = ata_read (ata_devices[drive].id_channel, ATA_REG_ERROR);
      if (st & ATA_ER_AMNF)
	{
	  printk ("* No address mark found\n");
	  err = 7;
	}
      if (st & ATA_ER_T0NF)
	{
	  printk ("* No media or media error\n");
	  err = 3;
	}
      if (st & ATA_ER_ABRT)
	{
	  printk ("* Command aborted\n");
	  err = 20;
	}
      if (st & ATA_ER_MCR)
	{
	  printk ("* No media or media error\n");
	  err = 3;
	}
      if (st & ATA_ER_IDNF)
	{
	  printk ("* ID mark not found\n");
	  err = 21;
	}
      if (st & ATA_ER_MC)
	{
	  printk ("* No media or media error\n");
	  err = 3;
	}
      if (st & ATA_ER_UNC)
	{
	  printk ("* Uncorrectable data error\n");
	  err = 22;
	}
      if (st & ATA_ER_BBK)
	{
	  printk ("* Bad sectors\n");
	  err = 13;
	}
      break;
    case 3:
      printk ("* Reads nothing\n");
      err = 23;
      break;
    case 4:
      printk ("* Write-protected\n");
      err = 8;
      break;
    }
  return err;
}

void
ata_init (u32 bar0, u32 bar1, u32 bar2, u32 bar3, u32 bar4)
{
  int i;
  int j;
  int k;
  int count = 0;

  ata_channels[ATA_PRIMARY].icr_base = (bar0 & 0xfffffffc) + 0x1f0 * !bar0;
  ata_channels[ATA_PRIMARY].icr_ctrl = (bar1 & 0xfffffffc) + 0x3f6 * !bar1;
  ata_channels[ATA_SECONDARY].icr_base = (bar2 & 0xfffffffc) + 0x170 * !bar2;
  ata_channels[ATA_SECONDARY].icr_ctrl = (bar3 & 0xfffffffc) + 0x376 * !bar3;
  ata_channels[ATA_PRIMARY].icr_bmide = bar4 & 0xfffffffc;
  ata_channels[ATA_SECONDARY].icr_bmide = (bar4 & 0xfffffffc) + 8;

  ata_write (ATA_PRIMARY, ATA_REG_CONTROL, 2);
  ata_write (ATA_SECONDARY, ATA_REG_CONTROL, 2);

  /* Detect and assign IDE drives */
  for (i = 0; i < 2; i++)
    {
      for (j = 0; j < 2; j++)
	{
	  u8 err = 0;
	  u8 type = IDE_ATA;
	  u8 status;
	  ata_devices[count].id_reserved = 0;

	  ata_write (i, ATA_REG_HDDEVSEL, 0xa0 | j << 4);
	  msleep (1);

	  ata_write (i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
	  msleep (1);

	  if (ata_read (i, ATA_REG_STATUS) == 0)
	    continue;
	  while (1)
	    {
	      status = ata_read (i, ATA_REG_STATUS);
	      if (status & ATA_SR_ERR)
		{
		  err = 1;
		  break;
		}
	      if ((status & ATA_SR_BSY) == 0 && status & ATA_SR_DRQ)
		break;
	    }

	  if (err != 0)
	    {
	      u8 cl = ata_read (i, ATA_REG_LBA1);
	      u8 ch = ata_read (i, ATA_REG_LBA2);

	      if ((cl == 0x14 && ch == 0xeb) || (cl == 0x69 && ch == 0x96))
		type = IDE_ATAPI;
	      else
		continue;

	      ata_write (i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
	      msleep (1);
	    }

	  /* Fill device properties */
	  ata_readbuf (i, ATA_REG_DATA, ata_buffer, 128);
	  ata_devices[count].id_reserved = 1;
	  ata_devices[count].id_type = type;
	  ata_devices[count].id_channel = i;
	  ata_devices[count].id_drive = j;
	  ata_devices[count].id_sig =
	    *((u16 *) (ata_buffer + ATA_IDENT_DEVICETYPE));
	  ata_devices[count].id_cap =
	    *((u16 *) (ata_buffer + ATA_IDENT_CAPABILITIES));
	  ata_devices[count].id_cmdset =
	    *((u32 *) (ata_buffer + ATA_IDENT_COMMANDSETS));

	  if (ata_devices[count].id_cmdset & 1 << 26)
	    ata_devices[count].id_size =
	      *((u32 *) (ata_buffer + ATA_IDENT_MAX_LBA_EXT));
	  else
	    ata_devices[count].id_size =
	      *((u32 *) (ata_buffer + ATA_IDENT_MAX_LBA));

	  for (k = 0; k < 40; k += 2)
	    {
	      ata_devices[count].id_model[k] =
		ata_buffer[ATA_IDENT_MODEL + k + 1];
	      ata_devices[count].id_model[k + 1] =
		ata_buffer[ATA_IDENT_MODEL + k];
	    }
	  ata_devices[count].id_model[40] = '\0';

	  count++;
	}
    }

  for (i = 0; i < 4; i++)
    {
      if (ata_devices[i].id_reserved)
	printk ("Found %s drive [%s] (%lu bytes)\n",
		ide_type_names[ata_devices[i].id_type],
		ata_devices[i].id_model, ata_devices[i].id_size);
    }
}
