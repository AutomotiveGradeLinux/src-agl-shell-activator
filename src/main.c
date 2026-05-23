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
 * @file main.c
 * @brief AGL shell activator – Wayland client entry point.
 *
 * Connects to a running Wayland compositor that exposes the
 * @c agl_shell_desktop protocol extension, discovers all available
 * @c wl_output globals, then sends a single
 * @c agl_shell_desktop_activate_app() request to bring the requested
 * application to the foreground on the first available output.
 *
 * @par Usage
 * @code
 *   agl-shell-activator <app-id>
 * @endcode
 *
 * @par Protocol flow
 * @code
 *   wl_display_connect()
 *     -> wl_display_get_registry()
 *       -> roundtrip #1: registry_handle_global() fires for each global
 *          (binds agl_shell_desktop and each wl_output)
 *       -> roundtrip #2: drains any per-object events (wl_output geometry etc.)
 *     -> agl_shell_desktop_activate_app(app_id, NULL, first_output)
 *     -> destroy_display()
 * @endcode
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>

#include <wayland-client.h>
#include <wayland-util.h>
#include "agl-shell-desktop-client-protocol.h"


/** Forward declaration of the top-level display state structure. */
struct display;

/**
 * @brief Per-output node stored in display::output_list.
 *
 * Each @c wl_output global announced by the compositor results in one
 * @c window_output being allocated and inserted into
 * @ref display::output_list.
 */
struct window_output {
	/** Back-pointer to the owning display state. */
	struct display *display;
	/** The Wayland output proxy object. */
        struct wl_output *output;
	/** Intrusive list link; belongs to display::output_list. */
        struct wl_list link;
};

/**
 * @brief Top-level application state.
 *
 * Allocated once by create_display() and freed by destroy_display().
 * Passed by pointer to all functions that need compositor access.
 */
struct display {
	/** Active connection to the Wayland compositor. */
	struct wl_display *display;
	/** Registry proxy used to enumerate compositor globals. */
	struct wl_registry *registry;
	/**
	 * Bound handle for the AGL shell desktop extension.
	 * NULL if the compositor does not advertise @c agl_shell_desktop.
	 */
	struct agl_shell_desktop *agl_shell_desktop;
	/** Linked list of @ref window_output nodes (one per wl_output). */
	struct wl_list output_list;
};

/**
 * @brief @c wl_output_listener callback: output geometry event (stub).
 *
 * Receives position, physical dimensions, subpixel layout, make, model,
 * and transform of a @c wl_output.  Not needed by this tool; intentionally
 * left empty.
 *
 * @param data       User data pointer (unused).
 * @param wl_output  The output proxy that generated the event (unused).
 * @param x          X position of the output in compositor space (unused).
 * @param y          Y position of the output in compositor space (unused).
 * @param physical_width   Physical width in millimetres (unused).
 * @param physical_height  Physical height in millimetres (unused).
 * @param subpixel   Subpixel layout enumeration value (unused).
 * @param make       Manufacturer string (unused).
 * @param model      Model string (unused).
 * @param transform  Output transform enumeration value (unused).
 */
static void
display_handle_geometry(void *data, struct wl_output *wl_output,
                        int x, int y, int physical_width, int physical_height,
                        int subpixel, const char *make, const char *model, int transform)
{
}

/**
 * @brief @c wl_output_listener callback: video mode event (stub).
 *
 * Receives the current or preferred video mode of the output.  Not needed
 * by this tool; intentionally left empty.
 *
 * @param data       User data pointer (unused).
 * @param wl_output  The output proxy (unused).
 * @param flags      Bitfield of @c wl_output_mode flags (unused).
 * @param width      Width of the mode in pixels (unused).
 * @param height     Height of the mode in pixels (unused).
 * @param refresh    Vertical refresh rate in mHz (unused).
 */
static void
display_handle_mode(void *data, struct wl_output *wl_output, uint32_t flags,
                    int width, int height, int refresh)
{
}

