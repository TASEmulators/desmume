/*
	memoryviewer.cpp - Memory Viewer tool for the DeSmuME GTK4 frontend

	Copyright (C) 2026 DeSmuME team

	The GTK4 counterpart of the Cocoa MemoryViewerWindow and the Windows
	memView.cpp. The platform-neutral reader/writer/breakpoint layer lives in
	shared/memory_viewer_core.cpp and is identical to the one macOS uses. This
	file only builds the view: a monospace GtkDrawingArea renders a 16-byte hex
	dump with an ASCII column, editable in place through key presses, plus the
	region / address / view-mode toolbar and the breakpoint bar.

	Only visible rows are read from the emulated bus on each refresh, so even
	the 4 GB "Full" dump opens instantly.
*/

#include "memoryviewer.h"
#include "../tools.h"
#include "../../../shared/memory_viewer_core.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "../../shared/memory_viewer_layout.h"

#define TOOL_NAME "Memory Viewer"

/* ------------------------------------------------------------------ constants */

typedef enum
{
	VIEW_BYTE = 0,
	VIEW_HALFWORD,
	VIEW_WORD
} ViewMode;

typedef enum
{
	EXPORT_TEXT = 0,
	EXPORT_RAW,
	EXPORT_FULL
} ExportKind;

static const int kBytesPerRow = 16;
static const int kMaxRows = 256;                 /* bounded by sBuffer (0x1000) */
static const int kDefaultRows = 16;
static const int kOuterMargin = 8;
static const int kMaxHistory = 20;
static const gint64 kRefreshIntervalUs = 150000; /* 150 ms, like the Cocoa timer */

/* -------------------------------------------------------------------- state */

typedef struct
{
	GtkWidget *window;
	GtkWidget *drawing;
	GtkAdjustment *adjustment;
	GSettings *fontSettings;
	gboolean updatingAdjustment;
	double scrollRemainder;

	GtkWidget *regionDrop;
	GtkWidget *addressEntry;
	GtkWidget *historyDrop;
	GtkWidget *viewDrop;
	GtkWidget *selectionLabel;
	GtkWidget *exportButton;

	GtkWidget *bpEntry;
	GtkWidget *bpSummary;
	GtkWidget *fullCheck;
	GtkWidget *beCheck;

	MemoryViewerRegionType region;
	ViewMode viewMode;
	guint32 address;             /* first byte shown in the viewport */
	gboolean selectionEnabled;
	guint32 selectionAddress;
	guint32 selectionPart;       /* hex digits typed into the current cell */
	guint32 selectionNewValue;
	gboolean bigEndian;
	gboolean fullCharacters;

	int visibleRows;
	guint32 windowSize;
	guint8 buffer[0x1000];
	gint64 lastRefresh;
	gboolean updatingHistory;
} MemoryViewerState;

static MemoryViewerState sView;

/* ------------------------------------------------------------------ helpers */

static inline int
bytes_per_unit(ViewMode mode)
{
	switch (mode)
	{
		case VIEW_HALFWORD: return 2;
		case VIEW_WORD: return 4;
		default: return 1;
	}
}

static inline int
digits_per_unit(ViewMode mode)
{
	switch (mode)
	{
		case VIEW_HALFWORD: return 4;
		case VIEW_WORD: return 8;
		default: return 2;
	}
}

static int
hex_digit_value(gunichar c)
{
	if (c >= '0' && c <= '9') return (int)(c - '0');
	if (c >= 'A' && c <= 'F') return (int)(c - 'A') + 0x0A;
	if (c >= 'a' && c <= 'f') return (int)(c - 'a') + 0x0A;
	return -1;
}

