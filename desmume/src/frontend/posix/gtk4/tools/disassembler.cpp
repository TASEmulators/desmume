/*
	disassembler.cpp - ARM disassembler tool for the DeSmuME GTK4 frontend

	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2026 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	GTK4 host for the shared disassembly core.  Decoding, address
	navigation, the ARM/Thumb/auto rules, register snapshots and the
	breakpoint list live in shared/disassembly_core.cpp, which the GTK3,
	Windows and Cocoa frontends use as well; this file only builds the
	window, forwards clicks to the core and draws.

	The instruction list is a GtkDrawingArea rather than a text or list
	widget: GTK4 dropped GtkTreeView, the core owns the scroll position, and
	drawing only the rows that fit keeps a 60 Hz auto-update cheap.  This is
	the same approach tools/memoryviewer.cpp takes.

	Run control goes through desmume_emu_set_running().  The core stops
	itself on a breakpoint or after a step (it clears the core's execute
	flag), so Update() mirrors that back into the frontend's running state;
	without it the frame tick would keep calling into a halted CPU.
*/

#include "disassembler.h"

#include "../config.h"
#include "../emulation.h"
#include "../tools.h"

#include "../../shared/gtk_monospace_font.h"
#include "../../shared/memory_viewer_layout.h"
#include "../../../shared/disassembly_core.h"

#include "NDSSystem.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#define TOOL_NAME "Disassembler"

static const int kDefaultWidth = 900;
static const int kDefaultHeight = 720;
static const int kSidebarWidth = 276;
static const int kBreakpointListHeight = 132;
static const int kMessageTimeoutMs = 3000;
static const int kMaxAutoUpdateFrames = 99;
static const int kWheelLines = 3;
static const int kPageLines = 24;

/* Row backgrounds for the PC and breakpoint lines.  Both are light and the
 * text on them is forced to black, so they stay readable in dark themes. */
static const double kCurrentRowColor[3] = { 1.00, 0.878, 0.40 };
static const double kBreakpointRowColor[3] = { 1.00, 0.690, 0.690 };

static const char *const kRegisterNames[16] =
{
	"R0",  "R1",  "R2",  "R3",  "R4",  "R5",  "R6",  "R7",
	"R8",  "R9",  "R10", "R11", "R12", "R13", "R14", "R15"
};

static GtkWidget *sWindow = NULL;
static GtkWidget *sDrawing = NULL;
static GtkWidget *sStatusLabel = NULL;
static GtkWidget *sMessageLabel = NULL;

static GtkWidget *sCpuDrop = NULL;
static GtkWidget *sModeDrop = NULL;
static GtkWidget *sGoEntry = NULL;
static GtkWidget *sAutoUpdateCheck = NULL;
static GtkWidget *sAutoUpdateSpin = NULL;

static GtkWidget *sRegisterEntries[16];
static GtkWidget *sCpsrValueLabel = NULL;
static GtkWidget *sCpsrFlagsLabel = NULL;
static GtkWidget *sModeValueLabel = NULL;
static GtkWidget *sSpsrValueLabel = NULL;

static GtkWidget *sBreakpointList = NULL;
static GtkWidget *sBreakpointEntry = NULL;
static GtkWidget *sRemoveButton = NULL;
static GtkWidget *sMoveUpButton = NULL;
static GtkWidget *sMoveDownButton = NULL;

static GSettings *sFontSettings = NULL;
static GtkCssProvider *sFontProvider = NULL;

/* Decoded instructions, rebuilt by refresh_view() and drawn by on_draw(). */
static std::vector<DisassemblyCore::Line> sLines;
static int sVisibleRows = 1;

static bool sAutoUpdate = true;
static int sAutoUpdateFrames = 1;
static guint sFramesSinceRefresh = 0;

/* Set when the CPU is armed and expected to stop again (step, run to return);
 * consumed by Update() once the CPU is idle. */
static bool sRefreshWhenStopped = FALSE;

/* Guards the control callbacks while the widgets are populated from the core. */
static bool sUpdating = FALSE;

/* Set while the breakpoint list is rebuilt: refilling it clears the selection,
 * and the selection handler writes the address into the entry field. */
static bool sRefreshingBreakpoints = FALSE;

static double sScrollRemainder = 0.0;
static guint sMessageSource = 0;

/* ---------------------------------------------------------------- helpers */

static DisassemblyCore &Core(void)
{
	return DisassemblyCore::Get();
}

static const char *CpuName(void)
{
	return (Core().GetCpu() == DisassemblyCore::CPU_ARM7) ? "ARM7" : "ARM9";
}

static std::string FormatAddress(guint32 address)
{
	char text[24];
	g_snprintf(text, sizeof(text), "%04X:%04X",
	           (unsigned)(address >> 16), (unsigned)(address & 0xFFFF));
	return std::string(text);
}

static gboolean clear_message(gpointer data)
{
	(void)data;
	sMessageSource = 0;
	if (sMessageLabel != NULL)
	{
		gtk_label_set_text(GTK_LABEL(sMessageLabel), "");
	}
	return G_SOURCE_REMOVE;
}

static void ShowMessage(const char *message)
{
	if ((message == NULL) || (sMessageLabel == NULL))
	{
		return;
	}

	gtk_label_set_text(GTK_LABEL(sMessageLabel), message);

	if (sMessageSource != 0)
	{
		g_source_remove(sMessageSource);
	}
	sMessageSource = g_timeout_add(kMessageTimeoutMs, clear_message, NULL);
}

static bool ParseEntry(GtkWidget *entry, guint32 *outValue)
{
	if (entry == NULL)
	{
		return false;
	}

	guint32 value = 0;
	if (!DisassemblyCore::ParseHex32(gtk_editable_get_text(GTK_EDITABLE(entry)), value))
	{
		return false;
	}

	*outValue = value;
	return true;
}

static void AddHexClass(GtkWidget *widget)
{
	gtk_widget_add_css_class(widget, "hex");
}

