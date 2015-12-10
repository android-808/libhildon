/*
 * This file is a part of hildon examples
 *
 * Copyright (C) 2005, 2006 Nokia Corporation, all rights reserved.
 *
 * Author: Michael Dominic Kostrzewa <michael.kostrzewa@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include                                        <stdio.h>
#include                                        <stdlib.h>
#include                                        <glib.h>
#include                                        <glib/gi18n.h>
#include                                        <gtk/gtk.h>
#include                                        <hildon/hildon.h>

static GtkWidget*
create_button_with_icon                         (const gchar *icon);

static GtkWidget*
create_button_with_icon                         (const gchar *icon)
{
    GtkImage *image = GTK_IMAGE (gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_BUTTON));
    if (image == NULL) {
        g_warning ("Failed to create a GtkImage from icon name: %s", icon);
        return NULL;
    }

//    gtk_misc_set_padding (GTK_MISC (image), 12, 12);
    g_object_set (image, "margin", 12, NULL);

    GtkButton *button = GTK_BUTTON (gtk_button_new ());

    gtk_container_add (GTK_CONTAINER (button), GTK_WIDGET (image));

    return (GtkWidget *) button;
}

int
main                                            (int argc,
                                                 char **argv)
{
    hildon_gtk_init (&argc, &argv);

    GtkDialog *dialog = GTK_DIALOG (gtk_dialog_new ());
    gtk_window_set_title (GTK_WINDOW (dialog), "icons");

    GtkGrid *grid = GTK_GRID (gtk_grid_new ());
    // TODO: GTK2 Table was set to use homogeneous cells but in GTK3 this results in grid spanning entire width.
    //gtk_grid_set_column_homogeneous(grid, TRUE);
    gtk_grid_set_row_homogeneous(grid, TRUE);
    
    gtk_grid_attach (grid, create_button_with_icon ("gtk-ok"), 0, 0, 1, 1);
    gtk_grid_attach (grid, create_button_with_icon ("gtk-cancel"), 1, 0, 1, 1);
    gtk_grid_attach (grid, create_button_with_icon ("window-close"), 2, 0, 1, 1);
    
    gtk_grid_attach (grid, create_button_with_icon ("document-save"), 0, 1, 1, 1);
    gtk_grid_attach (grid, create_button_with_icon ("media-playback-pause"), 1, 1, 1, 1);
    gtk_grid_attach (grid, create_button_with_icon ("text-x-generic"), 2, 1, 1, 1);
    
    gtk_grid_set_column_spacing (grid, 6);
    gtk_grid_set_row_spacing (grid, 6);

    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (dialog)), GTK_WIDGET (grid), FALSE, FALSE, 0);


    gtk_dialog_add_button (dialog, "Close", GTK_RESPONSE_CLOSE);

    gtk_widget_show_all (GTK_WIDGET (dialog));
    gtk_dialog_run (dialog);
    
    return 0;
}