static gboolean
parse_hex_address(const char *text, guint32 *out)
{
	if (text == NULL)
	{
		return FALSE;
	}
	while (*text == ' ' || *text == '\t')
	{
		++text;
	}
	size_t len = strlen(text);
	if (len > 0 && (text[len - 1] == 'h' || text[len - 1] == 'H'))
	{
		--len;
	}
	if (len >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
	{
		text += 2;
		len -= 2;
	}
	if (len == 0 || len > 8)
	{
		return FALSE;
	}

	guint32 value = 0;
	for (size_t i = 0; i < len; ++i)
	{
		const int digit = hex_digit_value((gunichar)(guchar)text[i]);
		if (digit < 0)
		{
			return FALSE;
		}
		value = (value << 4) | (guint32)digit;
	}
	*out = value;
	return TRUE;
}

static inline guint16
read_u16_le(const guint8 *p)
{
	return (guint16)((guint16)p[0] | ((guint16)p[1] << 8));
}

static inline guint32
read_u32_le(const guint8 *p)
{
	return (guint32)p[0] | ((guint32)p[1] << 8) |
	       ((guint32)p[2] << 16) | ((guint32)p[3] << 24);
}

static inline guint16
swap16(guint16 v)
{
	return (guint16)((v >> 8) | (v << 8));
}

static inline guint32
swap32(guint32 v)
{
	return ((v & 0x000000FF) << 24) | ((v & 0x0000FF00) << 8) |
	       ((v & 0x00FF0000) >> 8) | ((v & 0xFF000000) >> 24);
}

static guint32
value_at_offset(MemoryViewerState *st, int offset)
{
	switch (st->viewMode)
	{
		case VIEW_HALFWORD:
		{
			const guint16 raw = read_u16_le(&st->buffer[offset]);
			return (guint32)(st->bigEndian ? swap16(raw) : raw);
		}
		case VIEW_WORD:
		{
			const guint32 raw = read_u32_le(&st->buffer[offset]);
			return st->bigEndian ? swap32(raw) : raw;
		}
		default:
			return (guint32)st->buffer[offset];
	}
}

/* Keep the viewport inside the active region. Uses 64-bit arithmetic because
 * the CartROM/Full regions span almost the whole 32-bit space. */
static void
clamp_viewport(MemoryViewerState *st)
{
	const MemoryViewerRegionInfo *info = MemoryViewerRegionGet(st->region);
	const gint64 base = (gint64)info->hardwareAddress;
	const gint64 end = base + (gint64)info->size;
	gint64 last = end - (gint64)st->windowSize;
	if (last < base)
	{
		last = base;
	}

	gint64 address = (gint64)st->address;
	if (address < base)
	{
		address = base;
	}
	else if (address > last)
	{
		address = last;
	}
	st->address = (guint32)(address & 0xFFFFFFF0);
}

/* Re-read only the visible window from the emulated bus. */
static void
reload_buffer(MemoryViewerState *st)
{
	clamp_viewport(st);
	MemoryViewerRead(st->region, st->address, st->windowSize, st->buffer);
	if (st->adjustment != NULL)
	{
		const MemoryViewerRegionInfo *info = MemoryViewerRegionGet(st->region);
		const double totalRows = ceil((double)info->size / kBytesPerRow);
		const double pageRows = MIN((double)st->visibleRows, totalRows);
		st->updatingAdjustment = TRUE;
		gtk_adjustment_configure(st->adjustment,
		                         ((gint64)st->address - info->hardwareAddress) / kBytesPerRow,
		                         0, totalRows, 1, MAX(pageRows - 1, 1), pageRows);
		st->updatingAdjustment = FALSE;
	}
}

static void
set_selection_address(MemoryViewerState *st, guint32 address, gboolean reveal)
{
	const MemoryViewerRegionInfo *info = MemoryViewerRegionGet(st->region);
	const gint64 base = (gint64)info->hardwareAddress;
	const gint64 end = base + (gint64)info->size;

	if ((gint64)address < base)
	{
		address = (guint32)base;
	}
	if ((gint64)address >= end)
	{
		address = (guint32)(end - 1);
	}

	st->selectionEnabled = TRUE;
	st->selectionAddress = address;
	st->selectionPart = 0;
	st->selectionNewValue = 0;

	if (reveal)
	{
		if (st->selectionAddress < st->address)
		{
			st->address = st->selectionAddress & 0xFFFFFFF0;
		}
		else if ((guint64)st->selectionAddress >= (guint64)st->address + st->windowSize)
		{
			st->address = (st->selectionAddress & 0xFFFFFFF0) -
			              (st->windowSize - (guint32)kBytesPerRow);
		}
		clamp_viewport(st);
	}

	gchar label[64];
	g_snprintf(label, sizeof(label), "%08Xh", (unsigned)st->selectionAddress);
	gtk_label_set_text(GTK_LABEL(st->selectionLabel), label);

	reload_buffer(st);
	if (st->drawing != NULL)
	{
		gtk_widget_queue_draw(st->drawing);
	}
}

static void
scroll_by(MemoryViewerState *st, gint64 delta)
{
	const MemoryViewerRegionInfo *info = MemoryViewerRegionGet(st->region);
	const gint64 base = (gint64)info->hardwareAddress;
	const gint64 end = MIN(base + (gint64)info->size, (gint64)G_MAXUINT32 + 1);
	const gint64 next = CLAMP((gint64)st->address + delta, base, MAX(base, end - st->windowSize));
	st->address = (guint32)next;
	clamp_viewport(st);

	if (st->selectionEnabled &&
	    (st->selectionAddress < st->address ||
	     (guint64)st->selectionAddress >= (guint64)st->address + st->windowSize))
	{
		st->selectionEnabled = FALSE;
		st->selectionPart = 0;
		st->selectionNewValue = 0;
		gtk_label_set_text(GTK_LABEL(st->selectionLabel), "--------h");
	}

	reload_buffer(st);
	if (st->drawing != NULL)
	{
		gtk_widget_queue_draw(st->drawing);
	}
}

/* ------------------------------------------------------------------ drawing */

static void
set_text_color(cairo_t *cr, GtkWidget *widget, double alpha)
{
	GdkRGBA fg;
	gtk_widget_get_color(widget, &fg);
	cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, alpha);
}

static void
update_geometry(MemoryViewerState *st, int height)
{
	PangoLayout *layout = memory_viewer_layout_new(st->drawing, st->fontSettings);
	double char_w, line_h, header_h;
	memory_viewer_metrics(layout, &char_w, &line_h, &header_h);
	g_object_unref(layout);
	const int rows = CLAMP((int)floor((height - header_h - kOuterMargin) / line_h), 1, kMaxRows);
	if (rows != st->visibleRows)
	{
		st->visibleRows = rows;
		st->windowSize = (guint32)(rows * kBytesPerRow);
		reload_buffer(st);
	}
}

static void
on_resize(GtkDrawingArea *area, int width, int height, gpointer data)
{
	(void)area;
	(void)width;
	update_geometry((MemoryViewerState *)data, height);
}

static void
on_font_changed(GSettings *settings, gchar *key, gpointer data)
{
	(void)settings;
	(void)key;
	MemoryViewerState *st = (MemoryViewerState *)data;
	memory_viewer_set_min_width(st->drawing, st->fontSettings);
	update_geometry(st, gtk_widget_get_height(st->drawing));
	gtk_widget_queue_draw(st->drawing);
}

static void
on_adjustment_changed(GtkAdjustment *adjustment, gpointer data)
{
	MemoryViewerState *st = (MemoryViewerState *)data;
	if (!st->updatingAdjustment)
	{
		const gint64 base = MemoryViewerRegionGet(st->region)->hardwareAddress;
		const gint64 target = base + (gint64)round(gtk_adjustment_get_value(adjustment)) * kBytesPerRow;
		scroll_by(st, target - st->address);
	}
}

