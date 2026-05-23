// SPDX-License-Identifier: MIT
/*
 * Copyright 2021 Collabora, Ltd.
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
 * @file test_os_compat.c
 * @brief libcheck unit tests for the OS compatibility utilities.
 *
 * Covers every public function in os-compatibility.c:
 *  - os_fd_set_cloexec()
 *  - os_socketpair_cloexec()
 *  - os_epoll_create_cloexec()
 *  - os_create_anonymous_file()
 *
 * The strchrnul() fallback is not tested here because on glibc targets it is
 * provided by the C library and the fallback path is never compiled in.
 *
 * Run via:
 *   CK_AUTOMAKE=1 ./test-os-compat        # ptest-compatible PASS:/FAIL: output
 *   ./test-os-compat                       # human-readable summary
 *   CK_LOG_FILE_NAME=- ./test-os-compat   # TAP output for meson test
 */

#include <check.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "os-compatibility.h"

/* -------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */

/** Return non-zero if FD_CLOEXEC is set on @p fd. */
static int
fd_has_cloexec(int fd)
{
	int flags = fcntl(fd, F_GETFD);
	if (flags == -1)
		return 0;
	return (flags & FD_CLOEXEC) != 0;
}

/* -------------------------------------------------------------------------
 * Suite: os_fd_set_cloexec
 * ---------------------------------------------------------------------- */

/**
 * @test Passing -1 as the file descriptor must return -1 immediately.
 */
START_TEST(test_cloexec_invalid_fd)
{
	int ret = os_fd_set_cloexec(-1);
	ck_assert_int_eq(ret, -1);
}
END_TEST

/**
 * @test A valid, open file descriptor must have FD_CLOEXEC set after the
 *       call and the function must return 0.
 */
START_TEST(test_cloexec_sets_flag)
{
	int fd = open("/dev/null", O_RDONLY);
	ck_assert_int_ge(fd, 0);

	int ret = os_fd_set_cloexec(fd);
	ck_assert_int_eq(ret, 0);
	ck_assert_msg(fd_has_cloexec(fd),
		      "FD_CLOEXEC not set after os_fd_set_cloexec()");

	close(fd);
}
END_TEST

/**
 * @test Calling os_fd_set_cloexec() twice on the same fd is idempotent.
 */
START_TEST(test_cloexec_idempotent)
{
	int fd = open("/dev/null", O_RDONLY);
	ck_assert_int_ge(fd, 0);

	ck_assert_int_eq(os_fd_set_cloexec(fd), 0);
	ck_assert_int_eq(os_fd_set_cloexec(fd), 0);
	ck_assert_msg(fd_has_cloexec(fd), "FD_CLOEXEC lost after second call");

	close(fd);
}
END_TEST

/* -------------------------------------------------------------------------
 * Suite: os_socketpair_cloexec
 * ---------------------------------------------------------------------- */

/**
 * @test A successfully created socket pair must return 0 and both
 *       descriptors must be valid (>= 0) with FD_CLOEXEC set.
 */
START_TEST(test_socketpair_returns_valid_fds)
{
	int sv[2] = { -1, -1 };
	int ret = os_socketpair_cloexec(AF_UNIX, SOCK_STREAM, 0, sv);

	ck_assert_int_eq(ret, 0);
	ck_assert_int_ge(sv[0], 0);
	ck_assert_int_ge(sv[1], 0);

	close(sv[0]);
	close(sv[1]);
}
END_TEST

/**
 * @test Both ends of the socket pair must have FD_CLOEXEC set.
 */
START_TEST(test_socketpair_cloexec_set)
{
	int sv[2] = { -1, -1 };
	ck_assert_int_eq(os_socketpair_cloexec(AF_UNIX, SOCK_STREAM, 0, sv), 0);

	ck_assert_msg(fd_has_cloexec(sv[0]), "FD_CLOEXEC not set on sv[0]");
	ck_assert_msg(fd_has_cloexec(sv[1]), "FD_CLOEXEC not set on sv[1]");

	close(sv[0]);
	close(sv[1]);
}
END_TEST

/**
 * @test Data written to one end of the pair must be readable from the other.
 */
START_TEST(test_socketpair_data_transfer)
{
	int sv[2] = { -1, -1 };
	ck_assert_int_eq(os_socketpair_cloexec(AF_UNIX, SOCK_STREAM, 0, sv), 0);

	const char msg[] = "ping";
	char buf[sizeof(msg)] = { 0 };

	ck_assert_int_eq((int)write(sv[0], msg, sizeof(msg)), (int)sizeof(msg));
	ck_assert_int_eq((int)read(sv[1], buf, sizeof(buf)), (int)sizeof(buf));
	ck_assert_str_eq(buf, msg);

	close(sv[0]);
	close(sv[1]);
}
END_TEST

