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

#include <systemd/sd-daemon.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <dlfcn.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>

#include <glib.h>
#include <glib-object.h>
#include <dlog.h>

#include <tcore.h>
#include <plugin.h>
#include <server.h>
#include <util.h>
#include <log.h>

#include "monitor.h"

#ifndef DAEMON_VERSION
#define DAEMON_VERSION "unknown"
#endif

static Server *_server;

void tcore_log(enum tcore_log_type type, enum tcore_log_priority priority, const char *tag, const char *fmt, ...)
{
	va_list ap;
	char buf[1024];

	va_start(ap, fmt);
	vsnprintf(buf, 1023, fmt, ap);
	va_end(ap);

	__dlog_print(type, priority, tag, buf);
}

static gboolean load_plugins(Server *s, const char *path, int flag_test_load)
{
	const gchar *file;
	char *filename;
	GDir *dir;
	void *handle;
	GSList *list;
	struct stat stat_buf;
	char file_date[27];

	TcorePlugin *p;
	struct tcore_plugin_define_desc *desc;

	if ((path == NULL) || (s == NULL))
		return FALSE;

	dir = g_dir_open(path, 0, NULL);
	if (dir == NULL)
		return FALSE;

	while ((file = g_dir_read_name(dir)) != NULL) {
		if (g_str_has_prefix(file, "lib") == TRUE
				|| g_str_has_suffix(file, ".so") == FALSE)
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
		if (desc == NULL) {
			dbg("fail to load symbol: %s", dlerror());
			dlclose(handle);
			g_free(filename);
			continue;
		}

		dbg("%s plugin", desc->name);
		dbg(" - path = %s", filename);
		dbg(" - version = %d", desc->version);
		dbg(" - priority = %d", desc->priority);

		memset(&stat_buf, 0, sizeof(struct stat));
		if (stat(filename, &stat_buf) == 0) {
			if (ctime_r(&stat_buf.st_mtime, file_date) != NULL) {
				if (strlen(file_date) > 1)
					file_date[strlen(file_date)-1] = '\0';

				dbg(" - date = %s", file_date);
			}
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

		dbg("%s added", desc->name);
		g_free(filename);
	}
	g_dir_close(dir);

	info("plugin load finished");

	list = tcore_server_ref_plugins(s);
	for (; list; list = list->next) {
		p = list->data;
		if (p == NULL)
			continue;

		desc = (struct tcore_plugin_define_desc *)tcore_plugin_get_description(p);
		if (desc == NULL)
			continue;

		if (desc->init == NULL)
			continue;

		if (desc->init(p) == FALSE) {
			dbg("plugin(%s) init failed.", tcore_plugin_get_filename(p));
		}
	}

	info("plugin init finished");

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
	if (_server == NULL)
		return;

	monitor_server_state(_server);
}

int main(int argc, char *argv[])
{
	struct sigaction sigact_usr1;
	Server *s;
	int flag_test_load = 0;
	int opt;
	int opt_index;
	struct option options[] = {
			{ "help", 0, 0, 0 },
			{ "testload", 0, &flag_test_load, 1 },
			{ 0, 0, 0, 0 }
	};
	char *plugin_path = "/usr/lib/telephony/plugins/";
	char *tcore_ver;
	struct sysinfo info;

	if (sysinfo(&info) == 0) {
		info("uptime: %ld secs", info.uptime);
	}

	info("daemon version: %s", DAEMON_VERSION);

	tcore_ver = tcore_util_get_version();
	info("libtcore version: %s", tcore_ver);
	free(tcore_ver);

	sigact_usr1.sa_handler = on_signal_usr1;
	sigemptyset(&sigact_usr1.sa_mask);
	sigaddset(&sigact_usr1.sa_mask, SIGUSR1);
	sigact_usr1.sa_flags = 0;

	if (sigaction(SIGUSR1, &sigact_usr1, NULL) < 0) {
		warn("sigaction(SIGUSR1) failed.");
	}

	while (1) {
		opt = getopt_long(argc, argv, "hT", options, &opt_index);

		if (opt == -1)
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

	if (optind < argc)
		plugin_path = argv[optind];

	info("plugin_path: [%s]", plugin_path);

#if !GLIB_CHECK_VERSION(2,35,0)
	g_type_init();
#endif
#if !GLIB_CHECK_VERSION (2, 31, 0)
	g_thread_init(NULL);
#endif

	s = tcore_server_new();
	if (s == NULL) {
		err("server_new failed.");
		goto end;
	}
	_server = s;

	if (load_plugins(s, plugin_path, flag_test_load) == FALSE)
		goto free_end;

	if (flag_test_load)
		goto free_end;

	info("server mainloop start");

	/* Notification to systemd */
	sd_notify(0, "READY=1");

	if (tcore_server_run(s) == FALSE) {
		err("server_run failed.");
	}

	/*
	 * RUNNING
	 */

free_end:
	info("server end");
	tcore_server_free(s);

end:
	return EXIT_SUCCESS;
}
