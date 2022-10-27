/*
 * Copyright 2021, 2022 Collabora, Ltd.
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
#include <thread>

#include <wayland-client.h>
#include <wayland-util.h>

#include "AglShellGrpcClient.h"

static int running = 1;
const char *app_id_to_activate = NULL;

static void
run_in_thread(GrpcClient *client)
{
	grpc::Status status = client->Wait();
}

int main(int argc, char *argv[])
{
	struct display *display;
	struct window_output *w_output = NULL;
	char *output_name = NULL;
	int ret = 0;
	bool use_grpc_activation = false;

	if (argc < 2) {
		exit(EXIT_FAILURE);
	}

	if (argc == 3) {
		output_name = argv[2];
	}

	if (argc == 4 && strcmp(argv[3], "use-grpc") == 0)
		use_grpc_activation = true;


	display = create_display();

	/* the app has to be already started, or not already active */
	app_id_to_activate = argv[1];
	if (app_id_to_activate && strlen(app_id_to_activate) == 0)
		exit(EXIT_FAILURE);

	if (output_name) {
		struct window_output *woutput;

		wl_list_for_each(woutput, &display->output_list, link) {
			if (woutput->name && !strcmp(woutput->name, output_name)) {
				w_output = woutput;
				break;
			}
		}
	} else {
		w_output = wl_container_of(display->output_list.prev, w_output, link);
	}

	/* maybe that output doesn't exit, still allow a fallback */
	if (!w_output)
		w_output = wl_container_of(display->output_list.prev, w_output, link);

	fprintf(stderr, "Activating application '%s' on output '%s'\n",
			app_id_to_activate, w_output->name ?
			w_output->name : "first default output");

	if (use_grpc_activation) {
		fprintf(stderr, "using grpc\n");
		GrpcClient *client = new GrpcClient();

		// this blocks in wait
		std::thread thread(run_in_thread, client);

		// this does the actually call to get events
		client->AppStatusState();

		client->ActivateApp(std::string(app_id_to_activate),
		                    std::string(w_output->name));
		thread.join();
	} else {
		fprintf(stderr, "using agl_shell_desktop\n");
		agl_shell_desktop_activate_app(display->agl_shell_desktop, 
				app_id_to_activate, NULL, w_output->output);

		while (running && ret != -1) {
			ret = wl_display_dispatch(display->display);
		}
	}

	destroy_display(display);
	return 0;
}
