/*
 * telephony-daemon
 *
 * Copyright 2013 Samsung Electronics Co. Ltd.
 * Copyright 2013 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <dlfcn.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <systemd/sd-daemon.h>

#include <glib.h>
#include <dlog.h>

#ifdef ENABLE_MONITOR
#include "monitor.h"
#endif

#include <tcore.h>
#include <server.h>
#include <plugin.h>

#ifndef DAEMON_VERSION
#define DAEMON_VERSION "unknown"
#endif

#ifndef DEFAULT_PLUGINS_PATH
#define DEFAULT_PLUGINS_PATH "/usr/lib/telephony/plugins/"
#endif

#define NOTUSED(var) (var = var)

static Server *_server = NULL;

static void __usage_info(const gchar *exec)
{
	printf("Usage: %s [OPTION]... [PLUGIN_PATH]\n", exec);
	printf("\n");
	printf("  -T, --testload\t run with plugin load test mode and exit\n");
	printf("  -h, --help\t\t display this help and exit\n");
	printf("\n");
}

void tcore_log(enum tcore_log_type type, enum tcore_log_priority priority, const gchar *tag, const gchar *fmt, ...)
{
	va_list ap;
	gchar buf[1024];

	va_start(ap, fmt);
	vsnprintf(buf, 1023, fmt, ap);
	va_end(ap);

	__dlog_print(type, priority, tag, buf);
}

static void glib_log(const gchar *log_domain, GLogLevelFlags log_level,
		const gchar *msg, gpointer user_data)
{
	NOTUSED(log_domain);
	NOTUSED(log_level);
	NOTUSED(user_data);

	__dlog_print (LOG_ID_RADIO, DLOG_ERROR, "GLIB", msg);
}

#ifdef ENABLE_MONITOR
static void telephony_signal_handler(gint signo)
{
	if (_server == NULL) {
		err("Server is NULL");
		return;
	}

	switch (signo) {
	case SIGUSR1: {
		monitor_server_state(_server);
	} break;

	case SIGTERM: {
		tcore_server_free(_server);
	} break;

	default: {
		warn("*~*~*~* Unhandled Signal: [%d] *~*~*~*", signo);
	} break;
	} /* end switch */
}
#endif

static void __log_uptime()
{
	float a = 0.00, b = 0.00;
	FILE *fp = fopen("/proc/uptime", "r");
	g_return_if_fail(NULL != fp);

	info("Scanned %d items", fscanf(fp, "%f %f", &a, &b));
	info("proc uptime = %f idletime = %f\n", a, b);

	fclose(fp);
}

static gboolean __init_plugin(TcorePlugin *plugin)
{
	const struct tcore_plugin_define_desc *desc = tcore_plugin_get_description(plugin);

	if ((desc == NULL) || (desc->init == NULL)) {
		err("desc: [%p] desc->init: [%p]", desc, (desc ? desc->init : NULL));
		return FALSE;
	}

	if (desc->init(plugin) == FALSE) {		/* TODO: Remove plugin from server */
		gchar *plugin_name = tcore_plugin_get_filename(plugin);
		if (plugin_name != NULL) {
			err("Plug-in '%s' init failed!!!", plugin_name);
			tcore_free(plugin_name);
		}
		return FALSE;
	}

	return TRUE;
}

static gboolean init_plugins(Server *s)
{
	GSList *list = tcore_server_ref_plugins(s);

	while (list != NULL) {
		if (G_UNLIKELY(FALSE == __init_plugin(list->data))) {
			list = g_slist_next(list);
			continue;
		}
		list = g_slist_next(list);
	}

	info("[TIME_CHECK] plugin init finished");
	return TRUE;
}

static void *__load_plugin(const gchar *filename, struct tcore_plugin_define_desc **desc_out)
{
	void *handle;
	struct tcore_plugin_define_desc *desc;
	struct stat stat_buf;
	gchar file_date[27];

	handle = dlopen(filename, RTLD_NOW);
	if (G_UNLIKELY(NULL == handle)) {
		err("Failed to open '%s': %s", filename, dlerror());
		return NULL;
	}

	desc = dlsym(handle, "plugin_define_desc");
	if (G_UNLIKELY(NULL == desc)) {
		err("Failed to load symbol: %s", dlerror());

		dlclose(handle);
		return NULL;
	}

	dbg("'%s' plugin", desc->name);
	dbg(" - path = %s", filename);
	dbg(" - version = %d", desc->version);
	dbg(" - priority = %d", desc->priority);

	memset(&stat_buf, 0x00, sizeof(stat_buf));
	memset(&file_date, '\0', sizeof(file_date));

	if (stat(filename, &stat_buf) == 0) {
		if (ctime_r(&stat_buf.st_mtime, file_date) != NULL) {
			if (strlen(file_date) > 1)
				file_date[strlen(file_date)-1] = '\0';

			dbg(" - date = %s", file_date);
		}
	}

	if (G_LIKELY(desc->load)) {
		if (G_UNLIKELY(desc->load() == FALSE)) {
			warn("Failed to load Plug-in");

			dlclose(handle);
			return NULL;
		}
	}

	if (desc_out != NULL)
		*desc_out = desc;

	return handle;
}