static void
on_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data)
{
	MemoryViewerState *st = (MemoryViewerState *)data;
	(void)height;
	PangoLayout *layout = memory_viewer_layout_new(GTK_WIDGET(area), st->fontSettings);
	double char_w, line_h, header_h;
	memory_viewer_metrics(layout, &char_w, &line_h, &header_h);
	const int rows = st->visibleRows;

	const int step = bytes_per_unit(st->viewMode);
	const int digits = digits_per_unit(st->viewMode);
	const int units = kBytesPerRow / step;

	const double data_x = kOuterMargin + 8.0 * char_w + 2.0 * char_w;
	const double unit_w = ((double)digits + 1.0) * char_w;
	const double ascii_x = data_x + (double)units * unit_w + 2.0 * char_w;

	/* Column headers. */
	set_text_color(cr, GTK_WIDGET(area), 0.65);
	for (int unit = 0; unit < units; ++unit)
	{
		gchar text[8];
		g_snprintf(text, sizeof(text), "%02X", unit * step);
		memory_viewer_draw_cells(cr, layout, text, data_x + (double)unit * unit_w, 1.0, char_w, line_h);
	}
	memory_viewer_draw_cells(cr, layout, "0123456789ABCDEF", ascii_x, 1.0, char_w, line_h);

	cairo_set_line_width(cr, 1.0);
	cairo_move_to(cr, kOuterMargin, header_h - 1.0);
	cairo_line_to(cr, width - kOuterMargin, header_h - 1.0);
	cairo_stroke(cr);

	const gboolean has_breakpoints =
	    (MemoryViewerReadBreakpointCount() + MemoryViewerWriteBreakpointCount()) > 0;
	const guint8 endChar = st->fullCharacters ? 0xFF : 0x7F;
	gchar text[32];

	for (int row = 0; row < rows; ++row)
	{
		const double y = header_h + (double)row * line_h;
		const guint32 rowAddress = st->address + (guint32)(row * kBytesPerRow);

		set_text_color(cr, GTK_WIDGET(area), 0.65);
		g_snprintf(text, sizeof(text), "%08X", (unsigned)rowAddress);
		memory_viewer_draw_cells(cr, layout, text, kOuterMargin, y, char_w, line_h);

		for (int unit = 0; unit < units; ++unit)
		{
			const int offset = row * kBytesPerRow + unit * step;
			const guint32 cellAddress = st->address + (guint32)offset;
			const double x = data_x + (double)unit * unit_w;
			const guint32 value = value_at_offset(st, offset);
			const gboolean selected = st->selectionEnabled && (st->selectionAddress == cellAddress);

			if (selected)
			{
				cairo_set_source_rgba(cr, 0.25, 0.50, 0.95, 0.35);
				cairo_rectangle(cr, x - 1.0, y - 1.0, (double)digits * char_w + 2.0, line_h);
				cairo_fill(cr);

				if (st->selectionPart > 0)
				{
					const guint32 entered = MIN(st->selectionPart, (guint32)digits);
					const guint32 mask = (entered >= 8) ? 0xFFFFFFFFu
					                                    : ((1u << (entered * 4)) - 1u);
					gchar typed[16];
					int pos = 0;
					const guint32 shown = st->selectionNewValue & mask;
					g_snprintf(typed, sizeof(typed), "%0*X", (int)entered, (unsigned)shown);
					pos = (int)strlen(typed);
					for (guint32 i = entered; i < (guint32)digits && pos < 15; ++i)
					{
						typed[pos++] = '.';
					}
					typed[pos] = '\0';
					set_text_color(cr, GTK_WIDGET(area), 1.0);
					memory_viewer_draw_cells(cr, layout, typed, x, y, char_w, line_h);
				}
				else
				{
					g_snprintf(text, sizeof(text), "%0*X", digits, (unsigned)value);
					set_text_color(cr, GTK_WIDGET(area), 1.0);
					memory_viewer_draw_cells(cr, layout, text, x, y, char_w, line_h);
				}
			}
			else if (has_breakpoints && MemoryViewerIsBreakpoint(cellAddress))
			{
				cairo_set_source_rgb(cr, 0.85, 0.15, 0.15);
				cairo_rectangle(cr, x - 1.0, y - 1.0, (double)digits * char_w + 2.0, line_h);
				cairo_fill(cr);
				g_snprintf(text, sizeof(text), "%0*X", digits, (unsigned)value);
				cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
				memory_viewer_draw_cells(cr, layout, text, x, y, char_w, line_h);
			}
			else
			{
				g_snprintf(text, sizeof(text), "%0*X", digits, (unsigned)value);
				set_text_color(cr, GTK_WIDGET(area), 1.0);
				memory_viewer_draw_cells(cr, layout, text, x, y, char_w, line_h);
			}
		}

		/* ASCII column. */
		for (int i = 0; i < kBytesPerRow; ++i)
		{
			const guint8 value = st->buffer[row * kBytesPerRow + i];
			const gunichar c = (value >= 0x20 && value <= endChar && g_unichar_isprint(value)) ? value : '.';
			gchar asciiText[7] = { 0 };
			g_unichar_to_utf8(c, asciiText);
			const guint32 byteAddress = rowAddress + (guint32)i;
			const gboolean selectedByte = st->selectionEnabled &&
			                              (st->selectionAddress == byteAddress);
			const double x = ascii_x + (double)i * char_w;

			if (selectedByte)
			{
				cairo_set_source_rgba(cr, 0.25, 0.50, 0.95, 0.35);
				cairo_rectangle(cr, x, y - 1.0, char_w, line_h);
				cairo_fill(cr);
			}
			set_text_color(cr, GTK_WIDGET(area), 1.0);
			memory_viewer_draw_cells(cr, layout, asciiText, x, y, char_w, line_h);
		}
	}

	g_object_unref(layout);
}

/* ------------------------------------------------------------------ editing */

static void
input_hex_digit(MemoryViewerState *st, int digit)
{
	if (!st->selectionEnabled || !MemoryViewerIsAvailable(st->region, st->selectionAddress))
	{
		return;
	}

	st->selectionNewValue = (st->selectionNewValue << 4) | (guint32)digit;
	++st->selectionPart;
	const int digits = digits_per_unit(st->viewMode);

	if ((int)st->selectionPart >= digits)
	{
		switch (st->viewMode)
		{
			case VIEW_HALFWORD:
			{
				const guint16 value = st->bigEndian
					? swap16((guint16)st->selectionNewValue)
					: (guint16)st->selectionNewValue;
				MemoryViewerWrite16(st->region, st->selectionAddress, value);
				break;
			}
			case VIEW_WORD:
			{
				const guint32 value = st->bigEndian
					? swap32(st->selectionNewValue)
					: st->selectionNewValue;
				MemoryViewerWrite32(st->region, st->selectionAddress, value);
				break;
			}
			default:
				MemoryViewerWrite8(st->region, st->selectionAddress, (guint8)st->selectionNewValue);
				break;
		}

		st->selectionPart = 0;
		st->selectionNewValue = 0;
		const guint32 next = st->selectionAddress + (guint32)bytes_per_unit(st->viewMode);
		const MemoryViewerRegionInfo *info = MemoryViewerRegionGet(st->region);
		if (next < info->hardwareAddress + info->size)
		{
			set_selection_address(st, next, TRUE);
			return;
		}
	}

	reload_buffer(st);
	gtk_widget_queue_draw(st->drawing);
}

