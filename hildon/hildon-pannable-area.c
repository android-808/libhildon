/*
 * This file is a part of hildon
 *
 * Copyright (C) 2008 Nokia Corporation, all rights reserved.
 *
 * Contact: Rodrigo Novo <rodrigo.novo@nokia.com>
 *
 * This widget is based on MokoFingerScroll from libmokoui
 * OpenMoko Application Framework UI Library
 * Authored by Chris Lord <chris@openedhand.com>
 * Copyright (C) 2006-2007 OpenMoko Inc.
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
 * SECTION: hildon-pannable-area
 * @short_description: A scrolling widget designed for touch screens
 * @see_also: #GtkScrolledWindow
 *
 * #HildonPannableArea is a container widget that can be "panned" (scrolled)
 * up and down using the touchscreen with fingers. The widget has no scrollbars,
 * but it rather shows small scroll indicators to give an idea of the part of the
 * content that is visible at a time. The scroll indicators appear when a dragging
 * motion is started on the pannable area.
 *
 * The scrolling can be "kinetic", meaning the motion can be "flicked"
 * and it will continue from the initial motion by gradually slowing
 * down to an eventual stop. Motion can also be stopped immediately by
 * pressing the touchscreen over the pannable area. It also has other
 * mode to make it scroll just following the finger drag without
 * kinetics. Mode can also be auto, this is, it takes automatically
 * the best choice for every event of drag or tap. These options can be
 * selected with the #HildonPannableArea:mode property.
 *
 * With #HildonPannableArea:mov-mode property you can switch between
 * vertical, horizontal and both panning directions.
 */

#include <math.h>
#include <cairo.h>
#include <gdk/gdk.h>

#include "hildon-pannable-area.h"
#include "hildon-marshalers.h"
#include "hildon-enum-types.h"

#define SCROLL_BAR_MIN_SIZE 5
#define RATIO_TOLERANCE 0.000001
#define SCROLL_FADE_IN_TIMEOUT 50
#define SCROLL_FADE_TIMEOUT 100
#define MOTION_EVENTS_PER_SECOND 25
#define CURSOR_STOPPED_TIMEOUT 200
#define MAX_SPEED_THRESHOLD 280
#define PANNABLE_MAX_WIDTH 788
#define PANNABLE_MAX_HEIGHT 378
#define ACCEL_FACTOR 27
#define MIN_ACCEL_THRESHOLD 40
#define FAST_CLICK 125

struct _HildonPannableAreaPrivate {
  HildonPannableAreaMode mode;
  HildonMovementMode mov_mode;
  GdkWindow *event_window;
  gdouble x;		/* Used to store mouse co-ordinates of the first or */
  gdouble y;		/* previous events in a press-motion pair */
  gdouble ex;		/* Used to store mouse co-ordinates of the last */
  gdouble ey;		/* motion event in acceleration mode */
  gboolean enabled;
  gboolean button_pressed;
  guint32 last_time;	/* Last event time, to stop infinite loops */
  guint32 last_press_time;
  gint last_type;
  gboolean last_in;
  gboolean moved;
  gdouble vmin;
  gdouble vmax;
  gdouble vmax_overshooting;
  gdouble accel_vel_x;
  gdouble accel_vel_y;
  gdouble vfast_factor;
  gdouble decel;
  gdouble drag_inertia;
  gdouble scroll_time;
  gdouble vel_factor;
  guint sps;
  guint panning_threshold;
  guint scrollbar_fade_delay;
  guint bounce_steps;
  guint force;
  guint direction_error_margin;
  gdouble vel_x;
  gdouble vel_y;
  gdouble old_vel_x;
  gdouble old_vel_y;
  GdkWindow *child;
  gint child_width;
  gint child_height;
  gint ix;			/* Initial click mouse co-ordinates */
  gint iy;
  gint cx;			/* Initial click child window mouse co-ordinates */
  gint cy;
  guint idle_id;
  gdouble scroll_to_x;
  gdouble scroll_to_y;
  gdouble motion_x;
  gdouble motion_y;
  gint overshot_dist_x;
  gint overshot_dist_y;
  gint overshooting_y;
  gint overshooting_x;
  gdouble scroll_indicator_alpha;
  gint motion_event_scroll_timeout;
  gint scroll_indicator_timeout;
  gint scroll_indicator_event_interrupt;
  gint scroll_delay_counter;
  gint vovershoot_max;
  gint hovershoot_max;
  gboolean fade_in;
  gboolean initial_hint;
  gboolean initial_effect;
  gboolean low_friction_mode;
  gboolean first_drag;

  gboolean size_request_policy;
  gboolean hscroll_visible;
  gboolean vscroll_visible;
  GdkRectangle hscroll_rect;
  GdkRectangle vscroll_rect;
  guint indicator_width;

  GtkAdjustment *hadjust;
  GtkAdjustment *vadjust;
  gint x_offset;
  gint y_offset;

  GtkPolicyType vscrollbar_policy;
  GtkPolicyType hscrollbar_policy;

  GdkColor scroll_color;

