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

	The frame buffer itself is produced by the emulation layer as a pooled,
	opaque BGRA8888 GBytes. The widget wraps two zero-copy GdkMemoryTexture
	views over it and lets GSK do the scaling on the GPU, so no per-frame
	memcpy or alpha fixup happens here (the GTK3 Cairo path also draws
	straight from the filter buffer).
*/

#include "display.h"

#include <string.h>

#define DESMUME_PI 3.14159265358979323846
#define DESMUME_NDS_W 256.0
#define DESMUME_NDS_H 192.0


/* ------------------------------------------------- §6.3 presentation timing */
/* DESMUME_GTK4_TIMING=1 records the two costs the migration can actually
 * influence -- texture upload in push_frame() and GSK snapshot preparation --
 * and prints mean/p50/p95/max every TIMING_WINDOW presented frames to stderr.
 * Disabled by default: no clock reads, no samples, no allocation on the hot
 * path. */
#define TIMING_WINDOW 120

typedef struct
{
	gint64 samples[TIMING_WINDOW];
	guint count;
	gint64 total;
} TimingSeries;

typedef struct
{
	gboolean enabled;
	TimingSeries upload;
	TimingSeries snapshot;
	guint frames;
} TimingState;

static TimingState sTiming;

static gboolean
timing_init(void)
{
	static gint initialized = 0;
	if (!initialized)
	{
		initialized = 1;
		const char *env = g_getenv("DESMUME_GTK4_TIMING");
		sTiming.enabled = (env != NULL) &&
		                  ((g_strcmp0(env, "1") == 0) ||
		                   (g_ascii_strcasecmp(env, "true") == 0));
	}
	return sTiming.enabled;
}

static void
timing_add(TimingSeries *series, gint64 usec)
{
	if (series->count < TIMING_WINDOW)
	{
		series->samples[series->count++] = usec;
		series->total += usec;
	}
}

static gint
compare_gint64(gconstpointer a, gconstpointer b)
{
	const gint64 va = *(const gint64 *)a;
	const gint64 vb = *(const gint64 *)b;
	return (va < vb) ? -1 : (va > vb) ? 1 : 0;
}

static void
timing_report(const TimingSeries *series, const char *name)
{
	if (series->count == 0)
	{
		return;
	}
	gint64 sorted[TIMING_WINDOW];
	memcpy(sorted, series->samples, series->count * (gsize)sizeof(gint64));
	qsort(sorted, series->count, sizeof(gint64), compare_gint64);
	const gdouble mean = (gdouble)series->total / (gdouble)series->count;
	const gint64 p50 = sorted[series->count / 2];
	const gint64 p95 = sorted[(series->count * 95) / 100];
	const gint64 max = sorted[series->count - 1];
	g_printerr("[gtk4-timing] %-8s n=%3u mean=%6.1fus p50=%5" G_GINT64_FORMAT
	           "us p95=%5" G_GINT64_FORMAT "us max=%5" G_GINT64_FORMAT "us\n",
	           name, series->count, mean, p50, p95, max);
}

static void
timing_flush_if_due(void)
{
	/* Driven by presented frames (push_frame), after both series for this
	 * frame have been recorded.  GTK may call snapshot() a different number
	 * of times, so each report uses its own sample count. */
	if (++sTiming.frames < TIMING_WINDOW)
	{
		return;
	}
	sTiming.frames = 0;
	g_printerr("[gtk4-timing] ---- %d presented frames ----\n", TIMING_WINDOW);
	timing_report(&sTiming.upload, "upload");
	timing_report(&sTiming.snapshot, "snapshot");
	sTiming.upload.count = 0;
	sTiming.upload.total = 0;
	sTiming.snapshot.count = 0;
	sTiming.snapshot.total = 0;
}

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

	gboolean stylus_active;
	/* Primary button is held down over the touch screen.  Motion only feeds
	 * the stylus position while this is set, matching GTK3's `click` latch. */
	gboolean touch_active;

	/* §6.2: presentation throttle while the toplevel is not active. */
	gint64 last_present_us;
	gboolean inactive;
	gboolean active_signal_connected;

	/* Set by the key-event handler when the emulation bridge consumed the
	 * key.  Signals cannot return a value through g_signal_emit(), so the
	 * handler reports it here. */
	gboolean key_handled;
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

	/* §6.3: timing starts only once there is a real frame to compose. */
	const gint64 snapshot_start = sTiming.enabled ? g_get_monotonic_time() : 0;

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

	if (sTiming.enabled)
	{
		timing_add(&sTiming.snapshot, g_get_monotonic_time() - snapshot_start);
	}
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

