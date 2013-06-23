/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
    plugin.c
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

#include <config.h>
#include <ctype.h>
#include <stdlib.h>
#include <libanjuta/anjuta-shell.h>
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/anjuta-utils.h>
#include <libanjuta/anjuta-modeline.h>
#include <libanjuta/interfaces/ianjuta-iterable.h>
#include <libanjuta/interfaces/ianjuta-document.h>
#include <libanjuta/interfaces/ianjuta-document-manager.h>
#include <libanjuta/interfaces/ianjuta-editor.h>
#include <libanjuta/interfaces/ianjuta-file.h>
#include <libanjuta/interfaces/ianjuta-editor-cell.h>
#include <libanjuta/interfaces/ianjuta-editor-language.h>
#include <libanjuta/interfaces/ianjuta-editor-selection.h>
#include <libanjuta/interfaces/ianjuta-preferences.h>
#include <libanjuta/interfaces/ianjuta-symbol-manager.h>
#include <libanjuta/interfaces/ianjuta-language.h>
#include <libanjuta/interfaces/ianjuta-indenter.h>

#include "plugin.h"
#include "indentation.h"

/* Pixmaps */
#define ANJUTA_PIXMAP_SWAP                "anjuta-swap"
#define ANJUTA_PIXMAP_AUTOINDENT          "anjuta-indent-auto"
#define ANJUTA_STOCK_SWAP                 "anjuta-swap"
#define ANJUTA_STOCK_AUTOINDENT           "anjuta-indent"

#define UI_FILE PACKAGE_DATA_DIR"/ui/anjuta-indentation-c-style.xml"
#define PREFS_BUILDER PACKAGE_DATA_DIR"/glade/anjuta-indentation-c-style.ui"
#define ICON_FILE "anjuta-indentation-c-style-plugin.png"

/* Preferences keys */

#define ANJUTA_PREF_SCHEMA_PREFIX "org.gnome.anjuta."
#define PREF_SCHEMA "org.gnome.anjuta.plugins.indent-c"
#define PREF_INDENT_AUTOMATIC "indent-automatic"
#define PREF_INDENT_MODELINE "indent-modeline"

static gpointer parent_class;


static void
initialize_indentation_params (IndentCPlugin *plugin)
{
    plugin->smart_indentation = g_settings_get_boolean (plugin->settings, PREF_INDENT_AUTOMATIC);
    /* Disable editor intern auto-indent if smart indentation is enabled */
    ianjuta_editor_set_auto_indent (IANJUTA_EDITOR(plugin->current_editor),
                                    !plugin->smart_indentation, NULL);

    /* Initialize indentation parameters */
    plugin->param_brace_indentation = -1;
    plugin->param_case_indentation = -1;
    plugin->param_label_indentation = -1;

    if (g_settings_get_boolean (plugin->settings,
                                PREF_INDENT_MODELINE))
    {
        anjuta_apply_modeline (IANJUTA_EDITOR (plugin->current_editor));
    }
}

static gboolean
language_is_supported (const gchar* language)
{
    return (language &&
            (g_str_equal (language, "C") ||
             g_str_equal (language, "C++") ||
             g_str_equal (language, "Vala") ||
             g_str_equal (language, "Java") ||
             g_str_equal (language, "JavaScript") ||
             g_str_equal (language, "IDL")));
}

/* Enable/Disable language-support */
static void
install_support (IndentCPlugin *lang_plugin)
{
    IAnjutaLanguage* lang_manager =
        anjuta_shell_get_interface (ANJUTA_PLUGIN (lang_plugin)->shell,
                                    IAnjutaLanguage, NULL);

    if (!lang_manager)
        return;

    if (lang_plugin->support_installed)
        return;

    lang_plugin->current_language =
        ianjuta_language_get_name_from_editor (lang_manager,
                                               IANJUTA_EDITOR_LANGUAGE (lang_plugin->current_editor), NULL);

    DEBUG_PRINT("Indentation support installed for: %s",
                lang_plugin->current_language);

    if (language_is_supported (lang_plugin->current_language))
    {
        g_signal_connect (lang_plugin->current_editor,
                          "char-added",
                          G_CALLBACK (cpp_java_indentation_char_added),
                          lang_plugin);
        g_signal_connect (lang_plugin->current_editor,
                          "changed",
                          G_CALLBACK (cpp_java_indentation_changed),
                          lang_plugin);
    }
    else
    {
        return;
    }

    initialize_indentation_params (lang_plugin);
    lang_plugin->support_installed = TRUE;
}