  gboolean center_on_child_focus;
  gboolean center_on_child_focus_pending;

  gboolean selection_movement;

  // NEW from GtkAdjustment
  gdouble lower;
  gdouble upper;
  gdouble vvalue;
  gdouble hvalue;
  gdouble step_increment;
  gdouble page_increment;
  gdouble page_size;

  gdouble hsource;
  gdouble htarget;
  gdouble vsource;
  gdouble vtarget;

  guint duration;
  gint64 start_time;
  gint64 end_time;
  guint tick_id;
  GdkFrameClock *clock;
};

/*signals*/
enum {
  HORIZONTAL_MOVEMENT,
  VERTICAL_MOVEMENT,
  PANNING_STARTED,
  PANNING_FINISHED,
  LAST_SIGNAL
};

static guint pannable_area_signals [LAST_SIGNAL] = { 0 };

enum {
  PROP_ENABLED = 1,
  PROP_MODE,
  PROP_MOVEMENT_MODE,
  PROP_VELOCITY_MIN,
  PROP_VELOCITY_MAX,
  PROP_VEL_MAX_OVERSHOOTING,
  PROP_VELOCITY_FAST_FACTOR,
  PROP_DECELERATION,
  PROP_DRAG_INERTIA,
  PROP_SPS,
  PROP_PANNING_THRESHOLD,
  PROP_SCROLLBAR_FADE_DELAY,
  PROP_BOUNCE_STEPS,
  PROP_FORCE,
  PROP_DIRECTION_ERROR_MARGIN,
  PROP_VSCROLLBAR_POLICY,
  PROP_HSCROLLBAR_POLICY,
  PROP_VOVERSHOOT_MAX,
  PROP_HOVERSHOOT_MAX,
  PROP_SCROLL_TIME,
  PROP_INITIAL_HINT,
  PROP_LOW_FRICTION_MODE,
  PROP_SIZE_REQUEST_POLICY,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_CENTER_ON_CHILD_FOCUS,
  PROP_LAST
};

G_DEFINE_TYPE_WITH_PRIVATE (HildonPannableArea, hildon_pannable_area, GTK_TYPE_SCROLLED_WINDOW)

static GtkBinClass *bin_class = NULL;

static void hildon_pannable_area_class_init (HildonPannableAreaClass * klass);
static void hildon_pannable_area_init (HildonPannableArea * area);
static void hildon_pannable_area_get_property (GObject * object,
                                               guint property_id,
                                               GValue * value,
                                               GParamSpec * pspec);
static void hildon_pannable_area_set_property (GObject * object,
                                               guint property_id,
                                               const GValue * value,
                                               GParamSpec * pspec);
static void hildon_pannable_area_remove_timeouts (GtkWidget * widget);
static void hildon_pannable_area_dispose (GObject * object);
static void hildon_pannable_area_realize (GtkWidget * widget);
static void hildon_pannable_area_unrealize (GtkWidget * widget);
static void hildon_pannable_area_child_allocate_calculate (GtkWidget * widget,
                                                           GtkAllocation * allocation,
                                                           GtkAllocation * child_allocation);
static void hildon_pannable_area_grab_notify (GtkWidget *widget,
                                              gboolean was_grabbed,
                                              gpointer user_data);
static void hildon_pannable_area_redraw (HildonPannableArea * area);
static GdkWindow * hildon_pannable_area_get_topmost (GdkWindow * window,
                                                     gint x, gint y,
                                                     gint * tx, gint * ty,
                                                     GdkEventMask mask);
static void hildon_pannable_area_refresh (HildonPannableArea * area);
static gboolean hildon_pannable_area_check_scrollbars (HildonPannableArea * area);
static void hildon_pannable_axis_scroll (HildonPannableArea *area,
                                         GtkAdjustment *adjust,
                                         gdouble *vel,
                                         gdouble inc,
                                         gint *overshooting,
                                         gint *overshot_dist,
                                         gdouble *scroll_to,
                                         gint overshoot_max,
                                         gboolean *s);
static void hildon_pannable_area_child_mapped (GtkWidget *widget,
                                               GdkEvent  *event,
                                               gpointer user_data);
static void hildon_pannable_area_add (GtkContainer *container, GtkWidget *child);
static void hildon_pannable_area_remove (GtkContainer *container, GtkWidget *child);
static void hildon_pannable_area_set_focus_child (GtkContainer *container,
                                                 GtkWidget *child);
static void hildon_pannable_area_center_on_child_focus (HildonPannableArea *area);


static void
hildon_pannable_area_finalize (GObject *object)
{
  HildonPannableArea *area = HILDON_PANNABLE_AREA (object);
  HildonPannableAreaPrivate *priv = area->priv;

  if (priv->tick_id)
    g_signal_handler_disconnect (priv->clock, priv->tick_id);
  if (priv->clock)
    g_object_unref (priv->clock);

  G_OBJECT_CLASS (hildon_pannable_area_parent_class)->finalize (object);
}

