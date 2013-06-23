/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    plugin.c
    Copyright (C) 2004 Naba Kumar

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

#include <config.h>

/*#define DEBUG*/

#include "plugin.h"

#include "breakpoints.h"
#include "stack_trace.h"
#include "info.h"
#include "memory.h"
#include "disassemble.h"
#include "signals.h"
#include "sharedlib.h"
#include "registers.h"
#include "utilities.h"
#include "start.h"
#include "queue.h"
#include "variable.h"

#include <glib/gi18n.h>
#include <libanjuta/anjuta-shell.h>
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/anjuta-status.h>
#include <libanjuta/interfaces/ianjuta-file.h>
#include <libanjuta/interfaces/ianjuta-editor.h>
#include <libanjuta/interfaces/ianjuta-indicable.h>
#include <libanjuta/interfaces/ianjuta-markable.h>
#include <libanjuta/interfaces/ianjuta-debug-manager.h>
#include <libanjuta/interfaces/ianjuta-project-manager.h>
#include <libanjuta/interfaces/ianjuta-document-manager.h>
#include <libanjuta/interfaces/ianjuta-message-manager.h>

#include <gio/gio.h>

/* Contants defintion
 *---------------------------------------------------------------------------*/

#define ICON_FILE "anjuta-debug-manager-plugin-48.png"
#define UI_FILE PACKAGE_DATA_DIR "/ui/anjuta-debug-manager.xml"

/* Plugin type
 *---------------------------------------------------------------------------*/

struct _DebugManagerPlugin
{
	AnjutaPlugin parent;

	/* Debugger queue */
	DmaDebuggerQueue *queue;

	/* Menu item */
	gint uiid;
	GtkActionGroup *start_group;
	GtkActionGroup *loaded_group;
	GtkActionGroup *stopped_group;
	GtkActionGroup *running_group;
	GtkAction *run_stop_action;

	/* Project */
	gchar *project_root_uri;
	guint project_watch_id;

	/* Editor */
	IAnjutaEditor *current_editor;
	guint editor_watch_id;
	IAnjutaEditor *pc_editor;
	guint pc_line;
	gulong pc_address;
	gboolean busy;

	/* Debugger components */
	BreakpointsDBase *breakpoints;
	DmaStart *start;
	StackTrace *stack;
	CpuRegisters *registers;
	Sharedlibs *sharedlibs;
	Signals *signals;
	DmaMemory *memory;
	DmaDisassemble *disassemble;
	DmaVariableDBase *variable;


	GtkWidget *user_command_dialog;

	/* Logging view */
	IAnjutaMessageView* view;
};

struct _DebugManagerPluginClass
{
	AnjutaPluginClass parent_class;
};

/* Private functions
 *---------------------------------------------------------------------------*/

static void
register_stock_icons (AnjutaPlugin *plugin)
{
        static gboolean registered = FALSE;

        if (registered)
                return;
        registered = TRUE;

        /* Register stock icons */
		BEGIN_REGISTER_ICON (plugin)
		REGISTER_ICON (ICON_FILE, "debugger-icon");
        REGISTER_ICON ("stack.png", "gdb-stack-icon");
        REGISTER_ICON ("locals.png", "gdb-locals-icon");
        REGISTER_ICON_FULL ("anjuta-watch", "gdb-watch-icon");
        REGISTER_ICON_FULL ("anjuta-breakpoint-toggle", ANJUTA_STOCK_BREAKPOINT_TOGGLE);
        REGISTER_ICON_FULL ("anjuta-breakpoint-clear", ANJUTA_STOCK_BREAKPOINT_CLEAR);
    	/* We have no -24 version for the next two */
		REGISTER_ICON ("anjuta-breakpoint-disabled-16.png", ANJUTA_STOCK_BREAKPOINT_DISABLED);
        REGISTER_ICON ("anjuta-breakpoint-enabled-16.png", ANJUTA_STOCK_BREAKPOINT_ENABLED);
		REGISTER_ICON_FULL ("anjuta-attach", "debugger-attach");
		REGISTER_ICON_FULL ("anjuta-step-into", "debugger-step-into");
		REGISTER_ICON_FULL ("anjuta-step-out", "debugger-step-out");
		REGISTER_ICON_FULL ("anjuta-step-over", "debugger-step-over");
		REGISTER_ICON_FULL ("anjuta-run-to-cursor", "debugger-run-to-cursor");
		REGISTER_ICON_FULL ("anjuta-memory", "debugger-memory");
		REGISTER_ICON_FULL ("anjuta-disassembly", "debugger-disassembly");
		END_REGISTER_ICON
}

/* Program counter functions
 *---------------------------------------------------------------------------*/

static void
show_program_counter_in_editor(DebugManagerPlugin *self)
{
	IAnjutaEditor *editor = self->current_editor;

	if ((editor != NULL) && (self->pc_editor == editor))
	{
		if (IANJUTA_IS_MARKABLE (editor))
		{
			ianjuta_markable_mark(IANJUTA_MARKABLE (editor), self->pc_line, IANJUTA_MARKABLE_PROGRAM_COUNTER, NULL, NULL);
		}
		if (IANJUTA_IS_INDICABLE(editor))
		{
			IAnjutaIterable *begin =
				ianjuta_editor_get_line_begin_position(editor, self->pc_line, NULL);
			IAnjutaIterable *end =
				ianjuta_editor_get_line_end_position(editor, self->pc_line, NULL);

			ianjuta_indicable_set(IANJUTA_INDICABLE(editor), begin, end,
								  IANJUTA_INDICABLE_IMPORTANT, NULL);
			g_object_unref (begin);
			g_object_unref (end);
		}
	}
}

