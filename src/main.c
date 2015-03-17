/*
 * telephony-daemon
 *
 * Copyright (c) 2013 Samsung Electronics Co. Ltd. All rights reserved.
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

#ifdef TIZEN_DEBUG_ENABLE
#include "monitor.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <dlfcn.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>

#include <glib.h>
#include <dlog.h>
#include <vconf.h>

#include <tcore.h>
#include <plugin.h>
#include <server.h>

#ifndef DAEMON_VERSION
#define DAEMON_VERSION "unknown"
#endif

#ifndef DEFAULT_PLUGINS_PATH
#define DEFAULT_PLUGINS_PATH "/usr/lib/telephony/plugins/"
#endif

/* Internal vconf to maintain telephony load info (Number of times telephony daemon is loaded) */
#define VCONFKEY_TELEPHONY_DAEMON_LOAD_COUNT "memory/private/telephony/daemon_load_count"

#define NOTUSED(var) (var = var)

static Server *_server = NULL;

static void __usage_info(const char *exec)
{
	printf("Usage: %s [OPTION]... [PLUGIN_PATH]\n", exec);
	printf("\n");
	printf("  -T, --testload\t run with plugin load test mode and exit\n");
	printf("  -h, --help\t\t display this help and exit\n");
	printf("\n");
}

void tcore_log(enum tcore_log_type type, enum tcore_log_priority priority, const char *tag, const char *fmt, ...)
{
	va_list ap;
	char buf[1024];

	switch (type) {
	case TCORE_LOG_TYPE_RADIO: {
		if (priority >= TCORE_LOG_INFO) {
			va_start(ap, fmt);
			vsnprintf(buf, 1023, fmt, ap);
			va_end(ap);
			__dlog_print(LOG_ID_RADIO, priority, tag, buf);
		} else {
		#ifdef TIZEN_DEBUG_ENABLE
			va_start(ap, fmt);
			vsnprintf(buf, 1023, fmt, ap);
			va_end(ap);
			__dlog_print(LOG_ID_RADIO, priority, tag, buf);
		#endif
		}
	} break;

	case TCORE_LOG_TYPE_TIME_CHECK: {
	#ifdef TIZEN_DEBUG_ENABLE /* User Mode should not log performance data */
		float a = 0.00, b = 0.00;
		int next = 0;
		FILE *fp = fopen("/proc/uptime", "r");
		g_return_if_fail(NULL != fp);

		if(fscanf(fp, "%f %f", &a, &b)){};
		fclose(fp);
		next = sprintf(buf, "[UPTIME] %f ", a);
		if (next < 0)
			return;

		va_start(ap, fmt);
		vsnprintf(buf + next, 1023 - next, fmt, ap);
		va_end(ap);
		__dlog_print(LOG_ID_RADIO, priority, tag, buf);
	#endif
	} break;

	default:
	break;
	}
}

static void glib_log(const gchar *log_domain, GLogLevelFlags log_level,
		const gchar *msg, gpointer user_data)
{
	NOTUSED(log_domain);
	NOTUSED(log_level);
	NOTUSED(user_data);

	__dlog_print (LOG_ID_RADIO, DLOG_ERROR, "GLIB", msg);
}

#ifdef TIZEN_DEBUG_ENABLE
static void telephony_signal_handler(int signo)
{
	if (!_server)
		return;

	switch (signo) {
	case SIGUSR1: {
		monitor_server_state(_server);
	} break;

	case SIGTERM: {
		tcore_server_exit(_server);
	} break;

	default: {
		warn("*~*~*~* Unhandled Signal: [%d] *~*~*~*", signo);
	} break;
	} /* end switch */

	return;
}
#endif

static void __log_uptime()
{
	float a = 0.00, b = 0.00;
	FILE *fp = fopen("/proc/uptime", "r");
	g_return_if_fail(NULL != fp);
	info("scanned %d items", fscanf(fp, "%f %f", &a, &b));
	info("proc uptime = %f idletime = %f\n", a, b);
	fclose(fp);
}

