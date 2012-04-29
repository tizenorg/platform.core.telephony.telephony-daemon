/*
 * telephony-daemon
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Ja-young Gu <jygu@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <dlfcn.h>
#include <getopt.h>

#include <glib.h>
#include <glib-object.h>

#include <tcore.h>
#include <plugin.h>
#include <server.h>

#include "monitor.h"

static Server *_server;

static gboolean load_plugins(Server *s, const char *path, int flag_test_load)
{
	const gchar *file;
	char *filename;
	GDir *dir;
	void *handle;
	GSList *list;

	TcorePlugin *p;
	struct tcore_plugin_define_desc *desc;

	dir = g_dir_open(path, 0, NULL);
	if (!dir)
		return FALSE;

	while ((file = g_dir_read_name(dir)) != NULL) {
		if (g_str_has_prefix(file, "lib") == TRUE || g_str_has_suffix(file, ".so") == FALSE)
			continue;

		filename = g_build_filename(path, file, NULL);

		handle = dlopen(filename, RTLD_NOW);
		if (handle == NULL) {
			dbg("fail to load '%s': %s", filename, dlerror());
			g_free(filename);
			continue;
		}

		if (flag_test_load) {
			dbg("success to load '%s'", filename);
			dlclose(handle);
			g_free(filename);
			continue;
		}

		desc = dlsym(handle, "plugin_define_desc");
		if (!desc) {
			dbg("fail to load symbol: %s", dlerror());
			dlclose(handle);
			g_free(filename);
			continue;
		}

		if (desc->load) {
			if (desc->load() == FALSE) {
				dbg("false return from load(). skip this plugin");
				dlclose(handle);
				g_free(filename);
				continue;
			}
		}

		p = tcore_plugin_new(s, desc, filename, handle);
		tcore_server_add_plugin(s, p);

		dbg("plugin(%s) added", filename);
		g_free(filename);
	}
	g_dir_close(dir);

	list = tcore_server_ref_plugins(s);
	for (; list; list = list->next) {
		p = list->data;
		if (!p)
			continue;

		desc = (struct tcore_plugin_define_desc *)tcore_plugin_get_description(p);
		if (!desc)
			continue;

		if (!desc->init)
			continue;

		if (!desc->init(p)) {
			dbg("plugin(%s) init failed.", tcore_plugin_get_filename(p));
		}
	}

	return TRUE;
}

static void usage(const char *name)
{
	printf("Usage: %s [OPTION]... [PLUGIN_PATH]\n", name);
	printf("\n");
	printf("  -T, --testload\t run with plugin load test mode and exit\n");
	printf("  -h, --help\t\t display this help and exit\n");
	printf("\n");
}

static void on_signal_usr1(int signo)
{
	if (!_server)
		return;

	monitor_server_state(_server);
}

int main(int argc, char *argv[])
{
	struct sigaction sigact_usr1;
	Server *s;
	int flag_test_load=0;
	int opt;
	int opt_index;
	struct option options[] = {
			{ "help", 0, 0, 0 },
			{ "testload", 0, &flag_test_load, 1 },
			{ 0, 0, 0, 0 }
	};
	char *plugin_path = "/usr/lib/telephony/plugins/";

	sigact_usr1.sa_handler = on_signal_usr1;
	sigemptyset(&sigact_usr1.sa_mask);
	sigaddset(&sigact_usr1.sa_mask, SIGUSR1);
	sigact_usr1.sa_flags = 0;

	if (sigaction(SIGUSR1, &sigact_usr1, NULL) < 0) {
		warn("sigaction(SIGUSR1) failed.");
	}


	while (1) {
		opt = getopt_long(argc, argv, "hT", options, &opt_index);

		if (-1 == opt)
			break;

		switch (opt) {
			case 0:
				switch (opt_index) {
					case 0: // help
						usage(argv[0]);
						return 0;
						break;
				}
				break;

			case 'h':
				usage(argv[0]);
				return 0;
				break;

			case 'T':
				flag_test_load = 1;
				break;
		}
	}

	if (optind < argc) {
		plugin_path = argv[optind];
	}

	dbg("plugin_path: [%s]", plugin_path);
	dbg("flag[test_load]: %d", flag_test_load);

	g_type_init();
	g_thread_init(NULL);

	s = tcore_server_new();
	if (!s) {
		err("server_new failed.");
		goto end;
	}
	_server = s;

	if (!load_plugins(s, plugin_path, flag_test_load)) {
		goto free_end;
	}

	if (flag_test_load)
		goto free_end;

	if (tcore_server_run(s) == FALSE) {
		err("server_run failed.");
		goto free_end;
	}

	/*
	 * RUNNING
	 */

free_end:
	dbg("exit!");
	tcore_server_free(s);

end:
	return EXIT_SUCCESS;
}
