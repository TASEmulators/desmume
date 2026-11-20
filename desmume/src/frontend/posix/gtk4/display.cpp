/*
	display.cpp - GTK4 video widget for DeSmuME

	The widget owns two GdkMemoryTextures (top and bottom screen) and draws
	them with gtk_snapshot_* in a custom snapshot() vfunc.  GTK3's Cairo
	expose path had its own GdkWindow; here the display is part of the single
	toplevel surface, so resizing needs no reinitialisation.

	Input arrives through GTK4 event controllers and is forwarded as signals.
	Converting bottom-screen framebuffer coordinates to NDS touch coordinates
	(nds_screen.touch_matrix) belongs to the emulation side and is added in P3.
*/

#include "display.h"

#define DESMUME_PI 3.14159265358979323846

struct _DesmumeDisplay
{
	GtkWidget parent_instance;

	GdkTexture *tex_top;
	GdkTexture *tex_bottom;
	int frame_w;
	int frame_h; /* one screen; the caller's buffer is frame_h * 2 tall */

	DesmumeLayout layout;
	int rotation;
	int gap;
	gboolean swap;
	double hybrid_scale;
};

G_DEFINE_TYPE(DesmumeDisplay, desmume_display, GTK_TYPE_WIDGET)

GtkWidget *
desmume_display_new(void)
{
	return GTK_WIDGET(g_object_new(DESMUME_TYPE_DISPLAY, NULL));
}

enum
{
	SIG_TOUCH_PRESS,
	SIG_TOUCH_RELEASE,
	SIG_TOUCH_MOVE,
	SIG_KEY_EVENT,
	SIG_LAST
};

static guint display_signals[SIG_LAST];

/* ---------------------------------------------------------------- layout */

static void
compute_base(DesmumeDisplay *self, double *out_w, double *out_h)
{
	const double w = self->frame_w > 0 ? self->frame_w : 256.0;
	const double h = self->frame_h > 0 ? self->frame_h : 192.0;
	const double gap = self->gap > 0 ? self->gap : 0.0;
	const double hs = self->hybrid_scale > 0.0 ? self->hybrid_scale : 1.0;

	switch (self->layout)
	{
	case DESMUME_LAYOUT_HORIZONTAL:
		*out_w = 2.0 * w;
		*out_h = h;
		break;
	case DESMUME_LAYOUT_SINGLE:
		*out_w = w;
		*out_h = h;
		break;
	case DESMUME_LAYOUT_HYBRID_EQUAL:
	case DESMUME_LAYOUT_HYBRID_VERTICAL:
		*out_w = w * hs + w;
		*out_h = (self->layout == DESMUME_LAYOUT_HYBRID_VERTICAL)
		       ? h + gap + h : h * hs;
		break;
	case DESMUME_LAYOUT_VERTICAL:
	default:
		*out_w = w;
		*out_h = 2.0 * h + gap;
		break;
	}
}

/* Position of one screen in base coordinates.  FALSE means it is hidden
 * (single-screen mode), which also makes touch mapping return FALSE. */
static gboolean
screen_rect(DesmumeDisplay *self, gboolean bottom,
            double *out_x, double *out_y, double *out_w, double *out_h)
{
	const double w = (double)self->frame_w;
	const double h = (double)self->frame_h;
	const double gap = self->gap > 0 ? self->gap : 0.0;
	const double hs = self->hybrid_scale > 0.0 ? self->hybrid_scale : 1.0;

	switch (self->layout)
	{
	case DESMUME_LAYOUT_VERTICAL:
		*out_x = 0.0;
		*out_y = (bottom != (gboolean)FALSE) == (gboolean)self->swap ? 0.0 : h + gap;
		*out_w = w;
		*out_h = h;
		return TRUE;

	case DESMUME_LAYOUT_HORIZONTAL:
		*out_x = (bottom != (gboolean)FALSE) == (gboolean)self->swap ? 0.0 : w;
		*out_y = 0.0;
		*out_w = w;
		*out_h = h;
		return TRUE;

	case DESMUME_LAYOUT_SINGLE:
		/* GTK3: the top screen is hidden when swapped, the bottom one
		 * otherwise.  Only the visible screen can receive touch input. */
		if (bottom != (gboolean)self->swap)
		{
			return FALSE;
		}
		*out_x = 0.0;
		*out_y = 0.0;
		*out_w = w;
		*out_h = h;
		return TRUE;

	case DESMUME_LAYOUT_HYBRID_EQUAL:
	case DESMUME_LAYOUT_HYBRID_VERTICAL:
	{
		const gboolean big = (bottom == (gboolean)self->swap) ? TRUE : FALSE;
		if (big)
		{
			*out_x = 0.0;
			*out_y = 0.0;
			*out_w = w * hs;
			*out_h = h * hs;
		}
		else
		{
			*out_x = w * hs;
			*out_y = (self->layout == DESMUME_LAYOUT_HYBRID_VERTICAL) ? h + gap : 0.0;
			*out_w = w;
			*out_h = h;
		}
		return TRUE;
	}

	default:
		return FALSE;
	}
}