static void
hide_program_counter_in_editor(DebugManagerPlugin *self)
{
	IAnjutaEditor *editor = self->current_editor;

	if ((editor != NULL) && (self->pc_editor == editor))
	{
		if (IANJUTA_IS_MARKABLE (editor))
		{
			ianjuta_markable_delete_all_markers (IANJUTA_MARKABLE (editor), IANJUTA_MARKABLE_PROGRAM_COUNTER, NULL);
		}
		if (IANJUTA_IS_INDICABLE(editor))
		{
			ianjuta_indicable_clear(IANJUTA_INDICABLE(editor), NULL);
		}
	}
}

static void
set_program_counter(DebugManagerPlugin *self, const gchar* filename, guint line, gulong address)
{
	IAnjutaDocumentManager *docman = NULL;
	GFile* file;

	/* Remove previous marker */
	hide_program_counter_in_editor (self);
	if (self->pc_editor != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (self->pc_editor), (gpointer *)(gpointer)&self->pc_editor);
		self->pc_editor = NULL;
	}
	self->pc_address = address;

	if (filename != NULL)
	{
		file = g_file_new_for_path (filename);
		docman = anjuta_shell_get_interface (ANJUTA_PLUGIN (self)->shell, IAnjutaDocumentManager, NULL);
		if (docman)
		{
			IAnjutaEditor* editor;

			editor = ianjuta_document_manager_goto_file_line(docman, file, line, NULL);

			if (editor != NULL)
			{
				self->pc_editor = editor;
				g_object_add_weak_pointer (G_OBJECT (editor), (gpointer)(gpointer)&self->pc_editor);
				self->pc_line = line;
				show_program_counter_in_editor (self);
			}
		}
		g_object_unref (file);
	}
}

static void
value_added_project_root_uri (AnjutaPlugin *plugin, const gchar *name,
							  const GValue *value, gpointer user_data)
{
	DebugManagerPlugin *dm_plugin;
	const gchar *root_uri;

	dm_plugin = ANJUTA_PLUGIN_DEBUG_MANAGER (plugin);

	if (dm_plugin->project_root_uri)
		g_free (dm_plugin->project_root_uri);
	dm_plugin->project_root_uri = NULL;

	root_uri = g_value_get_string (value);
	if (root_uri)
	{
		dm_plugin->project_root_uri = g_strdup (root_uri);
	}
}

static void
value_removed_project_root_uri (AnjutaPlugin *plugin, const gchar *name,
								gpointer user_data)
{
	DebugManagerPlugin *dm_plugin;

	dm_plugin = ANJUTA_PLUGIN_DEBUG_MANAGER (plugin);

	if (dm_plugin->project_root_uri)
		g_free (dm_plugin->project_root_uri);
	dm_plugin->project_root_uri = NULL;
}

static void
value_added_current_editor (AnjutaPlugin *plugin, const char *name,
							const GValue *value, gpointer data)
{
	GObject *editor;
	DebugManagerPlugin *self;

	self = ANJUTA_PLUGIN_DEBUG_MANAGER (plugin);

	editor = g_value_get_object (value);
	DEBUG_PRINT("add value current editor %p",  editor);

	if (!IANJUTA_IS_EDITOR(editor))
	{
		self->current_editor = NULL;
		return;
	}

	self->current_editor = IANJUTA_EDITOR (editor);
	g_object_add_weak_pointer (G_OBJECT (self->current_editor), (gpointer *)(gpointer)&self->current_editor);

	/* Restore program counter marker */
	show_program_counter_in_editor (self);

	/* connect signal to enable/disable breakpoints on double clicking the line marks gutter */
	/* firstly, find the handler of previously connected signal */
	/* secondly, connect signal if a handler wasn't found for the signal */
	guint signal_id = g_signal_lookup( "line-marks-gutter-clicked", IANJUTA_TYPE_EDITOR);
	glong handler_id = g_signal_handler_find( (gpointer)self->current_editor,
            G_SIGNAL_MATCH_ID,
            signal_id,
            0, NULL, NULL, NULL );


	DEBUG_PRINT("current editor %p, breapoints db %p", self->current_editor, self->breakpoints);

	if(!handler_id) {
		g_signal_connect (
				self->current_editor,
				"line-marks-gutter-clicked",
				G_CALLBACK (breakpoint_toggle_handler),
				self->breakpoints
			);
	}

}

static void
value_removed_current_editor (AnjutaPlugin *plugin,
							  const char *name, gpointer data)
{
	DebugManagerPlugin *self = ANJUTA_PLUGIN_DEBUG_MANAGER (plugin);

	DEBUG_PRINT("remove value current editor %p", self->current_editor);
	if (self->current_editor)
	{
		hide_program_counter_in_editor (self);

		g_object_remove_weak_pointer (G_OBJECT (self->current_editor), (gpointer *)(gpointer)&self->current_editor);
	}
	self->current_editor = NULL;
}

static void
enable_log_view (DebugManagerPlugin *this, gboolean enable)
{
	if (enable)
	{
		if (this->view == NULL)
		{
			IAnjutaMessageManager* man;

			man = anjuta_shell_get_interface (ANJUTA_PLUGIN (this)->shell, IAnjutaMessageManager, NULL);
			this->view = ianjuta_message_manager_add_view (man, _("Debugger Log"), ICON_FILE, NULL);
			if (this->view != NULL)
			{
				/*g_signal_connect (G_OBJECT (this->view), "buffer_flushed", G_CALLBACK (on_message_buffer_flushed), this);*/
				g_object_add_weak_pointer (G_OBJECT (this->view), (gpointer *)(gpointer)&this->view);
				dma_queue_enable_log (this->queue, this->view);
			}
		}
		else
		{
			ianjuta_message_view_clear (this->view, NULL);
		}
	}
	else
	{
		if (this->view != NULL)
		{
			IAnjutaMessageManager* man;

			man = anjuta_shell_get_interface (ANJUTA_PLUGIN (this)->shell, IAnjutaMessageManager, NULL);
			ianjuta_message_manager_remove_view (man, this->view, NULL);
			this->view = NULL;
		}
		if (this->queue != NULL)
		{
			dma_queue_disable_log (this->queue);
		}
	}
}

