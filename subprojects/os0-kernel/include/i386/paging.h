/*************************************************************************
 * paging.h -- This file is part of OS/0.                                *
 * Copyright (C) 2020 XNSC                                               *
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

#include <sys/cdefs.h>

#define PAGE_DIR_SIZE 1024
#define PAGE_TBL_SIZE 1024

/* Page directory entry flags */

#define PAGE_DFLAG_PRESENT (1 << 0)
#define PAGE_DFLAG_WRITE   (1 << 1)
#define PAGE_DFLAG_USER    (1 << 2)
#define PAGE_DFLAG_WTHRU   (1 << 3)
#define PAGE_DFLAG_NOCACHE (1 << 4)
#define PAGE_DFLAG_ACCESS  (1 << 5)
#define PAGE_DFLAG_4M      (1 << 6)

/* Page table entry flags */

#define PAGE_TFLAG_MEMTYPE (1 << 0)
#define PAGE_TFLAG_DIRTY   (1 << 1)
#define PAGE_TFLAG_GLOBAL  (1 << 2)
#define PAGE_TFLAG_CACHE   (1 << 3)

__BEGIN_DECLS

void paging_init (void);
void paging_loaddir (u32 addr);
void paging_enable (void);

__END_DECLS

#endif