/* widget coordinates -> base coordinates (the combined two-screen frame) */
static void
compute_transform(DesmumeDisplay *self, int width, int height,
                  cairo_matrix_t *matrix,
                  double *out_scale, double *out_base_w, double *out_base_h)
{
	double bw, bh;
	compute_base(self, &bw, &bh);

	const int rotation = ((self->rotation % 360) + 360) % 360;
	const gboolean quarter_turn = (rotation == 90) || (rotation == 270);
	const double fit_w = quarter_turn ? bh : bw;
	const double fit_h = quarter_turn ? bw : bh;
	const double scale = MIN((double)width / fit_w, (double)height / fit_h);

	cairo_matrix_init_identity(matrix);
	cairo_matrix_translate(matrix, width / 2.0, height / 2.0);
	cairo_matrix_rotate(matrix, (double)rotation * DESMUME_PI / 180.0);
	cairo_matrix_scale(matrix, scale, scale);
	cairo_matrix_translate(matrix, -bw / 2.0, -bh / 2.0);

	if (out_scale != NULL)
	{
		*out_scale = scale;
	}
	if (out_base_w != NULL)
	{
		*out_base_w = bw;
	}
	if (out_base_h != NULL)
	{
		*out_base_h = bh;
	}
}

/* --------------------------------------------------------------- snapshot */

static void
append_screen(GtkSnapshot *snapshot, GdkTexture *texture,
              double x, double y, double w, double h)
{
	const graphene_rect_t rect = GRAPHENE_RECT_INIT((float)x, (float)y,
	                                                (float)w, (float)h);
	gtk_snapshot_append_texture(snapshot, texture, &rect);
}

static void
desmume_display_snapshot(GtkWidget *widget, GtkSnapshot *snapshot)
{
	DesmumeDisplay *self = DESMUME_DISPLAY(widget);
	const int width = gtk_widget_get_width(widget);
	const int height = gtk_widget_get_height(widget);
	const GdkRGBA black = { 0.0, 0.0, 0.0, 1.0 };
	const graphene_rect_t whole = GRAPHENE_RECT_INIT(0, 0,
	                                                 (float)width, (float)height);

	gtk_snapshot_append_color(snapshot, &black, &whole);

	if ((self->tex_top == NULL) || (width <= 0) || (height <= 0))
	{
		return;
	}

	double bw, bh, scale;
	cairo_matrix_t transform;
	compute_transform(self, width, height, &transform, &scale, &bw, &bh);
	(void)transform;

	const graphene_point_t center = GRAPHENE_POINT_INIT(width / 2.0f,
	                                                    height / 2.0f);
	const graphene_point_t origin = GRAPHENE_POINT_INIT(-(float)bw / 2.0f,
	                                                    -(float)bh / 2.0f);

	gtk_snapshot_save(snapshot);
	gtk_snapshot_translate(snapshot, &center);
	gtk_snapshot_rotate(snapshot,
	                    (float)(((self->rotation % 360) + 360) % 360));
	gtk_snapshot_scale(snapshot, (float)scale, (float)scale);
	gtk_snapshot_translate(snapshot, &origin);

	double x, y, w, h;
	if (screen_rect(self, FALSE, &x, &y, &w, &h))
	{
		append_screen(snapshot, self->tex_top, x, y, w, h);
	}
	if (screen_rect(self, TRUE, &x, &y, &w, &h))
	{
		append_screen(snapshot, self->tex_bottom, x, y, w, h);
	}

	if ((self->gap > 0) && (self->layout == DESMUME_LAYOUT_VERTICAL))
	{
		const GdkRGBA gap_color = { 0.10, 0.10, 0.12, 1.0 };
		const graphene_rect_t gap = GRAPHENE_RECT_INIT(0, (float)self->frame_h,
		                                              (float)self->frame_w,
		                                              (float)self->gap);
		gtk_snapshot_append_color(snapshot, &gap_color, &gap);
	}

	gtk_snapshot_restore(snapshot);
}