static GtkWidget *add_button(GtkWidget *box, const char *label, GCallback callback)
{
	GtkWidget *button = gtk_button_new_with_label(label);
	g_signal_connect(button, "clicked", callback, NULL);
	gtk_box_append(GTK_BOX(box), button);
	return button;
}

/* ------------------------------------------------------------ preferences */

static void save_geometry(void)
{
	if (sWindow == NULL)
	{
		return;
	}

	desmume::config::Config *config = desmume_host_config();
	if (config == NULL)
	{
		return;
	}

	config->disasm_win_w.set(gtk_widget_get_width(sWindow));
	config->disasm_win_h.set(gtk_widget_get_height(sWindow));
}

static void persist_preferences(void)
{
	desmume::config::Config *config = desmume_host_config();
	if (config == NULL)
	{
		return;
	}

	config->disasm_cpu.set((int)Core().GetCpu());
	config->disasm_mode.set((int)Core().GetViewMode());
	config->disasm_autoupdate.set(sAutoUpdate);
	config->disasm_autoupdate_frames.set(sAutoUpdateFrames);
	config->save();
}

static void load_preferences(void)
{
	desmume::config::Config *config = desmume_host_config();

	int cpu = (config != NULL) ? config->disasm_cpu.get() : 0;
	if ((cpu < 0) || (cpu >= DisassemblyCore::CPU_COUNT))
	{
		cpu = DisassemblyCore::CPU_ARM9;
	}
	Core().SetCpu((DisassemblyCore::Cpu)cpu);

	int mode = (config != NULL) ? config->disasm_mode.get() : 0;
	if ((mode < DisassemblyCore::ViewMode_Auto) || (mode > DisassemblyCore::ViewMode_Thumb))
	{
		mode = DisassemblyCore::ViewMode_Auto;
	}
	Core().SetViewMode((DisassemblyCore::ViewMode)mode);

	sAutoUpdate = (config != NULL) ? config->disasm_autoupdate.get() : true;

	int frames = (config != NULL) ? config->disasm_autoupdate_frames.get() : 1;
	if ((frames < 1) || (frames > kMaxAutoUpdateFrames))
	{
		frames = 1;
	}
	sAutoUpdateFrames = frames;
}

/* ------------------------------------------------------------------ fonts */

static void apply_monospace_font(void)
{
	if ((sFontProvider == NULL) || (sDrawing == NULL))
	{
		return;
	}

	PangoFontDescription *font = gtk_monospace_font_description(sDrawing, sFontSettings);
	const char *family = pango_font_description_get_family(font);
	if (family != NULL)
	{
		/* Applies to every widget carrying the class: register and
		 * breakpoint fields as well as the read-only hex labels. */
		char *css = g_strdup_printf(".hex { font-family: \"%s\"; }", family);
		/* GTK 4.12 replaced the GError form with the string form. */
#if GTK_CHECK_VERSION(4, 12, 0)
		gtk_css_provider_load_from_string(sFontProvider, css);
#else
		gtk_css_provider_load_from_data(sFontProvider, css, -1, NULL);
#endif
		g_free(css);
	}
	pango_font_description_free(font);

	gtk_widget_queue_draw(sDrawing);
}

static void on_font_changed(GSettings *settings, gchar *key, gpointer data)
{
	(void)settings;
	(void)key;
	(void)data;
	apply_monospace_font();
}

/* ------------------------------------------------------------------ draw */

static void layout_metrics(double *charWidth, double *lineHeight)
{
	*charWidth = 0.0;
	*lineHeight = 1.0;
	if (sDrawing == NULL)
	{
		return;
	}

	PangoLayout *layout = memory_viewer_layout_new(sDrawing, sFontSettings);
	double headerHeight = 0.0;
	memory_viewer_metrics(layout, charWidth, lineHeight, &headerHeight);
	g_object_unref(layout);
}

static void refresh_view(void);

static void on_resize(GtkDrawingArea *area, int width, int height, gpointer data)
{
	(void)area;
	(void)width;
	(void)data;

	double charWidth = 0.0;
	double lineHeight = 1.0;
	layout_metrics(&charWidth, &lineHeight);

	const int rows = MAX(1, (int)floor((double)height / lineHeight));
	if (rows == sVisibleRows)
	{
		return;
	}

	sVisibleRows = rows;

	/* Only the decoded window is sized from the row count; growing it needs
	 * more lines, shrinking does not.  GTK4 defers layout, so rebuilding here
	 * cannot recurse into another resize. */
	if (sLines.size() < (size_t)MAX(sVisibleRows + 2, 4))
	{
		refresh_view();
	}
	else if (sDrawing != NULL)
	{
		gtk_widget_queue_draw(sDrawing);
	}
}

static void on_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data)
{
	(void)height;

	if (sWindow == NULL)
	{
		return;
	}

	PangoLayout *layout = memory_viewer_layout_new(GTK_WIDGET(area), sFontSettings);
	double charWidth = 0.0;
	double lineHeight = 1.0;
	double headerHeight = 0.0;
	memory_viewer_metrics(layout, &charWidth, &lineHeight, &headerHeight);
	(void)charWidth;
	(void)headerHeight;

	GdkRGBA foreground;
	gtk_widget_get_color(GTK_WIDGET(area), &foreground);

	const int rows = MIN((int)sLines.size(), sVisibleRows);

	for (int row = 0; row < rows; ++row)
	{
		const DisassemblyCore::Line &line = sLines[(size_t)row];
		const double y = (double)row * lineHeight;

		if (line.Current || line.Breakpoint)
		{
			const double *color = line.Current ? kCurrentRowColor : kBreakpointRowColor;
			cairo_set_source_rgb(cr, color[0], color[1], color[2]);
			cairo_rectangle(cr, 0.0, y, (double)width, lineHeight);
			cairo_fill(cr);
			cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
		}
		else
		{
			cairo_set_source_rgba(cr, foreground.red, foreground.green, foreground.blue, 1.0);
		}

		pango_layout_set_text(layout, line.Text.c_str(), -1);
		cairo_move_to(cr, 4.0, y);
		pango_cairo_show_layout(cr, layout);
	}

	if (sLines.empty())
	{
		cairo_set_source_rgba(cr, foreground.red, foreground.green, foreground.blue, 0.6);
		pango_layout_set_text(layout, "No instructions to show.", -1);
		cairo_move_to(cr, 4.0, 0.0);
		pango_cairo_show_layout(cr, layout);
	}

	g_object_unref(layout);
}

