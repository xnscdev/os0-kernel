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
#include <video/vga.h>
#include <vm/heap.h>
#include <vm/paging.h>
#include <elf.h>

/* Macros for determining valid ELF header machine type */
#ifdef  ARCH_I386
#define MACHTYPE EM_386
#else
#error "No ELF machine type supported for architecture"
#endif

Process process_table[PROCESS_LIMIT];
int exit_task;

static int
process_load_segment (VFSInode *inode, Array *segments, Elf32_Phdr *phdr)
{
  uint32_t addr;
  ProcessSegment *segment;

  for (addr = phdr->p_vaddr & 0xfffff000; addr < phdr->p_vaddr + phdr->p_memsz;
       addr += PAGE_SIZE)
    {
      uint32_t paddr = alloc_page ();
      if (unlikely (paddr == 0))
	return -ENOMEM;
      map_page (curr_page_dir, paddr, addr, PAGE_FLAG_USER | PAGE_FLAG_WRITE);
#ifdef INVLPG_SUPPORT
      vm_page_inval (paddr);
#else
      vm_tlb_reset ();
#endif
    }

  segment = kmalloc (sizeof (ProcessSegment));
  if (segment == NULL)
    return -ENOMEM;
  segment->ps_addr = phdr->p_vaddr & 0xfffff000;
  segment->ps_size = phdr->p_memsz;
  array_append (segments, segment);

  if (phdr->p_filesz > 0)
    {
      if (vfs_read (inode, (void *) phdr->p_vaddr, phdr->p_filesz,
		    phdr->p_offset) < 0)
	return -EIO;
    }
  memset ((void *) (phdr->p_vaddr + phdr->p_filesz), 0,
	  phdr->p_memsz - phdr->p_filesz);
  return phdr->p_vaddr + phdr->p_memsz; /* Potential program break address */
}

static int
process_load_phdrs (VFSInode *inode, Array *segments, Elf32_Off phoff,
		    Elf32_Half phentsize, Elf32_Half phnum)
{
  Elf32_Phdr *phdr;
  Elf32_Half i;
  uint32_t pbreak;
  int ret;

  phdr = kmalloc (sizeof (Elf32_Phdr));
  if (unlikely (phdr == NULL))
    return -1;

  for (i = 0; i < phnum; i++)
    {
      if (vfs_read (inode, phdr, sizeof (Elf32_Phdr),
		    phoff + phentsize * i) < 0)
	{
	  kfree (phdr);
	  return -1;
	}
      if (phdr->p_type == PT_LOAD)
	{
	  /* Load the segment into memory */
	  ret = process_load_segment (inode, segments, phdr);
	  if (ret < 0)
	    {
	      kfree (phdr);
	      return ret;
	    }

	  /* Load headers are required to be in ascending order based on
	     virtual address, so the program break should be set to the
	     address of the last segment. This does not account for
	     overlapping segments */
	  pbreak = ret;
	}
    }

  kfree (phdr);
  return pbreak;
}

static void
process_segment_free (void *elem, void *data)
{
  ProcessSegment *segment = elem;
  uint32_t addr;
  for (addr = segment->ps_addr; addr < segment->ps_addr + segment->ps_size;
       addr += PAGE_SIZE)
    {
      uint32_t paddr = get_paddr (data, (void *) addr);
      if (paddr != 0)
	free_page (paddr);
    }
  kfree (segment);
}

int
process_exec (VFSInode *inode, uint32_t *entry)
{
  Elf32_Ehdr *ehdr;
  Array *segments;
  Process *proc;
  int ret;
  int i;
  __asm__ volatile ("cli");

  segments = array_new (PROCESS_SEGMENT_LIMIT);
  if (unlikely (segments == NULL))
    {
      ret = -ENOMEM;
      goto end;
    }

  /* Read ELF header */
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

  /* Check for 32-bit little-endian ELF executable for correct machine */
  if (ehdr->e_ident[EI_CLASS] != ELFCLASS32
      || ehdr->e_ident[EI_DATA] != ELFDATA2LSB || ehdr->e_type != ET_EXEC
      || ehdr->e_machine != MACHTYPE)
    {
      ret = -ENOEXEC;
      goto end;
    }

  ret = process_load_phdrs (inode, segments, ehdr->e_phoff, ehdr->e_phentsize,
			    ehdr->e_phnum);
  if (ret < 0)
    goto end;
  proc = &process_table[task_getpid ()];
  proc->p_break = ret;
  if (proc->p_break & (PAGE_SIZE - 1))
    {
      proc->p_break &= 0xfffff000;
      proc->p_break += PAGE_SIZE;
    }

  /* Clear all open file descriptors and open std streams */
  for (i = 0; i < PROCESS_FILE_LIMIT; i++)
    {
      VFSInode *fdi = proc->p_files[i].pf_inode;
      if (fdi != NULL)
	vfs_unref_inode (fdi);
    }
  if (process_setup_std_streams (task_getpid ()) != 0)
    goto end;

  if (entry != NULL)
    *entry = ehdr->e_entry;
  kfree (ehdr);
  __asm__ volatile ("sti");
  return 0;

 end:
  array_destroy (segments, process_segment_free, curr_page_dir);
  kfree (ehdr);
  __asm__ volatile ("sti");
  return ret;
}

