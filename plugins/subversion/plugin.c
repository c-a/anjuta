/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    plugin.c
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

#include <libanjuta/anjuta-shell.h>
#include <libanjuta/anjuta-preferences.h>
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/interfaces/ianjuta-file.h>
#include <libanjuta/interfaces/ianjuta-document-manager.h>
#include <libanjuta/interfaces/ianjuta-file-manager.h>
#include <libanjuta/interfaces/ianjuta-project-manager.h>
#include <libanjuta/interfaces/ianjuta-vcs.h>

#include "plugin.h"
#include "subversion-vcs-interface.h"
#include "subversion-add-dialog.h"
#include "subversion-remove-dialog.h"
#include "subversion-commit-dialog.h"
#include "subversion-update-dialog.h"
#include "subversion-revert-dialog.h"
#include "subversion-log-dialog.h"
#include "subversion-diff-dialog.h"
#include "subversion-copy-dialog.h"
#include "subversion-switch-dialog.h"
#include "subversion-merge-dialog.h"
#include "subversion-resolve-dialog.h"

#define UI_FILE PACKAGE_DATA_DIR"/ui/anjuta-subversion.xml"

static gpointer parent_class;

static GtkActionEntry actions_subversion[] = {
	{
		"ActionMenuSubversion",                       /* Action name */
		NULL,                            /* Stock icon, if any */
		N_("_Subversion"),                     /* Display label */
		NULL,                                     /* short-cut */
		NULL,                      /* Tooltip */
		NULL
	},
	{
		"ActionSubversionAdd",                       /* Action name */
		GTK_STOCK_ADD,                            /* Stock icon, if any */
		N_("_Add…"),                     /* Display label */
		NULL,                                     /* short-cut */
		N_("Add a new file/directory to the Subversion tree"),                      /* Tooltip */
		G_CALLBACK (on_menu_subversion_add)    /* action callback */
	},
	{
		"ActionSubversionRemove",                       /* Action name */
		GTK_STOCK_REMOVE,                            /* Stock icon, if any */
		N_("_Remove…"),                     /* Display label */
		NULL,                                     /* short-cut */
		N_("Remove a file/directory from Subversion tree"),                      /* Tooltip */
		G_CALLBACK (on_menu_subversion_remove)    /* action callback */
	},
	{
		"ActionSubversionCommit",                       /* Action name */
		GTK_STOCK_YES,                            /* Stock icon, if any */
		N_("_Commit…"),                     /* Display label */
		NULL,                                     /* short-cut */
		N_("Commit your changes to the Subversion tree"),                      /* Tooltip */
		G_CALLBACK (on_menu_subversion_commit)    /* action callback */
	},
	{
		"ActionSubversionRevert",                       /* Action name */
		GTK_STOCK_UNDO,                            /* Stock icon, if any */
		N_("_Revert…"),                     /* Display label */
		NULL,                                     /* short-cut */
		N_("Revert changes to your working copy."),                      /* Tooltip */
		G_CALLBACK (on_menu_subversion_revert)    /* action callback */
	},
	{
		"ActionSubversionResolve",                       /* Action name */
		GTK_STOCK_PREFERENCES,                            /* Stock icon, if any */
		N_("_Resolve Conflicts…"),                     /* Display label */
		NULL,                                     /* short-cut */
		N_("Resolve conflicts in your working copy."),                      /* Tooltip */
		G_CALLBACK (on_menu_subversion_resolve)    /* action callback */
	},
	{
		"ActionSubversionUpdate",                       /* Action name */
		GTK_STOCK_REFRESH,                            /* Stock icon, if any */
		N_("_Update…"),                     /* Display label */
		NULL,                                     /* short-cut */
		N_("Syncronize your local copy with the Subversion tree"),                      /* Tooltip */
		G_CALLBACK (on_menu_subversion_update)    /* action callback */
	},
	{
		"ActionSubversionCopy",                       /* Action name */
		GTK_STOCK_COPY,                            /* Stock icon, if any */
		N_("Copy Files/Folders…"),                     /* Display label */
		NULL,                                     /* short-cut */
		N_("Copy files/folders in the repository"),                      /* Tooltip */
		G_CALLBACK (on_menu_subversion_copy)    /* action callback */
	},
	{
		"ActionSubversionSwitch",                       /* Action name */
		GTK_STOCK_JUMP_TO,                            /* Stock icon, if any */
		N_("Switch to a Branch/Tag…"),                     /* Display label */
		NULL,                                     /* short-cut */
		N_("Switch your local copy to a branch or tag in the repository"),                      /* Tooltip */
		G_CALLBACK (on_menu_subversion_switch)    /* action callback */
	},
	{
		"ActionSubversionMerge",                       /* Action name */
		GTK_STOCK_CONVERT,                            /* Stock icon, if any */
		N_("Merge…"),                     /* Display label */
		NULL,                                     /* short-cut */
		N_("Merge changes into your working copy"),                      /* Tooltip */
		G_CALLBACK (on_menu_subversion_merge)    /* action callback */
	},
	{
		"ActionSubversionLog",                       /* Action name */
		GTK_STOCK_ZOOM_100,                            /* Stock icon, if any */
		N_("_View Log…"),                     /* Display label */
		NULL,                                     /* short-cut */
		N_("View file history"),                      /* Tooltip */
		G_CALLBACK (on_menu_subversion_log)    /* action callback */
	},
	{
		"ActionSubversionDiff",                       /* Action name */
		GTK_STOCK_ZOOM_100,                            /* Stock icon, if any */
		N_("_Diff…"),                     /* Display label */
		NULL,                                     /* short-cut */
		N_("Diff local tree with repository"),                      /* Tooltip */
		G_CALLBACK (on_menu_subversion_diff)    /* action callback */
	}
};