/* --------------------------------------------------------------- refresh */

static void refresh_registers(void);
static void refresh_status(void);

/* Assigning text invalidates the widget's layout even when the string is
 * identical, and the auto-update path runs this up to 60 times a second.
 * Comparing first keeps an idle disassembler window actually idle. */
static void set_entry_text(GtkWidget *entry, const char *text)
{
	if (entry == NULL)
	{
		return;
	}
	if (strcmp(gtk_editable_get_text(GTK_EDITABLE(entry)), text) == 0)
	{
		return;
	}
	gtk_editable_set_text(GTK_EDITABLE(entry), text);
}

static void set_label_text(GtkWidget *label, const char *text)
{
	if (label == NULL)
	{
		return;
	}
	if (strcmp(gtk_label_get_text(GTK_LABEL(label)), text) == 0)
	{
		return;
	}
	gtk_label_set_text(GTK_LABEL(label), text);
}

/* gtk_label_get_label() returns the string the markup was parsed from, so the
 * comparison is against the markup and not the visible text. */
static void set_label_markup(GtkWidget *label, const char *markup)
{
	if (label == NULL)
	{
		return;
	}
	const char *current = gtk_label_get_label(GTK_LABEL(label));
	if ((current != NULL) && (strcmp(current, markup) == 0))
	{
		return;
	}
	gtk_label_set_markup(GTK_LABEL(label), markup);
}

static void refresh_view(void)
{
	sLines.clear();
	sLines.reserve((size_t)MAX(sVisibleRows + 2, 4));
	Core().BuildLines((size_t)MAX(sVisibleRows + 2, 4), sLines);

	if (sDrawing != NULL)
	{
		gtk_widget_queue_draw(sDrawing);
	}

	refresh_registers();
	refresh_status();
}

static void refresh_registers(void)
{
	if ((sCpsrValueLabel == NULL) || (sSpsrValueLabel == NULL) ||
	    (sModeValueLabel == NULL) || (sCpsrFlagsLabel == NULL))
	{
		return;
	}

	DisassemblyCore::Registers regs;
	if (!Core().ReadRegisters(regs))
	{
		return;
	}

	char text[32];

	for (int i = 0; i < 16; ++i)
	{
		GtkWidget *field = sRegisterEntries[i];
		if ((field != NULL) && gtk_widget_has_focus(field))
		{
			/* Never overwrite a value the user is typing. */
			continue;
		}

		g_snprintf(text, sizeof(text), "%08X", (unsigned)regs.R[i]);
		set_entry_text(field, text);
	}

	g_snprintf(text, sizeof(text), "%08X", (unsigned)regs.CPSR);
	set_label_text(sCpsrValueLabel, text);

	g_snprintf(text, sizeof(text), "%08X", (unsigned)regs.SPSR);
	set_label_text(sSpsrValueLabel, text);

	g_snprintf(text, sizeof(text), "%02X %s", (unsigned)(regs.Mode & 0x1F),
	           DisassemblyCore::CpuModeName(regs.Mode));
	set_label_text(sModeValueLabel, text);

	/* Set flags red and bold, cleared ones grey. */
	const bool flags[8] = { regs.N, regs.Z, regs.C, regs.V, regs.Q, regs.I, regs.F, regs.T };
	static const char kFlagNames[8] = { 'N', 'Z', 'C', 'V', 'Q', 'I', 'F', 'T' };

	GString *markup = g_string_new(NULL);
	for (int i = 0; i < 8; ++i)
	{
		g_string_append_printf(markup,
		                       "<span foreground=\"%s\" weight=\"%s\">%c</span>%s",
		                       flags[i] ? "#D03030" : "#9A9A9A",
		                       flags[i] ? "bold" : "normal",
		                       kFlagNames[i],
		                       (i == 7) ? "" : " ");
	}
	set_label_markup(sCpsrFlagsLabel, markup->str);
	g_string_free(markup, TRUE);
}

static void refresh_status(void)
{
	if ((sWindow == NULL) || (sStatusLabel == NULL))
	{
		return;
	}

	char text[96];
	g_snprintf(text, sizeof(text), "%s  |  %s  |  %s", CpuName(),
	           Core().IsThumbMode() ? "Thumb" : "ARM",
	           FormatAddress(Core().AnchorAddress()).c_str());
	set_label_text(sStatusLabel, text);

	char title[96];
	g_snprintf(title, sizeof(title), "%s - %s (%s)", TOOL_NAME, CpuName(),
	           Core().IsThumbMode() ? "Thumb" : "ARM");
	if (strcmp(gtk_window_get_title(GTK_WINDOW(sWindow)), title) != 0)
	{
		gtk_window_set_title(GTK_WINDOW(sWindow), title);
	}
}

static int selected_breakpoint_index(void)
{
	if (sBreakpointList == NULL)
	{
		return -1;
	}

	GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(sBreakpointList));
	return (row != NULL) ? gtk_list_box_row_get_index(row) : -1;
}

static void update_breakpoint_buttons(void)
{
	const int index = selected_breakpoint_index();
	const int count = (int)Core().BreakpointCount();

	gtk_widget_set_sensitive(sRemoveButton, index >= 0);
	gtk_widget_set_sensitive(sMoveUpButton, index > 0);
	gtk_widget_set_sensitive(sMoveDownButton, (index >= 0) && (index < count - 1));
}