/* ------------------------------------------------------------------ events */

static gboolean
on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode,
               GdkModifierType modifiers, gpointer data)
{
	(void)controller;
	(void)keycode;
	MemoryViewerState *st = (MemoryViewerState *)data;
	const gboolean ctrl = (modifiers & GDK_CONTROL_MASK) != 0;
	const int step = bytes_per_unit(st->viewMode);
	const MemoryViewerRegionInfo *info = MemoryViewerRegionGet(st->region);

	switch (keyval)
	{
		case GDK_KEY_Left:
			set_selection_address(st, st->selectionEnabled ? st->selectionAddress - step : st->address, TRUE);
			return TRUE;
		case GDK_KEY_Right:
			set_selection_address(st, st->selectionEnabled ? st->selectionAddress + step : st->address, TRUE);
			return TRUE;
		case GDK_KEY_Up:
			set_selection_address(st, st->selectionEnabled ? st->selectionAddress - kBytesPerRow : st->address, TRUE);
			return TRUE;
		case GDK_KEY_Down:
			set_selection_address(st, st->selectionEnabled ? st->selectionAddress + kBytesPerRow : st->address, TRUE);
			return TRUE;
		case GDK_KEY_Page_Up:
			scroll_by(st, -(gint64)st->windowSize);
			set_selection_address(st, st->address, FALSE);
			return TRUE;
		case GDK_KEY_Page_Down:
			scroll_by(st, (gint64)st->windowSize);
			set_selection_address(st, st->address, FALSE);
			return TRUE;
		case GDK_KEY_Home:
			if (ctrl)
			{
				set_selection_address(st, info->hardwareAddress, TRUE);
			}
			else
			{
				set_selection_address(st, st->address, FALSE);
			}
			return TRUE;
		case GDK_KEY_End:
			if (ctrl)
			{
				const guint32 last = (info->hardwareAddress + info->size - kBytesPerRow) & 0xFFFFFFF0;
				set_selection_address(st, last, TRUE);
			}
			else
			{
				set_selection_address(st, st->address + st->windowSize - 1, FALSE);
			}
			return TRUE;
		default:
			break;
	}

	const gunichar unicode = gdk_keyval_to_unicode(keyval);
	const int digit = hex_digit_value(unicode);
	if (digit >= 0)
	{
		input_hex_digit(st, digit);
		return TRUE;
	}
	return FALSE;
}

static void
on_click_pressed(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer data)
{
	(void)gesture;
	(void)n_press;
	MemoryViewerState *st = (MemoryViewerState *)data;

	gtk_widget_grab_focus(st->drawing);

	PangoLayout *layout = memory_viewer_layout_new(st->drawing, st->fontSettings);
	double char_w, line_h, header_h;
	memory_viewer_metrics(layout, &char_w, &line_h, &header_h);
	g_object_unref(layout);

	if (y < header_h)
	{
		return;
	}
	const int row = (int)floor((y - header_h) / line_h);
	if (row < 0 || row >= st->visibleRows)
	{
		return;
	}

	const int step = bytes_per_unit(st->viewMode);
	const int digits = digits_per_unit(st->viewMode);
	const int units = kBytesPerRow / step;
	const double data_x = kOuterMargin + 8.0 * char_w + 2.0 * char_w;
	const double unit_w = ((double)digits + 1.0) * char_w;
	const double ascii_x = data_x + (double)units * unit_w + 2.0 * char_w;

	if (x >= data_x && x < data_x + (double)units * unit_w)
	{
		const int unit = (int)floor((x - data_x) / unit_w);
		const guint32 address = st->address + (guint32)(row * kBytesPerRow + unit * step);
		set_selection_address(st, address, FALSE);
	}
	else if (x >= ascii_x && x < ascii_x + (double)kBytesPerRow * char_w)
	{
		const int byte = (int)floor((x - ascii_x) / char_w);
		const guint32 address = st->address + (guint32)(row * kBytesPerRow + byte);
		set_selection_address(st, address, FALSE);
	}
}

static gboolean
on_scroll(GtkEventControllerScroll *controller, gdouble dx, gdouble dy, gpointer data)
{
	(void)dx;
	MemoryViewerState *st = (MemoryViewerState *)data;
	if (dy == 0.0)
	{
		return FALSE;
	}
	if (gtk_event_controller_scroll_get_unit(controller) == GDK_SCROLL_UNIT_SURFACE)
	{
		PangoLayout *layout = memory_viewer_layout_new(st->drawing, st->fontSettings);
		double char_w, line_h, header_h;
		memory_viewer_metrics(layout, &char_w, &line_h, &header_h);
		g_object_unref(layout);
		dy /= line_h;
	}
	st->scrollRemainder += dy;
	const gint64 rows = (gint64)st->scrollRemainder;
	st->scrollRemainder -= rows;
	if (rows != 0)
		scroll_by(st, rows * kBytesPerRow);
	return TRUE;
}

/* ------------------------------------------------------------ address / toolbar */

