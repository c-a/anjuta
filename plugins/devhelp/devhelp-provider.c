/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * devhelp-provider.c
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

#include <devhelp/devhelp.h>

#include <libanjuta/resources.h>
#include <libanjuta/interfaces/ianjuta-language-provider.h>
#include <libanjuta/anjuta-language-provider.h>

#include "devhelp-provider.h"


#define WORD_CHARACTERS "_0"

struct _DevhelpProviderPrivate
{
    DhAssistantView* assistant_view;
    AnjutaLanguageProvider* lang_prov;
    GdkPixbuf* icon;

    GList* proposals;

    /* Properties */
    AnjutaDevhelp* devhelp;
    DhBookManager* book_manager;
    GSettings* settings;
    IAnjutaEditorAssist* assist;
    gchar* language;
};

enum
{
    PROP_0,
    PROP_DEVHELP,
    PROP_BOOK_MANAGER,
    PROP_SETTINGS,
    PROP_ASSIST,
    PROP_LANGUAGE,
    N_PROPERTIES
};

static GParamSpec* properties[N_PROPERTIES] = { NULL, };

static void devhelp_provider_provider_interface_init (IAnjutaProviderIface* iface);
static void devhelp_provider_language_provider_interface_init (IAnjutaLanguageProviderIface* iface);

G_DEFINE_TYPE_WITH_CODE (DevhelpProvider, devhelp_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IANJUTA_TYPE_PROVIDER,
                                                devhelp_provider_provider_interface_init)
                         G_IMPLEMENT_INTERFACE (IANJUTA_TYPE_LANGUAGE_PROVIDER,
                                                devhelp_provider_language_provider_interface_init))


static IAnjutaSymbolType
get_symbol_type_from_link_type (DhLinkType link_type)
{
    switch (link_type)
    {
        case DH_LINK_TYPE_FUNCTION:
            return IANJUTA_SYMBOL_TYPE_FUNCTION;
        case DH_LINK_TYPE_STRUCT:
            return IANJUTA_SYMBOL_TYPE_STRUCT;
        case DH_LINK_TYPE_MACRO:
            return IANJUTA_SYMBOL_TYPE_MACRO;
        case DH_LINK_TYPE_ENUM:
            return IANJUTA_SYMBOL_TYPE_ENUM;
        case DH_LINK_TYPE_TYPEDEF:
            return IANJUTA_SYMBOL_TYPE_TYPEDEF;
        default:
            g_assert_not_reached ();
    }
}

static gboolean
supported_link_type (DhLinkType link_type)
{
    switch (link_type)
    {
        case DH_LINK_TYPE_FUNCTION:
        case DH_LINK_TYPE_STRUCT:
        case DH_LINK_TYPE_MACRO:
        case DH_LINK_TYPE_ENUM:
        case DH_LINK_TYPE_TYPEDEF:
            return TRUE;
        default:
            return FALSE;
    }
}

/**
 * devhelp_provider_proposal_new:
 * @symbol: IAnjutaSymbol to create the proposal for
 * Creates a new IAnjutaEditorAssistProposal for DhLink
 * Returns: a newly allocated IAnjutaEditorAssistProposal
 */
static IAnjutaEditorAssistProposal*
devhelp_provider_new_proposal (DevhelpProvider* self, DhLink* link)
{
    IAnjutaEditorAssistProposal* proposal = g_new0 (IAnjutaEditorAssistProposal, 1);
    AnjutaLanguageProposalData* data =
        anjuta_language_proposal_data_new (g_strdup (dh_link_get_name (link)));

    data->type = get_symbol_type_from_link_type (dh_link_get_link_type (link));
    switch (data->type)
    {
        case IANJUTA_SYMBOL_TYPE_FUNCTION:
            proposal->label = g_strdup_printf ("%s()", data->name);
            data->is_func = TRUE;
            break;

        default:
            proposal->label = g_strdup (data->name);
            data->is_func = FALSE;
    }

    data->has_para = FALSE;
    data->user_data = dh_link_ref (link);
    data->user_data_destroy_func = (GDestroyNotify)dh_link_unref;

    proposal->data = data;
    proposal->icon = self->priv->icon;
    return proposal;
}

