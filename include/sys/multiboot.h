/*************************************************************************
 * multiboot.h -- This file is part of OS/0.                             *
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

#ifndef _SYS_MULTIBOOT_H
#define _SYS_MULTIBOOT_H

#ifndef _ASM
#include <libk/types.h>
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

typedef struct
{
  u32 mi_flags;
  u32 mi_memlow;
  u32 mi_memhigh;
  u32 mi_bootdev;
  u32 mi_cmdline;
  u32 mi_modcnt;
  u32 mi_modaddr;
  u32 mi_syms[4];
  u32 mi_mmaplen;
  u32 mi_mmapaddr;
  u32 mi_drivelen;
  u32 mi_driveaddr;
  u32 mi_conftbl;
  u32 mi_loadname;
  u32 mi_apmtbl;
  u32 mi_vbectlinfo;
  u32 mi_vbemodinfo;
  u16 mi_vbemode;
  u16 mi_vbeifcseg;
  u16 mi_vbeifcoff;
  u16 mi_vbeifclen;
  u32 mi_fbaddr;
  u32 mi_fbpitch;
  u32 mi_fbwidth;
  u32 mi_fbheight;
  u8 mi_fbbpp;
  u8 mi_fbtype;
  u8 mi_fbcolinfo[6];
} __attribute__ ((packed)) MultibootInfo;

#endif

#endif
