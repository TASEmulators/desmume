/* memoryViewer.cpp - this file is part of DeSmuME
 *
 * Copyright (C) 2006-2026 DeSmuME Team
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

// GTK3 host for the Memory Viewer. This is a port of the GTK4
// tools/memoryviewer.cpp (which in turn mirrors the Cocoa MemoryViewerWindow
// and the Windows memView.cpp). The platform-neutral reader/writer/breakpoint
// layer lives in shared/memory_viewer_core.cpp. This file only builds the
// view: a monospace GtkDrawingArea renders a 16-byte hex dump with an ASCII
// column, editable in place through key presses, plus the region / address /
// view-mode toolbar and the breakpoint bar.
//
// Only visible rows are read from the emulated bus on each refresh, so even
// the 4 GB "Full" dump opens instantly.

#include <gtk/gtk.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../dTool.h"
#include "../../../shared/memory_viewer_core.h"
#include "../../shared/memory_viewer_layout.h"

#define SHORTNAME "memoryviewer"
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

	GtkWidget *regionCombo;
	GtkWidget *addressEntry;
	GtkWidget *historyCombo;
	GtkWidget *viewCombo;
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
	gchar history[kMaxHistory][16];
	guint historyCount;
} MemoryViewerState;

static MemoryViewerState sView;
static int sToolID = -1;

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
	GtkStyleContext *context = gtk_widget_get_style_context(widget);
	GdkRGBA fg;
	gtk_style_context_get_color(context, gtk_widget_get_state_flags(widget), &fg);
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
on_size_allocate(GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
	(void)widget;
	update_geometry((MemoryViewerState *)data, allocation->height);
}

static void
on_font_changed(GSettings *settings, gchar *key, gpointer data)
{
	(void)settings;
	(void)key;
	MemoryViewerState *st = (MemoryViewerState *)data;
	memory_viewer_set_min_width(st->drawing, st->fontSettings);
	update_geometry(st, gtk_widget_get_allocated_height(st->drawing));
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

static gboolean
on_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	MemoryViewerState *st = (MemoryViewerState *)data;

	PangoLayout *layout = memory_viewer_layout_new(widget, st->fontSettings);
	double char_w = 0.0;
	double line_h = 0.0;
	double header_h = 0.0;
	memory_viewer_metrics(layout, &char_w, &line_h, &header_h);
	const int width = gtk_widget_get_allocated_width(widget);
	const int rows = st->visibleRows;

	const int step = bytes_per_unit(st->viewMode);
	const int digits = digits_per_unit(st->viewMode);
	const int units = kBytesPerRow / step;

	const double data_x = kOuterMargin + 8.0 * char_w + 2.0 * char_w;
	const double unit_w = ((double)digits + 1.0) * char_w;
	const double ascii_x = data_x + (double)units * unit_w + 2.0 * char_w;

	/* Column headers. */
	set_text_color(cr, widget, 0.65);
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

		set_text_color(cr, widget, 0.65);
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
					set_text_color(cr, widget, 1.0);
					memory_viewer_draw_cells(cr, layout, typed, x, y, char_w, line_h);
				}
				else
				{
					g_snprintf(text, sizeof(text), "%0*X", digits, (unsigned)value);
					set_text_color(cr, widget, 1.0);
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
				set_text_color(cr, widget, 1.0);
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
			set_text_color(cr, widget, 1.0);
			memory_viewer_draw_cells(cr, layout, asciiText, x, y, char_w, line_h);
		}
	}

	g_object_unref(layout);
	return FALSE;
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
on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	(void)widget;
	MemoryViewerState *st = (MemoryViewerState *)data;
	const gboolean ctrl = (event->state & GDK_CONTROL_MASK) != 0;
	const int step = bytes_per_unit(st->viewMode);
	const MemoryViewerRegionInfo *info = MemoryViewerRegionGet(st->region);

	switch (event->keyval)
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

	const gunichar unicode = gdk_keyval_to_unicode(event->keyval);
	const int digit = hex_digit_value(unicode);
	if (digit >= 0)
	{
		input_hex_digit(st, digit);
		return TRUE;
	}
	return FALSE;
}