static void
dma_plugin_save_session (AnjutaPlugin *plugin, AnjutaSessionPhase phase,
                         AnjutaSession *session)
{
	DebugManagerPlugin *this = ANJUTA_PLUGIN_DEBUG_MANAGER (plugin);

	if (phase == ANJUTA_SESSION_PHASE_START)
		enable_log_view (this, FALSE);
	if (phase != ANJUTA_SESSION_PHASE_NORMAL)
		return;

	/* Close debugger when session changed */
	if (this->queue)
	{
		dma_queue_abort (this->queue);
	}

	dma_variable_dbase_save_session (this->variable, session);
	breakpoints_dbase_save_session (this->breakpoints, session);
	dma_start_save_session (this->start, session);
}

static void
dma_plugin_load_session (AnjutaPlugin *plugin, AnjutaSessionPhase phase,
                         AnjutaSession *session)
{
	DebugManagerPlugin *this = ANJUTA_PLUGIN_DEBUG_MANAGER (plugin);

	if (phase != ANJUTA_SESSION_PHASE_NORMAL)
		return;

	dma_variable_dbase_load_session (this->variable, session);
	breakpoints_dbase_load_session (this->breakpoints, session);
	dma_start_load_session (this->start, session);
}

/* State functions
 *---------------------------------------------------------------------------*/

/* Called when the debugger is started but no program is loaded */

static void
dma_plugin_debugger_started (DebugManagerPlugin *this)
{
	GtkAction *action;
	AnjutaStatus* status;

	DEBUG_PRINT ("%s", "DMA: dma_plugin_debugger_started");

	/* Update ui */
	action = gtk_action_group_get_action (this->start_group, "ActionDebuggerStop");
	gtk_action_set_sensitive (action, TRUE);
	gtk_action_group_set_visible (this->loaded_group, TRUE);
	gtk_action_group_set_sensitive (this->loaded_group, FALSE);
	gtk_action_group_set_visible (this->stopped_group, TRUE);
	gtk_action_group_set_sensitive (this->stopped_group, FALSE);
	gtk_action_group_set_visible (this->running_group, TRUE);
	gtk_action_group_set_sensitive (this->running_group, FALSE);

	status = anjuta_shell_get_status(ANJUTA_PLUGIN (this)->shell, NULL);
	anjuta_status_set_default (status, _("Debugger"), _("Started"));
}

/* Called when a program is loaded */

static void
dma_plugin_program_loaded (DebugManagerPlugin *this)
{
	AnjutaStatus* status;

	DEBUG_PRINT ("%s", "DMA: dma_plugin_program_loaded");

	/* Update ui */
	gtk_action_group_set_sensitive (this->loaded_group, TRUE);
	gtk_action_group_set_sensitive (this->stopped_group, FALSE);
	gtk_action_group_set_sensitive (this->running_group, FALSE);

	gtk_action_set_sensitive (this->run_stop_action, FALSE);

	status = anjuta_shell_get_status(ANJUTA_PLUGIN (this)->shell, NULL);
	anjuta_status_set_default (status, _("Debugger"), _("Loaded"));
}

/* Called when the program is running */

static void
dma_plugin_program_running (DebugManagerPlugin *this)
{
	AnjutaStatus* status;

	DEBUG_PRINT ("%s", "DMA: dma_plugin_program_running");

	/* Update ui */
	gtk_action_group_set_sensitive (this->loaded_group, TRUE);
	gtk_action_group_set_sensitive (this->stopped_group, FALSE);
	gtk_action_group_set_sensitive (this->running_group, TRUE);

	gtk_action_set_sensitive (this->run_stop_action, TRUE);
	gtk_action_set_stock_id (this->run_stop_action, GTK_STOCK_MEDIA_PAUSE);
	gtk_action_set_label (this->run_stop_action, _("Pa_use Program"));
	gtk_action_set_tooltip (this->run_stop_action, _("Pauses the execution of the program"));

	status = anjuta_shell_get_status(ANJUTA_PLUGIN (this)->shell, NULL);
	anjuta_status_set_default (status, _("Debugger"), _("Running…"));

	set_program_counter(this, NULL, 0, 0);
}

/* Called when the program is stopped */

static void
dma_plugin_program_stopped (DebugManagerPlugin *this)
{
	AnjutaStatus* status;

	DEBUG_PRINT ("%s", "DMA: dma_plugin_program_broken");

	/* Update ui */
	gtk_action_group_set_sensitive (this->loaded_group, TRUE);
	gtk_action_group_set_sensitive (this->stopped_group, TRUE);
	gtk_action_group_set_sensitive (this->running_group, FALSE);

	gtk_action_set_sensitive (this->run_stop_action, TRUE);
	gtk_action_set_stock_id (this->run_stop_action, GTK_STOCK_MEDIA_PLAY);
	gtk_action_set_label (this->run_stop_action, _("Run/_Continue"));
	gtk_action_set_tooltip (this->run_stop_action, _("Continue the execution of the program"));

	status = anjuta_shell_get_status(ANJUTA_PLUGIN (this)->shell, NULL);
	anjuta_status_set_default (status, _("Debugger"), _("Stopped"));
}

