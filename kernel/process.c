/*************************************************************************
 * process.c -- This file is part of OS/0.                               *
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

#include <kconfig.h>

#include <libk/libk.h>
#include <sys/process.h>
#include <vm/heap.h>
#include <vm/paging.h>
#include "elf.h"

Process process_table[PROCESS_LIMIT];

int
process_load_sections (VFSInode *inode, uint32_t *page_dir, Elf32_Off shoff,
		       Elf32_Half shentsize, Elf32_Half shnum)
{
  Elf32_Shdr *shdr;
  Elf32_Half i;

  shdr = kmalloc (sizeof (Elf32_Shdr));
  if (unlikely (shdr == NULL))
    return -1;

  for (i = 0; i < shnum; i++)
    {
      if (vfs_read (inode, shdr, sizeof (Elf32_Shdr),
		    shoff + shentsize * i) < 0)
	{
	  kfree (shdr);
	  return -1;
	}

      if (shdr->sh_flags & SHF_ALLOC)
	{
	  /* Allocate memory for section */
	  char *buffer;
	  uint32_t pdata = 0;
	  if (!(shdr->sh_addr & (PAGE_SIZE - 1)))
	    {
	      uint32_t flags = PAGE_FLAG_USER;
	      uint32_t j;
	      uint32_t *save_page_dir = curr_page_dir;

	      pdata = (uint32_t) mem_alloc (shdr->sh_size, 0);
	      if (unlikely (pdata == 0))
		{
		  kfree (shdr);
		  return -1;
		}
	      if (shdr->sh_flags & SHF_WRITE)
		flags |= PAGE_FLAG_WRITE;

	      curr_page_dir = page_dir;
	      for (j = 0; j < shdr->sh_size; j += PAGE_SIZE)
		map_page (pdata + j, shdr->sh_addr + j, flags);
	      curr_page_dir = save_page_dir;
	    }
	  buffer = (char *) shdr->sh_addr;

	  /* Fill buffer with section contents */
	  if (shdr->sh_type == SHT_PROGBITS)
	    {
	      if (vfs_read (inode, buffer, shdr->sh_size, shdr->sh_offset) < 0)
		{
		  mem_free ((void *) pdata, shdr->sh_size);
		  kfree (shdr);
		  return -1;
		}
	    }
	  else
	    memset (buffer, 0, shdr->sh_size);
	}
    }

  kfree (shdr);
  return 0;
}

int
process_load_elf (int fd)
{
  VFSInode *inode = process_table[0].p_files[fd].pf_inode;
  Elf32_Ehdr *ehdr;
  uint32_t *page_dir;
  ProcessTask *task;

  ehdr = kmalloc (sizeof (Elf32_Ehdr));
  if (unlikely (ehdr == NULL))
    return -1;
  if (vfs_read (inode, ehdr, sizeof (Elf32_Ehdr), 0) < 0)
    return -1;

  /* Check magic number */
  if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1
      || ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3)
    goto err;

  /* Check for 32-bit ELF */
  if (ehdr->e_ident[EI_CLASS] != ELFCLASS32)
    goto err;

  /* Check for little-endian */
  if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB)
    goto err;

  /* Make sure the file is executable */
  if (ehdr->e_type != ET_EXEC)
    goto err;

  /* Make sure the file is for Intel architecture */
  if (ehdr->e_machine != EM_386)
    goto err;

  /* Create new page directory */
  page_dir = kvalloc (PAGE_DIR_SIZE << 2);
  if (unlikely (page_dir == NULL))
    goto err;
  memset (page_dir, 0, PAGE_DIR_SIZE << 2);

  /* Map ELF sections into address space */
  if (process_load_sections (inode, page_dir, ehdr->e_shoff, ehdr->e_shentsize,
			     ehdr->e_shnum) != 0)
    goto err;

  /* Create new process */
  task = task_new (ehdr->e_entry, page_dir);
  memset (process_table[task->t_pid].p_files, 0,
	  sizeof (ProcessFile) * PROCESS_FILE_LIMIT);
  process_table[task->t_pid].p_task = task;

  kfree (ehdr);
  return 0;

 err:
  kfree (ehdr);
  return -1;
}