static gboolean load_plugins(Server *s, const gchar *path, gboolean flag_test_load)
{
	const gchar *file = NULL;
	gchar *filename = NULL;
	GDir *dir = NULL;
	void *handle = NULL;
	struct tcore_plugin_define_desc *desc = NULL;

	if ((path == NULL) || (s == NULL)) {
		err("path: [%p] s: [%p]", path, s);
		return FALSE;
	}

	dir = g_dir_open(path, 0, NULL);
	if (dir == NULL) {
		err("Failed to open directory '%s'", path);
		return FALSE;
	}

	while ((file = g_dir_read_name(dir)) != NULL) {
		if (g_str_has_prefix(file, "lib") == TRUE
				|| g_str_has_suffix(file, ".so") == FALSE)
			continue;

		filename = g_build_filename(path, file, NULL);

		/* Load a plugin */
		if (G_UNLIKELY((handle = __load_plugin(filename, &desc)) == NULL)) {
			g_free(filename);
			continue;
		}

		/* Don't add to server if flag_test_load */
		if (flag_test_load) {
			dbg("Loading '%s' - Successful", filename);

			dlclose(handle);
			g_free(filename);
			continue;
		}

		/* Add Plug-in to Server Plug-in list */
		tcore_server_add_plugin(s, tcore_plugin_new(s, desc, filename, handle));
		dbg("'%s' added", desc->name);

		g_free(filename);
	}

	g_dir_close(dir);
	info("[TIME_CHECK] Plug-in load finished");

	return TRUE;
}

gint main(gint argc, gchar *argv[])
{
#ifdef ENABLE_MONITOR
	struct sigaction sigact;
#endif
	Server *s;
	gboolean flag_test_load = FALSE;
	gint opt = 0, opt_index = 0, ret_code = EXIT_SUCCESS;
	struct option options[] = {
		{ "help", 0, 0, 0 },
		{ "testload", 0, &flag_test_load, 1 },
		{ 0, 0, 0, 0 }
	};
	gchar *plugin_path = DEFAULT_PLUGINS_PATH;
	gchar *tcore_ver = NULL;
	struct sysinfo sys_info;

	/* System Uptime */
	if (sysinfo(&sys_info) == 0)
		info("uptime: %ld secs", sys_info.uptime);
	__log_uptime();

	/* Version Info */
	tcore_ver = tcore_util_get_version();
	info("daemon version: %s", DAEMON_VERSION);
	info("libtcore version: %s", tcore_ver);
	tcore_free(tcore_ver);
	info("glib version: %u.%u.%u", glib_major_version, glib_minor_version, glib_micro_version);

#ifdef ENABLE_MONITOR
	/* Signal Registration */
	sigact.sa_handler = telephony_signal_handler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	if (sigaction(SIGTERM, &sigact, NULL) < 0)
		warn("sigaction(SIGTERM) failed.");
	if (sigaction(SIGUSR1, &sigact, NULL) < 0)
		warn("sigaction(SIGUSR1) failed.");

	/* Additional signals for dedugging the cause of Telephony crash */
	if (sigaction(SIGINT, &sigact, NULL) < 0)
		warn("sigaction(SIGINT) failed.");
	if (sigaction(SIGABRT, &sigact, NULL) < 0)
		warn("sigaction(SIGABRT) failed.");
	if (sigaction(SIGHUP, &sigact, NULL) < 0)
		warn("sigaction(SIGHUP) failed.");
	if (sigaction(SIGSEGV, &sigact, NULL) < 0)
		warn("sigaction(SIGSEGV) failed.");
	if (sigaction(SIGXCPU, &sigact, NULL) < 0)
		warn("sigaction(SIGXCPU) failed.");
	if (sigaction(SIGQUIT, &sigact, NULL) < 0)
		warn("sigaction(SIGQUIT) failed.");
#endif

	/* Commandline option parser TODO: Replace with GOptionContext */
	while (TRUE) {
		opt = getopt_long(argc, argv, "hT", options, &opt_index);
		if (opt == -1)
			break;

		switch (opt) {
		case 0: {
			switch (opt_index) {
			case 0: {
				__usage_info(argv[0]);
				return 0;
			} break;
			} /* end switch */
		} break;

		case 'h': {
			__usage_info(argv[0]);
			return 0;
		} break;

		case 'T': {
			flag_test_load = TRUE;
		} break;
		} /* end switch */
	}

	if (optind < argc)
		plugin_path = argv[optind];

	info("plugin_path: [%s]", plugin_path);

#if !GLIB_CHECK_VERSION(2, 35, 0)
	g_type_init();
#endif
#if !GLIB_CHECK_VERSION(2, 31, 0)
	g_thread_init(NULL);
#endif

	s = tcore_server_new();
	if (G_UNLIKELY(NULL == s)) {
		err("server_new failed.");
		return EXIT_FAILURE;
	}
	_server = s;

	g_log_set_default_handler(glib_log, s);

	/* Load Plugins */
	if (G_UNLIKELY(FALSE == load_plugins(s, (const gchar *)plugin_path, flag_test_load))) {
		err("load_plugins failed.");
		ret_code = EXIT_FAILURE;
		goto END;
	}

	if (flag_test_load) {
		ret_code = EXIT_SUCCESS;
		goto END;
	}

	/* Initialize Plugins */
	if (G_UNLIKELY(FALSE == init_plugins(s))) {
		err("init_plugins failed.");
		ret_code = EXIT_FAILURE;
		goto END;
	}

	info("Server mainloop start");

	/* Notification to systemd */
	sd_notify(0, "READY=1");

	/* Server Run */
	if (G_UNLIKELY(FALSE == tcore_server_run(s))) {
		err("Server_run - Failed!!!");
		ret_code = EXIT_FAILURE;
	}

END:
	tcore_server_free(s);

	return ret_code;
}
