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
 * @file os-compatibility.c
 * @brief OS portability utilities for file-descriptor and IPC operations.
 *
 * Provides thin wrappers around POSIX system calls that need to behave
 * consistently across different kernel/libc versions.  The main concerns
 * addressed are:
 *
 * - Setting @c FD_CLOEXEC safely on file descriptors.
 * - Creating socket pairs and epoll instances with @c CLOEXEC atomically
 *   (using the @c _CLOEXEC flag variants) while falling back gracefully on
 *   older kernels that do not support them.
 * - Creating anonymous, unlinked temporary files suitable for Wayland
 *   shared-memory buffer exchange via @c SCM_RIGHTS.
 * - Providing a fallback @c strchrnul() implementation for libc versions
 *   that omit it.
 *
 * This file originates from the Weston compositor project (Collabora, Ltd.,
 * 2012) and is reused here under the same MIT/Expat license.
 *
 * @note This translation unit is present in the repository for completeness
 *       but is @b not compiled into the @c agl-shell-activator binary by the
 *       current Meson build.  It is available for future use if shared-memory
 *       or IPC functionality is added.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>
#include <string.h>
#include <stdlib.h>

#include "os-compatibility.h"

/**
 * @brief Set the @c FD_CLOEXEC flag on a file descriptor.
 *
 * Uses two @c fcntl(2) calls: @c F_GETFD to read the current flags, then
 * @c F_SETFD to write them back with @c FD_CLOEXEC OR-ed in.  This ensures
 * the descriptor is automatically closed across @c exec(3) calls, preventing
 * file-descriptor leaks into child processes.
 *
 * @param fd  The file descriptor to modify.  Passing @c -1 is safe and
 *            returns @c -1 immediately without any system call.
 *
 * @return  0 on success.
 * @return -1 if @p fd is @c -1, or if either @c fcntl(2) call fails
 *            (with @c errno set by @c fcntl).
 */
int
os_fd_set_cloexec(int fd)
{
	long flags;

	if (fd == -1)
		return -1;

	flags = fcntl(fd, F_GETFD);
	if (flags == -1)
		return -1;

	if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
		return -1;

	return 0;
}

/**
 * @brief Set @c FD_CLOEXEC on @p fd, or close it on failure (internal).
 *
 * Convenience wrapper used internally before returning descriptors to
 * callers.  If os_fd_set_cloexec() fails the descriptor is closed to avoid
 * leaks and @c -1 is returned.
 *
 * @param fd  File descriptor to set CLOEXEC on.
 *
 * @return @p fd with @c FD_CLOEXEC set on success.
 * @return -1 (and @p fd is closed) on failure.
 */
static int
set_cloexec_or_close(int fd)
{
	if (os_fd_set_cloexec(fd) != 0) {
		close(fd);
		return -1;
	}
	return fd;
}

/**
 * @brief Create a socket pair with @c FD_CLOEXEC set on both ends.
 *
 * Attempts to use @c SOCK_CLOEXEC atomically (Linux 2.6.27+).  Falls back
 * to plain @c socketpair(2) followed by set_cloexec_or_close() on both
 * descriptors if @c SOCK_CLOEXEC is unavailable or raises @c EINVAL.
 *
 * If setting @c CLOEXEC fails on either descriptor, both are closed and
 * @c -1 is returned.
 *
 * @param domain    Communication domain (e.g., @c AF_UNIX).
 * @param type      Socket type (e.g., @c SOCK_STREAM).  @c SOCK_CLOEXEC
 *                  is OR-ed in internally; do not pass it explicitly.
 * @param protocol  Protocol to use (usually @c 0).
 * @param sv        Output array of two @c int that receives the socket
 *                  pair file descriptors.
 *
 * @return  0 on success, with @p sv[0] and @p sv[1] set and @c CLOEXEC.
 * @return -1 on failure (with @c errno from the failing system call).
 */
int
os_socketpair_cloexec(int domain, int type, int protocol, int *sv)
{
	int ret;

#ifdef SOCK_CLOEXEC
	ret = socketpair(domain, type | SOCK_CLOEXEC, protocol, sv);
	if (ret == 0 || errno != EINVAL)
		return ret;
#endif

	ret = socketpair(domain, type, protocol, sv);
	if (ret < 0)
		return ret;

	sv[0] = set_cloexec_or_close(sv[0]);
	sv[1] = set_cloexec_or_close(sv[1]);

	if (sv[0] != -1 && sv[1] != -1)
		return 0;

	close(sv[0]);
	close(sv[1]);
	return -1;
}

/**
 * @brief Create an epoll file descriptor with @c FD_CLOEXEC set.
 *
 * Attempts to use @c epoll_create1(EPOLL_CLOEXEC) atomically
 * (Linux 2.6.27+).  Falls back to @c epoll_create(1) followed by
 * set_cloexec_or_close() if @c EPOLL_CLOEXEC is unavailable or raises
 * @c EINVAL.
 *
 * @return A valid epoll file descriptor with @c FD_CLOEXEC set on success.
 * @return -1 on failure (with @c errno from the failing system call).
 */
