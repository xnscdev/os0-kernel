/*************************************************************************
 * device.h -- This file is part of OS/0.                                *
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

#ifndef _SYS_DEVICE_H
#define _SYS_DEVICE_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

#define DEVICE_TABLE_SIZE 64

#define DEVICE_TYPE_BLOCK 1
#define DEVICE_TYPE_CHAR  2

typedef struct
{
  dev_t sd_major;
  dev_t sd_minor;
  unsigned char sd_type;
  char sd_name[15];
} SpecDevice;

typedef struct
{
  unsigned char mpi_attr;
  unsigned char mpi_chs_start[3];
  unsigned char mpi_type;
  unsigned char mpi_chs_end[3];
  uint32_t mpi_lba;
  uint32_t mpi_sects;
} MBRPartInfo;

typedef union
{
  uint32_t dpi_type;
  uint32_t dpi_lba;
  uint32_t dpi_size;
  SpecDevice *dpi_dev;
} DiskPartInfo;

__BEGIN_DECLS

extern SpecDevice *device_table;
extern size_t device_table_size;

void devices_init (void);

SpecDevice *device_register (dev_t major, unsigned char type, const char *name);

__END_DECLS

#endif