static void refresh_breakpoints(void)
{
	if (sBreakpointList == NULL)
	{
		return;
	}

	int selectedIndex = selected_breakpoint_index();

	sRefreshingBreakpoints = TRUE;

	GtkWidget *child;
	while ((child = gtk_widget_get_first_child(sBreakpointList)) != NULL)
	{
		gtk_list_box_remove(GTK_LIST_BOX(sBreakpointList), child);
	}

	const size_t count = Core().BreakpointCount();
	for (size_t i = 0; i < count; ++i)
	{
		GtkWidget *label = gtk_label_new(FormatAddress(Core().BreakpointAt(i)).c_str());
		gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
		AddHexClass(label);
		gtk_list_box_append(GTK_LIST_BOX(sBreakpointList), label);
	}

	if ((size_t)selectedIndex >= count)
	{
		selectedIndex = (int)count - 1;
	}
	if (selectedIndex >= 0)
	{
		GtkListBoxRow *row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(sBreakpointList),
		                                                  selectedIndex);
		if (row != NULL)
		{
			gtk_list_box_select_row(GTK_LIST_BOX(sBreakpointList), row);
		}
	}

	sRefreshingBreakpoints = FALSE;
}

static void refresh_all(void)
{
	refresh_view();
	refresh_breakpoints();
	update_breakpoint_buttons();
}

static void sync_controls_from_core(void)
{
	sUpdating = TRUE;

	gtk_drop_down_set_selected(GTK_DROP_DOWN(sCpuDrop),
	                           (Core().GetCpu() == DisassemblyCore::CPU_ARM7) ? 1 : 0);
	gtk_drop_down_set_selected(GTK_DROP_DOWN(sModeDrop), (guint)Core().GetViewMode());

	gtk_check_button_set_active(GTK_CHECK_BUTTON(sAutoUpdateCheck), sAutoUpdate);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(sAutoUpdateSpin), (gdouble)sAutoUpdateFrames);
	gtk_widget_set_sensitive(sAutoUpdateSpin, sAutoUpdate);

	sUpdating = FALSE;
}

/* ----------------------------------------------------------- run control */

static void resume_emulation(void)
{
	sFramesSinceRefresh = 0;
	desmume_emu_set_running(TRUE);
}

static bool ensure_rom_loaded(void)
{
	if (desmume_emu_has_rom())
	{
		return true;
	}

	ShowMessage("No ROM loaded.");
	return false;
}

/* -------------------------------------------------------------- handlers */

static void on_cpu_changed(GObject *object, GParamSpec *pspec, gpointer data)
{
	(void)pspec;
	(void)data;

	if (sUpdating)
	{
		return;
	}

	const guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
	Core().SetCpu((selected == 1) ? DisassemblyCore::CPU_ARM7 : DisassemblyCore::CPU_ARM9);
	Core().SetAnchorAddress(Core().AnchorAddress()); /* re-align for the new CPU */

	persist_preferences();
	refresh_all();
}

static void on_mode_changed(GObject *object, GParamSpec *pspec, gpointer data)
{
	(void)pspec;
	(void)data;

	if (sUpdating)
	{
		return;
	}

	switch (gtk_drop_down_get_selected(GTK_DROP_DOWN(object)))
	{
		case 1:  Core().SetViewMode(DisassemblyCore::ViewMode_ARM); break;
		case 2:  Core().SetViewMode(DisassemblyCore::ViewMode_Thumb); break;
		default: Core().SetViewMode(DisassemblyCore::ViewMode_Auto); break;
	}

	persist_preferences();
	refresh_all();
}

static void do_go(void)
{
	guint32 address = 0;
	if (!ParseEntry(sGoEntry, &address))
	{
		ShowMessage("Not a valid hex address.");
		return;
	}

	Core().SetAnchorAddress(address);
	refresh_all();
}

static void on_go_clicked(GtkWidget *widget, gpointer data)
{
	(void)widget;
	(void)data;
	do_go();
}

static void on_auto_update_toggled(GtkCheckButton *button, gpointer data)
{
	(void)data;

	if (sUpdating)
	{
		return;
	}

	sAutoUpdate = gtk_check_button_get_active(button) ? true : false;
	gtk_widget_set_sensitive(sAutoUpdateSpin, sAutoUpdate);

	if (sAutoUpdate)
	{
		Core().FollowPC();
		sFramesSinceRefresh = 0;
		refresh_view();
	}

	persist_preferences();
}

static void on_auto_update_frames_changed(GtkSpinButton *spin, gpointer data)
{
	(void)data;

	if (sUpdating)
	{
		return;
	}

	const int frames = (int)gtk_spin_button_get_value(spin);
	if ((frames >= 1) && (frames <= kMaxAutoUpdateFrames))
	{
		sAutoUpdateFrames = frames;
		sFramesSinceRefresh = 0;
		persist_preferences();
	}
}

static void on_refresh_clicked(GtkWidget *widget, gpointer data)
{
	(void)widget;
	(void)data;
	Core().FollowPC();
	refresh_all();
}

static void on_step_clicked(GtkWidget *widget, gpointer data)
{
	(void)widget;
	(void)data;

	if (!ensure_rom_loaded())
	{
		return;
	}

	Core().StepInstruction();
	sRefreshWhenStopped = true;
	resume_emulation();
}

static void on_step_over_clicked(GtkWidget *widget, gpointer data)
{
	(void)widget;
	(void)data;

	if (!ensure_rom_loaded())
	{
		return;
	}

	Core().StepOver();
	sRefreshWhenStopped = true;
	resume_emulation();
}

static void on_run_to_return_clicked(GtkWidget *widget, gpointer data)
{
	(void)widget;
	(void)data;

	if (!ensure_rom_loaded())
	{
		return;
	}

	Core().RunToReturn();
	sRefreshWhenStopped = true;
	resume_emulation();
}

static void on_continue_clicked(GtkWidget *widget, gpointer data)
{
	(void)widget;
	(void)data;

	if (!ensure_rom_loaded())
	{
		return;
	}

	Core().Continue();
	sRefreshWhenStopped = true;
	resume_emulation();
}

static void commit_register(GtkWidget *entry)
{
	const int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(entry), "register-index"));
	guint32 value = 0;

	if (ParseEntry(entry, &value))
	{
		Core().WriteRegister((unsigned int)index, value);
	}

	/* Rewrites the field from the core, which also reverts text that did not
	 * parse, exactly like the other frontends do. */
	refresh_registers();
}

static void on_register_activate(GtkWidget *entry, gpointer data)
{
	(void)data;
	commit_register(entry);
}

