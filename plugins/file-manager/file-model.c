/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta
 * Copyright (C) Johannes Schmid 2007 <jhs@gnome.org>
 * 
 * anjuta is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * anjuta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with anjuta.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include "file-model.h"
#include <glib/gi18n.h>
#include <string.h>
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/anjuta-plugin.h>
#include <libanjuta/anjuta-async-notify.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

const gchar* BINARY_SUFFIX[] =
{
	".o",
	".lo",
	".a",
	".so",
	".pyc",
	".pyo",
	NULL
};

#define ICON_SIZE 16

enum
{
	PROP_0,
	PROP_BASE_PATH,
	PROP_FILTER_BINARY,
	PROP_FILTER_HIDDEN,
	PROP_FILTER_BACKUP,
	PROP_FILTER_UNVERSIONED
};

enum
{
	SIGNAL_DIRECTORY_EXPANDED,
	SIGNAL_LAST
};

guint signals [SIGNAL_LAST];

typedef struct _FileModelPrivate FileModelPrivate;
typedef struct _FileModelAsyncData FileModelAsyncData;

struct _FileModelPrivate
{
	GFile *base_path;
	gboolean filter_binary;
	gboolean filter_hidden;
	gboolean filter_backup;
	gboolean filter_unversioned;
	
	GtkTreeView* view;

	IAnjutaVcs *ivcs;
};

struct _FileModelAsyncData
{
	FileModel* model;
	GtkTreeRowReference* reference;
};

#define FILE_MODEL_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE((o), FILE_TYPE_MODEL, FileModelPrivate))

G_DEFINE_TYPE (FileModel, file_model, GTK_TYPE_TREE_STORE)

static gboolean
file_model_filter_file (FileModel* model,
						GFileInfo* file_info)
{
	FileModelPrivate* priv = FILE_MODEL_GET_PRIVATE (model);
	
	if (priv->filter_hidden && g_file_info_get_is_hidden(file_info))
		return FALSE;
	else if (priv->filter_backup && g_file_info_get_is_backup(file_info))
		return FALSE;
	else if (priv->filter_binary && 
			 g_file_info_get_file_type (file_info) != G_FILE_TYPE_DIRECTORY)
	{
		int i;
		const gchar* name = g_file_info_get_name (file_info);
		for (i = 0; BINARY_SUFFIX[i] != NULL; i++)
		{
			if (g_str_has_suffix (name, BINARY_SUFFIX[i]))
			{
				return FALSE;
			}
		}
	}
	
	return TRUE;
}

static void
file_model_add_dummy (FileModel* model,
					  GtkTreeIter* iter)
{
	GtkTreeStore* store = GTK_TREE_STORE (model);
	GtkTreeIter dummy;
	
	gtk_tree_store_append (store, &dummy, iter);
	gtk_tree_store_set (store, &dummy, 
					    COLUMN_FILENAME, _("Loading..."),
						COLUMN_SORT, -1,
	                    COLUMN_DUMMY, TRUE,
					    -1);
}

typedef struct
{
	FileModel* model;
	GtkTreeRowReference* ref;
} VcsData;

static void
file_model_vcs_status_callback(GFile *file,
							   AnjutaVcsStatus status,
							   gpointer user_data)
{
	VcsData* data = user_data;
	FileModelPrivate *priv = FILE_MODEL_GET_PRIVATE (data->model);
	gchar* path = g_file_get_path (file);
	
	GtkTreePath* tree_path = gtk_tree_row_reference_get_path (data->ref);
	if (tree_path)
	{
		GFile* dir;
		GFile* parent;
		GtkTreeIter iter;
		GtkTreeIter child;
		GtkTreeModel* model = gtk_tree_row_reference_get_model (data->ref);

		gtk_tree_model_get_iter (model,
								 &iter,
								 tree_path);
		gtk_tree_model_get (model, &iter,
		                    COLUMN_FILE, &dir, -1);

		parent = g_file_get_parent (file);
		
		if (gtk_tree_model_iter_children (model, &child, &iter) &&
		    g_file_equal (dir, parent))
		{
			do
			{
				GFile* child_file;
				gboolean dummy;
				gtk_tree_model_get (model, &child,
				                    COLUMN_FILE,
				                    &child_file,
				                    COLUMN_DUMMY,
				                    &dummy, -1);
				if (dummy)
					break;
				if (file && child_file && g_file_equal (file, child_file))
				{
					if (priv->filter_unversioned &&
					    (status == ANJUTA_VCS_STATUS_UNVERSIONED ||
					     status == ANJUTA_VCS_STATUS_IGNORED))
					{
						gtk_tree_store_remove (GTK_TREE_STORE (model), &child);
					}
					else
					{
						gtk_tree_store_set (GTK_TREE_STORE (model),
						                    &child,
						                    COLUMN_STATUS,
						                    status,
						                    -1);
					}
					g_object_unref (child_file);
					break;
				}
			}
			while (gtk_tree_model_iter_next (model, &child));
		}
		gtk_tree_path_free (tree_path);
		g_object_unref (dir);
		g_object_unref (parent);
	}
	g_free(path);
}

