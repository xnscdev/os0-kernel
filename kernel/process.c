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

#include <bits/mman.h>
#include <bits/mount.h>
#include <libk/libk.h>
#include <sys/process.h>
#include <sys/wait.h>
#include <video/vga.h>
#include <vm/heap.h>
#include <vm/paging.h>

Process process_table[PROCESS_LIMIT];
ProcessFile process_fd_table[PROCESS_SYS_FILE_LIMIT];
ProcessFile *process_fd_next = process_fd_table;
void *process_signal_handler;
int process_signal;
void *signal_return_addr;

extern int exit_task;

static int
process_load_segment (VFSInode *inode, SortedArray *mregions, Elf32_Phdr *phdr)
{
  uint32_t addr;
  ProcessMemoryRegion *segment;
  int prot = 0;

  for (addr = phdr->p_vaddr & 0xfffff000; addr < phdr->p_vaddr + phdr->p_memsz;
       addr += PAGE_SIZE)
    {
      uint32_t paddr = alloc_page ();
      if (unlikely (paddr == 0))
	return -ENOMEM;
      /* Map with write permission to copy data first */
      map_page (curr_page_dir, paddr, addr, PAGE_FLAG_USER | PAGE_FLAG_WRITE);
      vm_page_inval (addr);
    }
  vm_tlb_reset_386 ();

  if (phdr->p_flags & PF_R)
    prot |= PROT_READ;
  if (phdr->p_flags & PF_W)
    prot |= PROT_WRITE;
  if (phdr->p_flags & PF_X)
    prot |= PROT_EXEC;

  segment = kmalloc (sizeof (ProcessMemoryRegion));
  if (segment == NULL)
    return -ENOMEM;
  segment->pm_base = phdr->p_vaddr & 0xfffff000;
  segment->pm_len = ((phdr->p_memsz - 1) | (PAGE_SIZE - 1)) + 1;
  segment->pm_prot = prot;
  segment->pm_flags = MAP_PRIVATE | MAP_ANONYMOUS;
  segment->pm_ino = NULL;
  segment->pm_offset = 0;
  sorted_array_insert (mregions, segment);

  if (phdr->p_filesz > 0)
    {
      if (vfs_read (inode, (void *) phdr->p_vaddr, phdr->p_filesz,
		    phdr->p_offset) < 0)
	return -EIO;
    }
  memset ((void *) (phdr->p_vaddr + phdr->p_filesz), 0,
	  phdr->p_memsz - phdr->p_filesz);

  if (!(phdr->p_flags & PF_W))
    {
      /* Segment is not writable, remap all pages without write permission */
      for (addr = phdr->p_vaddr & 0xfffff000;
	   addr < phdr->p_vaddr + phdr->p_memsz; addr += PAGE_SIZE)
	{
	  uint32_t paddr = get_paddr (curr_page_dir, (void *) addr);
	  map_page (curr_page_dir, paddr, addr, PAGE_FLAG_USER);
	  vm_page_inval (addr);
	}
      vm_tlb_reset_386 ();
    }
  return phdr->p_vaddr + phdr->p_memsz; /* Potential program break address */
}

