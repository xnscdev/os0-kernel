/*************************************************************************
 * rtld.h -- This file is part of OS/0.                                  *
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

#ifndef _SYS_RTLD_H
#define _SYS_RTLD_H

#include <kconfig.h>

#include <libk/array.h>
#include <elf.h>
#include <stddef.h>

/* Macros for determining valid ELF header machine type */
#ifdef  ARCH_I386
#define MACHTYPE EM_386
#else
#error "No ELF machine type supported for architecture"
#endif

#define LD_SO_LOAD_ADDR 0x00008000

typedef struct
{
  char *st_table;
  size_t st_len;
} ELFStringTable;

typedef struct
{
  int dl_active;            /* Set if program uses dynamic linking */
  void *dl_loadbase;        /* Pointer to ELF header */
  char *dl_interp;          /* ELF interpreter path */
  void *dl_interpload;      /* Pointer to ELF header of interpreter */
  Elf32_Dyn *dl_dynamic;    /* Address of ELF .dynamic section */
  ELFStringTable dl_strtab; /* Dynamic symbol string table */
} DynamicLinkInfo;

__BEGIN_DECLS

int rtld_setup (Elf32_Ehdr *ehdr, Array *segments, DynamicLinkInfo *dlinfo);
int rtld_load_interp (Elf32_Ehdr *ehdr, Array *segments,
		      DynamicLinkInfo *dlinfo);

__END_DECLS

#endif