static GtkActionEntry popup_actions_subversion[] = {
	{
		"ActionPopupSubversion",                       /* Action name */
		NULL,                            /* Stock icon, if any */
		N_("_Subversion"),                     /* Display label */
		NULL,                                     /* short-cut */
		NULL,                      /* Tooltip */
		NULL
	},
	{
		"ActionPopupSubversionUpdate",                       /* Action name */
		GTK_STOCK_REFRESH,                            /* Stock icon, if any */
		N_("_Update…"),                     /* Display label */
		NULL,                                     /* short-cut */
		N_("Syncronize your local copy with the Subversion tree"),                      /* Tooltip */
		G_CALLBACK (on_fm_subversion_update)    /* action callback */
	},
	{
		"ActionPopupSubversionRevert",                       /* Action name */
		GTK_STOCK_UNDO,                            /* Stock icon, if any */
		N_("_Revert…"),                     /* Display label */
		NULL,                                     /* short-cut */
		N_("Revert changes to your working copy."),                      /* Tooltip */
		G_CALLBACK (on_fm_subversion_revert)    /* action callback */
	},
	{
		"ActionPopupSubversionAdd",                       /* Action name */
		GTK_STOCK_ADD,                            /* Stock icon, if any */
		N_("_Add…"),                     /* Display label */
		NULL,                                     /* short-cut */
		N_("Add a new file/directory to the Subversion tree"),                      /* Tooltip */
		G_CALLBACK (on_fm_subversion_add)    /* action callback */
	},
	{
		"ActionPopupSubversionRemove",                       /* Action name */
		GTK_STOCK_REMOVE,                            /* Stock icon, if any */
		N_("_Remove…"),                     /* Display label */
		NULL,                                     /* short-cut */
		N_("Remove a file/directory from Subversion tree"),                      /* Tooltip */
		G_CALLBACK (on_fm_subversion_remove)    /* action callback */
	},
	{
		"ActionPopupSubversionLog",                       /* Action name */
		GTK_STOCK_ZOOM_100,                            /* Stock icon, if any */
		N_("_View Log…"),                     /* Display label */
		NULL,                                     /* short-cut */
		N_("View file history"),                      /* Tooltip */
		G_CALLBACK (on_fm_subversion_log)    /* action callback */
	},
	{
		"ActionPopupSubversionCopy",                       /* Action name */
		GTK_STOCK_COPY,                            /* Stock icon, if any */
		N_("Copy…"),                     /* Display label */
		NULL,                                     /* short-cut */
		N_("Copy files/folders in the repository"),                      /* Tooltip */
		G_CALLBACK (on_fm_subversion_copy)    /* action callback */
	},
	{
		"ActionPopupSubversionDiff",                       /* Action name */
		GTK_STOCK_ZOOM_100,                            /* Stock icon, if any */
		N_("Diff…"),                     /* Display label */
		NULL,                                     /* short-cut */
		N_("Diff local tree with repository"),                      /* Tooltip */
		G_CALLBACK (on_fm_subversion_diff)    /* action callback */
	}
};