static void
file_model_free_vcs_data (VcsData *data)
{
	gtk_tree_row_reference_free (data->ref);
	g_free (data);
}

static void
file_model_get_vcs_status (FileModel* model,
						   GtkTreeIter* iter,
						   GFile* file)
{	
	GtkTreePath* path = gtk_tree_model_get_path (GTK_TREE_MODEL(model),
												 iter);
	FileModelPrivate* priv = FILE_MODEL_GET_PRIVATE(model);	
	
	if (priv->ivcs)
	{	
		VcsData* data = g_new0(VcsData, 1);
		AnjutaAsyncNotify* notify = anjuta_async_notify_new();
		data->ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (model),
												path);
		data->model = model;
		
		g_signal_connect_swapped (G_OBJECT (notify), "finished", 
								  G_CALLBACK (file_model_free_vcs_data), data);

		ianjuta_vcs_query_status(priv->ivcs,
								 file,
								 file_model_vcs_status_callback,
								 data,
								 NULL,
								 notify,
								 NULL);
	}						 
	gtk_tree_path_free (path);	
}

static void
file_model_update_file (FileModel* model,
						GtkTreeIter* iter,
						GFile* file,
						GFileInfo* file_info,
                        gboolean add)
{
	GtkTreeStore* store = GTK_TREE_STORE(model);
	gboolean is_dir = FALSE;
	gchar** icon_names;
	GtkIconInfo* icon_info;
	GIcon* icon;
	GdkPixbuf* pixbuf = NULL;
	gchar* display_name;
	
	icon = g_file_info_get_icon(file_info);
	if (icon)
	{
		g_object_get (icon, "names", &icon_names, NULL);
		
		if ((icon_info = gtk_icon_theme_choose_icon (gtk_icon_theme_get_default(),
													 (const gchar **)icon_names,
													 ICON_SIZE,
													 GTK_ICON_LOOKUP_FORCE_SIZE)))
		{
			pixbuf = gtk_icon_info_load_icon (icon_info, NULL);
			gtk_icon_info_free(icon_info);
		}
		g_strfreev (icon_names);
 	}
	
	if (g_file_info_get_file_type(file_info) == G_FILE_TYPE_DIRECTORY)
		is_dir = TRUE;
	
	display_name = g_markup_printf_escaped("%s", 
										   g_file_info_get_display_name(file_info));
	
	gtk_tree_store_set (store, iter,
						COLUMN_DISPLAY, display_name,
						COLUMN_FILENAME, display_name,
						COLUMN_FILE, file,
						COLUMN_PIXBUF, pixbuf,
						COLUMN_STATUS, ANJUTA_VCS_STATUS_NONE,
						COLUMN_IS_DIR, is_dir,
						COLUMN_SORT, g_file_info_get_sort_order(file_info),
	                    COLUMN_DUMMY, FALSE,
						-1);
	
	if (is_dir)
	{
		file_model_add_dummy(model, iter);
	}
	if (!is_dir && !add)
	{
		GtkTreeIter parent;
		GFile* dir;
		gtk_tree_model_iter_parent (GTK_TREE_MODEL (model), &parent, iter);
		gtk_tree_model_get (GTK_TREE_MODEL (model), &parent,
		                    COLUMN_FILE, &dir, -1);
		file_model_get_vcs_status (model, &parent, dir);
		g_object_unref (dir);
	}
	
	if (pixbuf)
		g_object_unref (pixbuf);
	g_free(display_name);
}