static void do_add_breakpoint(void)
{
	guint32 address = 0;
	if (!ParseEntry(sBreakpointEntry, &address))
	{
		ShowMessage("Not a valid hex address.");
		return;
	}

	/* Align to an instruction boundary so the address can actually be hit. */
	address &= ~(Core().InstructionSize() - 1);

	if (!Core().AddBreakpoint(address))
	{
		ShowMessage("Breakpoint already exists.");
		return;
	}

	refresh_all();
}

static void on_add_breakpoint_clicked(GtkWidget *widget, gpointer data)
{
	(void)widget;
	(void)data;
	do_add_breakpoint();
}

static void on_breakpoint_activate(GtkWidget *entry, gpointer data)
{
	(void)entry;
	(void)data;
	do_add_breakpoint();
}

static void on_remove_breakpoint_clicked(GtkWidget *widget, gpointer data)
{
	(void)widget;
	(void)data;

	const int index = selected_breakpoint_index();
	if (index < 0)
	{
		return;
	}

	Core().RemoveBreakpointAt((size_t)index);
	refresh_breakpoints();
	update_breakpoint_buttons();
}

/* Reorders one breakpoint by delta (-1 or +1).  The core only exposes append
 * and erase, so the list is rebuilt in the requested order; everything here
 * runs on the GTK main thread, so the emulation cannot observe the rebuild. */
static void move_breakpoint(int index, int delta)
{
	const int count = (int)Core().BreakpointCount();
	const int target = index + delta;
	if ((index < 0) || (index >= count) || (target < 0) || (target >= count))
	{
		return;
	}

	std::vector<guint32> order;
	order.reserve((size_t)count);
	for (int i = 0; i < count; ++i)
	{
		order.push_back(Core().BreakpointAt((size_t)i));
	}

	const guint32 moved = order[(size_t)index];
	order.erase(order.begin() + index);
	order.insert(order.begin() + target, moved);

	for (int i = count - 1; i >= 0; --i)
	{
		Core().RemoveBreakpointAt((size_t)i);
	}
	for (int i = 0; i < count; ++i)
	{
		Core().AddBreakpoint(order[(size_t)i]);
	}

	refresh_breakpoints();

	GtkListBoxRow *row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(sBreakpointList), target);
	if (row != NULL)
	{
		gtk_list_box_select_row(GTK_LIST_BOX(sBreakpointList), row);
	}

	update_breakpoint_buttons();
}

static void on_move_up_clicked(GtkWidget *widget, gpointer data)
{
	(void)widget;
	(void)data;

	const int index = selected_breakpoint_index();
	if (index > 0)
	{
		move_breakpoint(index, -1);
	}
}

static void on_move_down_clicked(GtkWidget *widget, gpointer data)
{
	(void)widget;
	(void)data;

	const int index = selected_breakpoint_index();
	if (index >= 0)
	{
		move_breakpoint(index, 1);
	}
}

static void on_breakpoint_selected(GtkListBox *box, GtkListBoxRow *row, gpointer data)
{
	(void)box;
	(void)data;

	update_breakpoint_buttons();

	if (sRefreshingBreakpoints || (row == NULL))
	{
		return;
	}

	const int index = gtk_list_box_row_get_index(row);
	if (index < 0)
	{
		return;
	}

	/* Selecting a breakpoint fills the entry, so the list doubles as a jump
	 * bar and "add" can start from an existing address. */
	gtk_editable_set_text(GTK_EDITABLE(sBreakpointEntry),
	                      FormatAddress(Core().BreakpointAt((size_t)index)).c_str());
}

static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval,
                               guint keycode, GdkModifierType modifiers, gpointer data)
{
	(void)controller;
	(void)keycode;
	(void)modifiers;
	(void)data;

	switch (keyval)
	{
		case GDK_KEY_Up:
			Core().ScrollLines(-1);
			break;
		case GDK_KEY_Down:
			Core().ScrollLines(1);
			break;
		case GDK_KEY_Page_Up:
			Core().ScrollPages(-1, (size_t)kPageLines);
			break;
		case GDK_KEY_Page_Down:
			Core().ScrollPages(1, (size_t)kPageLines);
			break;
		case GDK_KEY_Home:
			Core().FollowPC();
			break;
		case GDK_KEY_End:
			Core().SetAnchorAddress(0);
			break;
		default:
			return FALSE;
	}

	refresh_view();
	return TRUE;
}

static gboolean on_scroll(GtkEventControllerScroll *controller, gdouble dx, gdouble dy,
                          gpointer data)
{
	(void)dx;
	(void)data;

	if (dy == 0.0)
	{
		return FALSE;
	}

	/* Touchpads report surface units; convert to rows. */
	if (gtk_event_controller_scroll_get_unit(controller) == GDK_SCROLL_UNIT_SURFACE)
	{
		double charWidth = 0.0;
		double lineHeight = 1.0;
		layout_metrics(&charWidth, &lineHeight);
		dy /= lineHeight;
	}

	sScrollRemainder += dy;
	const int lines = (int)sScrollRemainder;
	sScrollRemainder -= (double)lines;

	if (lines != 0)
	{
		Core().ScrollLines(lines * kWheelLines);
		refresh_view();
	}

	return TRUE;
}

static void on_click_pressed(GtkGestureClick *gesture, int n_press, double x, double y,
                             gpointer data)
{
	(void)gesture;
	(void)n_press;
	(void)x;
	(void)y;
	(void)data;

	gtk_widget_grab_focus(sDrawing);
}

/* ------------------------------------------------------------ debug break */

static void Open(void);