/* Called when the program postion change */

static void
dma_plugin_program_moved (DebugManagerPlugin *this, guint pid, guint tid, gulong address, const gchar* file, guint line)
{
	DEBUG_PRINT ("DMA: dma_plugin_program_moved %s %d %lx", file, line, address);

	set_program_counter (this, file, line, address);
}

/* Called when a program is unloaded */
static void
dma_plugin_program_unload (DebugManagerPlugin *this)
{
	AnjutaStatus* status;

	DEBUG_PRINT ("%s", "DMA: dma_plugin_program_unload");

	/* Update ui */
	gtk_action_group_set_visible (this->start_group, TRUE);
	gtk_action_group_set_sensitive (this->start_group, TRUE);
	gtk_action_group_set_visible (this->loaded_group, TRUE);
	gtk_action_group_set_sensitive (this->loaded_group, FALSE);
	gtk_action_group_set_visible (this->stopped_group, TRUE);
	gtk_action_group_set_sensitive (this->stopped_group, FALSE);
	gtk_action_group_set_visible (this->running_group, TRUE);
	gtk_action_group_set_sensitive (this->running_group, FALSE);

	status = anjuta_shell_get_status(ANJUTA_PLUGIN (this)->shell, NULL);
	anjuta_status_set_default (status, _("Debugger"), _("Unloaded"));
}

/* Called when the debugger is stopped */

static void
dma_plugin_debugger_stopped (DebugManagerPlugin *self, GError *err)
{
	GtkAction *action;
	AnjutaStatus* state;

	DEBUG_PRINT ("%s", "DMA: dma_plugin_debugger_stopped");

	dma_plugin_program_unload (self);

	/* Update ui */
	gtk_action_group_set_visible (self->start_group, TRUE);
	gtk_action_group_set_sensitive (self->start_group, TRUE);
	action = gtk_action_group_get_action (self->start_group, "ActionDebuggerStop");
	gtk_action_set_sensitive (action, FALSE);
	gtk_action_group_set_visible (self->loaded_group, TRUE);
	gtk_action_group_set_sensitive (self->loaded_group, FALSE);
	gtk_action_group_set_visible (self->stopped_group, TRUE);
	gtk_action_group_set_sensitive (self->stopped_group, FALSE);
	gtk_action_group_set_visible (self->running_group, TRUE);
	gtk_action_group_set_sensitive (self->running_group, FALSE);

	/* clear indicator */
	set_program_counter (self, NULL, 0, 0);

	state = anjuta_shell_get_status(ANJUTA_PLUGIN (self)->shell, NULL);
	anjuta_status_set_default (state, _("Debugger"), NULL);

	/* Remove user command dialog if existing */
	if (self->user_command_dialog) gtk_widget_destroy (GTK_WIDGET (self->user_command_dialog));

	/* Display a warning if debugger stop unexpectedly */
	if (err != NULL)
	{
		GtkWindow *parent = GTK_WINDOW (ANJUTA_PLUGIN(self)->shell);
		anjuta_util_dialog_error (parent, _("Debugger terminated with error %d: %s\n"), err->code, err->message);
	}

}

static void
dma_plugin_signal_received (DebugManagerPlugin *self, const gchar *name, const gchar *description)
{
	GtkWindow *parent = GTK_WINDOW (ANJUTA_PLUGIN (self)->shell);

	/* Skip SIGINT signal */
	if (strcmp(name, "SIGINT") != 0)
	{
		anjuta_util_dialog_warning (parent, _("Program has received signal: %s\n"), description);
	}
}

/* Called when the user want to go to another location */

static void
dma_plugin_location_changed (DebugManagerPlugin *self, gulong address, const gchar *uri, guint line)
{
	/* Go to location in editor */
	if (uri != NULL)
	{
		IAnjutaDocumentManager *docman;
        docman = anjuta_shell_get_interface (ANJUTA_PLUGIN(self)->shell, IAnjutaDocumentManager, NULL);
        if (docman)
        {
			GFile *file = g_file_new_for_uri (uri);
			ianjuta_document_manager_goto_file_line (docman, file, line, NULL);
			g_object_unref (file);
        }
	}
}

/* Start/Stop menu functions
 *---------------------------------------------------------------------------*/

static void
on_start_debug_activate (GtkAction* action, DebugManagerPlugin* this)
{
	enable_log_view (this, TRUE);
	dma_run_target (this->start, NULL);
}

static void
on_attach_to_project_action_activate (GtkAction* action, DebugManagerPlugin* this)
{
	enable_log_view (this, TRUE);
	dma_attach_to_process (this->start);
}

static void
on_start_remote_debug_action_activate (GtkAction* action, DebugManagerPlugin* this)
{
	/* Returns true if user clicked "Connect" */
	enable_log_view (this, TRUE);
	dma_run_remote_target (this->start, NULL, NULL);
}

static void
on_debugger_stop_activate (GtkAction* action, DebugManagerPlugin* plugin)
{
	if (plugin->start)
	{
		dma_quit_debugger (plugin->start);
	}
}

static void
on_add_source_activate (GtkAction* action, DebugManagerPlugin* this)
{
	dma_add_source_path (this->start);
}

/* Execute call back
 *---------------------------------------------------------------------------*/

static void
on_run_continue_action_activate (GtkAction* action, DebugManagerPlugin* plugin)
{
	if (plugin->queue)
		dma_queue_run (plugin->queue);
}

