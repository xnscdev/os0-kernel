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

void user_mode_exec (uint32_t eip)
  __attribute__ ((noreturn, aligned (PAGE_SIZE)));

Process process_table[PROCESS_LIMIT];

static int
process_alloc_section (VFSInode *inode, uint32_t *page_dir, Elf32_Shdr *shdr)
{
  uint32_t flags;
  uint32_t addr;
  uint32_t offset;

  flags = PAGE_FLAG_USER;
  if (shdr->sh_flags & SHF_WRITE)
    flags |= PAGE_FLAG_WRITE;

  for (addr = shdr->sh_addr & 0xfffff000, offset = shdr->sh_offset;
       addr < shdr->sh_addr + shdr->sh_size; addr += PAGE_SIZE)
    {
      uint32_t paddr = get_paddr (page_dir, (void *) addr) & 0xfffff000;
      uint32_t start;
      if (paddr == 0)
	{
	  paddr = (uint32_t) mem_alloc (PAGE_SIZE, 0);
	  if (unlikely (paddr == 0))
	    return -ENOMEM;
	  map_page (page_dir, paddr, addr, flags);
	}
      map_page (curr_page_dir, paddr, PAGE_COPY_VADDR, PAGE_FLAG_WRITE);
#ifdef INVLPG_SUPPORT
      vm_page_inval (paddr);
#else
      vm_tlb_reset ();
#endif

      start = addr < shdr->sh_addr ? shdr->sh_addr & (PAGE_SIZE - 1) : 0;
      if (shdr->sh_type == SHT_PROGBITS)
	{
	  int ret = vfs_read (inode, (char *) (PAGE_COPY_VADDR + start),
			      PAGE_SIZE - start, offset);
	  if (ret < 0)
	    return ret;
	}
      else
	memset ((char *) (PAGE_COPY_VADDR + start), 0, PAGE_SIZE - start);
      offset += PAGE_SIZE - start;
    }
  return 0;
}

static int
process_load_sections (VFSInode *inode, uint32_t *page_dir, Elf32_Off shoff,
		       Elf32_Half shentsize, Elf32_Half shnum)
{
  Elf32_Shdr *shdr;
  Elf32_Half i;
  int ret;

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
	  ret = process_alloc_section (inode, page_dir, shdr);
	  if (ret < 0)
	    {
	      kfree (shdr);
	      return ret;
	    }
	}
    }

  kfree (shdr);
  return 0;
}

int
process_spawn (VFSInode *inode)
{
  Elf32_Ehdr *ehdr;
  int pid;
  int ret;

  __asm__ volatile ("cli");
  ehdr = kmalloc (sizeof (Elf32_Ehdr));
  if (unlikely (ehdr == NULL))
    {
      ret = -ENOMEM;
      goto end;
    }
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

  /* Check for Intel 32-bit little-endian ELF executable */
  if (ehdr->e_ident[EI_CLASS] != ELFCLASS32
      || ehdr->e_ident[EI_DATA] != ELFDATA2LSB || ehdr->e_type != ET_EXEC
      || ehdr->e_machine != EM_386)
    {
      ret = -ENOEXEC;
      goto end;
    }

  /* Create new process */
  pid = task_new ();
  if (pid < 0)
    {
      ret = pid;
      goto end;
    }

  /* Map ELF sections into address space */
  ret = process_load_sections (inode, process_table[pid].p_task->t_pgdir,
			       ehdr->e_shoff, ehdr->e_shentsize, ehdr->e_shnum);
  if (ret != 0)
    goto task_end;
  process_table[pid].p_task->t_ebp = ehdr->e_entry;
  process_table[pid].p_task->t_eip = USER_START_ADDR;
  map_page (process_table[pid].p_task->t_pgdir,
	    get_paddr (curr_page_dir, user_mode_exec), USER_START_ADDR,
	    PAGE_FLAG_USER);
  ret = pid;
  goto end;

 task_end:
  process_free (pid);
 end:
  kfree (ehdr);
  __asm__ volatile ("sti");
  return ret;
}

void
process_free (pid_t pid)
{
  int i;
  if (pid >= PROCESS_FILE_LIMIT || process_table[pid].p_task == NULL)
    return;
  task_free ((ProcessTask *) process_table[pid].p_task);
  for (i = 0; i < PROCESS_FILE_LIMIT; i++)
    {
      if (process_table[pid].p_files[i].pf_inode != NULL)
	{
	  vfs_destroy_inode (process_table[pid].p_files[i].pf_inode);
	  memset (&process_table[pid].p_files[i], 0, sizeof (ProcessFile));
	}
    }
  process_table[pid].p_task = NULL;
}