static int
process_load_phdrs (VFSInode *inode, SortedArray *mregions, Elf32_Ehdr *ehdr,
		    DynamicLinkInfo *dlinfo)
{
  Elf32_Phdr *phdr;
  Elf32_Half i;
  uint32_t pbreak;
  int ret;

  phdr = kmalloc (sizeof (Elf32_Phdr));
  if (unlikely (phdr == NULL))
    return -ENOMEM;

  for (i = 0; i < ehdr->e_phnum; i++)
    {
      ret = vfs_read (inode, phdr, sizeof (Elf32_Phdr),
		      ehdr->e_phoff + ehdr->e_phentsize * i);
      if (ret < 0)
	{
	  kfree (phdr);
	  return ret;
	}
      switch (phdr->p_type)
	{
	case PT_LOAD:
	  /* Load the segment into memory */
	  ret = process_load_segment (inode, mregions, phdr);
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
	  if (phdr->p_offset == 0)
	    dlinfo->dl_loadbase = (void *) phdr->p_vaddr;
	  break;
	case PT_INTERP:
	  /* Don't read the interpreter path from the executable, just set the
	     pointer to where it should be. As long as a load header loads
	     a segment containing the interpreter path into memory, the
	     pointer will point to a valid string. */
	  dlinfo->dl_active = 1;
	  dlinfo->dl_interp = (char *) phdr->p_vaddr;
	  break;
	case PT_DYNAMIC:
	  /* As explained with PT_INTERP, assume the structure will be valid
	     as long as a load header overlaps with the dynamic header */
	  dlinfo->dl_active = 1;
	  dlinfo->dl_dynamic = (Elf32_Dyn *) phdr->p_vaddr;
	  break;
	}
    }

  kfree (phdr);
  return pbreak;
}

int
process_exec (VFSInode *inode, uint32_t *entry, char *const *argv,
	      char *const *envp, DynamicLinkInfo *dlinfo)
{
  Elf32_Ehdr *ehdr = NULL;
  SortedArray *mregions;
  Process *proc;
  char hashbang[2];
  pid_t pid = task_getpid ();
  int ret;
  int i;

  /* Check for a hashbang */
  if (vfs_read (inode, hashbang, 2, 0) < 0)
    return -EIO;
  if (hashbang[0] == '#' && hashbang[1] == '!')
    return process_exec_sh (inode, entry, argv, envp, dlinfo);

  ret = task_setup_exec (argv, envp);
  if (ret < 0)
    return ret;

  /* Clear existing process data */
  process_clear (pid, 1);

  /* Close all close-on-exec file descriptors */
  proc = &process_table[pid];
  for (i = 0; i < PROCESS_FILE_LIMIT; i++)
    {
      if (proc->p_fdflags[i] & FD_CLOEXEC)
	process_free_fd (proc, i);
    }

  mregions = sorted_array_new (PROCESS_MMAP_LIMIT, process_mregion_cmp);
  if (unlikely (mregions == NULL))
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
      || ehdr->e_ident[EI_DATA] != ELFDATA2LSB
      || ehdr->e_type != ET_EXEC
      || ehdr->e_machine != MACHTYPE)
    {
      ret = -ENOEXEC;
      goto end;
    }

  /* Load program headers and set entry point */
  ret = process_load_phdrs (inode, mregions, ehdr, dlinfo);
  if (ret < 0)
    goto end;
  *entry = ehdr->e_entry;

  /* Setup program break */
  proc->p_break = ret;
  if (proc->p_break & (PAGE_SIZE - 1))
    {
      proc->p_break &= 0xfffff000;
      proc->p_break += PAGE_SIZE;
    }
  proc->p_initbreak = proc->p_break;

  if (dlinfo->dl_active)
    {
      /* Require a program header to have loaded the ELF header into memory,
	 and also ensure that there is a dynamic section */
      if (dlinfo->dl_loadbase == NULL || dlinfo->dl_dynamic == NULL)
	{
	  ret = -ENOEXEC;
	  goto end;
	}

      /* Load interpreter into memory */
      dlinfo->dl_entry = (void *) *entry;
      ret = rtld_setup (ehdr, mregions, entry, dlinfo);
      if (ret < 0)
	goto end;

      /* The saved entry point address needs to be changed to a helper
	 function instead, since we need to tell the dynamic linker where
	 the real executable entry point is. */
      dlinfo->dl_rtldentry = (void *) *entry;
      *entry = (uint32_t) rtld_setup_dynamic_linker;
    }
  else
    process_remap_segments (dlinfo->dl_loadbase, mregions);

  proc->p_mregions = mregions;
  kfree (ehdr);

  /* If the executable is setuid/setgid and the filesystem is not mounted with
     nosuid, change the process UID/GID */
  if (!(inode->vi_sb->sb_mntflags & MS_NOSUID))
    {
      if (inode->vi_mode & S_ISUID)
	proc->p_euid = inode->vi_uid;
      if (inode->vi_mode & S_ISGID)
	proc->p_egid = inode->vi_gid;
    }
  return 0;

 end:
  sorted_array_destroy (mregions, process_region_free, curr_page_dir);
  kfree (ehdr);
  return ret;
}

