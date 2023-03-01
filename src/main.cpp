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

#include <unistd.h>
#include <wayland-client.h>
#include <wayland-util.h>

#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(array) (sizeof(array) / sizeof(array[0]))
#endif

#include "AglShellGrpcClient.h"

enum action_state {
	NONE 		= -1,
	ACTIVATE,
	DEACTIVATE,
	SET_FLOAT,
	SET_SPLIT,
	SET_NORMAL,
	GETOUTPUTS,
	SET_FULLSCREEN,
};

static struct action {
	enum action_state action;
	const char *name;
} actions[] = {
	{ ACTIVATE,		"ACTIVATE" 	},
	{ DEACTIVATE,		"DEACTIVATE" 	},
	{ GETOUTPUTS,		"GETOUTPUTS" 	},
	{ SET_FLOAT,		"FLOAT" 	},
	{ SET_NORMAL,		"NORMAL" 	},
	{ SET_FULLSCREEN,	"FULLSCREEN" 	},
};

static int
get_action(const char *action_name)
{
	size_t i;

	for (i = 0; i < ARRAY_LENGTH(actions); i++) {
		if (action_name && strcasecmp(action_name, actions[i].name) == 0)
			return actions[i].action;
	}

	return -1;
}

static void
run_in_thread(GrpcClient *client)
{
	grpc::Status status = client->Wait();
}

static void
help(char **argv)
{
	fprintf(stderr, "Usage: %s [-a action] [-p app_id] [-o output_name] [-l]\n",
			argv[0]);
	fprintf(stderr, "\t-a -- action activate|deactivate|float|normal|getoutputs|fullscreen\n");
	fprintf(stderr, "\t-p -- app_id application_id\n");
	fprintf(stderr, "\t-o -- output_name one of the outputs from getoutputs action\n");
	fprintf(stderr, "\t-l -- continuously listen for window state events\n");
	exit(EXIT_FAILURE);
}

static void
app_status_callback(::agl_shell_ipc::AppStateResponse app_response)
{
	std::cout << " >> AppStateResponse app_id " <<
		app_response.app_id() << ", with state " <<
		app_response.state() << std::endl;
}

static void
read_outputs(GrpcClient *client)
{
	std::vector<std::string> outputs = client->GetOutputs();

	if (outputs.empty())
		return;

	for (size_t i = 0; i < outputs.size(); i++)
		fprintf(stderr, " %s ", outputs[i].c_str());

	fprintf(stderr, "\n");
}

int main(int argc, char *argv[])
{
	char *output = NULL;
	char *action = NULL;
	char *app_id = NULL;
	int opt;
	bool listen_flag = false;
	std::thread th;

	// app_id, output p[0] -> name, p[1] action, p[2] app_id, p[3] -> output
	while ((opt = getopt(argc, argv, "a:p:o:lsh")) != -1) {
		switch (opt) {
		case 'p':
			app_id = optarg;
			break;
		case 'a':
			action = optarg;
			break;
		case 'o':
			output = optarg;
			break;
		case 'l':
			listen_flag = true;
			break;
		case 'h':
		default: /* '?' */
			help(argv);
		}
	}

	if (!action && !listen_flag) {
		help(argv);
		exit(EXIT_FAILURE);
	}

	// start grpc connection
	GrpcClient *client = new GrpcClient();

	if (listen_flag) {
		fprintf(stderr, "Listening for events...\n");
		th = std::thread(run_in_thread, client);
		client->AppStatusState(app_status_callback);
	}

	switch (get_action(action)) {
	case ACTIVATE:
		if (!output && !app_id) {
			fprintf(stderr, "Activation require both an app_id and an output\n");
			help(argv);
		}
		fprintf(stderr, "Activating application '%s' on output '%s'\n",
				app_id, output);
		client->ActivateApp(std::string(app_id), std::string(output));
		break;
	case DEACTIVATE:
		if (!app_id) {
			fprintf(stderr, "Deactivation require an app_id\n");
			help(argv);
		}
		fprintf(stderr, "Deactivating application '%s'\n", app_id);
		client->DeactivateApp(std::string(app_id));
		break;
	case SET_FLOAT:
		if (!app_id) {
			fprintf(stderr, "Float require an app_id\n");
			help(argv);
		}
		fprintf(stderr, "Floating application '%s'\n", app_id);
		client->SetAppFloat(std::string(app_id), 40, 300);
		break;
	case SET_FULLSCREEN:
		if (!app_id) {
			fprintf(stderr, "Fullscreen require an app_id\n");
			help(argv);
		}
		fprintf(stderr, "Fullscreened application '%s'\n", app_id);
		client->SetAppFullscreen(std::string(app_id));
		break;
	case SET_NORMAL:
		if (!app_id) {
			fprintf(stderr, "Normal/maximized require an app_id\n");
			help(argv);
		}
		fprintf(stderr, "Maximizing application '%s'\n", app_id);
		client->SetAppNormal(std::string(app_id));
		break;
	case GETOUTPUTS:
		read_outputs(client);
		break;
	default:
		// allow listen flag to be passed
		if (listen_flag)
			break;

		fprintf(stderr, "Unknown action passed. Possible actions:\n\t");
		for (size_t i = 0; i < ARRAY_LENGTH(actions); i++)
			fprintf(stderr, " %s ", actions[i].name);

		help(argv);
		break;
	}

	if (listen_flag) {
		th.join();
	}

	return 0;
}
