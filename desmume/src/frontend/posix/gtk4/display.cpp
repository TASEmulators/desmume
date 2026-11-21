/*
	display.cpp - GTK4 video widget for DeSmuME

	The widget owns a small pool of alpha-fixed BGRX buffers which it wraps in
	two GdkMemoryTextures (top and bottom screen) and draws with gtk_snapshot_*
	in a custom snapshot() vfunc.  GTK3's Cairo expose path had its own
	GdkWindow; here the display is part of the single toplevel surface, so
	resizing needs no reinitialisation.

	Layout, rotation, gap and hybrid scaling follow gtk/main.cpp exactly
	(draw_params / ExposeDrawingArea / drawTopScreen / drawBottomScreen), so
	the GTK4 texture path is pixel-equivalent to the GTK3 Cairo path for the
	same framebuffer.  Input arrives through GTK4 event controllers; mapping
	ends in NDS screen coordinates (0..255, 0..191), ready for P3 to feed
	NDS_setTouchPos() and the HUD editor.
*/

#include "display.h"

#include <string.h>

#define DESMUME_PI 3.14159265358979323846
#define DESMUME_NDS_W 256.0
#define DESMUME_NDS_H 192.0

typedef struct
{
	gatomicrefcount ref_count;
	GMutex lock;
	GQueue free_buffers;
	gsize buffer_size;
} FramePool;

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
	double hybrid_scale; /* <= 0: compute like GTK3 */

	FramePool *pool;
	gboolean stylus_active;
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

typedef struct
{
	double bw, bh;     /* unrotated image size, framebuffer pixels */
	double fit_w, fit_h; /* the same size after 90/270 rotation */
	double scale;      /* widget pixels per framebuffer pixel */
	double hs;         /* hybrid big-screen scale; 0 outside hybrid layouts */
	double gap;        /* effective gap, framebuffer pixels */
	int rotation;
	gboolean quarter;
	gboolean hybrid;
} Geometry;

static double
frame_width(const DesmumeDisplay *self)
{
	return self->frame_w > 0 ? (double)self->frame_w : DESMUME_NDS_W;
}

static double
frame_height(const DesmumeDisplay *self)
{
	return self->frame_h > 0 ? (double)self->frame_h : DESMUME_NDS_H;
}

static void
compute_geometry(DesmumeDisplay *self, int widget_w, int widget_h, Geometry *g)
{
	const double w = frame_width(self);
	const double h = frame_height(self);
	const double hybrid_gap = self->gap > 0 ? (double)self->gap : 0.0;

	g->rotation = ((self->rotation % 360) + 360) % 360;
	g->quarter = (g->rotation == 90) || (g->rotation == 270);
	g->hybrid = (self->layout == DESMUME_LAYOUT_HYBRID_EQUAL) ||
	            (self->layout == DESMUME_LAYOUT_HYBRID_VERTICAL);

	/* GTK3 only applies the gap to vertical and hybrid layouts. */
	g->gap = (self->layout == DESMUME_LAYOUT_VERTICAL || g->hybrid)
	       ? hybrid_gap : 0.0;

	g->hs = 0.0;
	switch (self->layout)
	{
	case DESMUME_LAYOUT_HORIZONTAL:
		g->bw = 2.0 * w;
		g->bh = h;
		break;
	case DESMUME_LAYOUT_SINGLE:
		g->bw = w;
		g->bh = h;
		break;
	case DESMUME_LAYOUT_HYBRID_EQUAL:
	case DESMUME_LAYOUT_HYBRID_VERTICAL:
	{
		double hs = self->hybrid_scale;

		if (hs <= 0.0)
		{
			if (self->layout == DESMUME_LAYOUT_HYBRID_EQUAL)
			{
				/* gtk/main.cpp:3168 -- (dstH + gap) / (dstH / 2) */
				hs = (2.0 * h + g->gap) / h;
			}
			else
			{
				/* gtk/main.cpp:2444 calc_hybrid_vertical_params().  GTK3
				 * swaps daW/daH for quarter turns before computing. */
				const double avail_w = g->quarter ? (double)widget_h : (double)widget_w;
				const double avail_h = g->quarter ? (double)widget_w : (double)widget_h;
				const double ratio1 = MIN(avail_w / w, avail_h / (2.0 * h + g->gap));

				if (ratio1 > 0.0)
				{
					const double rem = avail_w - w * ratio1;
					const double ratio2 = MIN(rem / w, avail_h / h);
					hs = ratio2 / ratio1;
				}
				else
				{
					hs = (2.0 * h + g->gap) / h;
				}
			}
		}

		/* A degenerate allocation (measure phase, zero-size widget) must not
		 * produce a negative or zero big screen. */
		if (hs < 1.0)
		{
			hs = 1.0;
		}
		g->hs = hs;
		g->bw = w * (hs + 1.0);
		g->bh = 2.0 * h + g->gap;
		break;
	}
	case DESMUME_LAYOUT_VERTICAL:
	default:
		g->bw = w;
		g->bh = 2.0 * h + g->gap;
		break;
	}

	g->fit_w = g->quarter ? g->bh : g->bw;
	g->fit_h = g->quarter ? g->bw : g->bh;

	g->scale = MIN((double)widget_w / g->fit_w, (double)widget_h / g->fit_h);
	if (!(g->scale > 0.0))
	{
		g->scale = 1.0;
	}
}