static void
add_address_to_history(MemoryViewerState *st, guint32 address)
{
	gchar entry[16];
	g_snprintf(entry, sizeof(entry), "%08X", (unsigned)address);

	gtk_editable_set_text(GTK_EDITABLE(st->addressEntry), entry);

	GtkStringList *list = GTK_STRING_LIST(gtk_drop_down_get_model(GTK_DROP_DOWN(st->historyDrop)));
	guint count = g_list_model_get_n_items(G_LIST_MODEL(list));

	const gchar *strings[kMaxHistory + 1];
	guint used = 0;
	strings[used++] = entry;
	for (guint i = 0; i < count && used < kMaxHistory; ++i)
	{
		const gchar *item = gtk_string_list_get_string(list, i);
		if (item != NULL && g_strcmp0(item, entry) != 0)
		{
			strings[used++] = item;
		}
	}

	st->updatingHistory = TRUE;
	gtk_string_list_splice(list, 0, count, strings);
	gtk_drop_down_set_selected(GTK_DROP_DOWN(st->historyDrop), 0);
	st->updatingHistory = FALSE;
}

static void
jump_to_address(MemoryViewerState *st, guint32 address, gboolean remember)
{
	st->address = address;
	clamp_viewport(st);
	set_selection_address(st, st->address, FALSE);
	if (remember)
	{
		add_address_to_history(st, st->address);
	}
}

static void
on_go_clicked(GtkButton *button, gpointer data)
{
	(void)button;
	MemoryViewerState *st = (MemoryViewerState *)data;
	const char *text = gtk_editable_get_text(GTK_EDITABLE(st->addressEntry));
	guint32 address = 0;
	if (parse_hex_address(text, &address))
	{
		jump_to_address(st, address, TRUE);
	}
}

static void
on_history_selected(GObject *object, GParamSpec *pspec, gpointer data)
{
	(void)pspec;
	MemoryViewerState *st = (MemoryViewerState *)data;
	if (st->updatingHistory)
	{
		return;
	}
	GtkDropDown *drop = GTK_DROP_DOWN(object);
	const guint selected = gtk_drop_down_get_selected(drop);
	if (selected == GTK_INVALID_LIST_POSITION)
	{
		return;
	}
	GtkStringList *list = GTK_STRING_LIST(gtk_drop_down_get_model(drop));
	const gchar *text = gtk_string_list_get_string(list, selected);
	guint32 address = 0;
	if (text != NULL && parse_hex_address(text, &address))
	{
		jump_to_address(st, address, FALSE);
	}
}

static void
on_region_changed(GtkDropDown *drop, GParamSpec *pspec, gpointer data)
{
	(void)pspec;
	MemoryViewerState *st = (MemoryViewerState *)data;
	const guint index = gtk_drop_down_get_selected(drop);
	if (index >= MEMVIEW_REGION_COUNT)
	{
		return;
	}
	st->region = (MemoryViewerRegionType)index;
	st->selectionEnabled = FALSE;
	st->selectionPart = 0;
	st->selectionNewValue = 0;
	st->address = MemoryViewerRegionGet(st->region)->hardwareAddress;
	clamp_viewport(st);
	reload_buffer(st);

	gchar title[96];
	g_snprintf(title, sizeof(title), TOOL_NAME " : %s",
	           MemoryViewerRegionGetLongName(st->region));
	gtk_window_set_title(GTK_WINDOW(st->window), title);
	gtk_label_set_text(GTK_LABEL(st->selectionLabel), "--------h");
	gtk_widget_queue_draw(st->drawing);
}

static void
on_view_mode_changed(GtkDropDown *drop, GParamSpec *pspec, gpointer data)
{
	(void)pspec;
	MemoryViewerState *st = (MemoryViewerState *)data;
	const guint index = gtk_drop_down_get_selected(drop);
	if (index > VIEW_WORD)
	{
		return;
	}
	st->viewMode = (ViewMode)index;
	st->selectionPart = 0;
	st->selectionNewValue = 0;
	reload_buffer(st);
	gtk_widget_queue_draw(st->drawing);
}

static void
on_big_endian_toggled(GtkCheckButton *button, gpointer data)
{
	MemoryViewerState *st = (MemoryViewerState *)data;
	st->bigEndian = gtk_check_button_get_active(button);
	reload_buffer(st);
	gtk_widget_queue_draw(st->drawing);
}

static void
on_full_chars_toggled(GtkCheckButton *button, gpointer data)
{
	MemoryViewerState *st = (MemoryViewerState *)data;
	st->fullCharacters = gtk_check_button_get_active(button);
	gtk_widget_queue_draw(st->drawing);
}

/* --------------------------------------------------------------- breakpoints */

static void
update_breakpoint_summary(MemoryViewerState *st)
{
	const size_t reads = MemoryViewerReadBreakpointCount();
	const size_t writes = MemoryViewerWriteBreakpointCount();
	gchar text[128];
	if (reads == 0 && writes == 0)
	{
		g_snprintf(text, sizeof(text), "no breakpoints");
	}
	else
	{
		g_snprintf(text, sizeof(text), "R:%zu W:%zu", reads, writes);
	}
	gtk_label_set_text(GTK_LABEL(st->bpSummary), text);
}

static guint32
breakpoint_address(MemoryViewerState *st)
{
	const char *text = gtk_editable_get_text(GTK_EDITABLE(st->bpEntry));
	guint32 address = 0;
	if (parse_hex_address(text, &address))
	{
		return address;
	}
	if (st->selectionEnabled)
	{
		return st->selectionAddress;
	}
	return st->address;
}

static void
on_add_read_bp(GtkButton *button, gpointer data)
{
	(void)button;
	MemoryViewerState *st = (MemoryViewerState *)data;
	const guint32 address = breakpoint_address(st);
	MemoryViewerAddReadBreakpoint(address);
	gchar text[16];
	g_snprintf(text, sizeof(text), "%08X", (unsigned)address);
	gtk_editable_set_text(GTK_EDITABLE(st->bpEntry), text);
	update_breakpoint_summary(st);
	gtk_widget_queue_draw(st->drawing);
}

static void
on_add_write_bp(GtkButton *button, gpointer data)
{
	(void)button;
	MemoryViewerState *st = (MemoryViewerState *)data;
	const guint32 address = breakpoint_address(st);
	MemoryViewerAddWriteBreakpoint(address);
	gchar text[16];
	g_snprintf(text, sizeof(text), "%08X", (unsigned)address);
	gtk_editable_set_text(GTK_EDITABLE(st->bpEntry), text);
	update_breakpoint_summary(st);
	gtk_widget_queue_draw(st->drawing);
}

