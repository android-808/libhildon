/*
 * This file is a part of hildon
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version. or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "hildon-touch-selector.h"
#include "hildon-touch-selector-entry.h"

G_DEFINE_TYPE (HildonTouchSelectorEntry, hildon_touch_selector_entry, HILDON_TYPE_TOUCH_SELECTOR)

#define HILDON_TOUCH_SELECTOR_ENTRY_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), HILDON_TYPE_TOUCH_SELECTOR_ENTRY, HildonTouchSelectorEntryPrivate))

typedef struct _HildonTouchSelectorEntryPrivate HildonTouchSelectorEntryPrivate;

static void entry_on_text_changed (GtkEditable * editable, gpointer userdata);
static void hildon_touch_selector_entry_changed (HildonTouchSelector * selector,
                                                 gint column,
                                                 gpointer user_data);
static void hildon_touch_selector_entry_set_model (HildonTouchSelector * selector,
                                                   gint column, GtkTreeModel *model);
static gboolean hildon_touch_selector_entry_has_multiple_selection (HildonTouchSelector * selector);

struct _HildonTouchSelectorEntryPrivate {
  gint text_column;
  gulong signal_id;
  GtkWidget *entry;
};

enum {
  PROP_TEXT_COLUMN = 1
};

static void
hildon_touch_selector_entry_get_property (GObject *object, guint property_id,
                                          GValue *value, GParamSpec *pspec)
{
  switch (property_id) {
  case PROP_TEXT_COLUMN:
    g_value_set_int (value,
                     hildon_touch_selector_entry_get_text_column (HILDON_TOUCH_SELECTOR_ENTRY (object)));
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
hildon_touch_selector_entry_set_property (GObject *object, guint property_id,
                                          const GValue *value, GParamSpec *pspec)
{
  switch (property_id) {
  case PROP_TEXT_COLUMN:
    hildon_touch_selector_entry_set_text_column (HILDON_TOUCH_SELECTOR_ENTRY (object),
                                                 g_value_get_int (value));
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
hildon_touch_selector_entry_class_init (HildonTouchSelectorEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HildonTouchSelectorClass *selector_class = HILDON_TOUCH_SELECTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HildonTouchSelectorEntryPrivate));

  selector_class->set_model = hildon_touch_selector_entry_set_model;
  selector_class->has_multiple_selection = hildon_touch_selector_entry_has_multiple_selection;

  object_class->get_property = hildon_touch_selector_entry_get_property;
  object_class->set_property = hildon_touch_selector_entry_set_property;

  g_object_class_install_property (G_OBJECT_CLASS (klass),
                                   PROP_TEXT_COLUMN,
                                   g_param_spec_int ("text-column",
                                                     "Text Column",
                                                     "A column in the data source model to get the strings from.",
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_READWRITE));
}

static gchar *
hildon_touch_selector_entry_print_func (HildonTouchSelector * selector)
{
  HildonTouchSelectorEntryPrivate *priv;

  priv = HILDON_TOUCH_SELECTOR_ENTRY_GET_PRIVATE (selector);

  return g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->entry)));
}

static void
hildon_touch_selector_entry_init (HildonTouchSelectorEntry *self)
{
  HildonTouchSelectorEntryPrivate *priv;
  GtkEntryCompletion *completion;

  priv = HILDON_TOUCH_SELECTOR_ENTRY_GET_PRIVATE (self);

  priv->entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (priv->entry), TRUE);

  completion = gtk_entry_completion_new ();
  gtk_entry_completion_set_inline_completion (completion, TRUE);
  gtk_entry_completion_set_popup_completion (completion, FALSE);
  gtk_entry_set_completion (GTK_ENTRY (priv->entry), completion);

  gtk_widget_show (priv->entry);
  g_signal_connect (G_OBJECT (priv->entry), "changed",
                    G_CALLBACK (entry_on_text_changed), self);
  priv->signal_id = g_signal_connect (G_OBJECT (self), "changed",
                                      G_CALLBACK (hildon_touch_selector_entry_changed), NULL);

  hildon_touch_selector_set_print_func (HILDON_TOUCH_SELECTOR (self), hildon_touch_selector_entry_print_func);
  gtk_box_pack_start (GTK_BOX (self), priv->entry, FALSE, FALSE, 0);
}

GtkWidget *
hildon_touch_selector_entry_new (void)
{
  return g_object_new (HILDON_TYPE_TOUCH_SELECTOR_ENTRY, NULL);
}

/**
 * hildon_touch_selector_entry_new_text:
 * @void:
 *
 * Creates a #HildonTouchSelectorEntry with a single text column that
 * can be populated conveniently through hildon_touch_selector_append_text(),
 * hildon_touch_selector_prepend_text(), hildon_touch_selector_insert_text().
 *
 * Returns: A new #HildonTouchSelectorEntry
 **/
GtkWidget *
hildon_touch_selector_entry_new_text (void)
{
  GtkListStore *model;
  GtkWidget *selector;
  GtkEntryCompletion *completion;
  HildonTouchSelectorEntryPrivate *priv;

  selector = hildon_touch_selector_entry_new ();

  priv = HILDON_TOUCH_SELECTOR_ENTRY_GET_PRIVATE (selector);

  model = gtk_list_store_new (1, G_TYPE_STRING);
  completion = gtk_entry_get_completion (GTK_ENTRY (priv->entry));
  gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (model));

  hildon_touch_selector_append_text_column (HILDON_TOUCH_SELECTOR (selector),
                                            GTK_TREE_MODEL (model), FALSE);
  hildon_touch_selector_entry_set_text_column (HILDON_TOUCH_SELECTOR_ENTRY (selector), 0);

  return selector;
}