static void
hildon_pannable_area_class_init (HildonPannableAreaClass * class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
//  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
//  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  object_class->finalize     = hildon_pannable_area_finalize;
  object_class->dispose      = hildon_pannable_area_dispose;
  object_class->set_property = hildon_pannable_area_set_property;
  object_class->get_property = hildon_pannable_area_get_property;

  /*
  widget_class->realize = hildon_pannable_area_realize;
  widget_class->unrealize = hildon_pannable_area_unrealize;

  container_class->add = hildon_pannable_area_add;
  container_class->remove = hildon_pannable_area_remove;
  container_class->set_focus_child = hildon_pannable_area_set_focus_child;
*/
}

static void
hildon_pannable_area_init (HildonPannableArea * area)
{
  area->priv = hildon_pannable_area_get_instance_private (area);
  area->priv->duration = 200;
  area->priv->clock = gtk_widget_get_frame_clock (GTK_WIDGET (area));

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (area),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (area), NULL);
  gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (area), NULL);
}

static void
hildon_pannable_area_get_property (GObject * object,
                                   guint property_id,
				   GValue * value,
                                   GParamSpec * pspec)
{
//  HildonPannableAreaPrivate *priv = HILDON_PANNABLE_AREA (object)->priv;

  switch (property_id) {
/*  case PROP_CENTER_ON_CHILD_FOCUS:
    g_value_set_boolean (value, priv->center_on_child_focus);
    break;*/
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
hildon_pannable_area_set_property (GObject * object,
                                   guint property_id,
				   const GValue * value,
                                   GParamSpec * pspec)
{
//  HildonPannableAreaPrivate *priv = HILDON_PANNABLE_AREA (object)->priv;
//  gboolean enabled;

  switch (property_id) {
/*  case PROP_ENABLED:
    enabled = g_value_get_boolean (value);

    if ((priv->enabled != enabled) && (gtk_widget_get_realized (GTK_WIDGET (object)))) {
      if (enabled)
	gdk_window_raise (priv->event_window);
      else
	gdk_window_lower (priv->event_window);
    }

    priv->enabled = enabled;
    break;
*/

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
hildon_pannable_area_dispose (GObject * object)
{
  HildonPannableAreaPrivate *priv = HILDON_PANNABLE_AREA (object)->priv;
  GtkWidget *child = gtk_bin_get_child (GTK_BIN (object));

  hildon_pannable_area_remove_timeouts (GTK_WIDGET (object));

  if (child) {
    g_signal_handlers_disconnect_by_func (child,
                                          hildon_pannable_area_child_mapped,
                                          object);
  }

  if (G_OBJECT_CLASS (hildon_pannable_area_parent_class)->dispose)
    G_OBJECT_CLASS (hildon_pannable_area_parent_class)->dispose (object);
}


static void
hildon_pannable_area_remove_timeouts (GtkWidget * widget)
{
  HildonPannableAreaPrivate *priv = HILDON_PANNABLE_AREA (widget)->priv;

  if (priv->idle_id) {
    g_signal_emit (widget, pannable_area_signals[PANNING_FINISHED], 0);
    g_source_remove (priv->idle_id);
    priv->idle_id = 0;
  }

  if (priv->scroll_indicator_timeout){
    g_source_remove (priv->scroll_indicator_timeout);
    priv->scroll_indicator_timeout = 0;
  }

  if (priv->motion_event_scroll_timeout){
    g_source_remove (priv->motion_event_scroll_timeout);
    priv->motion_event_scroll_timeout = 0;
  }
}

static void
hildon_pannable_area_unrealize (GtkWidget * widget)
{
  HildonPannableAreaPrivate *priv;

  priv = HILDON_PANNABLE_AREA (widget)->priv;

  if (gtk_widget_get_mapped (widget))
      hildon_pannable_area_unmap (widget);

  hildon_pannable_area_remove_timeouts (widget);

  if (priv->event_window != NULL) {
    gdk_window_set_user_data (priv->event_window, NULL);
    gdk_window_destroy (priv->event_window);
    priv->event_window = NULL;
  }

  if (GTK_WIDGET_CLASS (hildon_pannable_area_parent_class)->unrealize)
    (*GTK_WIDGET_CLASS (hildon_pannable_area_parent_class)->unrealize)(widget);
}

static void
hildon_pannable_area_child_allocate_calculate (GtkWidget * widget,
                                               GtkAllocation * allocation,
                                               GtkAllocation * child_allocation)
{
  gint border_width;
  HildonPannableAreaPrivate *priv;

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  priv = HILDON_PANNABLE_AREA (widget)->priv;

  child_allocation->x = 0;
  child_allocation->y = 0;
  child_allocation->width = MAX (allocation->width - 2 * border_width -
                                 (priv->vscroll_visible ? priv->vscroll_rect.width : 0), 0);
  child_allocation->height = MAX (allocation->height - 2 * border_width -
                                  (priv->hscroll_visible ? priv->hscroll_rect.height : 0), 0);

  if (priv->overshot_dist_y > 0) {
    child_allocation->y = MIN (child_allocation->y + priv->overshot_dist_y,
                               child_allocation->height);
    child_allocation->height = MAX (child_allocation->height - priv->overshot_dist_y, 0);
  } else if (priv->overshot_dist_y < 0) {
    child_allocation->height = MAX (child_allocation->height + priv->overshot_dist_y, 0);
  }

  if (priv->overshot_dist_x > 0) {
    child_allocation->x = MIN (child_allocation->x + priv->overshot_dist_x,
                               child_allocation->width);
    child_allocation->width = MAX (child_allocation->width - priv->overshot_dist_x, 0);
  } else if (priv->overshot_dist_x < 0) {
    child_allocation->width = MAX (child_allocation->width + priv->overshot_dist_x, 0);
  }
}

static void
toplevel_window_unmapped (GtkWidget * widget,
                          HildonPannableArea * area)
{
    area->priv->initial_effect = TRUE;
}

static void
hildon_pannable_area_grab_notify (GtkWidget *widget,
                                  gboolean was_grabbed,
                                  gpointer user_data)
{
  /* an internal widget has grabbed the focus and now has returned it,
     we have to do some release actions */
  if (was_grabbed) {
    HildonPannableAreaPrivate *priv = HILDON_PANNABLE_AREA (widget)->priv;

    priv->scroll_indicator_event_interrupt = 0;

    if ((!priv->scroll_indicator_timeout)&&(priv->scroll_indicator_alpha)>0.1) {
      priv->scroll_delay_counter = priv->scrollbar_fade_delay;

      hildon_pannable_area_launch_fade_timeout (HILDON_PANNABLE_AREA (widget),
                                                priv->scroll_indicator_alpha);
    }

    priv->last_type = 3;
    priv->moved = FALSE;
  }
}

static void
hildon_pannable_area_redraw (HildonPannableArea * area)
{
  HildonPannableAreaPrivate *priv = HILDON_PANNABLE_AREA (area)->priv;

  /* Redraw scroll indicators */
  if (gtk_widget_is_drawable (GTK_WIDGET (area))) {
      if (priv->hscroll_visible) {
        gdk_window_invalidate_rect (gtk_widget_get_window (GTK_WIDGET (area)),
                                    &priv->hscroll_rect, FALSE);
      }

      if (priv->vscroll_visible) {
        gdk_window_invalidate_rect (gtk_widget_get_window (GTK_WIDGET (area)),
                                    &priv->vscroll_rect, FALSE);
      }
  }
}

static GdkWindow *
hildon_pannable_area_get_topmost (GdkWindow * window,
                                  gint x, gint y,
                                  gint * tx, gint * ty,
                                  GdkEventMask mask)
{
  /* Find the GdkWindow at the given point, by recursing from a given
   * parent GdkWindow. Optionally return the co-ordinates transformed
   * relative to the child window.
   */
  gint width, height;
  GList *c, *children;
  GdkWindow *selected_window = NULL;

  width = gdk_window_get_width (window);
  height = gdk_window_get_height (window);

  if ((x < 0) || (x >= width) || (y < 0) || (y >= height))
    return NULL;

  children = gdk_window_peek_children (window);

  if (!children) {
    if (tx)
      *tx = x;
    if (ty)
      *ty = y;
    selected_window = window;
  }

  for (c = children; c; c = c->next) {
    GdkWindow *child = (GdkWindow *) c->data;
    gint wx, wy;

    width = gdk_window_get_width (child);
    height = gdk_window_get_height (child);
    gdk_window_get_position (child, &wx, &wy);

    if ((x >= wx) && (x < (wx + width)) && (y >= wy) && (y < (wy + height)) &&
        (gdk_window_is_visible (child))) {

      if (gdk_window_peek_children (child)) {
        selected_window = hildon_pannable_area_get_topmost (child, x-wx, y-wy,
                                                            tx, ty, mask);
        if (!selected_window) {
          if (tx)
            *tx = x-wx;
          if (ty)
            *ty = y-wy;
          selected_window = child;
        }
      } else {
        if ((gdk_window_get_events (child)&mask)) {
          if (tx)
            *tx = x-wx;
          if (ty)
            *ty = y-wy;
          selected_window = child;
        }
      }
    }
  }

  return selected_window;
}

static void
hildon_pannable_area_child_mapped (GtkWidget *widget,
                                   GdkEvent  *event,
                                   gpointer user_data)
{
  HildonPannableAreaPrivate *priv = HILDON_PANNABLE_AREA (user_data)->priv;

  if (priv->event_window != NULL && priv->enabled)
    gdk_window_raise (priv->event_window);
}

static void
hildon_pannable_area_add (GtkContainer *container, GtkWidget *child)
{
  HildonPannableAreaPrivate *priv = HILDON_PANNABLE_AREA (container)->priv;
  GtkScrollable *scroll = GTK_SCROLLABLE (child);

  g_return_if_fail (gtk_bin_get_child (GTK_BIN (container)) == NULL);

  GTK_CONTAINER_CLASS (bin_class)->add (container, child);

  g_signal_connect_after (child, "map-event",
                          G_CALLBACK (hildon_pannable_area_child_mapped),
                          container);

  if (scroll != NULL) {
    gtk_scrollable_set_hadjustment (scroll, priv->hadjust);
    gtk_scrollable_set_vadjustment (scroll, priv->vadjust);
  }
  else {
    g_warning ("%s: cannot add non scrollable widget, "
               "wrap it in a viewport", __FUNCTION__);
  }
}

/* call this function if you are not panning */
static void
hildon_pannable_area_center_on_child_focus      (HildonPannableArea *area)
{
  GtkWidget *focused_child = NULL;
  GtkWidget *window = NULL;

  window = gtk_widget_get_toplevel (GTK_WIDGET (area));

  if (gtk_widget_is_toplevel (window)) {
    focused_child = gtk_window_get_focus (GTK_WINDOW (window));
  }

  if (focused_child) {
    hildon_pannable_area_scroll_to_child (area, focused_child);
  }
}

static void
hildon_pannable_area_set_focus_child            (GtkContainer     *container,
                                                 GtkWidget        *child)
{
  HildonPannableArea *area = HILDON_PANNABLE_AREA (container);

  if (!area->priv->center_on_child_focus) {
    return;
  }

  if (GTK_IS_WIDGET (child)) {
    area->priv->center_on_child_focus_pending = TRUE;
  }
}

static void
hildon_pannable_area_remove (GtkContainer *container, GtkWidget *child)
{
  GtkScrollable *scroll = GTK_SCROLLABLE (child);
  g_return_if_fail (HILDON_IS_PANNABLE_AREA (container));
  g_return_if_fail (child != NULL);
  g_return_if_fail (gtk_bin_get_child (GTK_BIN (container)) == child);

  if (scroll != NULL) {
    gtk_scrollable_set_hadjustment (scroll, NULL);
    gtk_scrollable_set_vadjustment (scroll, NULL);
  }

  g_signal_handlers_disconnect_by_func (child,
                                        hildon_pannable_area_child_mapped,
                                        container);

  /* chain parent class handler to remove child */
  GTK_CONTAINER_CLASS (hildon_pannable_area_parent_class)->remove (container, child);
}

/**
 * hildon_pannable_area_new:
 *
 * Create a new pannable area widget
 *
 * Returns: the newly created #HildonPannableArea
 *
 * Since: 2.2
 */

GtkWidget *
hildon_pannable_area_new (void)
{
  return g_object_new (HILDON_TYPE_PANNABLE_AREA, NULL);
}

/**
 * hildon_pannable_area_add_with_viewport:
 * @area: A #HildonPannableArea
 * @child: Child widget to add to the viewport
 *
 *
 * Convenience function used to add a child to a #GtkViewport, and add
 * the viewport to the scrolled window for childreen without native
 * scrolling capabilities.
 *
 * Note: Left for backwards compatibility at the moment. DEPRECATED!
 * 
 * See gtk_container_add() for more information.
 *
 * Since: 2.2
 */

void
hildon_pannable_area_add_with_viewport (HildonPannableArea * area,
					GtkWidget * child)
{
  g_return_if_fail (HILDON_IS_PANNABLE_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == NULL);

  gtk_container_add (GTK_CONTAINER (area), child);
}

static void hildon_pannable_area_on_frame_clock_update (GdkFrameClock *clock,
                                                        HildonPannableArea *area);

static void
hildon_pannable_area_begin_updating (HildonPannableArea *area)
{
  HildonPannableAreaPrivate *priv = area->priv;

  if (priv->tick_id == 0)
    {
      priv->tick_id = g_signal_connect (priv->clock, "update",
                                        G_CALLBACK (hildon_pannable_area_on_frame_clock_update), area);
      gdk_frame_clock_begin_updating (priv->clock);
    }
}

static void
hildon_pannable_area_end_updating (HildonPannableArea *area)
{
  HildonPannableAreaPrivate *priv = area->priv;

  if (priv->tick_id != 0)
    {
      g_signal_handler_disconnect (priv->clock, priv->tick_id);
      priv->tick_id = 0;
      gdk_frame_clock_end_updating (priv->clock);
    }
}

/* From clutter-easing.c, based on Robert Penner's
 * infamous easing equations, MIT license.
 */
static gdouble
ease_out_cubic (gdouble t)
{
  gdouble p = t - 1;

  return p * p * p + 1;
}

static void
hildon_pannable_area_on_frame_clock_update (GdkFrameClock *clock,
                                            HildonPannableArea *area)
{
  HildonPannableAreaPrivate *priv = area->priv;
  gint64 now;

  now = gdk_frame_clock_get_frame_time (clock);

  GtkAdjustment *hadj;
  GtkAdjustment *vadj;

  hadj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (area));
  vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (area));

  if (now < priv->end_time)
    {
      gdouble t;

      t = (now - priv->start_time) / (gdouble) (priv->end_time - priv->start_time);
      t = ease_out_cubic (t);
      gtk_adjustment_set_value (hadj, priv->hsource + t * (priv->htarget - priv->hsource));
      gtk_adjustment_set_value (vadj, priv->vsource + t * (priv->vtarget - priv->vsource));
    }
  else
    {
      gtk_adjustment_set_value (hadj, priv->htarget);
      gtk_adjustment_set_value (vadj, priv->vtarget);
      hildon_pannable_area_end_updating (area);
    }
}


// WORKING ON THIS.  HANDLE -1 values
static void hildon_pannable_area_set_value_internal (HildonPannableArea *area,
                                         gdouble             hvalue,
                                         gdouble             vvalue,
                                         gboolean            animate)
{
  HildonPannableAreaPrivate *priv = area->priv;

  /* don't use CLAMP() so we don't end up below lower if upper - page_size
   * is smaller than lower
   */

  GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (area));
  if (vvalue == -1)
    {
      vvalue = gtk_adjustment_get_value (vadj);
    }
  else
    {
      vvalue = MIN (vvalue, gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj));
      vvalue = MAX (vvalue, gtk_adjustment_get_lower(vadj));
    }

  GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (area));
  if (hvalue == -1)
    {
      hvalue = gtk_adjustment_get_value (hadj);
    }
  else
    {
      hvalue = MIN (hvalue, gtk_adjustment_get_upper(hadj) - gtk_adjustment_get_page_size(hadj));
      hvalue = MAX (hvalue, gtk_adjustment_get_lower(hadj));
    }

  area->priv->clock = gtk_widget_get_frame_clock (GTK_WIDGET (area));
  
  if (animate && priv->duration != 0 && priv->clock != NULL)
    {
      if (priv->tick_id && priv->htarget == hvalue && priv->vtarget == vvalue)
        return;

      priv->vsource = gtk_adjustment_get_value (vadj);
      priv->vtarget = vvalue;
      priv->hsource = gtk_adjustment_get_value (hadj);
      priv->htarget = hvalue;
      priv->start_time = gdk_frame_clock_get_frame_time (priv->clock);
      priv->end_time = priv->start_time + 1000 * priv->duration;
      hildon_pannable_area_begin_updating (area);
    }
  else
    {
      hildon_pannable_area_end_updating (area);
      gtk_adjustment_set_value (vadj, vvalue);
      gtk_adjustment_set_value (hadj, hvalue);
    }
}