static void
on_remove_bp(GtkButton *button, gpointer data)
{
	(void)button;
	MemoryViewerState *st = (MemoryViewerState *)data;
	const guint32 address = breakpoint_address(st);
	MemoryViewerRemoveReadBreakpoint(address);
	MemoryViewerRemoveWriteBreakpoint(address);
	update_breakpoint_summary(st);
	gtk_widget_queue_draw(st->drawing);
}

static void
on_clear_bp(GtkButton *button, gpointer data)
{
	(void)button;
	MemoryViewerState *st = (MemoryViewerState *)data;
	MemoryViewerClearReadBreakpoints();
	MemoryViewerClearWriteBreakpoints();
	update_breakpoint_summary(st);
	gtk_widget_queue_draw(st->drawing);
}

/* -------------------------------------------------------------------- export */

typedef struct
{
	ExportKind kind;
	MemoryViewerRegionType region;
	guint32 address;
	guint32 windowSize;
	ViewMode viewMode;
} ExportCtx;

static void
free_export_ctx(gpointer data)
{
	delete (ExportCtx *)data;
}

static void
on_export_saved(GObject *source, GAsyncResult *result, gpointer data)
{
	ExportCtx *ctx = (ExportCtx *)data;
	GError *error = NULL;
	GFile *file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result, &error);
	if (file == NULL)
	{
		g_clear_error(&error);
		free_export_ctx(ctx);
		return;
	}
	char *path = g_file_get_path(file);
	g_object_unref(file);
	if (path == NULL)
	{
		free_export_ctx(ctx);
		return;
	}

	const MemoryViewerRegionType region = ctx->region;
	const guint32 address = ctx->address;
	const guint32 windowSize = ctx->windowSize;
	const ViewMode mode = ctx->viewMode;
	const ExportKind kind = ctx->kind;

	guint8 *buffer = (guint8 *)g_malloc(windowSize);
	if (buffer != NULL)
	{
		if (kind == EXPORT_FULL)
		{
			const guint32 total = MemoryViewerRegionDumpSize(region);
			const guint32 base = (region == MEMVIEW_ROM) ? 0
			                                            : MemoryViewerRegionGet(region)->hardwareAddress;
			FILE *fp = fopen(path, "wb");
			if (fp != NULL && total > 0)
			{
				const guint32 block = 0x1000;
				for (guint32 offset = 0; offset < total; offset += block)
				{
					const guint32 chunk = MIN(block, total - offset);
					MemoryViewerRead(region, base + offset, chunk, buffer);
					fwrite(buffer, 1, chunk, fp);
				}
				fclose(fp);
			}
			else if (fp != NULL)
			{
				fclose(fp);
			}
		}
		else
		{
			MemoryViewerRead(region, address, windowSize, buffer);
			FILE *fp = fopen(path, (kind == EXPORT_TEXT) ? "a" : "ab");
			if (fp != NULL)
			{
				if (kind == EXPORT_RAW)
				{
					fwrite(buffer, 1, windowSize, fp);
				}
				else
				{
					const int step = bytes_per_unit(mode);
					const int digits = digits_per_unit(mode);
					const int units = kBytesPerRow / step;
					for (guint32 row = 0; row + kBytesPerRow <= windowSize; row += kBytesPerRow)
					{
						fprintf(fp, "%08X\t\t", (unsigned)(address + row));
						for (int unit = 0; unit < units; ++unit)
						{
							const int offset = (int)row + unit * step;
							guint32 value = 0;
							switch (mode)
							{
								case VIEW_HALFWORD: value = read_u16_le(&buffer[offset]); break;
								case VIEW_WORD: value = read_u32_le(&buffer[offset]); break;
								default: value = buffer[offset]; break;
							}
							fprintf(fp, "%0*X ", digits, (unsigned)value);
						}
						fprintf(fp, "\t\t");
						for (int i = 0; i < kBytesPerRow; ++i)
						{
							const guint8 value = buffer[row + (guint32)i];
							fputc((value >= 0x20 && value <= 0x7F) ? value : '.', fp);
						}
						fputc('\n', fp);
					}
				}
				fclose(fp);
			}
		}
		g_free(buffer);
	}
	g_free(path);
	free_export_ctx(ctx);
}

static void
present_export(MemoryViewerState *st, ExportKind kind)
{
	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Export memory dump");
	gtk_file_dialog_set_modal(dialog, TRUE);

	const MemoryViewerRegionInfo *info = MemoryViewerRegionGet(st->region);
	gchar name[64];
	if (kind == EXPORT_FULL)
	{
		g_snprintf(name, sizeof(name), "%s_%08X.bin", info->name,
		           (unsigned)((st->region == MEMVIEW_ROM) ? 0 : info->hardwareAddress));
	}
	else
	{
		g_snprintf(name, sizeof(name), "memory_%08X.%s", (unsigned)st->address,
		           (kind == EXPORT_TEXT) ? "txt" : "bin");
	}
	gtk_file_dialog_set_initial_name(dialog, name);

	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, (kind == EXPORT_TEXT) ? "Text dump (*.txt)" : "Binary dump (*.bin)");
	gtk_file_filter_add_pattern(filter, (kind == EXPORT_TEXT) ? "*.txt" : "*.bin");
	GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
	g_list_store_append(filters, filter);
	g_object_unref(filter);
	gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
	g_object_unref(filters);

	ExportCtx *ctx = new ExportCtx();
	ctx->kind = kind;
	ctx->region = st->region;
	ctx->address = st->address;
	ctx->windowSize = st->windowSize;
	ctx->viewMode = st->viewMode;
	gtk_file_dialog_save(dialog, GTK_WINDOW(st->window), NULL, on_export_saved, ctx);
	g_object_unref(dialog);
}