static void
on_continue_suspend_action_activate (GtkAction* action, DebugManagerPlugin* plugin)
{
	if (plugin->queue)
	{
		if (gtk_action_group_get_sensitive (plugin->running_group))
		{
			dma_queue_interrupt (plugin->queue);
		}
		else
		{
			dma_queue_run (plugin->queue);
		}
	}
}

static void
on_step_in_action_activate (GtkAction* action, DebugManagerPlugin* plugin)
{
	if (plugin->queue)
	{
		if ((plugin->disassemble != NULL) && (dma_disassemble_is_focus (plugin->disassemble)))
		{
			dma_queue_stepi_in (plugin->queue);
		}
		else
		{
			dma_queue_step_in (plugin->queue);
		}
	}
}

static void
on_step_over_action_activate (GtkAction* action, DebugManagerPlugin* plugin)
{
	if (plugin->queue)
	{
		if ((plugin->disassemble != NULL) && (dma_disassemble_is_focus (plugin->disassemble)))
		{
			dma_queue_stepi_over (plugin->queue);
		}
		else
		{
			dma_queue_step_over (plugin->queue);
		}
	}
}

static void
on_step_out_action_activate (GtkAction* action, DebugManagerPlugin* plugin)
{
	if (plugin->queue)
		dma_queue_step_out (plugin->queue);
}

static void
on_run_to_cursor_action_activate (GtkAction* action, DebugManagerPlugin* plugin)
{
	if (plugin->queue)
	{
		if ((plugin->disassemble != NULL) && (dma_disassemble_is_focus (plugin->disassemble)))
		{
			gulong address;

			address = dma_disassemble_get_current_address (plugin->disassemble);
			dma_queue_run_to_address (plugin->queue, address);
		}
		else
		{
			IAnjutaEditor *editor;
			GFile* file;
			gchar *filename;
			gint line;

			editor = dma_get_current_editor (ANJUTA_PLUGIN (plugin));
			if (editor == NULL)
				return;
			file = ianjuta_file_get_file (IANJUTA_FILE (editor), NULL);
			if (file == NULL)
				return;

			filename = g_file_get_path (file);

			line = ianjuta_editor_get_lineno (editor, NULL);
			dma_queue_run_to (plugin->queue, filename, line);
			g_free (filename);
			g_object_unref (file);
		}
	}
}

static void
on_run_from_cursor_action_activate (GtkAction* action, DebugManagerPlugin* plugin)
{
	if (plugin->queue)
	{
		if ((plugin->disassemble != NULL) && (dma_disassemble_is_focus (plugin->disassemble)))
		{
			gulong address;

			address = dma_disassemble_get_current_address (plugin->disassemble);
			dma_queue_run_from_address (plugin->queue, address);
		}
		else
		{
			IAnjutaEditor *editor;
			GFile* file;
			gchar *filename;
			gint line;

			editor = dma_get_current_editor (ANJUTA_PLUGIN (plugin));
			if (editor == NULL)
				return;
			file = ianjuta_file_get_file (IANJUTA_FILE (editor), NULL);
			if (file == NULL)
				return;

			filename = g_file_get_path (file);

			line = ianjuta_editor_get_lineno (editor, NULL);
			dma_queue_run_from (plugin->queue, filename, line);
			g_free (filename);
			g_object_unref (file);
		}
	}
}

static void
on_debugger_interrupt_activate (GtkAction* action, DebugManagerPlugin* plugin)
{
	if (plugin->queue)
		dma_queue_interrupt (plugin->queue);
}

/* Custom command
 *---------------------------------------------------------------------------*/

static void
on_debugger_command_entry_activate (GtkEntry *entry, DebugManagerPlugin *plugin)
{
        const gchar *command;

        command = gtk_entry_get_text (GTK_ENTRY (entry));
        if (command && strlen (command))
                dma_queue_send_command (plugin->queue, command);
        gtk_entry_set_text (entry, "");
}

static void
on_debugger_custom_command_activate (GtkAction * action, DebugManagerPlugin *plugin)
{
	if (plugin->user_command_dialog == NULL)
	{
        GtkBuilder *bxml;
        GtkWidget *entry;

		bxml = anjuta_util_builder_new (GLADE_FILE, NULL);
		if (!bxml) return;
		anjuta_util_builder_get_objects (bxml,
		    "debugger_command_dialog", &plugin->user_command_dialog,
		    "debugger_command_entry", &entry,
		    NULL);
        g_object_unref (bxml);

	    gtk_window_set_transient_for (GTK_WINDOW (plugin->user_command_dialog), GTK_WINDOW (ANJUTA_PLUGIN (plugin)->shell));

		g_object_add_weak_pointer (G_OBJECT (plugin->user_command_dialog), (gpointer *)&plugin->user_command_dialog);

        g_signal_connect_swapped (plugin->user_command_dialog, "response",
                                                          G_CALLBACK (gtk_widget_destroy),
                                                         plugin->user_command_dialog);
        g_signal_connect (entry, "activate",
                                          G_CALLBACK (on_debugger_command_entry_activate),
                                          plugin);

		gtk_widget_show_all (GTK_WIDGET (plugin->user_command_dialog));
	}
	else
	{
		gtk_window_present (GTK_WINDOW (plugin->user_command_dialog));
	}
}

/* Other informations
 *---------------------------------------------------------------------------*/

/*static void
on_info_memory_activate (GtkAction * action, DebugManagerPlugin *plugin)
{
	GtkWidget *win_memory;

	win_memory = memory_info_new (plugin->debugger,
								  GTK_WINDOW (ANJUTA_PLUGIN (plugin)->shell),
								  NULL);
	gtk_widget_show(win_memory);
}*/