static gboolean __init_plugin(TcorePlugin *plugin)
{
	const struct tcore_plugin_define_desc *desc = tcore_plugin_get_description(plugin);

	if (!desc || !desc->init)
		return FALSE;

	if (!desc->init(plugin)) { /* TODO: Remove plugin from server */
		char *plugin_name = tcore_plugin_get_filename(plugin);
		if (NULL != plugin_name) {
			err("plugin(%s) init failed.", plugin_name);
			free(plugin_name);
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

	return TRUE;
}

static void *__load_plugin(const gchar *filename, struct tcore_plugin_define_desc **desc_out)
{
	void *handle = NULL;
	struct tcore_plugin_define_desc *desc = NULL;
	struct stat stat_buf;
	char file_date[27];

	handle = dlopen(filename, RTLD_LAZY);
	if (G_UNLIKELY(NULL == handle)) {
		err("fail to load '%s': %s", filename, dlerror());
		return NULL;
	}

	desc = dlsym(handle, "plugin_define_desc");
	if (G_UNLIKELY(NULL == desc)) {
		err("fail to load symbol: %s", dlerror());
		dlclose(handle);
		return NULL;
	}

	dbg("%s plugin", desc->name);
	dbg(" - path = %s", filename);
	dbg(" - version = %d", desc->version);
	dbg(" - priority = %d", desc->priority);

	memset(&stat_buf, 0x00, sizeof(stat_buf));
	memset(&file_date, '\0', sizeof(file_date));

	if (0 == stat(filename, &stat_buf)) {
		if (NULL != ctime_r(&stat_buf.st_mtime, file_date)) {
			if (1 < strlen(file_date))
				file_date[strlen(file_date)-1] = '\0';
			dbg(" - date = %s", file_date);
		}
	}

	if (G_LIKELY(desc->load)) {
		if (G_UNLIKELY(FALSE == desc->load())) {
			warn("false return from load(). skip this plugin");
			dlclose(handle);
			return NULL;
		}
	}

	if (NULL != desc_out)
		*desc_out = desc;

	return handle;
}

static gboolean load_plugins(Server *s, const char *path, gboolean flag_test_load)
{
	const gchar *file = NULL;
	gchar *filename = NULL;
	GDir *dir = NULL;
	void *handle = NULL;
	struct tcore_plugin_define_desc *desc = NULL;

	if (!path || !s)
		return FALSE;

	dir = g_dir_open(path, 0, NULL);
	if (!dir)
		return FALSE;

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
			dbg("success to load '%s'", filename);
			dlclose(handle);
			g_free(filename);
			continue;
		}

		tcore_server_add_plugin(s, tcore_plugin_new(s, desc, filename, handle));

		dbg("%s added", desc->name);
		g_free(filename);
	}
	g_dir_close(dir);

	return TRUE;
}

int main(int argc, char *argv[])
{
#ifdef TIZEN_DEBUG_ENABLE
	struct sigaction sigact;
#endif
	Server *s = NULL;
	gboolean flag_test_load = FALSE;
	int opt = 0, opt_index = 0, ret_code = EXIT_SUCCESS;
	int daemon_load_count = 0;
	struct option options[] = {
		{ "help", 0, 0, 0 },
		{ "testload", 0, &flag_test_load, 1 },
		{ 0, 0, 0, 0 }
	};
	const char *plugin_path = DEFAULT_PLUGINS_PATH;
	char *tcore_ver = NULL;
	struct sysinfo sys_info;

	TIME_CHECK("Starting Telephony");

	/* System Uptime */
	if (0 == sysinfo(&sys_info))
		info("uptime: %ld secs", sys_info.uptime);
	__log_uptime();

	/* Version Info */
	tcore_ver = tcore_util_get_version();
	info("daemon version: %s", DAEMON_VERSION);
	info("libtcore version: %s", tcore_ver);
	free(tcore_ver);
	info("glib version: %u.%u.%u", glib_major_version, glib_minor_version, glib_micro_version);

	/* Telephony reset handling*/
	vconf_get_int(VCONFKEY_TELEPHONY_DAEMON_LOAD_COUNT,&daemon_load_count);
	daemon_load_count++;
	vconf_set_int(VCONFKEY_TELEPHONY_DAEMON_LOAD_COUNT,daemon_load_count);
	dbg("daemon load count = [%d]", daemon_load_count);

#ifdef TIZEN_DEBUG_ENABLE
	/* Signal Registration */
	sigact.sa_handler = telephony_signal_handler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	if (sigaction(SIGTERM, &sigact, NULL) < 0)
		warn("sigaction(SIGTERM) failed.");
	if (sigaction(SIGUSR1, &sigact, NULL) < 0)
		warn("sigaction(SIGUSR1) failed.");
#endif

	/* Commandline option parser TODO: Replace with GOptionContext */
	while (TRUE) {
		opt = getopt_long(argc, argv, "hT", options, &opt_index);

		if (-1 == opt)
			break;

		switch (opt) {
		case 0: {
			switch (opt_index) {
			case 0: {
				__usage_info(argv[0]);
				return 0;
			} break;
			default: {
				warn("unhandled opt_index.");
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
		default: {
			warn("unhandled opt case.");
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
		ret_code = EXIT_FAILURE;
		goto END;
	}
	_server = s;

	g_log_set_default_handler(glib_log, s);

	/* Load Plugins */
	if (G_UNLIKELY(FALSE == load_plugins(s, (const char *)plugin_path, flag_test_load))) {
		err("load_plugins failed.");
		ret_code = EXIT_FAILURE;
		goto END;
	}

	TIME_CHECK("Loading Plugins Complete");

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

	info("server mainloop start");
	TIME_CHECK("Initializing Plugins Complete. Starting Daemon");

	if (G_UNLIKELY(TCORE_RETURN_SUCCESS != tcore_server_run(s))) {
		err("server_run failed.");
		ret_code = EXIT_FAILURE;
	}

END:
	tcore_server_free(s); _server = NULL;
	return ret_code;
}
