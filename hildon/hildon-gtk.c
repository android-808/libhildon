/*
 * This file is a part of hildon
 *
 * Copyright (C) 2008, 2009 Nokia Corporation, all rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser Public License as published by
 * the Free Software Foundation; version 2 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser Public License for more details.
 *
 */

/**
 * SECTION:hildon-gtk
 * @short_description: Additional functions for Gtk widgets
 * @see_also: #HildonButton, #HildonCheckButton
 *
 * Hildon provides some functions to extend the functionality of
 * existing Gtk widgets. This also includes convenience functions to
 * easily perform frequent tasks.
 */

#include "hildon-gtk.h"
#include "hildon-window.h"
#include "hildon-window-private.h"
#include "hildon-edit-toolbar.h"
#include "hildon-edit-toolbar-private.h"
#include "hildon-private.h"

static void
image_visible_changed_cb                        (GtkWidget  *image,
                                                 GParamSpec *arg1,
                                                 gpointer   oldparent)
{
    if (!gtk_widget_get_visible (image))
        gtk_widget_show (image);
}

static void
parent_changed_cb                               (GtkWidget  *image,
                                                 GParamSpec *arg1,
                                                 gpointer    oldparent)
{
    GtkWidget *parent = gtk_widget_get_parent (image);

    /* If the parent has really changed, remove the old signal handlers */
    if (parent != oldparent) {
        g_signal_handlers_disconnect_by_func (image, parent_changed_cb, oldparent);
        g_signal_handlers_disconnect_by_func (image, image_visible_changed_cb, NULL);
    }
}

static void
image_changed_cb                                (GtkButton  *button,
                                                 GParamSpec *arg1,
                                                 gpointer    user_data)
{
    GtkWidget *image = gtk_button_get_image (button);

    g_return_if_fail (image == NULL || GTK_IS_WIDGET (image));

    if (image != NULL) {
        GtkWidget *parent = gtk_widget_get_parent (image);

        /* If the button has a new image, show it */
        gtk_widget_show (image);
        /* Show the image no matter the value of gtk-button-images */
        g_signal_connect (image, "notify::visible", G_CALLBACK (image_visible_changed_cb), NULL);
        /* If the image is removed from the button, disconnect these handlers */
        g_signal_connect (image, "notify::parent", G_CALLBACK (parent_changed_cb), parent);
    }
}

static void
button_common_init                              (GtkWidget      *button,
                                                 HildonSizeType  size)
{
    /* Set requested size */
    hildon_gtk_widget_set_theme_size (button, size);

    /* Set focus-on-click to FALSE by default */
    gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);

    /* Make sure that all images in this button are always shown */
    g_signal_connect (button, "notify::image", G_CALLBACK (image_changed_cb), NULL);
}

/**
 * hildon_gtk_menu_new:
 *
 * This is a convenience function to create a #GtkMenu setting its
 * widget name to allow Hildon specific styling.
 *
 * Return value: A newly created #GtkMenu widget.
 *
 * Since: 2.2
 **/
GtkWidget *
hildon_gtk_menu_new                             (void)
{
    GtkWidget *menu = gtk_menu_new ();
    gtk_widget_set_name (menu, "hildon-context-sensitive-menu");
    return menu;
}

/**
 * hildon_gtk_button_new:
 * @size: Flags indicating the size of the new button
 *
 * This is a convenience function to create a #GtkButton setting its
 * size to one of the pre-defined Hildon sizes.
 *
 * Buttons created with this function also override
 * #GtkSettings:gtk-button-images. Images set using
 * gtk_button_set_image() are always shown.
 *
 * Buttons created using this function have #GtkButton:focus-on-click
 * set to %FALSE by default.
 *
 * Return value: A newly created #GtkButton widget.
 *
 * Since: 2.2
 **/
GtkWidget *
hildon_gtk_button_new                           (HildonSizeType size)
{
    GtkWidget *button = gtk_button_new ();
    button_common_init (button, size);
    return button;
}

/**
 * hildon_gtk_toggle_button_new:
 * @size: Flags indicating the size of the new button
 *
 * This is a convenience function to create a #GtkToggleButton setting
 * its size to one of the pre-defined Hildon sizes.
 *
 * Buttons created with this function also override
 * #GtkSettings:gtk-button-images. Images set using
 * gtk_button_set_image() are always shown.
 *
 * Buttons created using this function have #GtkButton:focus-on-click
 * set to %FALSE by default.
 *
 * Return value: A newly created #GtkToggleButton widget.
 *
 * Since: 2.2
 **/
GtkWidget *
hildon_gtk_toggle_button_new                    (HildonSizeType size)
{
    GtkWidget *button = gtk_toggle_button_new ();
    button_common_init (button, size);
    return button;
}

