/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Library General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include "sourceview-prefs.h"
#include "sourceview-private.h"
#include "sourceview-provider.h"
#include <gtksourceview/gtksource.h>
#include <gtksourceview/completion-providers/words/gtksourcecompletionwords.h>

#include <libanjuta/anjuta-debug.h>
#include <libanjuta/interfaces/ianjuta-editor.h>

#define REGISTER_NOTIFY(settings, key, func) \
	g_signal_connect_object (settings, "changed::" key, G_CALLBACK(func), sv, 0);

#define ANJUTA_PREF_SCHEMA_PREFIX "org.gnome.anjuta."
#define PREF_SCHEMA "org.gnome.anjuta.plugins.sourceview"
#define MSGMAN_PREF_SCHEMA "org.gnome.anjuta.plugins.message-manager"

/* Editor preferences */
#define HIGHLIGHT_SYNTAX           "syntax-highlight"
#define HIGHLIGHT_CURRENT_LINE	   "currentline-highlight"
#define HIGHLIGHT_BRACKETS         "brackets-highlight"
#define INDENT_SIZE                "indent-size"
#define AUTOCOMPLETION             "autocomplete"

#define VIEW_RIGHTMARGIN           "rightmargin-visible"
#define RIGHTMARGIN_POSITION       "rightmargin-position"

#define MSGMAN_COLOR_ERROR		  "color-error"
#define MSGMAN_COLOR_WARNING		  "color-warning"
#define MSGMAN_COLOR_IMPORTANT		  "color-important"

#define FONT_THEME "font-use-theme"
#define FONT "font"
#define FONT_SCHEMA "org.gnome.desktop.interface"
#define GNOME_DOCUMENT_FONT "document-font-name"

static void
on_notify_view_spaces (GSettings* settings,
                       const gchar* key,
                       gpointer user_data)
{
	Sourceview *sv;
	sv = ANJUTA_SOURCEVIEW(user_data);
	GtkSourceDrawSpacesFlags flags =
		gtk_source_view_get_draw_spaces (GTK_SOURCE_VIEW (sv->priv->view));

	if (g_settings_get_boolean (settings, key))
		flags |= (GTK_SOURCE_DRAW_SPACES_SPACE | GTK_SOURCE_DRAW_SPACES_TAB);
	else
		flags &= ~(GTK_SOURCE_DRAW_SPACES_SPACE | GTK_SOURCE_DRAW_SPACES_TAB);

	gtk_source_view_set_draw_spaces (GTK_SOURCE_VIEW(sv->priv->view),
																	 flags);
}

static void
on_notify_view_eol (GSettings* settings,
                    const gchar* key,
                    gpointer user_data)
{
	Sourceview *sv;
	sv = ANJUTA_SOURCEVIEW(user_data);
	GtkSourceDrawSpacesFlags flags =
		gtk_source_view_get_draw_spaces (GTK_SOURCE_VIEW (sv->priv->view));

	if (g_settings_get_boolean (settings, key))
		flags |= GTK_SOURCE_DRAW_SPACES_NEWLINE;
	else
		flags &= ~GTK_SOURCE_DRAW_SPACES_NEWLINE;

	gtk_source_view_set_draw_spaces (GTK_SOURCE_VIEW(sv->priv->view),
																	 flags);
}

static void
on_notify_line_wrap (GSettings* settings,
                           const gchar* key,
                           gpointer user_data)
{
	Sourceview *sv;
	sv = ANJUTA_SOURCEVIEW(user_data);

	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (sv->priv->view),
	                             g_settings_get_boolean (settings, key) ? GTK_WRAP_WORD : GTK_WRAP_NONE);
}

static void
on_notify_use_tab_for_indentation (GSettings* settings,
                                   const gchar* key,
                                   gpointer user_data)
{
	Sourceview *sv;
	sv = ANJUTA_SOURCEVIEW(user_data);

	gtk_source_view_set_insert_spaces_instead_of_tabs(GTK_SOURCE_VIEW(sv->priv->view),
	                                                  !g_settings_get_boolean (settings, key));
}

