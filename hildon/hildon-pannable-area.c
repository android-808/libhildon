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

//G_DEFINE_TYPE (HildonPannableArea, hildon_pannable_area, GTK_TYPE_SCROLLED_WINDOW)

//#define PANNABLE_AREA_PRIVATE(o)                                \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), HILDON_TYPE_PANNABLE_AREA, \
                                HildonPannableAreaPrivate))

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
static void hildon_pannable_area_get_preferred_width (GtkWidget *widget,
                                                      gint      *minimum,
                                                      gint      *natural);
static void hildon_pannable_area_get_preferred_height (GtkWidget *widget,
                                                       gint      *minimum,
                                                       gint      *natural);
static void hildon_pannable_area_size_allocate (GtkWidget * widget,
                                                GtkAllocation * allocation);
static void hildon_pannable_area_child_allocate_calculate (GtkWidget * widget,
                                                           GtkAllocation * allocation,
                                                           GtkAllocation * child_allocation);
static void hildon_pannable_area_style_set (GtkWidget * widget,
                                            GtkStyle * previous_style);
static void hildon_pannable_area_grab_notify (GtkWidget *widget,
                                              gboolean was_grabbed,
                                              gpointer user_data);
static void rgb_from_gdkcolor (GdkColor *color, gdouble *r, gdouble *g, gdouble *b);
static void hildon_pannable_draw_vscroll (GtkWidget * widget,
                                          GdkColor *back_color,
                                          GdkColor *scroll_color);
static void hildon_pannable_draw_hscroll (GtkWidget * widget,
                                          GdkColor *back_color,
                                          GdkColor *scroll_color);
static void hildon_pannable_area_initial_effect (GtkWidget * widget);
static void hildon_pannable_area_redraw (HildonPannableArea * area);
static void hildon_pannable_area_launch_fade_timeout (HildonPannableArea * area,
                                                      gdouble alpha);
static void hildon_pannable_area_adjust_value_changed (HildonPannableArea * area,
                                                       gpointer data);
static void hildon_pannable_area_adjust_changed (HildonPannableArea * area,
                                                 gpointer data);
static gboolean hildon_pannable_area_scroll_indicator_fade(HildonPannableArea * area);
static gboolean hildon_pannable_area_draw (GtkWidget * widget,
                                           cairo_t   * cr);
static GdkWindow * hildon_pannable_area_get_topmost (GdkWindow * window,
                                                     gint x, gint y,
                                                     gint * tx, gint * ty,
                                                     GdkEventMask mask);
static void synth_crossing (GdkWindow * child,
                            GdkDevice * device,
                            gint x, gint y,
                            gint x_root, gint y_root,
                            guint32 time, gboolean in);
static gboolean hildon_pannable_area_button_press_cb (GtkWidget * widget,
                                                      GdkEventButton * event);
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
static void hildon_pannable_area_scroll (HildonPannableArea *area,
                                         gdouble x, gdouble y);
static gboolean hildon_pannable_area_timeout (HildonPannableArea * area);
static void hildon_pannable_area_calculate_velocity (gdouble *vel,
                                                     gdouble delta,
                                                     gdouble dist,
                                                     gdouble vmax,
                                                     gdouble drag_inertia,
                                                     gdouble force,
                                                     guint sps);
static gboolean hildon_pannable_area_motion_event_scroll_timeout (HildonPannableArea *area);
static void hildon_pannable_area_motion_event_scroll (HildonPannableArea *area,
                                                      gdouble x, gdouble y);
static void hildon_pannable_area_check_move (HildonPannableArea *area,
                                             GdkEventMotion * event,
                                             gdouble *x,
                                             gdouble *y);
static void hildon_pannable_area_handle_move (HildonPannableArea *area,
                                              GdkEventMotion * event,
                                              gdouble *x,
                                              gdouble *y);
static gboolean hildon_pannable_area_motion_notify_cb (GtkWidget * widget,
                                                       GdkEventMotion * event);
static gboolean hildon_pannable_leave_notify_event (GtkWidget *widget,
                                                    GdkEventCrossing *event);
static gboolean hildon_pannable_area_key_release_cb (GtkWidget * widget,
                                                     GdkEventKey * event);
static gboolean hildon_pannable_area_button_release_cb (GtkWidget * widget,
                                                        GdkEventButton * event);
static gboolean hildon_pannable_area_scroll_cb (GtkWidget *widget,
                                                GdkEventScroll *event);
static void hildon_pannable_area_child_mapped (GtkWidget *widget,
                                               GdkEvent  *event,
                                               gpointer user_data);
static void hildon_pannable_area_add (GtkContainer *container, GtkWidget *child);
static void hildon_pannable_area_remove (GtkContainer *container, GtkWidget *child);
static void hildon_pannable_calculate_vel_factor (HildonPannableArea * self);
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
  GObjectClass *gobject_class = (GObjectClass *) class;

  gobject_class->finalize     = hildon_pannable_area_finalize;
  gobject_class->dispose      = hildon_pannable_area_dispose;
  gobject_class->set_property = hildon_pannable_area_set_property;
  gobject_class->get_property = hildon_pannable_area_get_property;

  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);


  /*
  widget_class->realize = hildon_pannable_area_realize;
  widget_class->unrealize = hildon_pannable_area_unrealize;
  widget_class->size_allocate = hildon_pannable_area_size_allocate;
  widget_class->draw = hildon_pannable_area_draw;
  widget_class->style_set = hildon_pannable_area_style_set;
  widget_class->button_press_event = hildon_pannable_area_button_press_cb;
  widget_class->button_release_event = hildon_pannable_area_button_release_cb;
  widget_class->key_release_event = hildon_pannable_area_key_release_cb;
  widget_class->motion_notify_event = hildon_pannable_area_motion_notify_cb;
  widget_class->leave_notify_event = hildon_pannable_leave_notify_event;
  widget_class->scroll_event = hildon_pannable_area_scroll_cb;

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
  HildonPannableAreaPrivate *priv = HILDON_PANNABLE_AREA (object)->priv;

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
  HildonPannableAreaPrivate *priv = HILDON_PANNABLE_AREA (object)->priv;
  gboolean enabled;

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

  g_signal_handlers_disconnect_by_func (object,
                                        hildon_pannable_area_grab_notify,
                                        NULL);

  if (priv->hadjust) {
    g_signal_handlers_disconnect_by_func (priv->hadjust,
                                          hildon_pannable_area_adjust_value_changed,
                                          object);
    g_signal_handlers_disconnect_by_func (priv->hadjust,
                                          hildon_pannable_area_adjust_changed,
                                          object);
    g_object_unref (priv->hadjust);
    priv->hadjust = NULL;
  }

  if (priv->vadjust) {
    g_signal_handlers_disconnect_by_func (priv->vadjust,
                                          hildon_pannable_area_adjust_value_changed,
                                          object);
    g_signal_handlers_disconnect_by_func (priv->vadjust,
                                          hildon_pannable_area_adjust_changed,
                                          object);
    g_object_unref (priv->vadjust);
    priv->vadjust = NULL;
  }

  if (G_OBJECT_CLASS (hildon_pannable_area_parent_class)->dispose)
    G_OBJECT_CLASS (hildon_pannable_area_parent_class)->dispose (object);
}

static void
hildon_pannable_area_realize (GtkWidget * widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint border_width;
  HildonPannableAreaPrivate *priv;
  GtkAllocation allocation;

  priv = HILDON_PANNABLE_AREA (widget)->priv;

  gtk_widget_set_realized (widget, TRUE);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  gtk_widget_get_allocation (widget, &allocation);

  attributes.x = allocation.x + border_width;
  attributes.y = allocation.y + border_width;
  attributes.width = MAX (gtk_widget_get_allocated_width (widget) - 2 * border_width, 0);
  attributes.height = MAX (gtk_widget_get_allocated_height (widget) - 2 * border_width, 0);
  attributes.window_type = GDK_WINDOW_CHILD;

  /* avoid using the hildon_window */
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
  attributes.wclass = GDK_INPUT_OUTPUT;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  gtk_widget_set_window (widget,
                         gdk_window_new (gtk_widget_get_parent_window (widget),
                                         &attributes, attributes_mask));
  gdk_window_set_user_data (gtk_widget_get_window (widget), widget);

  /* create the events window */
  attributes.x = 0;
  attributes.y = 0;
  attributes.event_mask = gtk_widget_get_events (widget)
    | GDK_BUTTON_MOTION_MASK
    | GDK_BUTTON_PRESS_MASK
    | GDK_BUTTON_RELEASE_MASK
    | GDK_SCROLL_MASK
    | GDK_POINTER_MOTION_HINT_MASK
    | GDK_EXPOSURE_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK;
  attributes.wclass = GDK_INPUT_ONLY;

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  priv->event_window = gdk_window_new (gtk_widget_get_window (widget),
				       &attributes, attributes_mask);
  gdk_window_set_user_data (priv->event_window, widget);

  gtk_widget_style_attach (widget);
  gtk_style_set_background (gtk_widget_get_style (widget),
                            gtk_widget_get_window (widget), GTK_STATE_NORMAL);
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
hildon_pannable_area_size_allocate (GtkWidget * widget,
				    GtkAllocation * allocation)
{
  GtkAllocation child_allocation;
  HildonPannableAreaPrivate *priv;
  GtkWidget *child = gtk_bin_get_child (GTK_BIN (widget));
  gint border_width;
  gdouble hv, vv;

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  gtk_widget_set_allocation (widget, allocation);

  priv = HILDON_PANNABLE_AREA (widget)->priv;

  if (gtk_widget_get_realized (widget)) {
      gdk_window_move_resize (gtk_widget_get_window (widget),
			      allocation->x + border_width,
			      allocation->y  + border_width,
			      allocation->width  - border_width * 2,
			      allocation->height - border_width * 2);
      gdk_window_move_resize (priv->event_window,
			      0,
			      0,
			      allocation->width  - border_width * 2,
			      allocation->height - border_width * 2);
  }

  if (child && gtk_widget_get_visible (child)) {

    hildon_pannable_area_check_scrollbars (HILDON_PANNABLE_AREA (widget));

    hildon_pannable_area_child_allocate_calculate (widget,
                                                   allocation,
                                                   &child_allocation);

    gtk_widget_size_allocate (child, &child_allocation);

    if (hildon_pannable_area_check_scrollbars (HILDON_PANNABLE_AREA (widget))) {
      hildon_pannable_area_child_allocate_calculate (widget,
                                                     allocation,
                                                     &child_allocation);

      gtk_widget_size_allocate (child, &child_allocation);
    }

    if (gtk_adjustment_get_page_size (priv->vadjust) >= 0) {
      priv->accel_vel_y = MIN (priv->vmax,
                               gtk_adjustment_get_upper (priv->vadjust) / gtk_adjustment_get_page_size (priv->vadjust) * ACCEL_FACTOR);
      priv->accel_vel_x = MIN (priv->vmax,
                               gtk_adjustment_get_upper (priv->hadjust) / gtk_adjustment_get_page_size (priv->hadjust) * ACCEL_FACTOR);
    }

    hv = gtk_adjustment_get_value (priv->hadjust);
    vv = gtk_adjustment_get_value (priv->vadjust);

    /* we have to do this after child size_allocate because page_size is
     * changed when we allocate the size of the children */
    if (priv->overshot_dist_y < 0) {
      gtk_adjustment_set_value (priv->vadjust, gtk_adjustment_get_upper (priv->vadjust) - gtk_adjustment_get_page_size (priv->vadjust));
    }

    if (priv->overshot_dist_x < 0) {
      gtk_adjustment_set_value (priv->hadjust, gtk_adjustment_get_upper (priv->hadjust) - gtk_adjustment_get_page_size (priv->hadjust));
    }

    if (hv != gtk_adjustment_get_value (priv->hadjust))
      gtk_adjustment_value_changed (priv->hadjust);

    if (vv != gtk_adjustment_get_value (priv->vadjust))
      gtk_adjustment_value_changed (priv->vadjust);

  } else {
    hildon_pannable_area_check_scrollbars (HILDON_PANNABLE_AREA (widget));
  }
}