static gboolean
on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	MemoryViewerState *st = (MemoryViewerState *)data;

	if (event->button != 1)
	{
		return FALSE;
	}

	gtk_widget_grab_focus(widget);

	double char_w, line_h, header_h;
	PangoLayout *layout = memory_viewer_layout_new(widget, st->fontSettings);
	memory_viewer_metrics(layout, &char_w, &line_h, &header_h);
	g_object_unref(layout);

	if (event->y < header_h)
	{
		return TRUE;
	}
	const int row = (int)floor((event->y - header_h) / line_h);
	if (row < 0 || row >= st->visibleRows)
	{
		return TRUE;
	}

	const int step = bytes_per_unit(st->viewMode);
	const int digits = digits_per_unit(st->viewMode);
	const int units = kBytesPerRow / step;
	const double data_x = kOuterMargin + 8.0 * char_w + 2.0 * char_w;
	const double unit_w = ((double)digits + 1.0) * char_w;
	const double ascii_x = data_x + (double)units * unit_w + 2.0 * char_w;

	if (event->x >= data_x && event->x < data_x + (double)units * unit_w)
	{
		const int unit = (int)floor((event->x - data_x) / unit_w);
		const guint32 address = st->address + (guint32)(row * kBytesPerRow + unit * step);
		set_selection_address(st, address, FALSE);
	}
	else if (event->x >= ascii_x && event->x < ascii_x + (double)kBytesPerRow * char_w)
	{
		const int byte = (int)floor((event->x - ascii_x) / char_w);
		const guint32 address = st->address + (guint32)(row * kBytesPerRow + byte);
		set_selection_address(st, address, FALSE);
	}
	return TRUE;
}

static gboolean
on_scroll(GtkWidget *widget, GdkEventScroll *event, gpointer data)
{
	(void)widget;
	MemoryViewerState *st = (MemoryViewerState *)data;

	switch (event->direction)
	{
		case GDK_SCROLL_UP:
			scroll_by(st, -(gint64)kBytesPerRow);
			return TRUE;
		case GDK_SCROLL_DOWN:
			scroll_by(st, (gint64)kBytesPerRow);
			return TRUE;
		case GDK_SCROLL_SMOOTH:
			if (event->delta_y == 0.0)
			{
				return FALSE;
			}
			st->scrollRemainder += event->delta_y;
			{
				const gint64 rows = (gint64)st->scrollRemainder;
				st->scrollRemainder -= rows;
				if (rows != 0)
					scroll_by(st, rows * kBytesPerRow);
			}
			return TRUE;
		default:
			return FALSE;
	}
}

/* ------------------------------------------------------------ address / toolbar */

static void
rebuild_history_combo(MemoryViewerState *st)
{
	st->updatingHistory = TRUE;
	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(st->historyCombo));
	for (guint i = 0; i < st->historyCount; ++i)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->historyCombo), st->history[i]);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(st->historyCombo),
	                         (st->historyCount > 0) ? 0 : -1);
	st->updatingHistory = FALSE;
}

static void
add_address_to_history(MemoryViewerState *st, guint32 address)
{
	gchar entry[16];
	g_snprintf(entry, sizeof(entry), "%08X", (unsigned)address);

	gtk_entry_set_text(GTK_ENTRY(st->addressEntry), entry);

	gchar updated[kMaxHistory][16];
	guint used = 0;
	g_strlcpy(updated[used++], entry, sizeof(updated[0]));
	for (guint i = 0; i < st->historyCount && used < kMaxHistory; ++i)
	{
		if (g_strcmp0(st->history[i], entry) != 0)
		{
			g_strlcpy(updated[used++], st->history[i], sizeof(updated[0]));
		}
	}

	memcpy(st->history, updated, sizeof(updated));
	st->historyCount = used;
	rebuild_history_combo(st);
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
	const char *text = gtk_entry_get_text(GTK_ENTRY(st->addressEntry));
	guint32 address = 0;
	if (parse_hex_address(text, &address))
	{
		jump_to_address(st, address, TRUE);
	}
}