static void
devhelp_provider_proposal_free (IAnjutaEditorAssistProposal* proposal)
{
    anjuta_language_proposal_data_free (proposal->data);
    g_free (proposal->label);

    g_free (proposal);
}

static int
devhelp_provider_proposal_sort_func (IAnjutaEditorAssistProposal* p1,
                                     IAnjutaEditorAssistProposal* p2)
{
    return g_strcmp0 (p1->text, p2->text);
}

static void
devhelp_provider_clear_proposals (DevhelpProvider* self)
{
    g_list_free_full (self->priv->proposals,
                      (GDestroyNotify)devhelp_provider_proposal_free);
    self->priv->proposals = NULL;
}

static void
devhelp_provider_update_proposals (DevhelpProvider* self, const char* pre_word)
{
    int i;
    GList* b;
    gboolean case_sensitive;
    GList* completion_books;
    GList* proposals = NULL, *tenth;

    g_return_if_fail (self->priv->proposals == NULL);

    /* Matching case sensitive when any uppercase
     * letter is used in the search terms, matching vim
     * smartcase behaviour.
     */
    case_sensitive = FALSE;
    for (i = 0; pre_word[i] != '\0'; i++) {
        if (g_ascii_isupper (pre_word[i])) {
            case_sensitive = TRUE;
            break;
        }
    }

    completion_books = anjuta_devhelp_get_autocomplete_books (self->priv->devhelp);

    for (b = dh_book_manager_get_books (self->priv->book_manager);
         b;
         b = b->next)
    {
        DhBook* book;
        GList* l;
        
        book = b->data;

        /* Filter by books */
        if (!g_list_find_custom (completion_books, dh_book_get_name (book),
                                (GCompareFunc)g_strcmp0))
            continue;

        /* Filter by language */
        if (self->priv->language &&
            !g_strcmp0 (self->priv->language, dh_book_get_language (book)))
            continue;

        for (l = dh_book_get_keywords (book);
             l;
             l = l->next)
        {
            DhLink* link;
            char* name;

            link = l->data;
            if (!supported_link_type (dh_link_get_link_type (link)))
                continue;

            name = (case_sensitive ?
                    g_strdup (dh_link_get_name (link)) :
                    g_ascii_strdown (dh_link_get_name (link), -1));

            if (g_str_has_prefix (name, pre_word))
            {
                IAnjutaEditorAssistProposal* proposal;
                proposal = devhelp_provider_new_proposal (self, link);
                proposals = g_list_prepend (proposals, proposal);
            }
            g_free (name);
        }
    }

    proposals = g_list_sort (proposals,
                             (GCompareFunc)devhelp_provider_proposal_sort_func);
    /* Limit proposals to ten since GtkSourceCompletion has some performance
     * issues. */
    if ((tenth = g_list_nth (proposals, 9)))
    {
        g_list_free_full (tenth->next,
                          (GDestroyNotify)devhelp_provider_proposal_free);
        tenth->next = NULL;
    }

    self->priv->proposals = proposals;
}

static IAnjutaIterable*
devhelp_provider_populate_completions (IAnjutaLanguageProvider* ilangprov,
                                       IAnjutaIterable* cursor,
                                       GError** e)
{
    DevhelpProvider* self = DEVHELP_PROVIDER (ilangprov);

    char* pre_word;
    IAnjutaIterable* start_iter = NULL;

    devhelp_provider_clear_proposals (self);

    pre_word = anjuta_language_provider_get_pre_word (self->priv->lang_prov,
                                                      IANJUTA_EDITOR (self->priv->assist),
                                                      cursor, &start_iter,
                                                      WORD_CHARACTERS);
    if (pre_word)
    {
        devhelp_provider_update_proposals (self, pre_word);
        ianjuta_editor_assist_proposals (self->priv->assist, IANJUTA_PROVIDER (self),
                                         self->priv->proposals, pre_word, TRUE, NULL);
        g_free (pre_word);
    }
    return start_iter;
}

