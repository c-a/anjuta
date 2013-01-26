/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * devhelp-book-chooser.c
 * Copyright (C) 2013 Carl-Anton Ingmarsson <carlantoni@gnome.org>
 * 
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * anjuta is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "devhelp-books-chooser.h"

struct _DevhelpBooksChooserPrivate
{
    /* Properties */
    DhBookManager* book_manager;

    GtkTreeView* view;
    GtkListStore* store;

    /* The books that are currently active. */
    GList* active_books;
};

enum
{
    PROP_0,
    PROP_BOOKS_MANAGER,
    N_PROPERTIES
};
static GParamSpec* properties[N_PROPERTIES] = { NULL, };

enum
{
    SIGNAL_ACTIVE_BOOKS_CHANGED,
    N_SIGNALS
};
static int signals[N_SIGNALS] = { 0 };

enum
{
    COLUMN_NAME,
    COLUMN_TITLE,
    COLUMN_ACTIVE,
    COLUMN_BOOK,
    N_COLUMNS
};

G_DEFINE_TYPE (DevhelpBooksChooser, devhelp_books_chooser, GTK_TYPE_BIN);

/**
 * devhelp_books_chooser_get_active_books:
 * @self: a #DevhelpBooksChooser
 *
 * Get the active books.
 *
 * Returns: (element-type utf8) (transfer none): The active books.
 */
const GList*
devhelp_books_chooser_get_active_books (DevhelpBooksChooser* self)
{
    g_return_val_if_fail (DEVHELP_IS_BOOKS_CHOOSER (self), NULL);

    return self->priv->active_books;
}

/**
 * devhelp_books_chooser_set_active_books:
 * @self: a #DevhelpBooksChooser
 * @active_books: (allow-none): (element-type utf8): The books that are
 * active.
 *
 * Set the active books.
 */
void
devhelp_books_chooser_set_active_books (DevhelpBooksChooser* self,
                                        GList* active_books)
{
    GtkTreeModel* model;
    GtkTreeIter iter;
    gboolean res;

    g_return_if_fail (DEVHELP_IS_BOOKS_CHOOSER (self));

    model = GTK_TREE_MODEL (self->priv->store);
    for (res = gtk_tree_model_get_iter_first (model, &iter);
         res;
         res = gtk_tree_model_iter_next (model, &iter))
    {
        char* book_name;
        gboolean active;

        gtk_tree_model_get (model, &iter, COLUMN_NAME, &book_name, -1);
        active = !!g_list_find_custom (active_books, book_name, (GCompareFunc)g_strcmp0);
        g_free (book_name);

        gtk_list_store_set (self->priv->store, &iter, COLUMN_ACTIVE, active, -1);
    }

    g_list_free_full (self->priv->active_books, g_free);
    self->priv->active_books = g_list_copy_deep (active_books,
                                                 (GCopyFunc)g_strdup, NULL);
}

static void
devhelp_books_chooser_active_toggled (GtkCellRendererToggle* cell,
                                      char* path,
                                      gpointer user_data)
{
    DevhelpBooksChooser* self = DEVHELP_BOOKS_CHOOSER (user_data);

    GtkTreeIter iter;
    char* book_name;
    gboolean active;

    if (!gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (self->priv->store),
                                              &iter, path))
        return;

    gtk_tree_model_get (GTK_TREE_MODEL (self->priv->store), &iter,
                        COLUMN_NAME, &book_name,
                        COLUMN_ACTIVE, &active, -1);

    gtk_list_store_set (self->priv->store, &iter,
                        COLUMN_ACTIVE, !active, -1);

    if (!active)
        self->priv->active_books = g_list_prepend (self->priv->active_books,
                                                   book_name);
    else
    {
        GList* del = g_list_find_custom (self->priv->active_books, book_name,
                                         (GCompareFunc)g_strcmp0);
        if (del)
        {
            g_free (del->data);
            self->priv->active_books = g_list_delete_link (self->priv->active_books,
                                                           del);
        }
        g_free (book_name);
    }

    g_signal_emit (self, signals[SIGNAL_ACTIVE_BOOKS_CHANGED], 0);
}

static gint
devhelp_books_chooser_sort_func (GtkTreeModel* model,
                                 GtkTreeIter* a, GtkTreeIter* b,
                                 gpointer user_data)
{
    char *a_name, *b_name;
    gint ret;

    gtk_tree_model_get (model, a, COLUMN_NAME, &a_name, -1);
    gtk_tree_model_get (model, b, COLUMN_NAME, &b_name, -1);

    ret = g_strcmp0 (a_name, b_name);
    g_free (a_name);
    g_free (b_name);

    return ret;
}

static void
devhelp_books_chooser_add_book (DevhelpBooksChooser* self,
                                DhBook* book)
{
    GtkTreeIter iter;

    gtk_list_store_append (self->priv->store, &iter);
    gtk_list_store_set (self->priv->store, &iter,
                        COLUMN_NAME, dh_book_get_name (book),
                        COLUMN_TITLE, dh_book_get_title (book),
                        COLUMN_ACTIVE, FALSE, 
                        COLUMN_BOOK, book,
                        -1);
}