static void
on_notify_autocompletion (GSettings* settings,
                         const gchar* key,
                         gpointer user_data)
{
	Sourceview *sv;
	sv = ANJUTA_SOURCEVIEW(user_data);
	GtkSourceCompletion* completion = gtk_source_view_get_completion(GTK_SOURCE_VIEW(sv->priv->view));

	if (g_settings_get_boolean (settings, key))
	{
		DEBUG_PRINT ("Register word completion provider");
		GtkSourceCompletionWords *prov_words;
		prov_words = gtk_source_completion_words_new (NULL, NULL);

		gtk_source_completion_words_register (prov_words,
		                                      gtk_text_view_get_buffer (GTK_TEXT_VIEW (sv->priv->view)));

		gtk_source_completion_add_provider (completion,
		                                    GTK_SOURCE_COMPLETION_PROVIDER (prov_words),
		                                    NULL);
	}
	else
	{
		GList* node;
		for (node = gtk_source_completion_get_providers(completion); node != NULL; node = g_list_next (node))
		{
			if (GTK_SOURCE_IS_COMPLETION_WORDS(node->data))
			{
				DEBUG_PRINT ("Unregister word completion provider");
				gtk_source_completion_words_unregister (GTK_SOURCE_COMPLETION_WORDS(node->data),
				                                        gtk_text_view_get_buffer (GTK_TEXT_VIEW (sv->priv->view)));
				gtk_source_completion_remove_provider(completion, GTK_SOURCE_COMPLETION_PROVIDER(node->data), NULL);
				break;
			}
		}
	}
}

static void
on_notify_font (GSettings* settings,
                const gchar* key,
                gpointer user_data)
{
	Sourceview *sv;
	sv = ANJUTA_SOURCEVIEW(user_data);
	gchar* font = g_settings_get_string (settings, key);

	anjuta_view_set_font(sv->priv->view, FALSE,
	                     font);
	g_free (font);
}

static void
on_notify_font_theme (GSettings* settings,
                      const gchar* key,
                      gpointer user_data)
{
	Sourceview *sv;
	sv = ANJUTA_SOURCEVIEW(user_data);

	if (g_settings_get_boolean (settings, key))
	{
		GSettings* font_settings = g_settings_new (FONT_SCHEMA);
		gchar* desktop_font = g_settings_get_string (font_settings,
		                                             GNOME_DOCUMENT_FONT);

		if (desktop_font)
			anjuta_view_set_font(sv->priv->view, FALSE, desktop_font);		
		else
			anjuta_view_set_font(sv->priv->view, TRUE, NULL);
		g_free (desktop_font);
		g_object_unref (font_settings);
	}
	else
	{
		gchar* font = g_settings_get_string (settings, FONT);
		anjuta_view_set_font(sv->priv->view, FALSE, font);
		g_free (font);
	}
}

/* Preferences notifications */
static void
on_notify_indic_colors (GSettings* settings,
                        const gchar *key,
                        gpointer user_data)
{
	char* error_color =
		 g_settings_get_string (settings,
		                        MSGMAN_COLOR_ERROR);
	char* warning_color =
		 g_settings_get_string (settings,
		                        MSGMAN_COLOR_WARNING);
	char* important_color =
		 g_settings_get_string (settings,
		                        MSGMAN_COLOR_IMPORTANT);
	Sourceview* sv = ANJUTA_SOURCEVIEW (user_data);

	g_object_set (sv->priv->warning_indic, "foreground", warning_color, NULL);
	g_object_set (sv->priv->critical_indic, "foreground", error_color, NULL);
	g_object_set (sv->priv->important_indic, "background", important_color, NULL);

	g_free (error_color);
	g_free (warning_color);
	g_free (important_color);
}

static void
init_fonts(Sourceview* sv)
{
	on_notify_font_theme (sv->priv->settings, FONT_THEME, sv);
}

static void
source_view_prefs_load_session(Sourceview* sv, AnjutaSession* session)
{
    g_clear_object (&sv->priv->editor_settings);

    sv->priv->editor_settings = anjuta_session_create_settings (session, IANJUTA_EDITOR_PREF_SCHEMA);

	/* Bind simple options to GSettings */
	g_settings_bind (sv->priv->editor_settings, IANJUTA_EDITOR_TAB_WIDTH_KEY,
	                 sv->priv->view, "tab-width",
	                 G_SETTINGS_BIND_GET);
	g_settings_bind (sv->priv->editor_settings, IANJUTA_EDITOR_INDENT_WIDTH_KEY,
	                 sv->priv->view, "indent-width",
	                 G_SETTINGS_BIND_GET);

	/* Register notifications */
	REGISTER_NOTIFY (sv->priv->editor_settings, IANJUTA_EDITOR_USE_TABS_KEY, on_notify_use_tab_for_indentation);

	/* Init non-simple options */
	gtk_source_view_set_indent_width(GTK_SOURCE_VIEW(sv->priv->view),
	                                 g_settings_get_int (sv->priv->editor_settings, IANJUTA_EDITOR_INDENT_WIDTH_KEY));
	gtk_source_view_set_tab_width(GTK_SOURCE_VIEW(sv->priv->view),
	                              g_settings_get_int (sv->priv->editor_settings, IANJUTA_EDITOR_TAB_WIDTH_KEY));
	gtk_source_view_set_insert_spaces_instead_of_tabs(GTK_SOURCE_VIEW(sv->priv->view),
	                                                  !g_settings_get_boolean (sv->priv->editor_settings, IANJUTA_EDITOR_USE_TABS_KEY));
}

