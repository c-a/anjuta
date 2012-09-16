/* -*- Mode: C; indent-spaces-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
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
#include <libanjuta/interfaces/ianjuta-environment.h>

#include <sys/wait.h>

struct _JHBuildPluginClass
{
    AnjutaPluginClass parent_class;
};

static gboolean
jhbuild_plugin_environment_override (IAnjutaEnvironment* environment,
                                     char** dirp,
                                     char*** argvp,
                                     char*** envp,
                                     GError** error)
{
    JHBuildPlugin* self = ANJUTA_PLUGIN_JHBUILD(environment);

    if (g_str_has_suffix (*argvp[0], "configure") || g_str_has_suffix (*argvp[0], "autogen.sh"))
    {
        gboolean add_prefix = TRUE;
        char** argv_iter;

        for (argv_iter = *argvp; *argv_iter; argv_iter++)
        {
            if (g_str_has_prefix (*argv_iter, "--prefix") ||
                g_str_has_prefix (*argv_iter, "--libdir"))
            {
                add_prefix = FALSE;
                break;
            }
        }

        if (add_prefix)
        {
            guint argvp_length;
            char** new_argv;

            argvp_length = g_strv_length (*argvp);
            new_argv = g_new(char*, argvp_length + 3);
            memcpy(new_argv, *argvp, sizeof(char*) * argvp_length);

            new_argv[argvp_length] = g_strdup_printf("--prefix=%s", self->prefix);
            new_argv[argvp_length + 1] = g_strdup_printf("--libdir=%s", self->libdir);
            new_argv[argvp_length + 2] = NULL;

            g_free(*argvp);
            *argvp = new_argv;
        }
    }

    return TRUE;
}

static char**
jhbuild_plugin_environment_get_environment_variables (IAnjutaEnvironment* environment,
                                                      GError** error)
{
    JHBuildPlugin* self = ANJUTA_PLUGIN_JHBUILD(environment);

    return self->envvars;
}

static gboolean
jhbuild_plugin_run_jhbuild_envinfo(JHBuildPlugin* self, const char* option, char** output)
{
    char* argv[] = {"jhbuild", "envinfo", (char*)option, NULL};
    gboolean res;
    GError* err = NULL;
    char* standard_output = NULL;
    char* standard_error = NULL;
    int exit_status;
    
    res = g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                       &standard_output, &standard_error, &exit_status, &err);
    if (!res)
    {
        anjuta_util_dialog_error(GTK_WINDOW(self->shell),
                                 "Failed to run jhbuild (%s)", err->message);
        goto out;
    }

    if (WIFEXITED(exit_status) && WEXITSTATUS(exit_status) != 0)
    {
        anjuta_util_dialog_error(GTK_WINDOW(self->shell),
                                 "Failed to run \"jhbuild envinfo --prefix\" (%s)", standard_error);
        goto out;
    }

    *output = standard_output;
    standard_output = NULL;
    res = TRUE;

out:
    g_clear_error(&err);
    g_free(standard_output);
    g_free(standard_error);

    return res;
}

static char*
jhbuild_plugin_get_prefix(JHBuildPlugin* self)
{
    char* output;
    const char* prefix_end;
    char* prefix;

    if (!jhbuild_plugin_run_jhbuild_envinfo (self, "--prefix", &output))
        return NULL;

    if ((prefix_end = strchr(output, '\n')))
    {
        prefix = g_strndup(output, prefix_end - output);
        g_free(output);
    }
    else
        prefix = output;

    return prefix;
}

static char*
jhbuild_plugin_get_libdir(JHBuildPlugin* self)
{
    char* output;
    const char* libdir_end;
    char* libdir;

    if (!jhbuild_plugin_run_jhbuild_envinfo (self, "--libdir", &output))
        return NULL;

    if ((libdir_end = strchr(output, '\n')))
    {
        libdir = g_strndup(output, libdir_end - output);
        g_free(output);
    }
    else
        libdir = output;
    
    return libdir;
}

static char**
jhbuild_plugin_get_environment_variables(JHBuildPlugin* self)
{
    char* output;
    char** env_variables;
    char** variable;
    GPtrArray* env_array;

    if (!jhbuild_plugin_run_jhbuild_envinfo (self, "--env", &output))
        return NULL;

    env_array = g_ptr_array_new();
    env_variables = g_strsplit(output, "\n", 0);
    for (variable = env_variables; *variable; ++variable)
    {
        char *split = strchr(*variable, '=');
        if (split && split != *variable)
            g_ptr_array_add(env_array, g_strdup(*variable));
    }
    g_ptr_array_add (env_array, NULL);
    
    g_free(output);
    g_free(env_variables);
    
    return (char**)g_ptr_array_free(env_array, FALSE);
}

static gboolean
jhbuild_plugin_activate (AnjutaPlugin* plugin)
{
    JHBuildPlugin *self = ANJUTA_PLUGIN_JHBUILD (plugin);

    self->envvars = jhbuild_plugin_get_environment_variables(self);
    if (!self->envvars)
        return FALSE;

    self->prefix = jhbuild_plugin_get_prefix(self);
    if (!self->prefix)
        return FALSE;

    self->libdir = jhbuild_plugin_get_libdir(self);
    if (!self->libdir)
        return FALSE;

    return TRUE;
}

static gboolean
jhbuild_plugin_deactivate (AnjutaPlugin *plugin)
{
    return TRUE;
}

/* GObject functions
 *---------------------------------------------------------------------------*/

/* Used in dispose and finalize */
static gpointer parent_class;

static void
jhbuild_plugin_instance_init (GObject *obj)
{
    JHBuildPlugin *self = ANJUTA_PLUGIN_JHBUILD (obj);

    self->shell = anjuta_plugin_get_shell (ANJUTA_PLUGIN(self));
}

static void
jhbuild_plugin_finalize (GObject *obj)
{
    JHBuildPlugin *self = ANJUTA_PLUGIN_JHBUILD (obj);

    g_strfreev (self->envvars);
    g_free(self->prefix);
    g_free(self->libdir);
    

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
    iface->get_environment_variables = jhbuild_plugin_environment_get_environment_variables;
}

ANJUTA_PLUGIN_BEGIN (JHBuildPlugin, jhbuild_plugin);
ANJUTA_PLUGIN_ADD_INTERFACE (ienvironment, IANJUTA_TYPE_ENVIRONMENT);
ANJUTA_PLUGIN_END;

ANJUTA_SIMPLE_PLUGIN (JHBuildPlugin, jhbuild_plugin);