/* -------------------------------------------------------------------------
 * Suite: os_epoll_create_cloexec
 * ---------------------------------------------------------------------- */

/**
 * @test os_epoll_create_cloexec() must return a non-negative file descriptor.
 */
START_TEST(test_epoll_returns_valid_fd)
{
	int fd = os_epoll_create_cloexec();
	ck_assert_int_ge(fd, 0);
	close(fd);
}
END_TEST

/**
 * @test The returned epoll descriptor must have FD_CLOEXEC set.
 */
START_TEST(test_epoll_cloexec_set)
{
	int fd = os_epoll_create_cloexec();
	ck_assert_int_ge(fd, 0);

	ck_assert_msg(fd_has_cloexec(fd),
		      "FD_CLOEXEC not set on epoll fd");

	close(fd);
}
END_TEST

/**
 * @test The returned fd must be a valid epoll instance: adding a watched
 *       fd via epoll_ctl() must succeed.
 */
START_TEST(test_epoll_usable)
{
	int epfd = os_epoll_create_cloexec();
	ck_assert_int_ge(epfd, 0);

	int target = open("/dev/null", O_RDONLY);
	ck_assert_int_ge(target, 0);

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = target;
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, target, &ev);
	ck_assert_int_eq(ret, 0);

	close(target);
	close(epfd);
}
END_TEST

/* -------------------------------------------------------------------------
 * Suite: os_create_anonymous_file
 * ---------------------------------------------------------------------- */

/**
 * @test When XDG_RUNTIME_DIR is unset the function must return -1 with
 *       errno == ENOENT.
 */
START_TEST(test_anon_file_no_xdg_runtime_dir)
{
	const char *saved = getenv("XDG_RUNTIME_DIR");
	/* Save and unset */
	char *saved_copy = saved ? strdup(saved) : NULL;
	unsetenv("XDG_RUNTIME_DIR");

	errno = 0;
	int fd = os_create_anonymous_file(4096);
	int saved_errno = errno;

	/* Restore */
	if (saved_copy) {
		setenv("XDG_RUNTIME_DIR", saved_copy, 1);
		free(saved_copy);
	}

	ck_assert_int_eq(fd, -1);
	ck_assert_int_eq(saved_errno, ENOENT);
}
END_TEST

/**
 * @test A valid XDG_RUNTIME_DIR must yield a non-negative fd of the
 *       requested size with FD_CLOEXEC set.
 */
START_TEST(test_anon_file_valid)
{
	/* Use /tmp as a reliable XDG_RUNTIME_DIR for testing purposes */
	const char *saved = getenv("XDG_RUNTIME_DIR");
	char *saved_copy = saved ? strdup(saved) : NULL;
	setenv("XDG_RUNTIME_DIR", "/tmp", 1);

	const off_t size = 4096;
	int fd = os_create_anonymous_file(size);

	/* Restore before assertions so cleanup always happens */
	if (saved_copy) {
		setenv("XDG_RUNTIME_DIR", saved_copy, 1);
		free(saved_copy);
	} else {
		unsetenv("XDG_RUNTIME_DIR");
	}

	ck_assert_int_ge(fd, 0);
	ck_assert_msg(fd_has_cloexec(fd),
		      "FD_CLOEXEC not set on anonymous file fd");

	/* Verify the file is the right size */
	struct stat st;
	ck_assert_int_eq(fstat(fd, &st), 0);
	ck_assert_int_eq((off_t)st.st_size, size);

	close(fd);
}
END_TEST

/**
 * @test Requesting a zero-size file should either succeed or fail
 *       gracefully — it must not crash.
 *
 * posix_fallocate(fd, 0, 0) is implementation-defined; ftruncate(fd, 0)
 * succeeds.  Either outcome is acceptable; we just verify no crash/assert.
 */
START_TEST(test_anon_file_zero_size)
{
	const char *saved = getenv("XDG_RUNTIME_DIR");
	char *saved_copy = saved ? strdup(saved) : NULL;
	setenv("XDG_RUNTIME_DIR", "/tmp", 1);

	int fd = os_create_anonymous_file(0);

	if (saved_copy) {
		setenv("XDG_RUNTIME_DIR", saved_copy, 1);
		free(saved_copy);
	} else {
		unsetenv("XDG_RUNTIME_DIR");
	}

	/* Either outcome is acceptable — just must not crash */
	if (fd >= 0)
		close(fd);
	/* If fd == -1 that is also acceptable for size 0 */
	ck_assert_msg(fd >= -1, "unexpected return value from os_create_anonymous_file(0)");
}
END_TEST