static gboolean
file_model_update_file_foreach_func (GtkTreeModel* model,
                                     GtkTreePath* path,
                                     GtkTreeIter* iter,
                                     gpointer user_data)
{
	GFile* file;
	GFileInfo* info;
	
	gtk_tree_model_get (model, iter,
	                    COLUMN_FILE, &file, -1);

	if (!file)
		return FALSE;
	
	info = g_file_query_info (file,
	                          "standard::*",
	                          G_FILE_QUERY_INFO_NONE,
	                          NULL, NULL);

	if (!info)
	{
		g_object_unref (file);
		return FALSE;
	}
	
	file_model_update_file (FILE_MODEL (model), 
	                        iter,
	                        file,
	                        info,
	                        FALSE);
	g_object_unref (info);
	g_object_unref (file);
	
	/* Continue iterating */
	return FALSE;
}

static void
file_model_add_file (FileModel* model,
					 GtkTreeIter* parent,
					 GFile* file,
					 GFileInfo* file_info)
{
	GtkTreeIter iter;
	GtkTreeStore* store = GTK_TREE_STORE(model);

	if (file_model_filter_file (model, file_info))
	{
		gtk_tree_store_append (store, &iter, parent);
		file_model_update_file (model, &iter, file, file_info, TRUE);
	}
}

void file_model_update_vcs_status (FileModel* model)
{
    gtk_tree_model_foreach (GTK_TREE_MODEL(model), 
                            file_model_update_file_foreach_func, NULL);
}

static void
on_file_model_changed (GFileMonitor* monitor,
					   GFile* file,
					   GFile* other_file,
					   GFileMonitorEvent event_type,
					   gpointer data)
{
	GtkTreeRowReference* reference = (GtkTreeRowReference*)data;
	FileModel* model;
	GtkTreeIter iter;
	GtkTreePath* path;
	GtkTreeIter file_iter;
	gboolean found = FALSE;
	
	/* reference could be invalid if the file has already been destroyed */
	if (!gtk_tree_row_reference_valid(reference))
		return;
	
	model = FILE_MODEL(gtk_tree_row_reference_get_model (reference));
	path = gtk_tree_row_reference_get_path (reference);
	
	gtk_tree_model_get_iter (GTK_TREE_MODEL(model), &iter, path);
	gtk_tree_path_free (path);

	if (gtk_tree_model_iter_children (GTK_TREE_MODEL(model), &file_iter, &iter))
	{
		do
		{
			GFile* model_file;
			gtk_tree_model_get (GTK_TREE_MODEL(model), &file_iter,
								COLUMN_FILE, &model_file, -1);
			if (model_file && file && g_file_equal (model_file, file))
			{
				g_object_unref (model_file);
				found = TRUE;
				break;
			}
			g_clear_object (&model_file);
		}
		while (gtk_tree_model_iter_next (GTK_TREE_MODEL(model), &file_iter));
	}
	if (event_type == G_FILE_MONITOR_EVENT_CHANGED ||
	    event_type == G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED ||
		event_type == G_FILE_MONITOR_EVENT_DELETED)
	{
		if (!found)
			return;
	}
	
	switch (event_type)
	{
		case G_FILE_MONITOR_EVENT_CHANGED:
		case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
		case G_FILE_MONITOR_EVENT_CREATED:
		{
			GFileInfo* file_info;
			file_info = g_file_query_info (file,
										   "standard::*",
										   G_FILE_QUERY_INFO_NONE,
										   NULL, NULL);
			if (file_info)
			{
				if (!found)
					file_model_add_file (model, &iter, file, file_info);
				else
					file_model_update_file (model, &file_iter, file, file_info, FALSE);
				g_object_unref (file_info);
			}
			break;
		}
		case G_FILE_MONITOR_EVENT_DELETED:
		{
			gtk_tree_store_remove (GTK_TREE_STORE (model), &file_iter);			
			break;
		}
		default:
			/* do nothing */
			break;
	}
}

