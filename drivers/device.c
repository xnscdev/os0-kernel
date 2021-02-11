/*************************************************************************
 * device.c -- This file is part of OS/0.                                *
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
#include <sys/sysmacros.h>
#include <vm/heap.h>

static unsigned char mbr_buffer[512];

SpecDevice device_table[DEVICE_TABLE_SIZE];

static void
device_disk_init (int drive, SpecDevice *dev)
{
  char name[16];
  char *ptr;
  MBRPartInfo *mbr;
  int i;
  int j = 0;

  /* Don't initialize an empty drive */
  if (ata_devices[drive].id_size == 0)
    return;

  /* Read the MBR */
  if (ata_read_sectors (drive, 1, 0, mbr_buffer) != 0)
    {
      printk ("ATA drive %d: failed to read MBR\n", drive);
      return;
    }
  mbr = (MBRPartInfo *) &mbr_buffer[0x1be];

  /* Set up device name */
  memset (name, 0, 16);
  ptr = stpcpy (name, dev->sd_name);

  /* Read MBR entries */
  for (i = 0; i < 4; i++)
    {
      SpecDevice *part;
      if (mbr[i].mpi_type == 0)
	continue; /* Unused or invalid */
      *ptr = '0' + ++j;
      part = device_register (drive + 1, i + 1, DEVICE_TYPE_BLOCK, name,
			      ata_device_read, ata_device_write);
      part->sd_private = (void *) mbr[i].mpi_lba;
    }
}

void
devices_init (void)
{
  char name[] = "sdx";
  int i;
  int j = 0;
  if (unlikely (device_table == NULL))
    panic ("Failed to initialize device table");

  /* Initialize ATA devices */
  for (i = 0; i < 4; i++)
    {
      SpecDevice *dev;
      if (!ata_devices[i].id_reserved)
        continue;

      /* Set device name and register it */
      name[2] = 'a' + j++;
      dev = device_register (i + 1, 0, DEVICE_TYPE_BLOCK, name, ata_device_read,
			     ata_device_write);

      /* Create more block devices for each partition */
      device_disk_init (i, dev);
    }
}

SpecDevice *
device_register (dev_t major, dev_t minor, unsigned char type, const char *name,
		 int (*read) (SpecDevice *, void *, size_t, off_t),
		 int (*write) (SpecDevice *, void *, size_t, off_t))
{
  size_t i;
  for (i = 0; i < DEVICE_TABLE_SIZE; i++)
    {
      SpecDevice *dev = &device_table[i];
      if (dev->sd_type != 0)
	continue;
      dev->sd_major = major;
      dev->sd_minor = minor;
      dev->sd_type = type;
      strncpy (dev->sd_name, name, 15);
      dev->sd_private = NULL;
      dev->sd_name[15] = '\0';
      dev->sd_read = read;
      dev->sd_write = write;
      return dev;
    }
  return NULL;
}

SpecDevice *
device_lookup (const char *name)
{
  int i;
  if (name == NULL)
    return NULL;
  for (i = 0; i < DEVICE_TABLE_SIZE; i++)
    {
      if (strcmp (device_table[i].sd_name, name) == 0)
	return &device_table[i];
    }
  return NULL;
}

SpecDevice *
device_lookup_devid (dev_t major, dev_t minor)
{
  int i;
  for (i = 0; i < DEVICE_TABLE_SIZE; i++)
    {
      if (device_table[i].sd_major == major
	  || device_table[i].sd_minor == minor)
	return &device_table[i];
    }
  return NULL;
}