/**
 * @brief @c wl_output_listener callback: done event (stub).
 *
 * Sent by the compositor after all output properties for the current atomic
 * change have been transmitted.  Not needed by this tool; intentionally left
 * empty.
 *
 * @param data       User data pointer (unused).
 * @param wl_output  The output proxy (unused).
 */
static void
display_handle_done(void *data, struct wl_output *wl_output)
{
}

/**
 * @brief @c wl_output_listener callback: scale factor event (stub).
 *
 * Notifies the client of the output's scale factor (HiDPI multiplier).
 * Not needed by this tool; intentionally left empty.
 *
 * @param data       User data pointer (unused).
 * @param wl_output  The output proxy (unused).
 * @param factor     Integer scale factor (unused).
 */
static void
display_handle_scale(void *data, struct wl_output *wl_output, int32_t factor)
{
}

/**
 * @brief Listener vtable wired to every @c wl_output bound by this client.
 *
 * All four callbacks are stubs because output metadata (geometry, mode,
 * scale) is not required to send the @c activate_app message.
 */
static const struct wl_output_listener output_listener = {
        display_handle_geometry,
        display_handle_mode,
	display_handle_done,
	display_handle_scale,
};

/**
 * @brief Allocate and register a new output node.
 *
 * Called from registry_handle_global() whenever a @c wl_output global is
 * announced.  Binds the global at version 2, inserts the new
 * @ref window_output node at the head of @p display->output_list, and
 * attaches @ref output_listener.
 *
 * @param display  The owning display state.
 * @param id       The numeric name of the newly announced @c wl_output global.
 */
	static void
display_add_output(struct display *display, uint32_t id)
{
	struct window_output *w_output;

	w_output = calloc(1, sizeof(*w_output));
	w_output->display = display;
	w_output->output =
		wl_registry_bind(display->registry, id, &wl_output_interface, 2);

	wl_list_insert(&display->output_list, &w_output->link);

	wl_output_add_listener(w_output->output, &output_listener, display);
}

/**
 * @brief Release resources associated with a single output node.
 *
 * Removes @p w_output from @ref display::output_list and frees the
 * allocation.  Does @b not destroy the underlying @c wl_output proxy;
 * that is handled by the compositor side when the global is removed.
 *
 * @param w_output  The output node to destroy.  Must not be NULL.
 */
static void
destroy_output(struct window_output *w_output)
{
	wl_list_remove(&w_output->link);
	free(w_output);
}

/**
 * @brief @c wl_registry_listener callback: handle a new global.
 *
 * Invoked once for each global that the compositor advertises.  Binds
 * @c agl_shell_desktop at version 1 and each @c wl_output via
 * display_add_output().  All other globals are silently ignored.
 *
 * @param data       User data pointer; cast to @ref display.
 * @param registry   The registry proxy (unused beyond scoping).
 * @param id         Numeric name of the new global.
 * @param interface  Null-terminated interface name string.
 * @param version    Advertised interface version (unused; pinned at bind).
 */
static void
registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t id, const char *interface, uint32_t version)
{
	struct display *d = data;

	if (strcmp(interface, "agl_shell_desktop") == 0) {
		d->agl_shell_desktop =
			wl_registry_bind(registry, id, &agl_shell_desktop_interface, 1);
	} else if (strcmp(interface, "wl_output") == 0) {
		display_add_output(d, id);
	}
}

/**
 * @brief @c wl_registry_listener callback: handle a global removal (stub).
 *
 * Called when a previously announced global disappears (e.g., a monitor
 * is unplugged).  Dynamic removal is not handled by this tool; the callback
 * is intentionally left empty.
 *
 * @param data      User data pointer (unused).
 * @param registry  The registry proxy (unused).
 * @param name      Numeric name of the global being removed (unused).
 */
static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
			      uint32_t name)
{

}

/**
 * @brief Listener vtable for the Wayland global registry.
 *
 * Wires registry_handle_global() and registry_handle_global_remove()
 * into the @c wl_registry_listener interface.
 */