/**
 * hildon_pannable_area_scroll_to:
 * @area: A #HildonPannableArea.
 * @x: The x coordinate of the destination point or -1 to ignore this axis.
 * @y: The y coordinate of the destination point or -1 to ignore this axis.
 *
 * Smoothly scrolls @area to ensure that (@x, @y) is a visible point
 * on the widget. To move in only one coordinate, you must set the other one
 * to -1. Notice that, in %HILDON_PANNABLE_AREA_MODE_PUSH mode, this function
 * works just like hildon_pannable_area_jump_to().
 *
 * This function is useful if you need to present the user with a particular
 * element inside a scrollable widget, like #GtkTreeView. For instance,
 * the following example shows how to scroll inside a #GtkTreeView to
 * make visible an item, indicated by the #GtkTreeIter @iter.
 *
 * <example>
 * <programlisting>
 *  GtkTreePath *path;
 *  GdkRectangle rect;
 *  gint y;
 *  <!-- -->
 *  path = gtk_tree_model_get_path (model, &amp;iter);
 *  gtk_tree_view_get_background_area (GTK_TREE_VIEW (treeview),
 *                                     path, NULL, &amp;rect);
 *  gtk_tree_view_convert_bin_window_to_tree_coords (GTK_TREE_VIEW (treeview),
 *                                                   0, rect.y, NULL, &amp;y);
 *  hildon_pannable_area_scroll_to (panarea, -1, y);
 *  gtk_tree_path_free (path);
 * </programlisting>
 * </example>
 *
 * If you want to present a child widget in simpler scenarios,
 * use hildon_pannable_area_scroll_to_child() instead.
 *
 * There is a precondition to this function: the widget must be
 * already realized. Check the hildon_pannable_area_jump_to_child() for
 * more tips regarding how to call this function during
 * initialization.
 *
 * Since: 2.2
 **/
