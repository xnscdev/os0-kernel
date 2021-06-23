/*************************************************************************
 * mman.c -- This file is part of OS/0.                                  *
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
#include <sys/syscall.h>
#include <vm/heap.h>
#include <vm/paging.h>

void *
sys_mmap (void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *inode = NULL;
  ProcessMemoryRegion *region;
  uint32_t base;
  uint32_t vaddr;
  uint32_t i;
  int pgflags = PAGE_FLAG_USER;
  int first = 0;
  int last = proc->p_mregions->sa_size - 1;

  /* Make sure arguments are valid */
  if (!(flags & MAP_ANONYMOUS))
    {
      inode = inode_from_fd (fd);
      if (unlikely (inode == NULL || !S_ISREG (inode->vi_mode)))
	return (void *) -EBADF;
    }
  if (len == 0)
    return (void *) -EINVAL;
  if (proc->p_mregions->sa_size >= PROCESS_MREGION_LIMIT)
    return (void *) -ENOMEM;
  if (inode != NULL)
    {
      if (!(prot & PROT_WRITE)
	  && (proc->p_files[fd].pf_mode & O_ACCMODE) == O_WRONLY)
	return (void *) -EACCES;
      if (!(prot & PROT_READ)
	  && ((proc->p_files[fd].pf_mode & O_ACCMODE) == O_RDONLY))
	return (void *) -EACCES;
    }
  if (!(flags & MAP_SHARED) && !(flags & MAP_PRIVATE))
    return (void *) -EINVAL;

  /* Determine the address to place the mapping, considering the address
     hint given to this function, if specified */
  if (addr != NULL)
    {
      base = (uint32_t) addr;
      if (base & (PAGE_SIZE - 1))
	return (void *) -EINVAL; /* Address is not page aligned */
    }
  else
    base = PROCESS_BREAK_LIMIT;

  /* Search for the first mmap'd region with a base address above the
     address we calculated above */
  while (first <= last)
    {
      int mid = (first + last) / 2;
      ProcessMemoryRegion *region = proc->p_mregions->sa_elems[mid];
      if (region->pm_base > base)
	last = mid - 1;
      else
	first = mid + 1;
    }

  for (i = last + 1, first = base;
       region = proc->p_mregions->sa_elems[i], region->pm_base - first < len
	 && i < proc->p_mregions->sa_size;
       first = region->pm_base + region->pm_len, i++)
    ;
  base = first;

  if (prot & PROT_WRITE)
    pgflags |= PAGE_FLAG_WRITE;
  for (vaddr = base; vaddr < base + len; vaddr += PAGE_SIZE)
    {
      uint32_t paddr = alloc_page ();
      if (unlikely (paddr == 0))
	goto err;
      if (inode == NULL)
	map_page (curr_page_dir, paddr, vaddr, pgflags);
      else
	map_page (curr_page_dir, paddr, vaddr, PAGE_FLAG_WRITE);
      /* XXX Is invalidating page necessary when it should be not present? */
#ifdef INVLPG_SUPPORT
      vm_page_inval (paddr);
#endif
    }
#ifndef INVLPG_SUPPORT
  vm_tlb_reset ();
#endif

  /* Read file into memory area if specified
     TODO Support MAP_SHARED flag */
  if (inode != NULL)
    {
      int ret = vfs_read (inode, (void *) base, len, offset);
      if (ret < 0)
	return (void *) ret;
      for (vaddr = base; vaddr < base + len; vaddr += PAGE_SIZE)
	{
	  uint32_t paddr = get_paddr (curr_page_dir, (void *) vaddr);
	  map_page (curr_page_dir, paddr, vaddr, pgflags);
#ifdef INVLPG_SUPPORT
	  vm_page_inval (paddr);
#endif
	}
#ifndef INVLPG_SUPPORT
      vm_tlb_reset ();
#endif
    }

  /* Add the new region to the list and return the base address */
  region = kmalloc (sizeof (ProcessMemoryRegion));
  if (unlikely (region == NULL))
    goto err;
  region->pm_base = base;
  region->pm_len = len;
  region->pm_prot = prot;
  region->pm_flags = flags;
  region->pm_ino = inode;
  sorted_array_insert (proc->p_mregions, region);
  return (void *) base;

 err:
  for (i = base; i < vaddr; i += PAGE_SIZE)
    {
      uint32_t paddr = get_paddr (curr_page_dir, (void *) i);
      if (likely (paddr != 0))
	free_page (paddr);
    }
  return (void *) -ENOMEM;
}

int
sys_munmap (void *addr, size_t len)
{
  Process *proc = &process_table[task_getpid ()];
  ProcessMemoryRegion *region;
  int first = 0;
  int last = proc->p_mregions->sa_size - 1;
  int mid;

  /* Require the address to be page aligned */
  if (addr == NULL || (uint32_t) addr & (PAGE_SIZE - 1))
    return -EINVAL;

  /* Search for the memory region with the given base */
  while (first <= last)
    {
      mid = (first + last) / 2;
      region = proc->p_mregions->sa_elems[mid];
      if (region->pm_base == (uint32_t) addr)
	goto unmap;
      else if (region->pm_base > (uint32_t) addr)
	last = mid - 1;
      else
	first = mid + 1;
    }
  /* If we get here, no memory region has a base address that matches */
  return -EINVAL;

 unmap:
  for (first = region->pm_base; first < region->pm_base + region->pm_len;
       first += PAGE_SIZE)
    {
      uint32_t paddr = get_paddr (curr_page_dir, (void *) first);
      free_page (paddr);
      unmap_page (curr_page_dir, first);
#ifdef INVLPG_SUPPORT
      vm_page_inval (paddr);
#endif
    }
#ifndef INVLPG_SUPPORT
  vm_tlb_reset ();
#endif

  /* Remove region from array and return */
  sorted_array_remove (proc->p_mregions, mid);
  return 0;
}
