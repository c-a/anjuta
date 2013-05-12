/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta-session.c
 * Copyright (c) 2005 Naba Kumar  <naba@gnome.org>
 *
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

/**
 * SECTION:anjuta-session
 * @short_description: Store local user settings
 * @see_also:
 * @stability: Unstable
 * @include: libanjuta/anjuta-session.h
 *
 * A anjuta session contains local user settings, by example the list of files
 * open in one project. These settings are stored in
 * a .ini-like config file in a directory named session. Other libraries can
 * store their own settings in another format in the same directory.
 */

#include <stdlib.h>
#include <string.h>

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

#include "resources.h"
#include "anjuta-utils.h"

#include "anjuta-session.h"

G_DEFINE_TYPE (AnjutaSession, anjuta_session, G_TYPE_OBJECT);

struct _AnjutaSessionPriv {
	gchar *dir_path;

	GSettingsSchemaSource *schema_source;
	GSettingsBackend *key_file_backend;
};

static gpointer *parent_class = NULL;

enum {
	PROP_0, PROP_SESSION_DIRECTORY, N_PROPERTIES
};
static GParamSpec *properties[N_PROPERTIES];

static void
anjuta_session_constructed (GObject *object)
{
	AnjutaSession *session = ANJUTA_SESSION (object);
	gchar *filename;

	filename = anjuta_session_get_session_filename (session);
	session->priv->key_file_backend = g_keyfile_settings_backend_new (filename,
	                                                                  "/", NULL);

	g_free (filename);
}

