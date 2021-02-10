/*************************************************************************
 * ata-bio.c -- This file is part of OS/0.                               *
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
#include <sys/device.h>
#include <errno.h>

int
ata_read_sectors (unsigned char drive, unsigned char nsects, uint32_t lba,
		  void *buffer)
{
  int err;
  if (drive > 3 || !ata_devices[drive].id_reserved
      || lba + nsects > ata_devices[drive].id_size)
    return -EINVAL;

  if (ata_devices[drive].id_type == IDE_ATA)
    err = ata_access (ATA_READ, drive, lba, nsects, buffer);
  else if (ata_devices[drive].id_type == IDE_ATAPI)
    {
      int i;
      for (i = 0; i < nsects; i++)
	err = atapi_read (drive, lba + i, 1, buffer + i * ATAPI_SECTSIZE);
    }
  return ata_perror (drive, err);
}

int
ata_write_sectors (unsigned char drive, unsigned char nsects, uint32_t lba,
		   void *buffer)
{
  int err;
  if (drive > 3 || !ata_devices[drive].id_reserved
      || lba + nsects > ata_devices[drive].id_size)
    return -EINVAL;

  if (ata_devices[drive].id_type == IDE_ATA)
    err = ata_access (ATA_WRITE, drive, lba, nsects, buffer);
  else if (ata_devices[drive].id_type == IDE_ATAPI)
    err = -EINVAL;
  err = ata_perror (drive, err);
  return err;
}