/* ------------------------------------------------- §6.2 inactive throttling */
/* §6.2 item 2: when the toplevel loses focus, presentation drops to ~10 fps
 * while emulation and audio keep running at full speed.  The connection is
 * lazy -- on the first presented frame -- so a WM-less X server (where
 * gtk_window_is_active() may report FALSE forever) still renders normally
 * until GTK actually reports an is-active transition. */
#define INACTIVE_PRESENT_INTERVAL_US 100000 /* 10 fps */

static void
on_toplevel_is_active_changed(GObject *toplevel, GParamSpec *pspec,
                              gpointer user_data)
{
	(void)pspec;
	DesmumeDisplay *self = DESMUME_DISPLAY(user_data);
	self->inactive = !gtk_window_is_active(GTK_WINDOW(toplevel));
}

static gboolean
should_present(DesmumeDisplay *self)
{
	GtkWidget *widget = GTK_WIDGET(self);

	if (!self->active_signal_connected)
	{
		GtkRoot *root = gtk_widget_get_root(widget);
		if (GTK_IS_WINDOW(root))
		{
			self->inactive = !gtk_window_is_active(GTK_WINDOW(root));
			g_signal_connect_object(root, "notify::is-active",
			                        G_CALLBACK(on_toplevel_is_active_changed),
			                        self, (GConnectFlags)0);
			self->active_signal_connected = TRUE;
		}
	}

	if (!self->inactive)
	{
		return TRUE;
	}

	const gint64 now = g_get_monotonic_time();
	if ((self->last_present_us != 0) &&
	    ((now - self->last_present_us) < INACTIVE_PRESENT_INTERVAL_US))
	{
		return FALSE;
	}
	self->last_present_us = now;
	return TRUE;
}

