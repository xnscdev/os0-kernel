/*************************************************************************
 * rtld.c -- This file is part of OS/0.                                  *
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

#include <bits/mman.h>
#include <libk/libk.h>
#include <sys/process.h>
#include <vm/heap.h>
#include <vm/paging.h>

static inline Elf32_Sym *
rtld_get_symbol (DynamicLinkInfo *dlinfo, Elf32_Word index)
{
  return (Elf32_Sym *) ((uint32_t) dlinfo->dl_symtab.sym_table +
			index * dlinfo->dl_symtab.sym_entsize);
}

static void *
rtld_load_interp_segment (VFSInode *inode, SortedArray *mregions,
			  Elf32_Phdr *phdr)
{
  uint32_t vaddr;
  ProcessMemoryRegion *segment;
  void *start = (void *) (LD_SO_LOAD_ADDR + phdr->p_vaddr);
  uint32_t i;
  int prot = 0;

  for (vaddr = LD_SO_LOAD_ADDR; vaddr < LD_SO_LOAD_ADDR + phdr->p_memsz;
       vaddr += PAGE_SIZE)
    {
      uint32_t paddr = alloc_page ();
      if (unlikely (paddr == 0))
	goto err;
      map_page (curr_page_dir, paddr, vaddr + (phdr->p_vaddr & 0xfffff000),
	        PAGE_FLAG_USER | PAGE_FLAG_WRITE);
#ifdef INVLPG_SUPPORT
      vm_page_inval (paddr);
#endif
    }
#ifndef INVLPG_SUPPORT
  vm_tlb_reset ();
#endif

  if (phdr->p_filesz > 0)
    {
      if (vfs_read (inode, start, phdr->p_filesz, phdr->p_offset) < 0)
	goto err;
    }
  memset (start + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);

  if (phdr->p_flags & PF_R)
    prot |= PROT_READ;
  if (phdr->p_flags & PF_W)
    prot |= PROT_WRITE;
  if (phdr->p_flags & PF_X)
    prot |= PROT_EXEC;

  segment = kmalloc (sizeof (ProcessMemoryRegion));
  if (unlikely (segment == NULL))
    goto err;
  segment->pm_base = (uint32_t) start;
  segment->pm_len = phdr->p_memsz;
  segment->pm_prot = prot;
  segment->pm_flags = MAP_PRIVATE | MAP_ANONYMOUS;
  segment->pm_ino = NULL;
  segment->pm_offset = 0;
  sorted_array_insert (mregions, segment);
  return start;

 err:
  for (i = (uint32_t) start; i < vaddr + (phdr->p_vaddr & 0xfffff000);
       i += PAGE_SIZE)
    {
      uint32_t paddr = get_paddr (curr_page_dir, (void *) i);
      if (paddr != 0)
	free_page (paddr);
    }
  return NULL;
}

static int
rtld_load_interp_dynamic (Elf32_Dyn *dynamic, DynamicLinkInfo *dlinfo)
{
  Elf32_Dyn *entry;
  for (entry = dynamic; entry->d_tag != DT_NULL; entry++)
    {
      switch (entry->d_tag)
	{
	case DT_NEEDED:
	  return -1; /* ld.so is not allowed to use shared libraries */
	case DT_PLTRELSZ:
	  dlinfo->dl_pltrel.pt_size = entry->d_un.d_val;
	  break;
	case DT_PLTGOT:
	  dlinfo->dl_pltgot =
	    (Elf32_Addr *) (entry->d_un.d_ptr + LD_SO_LOAD_ADDR);
	  break;
	case DT_HASH:
	  dlinfo->dl_hash =
	    (Elf32_Word *) (entry->d_un.d_ptr + LD_SO_LOAD_ADDR);
	  break;
	case DT_STRTAB:
	  dlinfo->dl_strtab.st_table =
	    (char *) (entry->d_un.d_ptr + LD_SO_LOAD_ADDR);
	  break;
	case DT_SYMTAB:
	  dlinfo->dl_symtab.sym_table =
	    (Elf32_Sym *) (entry->d_un.d_ptr + LD_SO_LOAD_ADDR);
	  break;
	case DT_RELA:
	  dlinfo->dl_rela.ra_table =
	    (Elf32_Rela *) (entry->d_un.d_ptr + LD_SO_LOAD_ADDR);
	  break;
	case DT_RELASZ:
	  dlinfo->dl_rela.ra_size = entry->d_un.d_val;
	  break;
	case DT_RELAENT:
	  dlinfo->dl_rela.ra_entsize = entry->d_un.d_val;
	  break;
	case DT_STRSZ:
	  dlinfo->dl_strtab.st_len = entry->d_un.d_val;
	  break;
	case DT_SYMENT:
	  dlinfo->dl_symtab.sym_entsize = entry->d_un.d_val;
	  break;
	case DT_INIT:
	  dlinfo->dl_init = (void *) (entry->d_un.d_ptr + LD_SO_LOAD_ADDR);
	  break;
	case DT_FINI:
	  dlinfo->dl_fini = (void *) (entry->d_un.d_ptr + LD_SO_LOAD_ADDR);
	  break;
	case DT_REL:
	  dlinfo->dl_rel.r_table =
	    (Elf32_Rel *) (entry->d_un.d_ptr + LD_SO_LOAD_ADDR);
	  break;
	case DT_RELSZ:
	  dlinfo->dl_rel.r_size = entry->d_un.d_val;
	  break;
	case DT_RELENT:
	  dlinfo->dl_rel.r_entsize = entry->d_un.d_val;
	  break;
	case DT_PLTREL:
	  switch (entry->d_un.d_val)
	    {
	    case DT_REL:
	    case DT_RELA:
	      dlinfo->dl_pltrel.pt_type = entry->d_un.d_val;
	      break;
	    default:
	      return -1;
	    }
	  break;
	case DT_JMPREL:
	  dlinfo->dl_pltrel.pt_table =
	    (void *) (entry->d_un.d_ptr + LD_SO_LOAD_ADDR);
	  break;
	}
    }
  if (dlinfo->dl_strtab.st_table == NULL
      || dlinfo->dl_symtab.sym_table == NULL
      || dlinfo->dl_hash == NULL)
    return -1;
  return 0;
}

