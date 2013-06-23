/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    Copyright (C) 2009 Maxim Ermilov   <zaspire@rambler.ru>

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
#include <libanjuta/interfaces/ianjuta-indicable.h>
#include <libanjuta/interfaces/ianjuta-editor.h>
#include <libanjuta/interfaces/ianjuta-project-manager.h>
#include <libanjuta/anjuta-session.h>

#include "plugin.h"
#include "util.h"
#include "prefs.h"

static gchar*
get_gjs_path (void)
{
	JSLang* plugin = (JSLang*)getPlugin ();

	if (!plugin->prefs)
		plugin->prefs = g_settings_new (JS_SUPPORT_SCHEMA);

	gchar *path = g_settings_get_string (plugin->prefs, GJS_DIR_KEY);
	if (!path || strlen (path) < 1)
	{
		g_free (path);
		if (strlen (GJS_PATH) == 0)
			return NULL;
		return g_strdup (GJS_PATH);
	}
	return path;
}

GList *
get_import_include_paths ()
{
	JSLang *plugin = getPlugin();
	GList *ret = NULL;
	gchar *path = get_gjs_path ();
	if (path)
		ret = g_list_append (ret, path);

	GList* dir_list = anjuta_util_settings_get_string_list (plugin->session_settings,
	                                                        "js-dirs");
	GList *i;
	for (i = dir_list; i; i = g_list_next (i))
	{
		g_assert (i->data != NULL);
		ret = g_list_append (ret, i->data);
	}
	if (!dir_list)
	{
		ret = g_list_append (ret, g_strdup ("."));
		anjuta_util_settings_set_string_list (plugin->session_settings,
		                                      "js-dirs", ret);
	}

	return ret;
}

gpointer plugin = NULL;

gpointer
getPlugin ()
{
	if (plugin)
		return plugin;
	g_assert_not_reached ();
	return NULL;
}

void
setPlugin (gpointer pl)
{
	plugin = pl;
}

IJsSymbol*
global_search (const gchar *name)
{
	JSLang* plugin = (JSLang*)getPlugin ();
	return ijs_symbol_get_member (IJS_SYMBOL (plugin->symbol), name);
}

void
highlight_lines (GList *lines)
{
	JSLang* plugin = (JSLang*)getPlugin ();

	if (!plugin->prefs)
		plugin->prefs = g_settings_new ("org.gnome.anjuta.plugins.js");

	if (!g_settings_get_boolean (plugin->prefs, HIGHLIGHT_MISSEDSEMICOLON))
	{
		return;
	}

	IAnjutaEditor *editor = IANJUTA_EDITOR (((JSLang*)getPlugin ())->current_editor);
	if (!IANJUTA_IS_EDITOR (editor))
		return;
	IAnjutaIndicable *indicable = IANJUTA_INDICABLE (editor);
	if (!indicable)
		return;

	ianjuta_indicable_clear (indicable, NULL);

	GList *i;
	for (i = lines; i; i = g_list_next (i))
	{
		if (!i->data)
			continue;
		gint line = GPOINTER_TO_INT (i->data);
		IAnjutaIterable* begin = ianjuta_editor_get_line_begin_position (editor, line, NULL);

		IAnjutaIterable *end = ianjuta_editor_get_line_end_position (editor, line, NULL);
		ianjuta_indicable_set (indicable, begin, end, TRUE, NULL);
	}
}

enum code_state
{
	STATE_CODE,
	STATE_QSTR,
	STATE_STR,
	STATE_ONE_LINE_COMMENT,
	STATE_COMMENT
};


gboolean
code_is_in_comment_or_str (gchar *str, gboolean remove_not_code)
{
	g_assert (str != NULL);
	int s;
	gchar *i;
	for (i = str, s = STATE_CODE; *i != '\0'; i++)
	{
		switch (s)
		{
			case STATE_CODE:
				if (*i == '"')
				{
					s = STATE_QSTR;
					i++;
					break;
				}
				if (*i == '\'')
				{
					s = STATE_STR;
					i++;
					break;
				}
				if (*i == '/' && *(i + 1) == '*')
				{
					if (remove_not_code)
						*i = ' ';
					i++;
					s = STATE_COMMENT;
					break;
				}
				if (*i == '/' && *(i + 1) == '/')
				{
					if (remove_not_code)
						*i = ' ';
					i++;
					s = STATE_ONE_LINE_COMMENT;
					break;
				}
				break;
			case STATE_QSTR:
				if (*i == '\\' && *(i + 1) == '"')
				{
					if (remove_not_code)
						*i = ' ';
					i++;
					break;
				}
				if (*i == '"')
					s = STATE_CODE;
				break;
			case STATE_STR:
				if (*i == '\\' && *(i + 1) == '\'')
				{
					if (remove_not_code)
						*i = ' ';
					i++;
					break;
				}
				if (*i == '\'')
					s = STATE_CODE;
				break;
			case STATE_ONE_LINE_COMMENT:
				if (*i == '\n')
					s = STATE_CODE;
				break;
			case STATE_COMMENT:
				if (*i == '*' && *(i + 1) == '/')
				{
					if (remove_not_code)
						*i = ' ';
					i++;
					if (remove_not_code)
						*i = ' ';
					s = STATE_CODE;
				}
				break;
			default:
				g_assert_not_reached ();
		}
		if (s != STATE_CODE && remove_not_code)
			*i = ' ';
	}
	return s != STATE_CODE;
}

gchar*
get_gir_path ()
{
	JSLang* plugin = (JSLang*)getPlugin ();

	if (!plugin->prefs)
		plugin->prefs = g_settings_new ("org.gnome.anjuta.plugins.js");

	gchar *path = g_settings_get_string (plugin->prefs, GIR_DIR_KEY);
	if (!path || strlen (path) < 1)
	{
		g_free (path);
		if (strlen (GIR_PATH) == 0)
			return g_strdup (".");
		return g_strdup (GIR_PATH);
	}
	return path;
}
