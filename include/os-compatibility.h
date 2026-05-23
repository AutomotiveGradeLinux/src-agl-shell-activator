// SPDX-License-Identifier: MIT
/*
 * Copyright © 2012 Collabora, Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file os-compatibility.h
 * @brief Public interface for OS portability utilities.
 *
 * Declares wrappers that abstract over differences between kernel versions
 * and libc implementations for the following areas:
 *
 * - **FD_CLOEXEC** – setting close-on-exec on arbitrary file descriptors.
 * - **Socket pairs** – creating @c AF_UNIX socket pairs with @c CLOEXEC.
 * - **Epoll** – creating epoll instances with @c CLOEXEC.
 * - **Anonymous files** – creating unlinked, size-fixed temporary files
 *   for Wayland shared-memory buffers.
 * - **strchrnul** – fallback declaration for libc versions that lack it.
 * - **backtrace** – stub for platforms that lack @c <execinfo.h>.
 *
 * Implementations live in @ref os-compatibility.c.
 */

#ifndef OS_COMPATIBILITY_H
#define OS_COMPATIBILITY_H

#include <sys/types.h>

/**
 * @defgroup os_compat OS Compatibility Utilities
 * @brief Portability wrappers for file-descriptor and IPC primitives.
 * @{
 */

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#else
/**
 * @brief Stub @c backtrace(3) for platforms without @c <execinfo.h>.
 *
 * Returns 0 and does not populate @p buffer.  This allows code that
 * optionally calls @c backtrace() to compile and link on platforms (e.g.,
 * musl libc, some embedded targets) that do not provide the GNU extension.
 *
 * When @c HAVE_EXECINFO_H is defined the real @c backtrace(3) from
 * @c <execinfo.h> is used instead.
 *
 * @param buffer  Output array for instruction pointers (unused here).
 * @param size    Maximum number of frames to capture (unused here).
 *
 * @return Always 0.
 */
static inline int
backtrace(void **buffer, int size)
{
	return 0;
}
#endif

/**
 * @brief Set the @c FD_CLOEXEC flag on a file descriptor.
 *
 * Reads the current descriptor flags with @c fcntl(F_GETFD) and writes
 * them back with @c FD_CLOEXEC set via @c fcntl(F_SETFD).
 *
 * @param fd  File descriptor to modify.  Passing @c -1 is safe.
 *
 * @return  0 on success.
 * @return -1 if @p fd is @c -1, or if @c fcntl(2) fails.
 */
int
os_fd_set_cloexec(int fd);

/**
 * @brief Create a socket pair with @c FD_CLOEXEC set on both ends.
 *
 * Uses @c SOCK_CLOEXEC atomically when available (Linux 2.6.27+), falling
 * back to post-creation @c fcntl on older kernels.
 *
 * @param domain    Communication domain (e.g., @c AF_UNIX).
 * @param type      Socket type (e.g., @c SOCK_STREAM).
 * @param protocol  Protocol (usually @c 0).
 * @param sv        Output array of two @c int for the socket pair.
 *
 * @return  0 on success.
 * @return -1 on failure (with @c errno set).
 */
int
os_socketpair_cloexec(int domain, int type, int protocol, int *sv);

/**
 * @brief Create an epoll file descriptor with @c FD_CLOEXEC set.
 *
 * Uses @c epoll_create1(EPOLL_CLOEXEC) when available, falling back to
 * @c epoll_create(1) with post-creation @c fcntl on older kernels.
 *
 * @return A valid epoll file descriptor with @c FD_CLOEXEC set.
 * @return -1 on failure (with @c errno set).
 */
int
os_epoll_create_cloexec(void);

/**
 * @brief Create an anonymous shared-memory file of the specified size.
 *
 * Creates a unique, unlinked, @c CLOEXEC temporary file under
 * @c $XDG_RUNTIME_DIR.  The returned descriptor is immediately suitable
 * for @c mmap(2) and can be shared with other processes via @c SCM_RIGHTS.
 *
 * Disk space is reserved with @c posix_fallocate(3) when available, or
 * via @c ftruncate(2) otherwise.
 *
 * @param size  Desired file size in bytes.  Must be > 0.
 *
 * @return An open file descriptor on success.
 * @return -1 on failure (with @c errno set; @c ENOENT if
 *         @c $XDG_RUNTIME_DIR is unset, @c ENOSPC if space is insufficient).
 */
int
os_create_anonymous_file(off_t size);

#ifndef HAVE_STRCHRNUL
/**
 * @brief Locate a character in a string, returning a pointer to the NUL
 *        terminator when the character is absent.
 *
 * Fallback declaration compiled in only when the host libc does not
 * provide @c strchrnul(3) (i.e., @c HAVE_STRCHRNUL is not defined).
 *
 * @param s  NUL-terminated string to search.
 * @param c  Character to search for.
 *
 * @return Pointer to the first occurrence of @p c in @p s, or a pointer
 *         to the NUL terminator if @p c is not found.  Never returns NULL.
 */
char *
strchrnul(const char *s, int c);
#endif

/** @} */ /* end of os_compat group */

#endif /* OS_COMPATIBILITY_H */
