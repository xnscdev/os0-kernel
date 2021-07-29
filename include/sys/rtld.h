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
#include <sys/memory.h>
#include <elf.h>

/* Macros for determining valid ELF header machine type */
#ifdef  ARCH_I386
#define MACHTYPE EM_386
#else
#error "No ELF machine type supported for architecture"
#endif

#define LD_SO_LOAD_ADDR    0x00008000
#define LD_SO_ENTRY_SYMBOL "rtld_main"

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
  Elf32_Word pt_type;
  void *pt_table;
  size_t pt_size;
} PLTRelTable;

typedef struct
{
  int dl_active;            /* Set if program uses dynamic linking */
  void *dl_loadbase;        /* Pointer to ELF header */
  void *dl_entry;           /* Entry point of executable */
  void *dl_rtldentry;       /* Entry point of dynamic linker */
  char *dl_interp;          /* ELF interpreter path */
  Elf32_Dyn *dl_dynamic;    /* Address of ELF .dynamic section */
  Elf32_Addr *dl_pltgot;          /* Address of PLT/GOT */
  Elf32_Word *dl_hash;      /* Symbol hash table */
  ELFStringTable dl_strtab; /* Symbol string table */
  ELFSymbolTable dl_symtab; /* Dynamic symbol table */
  ELFRelaTable dl_rela;     /* Relocations with explicit addends */
  ELFRelTable dl_rel;       /* Relocation table */
  void (*dl_init) (void);   /* Address of initialization function */
  void (*dl_fini) (void);   /* Address of termination function */
  PLTRelTable dl_pltrel;    /* Relocation table for PLT */
} DynamicLinkInfo;

__BEGIN_DECLS

int rtld_setup (Elf32_Ehdr *ehdr, SortedArray *mregions, uint32_t *entry,
		DynamicLinkInfo *dlinfo);
int rtld_load_interp (Elf32_Ehdr *ehdr, SortedArray *mregions,
		      DynamicLinkInfo *dlinfo, DynamicLinkInfo *interp_dlinfo);
int rtld_perform_interp_reloc (DynamicLinkInfo *dlinfo);
void rtld_setup_dynamic_linker (void);

__END_DECLS

#endif
