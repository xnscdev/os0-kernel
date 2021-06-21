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

#include <sys/rtld.h>
#include <errno.h>

int
rtld_setup (DynamicLinkInfo *dlinfo)
{
  int ret = rtld_load_dynamic_section (dlinfo);
  if (ret < 0)
    return ret;
  return 0;
}

int
rtld_load_dynamic_section (DynamicLinkInfo *dlinfo)
{
  Elf32_Dyn *entry;
  for (entry = dlinfo->dl_dynamic; entry->d_tag != DT_NULL; entry++)
    {
      switch (entry->d_tag)
	{
	case DT_PLTGOT:
	  dlinfo->dl_pltgot = (void *) entry->d_un.d_ptr;
	  break;
	case DT_HASH:
	  dlinfo->dl_hash = (void *) entry->d_un.d_ptr;
	  break;
	case DT_STRTAB:
	  dlinfo->dl_strtab.st_table = (char *) entry->d_un.d_ptr;
	  break;
	case DT_SYMTAB:
	  dlinfo->dl_symtab.sym_table = (Elf32_Sym *) entry->d_un.d_ptr;
	  break;
	case DT_RELA:
	  dlinfo->dl_rela.ra_table = (Elf32_Rela *) entry->d_un.d_ptr;
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
	case DT_REL:
	  dlinfo->dl_rel.r_table = (Elf32_Rel *) entry->d_un.d_ptr;
	  break;
	case DT_RELSZ:
	  dlinfo->dl_rel.r_size = entry->d_un.d_val;
	  break;
	case DT_RELENT:
	  dlinfo->dl_rel.r_entsize = entry->d_un.d_val;
	  break;
	}
    }

  /* Make sure all mandatory fields in dynamic section were supplied */
  if (dlinfo->dl_pltgot == NULL
      || dlinfo->dl_hash == NULL
      || dlinfo->dl_strtab.st_table == NULL
      || dlinfo->dl_strtab.st_len == 0
      || dlinfo->dl_symtab.sym_table == NULL
      || dlinfo->dl_symtab.sym_entsize == 0)
    return -ENOEXEC;

  /* At least one of RELA or REL must be specified */
  if (dlinfo->dl_rela.ra_table == NULL && dlinfo->dl_rel.r_table == NULL)
    return -ENOEXEC;

  /* If relocation table is specified, entry size must be nonzero */
  if (dlinfo->dl_rela.ra_table != NULL && dlinfo->dl_rela.ra_entsize == 0)
    return -ENOEXEC;
  if (dlinfo->dl_rel.r_table != NULL && dlinfo->dl_rel.r_entsize == 0)
    return -ENOEXEC;
  return 0;
}