void
hildon_pannable_area_scroll_to (HildonPannableArea *area,
				const gint x, const gint y)
{
  gboolean hscroll_visible, vscroll_visible;

  g_return_if_fail (HILDON_IS_PANNABLE_AREA (area));
  g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (area)));

  GtkAdjustment *hadj;
  GtkAdjustment *vadj;

  hadj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (area));
  vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (area));

  vscroll_visible = (gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_lower(vadj) > gtk_adjustment_get_page_size(vadj));
  hscroll_visible = (gtk_adjustment_get_upper(hadj) - gtk_adjustment_get_lower(hadj) > gtk_adjustment_get_page_size(hadj));

  // We don't have a scroll bar present so don't do anything.
  if (((!vscroll_visible)&&(!hscroll_visible)) || (x == -1 && y == -1)) {
    return;
  }

  gdouble scroll_to_x = CLAMP (x - gtk_adjustment_get_page_size(hadj)/2,
                               gtk_adjustment_get_lower(hadj),
                               gtk_adjustment_get_upper(hadj) - gtk_adjustment_get_page_size(hadj));
  gdouble scroll_to_y = CLAMP (y - gtk_adjustment_get_page_size(vadj)/2,
                               gtk_adjustment_get_lower(vadj),
                               gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj));

  hildon_pannable_area_set_value_internal (area, scroll_to_x, scroll_to_y, TRUE);