static gboolean on_debug_break_idle(gpointer data)
{
	const int cpu = GPOINTER_TO_INT(data);

	/* Acknowledge the pause; nothing else consumes the flag on this frontend
	 * and leaving it set would report a stale stop later. */
	NDS_TakeDebuggerPause();

	/* The window may have been closed while the breakpoint stayed armed. */
	if (sWindow == NULL)
	{
		Open();
		if (sWindow == NULL)
		{
			return G_SOURCE_REMOVE;
		}
	}

	/* After Open(): it restores the saved CPU and view mode, which would
	 * otherwise undo the jump below. */
	Core().SetCpu((cpu == NDS_DEBUG_CPU_ARM7) ? DisassemblyCore::CPU_ARM7
	                                          : DisassemblyCore::CPU_ARM9);
	Core().FollowPC();

	/* The core cleared `execute`; mirror that in the frontend's running state
	 * so the frame tick stops stepping an armed CPU and the pause is visible. */
	desmume_emu_set_running(FALSE);

	persist_preferences();
	sync_controls_from_core();
	refresh_all();
	gtk_window_present(GTK_WINDOW(sWindow));

	return G_SOURCE_REMOVE;
}

static void on_debug_break(int cpu, u32 address)
{
	(void)address;

	/* Called from inside NDS_exec(); only queue the UI work, so no widget is
	 * touched while the CPU loop is unwinding. */
	g_idle_add(on_debug_break_idle, GINT_TO_POINTER(cpu));
}

/* --------------------------------------------------------------- window */

static void Update(void)
{
	if (sWindow == NULL)
	{
		return;
	}

	if (!execute)
	{
		/* The core stopped itself (breakpoint, completed step, run to return).
		 * desmume_emu_get_running() only tracks the frontend's own flag, so it
		 * has to be told; otherwise the frame tick keeps calling into the
		 * halted CPU and the window never looks paused. */
		if (desmume_emu_get_running())
		{
			desmume_emu_set_running(FALSE);
		}

		if (sRefreshWhenStopped)
		{
			sRefreshWhenStopped = false;
			refresh_all();
		}
		return;
	}

	if (!sAutoUpdate)
	{
		return;
	}

	if (++sFramesSinceRefresh < (guint)sAutoUpdateFrames)
	{
		return;
	}

	sFramesSinceRefresh = 0;
	Core().FollowPC();
	refresh_view();
}

static GtkWidget *make_title_label(const char *text)
{
	GtkWidget *label = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	char *markup = g_markup_printf_escaped("<b>%s</b>", text);
	gtk_label_set_markup(GTK_LABEL(label), markup);
	g_free(markup);
	return label;
}

static GtkWidget *make_hex_label(const char *text)
{
	GtkWidget *label = gtk_label_new(text);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	AddHexClass(label);
	return label;
}

static void build_toolbar(GtkWidget *parent)
{
	GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_append(GTK_BOX(parent), toolbar);

	gtk_box_append(GTK_BOX(toolbar), gtk_label_new("CPU:"));

	GtkStringList *cpus = gtk_string_list_new(NULL);
	gtk_string_list_append(cpus, "ARM9");
	gtk_string_list_append(cpus, "ARM7");
	sCpuDrop = gtk_drop_down_new(G_LIST_MODEL(cpus), NULL);
	gtk_widget_set_tooltip_text(sCpuDrop, "Which CPU to disassemble");
	g_signal_connect(sCpuDrop, "notify::selected", G_CALLBACK(on_cpu_changed), NULL);
	gtk_box_append(GTK_BOX(toolbar), sCpuDrop);

	gtk_box_append(GTK_BOX(toolbar), gtk_label_new("Mode:"));

	GtkStringList *modes = gtk_string_list_new(NULL);
	gtk_string_list_append(modes, "Auto");
	gtk_string_list_append(modes, "ARM");
	gtk_string_list_append(modes, "Thumb");
	sModeDrop = gtk_drop_down_new(G_LIST_MODEL(modes), NULL);
	gtk_widget_set_tooltip_text(sModeDrop, "Auto follows CPSR.T of the selected CPU");
	g_signal_connect(sModeDrop, "notify::selected", G_CALLBACK(on_mode_changed), NULL);
	gtk_box_append(GTK_BOX(toolbar), sModeDrop);

	GtkWidget *goLabel = gtk_label_new_with_mnemonic("_Go to:");
	gtk_box_append(GTK_BOX(toolbar), goLabel);

	sGoEntry = gtk_entry_new();
	gtk_editable_set_width_chars(GTK_EDITABLE(sGoEntry), 10);
	gtk_editable_set_max_width_chars(GTK_EDITABLE(sGoEntry), 16);
	gtk_entry_set_placeholder_text(GTK_ENTRY(sGoEntry), "0200:0000");
	gtk_widget_set_tooltip_text(sGoEntry, "Address to show, e.g. 02000000 or 0200:0000");
	gtk_label_set_mnemonic_widget(GTK_LABEL(goLabel), sGoEntry);
	AddHexClass(sGoEntry);
	g_signal_connect(sGoEntry, "activate", G_CALLBACK(on_go_clicked), NULL);
	gtk_box_append(GTK_BOX(toolbar), sGoEntry);
	add_button(toolbar, "Go", G_CALLBACK(on_go_clicked));

	sAutoUpdateCheck = gtk_check_button_new_with_label("Auto-update");
	gtk_widget_set_tooltip_text(sAutoUpdateCheck,
	                            "Keep the view on the PC while the CPU runs");
	g_signal_connect(sAutoUpdateCheck, "toggled",
	                 G_CALLBACK(on_auto_update_toggled), NULL);
	gtk_box_append(GTK_BOX(toolbar), sAutoUpdateCheck);

	sAutoUpdateSpin = gtk_spin_button_new_with_range(1.0, (double)kMaxAutoUpdateFrames, 1.0);
	gtk_widget_set_tooltip_text(sAutoUpdateSpin, "Refresh every N emulated frames");
	g_signal_connect(sAutoUpdateSpin, "value-changed",
	                 G_CALLBACK(on_auto_update_frames_changed), NULL);
	gtk_box_append(GTK_BOX(toolbar), sAutoUpdateSpin);

	gtk_box_append(GTK_BOX(toolbar), gtk_label_new("frame(s)"));

	GtkWidget *refresh = gtk_button_new_with_label("Refresh");
	gtk_widget_set_hexpand(refresh, FALSE);
	gtk_widget_set_halign(refresh, GTK_ALIGN_END);
	g_signal_connect(refresh, "clicked", G_CALLBACK(on_refresh_clicked), NULL);
	gtk_box_append(GTK_BOX(toolbar), refresh);
}