static void
on_debugger_sharedlibs_activate (GtkAction * action, DebugManagerPlugin *plugin)
{
	sharedlibs_show (plugin->sharedlibs);
}

static void
on_debugger_signals_activate (GtkAction * action, DebugManagerPlugin *plugin)
{
	signals_show (plugin->signals);
}

/* Actions table
 *---------------------------------------------------------------------------*/

static GtkActionEntry actions_start[] =
{
	{
		"ActionMenuDebug",                        /* Action name */
		NULL,                                     /* Stock icon, if any */
		N_("_Debug"),                             /* Display label */
		NULL,                                     /* short-cut */
		NULL,                                     /* Tooltip */
		NULL                                      /* action callback */
	},
	{
		"ActionMenuStart",
		"debugger-icon",
		N_("_Start Debugger"),
		NULL,
		NULL,
		NULL
	},
	{
		"ActionDebuggerRunTarget",
		NULL,
		N_("_Debug Program"),
		"<shift>F12",
		N_("Start debugger and load the program"),
		G_CALLBACK (on_start_debug_activate)
	},
	{
		"ActionDebuggerAttachProcess",
		"debugger-attach",
		N_("_Debug Process…"),
		NULL,
		N_("Start debugger and attach to a running program"),
		G_CALLBACK (on_attach_to_project_action_activate)
	},
	{
		"ActionDebuggerDebugRemote",
		"debugger-remote-target",
		N_("Debug _Remote Target…"),
		NULL,
		N_("Connect to a remote debugging target"),
		G_CALLBACK (on_start_remote_debug_action_activate),
	},
	{
		"ActionDebuggerStop",
		GTK_STOCK_STOP,
		N_("Stop Debugger"),
		NULL,
		N_("Say goodbye to the debugger"),
		G_CALLBACK (on_debugger_stop_activate)
	},
	{
		"ActionDebuggerAddSource",
		NULL,
		N_("Add source paths…"),
		NULL,
		N_("Add additional source paths"),
		G_CALLBACK (on_add_source_activate)
	}
};

static GtkActionEntry actions_loaded[] =
{
	{
		"ActionGdbCommand",                              /* Action name */
		NULL,                                            /* Stock icon, if any */
		N_("Debugger Command…"),                       /* Display label */
		NULL,                                            /* short-cut */
		N_("Custom debugger command"),                   /* Tooltip */
		G_CALLBACK (on_debugger_custom_command_activate) /* action callback */
	},
	{
		"ActionMenuGdbInformation",
		NULL,
		N_("_Info"),
		NULL,
		NULL,
		NULL
	},
	{
		"ActionGdbViewSharedlibs",
		NULL,
		N_("Shared Libraries"),
		NULL,
		N_("Show shared library mappings"),
		G_CALLBACK (on_debugger_sharedlibs_activate)
	},
	{
		"ActionGdbViewSignals",
		NULL,
		N_("Kernel Signals"),
		NULL,
		N_("Show kernel signals"),
		G_CALLBACK (on_debugger_signals_activate)
	},
	{
		"ActionDebuggerContinueSuspend",
		GTK_STOCK_MEDIA_PLAY,
		N_("_Continue/Suspend"),
		"F4",
		N_("Continue or suspend the execution of the program"),
		G_CALLBACK (on_continue_suspend_action_activate)
	}
};

static GtkActionEntry actions_stopped[] =
{
	{
		"ActionDebuggerRunContinue",                   /* Action name */
		GTK_STOCK_MEDIA_PLAY,                             /* Stock icon, if any */
		N_("Run/_Continue"),                           /* Display label */
		"F4",                                          /* short-cut */
		N_("Continue the execution of the program"),   /* Tooltip */
		G_CALLBACK (on_run_continue_action_activate)   /* action callback */
	},
	{
		"ActionDebuggerStepIn",
		"debugger-step-into",
		N_("Step _In"),
		"F5",
		N_("Single step into function"),
		G_CALLBACK (on_step_in_action_activate)
	},
	{
		"ActionDebuggerStepOver",
		"debugger-step-over",
		N_("Step O_ver"),
		"F6",
		N_("Single step over function"),
		G_CALLBACK (on_step_over_action_activate)
	},
	{
		"ActionDebuggerStepOut",
		"debugger-step-out",
		N_("Step _Out"),
		"<shift>F5",
		N_("Single step out of function"),
		G_CALLBACK (on_step_out_action_activate)
	},
	{
		"ActionDebuggerRunToPosition",
		"debugger-run-to-cursor",
		N_("_Run to Cursor"),
		"F8",
		N_("Run to the cursor"),
		G_CALLBACK (on_run_to_cursor_action_activate)
	},
	{
		"ActionDebuggerRunFromPosition",
		"debugger-run-from-cursor",
		N_("_Run from Cursor"),
		NULL,
		N_("Run from the cursor"),
		G_CALLBACK (on_run_from_cursor_action_activate)
	},
	{
		"ActionGdbCommand",
		NULL,
		N_("Debugger Command…"),
		NULL,
		N_("Custom debugger command"),
		G_CALLBACK (on_debugger_custom_command_activate)
	},
	{
		"ActionMenuGdbInformation",
		NULL,
		N_("_Info"),
		NULL,
		NULL,
		NULL
	},
	{
		"ActionGdbViewSharedlibs",
		NULL,
		N_("Shared Libraries"),
		NULL,
		N_("Show shared library mappings"),
		G_CALLBACK (on_debugger_sharedlibs_activate)
	},
	{
		"ActionGdbViewSignals",
		NULL,
		N_("Kernel Signals"),
		NULL,
		N_("Show kernel signals"),
		G_CALLBACK (on_debugger_signals_activate)
	}
};

