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

#include <sys/ata.h>
#include <sys/io.h>

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