int
process_exec_sh (VFSInode *inode, uint32_t *entry, char *const *argv,
		 char *const *envp, DynamicLinkInfo *dlinfo)
{
  char buffer[127];
  char *new_argv[256];
  char *ptr;
  VFSInode *new_inode;
  size_t nargs;
  int ret;
  int leave = 0;
  int i = 0;
  if (vfs_read (inode, buffer, 127, 2) < 0)
    return -EIO;

  /* Stop parsing hashbang line after newline */
  ptr = strchr (buffer, '\n');
  if (ptr != NULL)
    *ptr = '\0';

  /* Get number of arguments */
  for (nargs = 0; argv[nargs] != NULL; nargs++)
    ;

  /* Prepend contents of hashbang line to argv */
  ptr = buffer;
  while (!leave)
    {
      char *end;
      while (isspace (*ptr))
	ptr++;
      for (end = ptr; *end != '\0' && !isspace (*end); end++)
	;
      if (*end == '\0')
	leave = 1;
      else
	*end = '\0';
      if (i >= 255 - nargs)
	return -EIO;
      new_argv[i++] = ptr;
      ptr = end + 1;
    }
  memcpy (&new_argv[i], argv, sizeof (void *) * (nargs + 1));
  ret = vfs_open_file (&new_inode, new_argv[0], 1);
  if (ret < 0)
    return ret;

  /* TODO Replace new_argv[1] with the full path to the program if needed */

  ret = process_exec (new_inode, entry, new_argv, envp, dlinfo);
  vfs_unref_inode (new_inode);
  return ret;
}

int
process_valid (pid_t pid)
{
  return pid >= 0 && pid < PROCESS_LIMIT && process_table[pid].p_task != NULL;
}

void
process_clear (pid_t pid, int partial)
{
  Process *proc = &process_table[pid];
  uint32_t vaddr;
  sorted_array_destroy (proc->p_mregions, process_region_free,
			proc->p_task->t_pgdir);
  proc->p_mregions = NULL;

  /* Free process heap */
  for (vaddr = proc->p_initbreak; vaddr < proc->p_break; vaddr += PAGE_SIZE)
    {
      uint32_t paddr = get_paddr (proc->p_task->t_pgdir, (void *) vaddr);
      if (paddr != 0)
	free_page (paddr);
    }

  if (partial)
    {
      if (proc->p_task->t_pgcopied)
	page_dir_exec_free (proc->p_task->t_pgdir);
    }
  else
    {
      /* Remove scheduler task */
      task_free ((ProcessTask *) proc->p_task);
      proc->p_task = NULL;
    }

  /* Reset process data */
  proc->p_initbreak = 0;
  proc->p_break = 0;
  proc->p_maxbreak = 0;
  proc->p_maxfds = 0;
  proc->p_pause = 0;
  proc->p_sig = 0;
  proc->p_term = 0;
  proc->p_waitstat = 0;
  proc->p_umask = 0;
  proc->p_uid = 0;
  proc->p_euid = 0;
  proc->p_suid = 0;
  proc->p_gid = 0;
  proc->p_egid = 0;
  proc->p_sgid = 0;
  proc->p_pgid = 0;
  proc->p_sid = 0;
  memset (&proc->p_addrspace, 0, sizeof (struct rlimit));
  memset (&proc->p_coresize, 0, sizeof (struct rlimit));
  memset (&proc->p_cputime, 0, sizeof (struct rlimit));
  memset (&proc->p_filesize, 0, sizeof (struct rlimit));
  memset (&proc->p_memlock, 0, sizeof (struct rlimit));
  memset (&proc->p_rusage, 0, sizeof (struct rusage));
  memset (&proc->p_cusage, 0, sizeof (struct rusage));

  /* Clear all signal handlers and info */
  process_clear_sighandlers (pid);
  memset (&proc->p_sigblocked, 0, sizeof (sigset_t));
  memset (&proc->p_sigpending, 0, sizeof (sigset_t));
  memset (&proc->p_siginfo, 0, sizeof (siginfo_t));
}

