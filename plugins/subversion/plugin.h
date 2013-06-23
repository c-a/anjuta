/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    plugin.h
    Copyright (C) 2004 Naba Kumar, Johannes Schmid

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

#ifndef PLUGIN_H
#define PLUGIN_H

#include <config.h>
#include <libanjuta/anjuta-plugin.h>
#include <libanjuta/anjuta-launcher.h>
#include <libanjuta/interfaces/ianjuta-message-manager.h>
#include <libanjuta/interfaces/ianjuta-editor.h>
#include <svn_client.h>
#include <gio/gio.h>
#include <libanjuta/interfaces/ianjuta-message-view.h>
#include <libanjuta/interfaces/ianjuta-document-manager.h>
#include <libanjuta/interfaces/ianjuta-file.h>
#include <libanjuta/interfaces/ianjuta-project-manager.h>
#include <libanjuta/interfaces/ianjuta-file-manager.h>
#include <libanjuta/anjuta-shell.h>
#include <libanjuta/anjuta-dock.h>
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/anjuta-command-queue.h>

extern GType subversion_get_type (GTypeModule *module);
#define ANJUTA_TYPE_PLUGIN_SUBVERSION         (subversion_get_type (NULL))
#define ANJUTA_PLUGIN_SUBVERSION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), ANJUTA_TYPE_PLUGIN_SUBVERSION, Subversion))
#define ANJUTA_PLUGIN_SUBVERSION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), ANJUTA_TYPE_PLUGIN_SUBVERSION, SubversionClass))
#define ANJUTA_IS_PLUGIN_SUBVERSION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ANJUTA_TYPE_PLUGIN_SUBVERSION))
#define ANJUTA_IS_PLUGIN_SUBVERSION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), ANJUTA_TYPE_PLUGIN_SUBVERSION))
#define ANJUTA_PLUGIN_SUBVERSION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), ANJUTA_TYPE_PLUGIN_SUBVERSION, SubversionClass))

typedef struct _Subversion Subversion;
typedef struct _SubversionClass SubversionClass;

#define GLADE_FILE PACKAGE_DATA_DIR"/glade/anjuta-subversion.ui"
#define ICON_FILE "anjuta-subversion-plugin-48.png"

struct _Subversion{
	AnjutaPlugin parent;
	
	IAnjutaMessageView* mesg_view;
	AnjutaLauncher* launcher;
	
	/* Merge ID */
	gint uiid;
	
	/* Action groups */
	GtkActionGroup *action_group;
	GtkActionGroup *popup_action_group;
	
	/* Watch IDs */
	gint fm_watch_id;
	gint project_watch_id;
	gint editor_watch_id;

	/* Watched values */
	gchar *fm_current_filename;
	gchar *project_root_dir;
	gchar *current_editor_filename;
	
	/* Log viewer */
	GtkBuilder *log_bxml;
	GtkWidget *log_viewer;

	/* Session settings */
	GSettings *session_settings;
	GList *svn_commit_logs;
};

struct _SubversionClass{
	AnjutaPluginClass parent_class;
};

void subversion_plugin_status_changed_emit(AnjutaCommand *command, guint return_code, Subversion *plugin);

#endif