static void
value_added_fm_current_file (AnjutaPlugin *plugin, const char *name,
							const GValue *value, gpointer data)
{
	AnjutaUI *ui;
	GtkAction *subversion_menu_action;
	gchar *filename;
	GFile* file;
	GFile* svn_dir;
	GFileType type;
	GFileEnumerator *en;
	GFileInfo *file_info;
	
	file = G_FILE(g_value_get_object (value));
	filename = g_file_get_path (file);
	g_return_if_fail (filename != NULL);

	Subversion *subversion = ANJUTA_PLUGIN_SUBVERSION (plugin);
	ui = anjuta_shell_get_ui (plugin->shell, NULL);
	
	if (subversion->fm_current_filename)
		g_free (subversion->fm_current_filename);
	subversion->fm_current_filename = filename;
	
	/* Show popup menu if Subversion directory exists */
	subversion_menu_action = anjuta_ui_get_action (ui, "ActionGroupPopupSubversion", "ActionPopupSubversion");
	
	/* If a directory is selected we check if it contains a "Subversion" directory,
	if it is a file we check if it's directory contains a "Subversion" directory */
	file_info = g_file_query_info (file, 
			G_FILE_ATTRIBUTE_STANDARD_TYPE,
			G_FILE_QUERY_INFO_NONE,
			NULL, NULL);
	if (file_info == NULL)
	{
		return;
	}

	type = g_file_info_get_attribute_uint32 (file_info, 
			G_FILE_ATTRIBUTE_STANDARD_TYPE);
	g_object_unref (G_OBJECT (file_info));
	if (type == G_FILE_TYPE_DIRECTORY)
	{
		svn_dir = g_file_get_child (file, ".svn");
	}
	else
	{
		GFile *parent;

		parent = g_file_get_parent (file);
		if (parent != NULL)
		{
			svn_dir = g_file_get_child (parent, ".svn");
			g_object_unref (G_OBJECT (parent));
		}
		else
		{
			svn_dir = g_file_new_for_path("/.svn");
		}
	}

	en = g_file_enumerate_children (svn_dir, "", G_FILE_QUERY_INFO_NONE,
			NULL, NULL);
	if (en != NULL)
	{
		g_object_unref (en);
		g_object_set (G_OBJECT (subversion_menu_action), "sensitive", TRUE, NULL);
	}
	else
	{
		g_object_set (G_OBJECT (subversion_menu_action), "sensitive", FALSE, NULL);
	}

	g_object_unref (svn_dir);
}

static void
value_removed_fm_current_file (AnjutaPlugin *plugin,
							  const char *name, gpointer data)
{
	AnjutaUI *ui;
	GtkAction *action;
	
	Subversion *subversion = ANJUTA_PLUGIN_SUBVERSION (plugin);
	
	if (subversion->fm_current_filename)
		g_free (subversion->fm_current_filename);
	subversion->fm_current_filename = NULL;

	ui = anjuta_shell_get_ui (plugin->shell, NULL);
	action = anjuta_ui_get_action (ui, "ActionGroupPopupSubversion", "ActionPopupSubversion");
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);
}