void
process_free (pid_t pid)
{
  Process *proc = &process_table[pid];
  pid_t ppid;
  pid_t temp;
  int i;
  if (!process_valid (pid))
    return;
  ppid = proc->p_task->t_ppid;

  /* Decrement child process count of all parent processes */
  temp = ppid;
  while (temp != 0)
    {
      process_table[temp].p_children--;
      temp = process_table[temp].p_task->t_ppid;
    }
  process_table[0].p_children--;

  /* Add self rusage values to parent's child rusage */
  process_add_rusage (&process_table[ppid].p_cusage, proc);

  /* Clear process data */
  process_clear (pid, 0);
  vfs_unref_inode (proc->p_cwd);
  kfree (proc->p_cwdpath);

  /* Clear all open file descriptors */
  for (i = 0; i < PROCESS_FILE_LIMIT; i++)
    process_free_fd (proc, i);
  memset (proc->p_fdflags, 0, sizeof (int) * PROCESS_FILE_LIMIT);
}

void
process_region_free (void *elem, void *data)
{
  ProcessMemoryRegion *region = elem;
  uint32_t addr;
  for (addr = region->pm_base; addr < region->pm_base + region->pm_len;
       addr += PAGE_SIZE)
    {
      uint32_t paddr = get_paddr (data, (void *) addr);
      if (paddr != 0)
	free_page (paddr);
      unmap_page (data, addr);
      vm_page_inval (addr);
    }
  vm_tlb_reset_386 ();
  kfree (region);
}

int
process_setup_std_streams (pid_t pid)
{
  Process *proc = &process_table[pid];
  if (proc->p_files[STDIN_FILENO] != NULL
      || proc->p_files[STDOUT_FILENO] != NULL
      || proc->p_files[STDERR_FILENO] != NULL)
    return -EINVAL; /* File descriptors for std streams are used */

  /* Create stdin */
  assert (process_alloc_fd (proc, STDIN_FILENO) == STDIN_FILENO);
  proc->p_files[STDIN_FILENO]->pf_inode = vfs_alloc_inode (&tty_sb);
  proc->p_files[STDIN_FILENO]->pf_inode->vi_private =
    device_lookup (1, STDIN_FILENO);
  proc->p_files[STDIN_FILENO]->pf_path = strdup ("/dev/stdin");
  if (proc->p_files[STDIN_FILENO]->pf_path == NULL)
    return -ENOMEM;
  proc->p_files[STDIN_FILENO]->pf_mode = O_RDONLY;
  proc->p_files[STDIN_FILENO]->pf_offset = 0;

  /* Create stdout */
  assert (process_alloc_fd (proc, STDOUT_FILENO) == STDOUT_FILENO);
  proc->p_files[STDOUT_FILENO]->pf_inode = vfs_alloc_inode (&tty_sb);
  proc->p_files[STDOUT_FILENO]->pf_inode->vi_private =
    device_lookup (1, STDOUT_FILENO);
  proc->p_files[STDOUT_FILENO]->pf_path = strdup ("/dev/stdout");
  if (proc->p_files[STDOUT_FILENO]->pf_path == NULL)
    return -ENOMEM;
  proc->p_files[STDOUT_FILENO]->pf_mode = O_WRONLY | O_APPEND;
  proc->p_files[STDOUT_FILENO]->pf_offset = 0;

  /* Create stderr */
  assert (process_alloc_fd (proc, STDERR_FILENO) == STDERR_FILENO);
  proc->p_files[STDERR_FILENO]->pf_inode = vfs_alloc_inode (&tty_sb);
  proc->p_files[STDERR_FILENO]->pf_inode->vi_private =
    device_lookup (1, STDERR_FILENO);
  proc->p_files[STDERR_FILENO]->pf_path = strdup ("/dev/stderr");
  if (proc->p_files[STDERR_FILENO]->pf_path == NULL)
    return -ENOMEM;
  proc->p_files[STDERR_FILENO]->pf_mode = O_WRONLY | O_APPEND;
  proc->p_files[STDERR_FILENO]->pf_offset = 0;
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

  /* Map pages until the new program break is reached */
  for (i = ((proc->p_break - 1) | (PAGE_SIZE - 1)) + 1; i < addr;
       i += PAGE_SIZE)
    {
      uint32_t paddr = alloc_page ();
      if (paddr == 0)
	return proc->p_break;
      map_page (curr_page_dir, paddr, i, PAGE_FLAG_USER | PAGE_FLAG_WRITE);
      vm_page_inval (i);
    }
  vm_tlb_reset_386 ();
  memset ((void *) proc->p_break, 0, addr - proc->p_break);

  proc->p_break = addr;
  return proc->p_break;
}