static void
file_model_add_watch (FileModel* model, GtkTreePath* path)
{
	GtkTreeIter iter;
	GtkTreeRowReference* reference;
	GFile* file;
	GFileMonitor* monitor;
	
	gtk_tree_model_get_iter (GTK_TREE_MODEL (model),
							 &iter, path);
	
	gtk_tree_model_get (GTK_TREE_MODEL(model), &iter,
						COLUMN_FILE, &file, -1);
	
	reference = gtk_tree_row_reference_new (GTK_TREE_MODEL(model), path);
	
	monitor = g_file_monitor_directory (file, G_FILE_MONITOR_NONE,
										NULL, NULL);
	g_signal_connect (monitor, "changed", G_CALLBACK(on_file_model_changed),
					  reference);
	
	g_object_set_data_full (G_OBJECT(file), "file-monitor", monitor, (GDestroyNotify)g_object_unref);
	/* Reference is used by monitor, should be kept it until the monitor is destroyed */
	g_object_set_data_full (G_OBJECT(monitor), "reference", reference, (GDestroyNotify)gtk_tree_row_reference_free);
	g_object_unref (file);
}

static void
on_row_expanded_async (GObject* source_object,
					   GAsyncResult* result,
					   gpointer user_data)
{
	FileModelAsyncData* data = user_data;
	GFile* dir = G_FILE (source_object);
	GFileEnumerator* files;
	GError* err = NULL;
	GtkTreeIter real_iter;
	GtkTreeIter dummy;
	GtkTreeRowReference* ref = data->reference;
	GtkTreePath* path;
	FileModel* model = data->model;
	GFileInfo* file_info;
	
	files = g_file_enumerate_children_finish (dir, result, &err);	
	path = gtk_tree_row_reference_get_path (ref);
	
	if (!path)
	{
		gtk_tree_row_reference_free (ref);
		g_object_unref (files);
		return;
	}

	if (err)
	{
		DEBUG_PRINT ("GIO-Error: %s", err->message);
		g_error_free (err);
		// TODO: Collapse row
		return;
	}

	gtk_tree_model_get_iter (GTK_TREE_MODEL(data->model), &real_iter, path);
	
	while (files && (file_info = g_file_enumerator_next_file (files, NULL, NULL)))
	{
		GFile* file = g_file_get_child (dir, g_file_info_get_name(file_info));
		file_model_add_file (data->model, &real_iter, file, file_info);
		g_object_unref (file);
		g_object_unref (file_info);
	}
	/* Remove dummy node */
	gtk_tree_model_iter_children (GTK_TREE_MODEL(model), &dummy, &real_iter);
	gtk_tree_store_remove (GTK_TREE_STORE(model), &dummy);

	file_model_add_watch (model, path);
	file_model_get_vcs_status (model, &real_iter, dir);

	g_signal_emit (model, signals[SIGNAL_DIRECTORY_EXPANDED], 0, &real_iter, path);

	gtk_tree_path_free (path);
	gtk_tree_row_reference_free (ref);
	g_object_unref(files);
}

static void
file_model_row_expanded (GtkTreeView* tree_view, GtkTreeIter* iter,
					    GtkTreePath* path, gpointer user_data)
{
	GtkTreeModel* sort_model = gtk_tree_view_get_model(tree_view);
	FileModel* model = FILE_MODEL(user_data);
	GFile* dir;
	GtkTreeIter real_iter;
	GCancellable* cancel = g_cancellable_new ();
	GtkTreePath* real_path;
	
	DEBUG_PRINT ("%s", "row_expanded");
	
	gtk_tree_model_sort_convert_iter_to_child_iter(GTK_TREE_MODEL_SORT(sort_model),
												   &real_iter, iter);
	
	gtk_tree_model_get(GTK_TREE_MODEL(model), &real_iter,
					   COLUMN_FILE, &dir, -1);
	
	FileModelAsyncData* data = g_new0 (FileModelAsyncData, 1);
	data->model = model;
	real_path = gtk_tree_model_get_path (GTK_TREE_MODEL (model),
									&real_iter);
	data->reference = gtk_tree_row_reference_new (GTK_TREE_MODEL (model),
												  real_path);
	gtk_tree_path_free (real_path);
	
	g_object_set_data (G_OBJECT(dir), "_cancel", cancel);
	
	g_file_enumerate_children_async (dir,
									 "standard::*",
									 G_FILE_QUERY_INFO_NONE,
									 G_PRIORITY_LOW,
									 cancel,
									 on_row_expanded_async,
									 data);
	g_object_unref (dir);
}

