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

#include <glib.h>

#include "monitor.h"

#include <server.h>
#include <communicator.h>
#include <plugin.h>
//#include <hal.h>
#include <core_object.h>
#include <queue.h>
#include <storage.h>

#define NOTUSED(var) (var = var)

/* hacking TcoreQueue */
struct tcore_queue_type {
	TcorePlugin *plugin;
	GQueue *gq;
};

static void _hash_dump(gpointer key, gpointer value, gpointer user_data)
{
	NOTUSED(user_data);

	msg("         - %s: [%s]", key, value);
}

static void _monitor_core_objects(GSList *list)
{
	CoreObject *co;
	GHashTable *prop = NULL;

	while (list) {
		co = list->data;

		msg("       Type: 0x%x", tcore_object_get_type(co));
		msg("       - addr: %p", co);
		//msg("       - hal: %p", tcore_object_get_hal(co));

		//prop = tcore_object_ref_property_hash(co);
		if (prop) {
			msg("       - Properties: %d", g_hash_table_size(prop));
			g_hash_table_foreach(prop, _hash_dump, NULL);
		}

		list = g_slist_next(list);
	}
}

static void _monitor_plugin(Server *s)
{
	GSList *list;
	GSList *co_list = NULL;
	TcorePlugin *plugin;
	char *str;

	msg("-- Plugins --");

	list = tcore_server_ref_plugins(s);
	while (list) {
		plugin = list->data;
		if (plugin != NULL) {
			msg("Name: [%s]", tcore_plugin_get_description(plugin)->name);

			str = tcore_plugin_get_filename(plugin);
			if (str) {
				msg(" - file: %s", str);
				tcore_free(str);
			}

			msg(" - addr: %p", plugin);
			msg(" - userdata: %p", tcore_plugin_ref_user_data(plugin));

			co_list = tcore_plugin_ref_core_objects(plugin);
			if (co_list) {
				msg(" - core_object list: %d", g_slist_length(co_list));

				_monitor_core_objects(co_list);
				g_slist_free(co_list);
			}

			msg("");
		}

		list = g_slist_next(list);
	}
}

static void _monitor_storage(Server *s)
{
	GSList *list;
	TcoreStorage *strg;

	msg("-- Storages --");

	list = tcore_server_ref_storages(s);
	while (list) {
		strg = list->data;
		if (strg != NULL) {
			msg("Name: [%s]", tcore_storage_ref_name(strg));
			msg(" - addr: %p", strg);
			msg("");
		}

		list = g_slist_next(list);
	}
}

static void _monitor_communicator(Server *s)
{
	GSList *list;
	Communicator *comm;

	msg("-- Communicators --");

	list = tcore_server_ref_communicators(s);
	while (list) {
		comm = list->data;
		if (comm != NULL) {
			msg("Name: [%s]", tcore_communicator_ref_name(comm));
			msg(" - addr: %p", comm);
			msg(" - parent_plugin: %p", tcore_communicator_ref_plugin(comm));
			msg(" - userdata: %p", tcore_communicator_ref_user_data(comm));
			msg("");
		}

		list = g_slist_next(list);
	}
}

static void _monitor_modems(Server *s)
{
	GSList *list;
	TcorePlugin *plugin;

	msg("-- Modems --");

	list = tcore_server_ref_plugins(s);
	while (list) {
		plugin = list->data;
		if (plugin != NULL)
			tcore_server_print_modems(plugin);

		list = g_slist_next(list);
	}
}

void monitor_server_state(Server *s)
{
	_monitor_plugin(s);
	_monitor_storage(s);
	_monitor_communicator(s);
	_monitor_modems(s);
}