static void
on_history_changed(GtkComboBox *combo, gpointer data)
{
	MemoryViewerState *st = (MemoryViewerState *)data;
	if (st->updatingHistory)
	{
		return;
	}
	const gint selected = gtk_combo_box_get_active(combo);
	if (selected < 0 || (guint)selected >= st->historyCount)
	{
		return;
	}
	guint32 address = 0;
	if (parse_hex_address(st->history[selected], &address))
	{
		jump_to_address(st, address, FALSE);
	}
}

static void
on_region_changed(GtkComboBox *combo, gpointer data)
{
	MemoryViewerState *st = (MemoryViewerState *)data;
	const gint index = gtk_combo_box_get_active(combo);
	if (index < 0 || index >= MEMVIEW_REGION_COUNT)
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
on_view_mode_changed(GtkComboBox *combo, gpointer data)
{
	MemoryViewerState *st = (MemoryViewerState *)data;
	const gint index = gtk_combo_box_get_active(combo);
	if (index < 0 || index > VIEW_WORD)
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
on_big_endian_toggled(GtkToggleButton *button, gpointer data)
{
	MemoryViewerState *st = (MemoryViewerState *)data;
	st->bigEndian = gtk_toggle_button_get_active(button);
	reload_buffer(st);
	gtk_widget_queue_draw(st->drawing);
}

static void
on_full_chars_toggled(GtkToggleButton *button, gpointer data)
{
	MemoryViewerState *st = (MemoryViewerState *)data;
	st->fullCharacters = gtk_toggle_button_get_active(button);
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
	const char *text = gtk_entry_get_text(GTK_ENTRY(st->bpEntry));
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
	gtk_entry_set_text(GTK_ENTRY(st->bpEntry), text);
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
	gtk_entry_set_text(GTK_ENTRY(st->bpEntry), text);
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

static void
show_message(const char *message)
{
	GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(sView.window),
	                                           (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
	                                           GTK_MESSAGE_INFO,
	                                           GTK_BUTTONS_OK,
	                                           "%s", message);
	gtk_window_set_title(GTK_WINDOW(dialog), TOOL_NAME);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

static void
write_export(MemoryViewerState *st, ExportKind kind, const char *path)
{
	const MemoryViewerRegionType region = st->region;
	const guint32 address = st->address;
	const guint32 windowSize = st->windowSize;
	const ViewMode mode = st->viewMode;
	const guint32 block = 0x1000;

	/* The full dump is streamed in 4 KB blocks, so make sure the buffer can
	 * hold a whole block even when the visible window is shorter. */
	const guint32 bufferSize = (kind == EXPORT_FULL) ? block : windowSize;
	guint8 *buffer = (guint8 *)g_malloc((bufferSize > 0) ? bufferSize : 1);
	if (buffer == NULL)
	{
		return;
	}

	if (kind == EXPORT_FULL)
	{
		const guint32 total = MemoryViewerRegionDumpSize(region);
		const guint32 base = (region == MEMVIEW_ROM) ? 0
		                                            : MemoryViewerRegionGet(region)->hardwareAddress;
		FILE *fp = fopen(path, "wb");
		if (fp != NULL && total > 0)
		{
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

static void
present_export(MemoryViewerState *st, ExportKind kind)
{
	GtkFileChooserNative *chooser = gtk_file_chooser_native_new(
	    "Export memory dump", GTK_WINDOW(st->window), GTK_FILE_CHOOSER_ACTION_SAVE,
	    "_Save", "_Cancel");
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(chooser), TRUE);

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
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser), name);

	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, (kind == EXPORT_TEXT) ? "Text dump (*.txt)" : "Binary dump (*.bin)");
	gtk_file_filter_add_pattern(filter, (kind == EXPORT_TEXT) ? "*.txt" : "*.bin");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);

	if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT)
	{
		GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(chooser));
		gchar *path = (file != NULL) ? g_file_get_path(file) : NULL;
		if (path != NULL)
		{
			write_export(st, kind, path);
			g_free(path);
		}
		if (file != NULL)
		{
			g_object_unref(file);
		}
	}

	g_object_unref(chooser);
}