static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

/**
 * @brief Allocate and initialise the top-level display state.
 *
 * Performs the full Wayland connection and global discovery sequence:
 * -# Allocates a @ref display struct.
 * -# Connects to the compositor via @c wl_display_connect(NULL) (uses the
 *    default socket, respecting @c WAYLAND_DISPLAY / @c XDG_RUNTIME_DIR).
 * -# Retrieves the global registry and registers @ref registry_listener.
 * -# Calls @c wl_display_roundtrip() twice:
 *    - First round-trip delivers all @c wl_registry.global events so that
 *      @c agl_shell_desktop and each @c wl_output are bound.
 *    - Second round-trip delivers per-object events fired in response
 *      (e.g., @c wl_output geometry/mode/done/scale).
 * -# Logs a warning to @c stderr if @c agl_shell_desktop was not found.
 *
 * @return Pointer to the newly created @ref display, or calls @c exit(1)
 *         on allocation failure.
 *
 * @note The caller is responsible for calling destroy_display() when done.
 */
static struct display *
create_display(void)
{
	struct display *display;

	display = malloc(sizeof *display);
	if (display == NULL) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}

	wl_list_init(&display->output_list);
	display->display = wl_display_connect(NULL);
	assert(display->display);

	display->registry = wl_display_get_registry(display->display);

	wl_registry_add_listener(display->registry, &registry_listener, display);
	wl_display_roundtrip(display->display);

	if (display->agl_shell_desktop == NULL) {
		fprintf(stderr, "No agl_shell_desktop extension present\n");
	}

	wl_display_roundtrip(display->display);
	return display;
}

/**
 * @brief Tear down the display state and release all Wayland resources.
 *
 * -# Iterates @ref display::output_list safely (using
 *    @c wl_list_for_each_safe) and calls destroy_output() on every node.
 * -# Destroys the registry proxy.
 * -# Flushes any pending outbound messages with @c wl_display_flush().
 * -# Disconnects from the compositor with @c wl_display_disconnect().
 * -# Frees the @ref display struct itself.
 *
 * @param display  The display state to destroy.  Must not be NULL.
 *                 The pointer is invalid after this call returns.
 */
static void
destroy_display(struct display *display)
{
	struct window_output *w_output, *w_output_next;

	wl_list_for_each_safe(w_output, w_output_next, &display->output_list, link)
		destroy_output(w_output);

	wl_registry_destroy(display->registry);

	wl_display_flush(display->display);
	wl_display_disconnect(display->display);

	free(display);
}

/**
 * @brief Program entry point.
 *
 * Validates command-line arguments, establishes the Wayland connection,
 * sends the @c agl_shell_desktop_activate_app request for the given
 * @p app_id on the first available output, then tears down and exits.
 *
 * @par Exit codes
 * - @c EXIT_FAILURE  if fewer than two arguments are supplied, or if
 *                    @p argv[1] is an empty string.
 * - @c 0             on success (after the protocol message has been sent).
 *
 * @param argc  Argument count.  Must be >= 2.
 * @param argv  Argument vector.
 *              - @c argv[0]: program name (ignored).
 *              - @c argv[1]: the @c app_id of the application to activate.
 *
 * @return 0 on success, @c EXIT_FAILURE on invalid arguments.
 */
int main(int argc, char *argv[])
{
	struct display *display;
	const char *app_id = NULL;
	struct window_output *w_output;

	if (argc < 2) {
		exit(EXIT_FAILURE);
	}

	display = create_display();

	/* the app has to be already started, or not already active */
	app_id = argv[1];
	if (app_id && strlen(app_id) == 0)
		exit(EXIT_FAILURE);

	/* pick the first one available */
	w_output = wl_container_of(display->output_list.prev, w_output, link);
	agl_shell_desktop_activate_app(display->agl_shell_desktop, app_id,
					NULL, w_output->output);

	destroy_display(display);
	return 0;
}