void
desmume_display_push_frame(DesmumeDisplay *self, GBytes *frame,
                           int width, int height)
{
	g_return_if_fail(DESMUME_IS_DISPLAY(self));
	g_return_if_fail(frame != NULL);
	g_return_if_fail(width > 0 && height >= 2 && (height % 2) == 0);
	g_return_if_fail(g_bytes_get_size(frame) >= (gsize)width * height * 4);

	/* §6.2: an unmapped window needs no texture upload or redraw; emulation
	 * continues independently of presentation.  An unfocused toplevel is
	 * throttled to ~10 fps by should_present().  We still own `frame` on the
	 * early path and must release it so its buffer returns to the pool. */
	if (!gtk_widget_get_mapped(GTK_WIDGET(self)) || !should_present(self))
	{
		g_bytes_unref(frame);
		return;
	}

	/* §6.3: time the upload path (two zero-copy GBytes views and two
	 * GdkMemoryTextures) on presented frames only.  The color conversion and
	 * pooling happen in the emulation layer and are timed there. */
	const gint64 upload_start = sTiming.enabled ? g_get_monotonic_time() : 0;

	const int screen_h = height / 2;
	const gsize stride = (gsize)width * 4;
	const gsize screen_bytes = stride * (gsize)screen_h;

	/* Both screen textures are zero-copy views over the pooled frame. The
	 * child GBytes hold a ref to `frame`, and each GdkMemoryTexture holds a
	 * ref to its child, so the buffers stay alive until GSK is done with
	 * them. The converters already wrote alpha=0xFF, matching
	 * GDK_MEMORY_B8G8R8A8_PREMULTIPLIED, so no alpha fixup is needed. */
	GBytes *top_bytes = g_bytes_new_from_bytes(frame, 0, screen_bytes);
	GBytes *bottom_bytes = g_bytes_new_from_bytes(frame, screen_bytes, screen_bytes);

	GdkTexture *top = gdk_memory_texture_new(width, screen_h,
	                                         GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
	                                         top_bytes, stride);
	GdkTexture *bottom = gdk_memory_texture_new(width, screen_h,
	                                            GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
	                                            bottom_bytes, stride);

	g_bytes_unref(top_bytes);
	g_bytes_unref(bottom_bytes);
	g_bytes_unref(frame);

	g_clear_object(&self->tex_top);
	g_clear_object(&self->tex_bottom);
	self->tex_top = top;
	self->tex_bottom = bottom;
	self->frame_w = width;
	self->frame_h = screen_h;

	gtk_widget_queue_draw(GTK_WIDGET(self));

	if (sTiming.enabled)
	{
		timing_add(&sTiming.upload, g_get_monotonic_time() - upload_start);
		timing_flush_if_due();
	}
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

static gboolean
emit_touch(DesmumeDisplay *self, guint signal, double x, double y)
{
	double nx, ny;
	if (desmume_display_map_touch(self, x, y, &nx, &ny))
	{
		g_signal_emit(self, display_signals[signal], 0, nx, ny);
		return TRUE;
	}
	return FALSE;
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
	self->touch_active = emit_touch(self, SIG_TOUCH_PRESS, x, y);
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

	if (self->stylus_active || !self->touch_active)
	{
		return;
	}

	self->touch_active = FALSE;
	g_signal_emit(self, display_signals[SIG_TOUCH_RELEASE], 0);
}

static void
on_motion(GtkEventControllerMotion *controller,
          double x, double y, gpointer user_data)
{
	DesmumeDisplay *self = DESMUME_DISPLAY(user_data);
	(void)controller;

	/* Hovering must not touch the screen: only drag the stylus while the
	 * primary button (or a pen, via the stylus gesture) is down. */
	if (self->stylus_active || !self->touch_active)
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
	/* The pen owns the touch until it lifts, so a pending mouse press must
	 * not keep motion active afterwards. */
	self->touch_active = FALSE;
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

	self->key_handled = FALSE;
	g_signal_emit(self, display_signals[SIG_KEY_EVENT], 0, keyval, TRUE,
	              (guint)state);
	if (self->key_handled)
	{
		/* A bound game or turbo key owns the press; a modified key was not
		 * consumed by the bridge, so accelerators still work. */
		return TRUE;
	}
	if (keyval == GDK_KEY_Tab || keyval == GDK_KEY_ISO_Left_Tab)
		return TRUE;
	return FALSE; /* let accelerators (F11, Ctrl+O, ...) still work */
}

static gboolean
on_key_released(GtkEventControllerKey *controller, guint keyval,
                guint keycode, GdkModifierType state, gpointer user_data)
{
	DesmumeDisplay *self = DESMUME_DISPLAY(user_data);
	(void)controller;
	(void)keycode;

	self->key_handled = FALSE;
	g_signal_emit(self, display_signals[SIG_KEY_EVENT], 0, keyval, FALSE,
	              (guint)state);
	return self->key_handled;
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

	if (timing_init())
	{
		g_printerr("[gtk4-timing] enabled (DESMUME_GTK4_TIMING=1), "
		           "reporting every %d presented frames\n", TIMING_WINDOW);
	}

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
		NULL, NULL, NULL, G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_BOOLEAN,
		G_TYPE_UINT);
}

void
desmume_display_set_key_handled(DesmumeDisplay *self, gboolean handled)
{
	g_return_if_fail(DESMUME_IS_DISPLAY(self));
	self->key_handled = handled;
}

static void
desmume_display_init(DesmumeDisplay *self)
{
	self->layout = DESMUME_LAYOUT_VERTICAL;
	self->hybrid_scale = -1.0; /* auto, matching GTK3's draw_params */

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