static void
value_added_project_root_uri (AnjutaPlugin *plugin, const gchar *name,
							  const GValue *value, gpointer user_data)
{
	Subversion *bb_plugin;
	const gchar *root_uri;
	GtkAction *commit_action;
	GtkAction *revert_action;
	GtkAction *resolve_action;

	bb_plugin = ANJUTA_PLUGIN_SUBVERSION (plugin);
	commit_action = anjuta_ui_get_action (anjuta_shell_get_ui (plugin->shell,
															   NULL),
										  "ActionGroupSubversion",
										  "ActionSubversionCommit");
	revert_action = anjuta_ui_get_action (anjuta_shell_get_ui (plugin->shell,
															   NULL),
										  "ActionGroupSubversion",
										  "ActionSubversionRevert");
	resolve_action = anjuta_ui_get_action (anjuta_shell_get_ui (plugin->shell,
															   NULL),
										  "ActionGroupSubversion",
										  "ActionSubversionResolve");
	
	DEBUG_PRINT ("%s", "Project added");
	
	if (bb_plugin->project_root_dir)
		g_free (bb_plugin->project_root_dir);
	bb_plugin->project_root_dir = NULL;
	
	root_uri = g_value_get_string (value);
	if (root_uri)
	{
		bb_plugin->project_root_dir = 
			anjuta_util_get_local_path_from_uri (root_uri);
		if (bb_plugin->project_root_dir)
		{
			// update_project_ui (bb_plugin);
			subversion_log_set_whole_project_sensitive (bb_plugin->log_bxml,
														TRUE);
			gtk_action_set_sensitive (commit_action, TRUE);
			gtk_action_set_sensitive (revert_action, TRUE);
			gtk_action_set_sensitive (resolve_action, TRUE);
			
			
		}
	}
}

static void
value_removed_project_root_uri (AnjutaPlugin *plugin, const gchar *name,
								gpointer user_data)
{
	Subversion *bb_plugin;
	GtkAction *commit_action;
	GtkAction *revert_action;
	GtkAction *resolve_action;

	bb_plugin = ANJUTA_PLUGIN_SUBVERSION (plugin);
	commit_action = anjuta_ui_get_action (anjuta_shell_get_ui (plugin->shell,
															   NULL),
										  "ActionGroupSubversion",
										  "ActionSubversionCommit");
	revert_action = anjuta_ui_get_action (anjuta_shell_get_ui (plugin->shell,
															   NULL),
										  "ActionGroupSubversion",
										  "ActionSubversionRevert");
	resolve_action = anjuta_ui_get_action (anjuta_shell_get_ui (plugin->shell,
															   NULL),
										  "ActionGroupSubversion",
										  "ActionSubversionResolve");
	
	if (bb_plugin->project_root_dir)
		g_free (bb_plugin->project_root_dir);
	bb_plugin->project_root_dir = NULL;
	// update_project_ui (bb_plugin);
	
	subversion_log_set_whole_project_sensitive (bb_plugin->log_bxml,
												FALSE);
	gtk_action_set_sensitive (commit_action, FALSE);
	gtk_action_set_sensitive (revert_action, FALSE);
	gtk_action_set_sensitive (resolve_action, FALSE);
}

static void
value_added_current_editor (AnjutaPlugin *plugin, const char *name,
							const GValue *value, gpointer data)
{
	AnjutaUI *ui;
	GFile* file;
	GObject *editor;
	
	editor = g_value_get_object (value);
	
	if (!IANJUTA_IS_EDITOR(editor))
		return;
	
	Subversion *subversion = ANJUTA_PLUGIN_SUBVERSION (plugin);
	ui = anjuta_shell_get_ui (plugin->shell, NULL);
	
	if (subversion->current_editor_filename)
		g_free (subversion->current_editor_filename);
	subversion->current_editor_filename = NULL;
	
	file = ianjuta_file_get_file (IANJUTA_FILE (editor), NULL);
	if (file)
	{
		gchar *filename;
		
		filename = g_file_get_path (file);
		if (!filename)
			return;
		subversion->current_editor_filename = filename;
	}
}