void
process_free (pid_t pid)
{
  Process *proc;
  struct rusage *prusage;
  pid_t ppid;
  int i;
  if (pid >= PROCESS_LIMIT || process_table[pid].p_task == NULL)
    return;
  proc = &process_table[pid];
  array_destroy (proc->p_segments, process_segment_free, proc->p_task->t_pgdir);
  ppid = proc->p_task->t_ppid;
  task_free ((ProcessTask *) proc->p_task);
  proc->p_task = NULL;

  /* Add self rusage values to parent's child rusage */
  prusage = &process_table[ppid].p_cusage;
  prusage->ru_utime.tv_sec += proc->p_rusage.ru_utime.tv_sec +
    proc->p_cusage.ru_utime.tv_sec;
  prusage->ru_utime.tv_usec += proc->p_rusage.ru_utime.tv_usec +
    proc->p_cusage.ru_utime.tv_usec;
  prusage->ru_utime.tv_sec += prusage->ru_utime.tv_usec / 1000000;
  prusage->ru_utime.tv_usec %= 1000000;
  prusage->ru_stime.tv_sec += proc->p_rusage.ru_stime.tv_sec +
    proc->p_cusage.ru_stime.tv_sec;
  prusage->ru_stime.tv_usec += proc->p_rusage.ru_stime.tv_usec +
    proc->p_cusage.ru_stime.tv_usec;
  prusage->ru_stime.tv_sec += prusage->ru_stime.tv_usec / 1000000;
  prusage->ru_stime.tv_usec %= 1000000;
  prusage->ru_maxrss += proc->p_rusage.ru_maxrss + proc->p_cusage.ru_maxrss;
  prusage->ru_ixrss += proc->p_rusage.ru_ixrss + proc->p_cusage.ru_ixrss;
  prusage->ru_idrss += proc->p_rusage.ru_idrss + proc->p_cusage.ru_idrss;
  prusage->ru_isrss += proc->p_rusage.ru_isrss + proc->p_cusage.ru_isrss;
  prusage->ru_minflt += proc->p_rusage.ru_minflt + proc->p_cusage.ru_minflt;
  prusage->ru_majflt += proc->p_rusage.ru_majflt + proc->p_cusage.ru_majflt;
  prusage->ru_nswap += proc->p_rusage.ru_nswap + proc->p_cusage.ru_nswap;
  prusage->ru_inblock += proc->p_rusage.ru_inblock + proc->p_cusage.ru_inblock;
  prusage->ru_oublock += proc->p_rusage.ru_oublock + proc->p_cusage.ru_oublock;
  prusage->ru_msgsnd += proc->p_rusage.ru_msgsnd + proc->p_cusage.ru_msgsnd;
  prusage->ru_msgrcv += proc->p_rusage.ru_msgrcv + proc->p_cusage.ru_msgrcv;
  prusage->ru_nsignals += proc->p_rusage.ru_nsignals +
    proc->p_cusage.ru_nsignals;
  prusage->ru_nvcsw += proc->p_rusage.ru_nvcsw + proc->p_cusage.ru_nvcsw;
  prusage->ru_nivcsw += proc->p_rusage.ru_nivcsw + proc->p_cusage.ru_nivcsw;

  /* Reset process data */
  vfs_unref_inode (proc->p_cwd);
  proc->p_cwd = NULL;
  proc->p_break = 0;
  proc->p_pause = 0;
  proc->p_sig = 0;
  proc->p_term = 0;
  proc->p_waitstat = 0;
  proc->p_uid = 0;
  proc->p_euid = 0;
  proc->p_suid = 0;
  proc->p_gid = 0;
  proc->p_egid = 0;
  proc->p_sgid = 0;
  memset (&proc->p_rusage, 0, sizeof (struct rusage));
  memset (&proc->p_cusage, 0, sizeof (struct rusage));

  /* Clear all open file descriptors */
  for (i = 0; i < PROCESS_FILE_LIMIT; i++)
    {
      if (proc->p_files[i].pf_inode != NULL)
        vfs_unref_inode (proc->p_files[i].pf_inode);
      if (proc->p_files[i].pf_termios != NULL)
	kfree (proc->p_files[i].pf_termios);
    }
  memset (proc->p_files, 0, sizeof (ProcessFile) * PROCESS_FILE_LIMIT);

  /* Clear all signal handlers */
  memset (proc->p_sigactions, 0, sizeof (struct sigaction) * NR_signals);
  memset (&proc->p_sigblocked, 0, sizeof (sigset_t));
  memset (&proc->p_sigpending, 0, sizeof (sigset_t));
}