static GtkActionEntry actions_running[] =
{
    {
		"ActionGdbPauseProgram",                       /* Action name */
		GTK_STOCK_MEDIA_PAUSE,                        /* Stock icon, if any */
		N_("Pa_use Program"),                          /* Display label */
		NULL,                                          /* short-cut */
		N_("Pauses the execution of the program"),     /* Tooltip */
		G_CALLBACK (on_debugger_interrupt_activate)    /* action callback */
	},
};

/* AnjutaPlugin functions
 *---------------------------------------------------------------------------*/

static gboolean
dma_plugin_activate (AnjutaPlugin* plugin)
{
	DebugManagerPlugin *this;
	static gboolean initialized = FALSE;
	AnjutaUI *ui;

	DEBUG_PRINT ("%s", "DebugManagerPlugin: Activating Debug Manager plugin…");
	this = ANJUTA_PLUGIN_DEBUG_MANAGER (plugin);

    if (!initialized)
    {
		initialized = TRUE;
		register_stock_icons (ANJUTA_PLUGIN (plugin));
	}

	/* Load debugger */
	this->queue = dma_debugger_queue_new (plugin);
	g_signal_connect (this, "debugger-started", G_CALLBACK (dma_plugin_debugger_started), this);
	g_signal_connect (this, "debugger-stopped", G_CALLBACK (dma_plugin_debugger_stopped), this);
	g_signal_connect (this, "program-loaded", G_CALLBACK (dma_plugin_program_loaded), this);
	g_signal_connect (this, "program-running", G_CALLBACK (dma_plugin_program_running), this);
	g_signal_connect (this, "program-stopped", G_CALLBACK (dma_plugin_program_stopped), this);
	g_signal_connect (this, "program-exited", G_CALLBACK (dma_plugin_program_loaded), this);
	g_signal_connect (this, "program-moved", G_CALLBACK (dma_plugin_program_moved), this);
	g_signal_connect (this, "signal-received", G_CALLBACK (dma_plugin_signal_received), this);
	g_signal_connect (this, "location-changed", G_CALLBACK (dma_plugin_location_changed), this);

	/* Add all our debug manager actions */
	ui = anjuta_shell_get_ui (ANJUTA_PLUGIN (plugin)->shell, NULL);
	this->start_group =
		anjuta_ui_add_action_group_entries (ui, "ActionGroupDebugStart",
											_("Debugger operations"),
											actions_start,
											G_N_ELEMENTS (actions_start),
											GETTEXT_PACKAGE, TRUE, this);
	this->loaded_group =
		anjuta_ui_add_action_group_entries (ui, "ActionGroupDebugLoaded",
											_("Debugger operations"),
											actions_loaded,
											G_N_ELEMENTS (actions_loaded),
											GETTEXT_PACKAGE, TRUE, this);
	this->stopped_group =
		anjuta_ui_add_action_group_entries (ui, "ActionGroupDebugStopped",
											_("Debugger operations"),
											actions_stopped,
											G_N_ELEMENTS (actions_stopped),
											GETTEXT_PACKAGE, TRUE, this);
	this->running_group =
		anjuta_ui_add_action_group_entries (ui, "ActionGroupDebugRunning",
											_("Debugger operations"),
											actions_running,
											G_N_ELEMENTS (actions_running),
											GETTEXT_PACKAGE, TRUE, this);
	this->uiid = anjuta_ui_merge (ui, UI_FILE);

	/* Get run_stop_action */
	this->run_stop_action = anjuta_ui_get_action (ui, "ActionGroupDebugLoaded", "ActionDebuggerContinueSuspend");

	/* Variable */
	this->variable = dma_variable_dbase_new (this);

	/* Stack trace */
	this->stack = stack_trace_new (this);

	/* Create breakpoints list */
	this->breakpoints = breakpoints_dbase_new (this);

	/* Register list */
	this->registers = cpu_registers_new (this);

	/* Memory window */
	this->memory = dma_memory_new (this);

	/* Disassembly window */
	this->disassemble = dma_disassemble_new (this);

	/* Start debugger part */
	this->start = dma_start_new (this);

	/* Shared libraries part */
	this->sharedlibs = sharedlibs_new (this);

	/* Signal part */
	this->signals = signals_new (this);

	dma_plugin_debugger_stopped (this, 0);

	/* Add watches */
	this->project_watch_id =
		anjuta_plugin_add_watch (plugin, IANJUTA_PROJECT_MANAGER_PROJECT_ROOT_URI,
								 value_added_project_root_uri,
								 value_removed_project_root_uri, NULL);
	this->editor_watch_id =
		anjuta_plugin_add_watch (plugin, IANJUTA_DOCUMENT_MANAGER_CURRENT_DOCUMENT,
								 value_added_current_editor,
								 value_removed_current_editor, NULL);

	return TRUE;
}