static void
uninstall_support (IndentCPlugin *lang_plugin)
{
    if (!lang_plugin->support_installed)
        return;

    if (language_is_supported (lang_plugin->current_language))
    {
        g_signal_handlers_disconnect_by_func (lang_plugin->current_editor,
                                    G_CALLBACK (cpp_java_indentation_char_added),
                                    lang_plugin);
        g_signal_handlers_disconnect_by_func (lang_plugin->current_editor,
                                    G_CALLBACK (cpp_java_indentation_changed),
                                    lang_plugin);
    }
    
    lang_plugin->support_installed = FALSE;
}

static void
on_editor_language_changed (IAnjutaEditor *editor,
                            const gchar *new_language,
                            IndentCPlugin *plugin)
{
    uninstall_support (plugin);
    install_support (plugin);
}

static void
on_value_added_current_editor (AnjutaPlugin *plugin, const gchar *name,
                               const GValue *value, gpointer data)
{
    IndentCPlugin *lang_plugin;
    IAnjutaDocument* doc = IANJUTA_DOCUMENT(g_value_get_object (value));
    lang_plugin = ANJUTA_PLUGIN_INDENT_C (plugin);
    if (IANJUTA_IS_EDITOR(doc))
        lang_plugin->current_editor = G_OBJECT(doc);
    else
    {
        lang_plugin->current_editor = NULL;
        return;
    }
    if (IANJUTA_IS_EDITOR(lang_plugin->current_editor))
        install_support (lang_plugin);
    g_signal_connect (lang_plugin->current_editor, "language-changed",
                      G_CALLBACK (on_editor_language_changed),
                      plugin);
}

static void
on_value_removed_current_editor (AnjutaPlugin *plugin, const gchar *name,
                                 gpointer data)
{
    IndentCPlugin *lang_plugin;
    lang_plugin = ANJUTA_PLUGIN_INDENT_C (plugin);
    if (lang_plugin->current_editor)
        g_signal_handlers_disconnect_by_func (lang_plugin->current_editor,
                                          G_CALLBACK (on_editor_language_changed),
                                          plugin);
    if (IANJUTA_IS_EDITOR(lang_plugin->current_editor))
        uninstall_support (lang_plugin);
    lang_plugin->current_editor = NULL;
}

static void
on_auto_indent (GtkAction *action, gpointer data)
{
    IndentCPlugin *lang_plugin;
    IAnjutaEditor *editor;
    lang_plugin = ANJUTA_PLUGIN_INDENT_C (data);
    editor = IANJUTA_EDITOR (lang_plugin->current_editor);

    cpp_auto_indentation (editor, lang_plugin, NULL, NULL);
}

/* Plugin */

static GtkActionEntry actions[] = {
    {
        "ActionMenuEdit",
        NULL, N_("_Edit"),
        NULL, NULL, NULL
    },
    {
        "ActionEditAutoindent",
        ANJUTA_STOCK_AUTOINDENT,
        N_("Auto-Indent"), "<control>i",
        N_("Auto-indent current line or selection based on indentation settings"),
        G_CALLBACK (on_auto_indent)
    }
};

static void
register_stock_icons (AnjutaPlugin *plugin)
{
    static gboolean registered = FALSE;

    if (registered)
        return;
    registered = TRUE;

    /* Register stock icons */
    BEGIN_REGISTER_ICON (plugin);
    REGISTER_ICON_FULL (ANJUTA_PIXMAP_SWAP, ANJUTA_STOCK_SWAP);
    REGISTER_ICON_FULL (ANJUTA_PIXMAP_AUTOINDENT, ANJUTA_STOCK_AUTOINDENT);
    END_REGISTER_ICON;
}

static gboolean
indent_c_plugin_activate_plugin (AnjutaPlugin *plugin)
{
    AnjutaUI *ui;
    IndentCPlugin *lang_plugin;
    static gboolean initialized = FALSE;

    lang_plugin = ANJUTA_PLUGIN_INDENT_C (plugin);

    DEBUG_PRINT ("%s", "AnjutaIndentCPlugin: Activating plugin ...");

    if (!initialized)
    {
        register_stock_icons (plugin);
    }

    ui = anjuta_shell_get_ui (plugin->shell, NULL);
    lang_plugin->action_group =
        anjuta_ui_add_action_group_entries (ui, "ActionGroupCIndent",
                                            _("C Indentation"),
                                            actions,
                                            G_N_ELEMENTS (actions),
                                            GETTEXT_PACKAGE, TRUE,
                                            plugin);
    lang_plugin->uiid = anjuta_ui_merge (ui, UI_FILE);

    lang_plugin->editor_watch_id =
        anjuta_plugin_add_watch (plugin,
                                  IANJUTA_DOCUMENT_MANAGER_CURRENT_DOCUMENT,
                                 on_value_added_current_editor,
                                 on_value_removed_current_editor,
                                 plugin);

    initialized = FALSE;
    return TRUE;
}