static int
rtld_load_interp_phdrs (VFSInode *inode, Elf32_Ehdr *ehdr,
			SortedArray *mregions, DynamicLinkInfo *dlinfo,
			DynamicLinkInfo *interp_dlinfo)
{
  Elf32_Phdr *phdr = kmalloc (sizeof (Elf32_Phdr));
  Elf32_Dyn *dynamic = NULL;
  int i;
  if (unlikely (phdr == NULL))
    goto err;

  for (i = 0; i < ehdr->e_phnum; i++)
    {
      if (vfs_read (inode, phdr, sizeof (Elf32_Phdr),
		    ehdr->e_phoff + ehdr->e_phentsize * i) < 0)
	goto err;
      if (phdr->p_type == PT_LOAD)
	{
	  if (rtld_load_interp_segment (inode, mregions, phdr) == NULL)
	    goto err;
	}
      else if (phdr->p_type == PT_DYNAMIC)
        dynamic = (Elf32_Dyn *) (LD_SO_LOAD_ADDR + phdr->p_vaddr);
    }

  if (unlikely (dynamic == NULL))
    goto err;
  if (rtld_load_interp_dynamic (dynamic, interp_dlinfo) != 0)
    goto err;

  kfree (phdr);
  return 0;

 err:
  kfree (phdr);
  return -ENOEXEC;
}

static void
rtld_perform_rel (Elf32_Rel *entry, DynamicLinkInfo *dlinfo, Elf32_Sword addend)
{
  Elf32_Sym *symbol = rtld_get_symbol (dlinfo, ELF32_R_SYM (entry->r_info));
  switch (ELF32_R_TYPE (entry->r_info))
    {
#define REL_OFFSET ((Elf32_Addr *) (LD_SO_LOAD_ADDR + entry->r_offset))
    case R_386_GLOB_DAT:
    case R_386_JMP_SLOT:
      addend = 0;
    case R_386_32:
      *REL_OFFSET = LD_SO_LOAD_ADDR + symbol->st_value + addend;
      break;
    case R_386_PC32:
      *REL_OFFSET = symbol->st_value - entry->r_offset - 4 + addend;
      break;
    case R_386_RELATIVE:
      *REL_OFFSET += LD_SO_LOAD_ADDR + addend;
      break;
    case R_386_GOTOFF:
      *REL_OFFSET = symbol->st_value - (uint32_t) dlinfo->dl_pltgot + addend;
      break;
    case R_386_GOTPC:
      *REL_OFFSET = (uint32_t) dlinfo->dl_pltgot - entry->r_offset - 4 + addend;
      break;
#undef REL_OFFSET
    }
}

static unsigned long
rtld_symbol_hash (const unsigned char *name)
{
  unsigned long h = 0;
  unsigned long g;
  while (*name != '\0')
    {
      h = (h << 4) + *name++;
      g = h & 0xf0000000;
      if (g != 0)
	h ^= g >> 24;
      h &= ~g;
    }
  return h;
}