static void
on_export_text(GtkMenuItem *item, gpointer data)
{
	(void)item;
	present_export((MemoryViewerState *)data, EXPORT_TEXT);
}

static void
on_export_raw(GtkMenuItem *item, gpointer data)
{
	(void)item;
	present_export((MemoryViewerState *)data, EXPORT_RAW);
}

static void
on_export_all(GtkMenuItem *item, gpointer data)
{
	(void)item;
	MemoryViewerState *st = (MemoryViewerState *)data;
	const guint32 total = MemoryViewerRegionDumpSize(st->region);
	if (total == 0)
	{
		show_message("The \"Full\" region spans the entire address space and \"CartROM\" is "
		             "empty without a loaded ROM. Select ARM9, ARM7 or Firmware instead.");
		return;
	}
	present_export(st, EXPORT_FULL);
}

/* --------------------------------------------------------------- tool plumbing */

static void
on_window_destroy(GtkWidget *widget, gpointer data)
{
	(void)widget;
	(void)data;

	g_clear_object(&sView.fontSettings);
	memset(&sView, 0, sizeof(sView));

	if (sToolID >= 0)
	{
		dTool_CloseCallback(sToolID);
	}
	sToolID = -1;
}

static GtkWidget *
add_toolbar_button(GtkWidget *parent, const char *label, GCallback callback, gpointer data)
{
	GtkWidget *button = gtk_button_new_with_mnemonic(label);
	g_signal_connect(button, "clicked", callback, data);
	gtk_box_pack_start(GTK_BOX(parent), button, FALSE, FALSE, 0);
	return button;
}

static void
build_export_menu(MemoryViewerState *st)
{
	GtkWidget *menu = gtk_menu_new();

	GtkWidget *item = gtk_menu_item_new_with_label("Text Dump...");
	g_signal_connect(item, "activate", G_CALLBACK(on_export_text), st);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	item = gtk_menu_item_new_with_label("Raw Dump...");
	g_signal_connect(item, "activate", G_CALLBACK(on_export_raw), st);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	item = gtk_menu_item_new_with_label("Dump All...");
	g_signal_connect(item, "activate", G_CALLBACK(on_export_all), st);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	gtk_widget_show_all(menu);
	gtk_menu_button_set_popup(GTK_MENU_BUTTON(st->exportButton), menu);
}

