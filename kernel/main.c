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

#include <fs/devfs.h>
#include <libk/libk.h>
#include <sys/acpi.h>
#include <sys/ata.h>
#include <sys/cmdline.h>
#include <sys/multiboot.h>
#include <sys/process.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/timer.h>
#include <sys/wait.h>
#include <video/serial.h>
#include <video/vga.h>
#include <vm/heap.h>
#include <vm/paging.h>

BootOptions boot_options;

static char *init_argv[] = {
  "/sbin/init",
  NULL
};

static char *init_envp[] = {
  "PWD=/",
  "TERM=vt100",
  NULL
};

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
  printk ("Welcome to OS/0 " VERSION "\nCopyright (C) XNSC 2021\n\n"
	  "System time: %lu\n\n", time (NULL));
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

static void
mount_rootfs (void)
{
  int fd;
  /* Setup devfs first so block devices can be resolved properly */
  if (vfs_mount ("devfs", "/dev", 0, NULL) != 0)
    panic ("Failed to setup devfs");

  if (*boot_options.b_root == '\0')
    panic ("No root filesystem specified in boot options");

  if (*boot_options.b_rootfstype == '\0')
    {
      VFSInode *inode;
      SpecDevice *dev;
      int type;

      if (vfs_open_file (&inode, boot_options.b_root, 1) != 0)
	panic ("Failed to open root filesystem device %s", boot_options.b_root);
      /* Make sure we are mounting a valid block device */
      if (strcmp (inode->vi_sb->sb_fstype->vfs_name, devfs_vfs.vfs_name) != 0
	  || !S_ISBLK (inode->vi_mode))
	panic ("Invalid block device %s", boot_options.b_root);
      dev = device_lookup (major (inode->vi_rdev), minor (inode->vi_rdev));
      assert (dev != NULL);

      type = vfs_guess_type (dev);
      switch (type)
	{
	case FS_TYPE_EXT2:
	  strcpy (boot_options.b_rootfstype, "ext2");
	  break;
	default:
	  panic ("Root filesystem type not specified");
	}
    }

  if (vfs_mount (boot_options.b_rootfstype, "/", 0, boot_options.b_root) != 0)
    panic ("Failed to mount root filesystem");

  /* Setup kernel task working directory */
  fd = sys_open ("/", O_RDONLY, 0);
  if (fd < 0)
    panic ("Failed to load kernel task working directory");
  process_table[0].p_cwd = process_table[0].p_files[fd]->pf_inode;
  vfs_ref_inode (process_table[0].p_cwd);
  process_table[0].p_cwdpath = strdup ("/");
  assert (process_table[0].p_cwdpath != NULL);
  process_free_fd (process_table, fd);
}

static void
init (void)
{
  int pid = sys_fork ();
  if (pid < 0)
    panic ("Failed to fork kernel process");
  else if (pid == 0)
    {
      int ret = sys_execve ("/sbin/init", init_argv, init_envp);
      panic ("Failed to execute /sbin/init: %s", strerror (ret));
    }
  else
    {
      int status;
      sys_waitpid (pid, &status, 0);
      if (WIFEXITED (status))
	panic ("/sbin/init exited with status %d", WEXITSTATUS (status));
      else if (WIFSIGNALED (status))
	panic ("/sbin/init received signal %s", sys_signame[WTERMSIG (status)]);
    }
}

void
kmain (MultibootInfo *info)
{
  assert (info->mi_flags & MULTIBOOT_FLAG_MEMORY);

  timer_set_freq (TIMER_PORT_CHANNEL0, 1000);
  speaker_init ();
  vga_init ();
  serial_init ();
  splash ();
  if (info->mi_flags & MULTIBOOT_FLAG_CMDLINE)
    cmdline_init ((char *) (info->mi_cmdline + RELOC_VADDR));
  if (!acpi_enable ())
    printk ("acpi: failed to enable\n");

  random_init ();
  heap_init ();
  mem_init (info->mi_memhigh);
  scheduler_init ();

  ata_init ();
  devices_init ();
  vfs_init ();
  mount_rootfs ();
  process_setup_std_streams (0);

  init ();
}