static void
file_model_row_collapsed (GtkTreeView* tree_view, GtkTreeIter* iter,
						 GtkTreePath* path, gpointer data)
{
	GtkTreeModel* sort_model = gtk_tree_view_get_model(tree_view);
	FileModel* model = FILE_MODEL(data);
	GtkTreeIter child;
	GtkTreeIter sort_iter;
	GtkTreeIter real_iter;
	GFile* dir;
	GCancellable* cancel;
	
	/* Iter might be invalid in some conditions */
	gtk_tree_model_get_iter (sort_model, &sort_iter, path);
	
	gtk_tree_model_sort_convert_iter_to_child_iter(GTK_TREE_MODEL_SORT(sort_model),
												   &real_iter, &sort_iter);
	
	gtk_tree_model_get (GTK_TREE_MODEL (model), &real_iter, 
						COLUMN_FILE, &dir, -1);

	cancel = g_object_get_data (G_OBJECT(dir), "_cancel");
	g_cancellable_cancel (cancel);
	g_object_unref (cancel);
	g_object_unref (dir);
	
	while (gtk_tree_model_iter_children (GTK_TREE_MODEL(model), &child, &real_iter))
	{
		gtk_tree_store_remove (GTK_TREE_STORE (model), &child);		
	}
	
	file_model_add_dummy (model, &real_iter);
}

static void
file_model_init (FileModel *object)
{

}

static void
file_model_finalize (GObject *object)
{
	FileModel *model = FILE_MODEL(object);
	FileModelPrivate* priv = FILE_MODEL_GET_PRIVATE(model);

	g_clear_object (&priv->base_path);

	G_OBJECT_CLASS (file_model_parent_class)->finalize (object);
}