static void
open_window(int ID)
{
	if (sView.window != NULL)
	{
		gtk_window_present(GTK_WINDOW(sView.window));
		return;
	}

	sToolID = ID;
	memset(&sView, 0, sizeof(sView));

	MemoryViewerState *st = &sView;
	st->region = MEMVIEW_ARM9;
	st->viewMode = VIEW_BYTE;
	st->visibleRows = kDefaultRows;
	st->windowSize = kDefaultRows * kBytesPerRow;
	st->address = MemoryViewerRegionGet(st->region)->hardwareAddress;
	st->lastRefresh = 0;
	st->fontSettings = memory_viewer_font_settings();

	MemoryViewerResetRegions();

	st->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(st->window), TOOL_NAME " : ARM9 memory");
	gtk_window_set_default_size(GTK_WINDOW(st->window), 680, 460);
	gtk_window_set_resizable(GTK_WINDOW(st->window), TRUE);
	gtk_window_set_icon_name(GTK_WINDOW(st->window), "org.desmume.DeSmuME");
	gtk_container_set_border_width(GTK_CONTAINER(st->window), 6);
	g_signal_connect(st->window, "destroy", G_CALLBACK(on_window_destroy), NULL);

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_container_add(GTK_CONTAINER(st->window), root);

	/* --- toolbar --- */
	GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(root), toolbar, FALSE, FALSE, 0);

	st->regionCombo = gtk_combo_box_text_new();
	for (int i = 0; i < MEMVIEW_REGION_COUNT; ++i)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->regionCombo),
		                               MemoryViewerRegionGetName((MemoryViewerRegionType)i));
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(st->regionCombo), MEMVIEW_ARM9);
	gtk_widget_set_tooltip_text(st->regionCombo, "Memory region to inspect");
	g_signal_connect(st->regionCombo, "changed", G_CALLBACK(on_region_changed), st);
	gtk_box_pack_start(GTK_BOX(toolbar), st->regionCombo, FALSE, FALSE, 0);

	GtkWidget *addressLabel = gtk_label_new_with_mnemonic("_Address:");
	gtk_box_pack_start(GTK_BOX(toolbar), addressLabel, FALSE, FALSE, 0);
	st->addressEntry = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(st->addressEntry), 10);
	gtk_widget_set_tooltip_text(st->addressEntry, "Hex address to jump to (e.g. 02000000 or 0x02000000h)");
	gtk_label_set_mnemonic_widget(GTK_LABEL(addressLabel), st->addressEntry);
	g_signal_connect(st->addressEntry, "activate", G_CALLBACK(on_go_clicked), st);
	gtk_box_pack_start(GTK_BOX(toolbar), st->addressEntry, FALSE, FALSE, 0);
	add_toolbar_button(toolbar, "_Go", G_CALLBACK(on_go_clicked), st);

	st->historyCombo = gtk_combo_box_text_new();
	gtk_widget_set_tooltip_text(st->historyCombo, "Recently visited addresses");
	g_signal_connect(st->historyCombo, "changed", G_CALLBACK(on_history_changed), st);
	gtk_box_pack_start(GTK_BOX(toolbar), st->historyCombo, FALSE, FALSE, 0);

	st->viewCombo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->viewCombo), "Bytes");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->viewCombo), "Halfwords");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->viewCombo), "Words");
	gtk_combo_box_set_active(GTK_COMBO_BOX(st->viewCombo), VIEW_BYTE);
	gtk_widget_set_tooltip_text(st->viewCombo, "Cell width");
	g_signal_connect(st->viewCombo, "changed", G_CALLBACK(on_view_mode_changed), st);
	gtk_box_pack_start(GTK_BOX(toolbar), st->viewCombo, FALSE, FALSE, 0);

	st->selectionLabel = gtk_label_new("--------h");
	gtk_box_pack_start(GTK_BOX(toolbar), st->selectionLabel, FALSE, FALSE, 0);

	st->exportButton = gtk_menu_button_new();
	gtk_button_set_label(GTK_BUTTON(st->exportButton), "E_xport");
	gtk_button_set_use_underline(GTK_BUTTON(st->exportButton), TRUE);
	gtk_widget_set_tooltip_text(st->exportButton,
	                            "Dump the visible window as text/raw bytes, or the whole region");
	gtk_box_pack_start(GTK_BOX(toolbar), st->exportButton, FALSE, FALSE, 0);
	build_export_menu(st);

	/* --- hex dump --- */
	st->drawing = gtk_drawing_area_new();
	gtk_widget_set_can_focus(st->drawing, TRUE);
	gtk_widget_set_hexpand(st->drawing, TRUE);
	gtk_widget_set_vexpand(st->drawing, TRUE);
	memory_viewer_set_min_width(st->drawing, st->fontSettings);
	gtk_widget_add_events(st->drawing, GDK_BUTTON_PRESS_MASK | GDK_SCROLL_MASK |
	                                      GDK_SMOOTH_SCROLL_MASK | GDK_KEY_PRESS_MASK);
	g_signal_connect(st->drawing, "draw", G_CALLBACK(on_draw), st);
	g_signal_connect(st->drawing, "key-press-event", G_CALLBACK(on_key_press), st);
	g_signal_connect(st->drawing, "button-press-event", G_CALLBACK(on_button_press), st);
	g_signal_connect(st->drawing, "scroll-event", G_CALLBACK(on_scroll), st);
	GtkWidget *dumpBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(root), dumpBox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(dumpBox), st->drawing, TRUE, TRUE, 0);
	st->adjustment = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 1, 1, 1, 1));
	GtkWidget *scrollbar = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, st->adjustment);
	gtk_box_pack_start(GTK_BOX(dumpBox), scrollbar, FALSE, FALSE, 0);
	g_signal_connect(st->adjustment, "value-changed", G_CALLBACK(on_adjustment_changed), st);
	g_signal_connect(st->drawing, "size-allocate", G_CALLBACK(on_size_allocate), st);
	if (st->fontSettings != NULL)
		g_signal_connect(st->fontSettings, "changed::monospace-font-name", G_CALLBACK(on_font_changed), st);

	/* --- breakpoint bar --- */
	GtkWidget *bpBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(root), bpBar, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(bpBar), gtk_label_new("BP:"), FALSE, FALSE, 0);
	st->bpEntry = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(st->bpEntry), 10);
	gtk_entry_set_placeholder_text(GTK_ENTRY(st->bpEntry), "selected");
	gtk_widget_set_tooltip_text(st->bpEntry,
	                            "Breakpoint address; empty uses the selected byte");
	gtk_box_pack_start(GTK_BOX(bpBar), st->bpEntry, FALSE, FALSE, 0);
	add_toolbar_button(bpBar, "R+", G_CALLBACK(on_add_read_bp), st);
	add_toolbar_button(bpBar, "W+", G_CALLBACK(on_add_write_bp), st);
	add_toolbar_button(bpBar, "-", G_CALLBACK(on_remove_bp), st);
	add_toolbar_button(bpBar, "Clear", G_CALLBACK(on_clear_bp), st);
	st->bpSummary = gtk_label_new("no breakpoints");
	gtk_box_pack_start(GTK_BOX(bpBar), st->bpSummary, FALSE, FALSE, 0);

	st->fullCheck = gtk_check_button_new_with_label("Full");
	gtk_widget_set_tooltip_text(st->fullCheck, "Show printable characters up to 0xFF");
	g_signal_connect(st->fullCheck, "toggled", G_CALLBACK(on_full_chars_toggled), st);
	gtk_box_pack_start(GTK_BOX(bpBar), st->fullCheck, FALSE, FALSE, 0);
	st->beCheck = gtk_check_button_new_with_label("BE");
	gtk_widget_set_tooltip_text(st->beCheck, "Interpret halfwords and words as big-endian");
	g_signal_connect(st->beCheck, "toggled", G_CALLBACK(on_big_endian_toggled), st);
	gtk_box_pack_start(GTK_BOX(bpBar), st->beCheck, FALSE, FALSE, 0);

	update_breakpoint_summary(st);
	reload_buffer(st);
	gtk_widget_show_all(st->window);
	gtk_widget_grab_focus(st->drawing);
	gtk_window_present(GTK_WINDOW(st->window));
}

static void
update(void)
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
close_tool(void)
{
	if (sView.window != NULL)
	{
		gtk_widget_destroy(sView.window);
	}
}

dTool_t dTool_memoryViewer =
{
	SHORTNAME,
	TOOL_NAME,
	&open_window,
	&update,
	&close_tool
};