static gboolean
dma_plugin_deactivate (AnjutaPlugin* plugin)
{
	DebugManagerPlugin *this;
	AnjutaUI *ui;

	DEBUG_PRINT ("%s", "DebugManagerPlugin: Deactivating Debug Manager plugin…");

	this = ANJUTA_PLUGIN_DEBUG_MANAGER (plugin);

	/* Stop debugger */
	dma_plugin_debugger_stopped (this, 0);

	g_signal_handlers_disconnect_matched (plugin, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, plugin);

	anjuta_plugin_remove_watch (plugin, this->project_watch_id, FALSE);
	anjuta_plugin_remove_watch (plugin, this->editor_watch_id, FALSE);

	dma_debugger_queue_free (this->queue);
	this->queue = NULL;

	ui = anjuta_shell_get_ui (plugin->shell, NULL);
	anjuta_ui_unmerge (ui, this->uiid);

	dma_variable_dbase_free (this->variable);
	this->variable = NULL;

	breakpoints_dbase_destroy (this->breakpoints);
	this->breakpoints = NULL;

	stack_trace_free (this->stack);
	this->stack = NULL;

	cpu_registers_free (this->registers);
	this->registers = NULL;

	dma_memory_free (this->memory);
	this->memory = NULL;

	dma_disassemble_free (this->disassemble);
	this->disassemble = NULL;

	dma_start_free (this->start);
	this->start = NULL;

	sharedlibs_free (this->sharedlibs);
	this->sharedlibs = NULL;

	signals_free (this->signals);
	this->signals = NULL;

	ui = anjuta_shell_get_ui (ANJUTA_PLUGIN (this)->shell, NULL);
	anjuta_ui_remove_action_group (ui, this->start_group);
	anjuta_ui_remove_action_group (ui, this->loaded_group);
	anjuta_ui_remove_action_group (ui, this->stopped_group);
	anjuta_ui_remove_action_group (ui, this->running_group);

	if (this->view != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (this->view), (gpointer*)(gpointer)&this->view);
        this->view = NULL;
	}

	return TRUE;
}

/* Public functions
 *---------------------------------------------------------------------------*/

DmaDebuggerQueue*
dma_debug_manager_get_queue (DebugManagerPlugin *self)
{
	return self->queue;
}

/* GObject functions
 *---------------------------------------------------------------------------*/

/* Used in dispose and finalize */
static gpointer parent_class;

/* instance_init is the constructor. All functions should work after this
 * call. */

static void
dma_plugin_instance_init (GObject* obj)
{
	DebugManagerPlugin *plugin = ANJUTA_PLUGIN_DEBUG_MANAGER (obj);

	plugin->uiid = 0;

	plugin->project_root_uri = NULL;
	plugin->queue = NULL;
	plugin->current_editor = NULL;
	plugin->pc_editor = NULL;
	plugin->editor_watch_id = 0;
	plugin->project_watch_id = 0;
	plugin->breakpoints = NULL;
	plugin->registers = NULL;
	plugin->signals = NULL;
	plugin->sharedlibs = NULL;
	plugin->view = NULL;

	plugin->user_command_dialog = NULL;

	/* plugin->uri = NULL; */
}

/* dispose is the first destruction step. It is used to unref object created
 * with instance_init in order to break reference counting cycles. This
 * function could be called several times. All function should still work
 * after this call. It has to called its parents.*/

static void
dma_plugin_dispose (GObject* obj)
{
	DebugManagerPlugin *plugin = ANJUTA_PLUGIN_DEBUG_MANAGER (obj);

	if (plugin->user_command_dialog != NULL)	gtk_widget_destroy (GTK_WIDGET (plugin->user_command_dialog));

	G_OBJECT_CLASS (parent_class)->dispose (obj);
}

/* finalize is the last destruction step. It must free all memory allocated
 * with instance_init. It is called only one time just before releasing all
 * memory */

static void
dma_plugin_finalize (GObject* obj)
{
	DebugManagerPlugin *self = ANJUTA_PLUGIN_DEBUG_MANAGER (obj);

	if (self->pc_editor != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (self->pc_editor), (gpointer *)(gpointer)&self->pc_editor);
	}
	if (self->current_editor != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (self->current_editor), (gpointer *)(gpointer)&self->current_editor);
	}

	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/* class_init intialize the class itself not the instance */

static void
dma_plugin_class_init (GObjectClass* klass)
{
	AnjutaPluginClass *plugin_class = ANJUTA_PLUGIN_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	plugin_class->activate = dma_plugin_activate;
	plugin_class->deactivate = dma_plugin_deactivate;
	plugin_class->load_session = dma_plugin_load_session;
	plugin_class->save_session = dma_plugin_save_session;
	klass->dispose = dma_plugin_dispose;
	klass->finalize = dma_plugin_finalize;
}

/* Implementation of IAnjutaDebugManager interface
 *---------------------------------------------------------------------------*/

static gboolean
idebug_manager_start (IAnjutaDebugManager *plugin, const gchar *uri, GError **err)
{
	DebugManagerPlugin *self = ANJUTA_PLUGIN_DEBUG_MANAGER (plugin);

	return dma_run_target (self->start, uri);
}

static gboolean
idebug_manager_start_remote (IAnjutaDebugManager *plugin, const gchar *server, const gchar *uri, GError **err)
{
	DebugManagerPlugin *self = ANJUTA_PLUGIN_DEBUG_MANAGER (plugin);

	return dma_run_remote_target (self->start, server, uri);
}

static gboolean
idebug_manager_quit (IAnjutaDebugManager *plugin, GError **err)
{
	DebugManagerPlugin *self = ANJUTA_PLUGIN_DEBUG_MANAGER (plugin);

	return dma_quit_debugger (self->start);
}

static void
idebug_manager_iface_init (IAnjutaDebugManagerIface *iface)
{
	iface->start = idebug_manager_start;
	iface->start_remote = idebug_manager_start_remote;
	iface->quit = idebug_manager_quit;
}

ANJUTA_PLUGIN_BEGIN (DebugManagerPlugin, dma_plugin);
ANJUTA_PLUGIN_ADD_INTERFACE(idebug_manager, IANJUTA_TYPE_DEBUG_MANAGER);
ANJUTA_PLUGIN_END;

ANJUTA_SIMPLE_PLUGIN (DebugManagerPlugin, dma_plugin);
