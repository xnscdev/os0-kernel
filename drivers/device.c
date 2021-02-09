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
#include <sys/device.h>
#include <vm/heap.h>

static unsigned char mbr_buffer[512];

SpecDevice *device_table;
size_t device_table_size;

static void
device_disk_init (int drive, SpecDevice *dev)
{
  char name[16];
  char *ptr;
  MBRPartInfo *mbr = (MBRPartInfo *) &mbr_buffer[0x1be];
  int i;
  int j = 0;

  /* Set up device name */
  memset (name, 0, 16);
  ptr = stpcpy (name, dev->sd_name);

  /* Read MBR entries */
  for (i = 0; i < 4; i++)
    {
      if (mbr[i].mpi_attr == 0)
	continue; /* Unused or invalid */
      *ptr = ++j;
      device_register (i, DEVICE_TYPE_BLOCK, name);
    }
}

void
devices_init (void)
{
  int i;
  int j = 0;
  device_table = kzalloc (sizeof (SpecDevice) * DEVICE_TABLE_SIZE);
  if (unlikely (device_table == NULL))
    panic ("Failed to initialize device table");
  device_table_size = DEVICE_TABLE_SIZE;

  /* Initialize ATA devices */
  for (i = 0; i < 4; i++)
    {
      SpecDevice *dev;
      char name[4];
      if (!ata_devices[i].id_reserved)
        continue;

      /* Read device MBR into buffer */
      if (ata_read_sectors (i, 1, 0, 0, mbr_buffer) != 0)
	{
	  printk ("ATA drive %d: failed to read MBR\n", i);
	  continue;
	}

      /* Set device name and register it */
      strcpy (name, "sdx");
      name[2] = 'a' + j++;
      dev = device_register (i, DEVICE_TYPE_BLOCK, name);

      /* Create more block devices for each partition */
      device_disk_init (i, dev);
    }
}

SpecDevice *
device_register (dev_t major, unsigned char type, const char *name)
{
  size_t i;
  for (i = 0; i < device_table_size; i++)
    {
      SpecDevice *dev = &device_table[i];
      if (dev->sd_type != 0)
	continue;
      dev->sd_major = major;
      dev->sd_minor = 0;
      dev->sd_type = type;
      strncpy (dev->sd_name, name, 15);
      dev->sd_name[15] = '\0';
      return dev;
    }
  return NULL;
}
