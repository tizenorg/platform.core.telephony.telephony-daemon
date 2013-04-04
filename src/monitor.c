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
#include <hal.h>
#include <queue.h>
#include <storage.h>
#include <communicator.h>
#include <user_request.h>

#include "monitor.h"

/* Hacking TcoreQueue */
struct tcore_queue_type {
	TcorePlugin *plugin;
	GQueue *gq;
};

static void _monitor_plugin(Server *s)
{
	GSList *list;
	TcorePlugin *p;
	char *str;

	msg("-- Plugins --");

	list = tcore_server_ref_plugins(s);
	if (list == NULL)
		return;

	do {
		p = list->data;

		msg("Name: [%s]", tcore_plugin_get_description(p)->name);

		str = tcore_plugin_get_filename(p);
		msg(" - file: %s", str);
		if (str)
			free(str);

		msg(" - addr: %p", p);
		msg(" - userdata: %p", tcore_plugin_ref_user_data(p));
		msg("");


		list = list->next;
	} while (list);
}

static void _monitor_storage(Server *s)
{
	GSList *list;
	Storage *strg;

	msg("-- Storages --");

	list = tcore_server_ref_storages(s);
	if (list == NULL)
		return;

	do {
		strg = list->data;

		msg("Name: [%s]", tcore_storage_ref_name(strg));
		msg(" - addr: %p", strg);
		msg("");

		list = list->next;
	} while (list);
}

static void _monitor_communicator(Server *s)
{
	GSList *list;
	Communicator *comm;

	msg("-- Communicators --");

	list = tcore_server_ref_communicators(s);
	if (list == NULL)
		return;

	do {
		comm = list->data;

		msg("Name: [%s]", tcore_communicator_ref_name(comm));
		msg(" - addr: %p", comm);
		msg(" - parent_plugin: %p", tcore_communicator_ref_plugin(comm));
		msg(" - userdata: %p", tcore_communicator_ref_user_data(comm));
		msg("");

		list = list->next;
	} while (list);
}

static void _monitor_modems(Server *s)
{
	GSList *list;
	TcorePlugin *p;

	msg("-- Modems --");

	list = tcore_server_ref_plugins(s);
	if (list == NULL)
		return;

	for (; list != NULL; list = g_slist_next(list)) {
		p = list->data;
		if (p == NULL)
			continue;

		tcore_server_print_modems(p);
	}
}

void monitor_server_state(Server *s)
{
	_monitor_plugin(s);
	_monitor_storage(s);
	_monitor_communicator(s);
	_monitor_modems(s);
}