static void
anjuta_session_get_property (GObject *object, guint property_id, GValue *value,
                             GParamSpec *pspec)
{
	AnjutaSession *session = ANJUTA_SESSION (object);

	switch (property_id)
	{
		case PROP_SESSION_DIRECTORY:
			g_value_set_string (value, session->priv->dir_path);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
anjuta_session_set_property (GObject *object, guint property_id,
                             const GValue *value, GParamSpec *pspec)
{
	AnjutaSession *session = ANJUTA_SESSION (object);

	switch (property_id)
	{
		case PROP_SESSION_DIRECTORY:
			session->priv->dir_path = g_value_dup_string (value);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
anjuta_session_finalize (GObject *object)
{
	AnjutaSession *cobj;
	cobj = ANJUTA_SESSION (object);

	g_free (cobj->priv->dir_path);

	g_settings_schema_source_unref (cobj->priv->schema_source);
	g_clear_object (&cobj->priv->key_file_backend);

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
anjuta_session_class_init (AnjutaSessionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->constructed = anjuta_session_constructed;
	object_class->get_property = anjuta_session_get_property;
	object_class->set_property = anjuta_session_set_property;
	object_class->finalize = anjuta_session_finalize;

	properties[PROP_SESSION_DIRECTORY] =
		g_param_spec_string ("session-directory", "Session directory",
		                     "Session directory", NULL,
		                     G_PARAM_READABLE | G_PARAM_WRITABLE |
		                     G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	g_type_class_add_private (klass, sizeof (AnjutaSessionPriv));
}

static void
anjuta_session_init (AnjutaSession *obj)
{
	gchar *data_dir, *schema_dir;

	obj->priv = G_TYPE_INSTANCE_GET_PRIVATE (obj, ANJUTA_TYPE_SESSION,
	                                         AnjutaSessionPriv);

	obj->priv->dir_path = NULL;

	data_dir = anjuta_res_get_data_dir ();
	schema_dir = g_build_filename (data_dir, "session", NULL);

	obj->priv->schema_source =
		g_settings_schema_source_new_from_directory (schema_dir, NULL, TRUE, NULL);
	g_assert (obj->priv->schema_source != NULL);

	g_free (data_dir);
	g_free (schema_dir);
}

/**
 * anjuta_session_new:
 * @session_directory: Directory where session is loaded from/saved to.
 *
 * Created a new session object. @session_directory is the directory
 * where session information will be stored or loaded in case of existing
 * session.
 *
 * Returns: an #AnjutaSession Object
 */
AnjutaSession*
anjuta_session_new (const gchar *session_directory)
{
	g_return_val_if_fail (session_directory != NULL, NULL);
	g_return_val_if_fail (g_path_is_absolute (session_directory), NULL);

	return g_object_new (ANJUTA_TYPE_SESSION,
	                     "session-directory", session_directory,
	                     NULL);
}

/**
 * anjuta_session_get_session_directory:
 * @session: an #AnjutaSession object
 *
 * Returns the directory corresponding to this session object.
 *
 * Returns: session directory
 */
const gchar*
anjuta_session_get_session_directory (AnjutaSession *session)
{
	return session->priv->dir_path;
}

/**
 * anjuta_session_get_session_filename:
 * @session: an #AnjutaSession object
 *
 * Gets the session filename corresponding to this session object.
 *
 * Returns: session (absolute) filename
 */
gchar*
anjuta_session_get_session_filename (AnjutaSession *session)
{
	g_return_val_if_fail (ANJUTA_IS_SESSION (session), NULL);

	return g_build_filename (session->priv->dir_path,
							 "anjuta.session", NULL);
}

/**
 * anjuta_session_sync:
 * @session: an #AnjutaSession object
 *
 * Synchronizes session object with session file
 */
void
anjuta_session_sync (AnjutaSession *session)
{
	g_return_if_fail (ANJUTA_IS_SESSION (session));

	/* FIXME: Need to sync key_file_backend explicitly. */
}

/**
 * anjuta_session_clear:
 * @session: an #AnjutaSession object
 *
 * Clears the session.
 */
void
anjuta_session_clear (AnjutaSession *session)
{
	gchar *cmd;
	gchar *quoted;
	gchar *filename;

	g_return_if_fail (ANJUTA_IS_SESSION (session));

	g_clear_object (&session->priv->key_file_backend);

	quoted = g_shell_quote (session->priv->dir_path);
	cmd = g_strconcat ("rm -fr ", quoted, NULL);
	system (cmd);
	g_free (cmd);

	cmd = g_strconcat ("mkdir -p ", quoted, NULL);
	system (cmd);
	g_free (cmd);
	g_free (quoted);

	filename = anjuta_session_get_session_filename (session);
	session->priv->key_file_backend = g_keyfile_settings_backend_new (filename,
	                                                                  "/", NULL);
	g_free (filename);
}

/**
 * anjuta_session_create_settings:
 * @session: an #AnjutaSession object
 * @schema_id: the id of the schema
 * 
 * Returns: (transfer full): a new #GSettings object
 */
GSettings*
anjuta_session_create_settings (AnjutaSession* session, const gchar *schema_id)
{
	GSettingsSchema *schema;
	GSettings *settings;

	g_return_val_if_fail (ANJUTA_IS_SESSION (session), NULL);
	g_return_val_if_fail (schema_id != NULL, NULL);

	schema = g_settings_schema_source_lookup (session->priv->schema_source,
	                                          schema_id, FALSE);
	g_assert (schema != NULL);

	settings = g_settings_new_full (schema, session->priv->key_file_backend, NULL);
	g_settings_schema_unref (schema);
	return settings;
}

/**
 * anjuta_session_get_relative_uri_from_file:
 * @session: an #AnjutaSession object
 * @file: a GFile
 * @fragment: an optional fragment
 *
 * Return an URI relative to the session directory file with an optional
 * fragment.
 * It is useful to keep only relative file paths in a session file to be able
 * to copy the whole project without breaking references.
 *
 * Returns: (transfer full): A string that has to be freed with g_free().
 */
gchar *
anjuta_session_get_relative_uri_from_file (AnjutaSession *session,
                                           GFile *file,
                                           const gchar *fragment)
{
	GFile *parent;
	gchar *uri;
	gint level;

	parent = g_file_new_for_path (session->priv->dir_path);
	for (level = 0; (parent != NULL) && !g_file_has_prefix (file, parent); level++)
	{
		GFile *next = g_file_get_parent (parent);
		g_object_unref (parent);
		parent = next;
	}

	if (parent == NULL)
	{
		uri = g_file_get_uri (file);
	}
	else
	{
		gchar *path;

		path = g_file_get_relative_path (parent, file);
		uri = g_uri_escape_string (path, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
		g_free (path);
		if (level != 0)
		{
			gsize len;
			gchar *buffer;
			gchar *ptr;

			len = strlen (uri);
			buffer = g_new (gchar, len + level * 3 + 1);
			for (ptr = buffer; level; level--)
			{
				memcpy (ptr, ".." G_DIR_SEPARATOR_S, 3);
				ptr += 3;
			}
			memcpy (ptr, uri, len + 1);
			g_free (uri);

			uri = buffer;
		}
	}

	if (fragment != NULL)
	{
		gchar *with_fragment;

		with_fragment = g_strconcat (uri, "#", fragment, NULL);
		g_free (uri);
		uri = with_fragment;
	}

	return uri;
}


/**
 * anjuta_session_get_file_from_relative_uri:
 * @session: an #AnjutaSession object
 * @uri: a relative URI from a key
 * @fragment: (allow-none): fragment part of the URI if existing, can be %NULL
 *
 * Return a GFile corresponding to the URI and and optional fragment,
 * normally read from a session key.
 * The path is expected to be relative to the session directory but it works
 * with an absolute URI, in this case it returns the same file than
 * g_file_new_for_uri.
 * It is useful to keep only relative file paths in a session file to be able
 * to copy the whole project without breaking references.
 *
 * Returns: (transfer full): A new GFile that has to be freed with g_object_unref().
 */
GFile*
anjuta_session_get_file_from_relative_uri (AnjutaSession *session,
                                           const gchar *uri,
                                           const gchar **fragment)
{
	GFile *file;
	gchar *scheme;

	scheme =g_uri_parse_scheme (uri);
	if (scheme != NULL)
	{
		free (scheme);
		file = g_file_new_for_uri (uri);
	}
	else
	{
		gchar *parent_uri = g_filename_to_uri (session->priv->dir_path, NULL, NULL);
		gchar *full_uri;

		full_uri = g_strconcat (parent_uri, G_DIR_SEPARATOR_S, uri, NULL);
		file = g_file_new_for_uri (full_uri);
		g_free (full_uri);
		g_free (parent_uri);
	}
	if (fragment != NULL)
	{
		*fragment = strchr (uri, '#');
		if (*fragment != NULL) (*fragment)++;
	}

	return file;
}