static void
on_export_action(GSimpleAction *action, GVariant *parameter, gpointer data)
{
	(void)parameter;
	MemoryViewerState *st = (MemoryViewerState *)data;
	const char *name = g_action_get_name(G_ACTION(action));
	if (g_strcmp0(name, "export-text") == 0)
	{
		present_export(st, EXPORT_TEXT);
	}
	else if (g_strcmp0(name, "export-raw") == 0)
	{
		present_export(st, EXPORT_RAW);
	}
	else if (g_strcmp0(name, "export-all") == 0)
	{
		const guint32 total = MemoryViewerRegionDumpSize(st->region);
		if (total == 0)
		{
			GtkAlertDialog *alert = gtk_alert_dialog_new(TOOL_NAME);
			gtk_alert_dialog_set_message(alert,
				"The \"Full\" region spans the entire address space and \"CartROM\" is "
				"empty without a loaded ROM. Select ARM9, ARM7 or Firmware instead.");
			gtk_alert_dialog_choose(alert, GTK_WINDOW(st->window), NULL, NULL, NULL);
			g_object_unref(alert);
			return;
		}
		present_export(st, EXPORT_FULL);
	}
}

/* --------------------------------------------------------------- tool plumbing */

static void
clear_state(void)
{
	g_clear_object(&sView.fontSettings);
	sView.window = NULL;
	sView.drawing = NULL;
	sView.adjustment = NULL;
	sView.regionDrop = NULL;
	sView.addressEntry = NULL;
	sView.historyDrop = NULL;
	sView.viewDrop = NULL;
	sView.selectionLabel = NULL;
	sView.exportButton = NULL;
	sView.bpEntry = NULL;
	sView.bpSummary = NULL;
	sView.fullCheck = NULL;
	sView.beCheck = NULL;
}

static void
on_window_close_request(GtkWindow *window, gpointer data)
{
	(void)data;
	clear_state();
	gtk_window_destroy(window);
}

static GtkWidget *
add_toolbar_button(GtkWidget *parent, const char *label, GCallback callback, gpointer data)
{
	GtkWidget *button = gtk_button_new_with_mnemonic(label);
	g_signal_connect(button, "clicked", callback, data);
	gtk_box_append(GTK_BOX(parent), button);
	return button;
}

static void
build_history_menu_actions(MemoryViewerState *st)
{
	GSimpleActionGroup *group = g_simple_action_group_new();
	struct { const char *name; ExportKind kind; } actions[] =
	{
		{ "export-text", EXPORT_TEXT },
		{ "export-raw",  EXPORT_RAW },
		{ "export-all",  EXPORT_FULL }
	};
	for (guint i = 0; i < G_N_ELEMENTS(actions); ++i)
	{
		GSimpleAction *action = g_simple_action_new(actions[i].name, NULL);
		g_signal_connect(action, "activate", G_CALLBACK(on_export_action), st);
		g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
		g_object_unref(action);
	}
	gtk_widget_insert_action_group(st->window, "memview", G_ACTION_GROUP(group));
	g_object_unref(group);

	GMenu *menu = g_menu_new();
	g_menu_append(menu, "Text Dump…", "memview.export-text");
	g_menu_append(menu, "Raw Dump…", "memview.export-raw");
	g_menu_append(menu, "Dump All…", "memview.export-all");
	gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(st->exportButton), G_MENU_MODEL(menu));
	g_object_unref(menu);
}