static void
devhelp_books_chooser_book_deleted (DhBookManager* book_manager,
                                    DhBook* book, gpointer user_data)
{
    DevhelpBooksChooser* self = DEVHELP_BOOKS_CHOOSER (user_data);

    GtkTreeModel* model;
    GtkTreeIter iter;
    gboolean res;
    GList* del;

    g_return_if_fail (DEVHELP_IS_BOOKS_CHOOSER (self));

    model = GTK_TREE_MODEL (self->priv->store);
    for (res = gtk_tree_model_get_iter_first (model, &iter);
         res;
         res = gtk_tree_model_iter_next (model, &iter))
    {
        DhBook* b;

        gtk_tree_model_get (model, &iter, COLUMN_BOOK, &b, -1);
        if (b == book)
        {
            gtk_list_store_remove (self->priv->store, &iter);
            break;
        }
    }

    del = g_list_find_custom (self->priv->active_books, dh_book_get_name (book),
                              (GCompareFunc)g_strcmp0);
    if (del)
    {
        g_free (del->data);
        self->priv->active_books = g_list_delete_link (self->priv->active_books,
                                                       del);
    }
}

static void
devhelp_books_chooser_book_created (DhBookManager* book_manager,
                                    DhBook* book, gpointer user_data)
{
    DevhelpBooksChooser* self = DEVHELP_BOOKS_CHOOSER (user_data);

    devhelp_books_chooser_add_book (self, book);
}

static void
devhelp_books_chooser_fill_model (DevhelpBooksChooser* self)
{
    GList* b;

    for (b = dh_book_manager_get_books (self->priv->book_manager);
         b;
         b = b->next)
    {
        DhBook* book = b->data;

        devhelp_books_chooser_add_book (self, book);
    }
}

static void
devhelp_books_chooser_set_property (GObject* object,
                                    guint property_id,
                                    const GValue* value,
                                    GParamSpec* pspec)
{
    DevhelpBooksChooser* self = DEVHELP_BOOKS_CHOOSER (object);

    switch (property_id)
    {
        case PROP_BOOKS_MANAGER:
            self->priv->book_manager = g_value_dup_object (value);

            g_signal_connect_object (self->priv->book_manager, "book-created",
                                     G_CALLBACK (devhelp_books_chooser_book_created),
                                     self, 0);
            g_signal_connect_object (self->priv->book_manager, "book-deleted",
                                     G_CALLBACK (devhelp_books_chooser_book_deleted),
                                     self, 0);
            break;

        default:
            g_assert_not_reached ();
    }
}

static void
devhelp_books_chooser_finalize (GObject* object)
{
    G_OBJECT_CLASS (devhelp_books_chooser_parent_class)->finalize (object);
}

static void
devhelp_books_chooser_constructed (GObject* object)
{
    DevhelpBooksChooser* self = DEVHELP_BOOKS_CHOOSER (object);

    GtkTreeViewColumn* column;
    GtkCellRenderer* cell;
    GtkWidget* scrolled;

    GType column_types[N_COLUMNS] =
    {
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_BOOLEAN,
        DH_TYPE_BOOK
    };

    self->priv->store = gtk_list_store_newv (N_COLUMNS, column_types);
    devhelp_books_chooser_fill_model (self);

    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (self->priv->store),
                                     COLUMN_NAME, devhelp_books_chooser_sort_func,
                                     self, NULL);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self->priv->store),
                                          COLUMN_NAME, GTK_SORT_ASCENDING);

    self->priv->view = GTK_TREE_VIEW (gtk_tree_view_new ());
    gtk_tree_view_set_headers_visible (self->priv->view, FALSE);

    /* Active toggle */
    cell = gtk_cell_renderer_toggle_new ();
    g_signal_connect (cell, "toggled",
                      G_CALLBACK (devhelp_books_chooser_active_toggled), self);

    column = gtk_tree_view_column_new_with_attributes ("Active", cell,
                                                       "active", COLUMN_ACTIVE,
                                                       NULL);
    gtk_tree_view_append_column (self->priv->view, column);
    
    /* Name */
    cell = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Name", cell,
                                                       "text", COLUMN_NAME,
                                                       NULL);
    gtk_tree_view_append_column (self->priv->view, column);

    /* Title */
    cell = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Title", cell,
                                                       "text", COLUMN_TITLE,
                                                       NULL);
    gtk_tree_view_append_column (self->priv->view, column);

    gtk_tree_view_set_model (self->priv->view, GTK_TREE_MODEL (self->priv->store));


    scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (scrolled), GTK_WIDGET (self->priv->view));
    gtk_container_add (GTK_CONTAINER (self), scrolled);

    gtk_widget_show_all (GTK_WIDGET (self));
}

static void
devhelp_books_chooser_init (DevhelpBooksChooser *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DEVHELP_TYPE_BOOKS_CHOOSER,
                                              DevhelpBooksChooserPrivate);
}

static void
devhelp_books_chooser_class_init (DevhelpBooksChooserClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (DevhelpBooksChooserPrivate));

    object_class->constructed = devhelp_books_chooser_constructed;
    object_class->finalize = devhelp_books_chooser_finalize;
    object_class->set_property = devhelp_books_chooser_set_property;

    properties[PROP_BOOKS_MANAGER] =
        g_param_spec_object ("book-manager", "DhBookManager", "Book Manager",
                             DH_TYPE_BOOK_MANAGER,
                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    signals[SIGNAL_ACTIVE_BOOKS_CHANGED] =
        g_signal_new ("active-books-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      NULL,
                      G_TYPE_NONE,
                      0);
}

DevhelpBooksChooser*
devhelp_books_chooser_new (DhBookManager* book_manager)
{
    g_return_val_if_fail (DH_IS_BOOK_MANAGER (book_manager), NULL);

    return g_object_new (DEVHELP_TYPE_BOOKS_CHOOSER,
                         "book-manager", book_manager,
                         NULL);
}