static gboolean
indent_c_plugin_deactivate_plugin (AnjutaPlugin *plugin)
{
    AnjutaUI *ui;
    IndentCPlugin *lang_plugin;
    lang_plugin = ANJUTA_PLUGIN_INDENT_C (plugin);

    anjuta_plugin_remove_watch (plugin,
                                lang_plugin->editor_watch_id,
                                TRUE);

    ui = anjuta_shell_get_ui (plugin->shell, NULL);
    anjuta_ui_remove_action_group (ui, lang_plugin->action_group);
    anjuta_ui_unmerge (ui, lang_plugin->uiid);

    lang_plugin->uiid = 0;
    DEBUG_PRINT ("%s", "AnjutaIndentCPlugin: Deactivated plugin.");
    return TRUE;
}

static void
indent_c_plugin_finalize (GObject *obj)
{
    /* Finalization codes here */
    G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
indent_c_plugin_dispose (GObject *obj)
{
    IndentCPlugin* plugin = ANJUTA_PLUGIN_INDENT_C (obj);
    /* Disposition codes */

    g_object_unref (plugin->settings);
    g_object_unref (plugin->editor_settings);

    G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
indent_c_plugin_instance_init (GObject *obj)
{
    IndentCPlugin *plugin = ANJUTA_PLUGIN_INDENT_C (obj);
    plugin->current_editor = NULL;
    plugin->current_language = NULL;
    plugin->editor_watch_id = 0;
    plugin->uiid = 0;
    plugin->settings = g_settings_new (PREF_SCHEMA);
    plugin->editor_settings = g_settings_new (ANJUTA_PREF_SCHEMA_PREFIX IANJUTA_EDITOR_PREF_SCHEMA);
}

static void
indent_c_plugin_class_init (GObjectClass *klass)
{
    AnjutaPluginClass *plugin_class = ANJUTA_PLUGIN_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    plugin_class->activate = indent_c_plugin_activate_plugin;
    plugin_class->deactivate = indent_c_plugin_deactivate_plugin;
    klass->finalize = indent_c_plugin_finalize;
    klass->dispose = indent_c_plugin_dispose;
}

static void
ipreferences_merge (IAnjutaPreferences* ipref, AnjutaPreferences* prefs,
                    GError** e)
{
    GError* error = NULL;
    IndentCPlugin* plugin = ANJUTA_PLUGIN_INDENT_C (ipref);
    plugin->bxml = gtk_builder_new ();

    /* Add preferences */
    if (!gtk_builder_add_from_file (plugin->bxml, PREFS_BUILDER, &error))
    {
        g_warning ("Couldn't load builder file: %s", error->message);
        g_error_free (error);
    }
    anjuta_preferences_add_from_builder (prefs,
                                         plugin->bxml, plugin->settings, NULL,
                                         "preferences", _("Indentation"),
                                         ICON_FILE);
}

static void
ipreferences_unmerge (IAnjutaPreferences* ipref, AnjutaPreferences* prefs,
                      GError** e)
{
    IndentCPlugin* plugin = ANJUTA_PLUGIN_INDENT_C (ipref);
    anjuta_preferences_remove_page(prefs, _("Indentation"));
    g_object_unref (plugin->bxml);
}

static void
ipreferences_iface_init (IAnjutaPreferencesIface* iface)
{
    iface->merge = ipreferences_merge;
    iface->unmerge = ipreferences_unmerge;
}

static void
iindenter_indent (IAnjutaIndenter* indenter,
                  IAnjutaIterable* start,
                  IAnjutaIterable* end,
                  GError** e)
{
    IndentCPlugin* plugin = ANJUTA_PLUGIN_INDENT_C (indenter);

    cpp_auto_indentation (IANJUTA_EDITOR (plugin->current_editor),
                          plugin,
                          start, end);
}

static void
iindenter_iface_init (IAnjutaIndenterIface* iface)
{
    iface->indent = iindenter_indent;
}

ANJUTA_PLUGIN_BEGIN (IndentCPlugin, indent_c_plugin);
ANJUTA_PLUGIN_ADD_INTERFACE(ipreferences, IANJUTA_TYPE_PREFERENCES);
ANJUTA_PLUGIN_ADD_INTERFACE(iindenter, IANJUTA_TYPE_INDENTER);
ANJUTA_PLUGIN_END;

ANJUTA_SIMPLE_PLUGIN (IndentCPlugin, indent_c_plugin);