static void
on_load_session(AnjutaPlugin* plugin, AnjutaSessionPhase phase,
                AnjutaSession* session, gpointer user_data)
{
    Sourceview* sv = ANJUTA_SOURCEVIEW(user_data);

	if (phase != ANJUTA_SESSION_PHASE_NORMAL)
		return;

	source_view_prefs_load_session (sv, session);
}

void
sourceview_prefs_init(Sourceview* sv)
{
	GtkSourceDrawSpacesFlags flags = 0;
	AnjutaSession* session;

	/* We create a new GSettings object here because if we used the one from
	 * the editor might be destroyed while the plugin is still alive
	 */
	sv->priv->settings = g_settings_new (PREF_SCHEMA);
	sv->priv->msgman_settings = g_settings_new (MSGMAN_PREF_SCHEMA);

	/* Bind simple options to GSettings */
	g_settings_bind (sv->priv->settings, HIGHLIGHT_SYNTAX,
			 sv->priv->document, "highlight-syntax",
			 G_SETTINGS_BIND_GET);
	g_settings_bind (sv->priv->settings, HIGHLIGHT_CURRENT_LINE,
			 sv->priv->view, "highlight-current-line",
			 G_SETTINGS_BIND_GET);
	g_settings_bind (sv->priv->settings, HIGHLIGHT_BRACKETS,
			 sv->priv->document, "highlight-matching-brackets",
			 G_SETTINGS_BIND_GET);

	g_settings_bind (sv->priv->settings, VIEW_MARKS,
			 sv->priv->view, "show-line-marks",
			 G_SETTINGS_BIND_GET);

	g_settings_bind (sv->priv->settings, RIGHTMARGIN_POSITION,
			 sv->priv->view, "right-margin-position",
			 G_SETTINGS_BIND_GET);

	g_settings_bind (sv->priv->settings, VIEW_RIGHTMARGIN,
			 sv->priv->view, "show-right-margin",
			 G_SETTINGS_BIND_GET);

	g_settings_bind (sv->priv->settings, VIEW_LINENUMBERS,
			 sv->priv->view, "show-line-numbers",
			 G_SETTINGS_BIND_GET);

	/* Init non-simple options */
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (sv->priv->view),
	                             g_settings_get_boolean (sv->priv->settings, VIEW_EOL) ? GTK_WRAP_WORD : GTK_WRAP_NONE);


	if (g_settings_get_boolean (sv->priv->settings, VIEW_WHITE_SPACES))
		flags |= (GTK_SOURCE_DRAW_SPACES_SPACE | GTK_SOURCE_DRAW_SPACES_TAB);
	if (g_settings_get_boolean (sv->priv->settings, VIEW_EOL))
		flags |= GTK_SOURCE_DRAW_SPACES_NEWLINE;

	gtk_source_view_set_draw_spaces (GTK_SOURCE_VIEW (sv->priv->view),
	                                 flags);

	init_fonts(sv);

	on_notify_autocompletion(sv->priv->settings, AUTOCOMPLETION, sv);

	/* Register notifications */
	REGISTER_NOTIFY (sv->priv->settings, AUTOCOMPLETION, on_notify_autocompletion);
	REGISTER_NOTIFY (sv->priv->settings, VIEW_WHITE_SPACES, on_notify_view_spaces);
	REGISTER_NOTIFY (sv->priv->settings, VIEW_EOL, on_notify_view_eol);
	REGISTER_NOTIFY (sv->priv->settings, VIEW_LINE_WRAP, on_notify_line_wrap);
	REGISTER_NOTIFY (sv->priv->settings, FONT_THEME, on_notify_font_theme);
	REGISTER_NOTIFY (sv->priv->settings, FONT, on_notify_font);

	g_signal_connect (sv->priv->msgman_settings, "changed::" MSGMAN_COLOR_ERROR,
	                  G_CALLBACK (on_notify_indic_colors), sv);
	g_signal_connect (sv->priv->msgman_settings, "changed::" MSGMAN_COLOR_WARNING,
	                  G_CALLBACK (on_notify_indic_colors), sv);
	g_signal_connect (sv->priv->msgman_settings, "changed::" MSGMAN_COLOR_IMPORTANT,
	                  G_CALLBACK (on_notify_indic_colors), sv);

	/* Load session settings if we have a session already */
	session = anjuta_shell_get_session (sv->priv->plugin->shell);
	if (session)
		source_view_prefs_load_session (sv, session);

	g_signal_connect_object (sv->priv->plugin, "load-session",
	                         G_CALLBACK (on_load_session), sv, 0);
}

void sourceview_prefs_destroy(Sourceview* sv)
{
	g_clear_object (&sv->priv->settings);
	g_clear_object (&sv->priv->msgman_settings);
	g_clear_object (&sv->priv->editor_settings);
}
