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

#include <libk/string.h>
#include <sys/device.h>
#include <vm/heap.h>

SpecDevice *device_table;
size_t device_table_size;

void
devices_init (void)
{
  device_table = kzalloc (sizeof (SpecDevice) * DEVICE_TABLE_SIZE);
  device_table_size = DEVICE_TABLE_SIZE;
}

SpecDevice *
device_register (dev_t major, u8 type, const char *name, void *data)
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
      dev->sd_data = data;
      return dev;
    }
  return NULL;
}