static char*
devhelp_provider_get_calltip_context (IAnjutaLanguageProvider *ilangprov,
                                      IAnjutaIterable *iter,
                                      GError** error)
{
    return NULL;
}

static GList*
devhelp_provider_get_calltip_cache (IAnjutaLanguageProvider* ilangprov,
                                    gchar* call_context,
                                    GError** error)
{
    return NULL;
}

static void
devhelp_provider_new_calltip (IAnjutaLanguageProvider* ilangprov,
                              gchar* call_context,
                              IAnjutaIterable* cursor,
                              GError** error)
{
}

static GtkWidget*
devhelp_provider_get_info_widget (IAnjutaProvider* iprov, gpointer data,
                                  GError** error)
{
    DevhelpProvider* self = DEVHELP_PROVIDER (iprov);

    AnjutaLanguageProposalData* prop_data;
    DhLink* link;

    prop_data = data;
    link = prop_data->user_data;
    dh_assistant_view_set_link (self->priv->assistant_view, link);

    return GTK_WIDGET (self->priv->assistant_view);
}

static IAnjutaIterable* 
devhelp_provider_get_start_iter (IAnjutaProvider* iprov, GError** err)
{
    DevhelpProvider* self = DEVHELP_PROVIDER (iprov);
    return anjuta_language_provider_get_start_iter (self->priv->lang_prov);
}

static void
devhelp_provider_populate (IAnjutaProvider* iprov, IAnjutaIterable* iter,
                           GError** error)
{
    DevhelpProvider* self = DEVHELP_PROVIDER (iprov);

    anjuta_language_provider_populate (self->priv->lang_prov, iprov, iter);
}

static void
devhelp_provider_activate (IAnjutaProvider* iprov, IAnjutaIterable* iter, 
                           gpointer data, GError** error)
{
    DevhelpProvider* self = DEVHELP_PROVIDER (iprov);

    anjuta_language_provider_activate (self->priv->lang_prov, iprov, iter, data);
}

static const gchar*
devhelp_provider_get_name (IAnjutaProvider* provider, GError** error)
{
    return "Devhelp";
}

void
devhelp_provider_set_language (DevhelpProvider* self, const gchar* language)
{
    g_return_if_fail (DEVHELP_IS_PROVIDER (self));

    g_free (self->priv->language);
    self->priv->language = g_strdup (language);
}

static void
devhelp_provider_set_property (GObject* object,
                               guint property_id,
                               const GValue* value,
                               GParamSpec* pspec)
{
    DevhelpProvider* self = DEVHELP_PROVIDER (object);

    switch (property_id)
    {
        case PROP_DEVHELP:
            self->priv->devhelp = g_value_dup_object (value);
            break;

        case PROP_BOOK_MANAGER:
            self->priv->book_manager = g_value_dup_object (value);
            break;

        case PROP_SETTINGS:
            self->priv->settings = g_value_dup_object (value);
            break;

        case PROP_ASSIST:
            self->priv->assist = g_value_dup_object (value);
            break;

        case PROP_LANGUAGE:
            self->priv->language = g_value_dup_string (value);
            break;

        default:
            g_assert_not_reached ();
    }
}

static void
devhelp_provider_finalize (GObject* object)
{
    DevhelpProvider* self = DEVHELP_PROVIDER (object);
    
    if (self->priv->assist)
    {
        ianjuta_editor_assist_remove (self->priv->assist, IANJUTA_PROVIDER (self), NULL);
        g_clear_object (&self->priv->assist);
    }
    devhelp_provider_clear_proposals (self);

    g_clear_object (&self->priv->lang_prov);
    g_clear_object (&self->priv->assistant_view);
    g_clear_object (&self->priv->icon);

    g_clear_object (&self->priv->devhelp);
    g_clear_object (&self->priv->book_manager);
    g_clear_object (&self->priv->settings);
    g_clear_object (&self->priv->assist);

    G_OBJECT_CLASS (devhelp_provider_parent_class)->finalize (object);
}

