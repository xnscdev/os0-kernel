/*************************************************************************
 * sysconf.h -- This file is part of OS/0.                               *
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

#ifndef _BITS_SYSCONF_H
#define _BITS_SYSCONF_H

enum
{
  _SC_AIO_LISTIO_MAX = 1,
  _SC_AIO_MAX,
  _SC_AIO_PRIO_DELTA_MAX,
  _SC_ARG_MAX,
  _SC_ATEXIT_MAX,
  _SC_BC_BASE_MAX,
  _SC_BC_DIM_MAX,
  _SC_BC_SCALE_MAX,
  _SC_BC_STRING_MAX,
  _SC_CHILD_MAX,
  _SC_CLK_TCK,
  _SC_COLL_WEIGHTS_MAX,
  _SC_DELAYTIMER_MAX,
  _SC_EXPR_NEST_MAX,
  _SC_HOST_NAME_MAX,
  _SC_IOV_MAX,
  _SC_LINE_MAX,
  _SC_LOGIN_NAME_MAX,
  _SC_NGROUPS_MAX,
  _SC_GETGR_R_SIZE_MAX,
  _SC_GETPW_R_SIZE_MAX,
  _SC_MQ_OPEN_MAX,
  _SC_MQ_PRIO_MAX,
  _SC_OPEN_MAX,
  _SC_ADVISORY_INFO,
  _SC_BARRIERS,
  _SC_ASYNCHRONOUS_IO,
  _SC_CLOCK_SELECTION,
  _SC_CPUTIME,
  _SC_FSYNC,
  _SC_IPV6,
  _SC_JOB_CONTROL,
  _SC_MAPPED_FILES,
  _SC_MEMLOCK,
  _SC_MEMLOCK_RANGE,
  _SC_MEMORY_PROTECTION,
  _SC_MESSAGE_PASSING,
  _SC_MONOTONIC_CLOCK,
  _SC_PRIORITIZED_IO,
  _SC_PRIORITY_SCHEDULING,
  _SC_RAW_SOCKETS,
  _SC_READER_WRITER_LOCKS,
  _SC_REALTIME_SIGNALS,
  _SC_REGEXP,
  _SC_SAVED_IDS,
  _SC_SEMAPHORES,
  _SC_SHARED_MEMORY_OBJECTS,
  _SC_SHELL,
  _SC_SPAWN,
  _SC_SPIN_LOCKS,
  _SC_SPORADIC_SERVER,
  _SC_SS_REPL_MAX,
  _SC_SYNCHRONIZED_IO,
  _SC_THREAD_ATTR_STACKADDR,
  _SC_THREAD_ATTR_STACKSIZE,
  _SC_THREAD_CPUTIME,
  _SC_THREAD_PRIO_INHERIT,
  _SC_THREAD_PRIO_PROTECT,
  _SC_THREAD_PRIORITY_SCHEDULING,
  _SC_THREAD_PROCESS_SHARED,
  _SC_THREAD_SAFE_FUNCTIONS,
  _SC_THREAD_SPORADIC_SERVER,
  _SC_THREADS,
  _SC_TIMEOUTS,
  _SC_TIMERS,
  _SC_TRACE,
  _SC_TRACE_EVENT_FILTER,
  _SC_TRACE_EVENT_NAME_MAX,
  _SC_TRACE_INHERIT,
  _SC_TRACE_LOG,
  _SC_TRACE_NAME_MAX,
  _SC_TRACE_SYS_MAX,
  _SC_TRACE_USER_EVENT_MAX,
  _SC_TYPED_MEMORY_OBJECTS,
  _SC_VERSION,
  _SC_V6_ILP32_OFF32,
  _SC_V6_ILP32_OFFBIG,
  _SC_V6_LP64_OFF64,
  _SC_V6_LPBIG_OFFBIG,
  _SC_2_C_BIND,
  _SC_2_C_DEV,
  _SC_2_CHAR_TERM,
  _SC_2_FORT_DEV,
  _SC_2_FORT_RUN,
  _SC_2_LOCALEDEF,
  _SC_2_PBS,
  _SC_2_PBS_ACCOUNTING,
  _SC_2_PBS_CHECKPOINT,
  _SC_2_PBS_LOCATE,
  _SC_2_PBS_MESSAGE,
  _SC_2_PBS_TRACK,
  _SC_2_SW_DEV,
  _SC_2_UPE,
  _SC_2_VERSION,
  _SC_PAGE_SIZE,
  _SC_PAGESIZE,
  _SC_THREAD_DESTRUCTOR_ITERATIONS,
  _SC_THREAD_KEYS_MAX,
  _SC_THREAD_STACK_MIN,
  _SC_THREAD_THREADS_MAX,
  _SC_RE_DUP_MAX,
  _SC_RTSIG_MAX,
  _SC_SEM_NSEMS_MAX,
  _SC_SEM_VALUE_MAX,
  _SC_SIGQUEUE_MAX,
  _SC_STREAM_MAX,
  _SC_SYMLOOP_MAX,
  _SC_TIMER_MAX,
  _SC_TTY_NAME_MAX,
  _SC_TZNAME_MAX,
  _SC_XOPEN_CRYPT,
  _SC_XOPEN_ENH_I18N,
  _SC_XOPEN_LEGACY,
  _SC_XOPEN_REALTIME,
  _SC_XOPEN_REALTIME_THREADS,
  _SC_XOPEN_SHM,
  _SC_XOPEN_STREAMS,
  _SC_XOPEN_UNIX,
  _SC_XOPEN_VERSION
};

enum
{
  _PC_2_SYMLINKS = 1,
  _PC_ALLOC_SIZE_MIN,
  _PC_ASYNC_IO,
  _PC_CHOWN_RESTRICTED,
  _PC_FILESIZEBITS,
  _PC_LINK_MAX,
  _PC_MAX_CANON,
  _PC_MAX_INPUT,
  _PC_NAME_MAX,
  _PC_NO_TRUNC,
  _PC_PATH_MAX,
  _PC_PIPE_BUF,
  _PC_PRIO_IO,
  _PC_REC_INCR_XFER_SIZE,
  _PC_REC_MIN_XFER_SIZE,
  _PC_REC_XFER_ALIGN,
  _PC_SYMLINK_MAX,
  _PC_SYNC_IO,
  _PC_VDISABLE
};

enum
{
  _CS_PATH = 1,
  _CS_POSIX_V6_ILP32_OFF32_CFLAGS,
  _CS_POSIX_V6_ILP32_OFF32_LDFLAGS,
  _CS_POSIX_V6_ILP32_OFF32_LIBS,
  _CS_POSIX_V6_ILP32_OFFBIG_CFLAGS,
  _CS_POSIX_V6_ILP32_OFFBIG_LDFLAGS,
  _CS_POSIX_V6_ILP32_OFFBIG_LIBS,
  _CS_POSIX_V6_LP64_OFF64_CFLAGS,
  _CS_POSIX_V6_LP64_OFF64_LDFLAGS,
  _CS_POSIX_V6_LP64_OFF64_LIBS,
  _CS_POSIX_V6_LPBIG_OFFBIG_CFLAGS,
  _CS_POSIX_V6_LPBIG_OFFBIG_LDFLAGS,
  _CS_POSIX_V6_LPBIG_OFFBIG_LIBS,
  _CS_POSIX_V6_WIDTH_RESTRICTED_ENVS
};

#endif