/* Position of one screen in unrotated base coordinates.  FALSE means hidden
 * (single-screen mode), which also makes touch mapping return FALSE. */
static gboolean
screen_rect(DesmumeDisplay *self, const Geometry *g, gboolean bottom,
            double *out_x, double *out_y, double *out_w, double *out_h)
{
	const double w = frame_width(self);
	const double h = frame_height(self);

	switch (self->layout)
	{
	case DESMUME_LAYOUT_VERTICAL:
		*out_x = 0.0;
		*out_y = (bottom == (gboolean)self->swap) ? 0.0 : h + g->gap;
		*out_w = w;
		*out_h = h;
		return TRUE;

	case DESMUME_LAYOUT_HORIZONTAL:
		*out_x = (bottom == (gboolean)self->swap) ? 0.0 : w;
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
		if (bottom == (gboolean)self->swap)
		{
			/* Big screen: gtk/main.cpp:2392 / 2426 scale it in place. */
			*out_x = 0.0;
			*out_y = 0.0;
			*out_w = w * g->hs;
			*out_h = h * g->hs;
		}
		else
		{
			/* Small screen: drawTopScreen() puts it at (w*hs, 0),
			 * drawBottomScreen() at (w*hs, h+gap). */
			*out_x = w * g->hs;
			*out_y = bottom ? h + g->gap : 0.0;
			*out_w = w;
			*out_h = h;
		}
		return TRUE;

	default:
		return FALSE;
	}
}