static void
devhelp_provider_constructed (GObject* object)
{
    DevhelpProvider* self = DEVHELP_PROVIDER (object);

    gchar* icon_filename;

    icon_filename = anjuta_res_get_pixmap_file ("anjuta-devhelp-view-16.png");
    self->priv->icon = gdk_pixbuf_new_from_file (icon_filename, NULL);
    g_free (icon_filename);

    self->priv->assistant_view = DH_ASSISTANT_VIEW (dh_assistant_view_new ());
    g_object_ref_sink (G_OBJECT (self->priv->assistant_view));
    gtk_widget_set_size_request (GTK_WIDGET(self->priv->assistant_view),
                                 500, 300);
    gtk_widget_show (GTK_WIDGET (self->priv->assistant_view));

    self->priv->lang_prov = g_object_new (ANJUTA_TYPE_LANGUAGE_PROVIDER, NULL);
    anjuta_language_provider_install (self->priv->lang_prov,
                                      IANJUTA_EDITOR (self->priv->assist),
                                      self->priv->settings);
}

static void
devhelp_provider_init (DevhelpProvider* self)
{
    DevhelpProviderPrivate* priv;

    self->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                                     DEVHELP_TYPE_PROVIDER,
                                                     DevhelpProviderPrivate);
}

static void
devhelp_provider_class_init (DevhelpProviderClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (DevhelpProviderPrivate));

    object_class->constructed = devhelp_provider_constructed;
    object_class->finalize = devhelp_provider_finalize;
    object_class->set_property = devhelp_provider_set_property;

    properties[PROP_DEVHELP] =
        g_param_spec_object ("devhelp", "AnjutaDevhelp", "Devhelp plugin",
                             ANJUTA_TYPE_PLUGIN_DEVHELP,
                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    properties[PROP_BOOK_MANAGER] =
        g_param_spec_object ("book-manager", "DhBookManager", "Book Manager",
                             DH_TYPE_BOOK_MANAGER,
                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    properties[PROP_SETTINGS] = 
        g_param_spec_object ("settings", "GSettings", "Settings",
                             G_TYPE_SETTINGS,
                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    properties[PROP_ASSIST] = 
        g_param_spec_object ("assist", "IAnjutaEditorAssist", "The IAnjutaEditorAssist",
                             IANJUTA_TYPE_EDITOR_ASSIST,
                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    properties[PROP_LANGUAGE] = 
        g_param_spec_string ("language", "Language", "Language",
                             NULL,
                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
devhelp_provider_provider_interface_init (IAnjutaProviderIface* iface)
{
    iface->get_name = devhelp_provider_get_name;
    iface->activate = devhelp_provider_activate;
    iface->populate = devhelp_provider_populate;
    iface->get_start_iter = devhelp_provider_get_start_iter;
    iface->get_info_widget = devhelp_provider_get_info_widget;
}

static void
devhelp_provider_language_provider_interface_init (IAnjutaLanguageProviderIface* iface)
{
    iface->get_calltip_cache   = devhelp_provider_get_calltip_cache;
    iface->get_calltip_context = devhelp_provider_get_calltip_context;
    iface->new_calltip         = devhelp_provider_new_calltip;
    iface->populate_completions   = devhelp_provider_populate_completions;
}

DevhelpProvider*
devhelp_provider_new (AnjutaDevhelp*       devhelp,
                      DhBookManager*       book_manager,
                      GSettings*           settings,
                      IAnjutaEditorAssist* assist)
{
    return g_object_new (DEVHELP_TYPE_PROVIDER,
                         "devhelp", devhelp,
                         "book-manager", book_manager,
                         "settings", settings,
                         "assist", assist,
                         NULL);
}

gboolean
devhelp_provider_supports_language (const gchar* language)
{
    return (language &&
            (g_str_equal (language, "C") ||
             g_str_equal (language, "C++") ||
             g_str_equal (language, "Python")));
}