int
process_mregion_cmp (const void *a, const void *b)
{
  uint32_t ba = ((const ProcessMemoryRegion *) a)->pm_base;
  uint32_t bb = ((const ProcessMemoryRegion *) b)->pm_base;
  return ba < bb;
}

void
process_add_rusage (struct rusage *usage, const Process *proc)
{
  usage->ru_utime.tv_sec += proc->p_rusage.ru_utime.tv_sec +
    proc->p_cusage.ru_utime.tv_sec;
  usage->ru_utime.tv_usec += proc->p_rusage.ru_utime.tv_usec +
    proc->p_cusage.ru_utime.tv_usec;
  usage->ru_utime.tv_sec += usage->ru_utime.tv_usec / 1000000;
  usage->ru_utime.tv_usec %= 1000000;
  usage->ru_stime.tv_sec += proc->p_rusage.ru_stime.tv_sec +
    proc->p_cusage.ru_stime.tv_sec;
  usage->ru_stime.tv_usec += proc->p_rusage.ru_stime.tv_usec +
    proc->p_cusage.ru_stime.tv_usec;
  usage->ru_stime.tv_sec += usage->ru_stime.tv_usec / 1000000;
  usage->ru_stime.tv_usec %= 1000000;
  usage->ru_maxrss += proc->p_rusage.ru_maxrss + proc->p_cusage.ru_maxrss;
  usage->ru_ixrss += proc->p_rusage.ru_ixrss + proc->p_cusage.ru_ixrss;
  usage->ru_idrss += proc->p_rusage.ru_idrss + proc->p_cusage.ru_idrss;
  usage->ru_isrss += proc->p_rusage.ru_isrss + proc->p_cusage.ru_isrss;
  usage->ru_minflt += proc->p_rusage.ru_minflt + proc->p_cusage.ru_minflt;
  usage->ru_majflt += proc->p_rusage.ru_majflt + proc->p_cusage.ru_majflt;
  usage->ru_nswap += proc->p_rusage.ru_nswap + proc->p_cusage.ru_nswap;
  usage->ru_inblock += proc->p_rusage.ru_inblock + proc->p_cusage.ru_inblock;
  usage->ru_oublock += proc->p_rusage.ru_oublock + proc->p_cusage.ru_oublock;
  usage->ru_msgsnd += proc->p_rusage.ru_msgsnd + proc->p_cusage.ru_msgsnd;
  usage->ru_msgrcv += proc->p_rusage.ru_msgrcv + proc->p_cusage.ru_msgrcv;
  usage->ru_nsignals += proc->p_rusage.ru_nsignals +
    proc->p_cusage.ru_nsignals;
  usage->ru_nvcsw += proc->p_rusage.ru_nvcsw + proc->p_cusage.ru_nvcsw;
  usage->ru_nivcsw += proc->p_rusage.ru_nivcsw + proc->p_cusage.ru_nivcsw;
}

