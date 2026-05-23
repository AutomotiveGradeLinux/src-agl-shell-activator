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
 * @file test_args.c
 * @brief Black-box argument validation tests for the agl-shell-activator
 *        binary.
 *
 * These tests fork and exec the installed (or build-directory) binary and
 * verify its exit code for various argument combinations.  No Wayland
 * compositor is required: all tested code paths exit before attempting to
 * connect to a display socket.
 *
 * The binary path is resolved via the @c AGL_ACTIVATOR_BIN environment
 * variable.  When that is unset the test falls back to looking for
 * @c agl-shell-activator on @c PATH.  The Meson test() definition passes
 * the build-directory path through the @c env: keyword.
 *
 * Run via:
 *   CK_AUTOMAKE=1 ./test-args        # ptest-compatible PASS:/FAIL: output
 *   ./test-args                       # human-readable summary
 *   CK_LOG_FILE_NAME=- ./test-args   # TAP stream for meson test
 */

#include <check.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* -------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */

/**
 * @brief Fork and exec agl-shell-activator with the given argument list.
 *
 * @p argv must be NULL-terminated.  The binary path is taken from the
 * @c AGL_ACTIVATOR_BIN environment variable if set, otherwise
 * @c "agl-shell-activator" is looked up on @c PATH.
 *
 * stdout and stderr of the child are suppressed so test output is not
 * polluted by the binary's own error messages.
 *
 * @param argv  NULL-terminated argument vector (argv[0] is ignored and
 *              replaced with the resolved binary path).
 *
 * @return The child's exit status as returned by @c WEXITSTATUS(), or
 *         @c -1 if @c fork() or @c waitpid() failed.
 */
static int
run_activator(char *const argv[])
{
	const char *bin = getenv("AGL_ACTIVATOR_BIN");
	if (!bin || bin[0] == '\0')
		bin = "agl-shell-activator";

	pid_t pid = fork();
	if (pid < 0)
		return -1;

	if (pid == 0) {
		/* Child: suppress stdout/stderr, then exec */
		int devnull = open("/dev/null", O_WRONLY);
		if (devnull >= 0) {
			dup2(devnull, STDOUT_FILENO);
			dup2(devnull, STDERR_FILENO);
			close(devnull);
		}
		/* argv[0] is replaced with the binary path */
		char *exec_argv[16] = { NULL };
		exec_argv[0] = (char *)bin;
		int i;
		for (i = 1; argv[i] != NULL && i < 15; i++)
			exec_argv[i] = argv[i];
		exec_argv[i] = NULL;

		execvp(bin, exec_argv);
		/* execvp only returns on error */
		_exit(127);
	}

	/* Parent: wait for child */
	int status = 0;
	if (waitpid(pid, &status, 0) < 0)
		return -1;

	if (WIFEXITED(status))
		return WEXITSTATUS(status);

	/* Signalled or other abnormal termination */
	return -1;
}

/* -------------------------------------------------------------------------
 * Suite: argument validation
 * ---------------------------------------------------------------------- */

/**
 * @test Invoking the binary with no arguments must exit with a non-zero
 *       (failure) status.  The binary must not crash or hang.
 */
START_TEST(test_no_args_exits_failure)
{
	char *argv[] = { "", NULL };
	int ret = run_activator(argv);

	ck_assert_msg(ret != 0,
		      "expected non-zero exit for missing app-id, got %d", ret);
	ck_assert_msg(ret != 127,
		      "binary not found (execvp failed); set AGL_ACTIVATOR_BIN");
}
END_TEST

/**
 * @test Invoking the binary with an empty string as the app-id must exit
 *       with a non-zero (failure) status.
 *
 * An empty app-id is explicitly rejected by main() before any Wayland
 * connection is attempted.
 */
START_TEST(test_empty_appid_exits_failure)
{
	char *argv[] = { "", "", NULL };
	int ret = run_activator(argv);

	ck_assert_msg(ret != 0,
		      "expected non-zero exit for empty app-id, got %d", ret);
	ck_assert_msg(ret != 127,
		      "binary not found (execvp failed); set AGL_ACTIVATOR_BIN");
}
END_TEST

/**
 * @test Invoking the binary with a non-empty app-id when no Wayland
 *       compositor is running must exit with a non-zero status (the
 *       assert() on wl_display_connect() will abort, or the binary
 *       exits with an error).
 *
 * This test intentionally exercises the code path that attempts a Wayland
 * connection in an environment where no compositor is present.  It verifies
 * that the binary does not hang indefinitely.
 *
 * @note  This test is skipped when @c WAYLAND_DISPLAY is set to a reachable
 *        socket, since in that case the binary may succeed (exit 0) by
 *        actually activating an app.
 */
START_TEST(test_no_compositor_exits_nonzero)
{
	/* Skip if a live Wayland socket appears to be available */
	const char *wd = getenv("WAYLAND_DISPLAY");
	if (wd && wd[0] != '\0') {
		/* Cannot predict the outcome — skip rather than false-fail */
		return;
	}

	/* Point libwayland at a non-existent socket */
	setenv("WAYLAND_DISPLAY", "wayland-test-nonexistent-9999", 1);
	/* Also clear XDG_RUNTIME_DIR to guarantee no accidental match */
	const char *saved_xdg = getenv("XDG_RUNTIME_DIR");
	char *saved_xdg_copy = saved_xdg ? strdup(saved_xdg) : NULL;
	setenv("XDG_RUNTIME_DIR", "/tmp", 1);

	char *argv[] = { "", "some-app-id", NULL };
	int ret = run_activator(argv);

	/* Restore environment */
	unsetenv("WAYLAND_DISPLAY");
	if (saved_xdg_copy) {
		setenv("XDG_RUNTIME_DIR", saved_xdg_copy, 1);
		free(saved_xdg_copy);
	} else {
		unsetenv("XDG_RUNTIME_DIR");
	}

	ck_assert_msg(ret != 0,
		      "expected non-zero exit with no compositor, got %d", ret);
}
END_TEST

/* -------------------------------------------------------------------------
 * Suite assembly
 * ---------------------------------------------------------------------- */

/** Build the argument-validation test suite. */
static Suite *
suite_args(void)
{
	Suite *s = suite_create("argument_validation");
	TCase *tc = tcase_create("core");

	/* Allow up to 5 seconds per test in case of slow process startup */
	tcase_set_timeout(tc, 5);

	tcase_add_test(tc, test_no_args_exits_failure);
	tcase_add_test(tc, test_empty_appid_exits_failure);
	tcase_add_test(tc, test_no_compositor_exits_nonzero);

	suite_add_tcase(s, tc);
	return s;
}

/* -------------------------------------------------------------------------
 * Entry point
 * ---------------------------------------------------------------------- */

/**
 * @brief Test runner entry point.
 *
 * Output format is controlled by environment variables:
 * - Default:          human-readable summary.
 * - @c CK_AUTOMAKE=1: one @c PASS:/FAIL: line per test (ptest format).
 * - @c CK_LOG_FILE_NAME=-: TAP stream to stdout (for @c meson @c test).
 */
int
main(void)
{
	SRunner *sr = srunner_create(suite_args());

	srunner_run_all(sr, CK_ENV);

	int failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