static void
hildon_pannable_area_style_set (GtkWidget * widget,
                                GtkStyle * previous_style)
{
  HildonPannableAreaPrivate *priv = HILDON_PANNABLE_AREA (widget)->priv;

  GTK_WIDGET_CLASS (hildon_pannable_area_parent_class)->
    style_set (widget, previous_style);

  gtk_style_lookup_color (gtk_widget_get_style (widget), "SecondaryTextColor", &priv->scroll_color);
  gtk_widget_style_get (widget, "indicator-width", &priv->indicator_width, NULL);
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
rgb_from_gdkcolor (GdkColor *color, gdouble *r, gdouble *g, gdouble *b)
{
  *r = (color->red >> 8) / 255.0;
  *g = (color->green >> 8) / 255.0;
  *b = (color->blue >> 8) / 255.0;
}

static void
hildon_pannable_draw_vscroll (GtkWidget * widget,
                              GdkColor *back_color,
                              GdkColor *scroll_color)
{
  HildonPannableAreaPrivate *priv = HILDON_PANNABLE_AREA (widget)->priv;
  gfloat y, height;
  cairo_t *cr;
  cairo_pattern_t *pattern;
  gdouble r, g, b;
  gint radius = (priv->vscroll_rect.width/2) - 1;

  cr = gdk_cairo_create(gtk_widget_get_window (widget));

  /* Draw the background */
  rgb_from_gdkcolor (back_color, &r, &g, &b);
  cairo_set_source_rgb (cr, r, g, b);
  cairo_rectangle(cr, priv->vscroll_rect.x, priv->vscroll_rect.y,
                  priv->vscroll_rect.width,
                  priv->vscroll_rect.height);
  cairo_fill_preserve (cr);
  cairo_clip (cr);

  /* Calculate the scroll bar height and position */
  y = ((gtk_adjustment_get_value (priv->vadjust) - gtk_adjustment_get_lower (priv->vadjust)) / (gtk_adjustment_get_upper (priv->vadjust) - gtk_adjustment_get_lower (priv->vadjust))) *
    (gtk_widget_get_allocated_height (widget) -
     (priv->hscroll_visible ? priv->indicator_width : 0));
  height = ((((gtk_adjustment_get_value (priv->vadjust) - gtk_adjustment_get_lower (priv->vadjust)) +
              gtk_adjustment_get_page_size (priv->vadjust)) /
             (gtk_adjustment_get_upper (priv->vadjust) - gtk_adjustment_get_lower (priv->vadjust))) *
            (gtk_widget_get_allocated_height (widget) -
             (priv->hscroll_visible ? priv->indicator_width : 0))) - y;

  /* Set a minimum height */
  height = MAX (SCROLL_BAR_MIN_SIZE, height);

  /* Check the max y position */
  y = MIN (y, gtk_widget_get_allocated_height (widget) -
           (priv->hscroll_visible ? priv->hscroll_rect.height : 0) -
           height);

  /* Draw the scrollbar */
  rgb_from_gdkcolor (scroll_color, &r, &g, &b);

  pattern = cairo_pattern_create_linear(radius+1, y, radius+1,y + height);
  cairo_pattern_add_color_stop_rgb(pattern, 0, r, g, b);
  cairo_pattern_add_color_stop_rgb(pattern, 1, r/2, g/2, b/2);
  cairo_set_source(cr, pattern);
  cairo_fill(cr);
  cairo_pattern_destroy(pattern);

  cairo_arc(cr, priv->vscroll_rect.x + radius + 1, y + radius + 1, radius, G_PI, 0);
  cairo_line_to(cr, priv->vscroll_rect.x + (radius * 2) + 1, y + height - radius);
  cairo_arc(cr, priv->vscroll_rect.x + radius + 1, y + height - radius, radius, 0, G_PI);
  cairo_line_to(cr, priv->vscroll_rect.x + 1, y + height - radius);
  cairo_clip (cr);

  cairo_paint_with_alpha(cr, priv->scroll_indicator_alpha);

  cairo_destroy(cr);
}

static void
hildon_pannable_draw_hscroll (GtkWidget * widget,
                              GdkColor *back_color,
                              GdkColor *scroll_color)
{
  HildonPannableAreaPrivate *priv = HILDON_PANNABLE_AREA (widget)->priv;
  gfloat x, width;
  cairo_t *cr;
  cairo_pattern_t *pattern;
  gdouble r, g, b;
  gint radius = (priv->hscroll_rect.height/2) - 1;

  cr = gdk_cairo_create(gtk_widget_get_window (widget));

  /* Draw the background */
  rgb_from_gdkcolor (back_color, &r, &g, &b);
  cairo_set_source_rgb (cr, r, g, b);
  cairo_rectangle(cr, priv->hscroll_rect.x, priv->hscroll_rect.y,
                  priv->hscroll_rect.width,
                  priv->hscroll_rect.height);
  cairo_fill_preserve (cr);
  cairo_clip (cr);

  /* calculate the scrollbar width and position */
  x = ((gtk_adjustment_get_value (priv->hadjust) - gtk_adjustment_get_lower (priv->hadjust)) / (gtk_adjustment_get_upper (priv->hadjust) - gtk_adjustment_get_lower (priv->hadjust))) *
    (gtk_widget_get_allocated_width (widget) - (priv->vscroll_visible ? priv->indicator_width : 0));
  width =((((gtk_adjustment_get_value (priv->hadjust) - gtk_adjustment_get_lower (priv->hadjust)) +
            gtk_adjustment_get_page_size (priv->hadjust)) / (gtk_adjustment_get_upper (priv->hadjust) - gtk_adjustment_get_lower (priv->hadjust))) *
          (gtk_widget_get_allocated_width (widget) -
           (priv->vscroll_visible ? priv->indicator_width : 0))) - x;

  /* Set a minimum width */
  width = MAX (SCROLL_BAR_MIN_SIZE, width);

  /* Check the max x position */
  x = MIN (x, gtk_widget_get_allocated_width (widget) -
           (priv->vscroll_visible ? priv->vscroll_rect.width : 0) -
           width);

  /* Draw the scrollbar */
  rgb_from_gdkcolor (scroll_color, &r, &g, &b);

  pattern = cairo_pattern_create_linear(x, radius+1, x+width, radius+1);
  cairo_pattern_add_color_stop_rgb(pattern, 0, r, g, b);
  cairo_pattern_add_color_stop_rgb(pattern, 1, r/2, g/2, b/2);
  cairo_set_source(cr, pattern);
  cairo_fill(cr);
  cairo_pattern_destroy(pattern);

  cairo_arc_negative(cr, x + radius + 1, priv->hscroll_rect.y + radius + 1, radius, 3*G_PI_2, G_PI_2);
  cairo_line_to(cr, x + width - radius, priv->hscroll_rect.y + (radius * 2) + 1);
  cairo_arc_negative(cr, x + width - radius, priv->hscroll_rect.y + radius + 1, radius, G_PI_2, 3*G_PI_2);
  cairo_line_to(cr, x + width - radius, priv->hscroll_rect.y + 1);
  cairo_clip (cr);

  cairo_paint_with_alpha(cr, priv->scroll_indicator_alpha);

  cairo_destroy(cr);
}

static gboolean
launch_fade_in_timeout (GtkWidget * widget)
{
  HildonPannableAreaPrivate *priv = HILDON_PANNABLE_AREA (widget)->priv;
  priv->scroll_indicator_timeout =
      gdk_threads_add_timeout_full (G_PRIORITY_HIGH_IDLE + 20,
                                    SCROLL_FADE_IN_TIMEOUT,
                                    (GSourceFunc) hildon_pannable_area_scroll_indicator_fade,
                                    widget,
                                    NULL);
  return FALSE;
}

static void
hildon_pannable_area_initial_effect (GtkWidget * widget)
{
  HildonPannableAreaPrivate *priv = HILDON_PANNABLE_AREA (widget)->priv;

  if (priv->initial_hint) {
    if (priv->vscroll_visible || priv->hscroll_visible) {

      priv->fade_in = TRUE;
      priv->scroll_indicator_alpha = 0.0;
      priv->scroll_indicator_event_interrupt = 0;
      priv->scroll_delay_counter = 2000 / SCROLL_FADE_TIMEOUT; /* 2 seconds before fade-out */

      priv->scroll_indicator_timeout =
          gdk_threads_add_timeout (300, (GSourceFunc) launch_fade_in_timeout, widget);
    }
  }
}

static void
hildon_pannable_area_launch_fade_timeout (HildonPannableArea * area,
                                          gdouble alpha)
{
  HildonPannableAreaPrivate *priv = HILDON_PANNABLE_AREA (area)->priv;

  priv->scroll_indicator_alpha = alpha;
  priv->fade_in = FALSE;

  if (!priv->scroll_indicator_timeout)
    priv->scroll_indicator_timeout =
      gdk_threads_add_timeout_full (G_PRIORITY_HIGH_IDLE + 20,
				    SCROLL_FADE_TIMEOUT,
				    (GSourceFunc) hildon_pannable_area_scroll_indicator_fade,
				    area,
				    NULL);
}

static void
hildon_pannable_area_adjust_changed (HildonPannableArea * area,
                                     gpointer data)
{
  if (gtk_widget_get_realized (GTK_WIDGET (area)))
    hildon_pannable_area_refresh (area);
}

static void
hildon_pannable_area_adjust_value_changed (HildonPannableArea * area,
                                           gpointer data)
{
  HildonPannableAreaPrivate *priv = HILDON_PANNABLE_AREA (area)->priv;
  gint xdiff, ydiff;
  gint x = priv->x_offset;
  gint y = priv->y_offset;

  priv->x_offset = gtk_adjustment_get_value (priv->hadjust);
  xdiff = x - priv->x_offset;
  priv->y_offset = gtk_adjustment_get_value (priv->vadjust);
  ydiff = y - priv->y_offset;

  if ((xdiff || ydiff) && gtk_widget_is_drawable (GTK_WIDGET (area))) {
    hildon_pannable_area_redraw (area);

    if ((priv->vscroll_visible) || (priv->hscroll_visible)) {
      priv->scroll_indicator_event_interrupt = 0;
      priv->scroll_delay_counter = priv->scrollbar_fade_delay;

      hildon_pannable_area_launch_fade_timeout (area, 1.0);
    }
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

static gboolean
hildon_pannable_area_scroll_indicator_fade(HildonPannableArea * area)
{
  HildonPannableAreaPrivate *priv = area->priv;

  /* if moving do not fade out */
  if (((ABS (priv->vel_y)>priv->vmin)||
       (ABS (priv->vel_x)>priv->vmin))&&(!priv->button_pressed)) {

    return TRUE;
  }

  if (priv->scroll_indicator_event_interrupt || priv->fade_in) {
    if (priv->scroll_indicator_alpha > 0.9) {
      priv->scroll_indicator_alpha = 1.0;
      priv->scroll_indicator_timeout = 0;

      if (priv->fade_in)
        hildon_pannable_area_launch_fade_timeout (area, 1.0);

      return FALSE;
    } else {
      priv->scroll_indicator_alpha += 0.2;
      hildon_pannable_area_redraw (area);

      return TRUE;
    }
  }

  if ((priv->scroll_indicator_alpha > 0.9) &&
      (priv->scroll_delay_counter > 0)) {
    priv->scroll_delay_counter--;

    return TRUE;
  }

  if (!priv->scroll_indicator_event_interrupt) {
    /* Continue fade out */
    if (priv->scroll_indicator_alpha < 0.1) {
      priv->scroll_indicator_timeout = 0;
      priv->scroll_indicator_alpha = 0.0;

      return FALSE;
    } else {
      priv->scroll_indicator_alpha -= 0.2;
      hildon_pannable_area_redraw (area);

      return TRUE;
    }
  }

  return TRUE;
}

static gboolean
hildon_pannable_area_draw (GtkWidget * widget,
                           cairo_t   * cr)
{

  HildonPannableAreaPrivate *priv = HILDON_PANNABLE_AREA (widget)->priv;
  GdkColor back_color = gtk_widget_get_style (widget)->bg[GTK_STATE_NORMAL];
  GdkColor scroll_color = gtk_widget_get_style (widget)->base[GTK_STATE_SELECTED];

  if (G_UNLIKELY (priv->initial_effect)) {
    hildon_pannable_area_initial_effect (widget);

    priv->initial_effect = FALSE;
  }

  if (gtk_bin_get_child (GTK_BIN (widget))) {

    if (priv->scroll_indicator_alpha > 0.1) {
      if (priv->vscroll_visible) {
        hildon_pannable_draw_vscroll (widget, &back_color, &scroll_color);
      }
      if (priv->hscroll_visible) {
        hildon_pannable_draw_hscroll (widget, &back_color, &scroll_color);
      }
    }

    /* draw overshooting rectangles */
    if (priv->overshot_dist_y > 0) {
      gint overshot_height;

      overshot_height = MIN (priv->overshot_dist_y, gtk_widget_get_allocated_height (widget) -
                             (priv->hscroll_visible ? priv->hscroll_rect.height : 0));

      gtk_style_apply_default_background  (gtk_widget_get_style (widget), cr,
                                           gtk_widget_get_window (widget), GTK_STATE_NORMAL,
                                           0, 0,
                                           gtk_widget_get_allocated_width (widget) -
                                           (priv->vscroll_visible ? priv->vscroll_rect.width : 0),
                                           overshot_height);
    } else if (priv->overshot_dist_y < 0) {
      gint overshot_height;
      gint overshot_y;

      overshot_height =
        MAX (priv->overshot_dist_y,
             -(gtk_widget_get_allocated_height (widget) -
               (priv->hscroll_visible ? priv->hscroll_rect.height : 0)));

      overshot_y = MAX (gtk_widget_get_allocated_height (widget) +
                        overshot_height -
                        (priv->hscroll_visible ? priv->hscroll_rect.height : 0), 0);

      gtk_style_apply_default_background  (gtk_widget_get_style (widget), cr,
                                           gtk_widget_get_window (widget), GTK_STATE_NORMAL,
                                           0, overshot_y,
                                           gtk_widget_get_allocated_width (widget) -
                                           priv->vscroll_rect.width,
                                           -overshot_height);
    }

    if (priv->overshot_dist_x > 0) {
      gint overshot_width;

      overshot_width = MIN (priv->overshot_dist_x, gtk_widget_get_allocated_width (widget) -
                             (priv->vscroll_visible ? priv->vscroll_rect.width : 0));

      gtk_style_apply_default_background  (gtk_widget_get_style (widget), cr,
                                           gtk_widget_get_window (widget), GTK_STATE_NORMAL,
                                           0, 0,
                                           overshot_width,
                                           gtk_widget_get_allocated_height (widget) -
                                           (priv->hscroll_visible ? priv->hscroll_rect.height : 0));
    } else if (priv->overshot_dist_x < 0) {
      gint overshot_width;
      gint overshot_x;

      overshot_width =
        MAX (priv->overshot_dist_x,
             -(gtk_widget_get_allocated_width (widget) -
               (priv->vscroll_visible ? priv->vscroll_rect.width : 0)));

      overshot_x = MAX (gtk_widget_get_allocated_width (widget) +
                        overshot_width -
                        (priv->vscroll_visible ? priv->vscroll_rect.width : 0), 0);

      gtk_style_apply_default_background  (gtk_widget_get_style (widget), cr,
                                           gtk_widget_get_window (widget), GTK_STATE_NORMAL,
                                           overshot_x, 0,
                                           -overshot_width,
                                           gtk_widget_get_allocated_height (widget) -
                                           priv->hscroll_rect.height);
    }

  }

  return GTK_WIDGET_CLASS (hildon_pannable_area_parent_class)->draw (widget, cr);
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
synth_crossing (GdkWindow * child,
                GdkDevice * device,
		gint x, gint y,
		gint x_root, gint y_root,
                guint32 time, gboolean in)
{
  GdkEvent *event;
  GdkEventType type = in ? GDK_ENTER_NOTIFY : GDK_LEAVE_NOTIFY;

  /* Send synthetic enter event */
  event = gdk_event_new (type);
  gdk_event_set_device(event, device);
  event->any.type = type;
  event->any.window = g_object_ref (child);
  event->any.send_event = FALSE;
  event->crossing.subwindow = g_object_ref (child);
  event->crossing.time = time;
  event->crossing.x = x;
  event->crossing.y = y;
  event->crossing.x_root = x_root;
  event->crossing.y_root = y_root;
  event->crossing.mode = GDK_CROSSING_NORMAL;
  event->crossing.detail = GDK_NOTIFY_UNKNOWN;
  event->crossing.focus = FALSE;
  event->crossing.state = 0;
  gdk_event_put (event);
  gdk_event_free (event);
}

static gboolean
hildon_pannable_area_button_press_cb (GtkWidget * widget,
				      GdkEventButton * event)
{
  gint x, y;
  HildonPannableArea *area = HILDON_PANNABLE_AREA (widget);
  HildonPannableAreaPrivate *priv = area->priv;
  GdkDevice *device = gdk_event_get_device((GdkEvent *)event);

  priv->selection_movement =
      (event->state & GDK_SHIFT_MASK) &&
      (event->time == priv->last_time) &&
      (priv->last_type == 1);

  if ((!priv->enabled) || (event->button != 1) || (priv->selection_movement) ||
      ((event->time == priv->last_time) &&
       (priv->last_type == 1)) ||
      (gtk_bin_get_child (GTK_BIN (widget)) == NULL))
    return TRUE;

  priv->scroll_indicator_event_interrupt = 1;

  hildon_pannable_area_launch_fade_timeout (area,
                                            priv->scroll_indicator_alpha);

  priv->last_time = event->time;
  priv->last_press_time = event->time;
  priv->last_type = 1;

  priv->scroll_to_x = -1;
  priv->scroll_to_y = -1;

  if (priv->button_pressed && priv->child) {
    /* Widget stole focus on last click, send crossing-out event */
    synth_crossing (priv->child, device, 0, 0, event->x_root, event->y_root,
		    event->time, FALSE);
  }

  priv->x = event->x;
  priv->y = event->y;
  priv->ix = priv->x;
  priv->iy = priv->y;

  /* Don't allow a click if we're still moving fast */
  if ((ABS (priv->vel_x) <= (priv->vmax * priv->vfast_factor)) &&
      (ABS (priv->vel_y) <= (priv->vmax * priv->vfast_factor)))
    priv->child =
      hildon_pannable_area_get_topmost (
					gtk_widget_get_window (gtk_bin_get_child (GTK_BIN (widget))),
					event->x, event->y, &x, &y, GDK_BUTTON_PRESS_MASK);
  else
    priv->child = NULL;

  priv->button_pressed = TRUE;

  /* Stop scrolling on mouse-down (so you can flick, then hold to stop) */
  priv->old_vel_x = priv->vel_x;
  priv->old_vel_y = priv->vel_y;
  priv->vel_x = 0;
  priv->vel_y = 0;
  if (priv->idle_id) {
    g_source_remove (priv->idle_id);
    priv->idle_id = 0;
    g_signal_emit (area, pannable_area_signals[PANNING_FINISHED], 0);
  }

  if (priv->child) {

    priv->child_width = gdk_window_get_width (priv->child);
    priv->child_height = gdk_window_get_height (priv->child);
    priv->last_in = TRUE;

    g_object_add_weak_pointer ((GObject *) priv->child,
			       (gpointer) & priv->child);

    synth_crossing (priv->child, device, x, y, event->x_root,
		    event->y_root, event->time, TRUE);

    /* Avoid reinjecting the event to create an infinite loop */
    if (priv->event_window == ((GdkEvent*) event)->any.window) {
      event = (GdkEventButton *) gdk_event_copy ((GdkEvent *) event);
      /* remove the reference we added with the copy */
      g_object_unref (((GdkEvent*) event)->any.window);
      event->x = x;
      event->y = y;
      priv->cx = x;
      priv->cy = y;

      /* Send synthetic click (button press/release) event */
      ((GdkEvent*) event)->any.window = g_object_ref (priv->child);

      gdk_event_put ((GdkEvent *) event);
      gdk_event_free ((GdkEvent *) event);
    }
  } else
    priv->child = NULL;

  return TRUE;
}

static gboolean
hildon_pannable_area_check_scrollbars (HildonPannableArea * area)
{
  HildonPannableAreaPrivate *priv = area->priv;
  gboolean prev_hscroll_visible, prev_vscroll_visible;
  GtkAllocation *allocation = NULL;

  prev_hscroll_visible = priv->hscroll_visible;
  prev_vscroll_visible = priv->vscroll_visible;

  if (!gtk_bin_get_child (GTK_BIN (area))) {
    priv->vscroll_visible = FALSE;
    priv->hscroll_visible = FALSE;
  } else {
    switch (priv->hscrollbar_policy) {
    case GTK_POLICY_ALWAYS:
      priv->hscroll_visible = TRUE;
      break;
    case GTK_POLICY_NEVER:
      priv->hscroll_visible = FALSE;
      break;
    default:
      priv->hscroll_visible = (gtk_adjustment_get_upper (priv->hadjust) - gtk_adjustment_get_lower (priv->hadjust) >
                               gtk_adjustment_get_page_size (priv->hadjust));
    }

    switch (priv->vscrollbar_policy) {
    case GTK_POLICY_ALWAYS:
      priv->vscroll_visible = TRUE;
      break;
    case GTK_POLICY_NEVER:
      priv->vscroll_visible = FALSE;
      break;
    default:
      priv->vscroll_visible = (gtk_adjustment_get_upper (priv->vadjust) - gtk_adjustment_get_lower (priv->vadjust) >
                               gtk_adjustment_get_page_size (priv->vadjust));
    }

    /* Store the vscroll/hscroll areas for redrawing */
    if (priv->vscroll_visible) {
      gtk_widget_get_allocation (GTK_WIDGET (area), allocation);
      priv->vscroll_rect.x = allocation->width - priv->indicator_width;
      priv->vscroll_rect.y = 0;
      priv->vscroll_rect.width = priv->indicator_width;
      priv->vscroll_rect.height = allocation->height -
        (priv->hscroll_visible ? priv->indicator_width : 0);
    }
    if (priv->hscroll_visible) {
      gtk_widget_get_allocation (GTK_WIDGET (area), allocation);
      priv->hscroll_rect.y = allocation->height - priv->indicator_width;
      priv->hscroll_rect.x = 0;
      priv->hscroll_rect.height = priv->indicator_width;
      priv->hscroll_rect.width = allocation->width -
        (priv->vscroll_visible ? priv->indicator_width : 0);
    }
  }

  return ((priv->hscroll_visible != prev_hscroll_visible) ||
          (priv->vscroll_visible != prev_vscroll_visible));
}

static void
hildon_pannable_area_refresh (HildonPannableArea * area)
{
  if (gtk_widget_is_drawable (GTK_WIDGET (area)) &&
      hildon_pannable_area_check_scrollbars (area)) {
    HildonPannableAreaPrivate *priv = area->priv;

    gtk_widget_queue_resize (GTK_WIDGET (area));

    if ((priv->vscroll_visible || priv->hscroll_visible) && G_UNLIKELY (!priv->initial_effect)) {
      priv->scroll_indicator_event_interrupt = 0;
      priv->scroll_delay_counter = area->priv->scrollbar_fade_delay;

      hildon_pannable_area_launch_fade_timeout (area, 1.0);
    }
  } else {
    hildon_pannable_area_redraw (area);
  }
}

/* Scroll by a particular amount (in pixels). Optionally, return if
 * the scroll on a particular axis was successful.
 */
static void
hildon_pannable_axis_scroll (HildonPannableArea *area,
                             GtkAdjustment *adjust,
                             gdouble *vel,
                             gdouble inc,
                             gint *overshooting,
                             gint *overshot_dist,
                             gdouble *scroll_to,
                             gint overshoot_max,
                             gboolean *s)
{
  gdouble dist;
  HildonPannableAreaPrivate *priv = area->priv;

  dist = gtk_adjustment_get_value (adjust) - inc;

  /* Overshooting
   * We use overshot_dist to define the distance of the current overshoot,
   * and overshooting to define the direction/whether or not we are overshot
   */
  if (!(*overshooting)) {

    /* Initiation of the overshoot happens when the finger is released
     * and the current position of the pannable contents are out of range
     */
    if (dist < gtk_adjustment_get_lower (adjust)) {
      if (s) *s = FALSE;

      dist = gtk_adjustment_get_lower (adjust);

      if (overshoot_max!=0) {
        *overshooting = 1;
        *scroll_to = -1;
        *overshot_dist = CLAMP (*overshot_dist + *vel, 0, overshoot_max);
        *vel = MIN (priv->vmax_overshooting, *vel);
        gtk_widget_queue_resize (GTK_WIDGET (area));
      } else {
        *vel = 0.0;
        *scroll_to = -1;
      }
    } else if (dist > gtk_adjustment_get_upper (adjust) - gtk_adjustment_get_page_size (adjust)) {
      if (s) *s = FALSE;

      dist = gtk_adjustment_get_upper (adjust) - gtk_adjustment_get_page_size (adjust);

      if (overshoot_max!=0) {
        *overshooting = 1;
        *scroll_to = -1;
        *overshot_dist = CLAMP (*overshot_dist + *vel, -overshoot_max, 0);
        *vel = MAX (-priv->vmax_overshooting, *vel);
        gtk_widget_queue_resize (GTK_WIDGET (area));
      } else {
        *vel = 0.0;
        *scroll_to = -1;
      }
    } else {
      if ((*scroll_to) != -1) {
        if (((inc < 0)&&(*scroll_to <= dist))||
            ((inc > 0)&&(*scroll_to >= dist))) {
          dist = *scroll_to;
          *scroll_to = -1;
          *vel = 0;
        }
      }
    }

    gtk_adjustment_set_value (adjust, dist);
  } else {
    if (!priv->button_pressed) {

      /* When the overshoot has started we continue for
       * PROP_BOUNCE_STEPS more steps into the overshoot before we
       * reverse direction. The deceleration factor is calculated
       * based on the percentage distance from the first item with
       * each iteration, therefore always returning us to the
       * top/bottom most element
       */
      if (*overshot_dist > 0) {

        if ((*overshooting < priv->bounce_steps) && (*vel > 0)) {
          (*overshooting)++;
          *vel = (((gdouble)*overshot_dist)/overshoot_max) * (*vel);
        } else if ((*overshooting >= priv->bounce_steps) && (*vel > 0)) {
          *vel *= -1;
        } else if ((*overshooting > 1) && (*vel < 0)) {
          /* we add the MIN in order to avoid very small speeds */
          *vel = MIN (((((gdouble)*overshot_dist)*0.8) * -1), -10.0);
        }

        *overshot_dist = CLAMP (*overshot_dist + *vel, 0, overshoot_max);

        gtk_widget_queue_resize (GTK_WIDGET (area));

      } else if (*overshot_dist < 0) {

        if ((*overshooting < priv->bounce_steps) && (*vel < 0)) {
          (*overshooting)++;
          *vel = (((gdouble)*overshot_dist)/overshoot_max) * (*vel) * -1;
        } else if ((*overshooting >= priv->bounce_steps) && (*vel < 0)) {
          *vel *= -1;
        } else if ((*overshooting > 1) && (*vel > 0)) {
          /* we add the MAX in order to avoid very small speeds */
          *vel = MAX (((((gdouble)*overshot_dist)*0.8) * -1), 10.0);
        }

        *overshot_dist = CLAMP (*overshot_dist + (*vel), -overshoot_max, 0);

        gtk_widget_queue_resize (GTK_WIDGET (area));

      } else {
        *overshooting = 0;
        *vel = 0;
        gtk_widget_queue_resize (GTK_WIDGET (area));
      }
    } else {

      gint overshot_dist_old = *overshot_dist;

      if (*overshot_dist > 0) {
        *overshot_dist = CLAMP ((*overshot_dist) + inc, 0, overshoot_max);
      } else if (*overshot_dist < 0) {
        *overshot_dist = CLAMP ((*overshot_dist) + inc, -1 * overshoot_max, 0);
      } else {
        *overshooting = 0;
        gtk_adjustment_set_value (adjust,
                                  CLAMP (dist,
                                         gtk_adjustment_get_lower (adjust),
                                         gtk_adjustment_get_upper (adjust) -
                                         gtk_adjustment_get_page_size (adjust)));
      }

      if (*overshot_dist != overshot_dist_old)
        gtk_widget_queue_resize (GTK_WIDGET (area));
    }
  }
}

static void
hildon_pannable_area_scroll (HildonPannableArea *area,
                             gdouble x, gdouble y)
{
  gboolean sx, sy;
  HildonPannableAreaPrivate *priv = area->priv;
  gboolean hscroll_visible, vscroll_visible;
  gdouble hv, vv;

  if (gtk_bin_get_child (GTK_BIN (area)) == NULL)
    return;

  vscroll_visible = (gtk_adjustment_get_upper (priv->vadjust) - gtk_adjustment_get_lower (priv->vadjust) >
	     gtk_adjustment_get_page_size (priv->vadjust));
  hscroll_visible = (gtk_adjustment_get_upper (priv->hadjust) - gtk_adjustment_get_lower (priv->hadjust) >
	     gtk_adjustment_get_page_size (priv->hadjust));

  sx = TRUE;
  sy = TRUE;

  hv = gtk_adjustment_get_value (priv->hadjust);
  vv = gtk_adjustment_get_value (priv->vadjust);

  if (vscroll_visible) {
    hildon_pannable_axis_scroll (area, priv->vadjust, &priv->vel_y, y,
                                 &priv->overshooting_y, &priv->overshot_dist_y,
                                 &priv->scroll_to_y, priv->vovershoot_max, &sy);
  } else {
    priv->vel_y = 0.0;
    priv->scroll_to_y = -1;
  }

  if (hscroll_visible) {
    hildon_pannable_axis_scroll (area, priv->hadjust, &priv->vel_x, x,
                                 &priv->overshooting_x, &priv->overshot_dist_x,
                                 &priv->scroll_to_x, priv->hovershoot_max, &sx);
  } else {
    priv->vel_x = 0.0;
    priv->scroll_to_x = -1;
  }

  if (hv != gtk_adjustment_get_value (priv->hadjust))
    gtk_adjustment_value_changed (priv->hadjust);

  if (vv != gtk_adjustment_get_value (priv->vadjust))
    gtk_adjustment_value_changed (priv->vadjust);

  /* If the scroll on a particular axis wasn't succesful, reset the
   * initial scroll position to the new mouse co-ordinate. This means
   * when you get to the top of the page, dragging down works immediately.
   */
  if (priv->mode == HILDON_PANNABLE_AREA_MODE_ACCEL) {
      if (!sx) {
        priv->x = priv->ex;
      }

      if (!sy) {
        priv->y = priv->ey;
      }
    }
}

static gboolean
hildon_pannable_area_timeout (HildonPannableArea * area)
{
  HildonPannableAreaPrivate *priv = area->priv;

  if ((!priv->enabled) || (priv->mode == HILDON_PANNABLE_AREA_MODE_PUSH)) {
    priv->idle_id = 0;
    g_signal_emit (area, pannable_area_signals[PANNING_FINISHED], 0);

    return FALSE;
  }

  hildon_pannable_area_scroll (area, priv->vel_x, priv->vel_y);

  gdk_window_process_updates (gtk_widget_get_window (GTK_WIDGET (area)), FALSE);

  if (!priv->button_pressed) {
    /* Decelerate gradually when pointer is raised */
    if ((!priv->overshot_dist_y) &&
        (!priv->overshot_dist_x)) {

      /* in case we move to a specific point do not decelerate when arriving */
      if ((priv->scroll_to_x != -1)||(priv->scroll_to_y != -1)) {

        if (ABS (priv->vel_x) >= 1.5) {
          priv->vel_x *= priv->decel;
        }

        if (ABS (priv->vel_y) >= 1.5) {
          priv->vel_y *= priv->decel;
        }

      } else {
        if ((!priv->low_friction_mode) ||
            ((priv->mov_mode&HILDON_MOVEMENT_MODE_HORIZ) &&
             (ABS (priv->vel_x) < 0.8*priv->vmax)))
          priv->vel_x *= priv->decel;

        if ((!priv->low_friction_mode) ||
            ((priv->mov_mode&HILDON_MOVEMENT_MODE_VERT) &&
             (ABS (priv->vel_y) < 0.8*priv->vmax)))
          priv->vel_y *= priv->decel;

        if ((ABS (priv->vel_x) < 1.0) && (ABS (priv->vel_y) < 1.0)) {
          priv->vel_x = 0;
          priv->vel_y = 0;
          priv->idle_id = 0;

          g_signal_emit (area, pannable_area_signals[PANNING_FINISHED], 0);

          return FALSE;
        }
      }
    }
  } else if (priv->mode == HILDON_PANNABLE_AREA_MODE_AUTO) {
    priv->idle_id = 0;

    return FALSE;
  }

  return TRUE;
}

static void
hildon_pannable_area_calculate_velocity (gdouble *vel,
                                         gdouble delta,
                                         gdouble dist,
                                         gdouble vmax,
                                         gdouble drag_inertia,
                                         gdouble force,
                                         guint sps)
{
  gdouble rawvel;

  if (ABS (dist) >= RATIO_TOLERANCE) {
    rawvel = (dist / ABS (delta)) * force;
    *vel = *vel * (1 - drag_inertia) +
      rawvel * drag_inertia;
    *vel = *vel > 0 ? MIN (*vel, vmax)
      : MAX (*vel, -1 * vmax);
  }
}

static gboolean
hildon_pannable_area_motion_event_scroll_timeout (HildonPannableArea *area)
{
  HildonPannableAreaPrivate *priv = area->priv;

  if ((priv->motion_x != 0)||(priv->motion_y != 0))
    hildon_pannable_area_scroll (area, priv->motion_x, priv->motion_y);

  priv->motion_event_scroll_timeout = 0;

  return FALSE;
}

static void
hildon_pannable_area_motion_event_scroll (HildonPannableArea *area,
                                          gdouble x, gdouble y)
{
  HildonPannableAreaPrivate *priv = area->priv;

  if (priv->motion_event_scroll_timeout) {

    priv->motion_x += x;
    priv->motion_y += y;

  } else {

  /* we do not delay the first event but the next ones */
    hildon_pannable_area_scroll (area, x, y);

    priv->motion_x = 0;
    priv->motion_y = 0;

    priv->motion_event_scroll_timeout = gdk_threads_add_timeout_full
      (G_PRIORITY_HIGH_IDLE + 20,
       (gint) (1000.0 / (gdouble) MOTION_EVENTS_PER_SECOND),
       (GSourceFunc) hildon_pannable_area_motion_event_scroll_timeout, area, NULL);
  }
}

static void
hildon_pannable_area_check_move (HildonPannableArea *area,
                                 GdkEventMotion * event,
                                 gdouble *x,
                                 gdouble *y)
{
  HildonPannableAreaPrivate *priv = area->priv;
  GdkDevice *device = gdk_event_get_device((GdkEvent *)event);

  if (priv->first_drag && (!priv->moved) &&
      ((ABS (*x) > (priv->panning_threshold))
       || (ABS (*y) > (priv->panning_threshold)))) {
    priv->moved = TRUE;
    *x = 0;
    *y = 0;

    if (priv->first_drag) {
        gboolean vscroll_visible;
        gboolean hscroll_visible;

      if (ABS (priv->iy - event->y) >=
          ABS (priv->ix - event->x)) {

        g_signal_emit (area,
                       pannable_area_signals[VERTICAL_MOVEMENT],
                       0, (priv->iy > event->y) ?
                       HILDON_MOVEMENT_UP :
                       HILDON_MOVEMENT_DOWN,
                       (gdouble)priv->ix, (gdouble)priv->iy);

        vscroll_visible = (gtk_adjustment_get_upper (priv->vadjust) - gtk_adjustment_get_lower (priv->vadjust) >
                   gtk_adjustment_get_page_size (priv->vadjust));

        if (!((vscroll_visible)&&
              (priv->mov_mode&HILDON_MOVEMENT_MODE_VERT))) {

          hscroll_visible = (gtk_adjustment_get_upper (priv->hadjust) - gtk_adjustment_get_lower (priv->hadjust) >
                             gtk_adjustment_get_page_size (priv->hadjust));

          /* even in case we do not have to move we check if this
             could be a fake horizontal movement */
          if (!((hscroll_visible)&&
                (priv->mov_mode&HILDON_MOVEMENT_MODE_HORIZ)) ||
              (ABS (priv->iy - event->y) -
               ABS (priv->ix - event->x) >= priv->direction_error_margin))
            priv->moved = FALSE;
        }
      } else {

        g_signal_emit (area,
                       pannable_area_signals[HORIZONTAL_MOVEMENT],
                       0, (priv->ix > event->x) ?
                       HILDON_MOVEMENT_LEFT :
                       HILDON_MOVEMENT_RIGHT,
                       (gdouble)priv->ix, (gdouble)priv->iy);

        hscroll_visible = (gtk_adjustment_get_upper (priv->hadjust) - gtk_adjustment_get_lower (priv->hadjust) >
                           gtk_adjustment_get_page_size (priv->hadjust));

        if (!((hscroll_visible)&&
              (priv->mov_mode&HILDON_MOVEMENT_MODE_HORIZ))) {

          vscroll_visible = (gtk_adjustment_get_upper (priv->vadjust) - gtk_adjustment_get_lower (priv->vadjust) >
                             gtk_adjustment_get_page_size (priv->vadjust));

          /* even in case we do not have to move we check if this
             could be a fake vertical movement */
          if (!((vscroll_visible) &&
                (priv->mov_mode&HILDON_MOVEMENT_MODE_VERT)) ||
              (ABS (priv->ix - event->x) -
               ABS (priv->iy - event->y) >= priv->direction_error_margin))
            priv->moved = FALSE;
        }
      }

      if ((priv->moved)&&(priv->child)) {
        gint pos_x, pos_y;

        pos_x = priv->cx + (event->x - priv->ix);
        pos_y = priv->cy + (event->y - priv->iy);

        synth_crossing (priv->child, device, pos_x, pos_y, event->x_root,
                        event->y_root, event->time, FALSE);
      }

      if (priv->moved) {
        gboolean result_val;

        g_signal_emit (area,
                       pannable_area_signals[PANNING_STARTED],
                       0, &result_val);

        priv->moved = !result_val;
      }
    }

    priv->first_drag = FALSE;

    if ((priv->mode != HILDON_PANNABLE_AREA_MODE_PUSH) &&
	(priv->mode != HILDON_PANNABLE_AREA_MODE_AUTO)) {

      if (!priv->idle_id)
        priv->idle_id = gdk_threads_add_timeout_full
          (G_PRIORITY_HIGH_IDLE + 20,
	   (gint)(1000.0 / (gdouble) priv->sps),
	   (GSourceFunc)
	   hildon_pannable_area_timeout, area, NULL);
    }
  }
}

static void
hildon_pannable_area_handle_move (HildonPannableArea *area,
                                  GdkEventMotion * event,
                                  gdouble *x,
                                  gdouble *y)
{
  HildonPannableAreaPrivate *priv = area->priv;
  gdouble delta;

  switch (priv->mode) {
  case HILDON_PANNABLE_AREA_MODE_PUSH:
    /* Scroll by the amount of pixels the cursor has moved
     * since the last motion event.
     */
    hildon_pannable_area_motion_event_scroll (area, *x, *y);
    priv->x = event->x;
    priv->y = event->y;
    break;
  case HILDON_PANNABLE_AREA_MODE_ACCEL:
    /* Set acceleration relative to the initial click */
    priv->ex = event->x;
    priv->ey = event->y;
    priv->vel_x = ((*x > 0) ? 1 : -1) *
      (((ABS (*x) /
         (gdouble) gtk_widget_get_allocated_width (GTK_WIDGET (area))) *
        (priv->vmax - priv->vmin)) + priv->vmin);
    priv->vel_y = ((*y > 0) ? 1 : -1) *
      (((ABS (*y) /
         (gdouble) gtk_widget_get_allocated_height (GTK_WIDGET (area))) *
        (priv->vmax - priv->vmin)) + priv->vmin);
    break;
  case HILDON_PANNABLE_AREA_MODE_AUTO:

    delta = event->time - priv->last_time;

    if (priv->mov_mode&HILDON_MOVEMENT_MODE_VERT) {
      gdouble dist = event->y - priv->y;

      hildon_pannable_area_calculate_velocity (&priv->vel_y,
                                               delta,
                                               dist,
                                               priv->vmax,
                                               priv->drag_inertia,
                                               priv->force,
                                               priv->sps);
    } else {
      *y = 0;
      priv->vel_y = 0;
    }

    if (priv->mov_mode&HILDON_MOVEMENT_MODE_HORIZ) {
      gdouble dist = event->x - priv->x;

      hildon_pannable_area_calculate_velocity (&priv->vel_x,
                                               delta,
                                               dist,
                                               priv->vmax,
                                               priv->drag_inertia,
                                               priv->force,
                                               priv->sps);
    } else {
      *x = 0;
      priv->vel_x = 0;
    }

    hildon_pannable_area_motion_event_scroll (area, *x, *y);

    if (priv->mov_mode&HILDON_MOVEMENT_MODE_HORIZ)
      priv->x = event->x;
    if (priv->mov_mode&HILDON_MOVEMENT_MODE_VERT)
      priv->y = event->y;

    break;
  default:
    break;
  }
}

static gboolean
hildon_pannable_area_motion_notify_cb (GtkWidget * widget,
				       GdkEventMotion * event)
{
  HildonPannableArea *area = HILDON_PANNABLE_AREA (widget);
  HildonPannableAreaPrivate *priv = area->priv;
  GdkDevice *device = gdk_event_get_device((GdkEvent *)event);
  gdouble x, y;

  if (gtk_bin_get_child (GTK_BIN (widget)) == NULL)
    return TRUE;

  if ((!priv->enabled) || (!priv->button_pressed) ||
      ((event->time == priv->last_time) && (priv->last_type == 2))) {
    GdkWindow* window = gtk_widget_get_window (widget);
    gdk_window_get_device_position (window,
                                    gdk_device_manager_get_client_pointer (
                                      gdk_display_get_device_manager (
                                        gdk_window_get_display (window))),
                                    NULL, NULL, 0);
    return TRUE;
  }

  if (!priv->selection_movement) {

    if (priv->last_type == 1) {
      priv->first_drag = TRUE;
    }

    x = event->x - priv->x;
    y = event->y - priv->y;

    if (!priv->moved) {
      hildon_pannable_area_check_move (area, event, &x, &y);
    }

    if (priv->moved) {
      hildon_pannable_area_handle_move (area, event, &x, &y);
    } else if (priv->child) {
      gboolean in;
      gint pos_x, pos_y;

      pos_x = priv->cx + (event->x - priv->ix);
      pos_y = priv->cy + (event->y - priv->iy);

      in = (((0 <= pos_x)&&(priv->child_width >= pos_x)) &&
            ((0 <= pos_y)&&(priv->child_height >= pos_y)));

      if (((!priv->last_in)&&in)||((priv->last_in)&&(!in))) {

        synth_crossing (priv->child, device, pos_x, pos_y, event->x_root,
                        event->y_root, event->time, in);

        priv->last_in = in;
      }
    }

    priv->last_time = event->time;
    priv->last_type = 2;
  }

  if (priv->child && priv->event_window == ((GdkEvent*) event)->any.window) {
      /* Send motion notify to child */
      event = (GdkEventMotion *) gdk_event_copy ((GdkEvent *) event);
      /* remove the reference we added with the copy */
      g_object_unref (((GdkEvent*) event)->any.window);
      event->x = priv->cx + (event->x - priv->ix);
      event->y = priv->cy + (event->y - priv->iy);
      ((GdkEvent*) event)->any.window = g_object_ref (priv->child);
      gdk_event_put ((GdkEvent *) event);
      gdk_event_free ((GdkEvent *) event);
  }

  GdkWindow* window = gtk_widget_get_window (widget);
  gdk_window_get_device_position (window,
                                  gdk_device_manager_get_client_pointer (
                                    gdk_display_get_device_manager (
                                      gdk_window_get_display (window))),
                                  NULL, NULL, 0);


  return TRUE;
}

static gboolean
hildon_pannable_leave_notify_event (GtkWidget *widget,
                                    GdkEventCrossing *event)
{
  HildonPannableArea *area = HILDON_PANNABLE_AREA (widget);
  HildonPannableAreaPrivate *priv = area->priv;
  GdkDevice *device = gdk_event_get_device((GdkEvent *)event);

  if ((priv->child)&&(priv->last_in)) {
    priv->last_in = FALSE;

    synth_crossing (priv->child, device, 0, 0, event->x_root,
                    event->y_root, event->time, FALSE);
  }

  return FALSE;
}

static gboolean
hildon_pannable_area_key_release_cb (GtkWidget * widget,
                                     GdkEventKey * event)
{
  HildonPannableArea *area = HILDON_PANNABLE_AREA (widget);

  if (G_UNLIKELY (area->priv->center_on_child_focus_pending)) {
    hildon_pannable_area_center_on_child_focus (area);
    area->priv->center_on_child_focus_pending = FALSE;
  }

  return FALSE;
}

static gboolean
hildon_pannable_area_button_release_cb (GtkWidget * widget,
					GdkEventButton * event)
{
  HildonPannableArea *area = HILDON_PANNABLE_AREA (widget);
  HildonPannableAreaPrivate *priv = area->priv;
  GdkDevice *device = gdk_event_get_device((GdkEvent *)event);
  gint x, y;
  gdouble dx, dy;
  GdkWindow *child;
  gboolean force_fast = TRUE;

  if  (((event->time == priv->last_time) && (priv->last_type == 3))
       || (gtk_bin_get_child (GTK_BIN (widget)) == NULL)
       || (!priv->button_pressed) || (!priv->enabled) || (event->button != 1))
    return TRUE;

  if (!priv->selection_movement) {
    /* if last event was a motion-notify we have to check the movement
       and launch the animation */
    if (priv->last_type == 2) {

      dx = event->x - priv->x;
      dy = event->y - priv->y;

      hildon_pannable_area_check_move (area, (GdkEventMotion *) event, &dx, &dy);

      if (priv->moved) {
        gdouble delta = event->time - priv->last_time;

        hildon_pannable_area_handle_move (area, (GdkEventMotion *) event, &dx, &dy);

        /* move all the way to the last position now */
        if (priv->motion_event_scroll_timeout) {
          g_source_remove (priv->motion_event_scroll_timeout);
          hildon_pannable_area_motion_event_scroll_timeout (HILDON_PANNABLE_AREA (widget));
          priv->motion_x = 0;
          priv->motion_y = 0;
        }

        if ((ABS (dx) < 4.0) && (delta >= CURSOR_STOPPED_TIMEOUT))
          priv->vel_x = 0;

        if ((ABS (dy) < 4.0) && (delta >= CURSOR_STOPPED_TIMEOUT))
          priv->vel_y = 0;
      }
    }

    /* If overshoot has been initiated with a finger down, on release set max speed */
    if (priv->overshot_dist_y != 0) {
      priv->overshooting_y = priv->bounce_steps; /* Hack to stop a bounce in the finger down case */
      priv->vel_y = priv->overshot_dist_y * 0.9;
    }

    if (priv->overshot_dist_x != 0) {
      priv->overshooting_x = priv->bounce_steps; /* Hack to stop a bounce in the finger down case */
      priv->vel_x = priv->overshot_dist_x * 0.9;
    }

    priv->button_pressed = FALSE;

    /* if widget was moving fast in the panning, increase speed even more */
    if ((event->time - priv->last_press_time < FAST_CLICK) &&
        ((ABS (priv->old_vel_x) > priv->vmin) ||
         (ABS (priv->old_vel_y) > priv->vmin)) &&
        ((ABS (priv->old_vel_x) > MIN_ACCEL_THRESHOLD) ||
         (ABS (priv->old_vel_y) > MIN_ACCEL_THRESHOLD)))
      {
        gint symbol = 0;

        if (priv->vel_x != 0)
          symbol = ((priv->vel_x * priv->old_vel_x) > 0) ? 1 : -1;

        priv->vel_x = symbol *
          (priv->old_vel_x + ((priv->old_vel_x > 0) ? priv->accel_vel_x
                              : -priv->accel_vel_x));

        symbol = 0;

        if (priv->vel_y != 0)
          symbol = ((priv->vel_y * priv->old_vel_y) > 0) ? 1 : -1;

        priv->vel_y = symbol *
          (priv->old_vel_y + ((priv->old_vel_y > 0) ? priv->accel_vel_y
                              : -priv->accel_vel_y));

        force_fast = FALSE;
      }

    if  ((ABS (priv->vel_y) >= priv->vmin) ||
         (ABS (priv->vel_x) >= priv->vmin)) {

      /* we have to move because we are in overshooting position*/
      if (!priv->moved) {
        gboolean result_val;

        g_signal_emit (area,
                       pannable_area_signals[PANNING_STARTED],
                       0, &result_val);
      }

      priv->scroll_indicator_alpha = 1.0;

      if (force_fast) {
        if ((ABS (priv->vel_x) > MAX_SPEED_THRESHOLD) &&
            (priv->accel_vel_x > MAX_SPEED_THRESHOLD))
          priv->vel_x = (priv->vel_x > 0) ? priv->accel_vel_x : -priv->accel_vel_x;

        if ((ABS (priv->vel_y) > MAX_SPEED_THRESHOLD) &&
            (priv->accel_vel_y > MAX_SPEED_THRESHOLD))
          priv->vel_y = (priv->vel_y > 0) ? priv->accel_vel_y : -priv->accel_vel_y;
      }

      if (!priv->idle_id)
        priv->idle_id = gdk_threads_add_timeout_full (G_PRIORITY_HIGH_IDLE + 20,
                                                      (gint) (1000.0 / (gdouble) priv->sps),
                                                      (GSourceFunc) hildon_pannable_area_timeout,
                                                      widget, NULL);
    } else {
      if (priv->center_on_child_focus_pending) {
        hildon_pannable_area_center_on_child_focus (area);
      }

      if (priv->moved)
        g_signal_emit (widget, pannable_area_signals[PANNING_FINISHED], 0);
    }

    area->priv->center_on_child_focus_pending = FALSE;

    priv->scroll_indicator_event_interrupt = 0;
    priv->scroll_delay_counter = priv->scrollbar_fade_delay;

    hildon_pannable_area_launch_fade_timeout (HILDON_PANNABLE_AREA (widget),
                                              priv->scroll_indicator_alpha);
  }

  priv->last_time = event->time;
  priv->last_type = 3;

  if (!priv->child) {
    priv->moved = FALSE;
    return TRUE;
  }

  child =
    hildon_pannable_area_get_topmost (
				      gtk_widget_get_window (gtk_bin_get_child (GTK_BIN (widget))),
				      event->x, event->y, &x, &y, GDK_BUTTON_RELEASE_MASK);

  event = (GdkEventButton *) gdk_event_copy ((GdkEvent *) event);
  /* remove the reference we added with the copy */
  g_object_unref (((GdkEvent*) event)->any.window);
  event->x = x;
  event->y = y;

  /* Leave the widget if we've moved - This doesn't break selection,
   * but stops buttons from being clicked.
   */
  if ((child != priv->child) || (priv->moved)) {
    /* Send synthetic leave event */
    synth_crossing (priv->child, device, x, y, event->x_root,
		    event->y_root, event->time, FALSE);
    /* insure no click will happen for widgets that do not handle
       leave-notify */
    event->x = -16384;
    event->y = -16384;
    /* Send synthetic button release event */
    ((GdkEvent *) event)->any.window = g_object_ref (priv->child);
    gdk_event_put ((GdkEvent *) event);
  } else {
    /* Send synthetic button release event */
    ((GdkEvent *) event)->any.window = g_object_ref (child);
    gdk_event_put ((GdkEvent *) event);
    /* Send synthetic leave event */
    synth_crossing (priv->child, device, x, y, event->x_root,
		    event->y_root, event->time, FALSE);
  }
  g_object_remove_weak_pointer ((GObject *) priv->child,
				(gpointer) & priv->child);

  priv->moved = FALSE;
  gdk_event_free ((GdkEvent *) event);

  return TRUE;
}

/* utility event handler */
static gboolean
hildon_pannable_area_scroll_cb (GtkWidget *widget,
                                GdkEventScroll *event)
{
  GtkAdjustment *adj = NULL;
  HildonPannableAreaPrivate *priv = HILDON_PANNABLE_AREA (widget)->priv;

  if ((!priv->enabled) ||
      (gtk_bin_get_child (GTK_BIN (widget)) == NULL))
    return TRUE;

  priv->scroll_indicator_event_interrupt = 0;
  priv->scroll_delay_counter = priv->scrollbar_fade_delay + 20;

  hildon_pannable_area_launch_fade_timeout (HILDON_PANNABLE_AREA (widget), 1.0);

  /* Stop inertial scrolling */
  if (priv->idle_id) {
    priv->vel_x = 0.0;
    priv->vel_y = 0.0;
    priv->overshooting_x = 0;
    priv->overshooting_y = 0;

    if ((priv->overshot_dist_x>0)||(priv->overshot_dist_y>0)) {
      priv->overshot_dist_x = 0;
      priv->overshot_dist_y = 0;

      gtk_widget_queue_resize (GTK_WIDGET (widget));
    }

    g_signal_emit (widget, pannable_area_signals[PANNING_FINISHED], 0);

    g_source_remove (priv->idle_id);
    priv->idle_id = 0;
  }

  if (event->direction == GDK_SCROLL_UP || event->direction == GDK_SCROLL_DOWN)
    adj = priv->vadjust;
  else
    adj = priv->hadjust;

  if (adj)
    {
      gdouble delta, new_value;

      /* from gtkrange.c calculate delta*/
      delta = pow (gtk_adjustment_get_page_size (adj), 2.0 / 3.0);

      if (event->direction == GDK_SCROLL_UP ||
          event->direction == GDK_SCROLL_LEFT)
        delta = - delta;

      new_value = CLAMP (gtk_adjustment_get_value (adj) + delta, gtk_adjustment_get_lower (adj), gtk_adjustment_get_upper (adj) - gtk_adjustment_get_page_size (adj));

      gtk_adjustment_set_value (adj, new_value);
    }

  return TRUE;
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

/*
 * This method calculates a factor necessary to determine the initial distance
 * to jump in hildon_pannable_area_scroll_to(). For fixed time and frames per
 * second, we know in how many frames 'n' we need to reach the destination
 * point. We know that, for a distance d,
 *
 *   d = d_0 + d_1 + ... + d_n
 *
 * where d_i is the distance travelled in the i-th frame and decel_factor is
 * the deceleration factor. This can be rewritten as
 *
 *   d = d_0 + (d_0 * decel_factor) + ... + (d_n-1 * decel_factor),
 *
 * since the distance travelled on each frame is the distance travelled in the
 * previous frame reduced by the deceleration factor. Reducing this and
 * factoring d_0 out, we get
 *
 *   d = d_0 (1 + decel_factor + ... + decel_factor^(n-1)).
 *
 * Since the sum is independent of the distance to be travelled, we can define
 * vel_factor as
 *
 *   vel_factor = 1 + decel_factor + ... + decel_factor^(n-1).
 *
 * That's the gem we calculate in this method.
 */
static void
hildon_pannable_calculate_vel_factor (HildonPannableArea * self)
{
  HildonPannableAreaPrivate *priv = self->priv;
  gfloat fct = 1;
  gfloat fct_i = 1;
  gint i, n;

  n = ceil (priv->sps * priv->scroll_time);

  for (i = 1; i < n && fct_i >= RATIO_TOLERANCE; i++) {
    fct_i *= priv->decel;
    fct += fct_i;
  }

  priv->vel_factor = fct;
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
 * See gtk_scrolled_window_add_with_viewport() for more information.
 *
 * Since: 2.2
 */

void
hildon_pannable_area_add_with_viewport (HildonPannableArea * area,
					GtkWidget * child)
{
  GtkBin *bin;
  GtkWidget *viewport;

  g_return_if_fail (HILDON_IS_PANNABLE_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == NULL);

  bin = GTK_BIN (area);

  if (gtk_bin_get_child (bin) != NULL)
    {
      g_return_if_fail (GTK_IS_VIEWPORT (gtk_bin_get_child (bin)));
      g_return_if_fail (gtk_bin_get_child (GTK_BIN (gtk_bin_get_child (bin))) == NULL);

      viewport = gtk_bin_get_child (bin);
    }
  else
    {
      HildonPannableAreaPrivate *priv = area->priv;

      viewport = gtk_viewport_new (priv->hadjust,
                                   priv->vadjust);
      gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
      gtk_container_add (GTK_CONTAINER (area), viewport);
    }

  gtk_widget_show (viewport);
  gtk_container_add (GTK_CONTAINER (viewport), child);
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
  HildonPannableAreaPrivate *priv;
  gboolean hscroll_visible, vscroll_visible;

  g_return_if_fail (HILDON_IS_PANNABLE_AREA (area));
  g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (area)));

  priv = area->priv;

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
   HildonPannableAreaPrivate *priv;
  gboolean hscroll_visible, vscroll_visible;

  g_return_if_fail (HILDON_IS_PANNABLE_AREA (area));
  g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (area)));

  priv = area->priv;

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
