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
  int first = 0;
  int last = proc->p_mregions->sa_size - 1;
  int pgflags = flags != PROT_NONE ? PAGE_FLAG_USER : 0;
  size_t bytes = len;

  /* Make sure arguments are valid */
  if (!(flags & MAP_ANONYMOUS))
    {
      inode = inode_from_fd (fd);
      if (unlikely (inode == NULL || !S_ISREG (inode->vi_mode)))
	return (void *) -EBADF;
      if (offset & (PAGE_SIZE - 1))
	return (void *) -EINVAL;
      else if (offset > inode->vi_size)
	return (void *) -EINVAL;
    }
  if (len == 0)
    return (void *) -EINVAL;
  if (proc->p_mregions->sa_size >= PROCESS_MMAP_LIMIT)
    return (void *) -ENOMEM;
  if (inode != NULL)
    {
      if (!(prot & PROT_WRITE)
	  && (proc->p_files[fd]->pf_mode & O_ACCMODE) == O_WRONLY)
	return (void *) -EACCES;
      if (!(prot & PROT_READ)
	  && ((proc->p_files[fd]->pf_mode & O_ACCMODE) == O_RDONLY))
	return (void *) -EACCES;
    }
  if (!(flags & MAP_SHARED) && !(flags & MAP_PRIVATE))
    return (void *) -EINVAL;
  len = ((len - 1) | (PAGE_SIZE - 1)) + 1;
  prot &= __PROT_MASK;

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
  if (proc->p_mregions->sa_size == 0)
    goto map;

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

  /* Search each mapped region until we find a gap between regions at least
     len bytes long */
  if (last < proc->p_mregions->sa_size - 1)
    {
      ProcessMemoryRegion *before = proc->p_mregions->sa_elems[last];
      vaddr = before->pm_base + before->pm_len;
    }
  else
    vaddr = base;
  for (i = last + 1; i < proc->p_mregions->sa_size;
       vaddr = region->pm_base + region->pm_len, i++)
    {
      region = proc->p_mregions->sa_elems[i];
      if (region->pm_base - vaddr >= len)
	break;
    }
  base = vaddr;

 map:
  if (prot & PROT_WRITE)
    pgflags |= PAGE_FLAG_WRITE;
  for (vaddr = base; vaddr < base + len; vaddr += PAGE_SIZE)
    {
      uint32_t paddr = alloc_page ();
      if (unlikely (paddr == 0))
	goto err;
      map_page (curr_page_dir, paddr, vaddr, PAGE_FLAG_WRITE);
      /* XXX Is invalidating page necessary when it should be not present? */
#ifdef INVLPG_SUPPORT
      vm_page_inval (paddr);
#endif
    }
#ifndef INVLPG_SUPPORT
  vm_tlb_reset ();
#endif

  if (inode == NULL)
    {
      /* Zero memory region unless requested to be uninitialized */
      if (!(flags & MAP_UNINITIALIZED))
	memset ((void *) base, 0, len);
    }
  else
    {
      /* Read file into memory area if specified
	 TODO Support MAP_SHARED flag */
      int ret;
      ret = vfs_read (inode, (void *) base, bytes, offset);
      if (ret < 0)
	return (void *) ret;
      /* Zero remaining bytes in region unless requested to be uninitialized */
      if (!(flags & MAP_UNINITIALIZED))
	memset ((void *) (base + bytes), 0, len - ret);
    }

  /* Remap memory region with requested protection */
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

  /* Add the new region to the list of mapped areas */
  region = kmalloc (sizeof (ProcessMemoryRegion));
  if (unlikely (region == NULL))
    goto err;
  region->pm_base = base;
  region->pm_len = len;
  region->pm_prot = prot;
  region->pm_flags = flags;
  region->pm_ino = inode;
  region->pm_offset = offset;
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
  uint32_t vaddr;
  uint32_t temp;
  int mid;

  /* Require the address to be page aligned */
  if (addr == NULL || (uint32_t) addr & (PAGE_SIZE - 1))
    return -EINVAL;
  len = ((len - 1) | (PAGE_SIZE - 1)) + 1;

  /* Search for the memory region containing the given address */
  while (first <= last)
    {
      mid = (first + last) / 2;
      region = proc->p_mregions->sa_elems[mid];
      if (region->pm_base <= (uint32_t) addr
	  && (uint32_t) addr < region->pm_base + region->pm_len)
	goto unmap;
      else if (region->pm_base > (uint32_t) addr)
	last = mid - 1;
      else
	first = mid + 1;
    }
  /* If we get here, no memory region has a base address that matches */
  return -EINVAL;

 unmap:
  temp = (uint32_t) addr;
  for (vaddr = temp; vaddr < (uint32_t) addr + len; vaddr += PAGE_SIZE)
    {
      uint32_t paddr;
      if (vaddr >= region->pm_base + region->pm_len)
	{
	  /* We have overlapped into another adjacent memory area, unmap
	     the remaining pages from that area */
	  int ret = sys_munmap ((void *) vaddr, len - region->pm_len);
	  if (ret < 0)
	    return ret;
	  break;
	}
      paddr = get_paddr (curr_page_dir, (void *) vaddr);
      free_page (paddr);
      unmap_page (curr_page_dir, vaddr);
#ifdef INVLPG_SUPPORT
      vm_page_inval (paddr);
#endif
    }
