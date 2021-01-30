/*************************************************************************
 * multiboot.h -- This file is part of OS/0.                             *
 * Copyright (C) 2020 XNSC                                               *
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

#ifndef _SYS_MULTIBOOT_H
#define _SYS_MULTIBOOT_H

#ifndef _ASM
#include <stdint.h>
#endif

#define MULTIBOOT_MAGIC 0x1badb002

#define MULTIBOOT_FLAG_MEMORY   (1 << 0)
#define MULTIBOOT_FLAG_BOOTDEV  (1 << 1)
#define MULTIBOOT_FLAG_CMDLINE  (1 << 2)
#define MULTIBOOT_FLAG_MODULES  (1 << 3)
#define MULTIBOOT_FLAG_SYMBOLS  (1 << 4)
#define MULTIBOOT_FLAG_SECTIONS (1 << 5)
#define MULTIBOOT_FLAG_MEMMAP   (1 << 6)
#define MULTIBOOT_FLAG_DRIVES   (1 << 7)
#define MULTIBOOT_FLAG_CONFTBL  (1 << 8)
#define MULTIBOOT_FLAG_BLNAME   (1 << 9)
#define MULTIBOOT_FLAG_APMTBL   (1 << 10)
#define MULTIBOOT_FLAG_VBETBL   (1 << 11)
#define MULTIBOOT_FLAG_FBTBL    (1 << 12)

#ifndef _ASM

struct MultibootInfo
{
  uint32_t mi_flags;
  uint32_t mi_memlow;
  uint32_t mi_memhigh;
  uint32_t mi_bootdev;
  uint32_t mi_cmdline;
  uint32_t mi_modcnt;
  uint32_t mi_modaddr;
  uint32_t mi_syms[4];
  uint32_t mi_mmaplen;
  uint32_t mi_mmapaddr;
  uint32_t mi_drivelen;
  uint32_t mi_driveaddr;
  uint32_t mi_conftbl;
  uint32_t mi_loadname;
  uint32_t mi_apmtbl;
  uint32_t mi_vbectlinfo;
  uint32_t mi_vbemodinfo;
  uint16_t mi_vbemode;
  uint16_t mi_vbeifcseg;
  uint16_t mi_vbeifcoff;
  uint16_t mi_vbeifclen;
  uint32_t mi_fbaddr;
  uint32_t mi_fbpitch;
  uint32_t mi_fbwidth;
  uint32_t mi_fbheight;
  uint8_t mi_fbbpp;
  uint8_t mi_fbtype;
  uint8_t mi_fbcolinfo[6];
} __attribute__ ((packed));

#endif

#endif