static void
Open(void)
{
	if (sView.window != NULL)
	{
		gtk_window_present(GTK_WINDOW(sView.window));
		return;
	}

	MemoryViewerState *st = &sView;
	memset(st, 0, offsetof(MemoryViewerState, visibleRows));
	st->region = MEMVIEW_ARM9;
	st->viewMode = VIEW_BYTE;
	st->visibleRows = kDefaultRows;
	st->windowSize = kDefaultRows * kBytesPerRow;
	st->address = MemoryViewerRegionGet(st->region)->hardwareAddress;
	st->lastRefresh = 0;
	st->fontSettings = memory_viewer_font_settings();

	MemoryViewerResetRegions();

	st->window = desmume_tool_window_new(TOOL_NAME " : ARM9 memory", NULL, 680, 460);
	g_object_add_weak_pointer(G_OBJECT(st->window), (gpointer *)&st->window);
	g_signal_connect(st->window, "close-request",
	                 G_CALLBACK(on_window_close_request), NULL);

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_set_margin_start(root, 6);
	gtk_widget_set_margin_end(root, 6);
	gtk_widget_set_margin_top(root, 6);
	gtk_widget_set_margin_bottom(root, 6);
	gtk_window_set_child(GTK_WINDOW(st->window), root);

	/* --- toolbar --- */
	GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_append(GTK_BOX(root), toolbar);

	GtkStringList *regions = gtk_string_list_new(NULL);
	for (int i = 0; i < MEMVIEW_REGION_COUNT; ++i)
	{
		gtk_string_list_append(regions, MemoryViewerRegionGetName((MemoryViewerRegionType)i));
	}
	st->regionDrop = gtk_drop_down_new(G_LIST_MODEL(regions), NULL);
	gtk_widget_set_tooltip_text(st->regionDrop, "Memory region to inspect");
	g_signal_connect(st->regionDrop, "notify::selected", G_CALLBACK(on_region_changed), st);
	gtk_box_append(GTK_BOX(toolbar), st->regionDrop);

	GtkWidget *addressLabel = gtk_label_new_with_mnemonic("_Address:");
	gtk_box_append(GTK_BOX(toolbar), addressLabel);
	st->addressEntry = gtk_entry_new();
	gtk_editable_set_width_chars(GTK_EDITABLE(st->addressEntry), 10);
	gtk_widget_set_tooltip_text(st->addressEntry, "Hex address to jump to (e.g. 02000000 or 0x02000000h)");
	gtk_label_set_mnemonic_widget(GTK_LABEL(addressLabel), st->addressEntry);
	g_signal_connect(st->addressEntry, "activate", G_CALLBACK(on_go_clicked), st);
	gtk_box_append(GTK_BOX(toolbar), st->addressEntry);
	add_toolbar_button(toolbar, "_Go", G_CALLBACK(on_go_clicked), st);

	st->historyDrop = gtk_drop_down_new(G_LIST_MODEL(gtk_string_list_new(NULL)), NULL);
	gtk_widget_set_tooltip_text(st->historyDrop, "Recently visited addresses");
	g_signal_connect(st->historyDrop, "notify::selected", G_CALLBACK(on_history_selected), st);
	gtk_box_append(GTK_BOX(toolbar), st->historyDrop);

	GtkStringList *modes = gtk_string_list_new(NULL);
	gtk_string_list_append(modes, "Bytes");
	gtk_string_list_append(modes, "Halfwords");
	gtk_string_list_append(modes, "Words");
	st->viewDrop = gtk_drop_down_new(G_LIST_MODEL(modes), NULL);
	gtk_widget_set_tooltip_text(st->viewDrop, "Cell width");
	g_signal_connect(st->viewDrop, "notify::selected", G_CALLBACK(on_view_mode_changed), st);
	gtk_box_append(GTK_BOX(toolbar), st->viewDrop);

	st->selectionLabel = gtk_label_new("--------h");
	gtk_widget_add_css_class(st->selectionLabel, "desmume-tool-cell");
	gtk_box_append(GTK_BOX(toolbar), st->selectionLabel);

	st->exportButton = gtk_menu_button_new();
	gtk_menu_button_set_label(GTK_MENU_BUTTON(st->exportButton), "E_xport");
	gtk_widget_set_tooltip_text(st->exportButton,
	                            "Dump the visible window as text/raw bytes, or the whole region");
	gtk_box_append(GTK_BOX(toolbar), st->exportButton);
	build_history_menu_actions(st);

	/* --- hex dump --- */
	st->drawing = gtk_drawing_area_new();
	gtk_widget_set_focusable(st->drawing, TRUE);
	gtk_widget_set_hexpand(st->drawing, TRUE);
	gtk_widget_set_vexpand(st->drawing, TRUE);
	memory_viewer_set_min_width(st->drawing, st->fontSettings);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(st->drawing), on_draw, st, NULL);
	GtkWidget *dumpBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_vexpand(dumpBox, TRUE);
	gtk_box_append(GTK_BOX(root), dumpBox);
	gtk_box_append(GTK_BOX(dumpBox), st->drawing);
	st->adjustment = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 1, 1, 1, 1));
	GtkWidget *scrollbar = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, st->adjustment);
	gtk_box_append(GTK_BOX(dumpBox), scrollbar);
	g_signal_connect(st->adjustment, "value-changed", G_CALLBACK(on_adjustment_changed), st);
	g_signal_connect(st->drawing, "resize", G_CALLBACK(on_resize), st);
	if (st->fontSettings != NULL)
		g_signal_connect(st->fontSettings, "changed::monospace-font-name", G_CALLBACK(on_font_changed), st);

	GtkEventController *key = gtk_event_controller_key_new();
	g_signal_connect(key, "key-pressed", G_CALLBACK(on_key_pressed), st);
	gtk_widget_add_controller(st->drawing, key);

	GtkGesture *click = gtk_gesture_click_new();
	g_signal_connect(click, "pressed", G_CALLBACK(on_click_pressed), st);
	gtk_widget_add_controller(st->drawing, GTK_EVENT_CONTROLLER(click));

	GtkEventController *scroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
	g_signal_connect(scroll, "scroll", G_CALLBACK(on_scroll), st);
	gtk_widget_add_controller(st->drawing, scroll);

	/* --- breakpoint bar --- */
	GtkWidget *bpBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_append(GTK_BOX(root), bpBar);
	gtk_box_append(GTK_BOX(bpBar), gtk_label_new("BP:"));
	st->bpEntry = gtk_entry_new();
	gtk_editable_set_width_chars(GTK_EDITABLE(st->bpEntry), 10);
	gtk_entry_set_placeholder_text(GTK_ENTRY(st->bpEntry), "selected");
	gtk_widget_set_tooltip_text(st->bpEntry,
	                            "Breakpoint address; empty uses the selected byte");
	gtk_box_append(GTK_BOX(bpBar), st->bpEntry);
	add_toolbar_button(bpBar, "R+", G_CALLBACK(on_add_read_bp), st);
	add_toolbar_button(bpBar, "W+", G_CALLBACK(on_add_write_bp), st);
	add_toolbar_button(bpBar, "-", G_CALLBACK(on_remove_bp), st);
	add_toolbar_button(bpBar, "Clear", G_CALLBACK(on_clear_bp), st);
	st->bpSummary = gtk_label_new("no breakpoints");
	gtk_box_append(GTK_BOX(bpBar), st->bpSummary);

	st->fullCheck = gtk_check_button_new_with_label("Full");
	gtk_widget_set_tooltip_text(st->fullCheck, "Show printable characters up to 0xFF");
	g_signal_connect(st->fullCheck, "toggled", G_CALLBACK(on_full_chars_toggled), st);
	gtk_box_append(GTK_BOX(bpBar), st->fullCheck);
	st->beCheck = gtk_check_button_new_with_label("BE");
	gtk_widget_set_tooltip_text(st->beCheck, "Interpret halfwords and words as big-endian");
	g_signal_connect(st->beCheck, "toggled", G_CALLBACK(on_big_endian_toggled), st);
	gtk_box_append(GTK_BOX(bpBar), st->beCheck);

	update_breakpoint_summary(st);
	reload_buffer(st);
	gtk_widget_grab_focus(st->drawing);
	gtk_window_present(GTK_WINDOW(st->window));
}

static void
Update(void)
{
	MemoryViewerState *st = &sView;
	if ((st->window == NULL) || !gtk_widget_get_mapped(st->window))
	{
		return;
	}

	const gint64 now = g_get_monotonic_time();
	if ((st->lastRefresh != 0) && (now - st->lastRefresh < kRefreshIntervalUs))
	{
		return;
	}
	st->lastRefresh = now;

	reload_buffer(st);
	update_breakpoint_summary(st);
	gtk_widget_queue_draw(st->drawing);
}

static void
Close(void)
{
	if (sView.window != NULL)
	{
		gtk_window_destroy(GTK_WINDOW(sView.window));
	}
	clear_state();
}

static gboolean
IsOpen(void)
{
	return sView.window != NULL;
}

const DesmumeTool desmume_tool_memoryviewer =
{
	"memoryviewer",
	"Memory Viewer",
	Open,
	Update,
	Close,
	IsOpen
};