void
process_remap_segments (void *base, SortedArray *mregions)
{
  uint32_t vaddr;
  int i;
  for (i = 0; i < mregions->sa_size; i++)
    {
      ProcessMemoryRegion *segment = mregions->sa_elems[i];
      if (!(segment->pm_prot & PROT_WRITE))
	{
	  /* Segment is not writable, remap segment memory region without
	     write permission */
	  for (vaddr = LD_SO_LOAD_ADDR;
	       vaddr < LD_SO_LOAD_ADDR + segment->pm_len; vaddr += PAGE_SIZE)
	    {
	      uint32_t paddr = get_paddr (curr_page_dir, (void *) vaddr);
	      map_page (curr_page_dir, paddr, vaddr, PAGE_FLAG_USER);
	      vm_page_inval (vaddr);
	    }
	}
    }
  vm_tlb_reset_386 ();
}

int
process_alloc_fd (Process *proc, int fd)
{
  ProcessFile *file;
  int nfd;
  for (file = process_fd_next; file < process_fd_table + PROCESS_SYS_FILE_LIMIT;
       file++)
    {
      if (file->pf_refcnt == 0)
	goto found;
    }
  return -ENFILE; /* System file descriptor limit reached */

 found:
  for (nfd = fd; nfd < PROCESS_FILE_LIMIT; nfd++)
    {
      if (proc->p_files[nfd] == NULL)
	break;
    }
  if (unlikely (nfd == PROCESS_FILE_LIMIT))
    return -EMFILE; /* Process file descriptor limit reached */
  proc->p_files[nfd] = file;
  proc->p_files[nfd]->pf_refcnt = 1;
  process_fd_next = file + 1; /* Start the next search after this nfd */
  return nfd;
}

int
process_free_fd (Process *proc, int fd)
{
  ProcessFile *file = proc->p_files[fd];
  if (file == NULL)
    return -EBADF;
  proc->p_files[fd] = NULL;
  if (--file->pf_refcnt == 0)
    {
      vfs_unref_inode (file->pf_inode);
      file->pf_inode = NULL;
      kfree (file->pf_path);
      file->pf_path = NULL;
      file->pf_mode = 0;
      file->pf_offset = 0;
    }
  if (file < process_fd_next)
    process_fd_next = file; /* Start the next search at this fd */
  return 0;
}

int
process_terminated (pid_t pid)
{
  return process_table[pid].p_term;
}

int
process_send_signal (pid_t pid, int sig)
{
  int exit = sig == SIGKILL;
  int stop = sig == SIGSTOP;
  struct sigaction *sigaction = &process_table[pid].p_sigactions[sig];
  task_switch_enabled = 0;
  if (!exit)
    {
      if (sigismember (&process_table[pid].p_sigblocked, sig))
	{
	  /* Set signal as pending */
	  sigaddset (&process_table[pid].p_sigpending, sig);
	  task_switch_enabled = 1;
	  return 0;
	}
      else if (!(sigaction->sa_flags & SA_SIGINFO)
	       && sigaction->sa_handler == SIG_IGN)
	{
	  /* Ignore signal */
	  task_switch_enabled = 1;
	  return 0;
	}
    }

  switch (sig)
    {
    case SIGABRT:
    case SIGALRM:
    case SIGBUS:
    case SIGFPE:
    case SIGHUP:
    case SIGILL:
    case SIGINT:
    case SIGIO:
    case SIGPIPE:
    case SIGPROF:
    case SIGPWR:
    case SIGQUIT:
    case SIGSEGV:
    case SIGSTKFLT:
    case SIGSYS:
    case SIGTERM:
    case SIGTRAP:
    case SIGUSR1:
    case SIGUSR2:
    case SIGVTALRM:
    case SIGXCPU:
    case SIGXFSZ:
      if (!(sigaction->sa_flags & SA_SIGINFO)
	  && sigaction->sa_handler == SIG_DFL)
	exit = 1; /* Default action is to terminate process */
      break;
    case SIGTTIN:
    case SIGTTOU:
    case SIGTSTP:
      if (!(sigaction->sa_flags & SA_SIGINFO)
	  && sigaction->sa_handler == SIG_DFL)
        stop = 1; /* Default action is to stop process */
      break;
    case SIGCONT:
      if (WIFSTOPPED (process_table[pid].p_waitstat))
	{
	  /* Continue process if stopped */
	  process_table[pid].p_term = 0;
	  process_table[pid].p_waitstat = 0;
	}
      break;
    }

  if (exit)
    {
      pid_t ppid = process_table[pid].p_task->t_ppid;
      process_table[pid].p_term = 1;
      process_table[pid].p_waitstat = sig;
      exit_task = pid;
      if (ppid != 0)
	process_send_signal (ppid, SIGCHLD);
    }
  else if (stop)
    {
      process_table[pid].p_term = 1;
      process_table[pid].p_waitstat = (sig << 8) | 0x7f;
      if (!(sigaction->sa_flags & SA_NOCLDSTOP))
	{
	  pid_t ppid = process_table[pid].p_task->t_ppid;
	  if (ppid != 0)
	    process_send_signal (ppid, SIGCHLD);
	}
    }

  process_table[pid].p_sig = sig;
  task_switch_enabled = 1;
  if (pid == task_getpid ())
    task_yield ();
  return 0;
}