int
os_epoll_create_cloexec(void)
{
	int fd;

#ifdef EPOLL_CLOEXEC
	fd = epoll_create1(EPOLL_CLOEXEC);
	if (fd >= 0)
		return fd;
	if (errno != EINVAL)
		return -1;
#endif

	fd = epoll_create(1);
	return set_cloexec_or_close(fd);
}

/**
 * @brief Create an anonymous, unlinked temporary file (internal helper).
 *
 * Creates a temporary file using @c mkostemp(3) (with @c O_CLOEXEC) if
 * available, falling back to @c mkstemp(3) followed by
 * set_cloexec_or_close().  The file is immediately unlinked so it has no
 * name in the filesystem but remains open via the returned descriptor.
 *
 * @param tmpname  Template string suitable for @c mkostemp / @c mkstemp
 *                 (must end with @c "XXXXXX").  The template is modified
 *                 in-place by the underlying call.
 *
 * @return An open, unlinked, @c CLOEXEC file descriptor on success.
 * @return -1 on failure.
 */
static int
create_tmpfile_cloexec(char *tmpname)
{
	int fd;

#ifdef HAVE_MKOSTEMP
	fd = mkostemp(tmpname, O_CLOEXEC);
	if (fd >= 0)
		unlink(tmpname);
#else
	fd = mkstemp(tmpname);
	if (fd >= 0) {
		fd = set_cloexec_or_close(fd);
		unlink(tmpname);
	}
#endif

	return fd;
}

/**
 * @brief Create a new anonymous shared-memory file of the given size.
 *
 * Creates a unique, anonymous (unlinked), @c CLOEXEC temporary file of
 * exactly @p size bytes under @c $XDG_RUNTIME_DIR.  The returned file
 * descriptor is immediately suitable for:
 * - @c mmap(2) at offset 0 with length @p size.
 * - Sharing between processes by transmitting the descriptor over a Unix
 *   domain socket using @c SCM_RIGHTS.
 *
 * Storage is guaranteed by @c posix_fallocate(3) when available; otherwise
 * @c ftruncate(2) is used (which may result in a sparse file — accessing
 * unmapped regions can trigger @c SIGBUS on some filesystems).
 *
 * @par Error conditions
 * - @c ENOENT: @c $XDG_RUNTIME_DIR is not set.
 * - @c ENOMEM: heap allocation failed.
 * - @c ENOSPC: @c posix_fallocate() could not reserve @p size bytes.
 * - Any error from @c mkostemp / @c mkstemp or @c ftruncate.
 *
 * @param size  Desired file size in bytes.  Must be > 0.
 *
 * @return An open file descriptor on success (caller must @c close(2) it).
 * @return -1 on failure (with @c errno set appropriately).
 */
int
os_create_anonymous_file(off_t size)
{
	static const char template[] = "/weston-shared-XXXXXX";
	const char *path;
	char *name;
	int fd;
	int ret;

	path = getenv("XDG_RUNTIME_DIR");
	if (!path) {
		errno = ENOENT;
		return -1;
	}

	name = malloc(strlen(path) + sizeof(template));
	if (!name)
		return -1;

	strcpy(name, path);
	strcat(name, template);

	fd = create_tmpfile_cloexec(name);

	free(name);

	if (fd < 0)
		return -1;

#ifdef HAVE_POSIX_FALLOCATE
	ret = posix_fallocate(fd, 0, size);
	if (ret != 0) {
		close(fd);
		errno = ret;
		return -1;
	}
#else
	ret = ftruncate(fd, size);
	if (ret < 0) {
		close(fd);
		return -1;
	}
#endif

	return fd;
}

#ifndef HAVE_STRCHRNUL
/**
 * @brief Locate a character in a string, returning a pointer to the NUL
 *        terminator if the character is not found.
 *
 * This is a fallback implementation of the GNU/POSIX extension
 * @c strchrnul(3), compiled in only when the host libc does not provide it
 * (i.e., when @c HAVE_STRCHRNUL is not defined at build time).
 *
 * Scans @p s forward until it finds @p c or the NUL terminator, then
 * returns a pointer to that position.  Unlike @c strchr(3), this function
 * never returns @c NULL.
 *
 * @param s  The string to search.  Must be NUL-terminated.
 * @param c  The character to search for (interpreted as @c unsigned @c char).
 *
 * @return Pointer to the first occurrence of @p c in @p s, or a pointer
 *         to the NUL terminator of @p s if @p c is not present.
 */
char *
strchrnul(const char *s, int c)
{
	while (*s && *s != c)
		s++;
	return (char *)s;
}
#endif