void
hildon_touch_selector_entry_set_text_column (HildonTouchSelectorEntry *selector,
                                             gint text_column)
{
  HildonTouchSelectorEntryPrivate *priv;
  GtkEntryCompletion *completion;

  g_return_if_fail (HILDON_IS_TOUCH_SELECTOR_ENTRY (selector));
  g_return_if_fail (text_column >= -1);

  priv = HILDON_TOUCH_SELECTOR_ENTRY_GET_PRIVATE (selector);
  completion = gtk_entry_get_completion (GTK_ENTRY (priv->entry));

  gtk_entry_completion_set_text_column (completion, text_column);
  priv->text_column = text_column;
}

gint
hildon_touch_selector_entry_get_text_column (HildonTouchSelectorEntry *selector)
{
  HildonTouchSelectorEntryPrivate *priv;

  g_return_val_if_fail (HILDON_IS_TOUCH_SELECTOR_ENTRY (selector), -1);

  priv = HILDON_TOUCH_SELECTOR_ENTRY_GET_PRIVATE (selector);

  return priv->text_column;
}

static void
entry_on_text_changed (GtkEditable * editable,
                       gpointer userdata)
{
  HildonTouchSelector *selector;
  HildonTouchSelectorEntryPrivate *priv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkEntry *entry;
  const gchar *prefix;
  gchar *text;
  gboolean found = FALSE;

  entry = GTK_ENTRY (editable);
  selector = HILDON_TOUCH_SELECTOR (userdata);
  priv = HILDON_TOUCH_SELECTOR_ENTRY_GET_PRIVATE (selector);

  prefix = gtk_entry_get_text (entry);
  model = hildon_touch_selector_get_model (HILDON_TOUCH_SELECTOR (selector), 0);

  if (!gtk_tree_model_get_iter_first (model, &iter)) {
    return;
  }

  do {
    gtk_tree_model_get (model, &iter, priv->text_column, &text, -1);
    found = g_str_has_prefix (text, prefix);
    g_free (text);
  } while (found != TRUE && gtk_tree_model_iter_next (model, &iter));

  g_signal_handler_block (selector, priv->signal_id);
  {
    /* We emit the HildonTouchSelector::changed signal because a change in the
       GtkEntry represents a change in current selection, and therefore, users
       should be notified. */
    if (found) {
      hildon_touch_selector_set_active_iter (selector, 0, &iter, TRUE);
    }
    g_signal_emit_by_name (selector, "changed", 0);
  }
  g_signal_handler_unblock (selector, priv->signal_id);

}

/* FIXME: This is actually a very ugly way to retrieve the text. Ideally,
   we would have API to retrieve it from the base clase (HildonTouchSelector).
   In the meantime, leaving it here.
 */
static gchar *
hildon_touch_selector_get_text_from_model (HildonTouchSelectorEntry * selector)
{
  HildonTouchSelectorEntryPrivate *priv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;
  GList *selected_rows;
  gchar *text;

  priv = HILDON_TOUCH_SELECTOR_ENTRY_GET_PRIVATE (selector);

  model = hildon_touch_selector_get_model (HILDON_TOUCH_SELECTOR (selector), 0);
  selected_rows = hildon_touch_selector_get_selected_rows (HILDON_TOUCH_SELECTOR (selector), 0);

  if (selected_rows == NULL) {
    return NULL;
  }

  /* We are in single selection mode */
  g_assert (selected_rows->next == NULL);

  path = (GtkTreePath *)selected_rows->data;
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, priv->text_column, &text, -1);

  gtk_tree_path_free (path);
  g_list_free (selected_rows);

  return text;
}

static void
hildon_touch_selector_entry_changed (HildonTouchSelector * selector,
                                     gint column, gpointer user_data)
{
  HildonTouchSelectorEntryPrivate *priv;
  gchar *text;

  priv = HILDON_TOUCH_SELECTOR_ENTRY_GET_PRIVATE (selector);

  text = hildon_touch_selector_get_text_from_model (HILDON_TOUCH_SELECTOR_ENTRY (selector));
  gtk_entry_set_text (GTK_ENTRY (priv->entry), text);
  gtk_editable_select_region (GTK_EDITABLE (priv->entry), 0, -1);
  g_free (text);
}

static void
hildon_touch_selector_entry_set_model (HildonTouchSelector * selector,
                                       gint column, GtkTreeModel *model)
{
  GtkEntryCompletion *completion;
  HildonTouchSelectorEntryPrivate *priv;

  g_return_if_fail (HILDON_IS_TOUCH_SELECTOR_ENTRY (selector));
  g_return_if_fail (column == 0);
  g_return_if_fail (GTK_IS_TREE_MODEL (model));

  HILDON_TOUCH_SELECTOR_CLASS (hildon_touch_selector_entry_parent_class)->set_model (selector, column, model);

  priv = HILDON_TOUCH_SELECTOR_ENTRY_GET_PRIVATE (selector);

  completion = gtk_entry_get_completion (GTK_ENTRY (priv->entry));
  gtk_entry_completion_set_model (completion, model);
  gtk_entry_completion_set_text_column (completion, priv->text_column);
}

static gboolean
hildon_touch_selector_entry_has_multiple_selection (HildonTouchSelector * selector)
{
  /* Always TRUE, given the GtkEntry. */
  return TRUE;
}