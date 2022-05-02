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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>

#include <wayland-client.h>
#include <wayland-util.h>
#include "agl-shell-desktop-client-protocol.h"

static int running = 1;
const char *app_id_to_activate = NULL;

struct display;

struct window_output {
	struct display *display;
        struct wl_output *output;
        struct wl_list link;	/** display::output_list */
};

struct display {
	struct wl_display *display;
	struct wl_registry *registry;
	struct agl_shell_desktop *agl_shell_desktop;

	struct wl_list output_list;	/** window_output::link */
};

static void
display_handle_geometry(void *data, struct wl_output *wl_output,
                        int x, int y, int physical_width, int physical_height,
                        int subpixel, const char *make, const char *model, int transform)
{
}

static void
display_handle_mode(void *data, struct wl_output *wl_output, uint32_t flags,
                    int width, int height, int refresh)
{
}

static void
display_handle_done(void *data, struct wl_output *wl_output)
{
}

static void
display_handle_scale(void *data, struct wl_output *wl_output, int32_t factor)
{
}

static const struct wl_output_listener output_listener = {
        display_handle_geometry,
        display_handle_mode,
	display_handle_done,
	display_handle_scale,
};


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

static void
destroy_output(struct window_output *w_output)
{
	wl_list_remove(&w_output->link);
	free(w_output);
}

static void app(void *data, struct agl_shell_desktop *agl_shell_desktop,
		const char *appid)
{
		/* UNUSED */
}

static void state_app(void *data, struct agl_shell_desktop *agl_shell_desktop,
		      const char *app_id, const char *app_data, uint32_t state,
		      uint32_t role)
{
	if (strcmp(app_id, app_id_to_activate) == 0 &&
	    state == AGL_SHELL_DESKTOP_APP_STATE_ACTIVATED) {
		running = 0;
	}
}

static const struct agl_shell_desktop_listener desktop_shell_listener = {
	app,
	state_app,
};

static void
registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t id, const char *interface, uint32_t version)
{
	struct display *d = data;

	if (strcmp(interface, "agl_shell_desktop") == 0) {
		d->agl_shell_desktop =
			wl_registry_bind(registry, id, &agl_shell_desktop_interface, 1);
		agl_shell_desktop_add_listener(d->agl_shell_desktop,
					       &desktop_shell_listener, d);
	} else if (strcmp(interface, "wl_output") == 0) {
		display_add_output(d, id);
	}
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
			      uint32_t name)
{

}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

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

int main(int argc, char *argv[])
{
	struct display *display;
	struct window_output *w_output;
	int ret = 0;

	if (argc < 2) {
		exit(EXIT_FAILURE);
	}

	display = create_display();

	/* the app has to be already started, or not already active */
	app_id_to_activate = argv[1];
	if (app_id_to_activate && strlen(app_id_to_activate) == 0)
		exit(EXIT_FAILURE);

	/* pick the first one available */
	w_output = wl_container_of(display->output_list.prev, w_output, link);
	agl_shell_desktop_activate_app(display->agl_shell_desktop, app_id_to_activate,
					NULL, w_output->output);

	while (running && ret != -1) {
		ret = wl_display_dispatch(display->display);
	}

	destroy_display(display);
	return 0;
}
