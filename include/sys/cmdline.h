/*************************************************************************
 * cmdline.h -- This file is part of OS/0.                               *
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

#ifndef _SYS_CMDLINE_H
#define _SYS_CMDLINE_H

#include <sys/cdefs.h>
#include <stdint.h>

typedef struct
{
  char b_root[24];
  char b_rootfstype[16];
  uint32_t b_verbose;
} BootOptions;

__BEGIN_DECLS

extern BootOptions boot_options;

__END_DECLS

#endif
