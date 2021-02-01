/*
 * Copyright © 2020 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

/* NOTE: This file must be included *after all other includes*. */

/* NSIG isn't in POSIX, so if it doesn't exist use this here. See bug #759196 */
#ifndef NSIG
#define NSIG (8 * sizeof(sigset_t))
#endif

#ifndef HAVE_FDWALK
int fdwalk(int (*cb)(void* data, int fd),
           void* data);
#endif

#ifndef HAVE_STRCHRNUL
char* strchrnul(char const* s,
                int c);
#endif

#ifdef __linux__

/* BEGIN
 * The following is copied from systemd/src/basic/missing_syscall_def.h (LGPL2.1+)
 */
#ifndef __NR_close_range
#  if defined(__aarch64__)
#    define __NR_close_range 436
#  elif defined(__alpha__)
#    define __NR_close_range 546
#  elif defined(__arc__) || defined(__tilegx__)
#    define __NR_close_range 436
#  elif defined(__arm__)
#    define __NR_close_range 436
#  elif defined(__i386__)
#    define __NR_close_range 436
#  elif defined(__ia64__)
#    define __NR_close_range 1460
#  elif defined(__m68k__)
#    define __NR_close_range 436
#  elif defined(_MIPS_SIM)
#    if _MIPS_SIM == _MIPS_SIM_ABI32
#      define __NR_close_range 4436
#    elif _MIPS_SIM == _MIPS_SIM_NABI32
#      define __NR_close_range 6436
#    elif _MIPS_SIM == _MIPS_SIM_ABI64
#      define __NR_close_range 5436
#    else
#      error "Unknown MIPS ABI"
#    endif
#  elif defined(__powerpc__)
#    define __NR_close_range 436
#  elif defined(__s390__)
#    define __NR_close_range 436
#  elif defined(__sparc__)
#    define __NR_close_range 436
#  elif defined(__x86_64__)
#    if defined(__ILP32__)
#      define __NR_close_range (436 | /* __X32_SYSCALL_BIT */ 0x40000000)
#    else
#      define __NR_close_range 436
#    endif
#  else
#    warning "close_range() syscall number is unknown for your architecture"
#  endif
#endif /* !__NR_close_range */

/* END copied from systemd */

#if !defined(SYS_close_range) && defined(__NR_close_range)
#define SYS_close_range __NR_close_range
#endif

#ifndef CLOSE_RANGE_CLOEXEC
#define CLOSE_RANGE_CLOEXEC (1u << 2)
#endif

#endif /* __linux__ */
