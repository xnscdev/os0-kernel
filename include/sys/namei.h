/*************************************************************************
 * namei.h -- This file is part of OS/0.                                 *
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

#ifndef _SYS_NAMEI_H
#define _SYS_NAMEI_H

#include <sys/cdefs.h>
#include <sys/stat.h>

typedef struct _NameiData NameiData;

struct _NameiData
{
  struct stat nd_stat;
  char *nd_name;
  char *nd_abslnk;
  int nd_relstart;
  int nd_level;
  int nd_mntpoint;
  int nd_noent;
  NameiData *nd_next;
};

__BEGIN_DECLS

NameiData *namei (NameiData *parent, const char *path, int start,
		  NameiData **last);

__END_DECLS

#endif
