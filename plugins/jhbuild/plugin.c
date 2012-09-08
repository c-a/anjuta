/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    plugin.c
    Copyright (C) 2012 Carl-Anton Ingmarsson

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

#include "plugin.h"

#include <libanjuta/anjuta-debug.h>
#include <libanjuta/interfaces/ianjuta-project-manager.h>

#include <signal.h>

#define UI_FILE PACKAGE_DATA_DIR"/ui/anjuta-quick-open.xml"

struct _JHBuildPluginClass
{
	AnjutaPluginClass parent_class;
};

static gboolean
jhbuild_plugin_environment_override (IAnjutaEnvironment *environment,
                                     gchar **dirp,
                                     gchar ***argvp,
                                     gchar ***envp,
                                     GError **err)
{
}

static gboolean
jhbuild_plugin_activate (AnjutaPlugin *plugin)
{
	JHBuildPlugin *self = ANJUTA_PLUGIN_jhbuild (plugin);

	const gchar *path;
	gchar **paths, **iter;
	GFile *file;

	path = g_getenv ("PATH");
	if (!path)
		return FALSE;

	paths = g_strsplit (path, ":", 0);
	for (iter = paths; *iter; iter++)
	{
		file = g_file_new_for_path (*iter);
		if (g_file_query_exists (file, NULL))
			break;
		else
			g_clear_object (&file);
		
	}
	g_strfreev (paths);

	if (!file)
		return FALSE;

	plugin->jhbuild_path = file;

	return TRUE;
}

static gboolean
jhbuild_plugin_deactivate (AnjutaPlugin *plugin)
{
	JHBuildPlugin *self = ANJUTA_PLUGIN_jhbuild (plugin);
	
	return TRUE;
}

/* GObject functions
 *---------------------------------------------------------------------------*/

/* Used in dispose and finalize */
static gpointer parent_class;

static void
jhbuild_plugin_instance_init (GObject *obj)
{
	JHBuildPlugin *plugin = ANJUTA_PLUGIN_jhbuild (obj);
}

static void
jhbuild_plugin_finalize (GObject *obj)
{
	JHBuildPlugin *plugin = ANJUTA_PLUGIN_jhbuild (obj);

	g_clear_object (&plugin->jhbuild_path);

	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
jhbuild_plugin_class_init (GObjectClass *klass) 
{
	AnjutaPluginClass *plugin_class = ANJUTA_PLUGIN_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	plugin_class->activate = jhbuild_plugin_activate;
	plugin_class->deactivate = jhbuild_plugin_deactivate;
	klass->finalize = jhbuild_plugin_finalize;
}

static void
ienvironment_iface_init(IAnjutaEnvironmentIface *iface)
{
	iface->override = jhbuild_plugin_environment_override;
}

ANJUTA_PLUGIN_BEGIN (JHBuildPlugin, jhbuild_plugin);
ANJUTA_PLUGIN_ADD_INTERFACE (ienvironment, IANJUTA_TYPE_ENVIRONMENT);
ANJUTA_PLUGIN_END;
					 
ANJUTA_SIMPLE_PLUGIN (JHBuildPlugin, jhbuild_plugin);