static void build_register_pane(GtkWidget *parent)
{
	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 2);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
	gtk_box_append(GTK_BOX(parent), grid);

	/* Two columns of eight registers keeps the pane short. */
	for (int i = 0; i < 16; ++i)
	{
		const int column = (i < 8) ? 0 : 2;
		const int row = (i < 8) ? i : (i - 8);

		GtkWidget *label = gtk_label_new(kRegisterNames[i]);
		gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
		if (column == 2)
		{
			gtk_widget_set_margin_start(label, 8);
		}
		gtk_grid_attach(GTK_GRID(grid), label, column, row, 1, 1);

		GtkWidget *entry = gtk_entry_new();
		gtk_editable_set_width_chars(GTK_EDITABLE(entry), 9);
		gtk_editable_set_max_width_chars(GTK_EDITABLE(entry), 10);
		gtk_widget_set_hexpand(entry, TRUE);
		AddHexClass(entry);
		/* R15 is the PC and stays read-only, like the other frontends. */
		gtk_editable_set_editable(GTK_EDITABLE(entry), (i != 15));
		g_object_set_data(G_OBJECT(entry), "register-index", GINT_TO_POINTER(i));
		g_signal_connect(entry, "activate", G_CALLBACK(on_register_activate), NULL);
		gtk_grid_attach(GTK_GRID(grid), entry, column + 1, row, 1, 1);

		sRegisterEntries[i] = entry;
	}

	const int infoRow = 8;

	GtkWidget *cpsrTitle = gtk_label_new("CPSR");
	gtk_label_set_xalign(GTK_LABEL(cpsrTitle), 1.0f);
	gtk_widget_set_margin_top(cpsrTitle, 6);
	gtk_grid_attach(GTK_GRID(grid), cpsrTitle, 0, infoRow, 1, 1);

	GtkWidget *cpsrBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_set_margin_top(cpsrBox, 6);
	sCpsrValueLabel = make_hex_label("00000000");
	gtk_box_append(GTK_BOX(cpsrBox), sCpsrValueLabel);
	sCpsrFlagsLabel = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(sCpsrFlagsLabel), 0.0f);
	gtk_box_append(GTK_BOX(cpsrBox), sCpsrFlagsLabel);
	gtk_grid_attach(GTK_GRID(grid), cpsrBox, 1, infoRow, 3, 1);

	GtkWidget *modeTitle = gtk_label_new("Mode");
	gtk_label_set_xalign(GTK_LABEL(modeTitle), 1.0f);
	gtk_grid_attach(GTK_GRID(grid), modeTitle, 0, infoRow + 1, 1, 1);
	sModeValueLabel = make_hex_label("");
	gtk_grid_attach(GTK_GRID(grid), sModeValueLabel, 1, infoRow + 1, 3, 1);

	GtkWidget *spsrTitle = gtk_label_new("SPSR");
	gtk_label_set_xalign(GTK_LABEL(spsrTitle), 1.0f);
	gtk_grid_attach(GTK_GRID(grid), spsrTitle, 0, infoRow + 2, 1, 1);
	sSpsrValueLabel = make_hex_label("00000000");
	gtk_grid_attach(GTK_GRID(grid), sSpsrValueLabel, 1, infoRow + 2, 3, 1);
}

static void build_breakpoint_pane(GtkWidget *parent)
{
	gtk_box_append(GTK_BOX(parent), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_box_append(GTK_BOX(parent), make_title_label("Breakpoints"));

	GtkWidget *scroller = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
	                               GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(scroller, -1, kBreakpointListHeight);
	gtk_box_append(GTK_BOX(parent), scroller);

	sBreakpointList = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(sBreakpointList), GTK_SELECTION_SINGLE);
	g_signal_connect(sBreakpointList, "row-selected",
	                 G_CALLBACK(on_breakpoint_selected), NULL);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), sBreakpointList);

	GtkWidget *entryRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_append(GTK_BOX(parent), entryRow);

	sBreakpointEntry = gtk_entry_new();
	gtk_editable_set_width_chars(GTK_EDITABLE(sBreakpointEntry), 10);
	gtk_editable_set_max_width_chars(GTK_EDITABLE(sBreakpointEntry), 16);
	gtk_entry_set_placeholder_text(GTK_ENTRY(sBreakpointEntry), "0200:0000");
	AddHexClass(sBreakpointEntry);
	g_signal_connect(sBreakpointEntry, "activate",
	                 G_CALLBACK(on_breakpoint_activate), NULL);
	gtk_widget_set_hexpand(sBreakpointEntry, TRUE);
	gtk_box_append(GTK_BOX(entryRow), sBreakpointEntry);
	add_button(entryRow, "Add", G_CALLBACK(on_add_breakpoint_clicked));

	GtkWidget *buttonRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_append(GTK_BOX(parent), buttonRow);

	sRemoveButton = add_button(buttonRow, "Remove", G_CALLBACK(on_remove_breakpoint_clicked));
	sMoveUpButton = add_button(buttonRow, "Up", G_CALLBACK(on_move_up_clicked));
	sMoveDownButton = add_button(buttonRow, "Down", G_CALLBACK(on_move_down_clicked));
}

static gboolean on_close_request(GtkWindow *window, gpointer data)
{
	(void)window;
	(void)data;

	save_geometry();
	persist_preferences();
	return FALSE;
}

static void clear_state(void)
{
	if (sMessageSource != 0)
	{
		g_source_remove(sMessageSource);
		sMessageSource = 0;
	}

	sWindow = NULL;
	sDrawing = NULL;
	sStatusLabel = NULL;
	sMessageLabel = NULL;
	sCpuDrop = NULL;
	sModeDrop = NULL;
	sGoEntry = NULL;
	sAutoUpdateCheck = NULL;
	sAutoUpdateSpin = NULL;
	sCpsrValueLabel = NULL;
	sCpsrFlagsLabel = NULL;
	sModeValueLabel = NULL;
	sSpsrValueLabel = NULL;
	sBreakpointList = NULL;
	sBreakpointEntry = NULL;
	sRemoveButton = NULL;
	sMoveUpButton = NULL;
	sMoveDownButton = NULL;

	for (int i = 0; i < 16; ++i)
	{
		sRegisterEntries[i] = NULL;
	}

	sLines.clear();
	sVisibleRows = 1;
}