int
process_setup_std_streams (pid_t pid)
{
  struct termios *term;
  Process *proc;
  if (pid >= PROCESS_LIMIT || process_table[pid].p_task == NULL)
    return -EINVAL;
  proc = &process_table[pid];
  if (proc->p_files[STDIN_FILENO].pf_inode != NULL
      || proc->p_files[STDOUT_FILENO].pf_inode != NULL
      || proc->p_files[STDERR_FILENO].pf_inode != NULL)
    return -EINVAL; /* File descriptors for std streams are used */

  /* Create stdin */
  proc->p_files[STDIN_FILENO].pf_inode = vfs_alloc_inode (&vga_tty_sb);
  proc->p_files[STDIN_FILENO].pf_inode->vi_private =
    device_lookup (1, STDIN_FILENO);
  proc->p_files[STDIN_FILENO].pf_mode = O_RDONLY;
  proc->p_files[STDIN_FILENO].pf_flags = 0;
  proc->p_files[STDIN_FILENO].pf_offset = 0;

  term = kmalloc (sizeof (struct termios));
  if (term == NULL)
    return -ENOMEM;
  term->c_iflag = DEFAULT_IFLAG;
  term->c_oflag = DEFAULT_OFLAG;
  term->c_cflag = DEFAULT_CFLAG;
  term->c_lflag = DEFAULT_LFLAG;
  proc->p_files[STDIN_FILENO].pf_termios = term;

  /* Create stdout */
  proc->p_files[STDOUT_FILENO].pf_inode = vfs_alloc_inode (&vga_tty_sb);
  proc->p_files[STDOUT_FILENO].pf_inode->vi_private =
    device_lookup (1, STDOUT_FILENO);
  proc->p_files[STDOUT_FILENO].pf_mode = O_WRONLY | O_APPEND;
  proc->p_files[STDOUT_FILENO].pf_flags = 0;
  proc->p_files[STDOUT_FILENO].pf_offset = 0;

  term = kmalloc (sizeof (struct termios));
  if (term == NULL)
    return -ENOMEM;
  term->c_iflag = DEFAULT_IFLAG;
  term->c_oflag = DEFAULT_OFLAG;
  term->c_cflag = DEFAULT_CFLAG;
  term->c_lflag = DEFAULT_LFLAG;
  proc->p_files[STDOUT_FILENO].pf_termios = term;

  /* Create stderr */
  proc->p_files[STDERR_FILENO].pf_inode = vfs_alloc_inode (&vga_tty_sb);
  proc->p_files[STDERR_FILENO].pf_inode->vi_private =
    device_lookup (1, STDERR_FILENO);
  proc->p_files[STDERR_FILENO].pf_mode = O_WRONLY | O_APPEND;
  proc->p_files[STDERR_FILENO].pf_flags = 0;
  proc->p_files[STDERR_FILENO].pf_offset = 0;

  term = kmalloc (sizeof (struct termios));
  if (term == NULL)
    return -ENOMEM;
  term->c_iflag = DEFAULT_IFLAG;
  term->c_oflag = DEFAULT_OFLAG;
  term->c_cflag = DEFAULT_CFLAG;
  term->c_lflag = DEFAULT_LFLAG;
  proc->p_files[STDERR_FILENO].pf_termios = term;
  return 0;
}

uint32_t
process_set_break (uint32_t addr)
{
  Process *proc = &process_table[task_getpid ()];
  uint32_t i;

  /* Fail if address is behind current break or is too high */
  if (addr < proc->p_break || addr >= PROCESS_BREAK_LIMIT)
    return proc->p_break;

  /* Page align starting address */
  i = proc->p_break;
  if (i & (PAGE_SIZE - 1))
    {
      i &= 0xfffff000;
      i += PAGE_SIZE;
    }

  /* Map pages until the new program break is reached */
  for (; i <= addr; i += PAGE_SIZE)
    {
      uint32_t paddr = alloc_page ();
      if (paddr == 0)
	return proc->p_break;
      map_page (curr_page_dir, paddr, i, PAGE_FLAG_USER | PAGE_FLAG_WRITE);
#ifdef INVLPG_SUPPORT
      vm_page_inval (paddr);
#endif
    }
#ifndef INVLPG_SUPPORT
  vm_tlb_reset ();
#endif

  memset ((void *) proc->p_break, 0, addr - proc->p_break);
  proc->p_break = addr;
  return proc->p_break;
}

int
process_terminated (pid_t pid)
{
  return process_table[pid].p_term;
}

void
process_handle_signal (void)
{
  Process *proc = &process_table[task_getpid ()];
  if (proc->p_sig != 0)
    {
      struct sigaction *sigaction = &proc->p_sigactions[proc->p_sig];
      if (sigaction->sa_flags & SA_SIGINFO)
	; /* TODO Call sigaction->sa_sigaction */
      else
	sigaction->sa_handler (proc->p_sig);
      proc->p_pause = 0;
      proc->p_sig = 0;
    }
}