static void
value_removed_current_editor (AnjutaPlugin *plugin,
							  const char *name, gpointer data)
{
	Subversion *subversion = ANJUTA_PLUGIN_SUBVERSION (plugin);
	
	if (subversion->current_editor_filename)
		g_free (subversion->current_editor_filename);
	subversion->current_editor_filename = NULL;
	
	// update_module_ui (subversion);
}

static void
subversion_load_session (AnjutaPlugin *plugin, AnjutaSessionPhase phase,
                         AnjutaSession *session)
{
	Subversion *subversion = ANJUTA_PLUGIN_SUBVERSION (plugin);

	if (phase != ANJUTA_SESSION_PHASE_NORMAL)
		return;

	DEBUG_PRINT ("Loading session");

	g_clear_object (&subversion->session_settings);
	subversion->session_settings = anjuta_session_create_settings (session, "subversion");

	g_list_free_full(subversion->svn_commit_logs, g_free);
	subversion->svn_commit_logs = anjuta_util_settings_get_string_list (subversion->session_settings,
	                                                                "logs");
}

static void
subversion_save_session (AnjutaPlugin *plugin, AnjutaSessionPhase phase,
                         AnjutaSession *session)
{
	Subversion *subversion = ANJUTA_PLUGIN_SUBVERSION (plugin);

	if (phase != ANJUTA_SESSION_PHASE_NORMAL)
		return;

	DEBUG_PRINT ("Saving session");
	anjuta_util_settings_set_string_list (subversion->session_settings,
	                                      "logs", subversion->svn_commit_logs);
}
				 
static gboolean
activate_plugin (AnjutaPlugin *plugin)
{
	AnjutaUI *ui;
	Subversion *subversion;
	GtkAction *commit_action;
	GtkAction *revert_action;
	GtkAction *resolve_action;
	GError* error = NULL;
	
	DEBUG_PRINT ("%s", "Subversion: Activating Subversion plugin …");
	subversion = ANJUTA_PLUGIN_SUBVERSION (plugin);
	
	ui = anjuta_shell_get_ui (plugin->shell, NULL);
	
	/* Add all our actions */
	subversion->action_group = 
		anjuta_ui_add_action_group_entries (ui, "ActionGroupSubversion",
											_("Subversion operations"),
											actions_subversion,
											G_N_ELEMENTS (actions_subversion),
											GETTEXT_PACKAGE, TRUE, plugin);
	subversion->popup_action_group = 
		anjuta_ui_add_action_group_entries (ui, "ActionGroupPopupSubversion",
											_("Subversion popup operations"),
											popup_actions_subversion,
											G_N_ELEMENTS (popup_actions_subversion),
											GETTEXT_PACKAGE, FALSE, plugin);
	
	/* Merge UI */
	subversion->uiid = anjuta_ui_merge (ui, UI_FILE);
	
	subversion->log_bxml = gtk_builder_new ();
	if (!gtk_builder_add_from_file (subversion->log_bxml, GLADE_FILE, &error))
	{
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
	}

	/* Add watches */
	subversion->fm_watch_id = 
		anjuta_plugin_add_watch (plugin, IANJUTA_FILE_MANAGER_SELECTED_FILE,
								 value_added_fm_current_file,
								 value_removed_fm_current_file, NULL);
	subversion->project_watch_id = 
		anjuta_plugin_add_watch (plugin, IANJUTA_PROJECT_MANAGER_PROJECT_ROOT_URI,
								 value_added_project_root_uri,
								 value_removed_project_root_uri, NULL);
	subversion->editor_watch_id = 
		anjuta_plugin_add_watch (plugin, IANJUTA_DOCUMENT_MANAGER_CURRENT_DOCUMENT,
								 value_added_current_editor,
								 value_removed_current_editor, NULL);
	
	subversion->log_viewer = subversion_log_window_create (subversion);
	anjuta_shell_add_widget (plugin->shell, subversion->log_viewer,
							 "AnjutaSubversionLogViewer", 
							 _("Subversion Log"),
							 GTK_STOCK_ZOOM_100,
							 ANJUTA_SHELL_PLACEMENT_CENTER,
							 NULL);
	
	g_object_unref (subversion->log_viewer);
	
	commit_action = anjuta_ui_get_action (anjuta_shell_get_ui (plugin->shell,
															   NULL),
										  "ActionGroupSubversion",
										  "ActionSubversionCommit");
	revert_action = anjuta_ui_get_action (anjuta_shell_get_ui (plugin->shell,
															   NULL),
										  "ActionGroupSubversion",
										  "ActionSubversionRevert");
	resolve_action = anjuta_ui_get_action (anjuta_shell_get_ui (plugin->shell,
															   NULL),
										  "ActionGroupSubversion",
										  "ActionSubversionResolve");

	if (!subversion->project_root_dir)
	{
		gtk_action_set_sensitive (commit_action, FALSE);
		gtk_action_set_sensitive (revert_action, FALSE);
		gtk_action_set_sensitive (resolve_action, FALSE);
	}

	return TRUE;
}

