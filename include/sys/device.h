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

#include <libk/types.h>
#include <sys/cdefs.h>

#define DEVICE_TABLE_SIZE 64

typedef struct
{
  dev_t sd_major;
  dev_t sd_minor;
  char sd_name[12];
  int (*sd_init) (void);
  int (*sd_destroy) (void);
} SpecDevice;

__BEGIN_DECLS

extern SpecDevice *device_table;

void devices_init (void);

__END_DECLS

#endif