void
process_clear_sighandlers (pid_t pid)
{
  memset (process_table[pid].p_sigactions, 0, sizeof (struct sigaction) * NSIG);
}

void
process_handle_signal (void)
{
  Process *proc = &process_table[task_getpid ()];
  struct sigaction *sigaction;
  sigset_t mask;
  uint32_t paddr;
  int i;
  if (proc->p_sig == 0)
    {
      /* Check if a previously pending signal has been unblocked */
      for (i = SIGHUP; i <= SIGSYS; i++)
	{
	  if (!sigismember (&proc->p_sigblocked, i)
	      && sigismember (&proc->p_sigpending, i))
	    {
	      proc->p_sig = i;
	      sigdelset (&proc->p_sigpending, i);
	      break;
	    }
	}
    }
  process_signal = proc->p_sig;
  if (proc->p_sig == 0)
    return;
  sigaction = &proc->p_sigactions[proc->p_sig];
  memcpy (&mask, &proc->p_sigblocked, sizeof (sigset_t));

  /* Remove from pending signals */
  sigdelset (&proc->p_sigpending, proc->p_sig);

  /* Block signals specified in sigaction signal mask, and block delivered
     signal unless SA_NODEFER is active */
  for (i = 0; i < sizeof (sigset_t); i++)
    proc->p_sigblocked.sig[i] |= sigaction->sa_mask.sig[i];
  if (!(sigaction->sa_flags & SA_NODEFER))
    sigaddset (&proc->p_sigblocked, proc->p_sig);

  /* Map process siginfo to new user-mode page first */
  paddr = alloc_page ();
  map_page (curr_page_dir, paddr, TASK_SIGINFO_PAGE,
	    PAGE_FLAG_USER | PAGE_FLAG_WRITE);
  vm_page_inval_386 (TASK_SIGINFO_PAGE);
  memcpy ((void *) TASK_SIGINFO_PAGE, &proc->p_siginfo, sizeof (siginfo_t));

  process_signal_handler = sigaction->sa_flags & SA_SIGINFO ?
    (void *) sigaction->sa_sigaction : (void *) sigaction->sa_handler;
}

void
process_signal_handled (void)
{
  Process *proc = &process_table[task_getpid ()];
  uint32_t paddr = get_paddr (curr_page_dir, (void *) TASK_SIGINFO_PAGE);
  free_page (paddr);
  proc->p_sig = 0;
  proc->p_pause = 0;
  memset (&proc->p_siginfo, 0, sizeof (siginfo_t));
}