static gboolean
deactivate_plugin (AnjutaPlugin *plugin)
{
	Subversion *subversion = ANJUTA_PLUGIN_SUBVERSION (plugin);
	AnjutaUI *ui = anjuta_shell_get_ui (plugin->shell, NULL);
	DEBUG_PRINT ("%s", "Subversion: Dectivating Subversion plugin …");
	anjuta_plugin_remove_watch (plugin, subversion->fm_watch_id, TRUE);
	anjuta_plugin_remove_watch (plugin, subversion->project_watch_id, TRUE);
	anjuta_plugin_remove_watch (plugin, subversion->editor_watch_id, TRUE);
	anjuta_ui_unmerge (ui, ANJUTA_PLUGIN_SUBVERSION (plugin)->uiid);
	anjuta_ui_remove_action_group (ui, ANJUTA_PLUGIN_SUBVERSION (plugin)->action_group);
	anjuta_ui_remove_action_group (ui, ANJUTA_PLUGIN_SUBVERSION (plugin)->popup_action_group);
	
	anjuta_shell_remove_widget (plugin->shell, 
								ANJUTA_PLUGIN_SUBVERSION (plugin)->log_viewer,
								NULL);
	
	g_object_unref (ANJUTA_PLUGIN_SUBVERSION (plugin)->log_bxml);
	g_list_free_full(ANJUTA_PLUGIN_SUBVERSION (plugin)->svn_commit_logs, g_free);

	return TRUE;
}

static void
finalize (GObject *obj)
{
	apr_terminate ();
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
dispose (GObject *obj)
{
	Subversion *plugin = ANJUTA_PLUGIN_SUBVERSION (obj);

	g_clear_object (&plugin->session_settings);

	G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
subversion_instance_init (GObject *obj)
{
	Subversion *plugin = ANJUTA_PLUGIN_SUBVERSION (obj);
	plugin->uiid = 0;
	plugin->mesg_view = NULL;
	plugin->launcher = NULL;
	plugin->fm_current_filename = NULL;
	plugin->project_root_dir = NULL;
	plugin->current_editor_filename = NULL;
	plugin->log_bxml = NULL;
	plugin->log_viewer = NULL;
	
	apr_initialize ();
}

static void
subversion_class_init (GObjectClass *klass) 
{
	AnjutaPluginClass *plugin_class = ANJUTA_PLUGIN_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	plugin_class->activate = activate_plugin;
	plugin_class->deactivate = deactivate_plugin;
	plugin_class->load_session = subversion_load_session;
	plugin_class->save_session = subversion_save_session;

	klass->dispose = dispose;
	klass->finalize = finalize;
}

void subversion_plugin_status_changed_emit(AnjutaCommand *command, guint return_code, Subversion *plugin)
{
        g_signal_emit_by_name(plugin, "status-changed");
}

ANJUTA_PLUGIN_BEGIN (Subversion, subversion);
ANJUTA_PLUGIN_ADD_INTERFACE (subversion_ivcs, IANJUTA_TYPE_VCS);
ANJUTA_PLUGIN_END;

ANJUTA_SIMPLE_PLUGIN (Subversion, subversion);