#ifndef INVLPG_SUPPORT
  vm_tlb_reset ();
#endif

  /* If there are more pages beyond the unmapped region, mark them as part
     of a separate region */
  if (vaddr < region->pm_base + region->pm_len)
    {
      ProcessMemoryRegion *new = kmalloc (sizeof (ProcessMemoryRegion));
      if (unlikely (new == NULL))
	return -ENOMEM;
      new->pm_base = vaddr;
      new->pm_len = region->pm_base + region->pm_len - vaddr;
      new->pm_prot = region->pm_prot;
      new->pm_flags = region->pm_flags;
      new->pm_ino = region->pm_ino;
      new->pm_offset = region->pm_offset + vaddr - region->pm_base;
      vfs_ref_inode (new->pm_ino);
      sorted_array_insert (proc->p_mregions, new);
    }

  /* If there are pages beneath the unmapped region, update the length
     of the region accordingly, otherwise remove the entry from the array */
  if (temp > region->pm_base)
    region->pm_len = temp - region->pm_base;
  else
    {
      sorted_array_remove (proc->p_mregions, mid);
      vfs_unref_inode (region->pm_ino);
      kfree (region);
    }

  return 0;
}

int
sys_mprotect (void *addr, size_t len, int prot)
{
  Process *proc = &process_table[task_getpid ()];
  uint32_t rem;
  uint32_t vaddr;
  int first = 0;
  int last = proc->p_mregions->sa_size - 1;
  ProcessMemoryRegion *region;
  int pgflags = prot != PROT_NONE ? PAGE_FLAG_USER : 0;
  if ((uint32_t) addr & (PAGE_SIZE - 1))
    return -EINVAL;
  if (len == 0)
    return -EINVAL;
  len = ((len - 1) | (PAGE_SIZE - 1)) + 1;
  prot &= __PROT_MASK;

  /* Search for the memory region containing the given address */
  while (first <= last)
    {
      int mid = (first + last) / 2;
      region = proc->p_mregions->sa_elems[mid];
      if (region->pm_base <= (uint32_t) addr
	  && (uint32_t) addr < region->pm_base + region->pm_len)
	goto protect;
      else if (region->pm_base > (uint32_t) addr)
	last = mid - 1;
      else
	first = mid + 1;
    }
  /* If we get here, no memory region has a base address that matches */
  return -EINVAL;

 protect:
  /* Change protection of part of the overlapping memory area if the length 
     exceeds the remaining number of bytes in the current one */
  rem = region->pm_base + region->pm_len - (uint32_t) addr;
  if (len > rem)
    {
      int ret = sys_mprotect ((void *) (region->pm_base + region->pm_len),
			      len - rem, prot);
      if (ret < 0)
	return ret;
      len -= rem;
    }

  if ((region->pm_prot | PROT_EXEC) == (prot | PROT_EXEC))
    return 0; /* Already has requested protection (PROT_EXEC is ignored) */

  /* Remap memory region with requested protection */
  if (prot & PROT_WRITE)
    pgflags |= PAGE_FLAG_WRITE;
  for (vaddr = (uint32_t) addr; vaddr < (uint32_t) addr + len;
       vaddr += PAGE_SIZE)
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
  region->pm_prot = prot;
  return 0;
}