/*
  if (x != -1)
    gtk_adjustment_set_value(hadj, CLAMP (x - gtk_adjustment_get_page_size(hadj)/2,
                               gtk_adjustment_get_lower(hadj),
                               gtk_adjustment_get_upper(hadj) - gtk_adjustment_get_page_size(hadj)));
  if (y != -1)
  {
    gtk_adjustment_set_value(vadj, CLAMP (y - gtk_adjustment_get_page_size(vadj)/2,
                               gtk_adjustment_get_lower(vadj),
                               gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj)));
  }*/
}

/**
 * hildon_pannable_area_jump_to:
 * @area: A #HildonPannableArea.
 * @x: The x coordinate of the destination point or -1 to ignore this axis.
 * @y: The y coordinate of the destination point or -1 to ignore this axis.
 *
 * Jumps the position of @area to ensure that (@x, @y) is a visible
 * point in the widget. In order to move in only one coordinate, you
 * must set the other one to -1. See hildon_pannable_area_scroll_to()
 * function for an example of how to calculate the position of
 * children in scrollable widgets like #GtkTreeview.
 *
 * There is a precondition to this function: the widget must be
 * already realized. Check the hildon_pannable_area_jump_to_child() for
 * more tips regarding how to call this function during
 * initialization.
 *
 * Since: 2.2
 **/