static void
file_model_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	g_return_if_fail (FILE_IS_MODEL (object));
	FileModel* model = FILE_MODEL(object);
	FileModelPrivate* priv = FILE_MODEL_GET_PRIVATE(model);
	
	switch (prop_id)
	{
	case PROP_BASE_PATH:
		g_clear_object (&priv->base_path);
		priv->base_path = g_value_dup_object (value);
		if (!priv->base_path)
			priv->base_path = g_file_new_for_uri ("file:///");
		break;
	case PROP_FILTER_BINARY:
		priv->filter_binary = g_value_get_boolean (value);
		break;
	case PROP_FILTER_HIDDEN:
		priv->filter_hidden = g_value_get_boolean (value);
		break;
	case PROP_FILTER_BACKUP:
		priv->filter_backup = g_value_get_boolean (value);
		break;
	case PROP_FILTER_UNVERSIONED:
		priv->filter_unversioned = g_value_get_boolean (value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
file_model_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	g_return_if_fail (FILE_IS_MODEL (object));
	FileModel* model = FILE_MODEL(object);
	FileModelPrivate* priv = FILE_MODEL_GET_PRIVATE(model);
	
	switch (prop_id)
	{
	case PROP_BASE_PATH:
		g_value_set_object (value, priv->base_path);
		break;
	case PROP_FILTER_BINARY:
		g_value_set_boolean (value, priv->filter_binary);
	case PROP_FILTER_HIDDEN:
		g_value_set_boolean (value, priv->filter_hidden);
	case PROP_FILTER_BACKUP:
		g_value_set_boolean (value, priv->filter_backup);
	case PROP_FILTER_UNVERSIONED:
		g_value_set_boolean (value, priv->filter_unversioned);
		
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}


static void
file_model_class_init (FileModelClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = file_model_finalize;
	object_class->set_property = file_model_set_property;
	object_class->get_property = file_model_get_property;

	g_type_class_add_private (object_class, sizeof(FileModelPrivate));
	
	g_object_class_install_property (object_class,
	                                 PROP_BASE_PATH,
									 g_param_spec_object ("base-path",
														  _("Base Path"),
														  _("GFile representing the top-most path displayed"),
														  G_TYPE_FILE,
														  G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
									 PROP_FILTER_BINARY,
									 g_param_spec_boolean ("filter_binary",
														   "Filter binary files",
														   "file_binary",
														   TRUE,
														   G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));
	
	g_object_class_install_property (object_class,
									 PROP_FILTER_HIDDEN,
									 g_param_spec_boolean ("filter_hidden",
														   "Filter hidden files",
														   "file_hidden",
														   TRUE,
														   G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));
	
	g_object_class_install_property (object_class,
									 PROP_FILTER_BACKUP,
									 g_param_spec_boolean ("filter_backup",
														   "Filter backup files",
														   "file_backup",
														   TRUE,
														   G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));
	
	g_object_class_install_property (object_class,
									 PROP_FILTER_UNVERSIONED,
									 g_param_spec_boolean ("filter_unversioned",
														   "Filter unversioned files",
														   "file_unversioned",
														   TRUE,
														   G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

	signals[SIGNAL_DIRECTORY_EXPANDED] =
		g_signal_new ("directory-expanded",
					  G_TYPE_FROM_CLASS (object_class),
					  G_SIGNAL_RUN_FIRST,
					  0, NULL, NULL, NULL,
					  G_TYPE_NONE, 2, GTK_TYPE_TREE_ITER, GTK_TYPE_TREE_PATH);
	
}

FileModel*
file_model_new (GtkTreeView* tree_view, GFile* base_path)
{
	GObject* model =
		g_object_new (FILE_TYPE_MODEL, "base-path", base_path, NULL);
	GType types[N_COLUMNS] = {GDK_TYPE_PIXBUF, G_TYPE_STRING,
		G_TYPE_STRING, G_TYPE_UINT, G_TYPE_OBJECT,
		G_TYPE_BOOLEAN, G_TYPE_INT, G_TYPE_BOOLEAN};
	FileModelPrivate* priv = FILE_MODEL_GET_PRIVATE(model);
	
	g_signal_connect (G_OBJECT (tree_view), "row-collapsed", 
					  G_CALLBACK (file_model_row_collapsed), model);
	g_signal_connect (G_OBJECT (tree_view), "row-expanded", 
					  G_CALLBACK (file_model_row_expanded), model);
	
	gtk_tree_store_set_column_types (GTK_TREE_STORE (model), N_COLUMNS,
									 types);
	
	priv->view = tree_view;
	
	return FILE_MODEL(model);
}

void
file_model_refresh (FileModel* model)
{
	GtkTreeStore* store = GTK_TREE_STORE (model);
	FileModelPrivate* priv = FILE_MODEL_GET_PRIVATE(model);
	GFileInfo* base_info;
	
	gtk_tree_store_clear (store);
	
	base_info = g_file_query_info (priv->base_path, "standard::*",
								   G_FILE_QUERY_INFO_NONE, NULL, NULL);
	
	if (!base_info)
		return;
 	
 	file_model_add_file (model, NULL, priv->base_path, base_info);
	g_object_unref (base_info);
}

GFile*
file_model_get_file (FileModel* model, GtkTreeIter* iter)
{
	GFile* file;
	
	gtk_tree_model_get (GTK_TREE_MODEL (model), iter, COLUMN_FILE, &file, -1);
	
	return file;
}

gchar*
file_model_get_filename (FileModel* model, GtkTreeIter* iter)
{
	gchar* filename;
	gtk_tree_model_get (GTK_TREE_MODEL (model), iter, COLUMN_FILENAME, &filename, -1);
	
	return filename;
}

void
file_model_set_ivcs (FileModel* model, IAnjutaVcs *ivcs)
{
	FileModelPrivate *priv = FILE_MODEL_GET_PRIVATE (model);

	if (priv->ivcs)
		g_object_remove_weak_pointer (G_OBJECT (priv->ivcs), (void**)&priv->ivcs);

	priv->ivcs = ivcs;
	if (priv->ivcs)
		g_object_add_weak_pointer (G_OBJECT (priv->ivcs), (void**)&priv->ivcs);
}