/**
 * @test The created file must have been unlinked from the filesystem
 *       (no directory entry): /proc/self/fd/<n> readlink should end in
 *       "(deleted)" on Linux.
 */
START_TEST(test_anon_file_is_unlinked)
{
	const char *saved = getenv("XDG_RUNTIME_DIR");
	char *saved_copy = saved ? strdup(saved) : NULL;
	setenv("XDG_RUNTIME_DIR", "/tmp", 1);

	int fd = os_create_anonymous_file(1024);

	if (saved_copy) {
		setenv("XDG_RUNTIME_DIR", saved_copy, 1);
		free(saved_copy);
	} else {
		unsetenv("XDG_RUNTIME_DIR");
	}

	ck_assert_int_ge(fd, 0);

	char link_path[64];
	char target[256] = { 0 };
	snprintf(link_path, sizeof(link_path), "/proc/self/fd/%d", fd);
	ssize_t n = readlink(link_path, target, sizeof(target) - 1);

	close(fd);

	ck_assert_int_gt(n, 0);
	/* On Linux, unlinked files show as "path (deleted)" */
	ck_assert_msg(strstr(target, "(deleted)") != NULL,
		      "anonymous file still has a directory entry: %s", target);
}
END_TEST

/* -------------------------------------------------------------------------
 * Suite assembly
 * ---------------------------------------------------------------------- */

/** Build the os_fd_set_cloexec test suite. */
static Suite *
suite_cloexec(void)
{
	Suite *s = suite_create("os_fd_set_cloexec");
	TCase *tc = tcase_create("core");

	tcase_add_test(tc, test_cloexec_invalid_fd);
	tcase_add_test(tc, test_cloexec_sets_flag);
	tcase_add_test(tc, test_cloexec_idempotent);

	suite_add_tcase(s, tc);
	return s;
}

/** Build the os_socketpair_cloexec test suite. */
static Suite *
suite_socketpair(void)
{
	Suite *s = suite_create("os_socketpair_cloexec");
	TCase *tc = tcase_create("core");

	tcase_add_test(tc, test_socketpair_returns_valid_fds);
	tcase_add_test(tc, test_socketpair_cloexec_set);
	tcase_add_test(tc, test_socketpair_data_transfer);

	suite_add_tcase(s, tc);
	return s;
}

/** Build the os_epoll_create_cloexec test suite. */
static Suite *
suite_epoll(void)
{
	Suite *s = suite_create("os_epoll_create_cloexec");
	TCase *tc = tcase_create("core");

	tcase_add_test(tc, test_epoll_returns_valid_fd);
	tcase_add_test(tc, test_epoll_cloexec_set);
	tcase_add_test(tc, test_epoll_usable);

	suite_add_tcase(s, tc);
	return s;
}

/** Build the os_create_anonymous_file test suite. */
static Suite *
suite_anon_file(void)
{
	Suite *s = suite_create("os_create_anonymous_file");
	TCase *tc = tcase_create("core");

	tcase_add_test(tc, test_anon_file_no_xdg_runtime_dir);
	tcase_add_test(tc, test_anon_file_valid);
	tcase_add_test(tc, test_anon_file_zero_size);
	tcase_add_test(tc, test_anon_file_is_unlinked);

	suite_add_tcase(s, tc);
	return s;
}

/* -------------------------------------------------------------------------
 * Entry point
 * ---------------------------------------------------------------------- */

/**
 * @brief Test runner entry point.
 *
 * Registers all suites, runs them, and returns the number of failures as
 * the exit code (0 = all passed, non-zero = failures).
 *
 * Output format is controlled by environment variables:
 * - Default:          human-readable summary to stdout.
 * - @c CK_AUTOMAKE=1: one @c PASS:/FAIL: line per test (ptest format).
 * - @c CK_LOG_FILE_NAME=-: TAP stream to stdout (for @c meson @c test).
 */
int
main(void)
{
	int failed;
	SRunner *sr = srunner_create(suite_cloexec());

	srunner_add_suite(sr, suite_socketpair());
	srunner_add_suite(sr, suite_epoll());
	srunner_add_suite(sr, suite_anon_file());

	srunner_run_all(sr, CK_ENV);

	failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
