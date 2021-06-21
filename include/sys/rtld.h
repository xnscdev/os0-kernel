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

#include <sys/cdefs.h>
#include <elf.h>
#include <stddef.h>

typedef struct
{
  char *st_table;
  size_t st_len;
} ELFStringTable;

typedef struct
{
  Elf32_Sym *sym_table;
  size_t sym_entsize;
} ELFSymbolTable;

typedef struct
{
  Elf32_Rela *ra_table;
  size_t ra_size;
  size_t ra_entsize;
} ELFRelaTable;

typedef struct
{
  Elf32_Rel *r_table;
  size_t r_size;
  size_t r_entsize;
} ELFRelTable;

typedef struct
{
  int dl_active;            /* Set if program uses dynamic linking */
  void *dl_loadbase;        /* Pointer to ELF header */
  char *dl_interp;          /* ELF interpreter path */
  Elf32_Dyn *dl_dynamic;    /* Address of ELF .dynamic section */
  void *dl_pltgot;          /* Address of PLT/GOT */
  void *dl_hash;            /* Hash table */
  ELFStringTable dl_strtab; /* Dynamic symbol string table */
  ELFSymbolTable dl_symtab; /* Dynamic symbol table */
  ELFRelaTable dl_rela;     /* Relocations with explicit addends */
  ELFRelTable dl_rel;       /* Relocation table */
} DynamicLinkInfo;

__BEGIN_DECLS

int rtld_setup (DynamicLinkInfo *dlinfo);
int rtld_load_dynamic_section (DynamicLinkInfo *dlinfo);

__END_DECLS

#endif
