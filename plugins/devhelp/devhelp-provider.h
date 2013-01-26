/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * devhelp-provider.h
 * Copyright (C) 2013 Carl-Anton Ingmarsson <carlantoni@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * anjuta is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _DEVHELP_PROVIDER_H_
#define _DEVHELP_PROVIDER_H_

#include <glib-object.h>

#include <libanjuta/interfaces/ianjuta-editor-assist.h>

#include "plugin.h"

G_BEGIN_DECLS

#define DEVHELP_TYPE_PROVIDER             (devhelp_provider_get_type ())
#define DEVHELP_PROVIDER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), DEVHELP_TYPE_PROVIDER, DevhelpProvider))
#define DEVHELP_PROVIDER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), DEVHELP_TYPE_PROVIDER, DevhelpProviderClass))
#define DEVHELP_IS_PROVIDER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DEVHELP_TYPE_PROVIDER))
#define DEVHELP_IS_PROVIDER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), DEVHELP_TYPE_PROVIDER))
#define DEVHELP_PROVIDER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), DEVHELP_TYPE_PROVIDER, DevhelpProviderClass))

typedef struct _DevhelpProviderClass DevhelpProviderClass;
typedef struct _DevhelpProvider DevhelpProvider;
typedef struct _DevhelpProviderPrivate DevhelpProviderPrivate;

struct _DevhelpProviderClass
{
    GObjectClass parent_class;
};

struct _DevhelpProvider
{
    GObject parent_instance;

    DevhelpProviderPrivate* priv;
};

GType devhelp_provider_get_type (void) G_GNUC_CONST;

gboolean
devhelp_provider_supports_language     (const gchar* language);

DevhelpProvider*
devhelp_provider_new                   (AnjutaDevhelp*       devhelp,
                                        DhBookManager*       book_manager,
                                        GSettings*           settings,
                                        IAnjutaEditorAssist* assist);

void
devhelp_provider_set_language          (DevhelpProvider* self,
                                        const gchar* language);

G_END_DECLS

#endif /* _DEVHELP_PROVIDER_H_ */