static void *
rtld_get_entry_point (DynamicLinkInfo *dlinfo)
{
  /* Look up the symbol named by the LD_SO_ENTRY_SYMBOL macro using
     the symbol hash function, and get the address of the entry point
     to the dynamic linker */
  Elf32_Word nbucket = dlinfo->dl_hash[0];
  Elf32_Word nchain = dlinfo->dl_hash[1];
  Elf32_Word *bucket = &dlinfo->dl_hash[2];
  Elf32_Word *chain = &bucket[nbucket];
  unsigned long hash = rtld_symbol_hash (LD_SO_ENTRY_SYMBOL);
  Elf32_Word y = bucket[hash % nbucket];
  while (y != STN_UNDEF)
    {
      Elf32_Sym *symbol =
	(Elf32_Sym *) ((uint32_t) dlinfo->dl_symtab.sym_table +
		       y * dlinfo->dl_symtab.sym_entsize);
      if (strcmp (dlinfo->dl_strtab.st_table + symbol->st_name,
		  LD_SO_ENTRY_SYMBOL) == 0)
	{
	  if (ELF32_ST_BIND (symbol->st_info) != STB_GLOBAL)
	    return NULL;
	  return (void *) (symbol->st_value + LD_SO_LOAD_ADDR);
	}
      y = chain[y % nchain];
    }
  return NULL; /* Could not find symbol */
}

int
rtld_setup (Elf32_Ehdr *ehdr, SortedArray *mregions, uint32_t *entry,
	    DynamicLinkInfo *dlinfo)
{
  DynamicLinkInfo interp_dlinfo;
  void *rtld_entry_addr;
  int ret;

  /* Load ELF interpreter */
  memset (&interp_dlinfo, 0, sizeof (DynamicLinkInfo));
  ret = rtld_load_interp (ehdr, mregions, dlinfo, &interp_dlinfo);
  if (unlikely (ret < 0))
    return ret;

  /* Perform relocations on ELF interpreter */
  ret = rtld_perform_interp_reloc (&interp_dlinfo);
  if (unlikely (ret < 0))
    return ret;

  /* Load entry point of ELF interpreter */
  rtld_entry_addr = rtld_get_entry_point (&interp_dlinfo);
  if (unlikely (rtld_entry_addr == NULL))
    return -ENOEXEC;
  *entry = (uint32_t) rtld_entry_addr;
  return 0;
}

int
rtld_load_interp (Elf32_Ehdr *ehdr, SortedArray *mregions,
		  DynamicLinkInfo *dlinfo, DynamicLinkInfo *interp_dlinfo)
{
  VFSInode *inode;
  int ret = vfs_open_file (&inode, dlinfo->dl_interp, 1);
  if (ret < 0)
    return ret;

  /* Read ELF header */
  ret = vfs_read (inode, ehdr, sizeof (Elf32_Ehdr), 0);
  if (ret < 0)
    goto end;

  /* Check magic number */
  if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1
      || ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3)
    {
      ret = -ENOEXEC;
      goto end;
    }

  /* Check for 32-bit little-endian ELF shared object for correct machine */
  if (ehdr->e_ident[EI_CLASS] != ELFCLASS32
      || ehdr->e_ident[EI_DATA] != ELFDATA2LSB
      || ehdr->e_type != ET_DYN
      || ehdr->e_machine != MACHTYPE)
    {
      ret = -ENOEXEC;
      goto end;
    }

  /* Load program headers */
  ret = rtld_load_interp_phdrs (inode, ehdr, mregions, dlinfo, interp_dlinfo);

 end:
  vfs_unref_inode (inode);
  return ret;
}

int
rtld_perform_interp_reloc (DynamicLinkInfo *dlinfo)
{
  size_t size;
  if (dlinfo->dl_rel.r_table != NULL)
    {
      for (size = 0; size < dlinfo->dl_rel.r_size;
	   size += dlinfo->dl_rel.r_entsize)
	{
	  Elf32_Rel *entry =
	    (Elf32_Rel *) ((uint32_t) dlinfo->dl_rel.r_table + size);
	  rtld_perform_rel (entry, dlinfo, 0);
	}
    }
  if (dlinfo->dl_rela.ra_table != NULL)
    {
      for (size = 0; size < dlinfo->dl_rela.ra_size;
	   size += dlinfo->dl_rela.ra_entsize)
	{
	  Elf32_Rela *entry =
	    (Elf32_Rela *) ((uint32_t) dlinfo->dl_rela.ra_table + size);
	  rtld_perform_rel ((Elf32_Rel *) entry, dlinfo, entry->r_addend);
	}
    }

  if (dlinfo->dl_pltrel.pt_type == DT_REL)
    {
      for (size = 0; size < dlinfo->dl_pltrel.pt_size / sizeof (Elf32_Rel);
	   size++)
	{
	  Elf32_Rel *entry = (Elf32_Rel *) dlinfo->dl_pltrel.pt_table + size;
	  rtld_perform_rel (entry, dlinfo, 0);
	}
    }
  else
    {
      for (size = 0; size < dlinfo->dl_pltrel.pt_size / sizeof (Elf32_Rela);
	   size++)
	{
	  Elf32_Rela *entry = (Elf32_Rela *) dlinfo->dl_pltrel.pt_table + size;
	  rtld_perform_rel ((Elf32_Rel *) entry, dlinfo, entry->r_addend);
	}
    }
  return 0;
}
