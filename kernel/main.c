/*************************************************************************
 * main.c -- This file is part of OS/0.                                  *
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

#include <fs/vfs.h>
#include <libk/libk.h>
#include <sys/ata.h>
#include <sys/cmdline.h>
#include <sys/device.h>
#include <sys/multiboot.h>
#include <sys/timer.h>
#include <video/vga.h>
#include <vm/heap.h>
#include <vm/paging.h>

BootOptions boot_options;

static struct
{
  const char *b_name;
  int b_arg;
  void *b_data;
  size_t b_maxsize;
} boot_option_table[] = {
  {"root", 1, boot_options.b_root, 24},
  {"rootfstype", 1, boot_options.b_rootfstype, 16},
  {"verbose", 0, &boot_options.b_verbose, 0},
  {NULL, 0, NULL, 0}
};

static void
splash (void)
{
  printk ("Welcome to OS/0 " VERSION "\nCopyright (C) XNSC 2021\n\n");
}

static void
cmdline_parse (char *ptr)
{
  char *val = strchr (ptr, '=');
  int i;
  if (val != NULL)
    *val++ = '\0';
  for (i = 0; boot_option_table[i].b_name != NULL; i++)
    {
      if (strcmp (boot_option_table[i].b_name, ptr) == 0)
	{
	  if (boot_option_table[i].b_arg && val == NULL)
	    panic ("Boot option `%s' requires an argument", ptr);
	  if (boot_option_table[i].b_arg)
	    memcpy (boot_option_table[i].b_data, val,
		    boot_option_table[i].b_maxsize);
	  else
	    *((int *) boot_option_table[i].b_data) = 1;
	  return;
	}
    }
  panic ("Unrecognized boot option: %s", ptr);
}

static void
cmdline_init (char *cmdline)
{
  char *ptr;
  for (ptr = strtok (cmdline, " \t\n\r\f"); ptr != NULL;
       ptr = strtok (NULL, " \t\n\r\f"))
    cmdline_parse (ptr);
}

void
kmain (MultibootInfo *info)
{
  timer_set_freq (1000);
  vga_init ();
  splash ();
  cmdline_init ((char *) (info->mi_cmdline + RELOC_VADDR));

  assert (info->mi_flags & MULTIBOOT_FLAG_MEMORY);
  mem_init (info->mi_memhigh);
  heap_init ();

  ata_init (PATA_BAR0, PATA_BAR1, PATA_BAR2, PATA_BAR3, PATA_BAR4);
  devices_init ();
  vfs_init ();

  if (strncmp (boot_options.b_root, "/dev/", 5) == 0)
    {
      memmove (boot_options.b_root, &boot_options.b_root[5], 11);
      memset (&boot_options.b_root[11], 0, 5);
    }
  else
    panic ("No root filesystem specified in boot options");
}