/**
 * hildon_gtk_radio_button_new:
 * @size: Flags indicating the size of the new button
 * @group: An existing radio button group, or %NULL if you are
 * creating a new group
 *
 * This is a convenience function to create a #GtkRadioButton setting
 * its size to one of the pre-defined Hildon sizes.
 *
 * Buttons created with this function also override
 * #GtkSettings:gtk-button-images. Images set using
 * gtk_button_set_image() are always shown.
 *
 * Buttons created using this function have #GtkButton:focus-on-click
 * set to %FALSE by default.
 *
 * Return value: A newly created #GtkRadioButton widget.
 *
 * Since: 2.2
 **/
GtkWidget *
hildon_gtk_radio_button_new                     (HildonSizeType  size,
                                                 GSList         *group)
{
    GtkWidget *button = gtk_radio_button_new (group);
    button_common_init (button, size);
    return button;
}

/**
 * hildon_gtk_radio_button_new_from_widget:
 * @size: Flags indicating the size of the new button
 * @radio_group_member: widget to get radio group from or %NULL
 *
 * This is a convenience function to create a #GtkRadioButton setting
 * its size to one of the pre-defined Hildon sizes.
 *
 * Buttons created with this function also override
 * #GtkSettings:gtk-button-images. Images set using
 * gtk_button_set_image() are always shown.
 *
 * Buttons created using this function have #GtkButton:focus-on-click
 * set to %FALSE by default.
 *
 * Return value: A newly created #GtkRadioButton widget.
 *
 * Since: 2.2
 **/
GtkWidget *
hildon_gtk_radio_button_new_from_widget         (HildonSizeType  size,
                                                 GtkRadioButton *radio_group_member)
{
    GtkWidget *button = gtk_radio_button_new_from_widget (radio_group_member);
    button_common_init (button, size);
    return button;
}

static void
do_set_progress_indicator                       (GtkWindow *window,
                                                 gpointer   stateptr)
{
    guint state = GPOINTER_TO_UINT (stateptr);
    hildon_gtk_window_set_clear_window_flag (window, "_HILDON_WM_WINDOW_PROGRESS_INDICATOR",
                                             XA_INTEGER, state);
    g_signal_handlers_disconnect_matched (window, G_SIGNAL_MATCH_FUNC,
                                          0, 0, NULL, do_set_progress_indicator, NULL);
}

static void
do_set_do_not_disturb                           (GtkWindow *window,
                                                 gpointer   dndptr)
{
    gboolean dndflag = GPOINTER_TO_INT (dndptr);
    hildon_gtk_window_set_clear_window_flag (window, "_HILDON_DO_NOT_DISTURB",
                                             XA_INTEGER, dndflag);
    g_signal_handlers_disconnect_matched (window, G_SIGNAL_MATCH_FUNC,
                                          0, 0, NULL, do_set_do_not_disturb, NULL);
}

static void
do_set_zoom_keys                                (GtkWindow *window,
                                                 gpointer   zoomptr)
{
    gboolean zoomflag = GPOINTER_TO_INT (zoomptr);
    hildon_gtk_window_set_clear_window_flag (window, "_HILDON_ZOOM_KEY_ATOM",
                                             XA_INTEGER, zoomflag);
    g_signal_handlers_disconnect_matched (window, G_SIGNAL_MATCH_FUNC,
                                          0, 0, NULL, do_set_zoom_keys, NULL);
}

static void
do_set_portrait_flags                           (GtkWindow *window,
                                                 gpointer   flagsptr)
{
    HildonPortraitFlags flags = GPOINTER_TO_INT (flagsptr);

    hildon_gtk_window_set_clear_window_flag (window, "_HILDON_PORTRAIT_MODE_REQUEST",
                                             XA_CARDINAL,
                                             flags & HILDON_PORTRAIT_MODE_REQUEST);
    hildon_gtk_window_set_clear_window_flag (window, "_HILDON_PORTRAIT_MODE_SUPPORT",
                                             XA_CARDINAL,
                                             flags & HILDON_PORTRAIT_MODE_SUPPORT);

    g_signal_handlers_disconnect_matched (window, G_SIGNAL_MATCH_FUNC,
                                          0, 0, NULL, do_set_portrait_flags, NULL);
}

/**
 * hildon_gtk_window_set_progress_indicator:
 * @window: a #GtkWindow.
 * @state: The state we want to set: 1 -> show progress indicator, 0
 *          -> hide progress indicator.
 *
 * This functions tells the window manager to show/hide a progress
 * indicator in the window title. It applies to #HildonDialog and
 * #HildonWindow (including subclasses).
 *
 * Since: 2.2
 **/