static void
desmume_display_measure(GtkWidget *widget, GtkOrientation orientation,
                        int for_size, int *minimum, int *natural,
                        int *minimum_baseline, int *natural_baseline)
{
	DesmumeDisplay *self = DESMUME_DISPLAY(widget);
	double bw, bh;
	compute_base(self, &bw, &bh);

	const int size = (orientation == GTK_ORIENTATION_HORIZONTAL)
	               ? (int)bw : (int)bh;

	(void)for_size;
	*minimum = 1;
	*natural = size;
	*minimum_baseline = -1;
	*natural_baseline = -1;
}

/* ------------------------------------------------------------ frame input */

void
desmume_display_push_frame(DesmumeDisplay *self, const guchar *bgra,
                           int width, int height)
{
	g_return_if_fail(DESMUME_IS_DISPLAY(self));
	g_return_if_fail(bgra != NULL);
	g_return_if_fail(width > 0 && height >= 2);

	/* §6.2: an unmapped window needs no texture upload or redraw; emulation
	 * continues independently of presentation. */
	if (!gtk_widget_get_mapped(GTK_WIDGET(self)))
	{
		return;
	}

	const int screen_h = height / 2;
	const gsize stride = (gsize)width * 4;
	const gsize screen_bytes = stride * (gsize)screen_h;
	const gsize total = screen_bytes * 2;

	/* One copy is needed here to force alpha to 0xFF: the framebuffer uses
	 * BGRX and GDK_MEMORY_B8G8R8A8_PREMULTIPLIED must not see garbage alpha.
	 * P2b can replace this with a GBytes pool around the emulator's own
	 * double-buffered frames. */
	guchar *copy = (guchar *)g_malloc(total);
	for (gsize i = 0; i < total; i += 4)
	{
		copy[i + 0] = bgra[i + 0];
		copy[i + 1] = bgra[i + 1];
		copy[i + 2] = bgra[i + 2];
		copy[i + 3] = 0xFF;
	}

	GBytes *parent = g_bytes_new_take(copy, total);
	GBytes *top_bytes = g_bytes_new_from_bytes(parent, 0, screen_bytes);
	GBytes *bottom_bytes = g_bytes_new_from_bytes(parent, screen_bytes, screen_bytes);

	GdkTexture *top = gdk_memory_texture_new(width, screen_h,
	                                         GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
	                                         top_bytes, stride);
	GdkTexture *bottom = gdk_memory_texture_new(width, screen_h,
	                                            GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
	                                            bottom_bytes, stride);

	g_bytes_unref(top_bytes);
	g_bytes_unref(bottom_bytes);
	g_bytes_unref(parent);

	g_clear_object(&self->tex_top);
	g_clear_object(&self->tex_bottom);
	self->tex_top = top;
	self->tex_bottom = bottom;
	self->frame_w = width;
	self->frame_h = screen_h;

	gtk_widget_queue_draw(GTK_WIDGET(self));
}

#define DEFINE_SETTER(name, type, field)                                     \
	void desmume_display_set_##name(DesmumeDisplay *self, type value)         \
	{                                                                        \
		g_return_if_fail(DESMUME_IS_DISPLAY(self));                           \
		self->field = value;                                                 \
		gtk_widget_queue_resize(GTK_WIDGET(self));                           \
	}

DEFINE_SETTER(layout, DesmumeLayout, layout)
DEFINE_SETTER(rotation, int, rotation)
DEFINE_SETTER(gap, int, gap)
DEFINE_SETTER(swap, gboolean, swap)
DEFINE_SETTER(hybrid_scale, double, hybrid_scale)

#undef DEFINE_SETTER

gboolean
desmume_display_map_touch(DesmumeDisplay *self, double x, double y,
                          double *out_x, double *out_y)
{
	g_return_val_if_fail(DESMUME_IS_DISPLAY(self), FALSE);
	g_return_val_if_fail(out_x != NULL && out_y != NULL, FALSE);

	cairo_matrix_t transform, inverse;
	compute_transform(self, gtk_widget_get_width(GTK_WIDGET(self)),
	                  gtk_widget_get_height(GTK_WIDGET(self)),
	                  &transform, NULL, NULL, NULL);
	inverse = transform;
	if (cairo_matrix_invert(&inverse) != CAIRO_STATUS_SUCCESS)
	{
		return FALSE;
	}

	double sx = x;
	double sy = y;
	cairo_matrix_transform_point(&inverse, &sx, &sy);

	double rx, ry, rw, rh;
	if (!screen_rect(self, TRUE, &rx, &ry, &rw, &rh) ||
	    sx < rx || sx > rx + rw || sy < ry || sy > ry + rh || rw <= 0 || rh <= 0)
	{
		return FALSE;
	}

	*out_x = (sx - rx) * (double)self->frame_w / rw;
	*out_y = (sy - ry) * (double)self->frame_h / rh;
	return TRUE;
}

