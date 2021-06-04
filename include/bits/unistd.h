/*************************************************************************
 * unistd.h -- This file is part of OS/0.                                *
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

#ifndef _BITS_UNISTD_H
#define _BITS_UNISTD_H

#if !defined _SYS_SYSLIMITS_H && !defined _SYS_UNISTD_H
#error  "<bits/unistd.h> should not be included directly"
#endif

#define _POSIX_VERSION 200112L

#define _POSIX_ADVISORY_INFO              -1
#define _POSIX_ASYNCHRONOUS_IO            -1
#define _POSIX_BARRIERS                   -1
#undef  _POSIX_CHOWN_RESTRICTED
#define _POSIX_CLOCK_SELECTION            -1
#define _POSIX_CPUTIME                    -1
#define _POSIX_FSYNC                      -1
#define _POSIX_IPV6                       -1
#undef  _POSIX_JOB_CONTROL
#define _POSIX_MAPPED_FILES               -1
#define _POSIX_MEMLOCK                    -1
#define _POSIX_MEMLOCK_RANGE              -1
#define _POSIX_MEMORY_PROTECTION          -1
#define _POSIX_MESSAGE_PASSING            -1
#define _POSIX_MONOTONIC_CLOCK            -1
#undef  _POSIX_NO_TRUNC
#define _POSIX_PRIORITIZED_IO             -1
#define _POSIX_PRIORITY_SCHEDULING        -1
#define _POSIX_RAW_SOCKETS                -1
#define _POSIX_READER_WRITER_LOCKS        -1
#define _POSIX_REALTIME_SIGNALS           -1
#undef  _POSIX_REGEXP
#undef  _POSIX_SAVED_IDS
#define _POSIX_SEMAPHORES                 -1
#define _POSIX_SHARED_MEMORY_OBJECTS      -1
#undef  _POSIX_SHELL
#define _POSIX_SPAWN                      -1
#define _POSIX_SPIN_LOCKS                 -1
#define _POSIX_SPORADIC_SERVER            -1
#define _POSIX_SYNCHRONIZED_IO            -1
#define _POSIX_THREAD_ATTR_STACKADDR      -1
#define _POSIX_THREAD_ATTR_STACKSIZE      -1
#define _POSIX_THREAD_CPUTIME             -1
#define _POSIX_THREAD_PRIO_INHERIT        -1
#define _POSIX_THREAD_PRIO_PROTECT        -1
#define _POSIX_THREAD_PRIORITY_SCHEDULING -1
#define _POSIX_THREAD_PROCESS_SHARED      -1
#define _POSIX_THREAD_SAFE_FUNCTIONS      -1
#define _POSIX_THREAD_SPORADIC_SERVER     -1
#define _POSIX_THREADS                    -1
#define _POSIX_TIMEOUTS                   -1
#define _POSIX_TIMERS                     -1
#define _POSIX_TRACE                      -1
#define _POSIX_TRACE_EVENT_FILTER         -1
#define _POSIX_TRACE_INHERIT              -1
#define _POSIX_TRACE_LOG                  -1
#define _POSIX_TYPED_MEMORY_OBJECTS       -1
#define _POSIX_VDISABLE                   '\377'
#define _POSIX2_C_BIND                    200122L
#define _POSIX2_C_DEV                     200122L
#define _POSIX2_CHAR_TERM                 1
#define _POSIX2_FORT_DEV                  200122L
#define _POSIX2_FORT_RUN                  200122L
#define _POSIX2_LOCALEDEF                 -1
#define _POSIX2_PBS                       -1
#define _POSIX2_PBS_ACCOUNTING            -1
#define _POSIX2_PBS_CHECKPOINT            -1
#define _POSIX2_PBS_LOCATE                -1
#define _POSIX2_PBS_MESSAGE               -1
#define _POSIX2_PBS_TRACK                 -1
#define _POSIX2_SW_DEV                    200122L
#define _POSIX2_UPE                       200122L

#define _POSIX_V6_ILP32_OFF32
#undef  _POSIX_V6_ILP32_OFFBIG
#undef  _POSIX_V6_LP64_OFF64
#undef  _POSIX_V6_LP64_OFFBIG

#undef _XOPEN_CRYPT
#undef _XOPEN_ENH_I18N
#undef _XOPEN_LEGACY
#undef _XOPEN_REALTIME
#undef _XOPEN_REALTIME_THREADS
#undef _XOPEN_SHM
#undef _XOPEN_STREAMS
#undef _XOPEN_UNIX

#define _POSIX_ASYNC_IO -1
#define _POSIX_PRIO_IO  -1
#define _POSIX_SYNC_IO  -1

#endif
