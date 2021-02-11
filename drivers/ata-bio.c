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

#include <libk/compile.h>
#include <sys/ata.h>
#include <vm/heap.h>
#include <errno.h>
#include <string.h>

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

int
ata_device_read (SpecDevice *dev, void *buffer, size_t len, off_t offset)
{
  size_t start_diff;
  size_t end_diff;
  off_t start_lba;
  off_t mid_lba;
  off_t end_lba;
  off_t part_offset;
  size_t sectors;
  void *temp = NULL;
  int ret;

  if (buffer == NULL)
    return -EINVAL;
  if (len == 0)
    return 0;
  start_lba = offset / ATA_SECTSIZE;
  mid_lba = start_lba + (offset % ATA_SECTSIZE != 0);
  end_lba = (offset + len) / ATA_SECTSIZE;
  sectors = end_lba - mid_lba;
  start_diff = mid_lba * ATA_SECTSIZE - offset;
  end_diff = offset + len - end_lba * ATA_SECTSIZE;

  /* Calculate LBA offset for MBR partition devices */
  if (dev->sd_minor != 0)
    part_offset = (uint32_t) dev->sd_private;
  else
    part_offset = 0;

  ret = ata_read_sectors (dev->sd_major - 1, sectors, mid_lba + part_offset,
			  buffer + start_diff);
  if (ret != 0)
    return ret;

  /* Read unaligned starting bytes */
  if (start_diff != 0)
    {
      temp = kmalloc (ATA_SECTSIZE);
      if (unlikely (temp == NULL))
	return -ENOMEM;
      ret = ata_read_sectors (dev->sd_major - 1, 1, start_lba + part_offset,
			      temp);
      if (ret != 0)
	return ret;
      memcpy (buffer, temp + ATA_SECTSIZE - start_diff, start_diff);
    }

  /* Read unaligned ending bytes */
  if (end_diff != 0)
    {
      if (temp == NULL)
	{
	  temp = kmalloc (ATA_SECTSIZE);
	  if (unlikely (temp == NULL))
	    return -ENOMEM;
	}
      ret = ata_read_sectors (dev->sd_major - 1, 1, end_lba + part_offset,
			      temp);
      if (ret != 0)
	return ret;
      memcpy (buffer + start_diff + sectors * ATA_SECTSIZE, temp, end_diff);
    }

  kfree (temp);
  return 0;
}

int
ata_device_write (SpecDevice *dev, void *buffer, size_t len, off_t offset)
{
  size_t start_diff;
  size_t end_diff;
  off_t start_lba;
  off_t mid_lba;
  off_t end_lba;
  off_t part_offset;
  size_t sectors;
  void *temp = NULL;
  int ret;

  if (buffer == NULL)
    return -EINVAL;
  if (len == 0)
    return 0;
  start_lba = offset / ATA_SECTSIZE;
  mid_lba = start_lba + (offset % ATA_SECTSIZE != 0);
  end_lba = (offset + len) / ATA_SECTSIZE;
  sectors = end_lba - mid_lba;
  start_diff = mid_lba * ATA_SECTSIZE - offset;
  end_diff = offset + len - end_lba * ATA_SECTSIZE;

  /* Calculate LBA offset for MBR partition devices */
  if (dev->sd_minor != 0)
    part_offset = (uint32_t) dev->sd_private;
  else
    part_offset = 0;

  ret = ata_write_sectors (dev->sd_major - 1, sectors, mid_lba + part_offset,
			   buffer + start_diff);
  if (ret != 0)
    return ret;

  /* Write unaligned starting bytes */
  if (start_diff != 0)
    {
      temp = kmalloc (ATA_SECTSIZE);
      if (unlikely (temp == NULL))
	return -ENOMEM;
      ret = ata_read_sectors (dev->sd_major - 1, 1, start_lba + part_offset,
			      temp);
      if (ret != 0)
	return ret;
      memcpy (temp + ATA_SECTSIZE - start_diff, buffer, start_diff);
      ret = ata_write_sectors (dev->sd_major - 1, 1, start_lba + part_offset,
			       temp);
      if (ret != 0)
	return ret;
    }

  /* Write unaligned ending bytes */
  if (end_diff != 0)
    {
      if (temp == NULL)
	{
	  temp = kmalloc (ATA_SECTSIZE);
	  if (unlikely (temp == NULL))
	    return -ENOMEM;
	}
      ret = ata_read_sectors (dev->sd_major - 1, 1, end_lba + part_offset,
			      temp);
      if (ret != 0)
	return ret;
      memcpy (temp, buffer + start_diff + sectors * ATA_SECTSIZE, end_diff);
      ret = ata_write_sectors (dev->sd_major - 1, 1, end_lba + part_offset,
			       temp);
      if (ret != 0)
	return ret;
    }

  kfree (temp);
  return 0;
}