/* --------------------------------------------------------------- controllers */

static gboolean
map_event(DesmumeDisplay *self, double x, double y, double *out_x, double *out_y)
{
	return desmume_display_map_touch(self, x, y, out_x, out_y);
}

static void
on_click_pressed(GtkGestureClick *gesture, int n_press,
                 double x, double y, gpointer user_data)
{
	DesmumeDisplay *self = DESMUME_DISPLAY(user_data);
	(void)n_press;

	gtk_widget_grab_focus(GTK_WIDGET(self));

	double nx, ny;
	if (map_event(self, x, y, &nx, &ny))
	{
		g_signal_emit(self, display_signals[SIG_TOUCH_PRESS], 0, nx, ny);
	}
}

static void
on_click_released(GtkGestureClick *gesture, int n_press,
                  double x, double y, gpointer user_data)
{
	DesmumeDisplay *self = DESMUME_DISPLAY(user_data);
	(void)gesture;
	(void)n_press;
	(void)x;
	(void)y;

	g_signal_emit(self, display_signals[SIG_TOUCH_RELEASE], 0);
}

static void
on_motion(GtkEventControllerMotion *controller,
          double x, double y, gpointer user_data)
{
	DesmumeDisplay *self = DESMUME_DISPLAY(user_data);
	(void)controller;

	double nx, ny;
	if (map_event(self, x, y, &nx, &ny))
	{
		g_signal_emit(self, display_signals[SIG_TOUCH_MOVE], 0, nx, ny);
	}
}

static gboolean
on_key_pressed(GtkEventControllerKey *controller, guint keyval,
               guint keycode, GdkModifierType state, gpointer user_data)
{
	DesmumeDisplay *self = DESMUME_DISPLAY(user_data);
	(void)controller;
	(void)keycode;
	(void)state;

	g_signal_emit(self, display_signals[SIG_KEY_EVENT], 0, keyval, TRUE);
	return FALSE; /* let accelerators (F11, Ctrl+O, ...) still work */
}

static gboolean
on_key_released(GtkEventControllerKey *controller, guint keyval,
                guint keycode, GdkModifierType state, gpointer user_data)
{
	DesmumeDisplay *self = DESMUME_DISPLAY(user_data);
	(void)controller;
	(void)keycode;
	(void)state;

	g_signal_emit(self, display_signals[SIG_KEY_EVENT], 0, keyval, FALSE);
	return FALSE;
}

/* ------------------------------------------------------------------ GObject */

static void
desmume_display_dispose(GObject *object)
{
	DesmumeDisplay *self = DESMUME_DISPLAY(object);

	g_clear_object(&self->tex_top);
	g_clear_object(&self->tex_bottom);

	G_OBJECT_CLASS(desmume_display_parent_class)->dispose(object);
}

static void
desmume_display_class_init(DesmumeDisplayClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	object_class->dispose = desmume_display_dispose;
	widget_class->snapshot = desmume_display_snapshot;
	widget_class->measure = desmume_display_measure;

	display_signals[SIG_TOUCH_PRESS] = g_signal_new(
		"touch-press", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0,
		NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
	display_signals[SIG_TOUCH_RELEASE] = g_signal_new(
		"touch-release", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0,
		NULL, NULL, NULL, G_TYPE_NONE, 0);
	display_signals[SIG_TOUCH_MOVE] = g_signal_new(
		"touch-move", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0,
		NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
	display_signals[SIG_KEY_EVENT] = g_signal_new(
		"key-event", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0,
		NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_BOOLEAN);
}

static void
desmume_display_init(DesmumeDisplay *self)
{
	self->layout = DESMUME_LAYOUT_VERTICAL;
	self->hybrid_scale = 1.0;

	gtk_widget_set_focusable(GTK_WIDGET(self), TRUE);
	gtk_widget_add_css_class(GTK_WIDGET(self), "desmume-display");

	GtkGesture *click = gtk_gesture_click_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_PRIMARY);
	g_signal_connect(click, "pressed", G_CALLBACK(on_click_pressed), self);
	g_signal_connect(click, "released", G_CALLBACK(on_click_released), self);
	gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(click));

	GtkEventController *motion = gtk_event_controller_motion_new();
	g_signal_connect(motion, "motion", G_CALLBACK(on_motion), self);
	gtk_widget_add_controller(GTK_WIDGET(self), motion);

	GtkEventController *key = gtk_event_controller_key_new();
	g_signal_connect(key, "key-pressed", G_CALLBACK(on_key_pressed), self);
	g_signal_connect(key, "key-released", G_CALLBACK(on_key_released), self);
	gtk_widget_add_controller(GTK_WIDGET(self), key);
}