void
hildon_gtk_window_set_progress_indicator        (GtkWindow *window,
                                                 guint      state)
{
    hildon_gtk_window_set_flag (window, (HildonFlagFunc) do_set_progress_indicator, GUINT_TO_POINTER (state));
    if (HILDON_IS_WINDOW (window)) {
        HildonWindowPrivate *priv = HILDON_WINDOW_GET_PRIVATE (window);
        if (priv->edit_toolbar) {
            HildonEditToolbar *tb = HILDON_EDIT_TOOLBAR (priv->edit_toolbar);
            hildon_edit_toolbar_set_progress_indicator (tb, state);
        }
    }
}

/**
 * hildon_gtk_window_set_do_not_disturb:
 * @window: a #GtkWindow
 * @dndflag: %TRUE to set the "do-not-disturb" flag, %FALSE to clear it
 *
 * This function tells the window manager to set (or clear) the
 * "do-not-disturb" flag on @window.
 *
 * Since: 2.2
 **/
void
hildon_gtk_window_set_do_not_disturb            (GtkWindow *window,
                                                 gboolean   dndflag)
{
    hildon_gtk_window_set_flag (window, (HildonFlagFunc) do_set_do_not_disturb, GUINT_TO_POINTER (dndflag));
}

/**
 * hildon_gtk_window_set_portrait_flags:
 * @window: a #GtkWindow
 * @portrait_flags: a combination of #HildonPortraitFlags
 *
 * Sets the portrait flags for @window.
 *
 * Since: 2.2
 **/
void
hildon_gtk_window_set_portrait_flags            (GtkWindow           *window,
                                                 HildonPortraitFlags  portrait_flags)
{
    hildon_gtk_window_set_flag (window, (HildonFlagFunc) do_set_portrait_flags, GUINT_TO_POINTER (portrait_flags));
}

/**
 * hildon_gtk_window_enable_zoom_keys:
 * @window: a #GtkWindow
 * @enable: %TRUE to let the window use the zoom keys
 *
 * Use this function with @enable set to %TRUE to make the window
 * receive events from the zoom keys (#HILDON_HARDKEY_DECREASE and
 * #HILDON_HARDKEY_INCREASE).
 *
 * If set to %FALSE, those keys will be reserved for the system to
 * control things such as the volume level.
 *
 * Since: 2.2.2
 **/
void
hildon_gtk_window_enable_zoom_keys              (GtkWindow *window,
                                                 gboolean   enable)
{
    hildon_gtk_window_set_flag (window, (HildonFlagFunc) do_set_zoom_keys, GUINT_TO_POINTER (enable));
}

/**
 * hildon_gtk_window_take_screenshot:
 * @window: a #GtkWindow
 * @take: %TRUE to take a screenshot, %FALSE to destroy the existing one.
 *
 * Tells the window manager to create a screenshot of @window and save
 * it, or to destroy the existing one. If @take is %TRUE but the
 * screenshot is already available, the window manager will not create
 * it again.
 *
 * You should only call this method when @window is already mapped.
 *
 * In Maemo 5 this screenshot, if existent, will be used by the window
 * manager in subsequent launches of the application that created
 * it. The window manager will remove this screenshot automatically
 * whenever the theme, locale, or the time changes; also when a backup
 * is restored. If your application changes its appearance between
 * runs and you want to force the existent screenshot to be removed,
 * set @take to %FALSE.
 *
 * Since: 2.2
 *
 **/
void
hildon_gtk_window_take_screenshot               (GtkWindow *window,
                                                 gboolean   take)
{
    XEvent xev = { 0 };

    g_return_if_fail (GTK_IS_WINDOW (window));
    g_return_if_fail (gtk_widget_get_mapped (GTK_WIDGET (window)));

    xev.xclient.type = ClientMessage;
    xev.xclient.serial = 0;
    xev.xclient.send_event = True;
    xev.xclient.display = GDK_DISPLAY_XDISPLAY (gtk_widget_get_display (GTK_WIDGET (window)));
    xev.xclient.window = XDefaultRootWindow (xev.xclient.display);
    xev.xclient.message_type = XInternAtom (xev.xclient.display, "_HILDON_LOADING_SCREENSHOT", False);
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = take ? 0 : 1;
    xev.xclient.data.l[1] = GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (window)));

    XSendEvent (xev.xclient.display,
                xev.xclient.window,
                False,
                SubstructureRedirectMask | SubstructureNotifyMask,
                &xev);

    XFlush (xev.xclient.display);
    XSync (xev.xclient.display, False);
}