/* "destroy" carries (GtkWidget *, gpointer); clear_state() takes neither, so
 * it cannot be connected directly. */
static void on_destroy(GtkWidget *widget, gpointer data)
{
	(void)widget;
	(void)data;
	clear_state();
}

static void build_window(void)
{
	desmume::config::Config *config = desmume_host_config();

	int width = kDefaultWidth;
	int height = kDefaultHeight;
	if (config != NULL)
	{
		const int savedWidth = config->disasm_win_w.get();
		const int savedHeight = config->disasm_win_h.get();
		if ((savedWidth >= 700) && (savedHeight >= 500))
		{
			width = savedWidth;
			height = savedHeight;
		}
	}

	sWindow = desmume_tool_window_new(TOOL_NAME, NULL, width, height);
	g_object_add_weak_pointer(G_OBJECT(sWindow), (gpointer *)&sWindow);
	g_signal_connect(sWindow, "close-request", G_CALLBACK(on_close_request), NULL);
	g_signal_connect(sWindow, "destroy", G_CALLBACK(on_destroy), NULL);

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start(root, 6);
	gtk_widget_set_margin_end(root, 6);
	gtk_widget_set_margin_top(root, 6);
	gtk_widget_set_margin_bottom(root, 6);
	gtk_window_set_child(GTK_WINDOW(sWindow), root);

	build_toolbar(root);

	GtkWidget *body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_set_vexpand(body, TRUE);
	gtk_box_append(GTK_BOX(root), body);

	sDrawing = gtk_drawing_area_new();
	gtk_widget_set_focusable(sDrawing, TRUE);
	gtk_widget_set_hexpand(sDrawing, TRUE);
	gtk_widget_set_vexpand(sDrawing, TRUE);
	gtk_widget_set_size_request(sDrawing, 520, 320);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(sDrawing), on_draw, NULL, NULL);
	gtk_box_append(GTK_BOX(body), sDrawing);

	g_signal_connect(sDrawing, "resize", G_CALLBACK(on_resize), NULL);

	GtkEventController *key = gtk_event_controller_key_new();
	g_signal_connect(key, "key-pressed", G_CALLBACK(on_key_pressed), NULL);
	gtk_widget_add_controller(sDrawing, key);

	GtkEventController *scroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
	g_signal_connect(scroll, "scroll", G_CALLBACK(on_scroll), NULL);
	gtk_widget_add_controller(sDrawing, scroll);

	/* Named "clickGesture" because the core exports a global BOOL click. */
	GtkGesture *clickGesture = gtk_gesture_click_new();
	g_signal_connect(clickGesture, "pressed", G_CALLBACK(on_click_pressed), NULL);
	gtk_widget_add_controller(sDrawing, GTK_EVENT_CONTROLLER(clickGesture));

	GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_size_request(sidebar, kSidebarWidth, -1);
	gtk_widget_set_hexpand(sidebar, FALSE);
	gtk_box_append(GTK_BOX(body), sidebar);

	build_register_pane(sidebar);
	build_breakpoint_pane(sidebar);

	GtkWidget *buttonBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_append(GTK_BOX(root), buttonBar);
	add_button(buttonBar, "Step", G_CALLBACK(on_step_clicked));
	add_button(buttonBar, "Step Over", G_CALLBACK(on_step_over_clicked));
	add_button(buttonBar, "Continue", G_CALLBACK(on_continue_clicked));
	add_button(buttonBar, "Run To Return", G_CALLBACK(on_run_to_return_clicked));

	GtkWidget *statusBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_append(GTK_BOX(root), statusBar);

	sStatusLabel = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(sStatusLabel), 0.0f);
	gtk_widget_set_hexpand(sStatusLabel, TRUE);
	gtk_box_append(GTK_BOX(statusBar), sStatusLabel);

	sMessageLabel = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(sMessageLabel), 1.0f);
	gtk_box_append(GTK_BOX(statusBar), sMessageLabel);

	/* The font objects outlive individual windows: they are display-wide and
	 * recreating them per open would leak a GSettings and duplicate the
	 * "changed" handler. */
	if (sFontSettings == NULL)
	{
		sFontSettings = gtk_monospace_font_settings_new();
		if (sFontSettings != NULL)
		{
			g_signal_connect(sFontSettings, "changed::monospace-font-name",
			                 G_CALLBACK(on_font_changed), NULL);
		}
	}
	if (sFontProvider == NULL)
	{
		sFontProvider = gtk_css_provider_new();
		gtk_style_context_add_provider_for_display(gdk_display_get_default(),
		                                           GTK_STYLE_PROVIDER(sFontProvider),
		                                           GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}
	apply_monospace_font();

	/* A rough first guess; on_resize() replaces it once the widget is
	 * allocated, and refresh_view() decodes a couple of spare rows so the
	 * first paint never looks truncated. */
	sVisibleRows = kPageLines;
}

static void Open(void)
{
	if (sWindow != NULL)
	{
		gtk_window_present(GTK_WINDOW(sWindow));
		return;
	}

	load_preferences();
	NDS_SetDebugBreakHandler(on_debug_break);

	build_window();
	sync_controls_from_core();

	if (Core().AnchorAddress() == 0)
	{
		Core().FollowPC();
	}

	refresh_all();
	gtk_window_present(GTK_WINDOW(sWindow));
}

static void Close(void)
{
	if (sWindow != NULL)
	{
		gtk_window_destroy(GTK_WINDOW(sWindow));
	}
	clear_state();
}

static gboolean IsOpen(void)
{
	return (sWindow != NULL);
}

const DesmumeTool desmume_tool_disassembler =
{
	"disassembler", "Disassembler", Open, Update, Close, IsOpen
};
