/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    plugin.h
    Copyright (C) 2000 Naba Kumar

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#include <libanjuta/anjuta-plugin.h>
#include <libanjuta/interfaces/ianjuta-buildable.h>
#include <libanjuta/interfaces/ianjuta-editor.h>

#include "configuration-list.h"
#include "program.h"

#define BUILDER_FILE PACKAGE_DATA_DIR "/glade/anjuta-build-basic-autotools-plugin.ui"

extern GType basic_autotools_plugin_get_type (GTypeModule *module);
#define ANJUTA_TYPE_PLUGIN_BASIC_AUTOTOOLS         (basic_autotools_plugin_get_type (NULL))
#define ANJUTA_PLUGIN_BASIC_AUTOTOOLS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), ANJUTA_TYPE_PLUGIN_BASIC_AUTOTOOLS, BasicAutotoolsPlugin))
#define ANJUTA_PLUGIN_BASIC_AUTOTOOLS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), ANJUTA_TYPE_PLUGIN_BASIC_AUTOTOOLS, BasicAutotoolsPluginClass))
#define ANJUTA_IS_PLUGIN_BASIC_AUTOTOOLS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ANJUTA_TYPE_PLUGIN_BASIC_AUTOTOOLS))
#define ANJUTA_IS_PLUGIN_BASIC_AUTOTOOLS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), ANJUTA_TYPE_PLUGIN_BASIC_AUTOTOOLS))
#define ANJUTA_PLUGIN_BASIC_AUTOTOOLS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), ANJUTA_TYPE_PLUGIN_BASIC_AUTOTOOLS, BasicAutotoolsPluginClass))

typedef struct _BasicAutotoolsPlugin BasicAutotoolsPlugin;
typedef struct _BasicAutotoolsPluginClass BasicAutotoolsPluginClass;

struct _BasicAutotoolsPlugin{
	AnjutaPlugin parent;

	/* Build contexts pool */
	GList *contexts_pool;

	/* Watch IDs */
	gint fm_watch_id;
	gint pm_watch_id;
	gint project_root_watch_id;
	gint project_build_watch_id;
	gint editor_watch_id;

	/* GSource ids */
	guint update_indicators_idle;

	/* Watched values */
	GFile *fm_current_file;
	GFile *pm_current_file;
	GFile *current_editor_file;
	GFile *project_root_dir;
	GFile *project_build_dir;
	IAnjutaEditor *current_editor;

	/* UI */
	gint build_merge_id;
	GtkActionGroup *build_action_group;
	GtkActionGroup *build_popup_action_group;
	GtkWidget *configuration_menu;

	/* commands overrides */
	gchar *commands[IANJUTA_BUILDABLE_N_COMMANDS];

	/* Build parameters */
	BuildConfigurationList *configurations;

	/* Execution parameters */
	gchar *program_args;
	gboolean run_in_terminal;
	gchar *last_exec_uri;

	/* Editors that have been created so far */
	GHashTable *editors_created;

	/* Settings */
	GSettings *settings;
	GSettings *session_settings;
};

struct _BasicAutotoolsPluginClass{
	AnjutaPluginClass parent_class;
};

typedef struct _BuildContext BuildContext;

BuildContext* build_get_context (BasicAutotoolsPlugin *plugin, const gchar *dir, gboolean with_view, gboolean check_passwd);
void build_context_destroy (BuildContext *context);

void build_set_command_in_context (BuildContext* context, BuildProgram *prog);
gboolean build_execute_command_in_context (BuildContext* context, GError **err);
gboolean build_save_and_execute_command_in_context (BuildContext* context, GError **err);
const gchar *build_context_get_work_dir (BuildContext* context);
AnjutaPlugin *build_context_get_plugin (BuildContext* context);


void build_update_configuration_menu (BasicAutotoolsPlugin *plugin);

#endif
