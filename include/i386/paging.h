/*************************************************************************
 * paging.h -- This file is part of OS/0.                                *
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

#ifndef _I386_PAGING_H
#define _I386_PAGING_H

#ifndef _ASM
#include <kconfig.h>

#include <sys/cdefs.h>
#include <stdint.h>
#endif

#define PAGE_DIR_SIZE 1024
#define PAGE_TBL_SIZE 1024

#define PAGE_COPY_VADDR 0x1000

/* Page entry flags */

#define PAGE_FLAG_PRESENT (1 << 0)
#define PAGE_FLAG_WRITE   (1 << 1)
#define PAGE_FLAG_USER    (1 << 2)
#define PAGE_FLAG_WTHRU   (1 << 3)
#define PAGE_FLAG_NOCACHE (1 << 4)
#define PAGE_FLAG_ACCESS  (1 << 5)
#define PAGE_FLAG_4M      (1 << 6)

/* Page fault flags */

#define PF_FLAG_PROT      (1 << 0)
#define PF_FLAG_WRITE     (1 << 1)
#define PF_FLAG_USER      (1 << 2)
#define PF_FLAG_RESERVED  (1 << 3)
#define PF_FLAG_INSTFETCH (1 << 4)

#ifndef _ASM

__BEGIN_DECLS

/* Invalidates the entire translation lookaside buffer */

__always_inline static inline void
vm_tlb_reset (void)
{
  __asm__ volatile ("mov %%cr3, %%eax\nmov %%eax, %%cr3" ::: "memory");
}

/* Invalidates the entire TLB only if running on an i386 processor without 
   support for the invlpg instruction */

#ifdef INVLPG_SUPPORT
#define vm_tlb_reset_386()
#else
#define vm_tlb_reset_386() vm_tlb_reset ()
#endif

/* Invalidates a single entry in the TLB, requires support for invlpg */

__always_inline static inline void
vm_page_inval (uint32_t addr)
{
#ifdef INVLPG_SUPPORT
  __asm__ volatile ("invlpg (%0)" :: "r" (addr) : "memory");
#endif
}

/* Invalidates a single entry in the TLB, or the entire TLB if invlpg is
   not supported */

#ifdef INVLPG_SUPPORT
#define vm_page_inval_386(addr) vm_page_inval (addr)
#else
#define vm_page_inval_386(addr) vm_tlb_reset ()
#endif

uint32_t *page_table_clone (uint32_t index, uint32_t *orig);
uint32_t *page_dir_clone (uint32_t *orig);
void page_dir_free (uint32_t *dir);
void page_dir_exec_free (uint32_t *dir);

__END_DECLS

#endif

#endif
