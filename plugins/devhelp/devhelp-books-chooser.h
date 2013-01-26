/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * devhelp-book-chooser.h
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

#ifndef _DEVHELP_BOOKS_CHOOSER_H_
#define _DEVHELP_BOOKS_CHOOSER_H_

#include <devhelp/devhelp.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define DEVHELP_TYPE_BOOKS_CHOOSER             (devhelp_books_chooser_get_type ())
#define DEVHELP_BOOKS_CHOOSER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), DEVHELP_TYPE_BOOKS_CHOOSER, DevhelpBooksChooser))
#define DEVHELP_BOOKS_CHOOSER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), DEVHELP_TYPE_BOOKS_CHOOSER, DevhelpBooksChooserClass))
#define DEVHELP_IS_BOOKS_CHOOSER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DEVHELP_TYPE_BOOKS_CHOOSER))
#define DEVHELP_IS_BOOKS_CHOOSER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), DEVHELP_TYPE_BOOKS_CHOOSER))
#define DEVHELP_BOOKS_CHOOSER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), DEVHELP_TYPE_BOOKS_CHOOSER, DevhelpBooksChooserClass))

typedef struct _DevhelpBooksChooserClass DevhelpBooksChooserClass;
typedef struct _DevhelpBooksChooser DevhelpBooksChooser;
typedef struct _DevhelpBooksChooserPrivate DevhelpBooksChooserPrivate;


struct _DevhelpBooksChooserClass
{
    GtkBinClass parent_class;
};

struct _DevhelpBooksChooser
{
    GtkBin parent_instance;

    DevhelpBooksChooserPrivate* priv;
};

GType devhelp_books_chooser_get_type (void) G_GNUC_CONST;

DevhelpBooksChooser*
devhelp_books_chooser_new              (DhBookManager* book_manager);

void
devhelp_books_chooser_set_active_books (DevhelpBooksChooser* self,
                                        GList* active_books);
const GList*
devhelp_books_chooser_get_active_books (DevhelpBooksChooser* self);

G_END_DECLS

#endif /* _DEVHELP_BOOKS_CHOOSER_H_ */