void
hildon_pannable_area_jump_to (HildonPannableArea *area,
                              const gint x, const gint y)
{
  gboolean hscroll_visible, vscroll_visible;

  g_return_if_fail (HILDON_IS_PANNABLE_AREA (area));
  g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (area)));

  GtkAdjustment *hadj;
  GtkAdjustment *vadj;

  hadj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (area));
  vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (area));

  vscroll_visible = (gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_lower(vadj) > gtk_adjustment_get_page_size(vadj));
  hscroll_visible = (gtk_adjustment_get_upper(hadj) - gtk_adjustment_get_lower(hadj) > gtk_adjustment_get_page_size(hadj));

  // We don't have a scroll bar present so don't do anything.
  if (((!vscroll_visible)&&(!hscroll_visible)) || (x == -1 && y == -1)) {
    return;
  }

  gdouble jump_to_x = CLAMP (x - gtk_adjustment_get_page_size(hadj)/2,
                               gtk_adjustment_get_lower(hadj),
                               gtk_adjustment_get_upper(hadj) - gtk_adjustment_get_page_size(hadj));
  gdouble jump_to_y = CLAMP (y - gtk_adjustment_get_page_size(vadj)/2,
                               gtk_adjustment_get_lower(vadj),
                               gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj));

  hildon_pannable_area_set_value_internal (area, jump_to_x, jump_to_y, FALSE);

/*  if (x != -1)
    gtk_adjustment_set_value(hadj, CLAMP (x - gtk_adjustment_get_page_size(hadj)/2,
                               gtk_adjustment_get_lower(hadj),
                               gtk_adjustment_get_upper(hadj) - gtk_adjustment_get_page_size(hadj)));
  if (y != -1)
  {
    gtk_adjustment_set_value(vadj, CLAMP (y - gtk_adjustment_get_page_size(vadj)/2,
                               gtk_adjustment_get_lower(vadj),
                               gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj)));
  }*/
}

/**
 * hildon_pannable_area_scroll_to_child:
 * @area: A #HildonPannableArea.
 * @child: A #GtkWidget, descendant of @area.
 *
 * Smoothly scrolls until @child is visible inside @area. @child must
 * be a descendant of @area. If you need to scroll inside a scrollable
 * widget, e.g., #GtkTreeview, see hildon_pannable_area_scroll_to().
 *
 * There is a precondition to this function: the widget must be
 * already realized. Check the hildon_pannable_area_jump_to_child() for
 * more tips regarding how to call this function during
 * initialization.
 *
 * Since: 2.2
 **/
void
hildon_pannable_area_scroll_to_child (HildonPannableArea *area, GtkWidget *child)
{
  GtkWidget *bin_child;
  gint x, y;

  g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (area)));
  g_return_if_fail (HILDON_IS_PANNABLE_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_is_ancestor (child, GTK_WIDGET (area)));

  if (gtk_bin_get_child (GTK_BIN (area)) == NULL)
    return;

  /* We need to get to check the child of the inside the area */
  bin_child = gtk_bin_get_child (GTK_BIN (area));

  /* we check if we added a viewport */
  if (GTK_IS_VIEWPORT (bin_child)) {
    bin_child = gtk_bin_get_child (GTK_BIN (bin_child));
  }

  if (gtk_widget_translate_coordinates (child, bin_child, 0, 0, &x, &y))
    hildon_pannable_area_scroll_to (area, x, y);
}

