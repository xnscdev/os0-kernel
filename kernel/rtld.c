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

#include <libk/libk.h>
#include <sys/process.h>
#include <sys/rtld.h>
#include <vm/heap.h>
#include <vm/paging.h>

static void *
rtld_load_interp_segment (VFSInode *inode, Array *segments, Elf32_Phdr *phdr)
{
  uint32_t vaddr;
  ProcessSegment *segment;
  void *start = (void *) (LD_SO_LOAD_ADDR + (phdr->p_vaddr & 0xfffff000));
  uint32_t i;

  for (vaddr = (uint32_t) start; vaddr < (uint32_t) start + phdr->p_memsz;
       vaddr += PAGE_SIZE)
    {
      uint32_t paddr = alloc_page ();
      if (unlikely (paddr == 0))
	goto err;
      map_page (curr_page_dir, paddr, vaddr, PAGE_FLAG_USER | PAGE_FLAG_WRITE);
#ifdef INVLPG_SUPPORT
      vm_page_inval (paddr);
#else
      vm_tlb_reset ();
#endif
    }

  if (phdr->p_filesz > 0)
    {
      if (vfs_read (inode, start, phdr->p_filesz, phdr->p_offset) < 0)
	goto err;
    }
  memset (start + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);

  segment = kmalloc (sizeof (ProcessSegment));
  if (unlikely (segment == NULL))
    goto err;
  segment->ps_addr = (uint32_t) start;
  segment->ps_size = phdr->p_memsz;
  array_append (segments, segment);
  return start;

 err:
  for (i = (uint32_t) start; i < vaddr; i += PAGE_SIZE)
    {
      uint32_t paddr = get_paddr (curr_page_dir, (void *) i);
      if (paddr != 0)
	free_page (paddr);
    }
  return NULL;
}

int
rtld_setup (Elf32_Ehdr *ehdr, Array *segments, DynamicLinkInfo *dlinfo)
{
  int ret = rtld_load_interp (ehdr, segments, dlinfo);
  if (ret < 0)
    return ret;
  return 0;
}

int
rtld_load_interp (Elf32_Ehdr *ehdr, Array *segments, DynamicLinkInfo *dlinfo)
{
  Elf32_Phdr *phdr = NULL;
  VFSInode *inode;
  int ret;
  int i;

  ret = vfs_open_file (&inode, dlinfo->dl_interp, 1);
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
  phdr = kmalloc (sizeof (Elf32_Phdr));
  if (unlikely (phdr == NULL))
    {
      ret = -ENOMEM;
      goto end;
    }
  for (i = 0; i < ehdr->e_phnum; i++)
    {
      ret = vfs_read (inode, phdr, sizeof (Elf32_Phdr),
		      ehdr->e_phoff + ehdr->e_phentsize * i);
      if (ret < 0)
	{
	  kfree (phdr);
	  return ret;
	}
      if (phdr->p_type == PT_LOAD)
	{
	  void *addr = rtld_load_interp_segment (inode, segments, phdr);
	  if (addr == NULL)
	    {
	      ret = -ENOEXEC;
	      goto end;
	    }
	  if (phdr->p_offset == 0)
	    dlinfo->dl_interpload = addr;
	}
    }

 end:
  vfs_unref_inode (inode);
  kfree (phdr);
  return ret;
}