/* Same matrix the snapshot() transform builds, so inverse mapping is exact. */
static void
compute_transform_matrix(const Geometry *g, int width, int height,
                         cairo_matrix_t *matrix)
{
	cairo_matrix_init_identity(matrix);
	cairo_matrix_translate(matrix, width / 2.0, height / 2.0);
	cairo_matrix_rotate(matrix, (double)g->rotation * DESMUME_PI / 180.0);
	cairo_matrix_scale(matrix, g->scale, g->scale);
	cairo_matrix_translate(matrix, -g->fit_w / 2.0, -g->fit_h / 2.0);
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

	Geometry g;
	compute_geometry(self, width, height, &g);

	const graphene_point_t center = GRAPHENE_POINT_INIT(width / 2.0f,
	                                                    height / 2.0f);
	const graphene_point_t origin = GRAPHENE_POINT_INIT(-(float)(g.fit_w / 2.0),
	                                                    -(float)(g.fit_h / 2.0));

	gtk_snapshot_save(snapshot);
	gtk_snapshot_translate(snapshot, &center);
	gtk_snapshot_rotate(snapshot, (float)g.rotation);
	gtk_snapshot_scale(snapshot, (float)g.scale, (float)g.scale);
	gtk_snapshot_translate(snapshot, &origin);

	double x, y, w, h;
	if (screen_rect(self, &g, FALSE, &x, &y, &w, &h))
	{
		append_screen(snapshot, self->tex_top, x, y, w, h);
	}
	if (screen_rect(self, &g, TRUE, &x, &y, &w, &h))
	{
		append_screen(snapshot, self->tex_bottom, x, y, w, h);
	}

	/* gtk/main.cpp:3219 draws the gap last, in the same unrotated space. */
	if (g.gap > 0.0)
	{
		const GdkRGBA gap_color = { 0.3, 0.3, 0.3, 1.0 };
		const double gap_x = g.hybrid > 0.0 ? frame_width(self) * g.hs : 0.0;
		const graphene_rect_t gap = GRAPHENE_RECT_INIT(
			(float)gap_x, (float)frame_height(self),
			(float)frame_width(self), (float)g.gap);
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
	Geometry g;
	compute_geometry(self, 0, 0, &g);

	const int size = (orientation == GTK_ORIENTATION_HORIZONTAL)
	               ? (int)g.fit_w : (int)g.fit_h;

	(void)for_size;
	*minimum = 1;
	*natural = size;
	*minimum_baseline = -1;
	*natural_baseline = -1;
}

/* ------------------------------------------------------------ frame input */

typedef struct
{
	FramePool *pool;
	guchar *data;
	gsize size;
} BufferRef;

static FramePool *
frame_pool_new(void)
{
	FramePool *pool = g_new0(FramePool, 1);
	g_atomic_ref_count_init(&pool->ref_count);
	g_mutex_init(&pool->lock);
	g_queue_init(&pool->free_buffers);
	return pool;
}

static void
frame_pool_unref(FramePool *pool)
{
	if (g_atomic_ref_count_dec(&pool->ref_count))
	{
		guchar *data;
		while ((data = (guchar *)g_queue_pop_head(&pool->free_buffers)) != NULL)
		{
			g_free(data);
		}
		g_mutex_clear(&pool->lock);
		g_free(pool);
	}
}

static void
release_buffer(gpointer user_data)
{
	BufferRef *ref = (BufferRef *)user_data;
	FramePool *pool = ref->pool;

	g_mutex_lock(&pool->lock);
	if (ref->size == pool->buffer_size)
	{
		g_queue_push_head(&pool->free_buffers, ref->data);
	}
	else
	{
		/* The frame size changed while the texture was alive. */
		g_free(ref->data);
	}
	g_mutex_unlock(&pool->lock);

	frame_pool_unref(pool);
	g_free(ref);
}

static GBytes *
wrap_frame(DesmumeDisplay *self, const guchar *src, gsize size)
{
	FramePool *pool = self->pool;
	guchar *dst = NULL;

	g_mutex_lock(&pool->lock);
	dst = (guchar *)g_queue_pop_head(&pool->free_buffers);
	if ((dst != NULL) && (pool->buffer_size != size))
	{
		g_free(dst);
		dst = NULL;
	}
	pool->buffer_size = size;
	g_mutex_unlock(&pool->lock);

	const gboolean from_pool = (dst != NULL);
	if (!from_pool)
	{
		dst = (guchar *)g_malloc(size);
	}

	/* The emulator's buffer is BGRX.  GDK_MEMORY_B8G8R8A8_PREMULTIPLIED
	 * needs a defined alpha; a plain memcpy plus one byte per pixel is
	 * cheaper than the old element-by-element copy.  GTK 4.14 could use
	 * GDK_MEMORY_B8G8R8X8 and skip this entirely, but the 4.10 baseline
	 * does not have that format. */
	memcpy(dst, src, size);
	for (gsize i = 3; i < size; i += 4)
	{
		dst[i] = 0xFF;
	}

	if (from_pool)
	{
		BufferRef *ref = g_new0(BufferRef, 1);
		ref->pool = pool;
		ref->data = dst;
		ref->size = size;
		/* The borrowed buffer keeps the pool alive past widget dispose. */
		g_atomic_ref_count_inc(&pool->ref_count);
		return g_bytes_new_with_free_func(dst, size, release_buffer, ref);
	}

	return g_bytes_new_with_free_func(dst, size, (GDestroyNotify)g_free, dst);
}

void
desmume_display_push_frame(DesmumeDisplay *self, const guchar *bgra,
                           int width, int height)
{
	g_return_if_fail(DESMUME_IS_DISPLAY(self));
	g_return_if_fail(bgra != NULL);
	g_return_if_fail(width > 0 && height >= 2 && (height % 2) == 0);

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

	GBytes *parent = wrap_frame(self, bgra, total);
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

/* ----------------------------------------------------------------- mapping */

gboolean
desmume_display_map_screen(DesmumeDisplay *self, double x, double y,
                           gboolean bottom, double *out_x, double *out_y)
{
	g_return_val_if_fail(DESMUME_IS_DISPLAY(self), FALSE);
	g_return_val_if_fail(out_x != NULL && out_y != NULL, FALSE);

	GtkWidget *widget = GTK_WIDGET(self);
	Geometry g;
	compute_geometry(self, gtk_widget_get_width(widget),
	                 gtk_widget_get_height(widget), &g);

	cairo_matrix_t transform, inverse;
	compute_transform_matrix(&g, gtk_widget_get_width(widget),
	                         gtk_widget_get_height(widget), &transform);
	inverse = transform;
	if (cairo_matrix_invert(&inverse) != CAIRO_STATUS_SUCCESS)
	{
		return FALSE;
	}

	cairo_matrix_transform_point(&inverse, &x, &y);

	double rx, ry, rw, rh;
	if (!screen_rect(self, &g, bottom, &rx, &ry, &rw, &rh))
	{
		return FALSE;
	}
	(void)rw;
	(void)rh;

	if ((g.hs > 0.0) && (bottom == (gboolean)self->swap))
	{
		/* The big screen has its own scale and, in hybrid-vertical mode,
		 * an extra translate -- gtk/main.cpp:2428. */
		const double offset = (self->layout == DESMUME_LAYOUT_HYBRID_VERTICAL)
		                    ? frame_height(self) * (2.0 - g.hs) + g.gap : 0.0;
		x = (x - rx) / g.hs;
		y = (y - ry - offset) / g.hs;
	}
	else
	{
		x -= rx;
		y -= ry;
	}

	const double w = frame_width(self);
	const double h = frame_height(self);
	if ((x < 0.0) || (y < 0.0) || (x >= w) || (y >= h))
	{
		return FALSE;
	}

	*out_x = CLAMP(x * DESMUME_NDS_W / w, 0.0, DESMUME_NDS_W - 1.0);
	*out_y = CLAMP(y * DESMUME_NDS_H / h, 0.0, DESMUME_NDS_H - 1.0);
	return TRUE;
}

gboolean
desmume_display_map_touch(DesmumeDisplay *self, double x, double y,
                          double *out_x, double *out_y)
{
	return desmume_display_map_screen(self, x, y, TRUE, out_x, out_y);
}

/* --------------------------------------------------------------- controllers */

static void
emit_touch(DesmumeDisplay *self, guint signal, double x, double y)
{
	double nx, ny;
	if (desmume_display_map_touch(self, x, y, &nx, &ny))
	{
		g_signal_emit(self, display_signals[signal], 0, nx, ny);
	}
}

static void
on_click_pressed(GtkGestureClick *gesture, int n_press,
                 double x, double y, gpointer user_data)
{
	DesmumeDisplay *self = DESMUME_DISPLAY(user_data);
	(void)gesture;
	(void)n_press;

	/* A stylus also produces a click gesture; the stylus handler owns the
	 * touch while it is down. */
	if (self->stylus_active)
	{
		return;
	}

	gtk_widget_grab_focus(GTK_WIDGET(self));
	emit_touch(self, SIG_TOUCH_PRESS, x, y);
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

	if (self->stylus_active)
	{
		return;
	}

	g_signal_emit(self, display_signals[SIG_TOUCH_RELEASE], 0);
}

static void
on_motion(GtkEventControllerMotion *controller,
          double x, double y, gpointer user_data)
{
	DesmumeDisplay *self = DESMUME_DISPLAY(user_data);
	(void)controller;

	if (self->stylus_active)
	{
		return;
	}

	emit_touch(self, SIG_TOUCH_MOVE, x, y);
}

static gboolean
get_stylus_point(GtkGestureStylus *stylus, double *x, double *y)
{
	/* get_coordinates() is gone from the current GTK4 API; the axis
	 * accessors work on every 4.10+ release. */
	return gtk_gesture_stylus_get_axis(stylus, GDK_AXIS_X, x) &&
	       gtk_gesture_stylus_get_axis(stylus, GDK_AXIS_Y, y);
}

static void
on_stylus_down(GtkGestureStylus *stylus, gpointer user_data)
{
	DesmumeDisplay *self = DESMUME_DISPLAY(user_data);
	double x, y;

	self->stylus_active = TRUE;
	gtk_widget_grab_focus(GTK_WIDGET(self));
	if (get_stylus_point(stylus, &x, &y))
	{
		emit_touch(self, SIG_TOUCH_PRESS, x, y);
	}
}

static void
on_stylus_motion(GtkGestureStylus *stylus, gpointer user_data)
{
	DesmumeDisplay *self = DESMUME_DISPLAY(user_data);
	double x, y;

	if (get_stylus_point(stylus, &x, &y))
	{
		emit_touch(self, SIG_TOUCH_MOVE, x, y);
	}
}

static void
on_stylus_up(GtkGestureStylus *stylus, gpointer user_data)
{
	DesmumeDisplay *self = DESMUME_DISPLAY(user_data);
	(void)stylus;

	self->stylus_active = FALSE;
	g_signal_emit(self, display_signals[SIG_TOUCH_RELEASE], 0);
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

	frame_pool_unref(self->pool);
	self->pool = NULL;

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
	self->hybrid_scale = -1.0; /* auto, matching GTK3's draw_params */
	self->pool = frame_pool_new();

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

	GtkGesture *stylus = gtk_gesture_stylus_new();
	gtk_gesture_stylus_set_stylus_only(GTK_GESTURE_STYLUS(stylus), TRUE);
	g_signal_connect(stylus, "down", G_CALLBACK(on_stylus_down), self);
	g_signal_connect(stylus, "motion", G_CALLBACK(on_stylus_motion), self);
	g_signal_connect(stylus, "up", G_CALLBACK(on_stylus_up), self);
	gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(stylus));

	GtkEventController *key = gtk_event_controller_key_new();
	g_signal_connect(key, "key-pressed", G_CALLBACK(on_key_pressed), self);
	g_signal_connect(key, "key-released", G_CALLBACK(on_key_released), self);
	gtk_widget_add_controller(GTK_WIDGET(self), key);
}