/**
 * hildon_pannable_area_jump_to_child:
 * @area: A #HildonPannableArea.
 * @child: A #GtkWidget, descendant of @area.
 *
 * Jumps to make sure @child is visible inside @area. @child must
 * be a descendant of @area. If you want to move inside a scrollable
 * widget, like, #GtkTreeview, see hildon_pannable_area_scroll_to().
 *
 * There is a precondition to this function: the widget must be
 * already realized. You can control if the widget is ready with the
 * GTK_WIDGET_REALIZED macro. If you want to call this function during
 * the initialization process of the widget do it inside a callback to
 * the ::realize signal, using g_signal_connect_after() function.
 *
 * Since: 2.2
 **/
void
hildon_pannable_area_jump_to_child (HildonPannableArea *area, GtkWidget *child)
{
  GtkWidget *bin_child;
  gint x, y;

  g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (area)));
  g_return_if_fail (HILDON_IS_PANNABLE_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_is_ancestor (child, GTK_WIDGET (area)));

  if (gtk_bin_get_child (GTK_BIN (area)) == NULL)
    return;

  /* We need to get to check the child of the inside the area */
  bin_child = gtk_bin_get_child (GTK_BIN (area));

  /* we check if we added a viewport */
  if (GTK_IS_VIEWPORT (bin_child)) {
    bin_child = gtk_bin_get_child (GTK_BIN (bin_child));
  }

  if (gtk_widget_translate_coordinates (child, bin_child, 0, 0, &x, &y))
    hildon_pannable_area_jump_to (area, x, y);
}

/**
 * hildon_pannable_get_child_widget_at:
 * @area: A #HildonPannableArea.
 * @x: horizontal coordinate of the point
 * @y: vertical coordinate of the point
 *
 * Get the widget at the point (x, y) inside the pannable area. In
 * case no widget found it returns NULL.
 *
 * returns: the #GtkWidget if we find a widget, NULL in any other case
 *
 * Since: 2.2
 **/
GtkWidget*
hildon_pannable_get_child_widget_at (HildonPannableArea *area,
                                     gdouble x, gdouble y)
{
  GdkWindow *window = NULL;
  GtkWidget *child_widget = NULL;

  window = hildon_pannable_area_get_topmost
    (gtk_widget_get_window (gtk_bin_get_child (GTK_BIN (area))),
     x, y, NULL, NULL, GDK_ALL_EVENTS_MASK);

  gdk_window_get_user_data (window, (gpointer) &child_widget);

  return child_widget;
}


/**
 * hildon_pannable_area_get_hadjustment:
 * @area: A #HildonPannableArea.
 *
 * Returns the horizontal adjustment. This adjustment is the internal
 * widget adjustment used to control the animations. Do not modify it
 * directly to change the position of the pannable, to do that use the
 * pannable API. If you modify the object directly it could cause
 * artifacts in the animations.
 *
 * returns: The horizontal #GtkAdjustment
 *
 * Since: 2.2
 **/
GtkAdjustment*
hildon_pannable_area_get_hadjustment            (HildonPannableArea *area)
{

  g_return_val_if_fail (HILDON_IS_PANNABLE_AREA (area), NULL);

  return area->priv->hadjust;
}

/**
 * hildon_pannable_area_get_vadjustment:
 * @area: A #HildonPannableArea.
 *
 * Returns the vertical adjustment. This adjustment is the internal
 * widget adjustment used to control the animations. Do not modify it
 * directly to change the position of the pannable, to do that use the
 * pannable API. If you modify the object directly it could cause
 * artifacts in the animations.
 *
 * returns: The vertical #GtkAdjustment
 *
 * Since: 2.2
 **/
GtkAdjustment*
hildon_pannable_area_get_vadjustment            (HildonPannableArea *area)
{
  g_return_val_if_fail (HILDON_IS_PANNABLE_AREA (area), NULL);

  return area->priv->vadjust;
}


/**
 * hildon_pannable_area_get_center_on_child_focus
 * @area: A #HildonPannableArea
 *
 * Gets the @area #HildonPannableArea:center-on-child-focus property
 * value.
 *
 * See #HildonPannableArea:center-on-child-focus for more information.
 *
 * Returns: the @area #HildonPannableArea:center-on-child-focus value
 *
 * Since: 2.2
 **/
gboolean
hildon_pannable_area_get_center_on_child_focus  (HildonPannableArea *area)
{
  g_return_val_if_fail (HILDON_IS_PANNABLE_AREA (area), FALSE);

  return area->priv->center_on_child_focus;
}

/**
 * hildon_pannable_area_set_center_on_child_focus
 * @area: A #HildonPannableArea
 * @value: the new value
 *
 * Sets the @area #HildonPannableArea:center-on-child-focus property
 * to @value.
 *
 * See #HildonPannableArea:center-on-child-focus for more information.
 *
 * Since: 2.2
 **/
void
hildon_pannable_area_set_center_on_child_focus  (HildonPannableArea *area,
                                                 gboolean value)
{
  g_return_if_fail (HILDON_IS_PANNABLE_AREA (area));

  area->priv->center_on_child_focus = value;
}