/* XIfEvent() predicate to check for a reply to a
 * _HILDON_LOADING_SCREENSHOT command. */
static Bool
screenshot_done (Display *dpy, const XEvent *event, GtkWindow *window)
{
  return event->type == ClientMessage
    && event->xclient.message_type == XInternAtom (dpy,
                                           "_HILDON_LOADING_SCREENSHOT",
                                           False)
    && event->xclient.window == GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (window)));
}

/**
 * hildon_gtk_window_take_screenshot_sync:
 * @window: a #GtkWindow
 * @take: %TRUE to take a screenshot, %FALSE to destroy the existing one.
 *
 * Like hildon_gtk_window_take_screenshot() but blocks until the
 * operation is complete.
 *
 * Since: 2.2.9
 *
 **/
void
hildon_gtk_window_take_screenshot_sync          (GtkWindow *window,
                                                 gboolean   take)
{
  XEvent foo;

  hildon_gtk_window_take_screenshot (window, take);
  XIfEvent (GDK_DISPLAY_XDISPLAY (gtk_widget_get_display (GTK_WIDGET (window))),
            &foo, (void *)screenshot_done, (XPointer)window);
}

/**
 * hildon_gtk_hscale_new:
 *
 * Creates a new horizontal scale widget that lets the user select
 * a value. The value is technically a double between 0.0 and 1.0.
 * See gtk_adjustment_configure() for reconfiguring the adjustment.
 *
 * The scale is hildonized, which means that a click or tap immediately
 * jumps to the desired position, see gtk_range_set_jump_to_position().
 * Further more the value is not displayed, see gtk_scale_set_draw_value().
 *
 * Returns: a new hildonized horizontally orientated #GtkScale
 *
 * Since: 2.2
 **/
GtkWidget*
hildon_gtk_hscale_new                           (void)
{
  GtkWidget *scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.1);
  g_object_set (scale, "draw-value", FALSE, NULL);

  return scale;
}

/**
 * hildon_gtk_vscale_new:
 *
 * Creates a new vertical scale widget that lets the user select
 * a value. The value is technically a double between 0.0 and 1.0.
 * See gtk_adjustment_configure() for reconfiguring the adjustment.
 *
 * The scale is hildonized, which means that a click or tap immediately
 * jumps to the desired position, see gtk_range_set_jump_to_position().
 * Further more the value is not displayed, see gtk_scale_set_draw_value().
 *
 * Returns: a new hildonized vertically orientated #GtkScale
 *
 * Since: 2.2
 **/
GtkWidget*
hildon_gtk_vscale_new                           (void)
{
  GtkWidget *scale = gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL, 0.0, 1.0, 0.1);
  g_object_set (scale, "draw-value", FALSE, NULL);

  return scale;
}

#define HILDON_HEIGHT_FINGER    70

#define HILDON_HEIGHT_THUMB     105

#define HILDON_WIDTH_FULLSCREEN (gdk_screen_get_width (gdk_screen_get_default ()))

#define HILDON_WIDTH_HALFSCREEN (HILDON_WIDTH_FULLSCREEN / 2)

/**
 * hildon_gtk_widget_set_theme_size:
 * @widget: A #GtkWidget
 * @size: Flags indicating the size of the widget
 *
 * This function sets the requested size of a widget.
 *
 * Since: maemo 2.0
 * Stability: Unstable
 **/
void
hildon_gtk_widget_set_theme_size (GtkWidget      *widget,
                                  HildonSizeType  size)
{
  gint width = -1;
  gint height = -1;
  gchar *widget_name = NULL;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  /* Requested height */
  if (size & HILDON_SIZE_FINGER_HEIGHT)
    {
      height = HILDON_HEIGHT_FINGER;
      widget_name = "-finger";
    }
  else if (size & HILDON_SIZE_THUMB_HEIGHT)
    {
      height = HILDON_HEIGHT_THUMB;
      widget_name = "-thumb";
    }

  if (widget_name)
    widget_name = g_strconcat (g_type_name (G_OBJECT_TYPE (widget)),
                               widget_name, NULL);

    /* Requested width */
  if (size & HILDON_SIZE_HALFSCREEN_WIDTH)
    {
      //width = HILDON_WIDTH_HALFSCREEN;
      gtk_widget_set_hexpand (widget, TRUE);
    }
  else if (size & HILDON_SIZE_FULLSCREEN_WIDTH)
    {
      //width = HILDON_WIDTH_FULLSCREEN;
      gtk_widget_set_hexpand (widget, TRUE);
    }

  gtk_widget_set_size_request (widget, width, height);

  if (widget_name)
    {
      gtk_widget_set_name (widget, widget_name);
      g_free (widget_name);
    }
}
