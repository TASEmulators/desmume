/* disassembler.cpp - this file is part of DeSmuME
 *
 * Copyright (C) 2006 yopyop
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

// GTK3 host for the ARM disassembler.
//
// Decoding, address navigation, the ARM/Thumb/auto rules, register snapshots
// and the breakpoint list all live in ../../../shared/disassembly_core.cpp,
// which the Windows and Cocoa frontends use as well; this file only builds the
// window, forwards clicks to the core and redraws. The Windows tool used to
// open two windows, one per CPU; here the CPU is a combo box, exactly like the
// merged Windows dialog.
//
// Run control follows the same split as the other hosts of this core. The core
// arms a single step / run-to-return, or stops on a breakpoint, by itself; this
// file reports the stop and, when a button asks for execution to continue,
// calls Launch() so GTK's emulation idle source and the sound core come back
// up. Breakpoints are checked inside NDS_exec(), which runs on the GTK main
// thread, but the resulting UI work is deferred to an idle callback so that no
// widget is touched while the CPU loop is still unwinding.

#include <gtk/gtk.h>

#include <stdio.h>

#include <string>
#include <vector>

#include "../dTool.h"
#include "../config.h"
#include "../main.h"
#include "../../shared/gtk_monospace_font.h"
#include "../../../shared/disassembly_core.h"

#include "NDSSystem.h"

// The frontend's single config store, defined in main.cpp.
extern desmume::config::Config config;

// Owned by main.cpp; set to FALSE by dTool_CloseCallback() when the window is
// destroyed. A breakpoint can fire while the window is closed, in which case
// the window is rebuilt from scratch and has to re-register with the tool
// framework so that update() (and therefore auto-update) keeps working.
extern BOOL *dTools_running;

#define SHORTNAME "disassembler"
#define TOOL_NAME "Disassembler"

// The list is scrolled through the shared core (wheel, arrow and page keys move
// the anchor) instead of by GtkTextView, so the buffer only has to hold more
// instructions than the tallest window can show. Overbuilding is cheap: a
// refresh decodes this many instructions.
static const int kLineCount = 200;

// Used by the page keys. There is no line-based scrollbar, so an approximate
// page size is good enough.
static const int kPageLines = 24;

static const int kDefaultWidth = 900;
static const int kDefaultHeight = 720;
static const int kMinimumWidth = 780;
static const int kMinimumHeight = 640;
static const int kSidebarWidth = 276;
static const int kBreakpointListHeight = 132;
static const int kMessageTimeoutMs = 3000;
static const int kMaxAutoUpdateFrames = 99;
static const int kWheelLines = 3;

// Backgrounds for the two highlighted rows. Both are light on purpose: the
// foreground of those rows is forced to black, so they stay readable whatever
// the desktop theme is, which is what the Windows dialog does as well.
static const char kCurrentRowBackground[] = "#FFE066";
static const char kBreakpointRowBackground[] = "#FFB0B0";
static const char kForcedRowForeground[] = "#000000";

static const char * const kRegisterNames[16] =
{
	"R0",  "R1",  "R2",  "R3",  "R4",  "R5",  "R6",  "R7",
	"R8",  "R9",  "R10", "R11", "R12", "R13", "R14", "R15"
};

static const char * const kViewModeLabels[3] = { "Auto", "ARM", "Thumb" };

static GtkWidget *sWindow = NULL;
static GtkWidget *sStatusLabel = NULL;
static GtkWidget *sMessageLabel = NULL;

static GtkWidget *sCpuCombo = NULL;
static GtkWidget *sModeCombo = NULL;
static GtkWidget *sGoEntry = NULL;
static GtkWidget *sAutoUpdateCheck = NULL;
static GtkWidget *sAutoUpdateSpin = NULL;

static GtkWidget *sTextView = NULL;
static GtkTextBuffer *sBuffer = NULL;
static GtkTextTag *sBaseTag = NULL;
static GtkTextTag *sCurrentTag = NULL;
static GtkTextTag *sBreakpointTag = NULL;

static GtkWidget *sRegisterEntries[16];
static GtkWidget *sCpsrValueLabel = NULL;
static GtkWidget *sCpsrFlagsLabel = NULL;
static GtkWidget *sModeValueLabel = NULL;
static GtkWidget *sSpsrValueLabel = NULL;

static GtkListStore *sBreakpointStore = NULL;
static GtkWidget *sBreakpointView = NULL;
static GtkWidget *sBreakpointEntry = NULL;
static GtkCellRenderer *sBreakpointRenderer = NULL;
static GtkWidget *sRemoveButton = NULL;
static GtkWidget *sMoveUpButton = NULL;
static GtkWidget *sMoveDownButton = NULL;

// CSS provider holding the monospace family for the hex entry fields and the
// value labels; reloaded when the desktop monospace font changes.
static GtkCssProvider *sFontProvider = NULL;

static int sToolID = -1;
static bool sAutoUpdate = true;
static int sAutoUpdateFrames = 1;
static guint sFramesSinceRefresh = 0;

// Set when the CPU is armed and expected to stop again (step, run to return).
// update() consumes it once the CPU is idle, so the view shows the instruction
// the CPU stopped on even when auto-update is off.
static bool sRefreshWhenStopped = FALSE;

// Guards the signal handlers while the controls are being populated from the
// core, otherwise setting a combo box would write the value straight back.
static bool sUpdating = FALSE;

// Set while refresh_breakpoints() rebuilds the list. Repopulating a
// GtkListStore clears and re-emits the selection, and the selection handler
// writes the selected address into the entry field, which would overwrite what
// the user just typed.
static bool sRefreshingBreakpoints = FALSE;

static guint sMessageSource = 0;
static gdouble sSmoothScrollRemainder = 0.0;

/////////////////////////////// SMALL HELPERS ///////////////////////////////

static DisassemblyCore& Core(void)
{
	return DisassemblyCore::Get();
}

static const char* CpuName(void)
{
	return (Core().GetCpu() == DisassemblyCore::CPU_ARM7) ? "ARM7" : "ARM9";
}

static bool ParseEntry(GtkWidget *entry, uint32_t *outValue)
{
	uint32_t value = 0;
	if (!DisassemblyCore::ParseHex32(gtk_entry_get_text(GTK_ENTRY(entry)), value))
	{
		return false;
	}

	*outValue = value;
	return true;
}

static std::string FormatAddress(uint32_t address)
{
	char text[24];
	snprintf(text, sizeof(text), "%04X:%04X",
	         (unsigned int)(address >> 16), (unsigned int)(address & 0xFFFF));
	return std::string(text);
}

static gboolean clear_message(gpointer data)
{
	sMessageSource = 0;
	if (sMessageLabel != NULL)
	{
		gtk_label_set_text(GTK_LABEL(sMessageLabel), "");
	}
	return FALSE;
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

/////////////////////////////// PREFERENCES ///////////////////////////////

static void save_geometry(void)
{
	int width = 0;
	int height = 0;
	gtk_window_get_size(GTK_WINDOW(sWindow), &width, &height);

	if ((width >= kMinimumWidth) && (height >= kMinimumHeight))
	{
		config.disasm_win_w = width;
		config.disasm_win_h = height;
	}
}

static void persist_preferences(void)
{
	config.disasm_cpu = (int)Core().GetCpu();
	config.disasm_mode = (int)Core().GetViewMode();
	config.disasm_autoupdate = sAutoUpdate;
	config.disasm_autoupdate_frames = sAutoUpdateFrames;
	config.save();
}

static void load_preferences(void)
{
	int cpu = config.disasm_cpu;
	if ((cpu < 0) || (cpu >= DisassemblyCore::CPU_COUNT))
	{
		cpu = DisassemblyCore::CPU_ARM9;
	}
	Core().SetCpu((DisassemblyCore::Cpu)cpu);

	int mode = config.disasm_mode;
	if ((mode < DisassemblyCore::ViewMode_Auto) || (mode > DisassemblyCore::ViewMode_Thumb))
	{
		mode = DisassemblyCore::ViewMode_Auto;
	}
	Core().SetViewMode((DisassemblyCore::ViewMode)mode);

	sAutoUpdate = config.disasm_autoupdate;

	int frames = config.disasm_autoupdate_frames;
	if ((frames < 1) || (frames > kMaxAutoUpdateFrames))
	{
		frames = 1;
	}
	sAutoUpdateFrames = frames;
}

/////////////////////////////// FONTS ///////////////////////////////

// The view follows the desktop monospace font, like the RAM Watch and RAM
// Search tables do. Each widget type needs a different mechanism: a text tag
// for the GtkTextView, a CSS class for entries and labels, and a property for
// the cell renderer of the breakpoint list.
static void apply_monospace_font(GSettings *settings)
{
	PangoFontDescription *font = gtk_monospace_font_description(sTextView, settings);

	if (sBaseTag != NULL)
	{
		g_object_set(sBaseTag, "font-desc", font, NULL);
	}

	if (sBreakpointRenderer != NULL)
	{
		const char *family = pango_font_description_get_family(font);
		if (family != NULL)
		{
			g_object_set(sBreakpointRenderer, "family", family, NULL);
		}
	}

	const char *family = pango_font_description_get_family(font);
	if ((family != NULL) && (sFontProvider != NULL))
	{
		char *css = g_strdup_printf("entry.hex, label.hex { font-family: \"%s\"; }", family);
		gtk_css_provider_load_from_data(sFontProvider, css, -1, NULL);
		g_free(css);
	}

	pango_font_description_free(font);
}

static void on_monospace_font_changed(GSettings *settings, gchar *key, gpointer data)
{
	apply_monospace_font(settings);
}

/////////////////////////////// REFRESH ///////////////////////////////

static void reload_lines(void)
{
	if (sBuffer == NULL)
	{
		return;
	}

	std::vector<DisassemblyCore::Line> lines;
	lines.reserve((size_t)kLineCount);
	Core().BuildLines((size_t)kLineCount, lines);

	gtk_text_buffer_set_text(sBuffer, "", -1);

	GtkTextIter iter;
	gtk_text_buffer_get_end_iter(sBuffer, &iter);

	for (size_t i = 0; i < lines.size(); ++i)
	{
		const DisassemblyCore::Line &line = lines[i];

		GtkTextTag *tag = NULL;
		if (line.Current)
		{
			tag = sCurrentTag;
		}
		else if (line.Breakpoint)
		{
			tag = sBreakpointTag;
		}

		gtk_text_buffer_insert_with_tags(sBuffer, &iter, line.Text.c_str(), -1, sBaseTag, tag, NULL);
		gtk_text_buffer_insert_with_tags(sBuffer, &iter, "\n", -1, sBaseTag, NULL);
	}
}

static void refresh_registers(void)
{
	DisassemblyCore::Registers regs;
	if (!Core().ReadRegisters(regs))
	{
		return;
	}

	char text[32];

	for (int i = 0; i < 16; ++i)
	{
		GtkWidget *field = sRegisterEntries[i];
		if ((field == NULL) || gtk_widget_has_focus(field))
		{
			// Never overwrite a value the user is in the middle of typing.
			continue;
		}

		snprintf(text, sizeof(text), "%08X", (unsigned int)regs.R[i]);
		gtk_entry_set_text(GTK_ENTRY(field), text);
	}

	snprintf(text, sizeof(text), "%08X", (unsigned int)regs.CPSR);
	gtk_label_set_text(GTK_LABEL(sCpsrValueLabel), text);

	snprintf(text, sizeof(text), "%08X", (unsigned int)regs.SPSR);
	gtk_label_set_text(GTK_LABEL(sSpsrValueLabel), text);

	snprintf(text, sizeof(text), "%02X %s", (unsigned int)(regs.Mode & 0x1F),
	         DisassemblyCore::CpuModeName(regs.Mode));
	gtk_label_set_text(GTK_LABEL(sModeValueLabel), text);

	// Set flags are red and bold, cleared ones grey, so the strip stays
	// readable on both light and dark themes.
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
	gtk_label_set_markup(GTK_LABEL(sCpsrFlagsLabel), markup->str);
	g_string_free(markup, TRUE);
}

static void refresh_status(void)
{
	char text[96];
	snprintf(text, sizeof(text), "%s  |  %s  |  %s", CpuName(),
	         Core().IsThumbMode() ? "Thumb" : "ARM",
	         FormatAddress(Core().AnchorAddress()).c_str());
	gtk_label_set_text(GTK_LABEL(sStatusLabel), text);

	char title[96];
	snprintf(title, sizeof(title), "%s - %s (%s)", TOOL_NAME, CpuName(),
	         Core().IsThumbMode() ? "Thumb" : "ARM");
	gtk_window_set_title(GTK_WINDOW(sWindow), title);
}

// Instructions and registers only: cheap enough to run every emulated frame.
static void refresh_view(void)
{
	reload_lines();
	refresh_registers();
	refresh_status();
}

static bool selected_breakpoint_index(int *outIndex)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sBreakpointView));
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;

	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		return false;
	}

	GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
	if (path == NULL)
	{
		return false;
	}

	*outIndex = gtk_tree_path_get_indices(path)[0];
	gtk_tree_path_free(path);
	return true;
}

static void select_breakpoint_row(int index)
{
	if (index < 0)
	{
		return;
	}

	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sBreakpointView));
	GtkTreePath *path = gtk_tree_path_new_from_indices(index, -1);
	gtk_tree_selection_select_path(selection, path);
	gtk_tree_path_free(path);
}

static void update_breakpoint_buttons(void)
{
	int index = -1;
	const int count = (int)Core().BreakpointCount();
	const bool selected = selected_breakpoint_index(&index);

	gtk_widget_set_sensitive(sRemoveButton, selected);
	gtk_widget_set_sensitive(sMoveUpButton, selected && (index > 0));
	gtk_widget_set_sensitive(sMoveDownButton, selected && (index < count - 1));
}

static void refresh_breakpoints(void)
{
	if ((sBreakpointStore == NULL) || (sBreakpointView == NULL))
	{
		return;
	}

	// Keep the selection on the same row so that Add/Remove/Up/Down do not lose
	// the user's place.
	int selectedIndex = -1;
	selected_breakpoint_index(&selectedIndex);

	sRefreshingBreakpoints = TRUE;

	gtk_list_store_clear(sBreakpointStore);

	const size_t count = Core().BreakpointCount();
	for (size_t i = 0; i < count; ++i)
	{
		GtkTreeIter iter;
		gtk_list_store_append(sBreakpointStore, &iter);
		gtk_list_store_set(sBreakpointStore, &iter, 0,
		                   FormatAddress(Core().BreakpointAt(i)).c_str(), -1);
	}

	if ((size_t)selectedIndex >= count)
	{
		selectedIndex = (int)count - 1;
	}
	select_breakpoint_row(selectedIndex);

	sRefreshingBreakpoints = FALSE;
}

// Everything, including the parts that can only change through this window.
static void refresh_all(void)
{
	refresh_view();
	refresh_breakpoints();
	update_breakpoint_buttons();
}

static void sync_controls_from_core(void)
{
	sUpdating = true;

	gtk_combo_box_set_active(GTK_COMBO_BOX(sCpuCombo),
	                         (Core().GetCpu() == DisassemblyCore::CPU_ARM7) ? 1 : 0);
	gtk_combo_box_set_active(GTK_COMBO_BOX(sModeCombo), (int)Core().GetViewMode());

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sAutoUpdateCheck), sAutoUpdate ? TRUE : FALSE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(sAutoUpdateSpin), (gdouble)sAutoUpdateFrames);
	gtk_widget_set_sensitive(sAutoUpdateSpin, sAutoUpdate ? TRUE : FALSE);

	sUpdating = false;
}

/////////////////////////////// RUN CONTROL ///////////////////////////////

// The core armed the CPU; hand control back to the GTK emulation loop. Launch()
// resumes the sound core as well, which NDS_debug_continue() knows nothing
// about.
static void resume_emulation(void)
{
	sFramesSinceRefresh = 0;
	Launch(NULL, NULL, NULL);
}

// Checked before arming anything: without a ROM there is no CPU to step, and
// NDS_exec() would spin on a null cartridge. Same guard as FrameAdvance().
static bool ensure_rom_loaded(void)
{
	if (gameInfo.fROM != NULL)
	{
		return true;
	}

	ShowMessage("No ROM loaded.");
	return false;
}

/////////////////////////////// SIGNAL HANDLERS ///////////////////////////////

static void on_cpu_changed(GtkComboBox *combo, gpointer data)
{
	if (sUpdating)
	{
		return;
	}

	Core().SetCpu((gtk_combo_box_get_active(combo) == 1) ? DisassemblyCore::CPU_ARM7
	                                                    : DisassemblyCore::CPU_ARM9);
	Core().SetAnchorAddress(Core().AnchorAddress()); // re-align for the new CPU

	persist_preferences();
	refresh_all();
}

static void on_mode_changed(GtkComboBox *combo, gpointer data)
{
	if (sUpdating)
	{
		return;
	}

	switch (gtk_combo_box_get_active(combo))
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
	uint32_t address = 0;
	if (!ParseEntry(sGoEntry, &address))
	{
		ShowMessage("Not a valid hex address.");
		return;
	}

	Core().SetAnchorAddress(address);
	refresh_all();
}

static void on_go_clicked(GtkButton *button, gpointer data)
{
	do_go();
}

static gboolean on_go_activate(GtkEntry *entry, gpointer data)
{
	do_go();
	return TRUE;
}

static void on_auto_update_toggled(GtkToggleButton *button, gpointer data)
{
	if (sUpdating)
	{
		return;
	}

	sAutoUpdate = gtk_toggle_button_get_active(button) ? true : false;
	gtk_widget_set_sensitive(sAutoUpdateSpin, sAutoUpdate ? TRUE : FALSE);

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

static void on_refresh_clicked(GtkButton *button, gpointer data)
{
	Core().FollowPC();
	refresh_all();
}

static void on_step_clicked(GtkButton *button, gpointer data)
{
	if (!ensure_rom_loaded())
	{
		return;
	}

	Core().StepInstruction();
	sRefreshWhenStopped = true;
	resume_emulation();
}

static void on_step_over_clicked(GtkButton *button, gpointer data)
{
	if (!ensure_rom_loaded())
	{
		return;
	}

	Core().StepOver();
	sRefreshWhenStopped = true;
	resume_emulation();
}

static void on_run_to_return_clicked(GtkButton *button, gpointer data)
{
	if (!ensure_rom_loaded())
	{
		return;
	}

	Core().RunToReturn();
	sRefreshWhenStopped = true;
	resume_emulation();
}

static void on_continue_clicked(GtkButton *button, gpointer data)
{
	if (!ensure_rom_loaded())
	{
		return;
	}

	Core().Continue();
	sRefreshWhenStopped = true;
	resume_emulation();
}

static void commit_register(GtkEntry *entry)
{
	const int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(entry), "register-index"));
	uint32_t value = 0;

	if (ParseEntry(GTK_WIDGET(entry), &value))
	{
		Core().WriteRegister((unsigned int)index, value);
	}

	// Rewrites the field from the core, which also reverts text that did not
	// parse, exactly like the other two frontends do.
	refresh_registers();
}

static void on_register_activate(GtkEntry *entry, gpointer data)
{
	commit_register(entry);
}

static gboolean on_register_focus_out(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
	commit_register(GTK_ENTRY(widget));
	return FALSE;
}

static void do_add_breakpoint(void)
{
	uint32_t address = 0;
	if (!ParseEntry(sBreakpointEntry, &address))
	{
		ShowMessage("Not a valid hex address.");
		return;
	}

	// Align first so the address matches an instruction boundary: adding
	// "0200:0001" in ARM mode would otherwise never be reached.
	address &= ~(Core().InstructionSize() - 1);

	if (!Core().AddBreakpoint(address))
	{
		ShowMessage("Breakpoint already exists.");
		return;
	}

	refresh_all();
}

static void on_add_breakpoint_clicked(GtkButton *button, gpointer data)
{
	do_add_breakpoint();
}

static gboolean on_breakpoint_activate(GtkEntry *entry, gpointer data)
{
	do_add_breakpoint();
	return TRUE;
}

static void on_remove_breakpoint_clicked(GtkButton *button, gpointer data)
{
	int index = -1;
	if (!selected_breakpoint_index(&index))
	{
		return;
	}

	Core().RemoveBreakpointAt((size_t)index);
	refresh_breakpoints();
	update_breakpoint_buttons();
}

// Reorders one breakpoint by delta (-1 or +1). The core only exposes append and
// erase, so the whole list is rebuilt in the requested order; every call here
// runs on the GTK main thread, so the emulation cannot observe the rebuild.
static void move_breakpoint(int index, int delta)
{
	const int count = (int)Core().BreakpointCount();
	const int target = index + delta;
	if ((index < 0) || (index >= count) || (target < 0) || (target >= count))
	{
		return;
	}

	std::vector<uint32_t> order;
	order.reserve((size_t)count);
	for (int i = 0; i < count; ++i)
	{
		order.push_back(Core().BreakpointAt((size_t)i));
	}

	const uint32_t moved = order[(size_t)index];
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
	select_breakpoint_row(target);
	update_breakpoint_buttons();
}

static void on_move_up_clicked(GtkButton *button, gpointer data)
{
	int index = -1;
	if (selected_breakpoint_index(&index))
	{
		move_breakpoint(index, -1);
	}
}

static void on_move_down_clicked(GtkButton *button, gpointer data)
{
	int index = -1;
	if (selected_breakpoint_index(&index))
	{
		move_breakpoint(index, 1);
	}
}

static void on_breakpoint_selection_changed(GtkTreeSelection *selection, gpointer data)
{
	update_breakpoint_buttons();

	if (sRefreshingBreakpoints)
	{
		return;
	}

	// Show the selected breakpoint so that the list doubles as a jump bar.
	int index = -1;
	if (!selected_breakpoint_index(&index))
	{
		return;
	}

	const uint32_t address = Core().BreakpointAt((size_t)index);
	char text[16];
	snprintf(text, sizeof(text), "%04X:%04X",
	         (unsigned int)(address >> 16), (unsigned int)(address & 0xFFFF));
	gtk_entry_set_text(GTK_ENTRY(sBreakpointEntry), text);
}

static gboolean on_list_scroll(GtkWidget *widget, GdkEventScroll *event, gpointer data)
{
	gdouble deltaX = 0.0;
	gdouble deltaY = 0.0;

	// Smooth (touchpad) scrolling reports deltas instead of a direction.
	if (gdk_event_get_scroll_deltas((GdkEvent *)event, &deltaX, &deltaY))
	{
		sSmoothScrollRemainder += deltaY;
		const int lines = (int)sSmoothScrollRemainder;
		if (lines != 0)
		{
			sSmoothScrollRemainder -= (gdouble)lines;
			Core().ScrollLines(lines * kWheelLines);
			refresh_view();
		}
		return TRUE;
	}

	GdkScrollDirection direction;
	if (gdk_event_get_scroll_direction((GdkEvent *)event, &direction))
	{
		if (direction == GDK_SCROLL_UP)
		{
			Core().ScrollLines(-kWheelLines);
			refresh_view();
		}
		else if (direction == GDK_SCROLL_DOWN)
		{
			Core().ScrollLines(kWheelLines);
			refresh_view();
		}
	}

	return TRUE;
}

static gboolean on_list_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	switch (event->keyval)
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

/////////////////////////////// DEBUG BREAK ///////////////////////////////

static void open_window(int id);

static gboolean on_debug_break_idle(gpointer data)
{
	const int cpu = GPOINTER_TO_INT(data);

	// Acknowledge the pause: nothing else reads the flag on GTK, and leaving it
	// set would make a later NDS_TakeDebuggerPause() report a stale stop.
	NDS_TakeDebuggerPause();

	// The window may have been closed while the breakpoint stayed armed.
	if (sWindow == NULL)
	{
		open_window(sToolID);
		if (sWindow == NULL)
		{
			return FALSE;
		}

		// open_window() registered the tool once already, but the framework
		// cleared that flag on destroy.
		if ((dTools_running != NULL) && (sToolID >= 0))
		{
			dTools_running[sToolID] = TRUE;
		}
	}

	// After open_window(): it loads the saved CPU and view mode from the
	// preferences, which would otherwise undo the jump below.
	Core().SetCpu((cpu == NDS_DEBUG_CPU_ARM7) ? DisassemblyCore::CPU_ARM7 : DisassemblyCore::CPU_ARM9);
	Core().FollowPC();

	// Mirrors what Pause() does from the menu: sound off, status bar and the
	// Run/Pause actions consistent with a stopped CPU.
	Pause(NULL, NULL, NULL);

	persist_preferences();
	sync_controls_from_core();
	refresh_all();
	gtk_window_present(GTK_WINDOW(sWindow));

	return FALSE;
}

static void on_debug_break(int cpu, u32 address)
{
	(void)address;

	// Called from inside NDS_exec(). Only queue the UI work: touching widgets,
	// or resuming emulation, while the CPU loop is still unwinding would
	// re-enter the core.
	g_idle_add(on_debug_break_idle, GINT_TO_POINTER(cpu));
}

/////////////////////////////// WINDOW ///////////////////////////////

static void update(void)
{
	if (sWindow == NULL)
	{
		return;
	}

	if (!execute)
	{
		// A step or a run-to-return leaves the CPU stopped inside the frame
		// that was just emulated. Refresh once here so the view shows where it
		// stopped even when auto-update is off.
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

static GtkWidget *make_label(const char *text, bool bold)
{
	GtkWidget *label = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);

	if (bold)
	{
		char *markup = g_markup_printf_escaped("<b>%s</b>", text);
		gtk_label_set_markup(GTK_LABEL(label), markup);
		g_free(markup);
	}
	else
	{
		gtk_label_set_text(GTK_LABEL(label), text);
	}

	return label;
}

static GtkWidget *make_hex_label(const char *text)
{
	GtkWidget *label = gtk_label_new(text);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_style_context_add_class(gtk_widget_get_style_context(label), "hex");
	return label;
}

static GtkWidget *make_button(const char *text, GCallback callback, GtkWidget *box)
{
	GtkWidget *button = gtk_button_new_with_label(text);
	g_signal_connect(G_OBJECT(button), "clicked", callback, NULL);
	gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
	return button;
}

static void build_top_bar(GtkWidget *parent)
{
	GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(parent), bar, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(bar), gtk_label_new("CPU:"), FALSE, FALSE, 0);

	sCpuCombo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sCpuCombo), "ARM9");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sCpuCombo), "ARM7");
	gtk_combo_box_set_active(GTK_COMBO_BOX(sCpuCombo), 0);
	g_signal_connect(G_OBJECT(sCpuCombo), "changed", G_CALLBACK(on_cpu_changed), NULL);
	gtk_box_pack_start(GTK_BOX(bar), sCpuCombo, FALSE, FALSE, 0);

	GtkWidget *modeLabel = gtk_label_new("Mode:");
	gtk_widget_set_margin_start(modeLabel, 6);
	gtk_box_pack_start(GTK_BOX(bar), modeLabel, FALSE, FALSE, 0);

	sModeCombo = gtk_combo_box_text_new();
	for (int i = 0; i < 3; ++i)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sModeCombo), kViewModeLabels[i]);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(sModeCombo), 0);
	g_signal_connect(G_OBJECT(sModeCombo), "changed", G_CALLBACK(on_mode_changed), NULL);
	gtk_box_pack_start(GTK_BOX(bar), sModeCombo, FALSE, FALSE, 0);

	GtkWidget *goLabel = gtk_label_new("Go to:");
	gtk_widget_set_margin_start(goLabel, 6);
	gtk_box_pack_start(GTK_BOX(bar), goLabel, FALSE, FALSE, 0);

	sGoEntry = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(sGoEntry), 10);
	gtk_entry_set_max_length(GTK_ENTRY(sGoEntry), 16);
	gtk_entry_set_placeholder_text(GTK_ENTRY(sGoEntry), "0200:0000");
	gtk_style_context_add_class(gtk_widget_get_style_context(sGoEntry), "hex");
	g_signal_connect(G_OBJECT(sGoEntry), "activate", G_CALLBACK(on_go_activate), NULL);
	gtk_box_pack_start(GTK_BOX(bar), sGoEntry, FALSE, FALSE, 0);
	make_button("Go", G_CALLBACK(on_go_clicked), bar);

	sAutoUpdateCheck = gtk_check_button_new_with_label("Auto-update");
	gtk_widget_set_margin_start(sAutoUpdateCheck, 12);
	g_signal_connect(G_OBJECT(sAutoUpdateCheck), "toggled", G_CALLBACK(on_auto_update_toggled), NULL);
	gtk_box_pack_start(GTK_BOX(bar), sAutoUpdateCheck, FALSE, FALSE, 0);

	sAutoUpdateSpin = gtk_spin_button_new_with_range(1.0, (double)kMaxAutoUpdateFrames, 1.0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(sAutoUpdateSpin), 1.0);
	gtk_widget_set_tooltip_text(sAutoUpdateSpin,
	                            "Follow the PC and refresh every N emulated frames.");
	g_signal_connect(G_OBJECT(sAutoUpdateSpin), "value-changed",
	                 G_CALLBACK(on_auto_update_frames_changed), NULL);
	gtk_box_pack_start(GTK_BOX(bar), sAutoUpdateSpin, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(bar), gtk_label_new("frame(s)"), FALSE, FALSE, 0);

	GtkWidget *refreshButton = gtk_button_new_with_label("Refresh");
	g_signal_connect(G_OBJECT(refreshButton), "clicked", G_CALLBACK(on_refresh_clicked), NULL);
	gtk_box_pack_end(GTK_BOX(bar), refreshButton, FALSE, FALSE, 0);
}

static void build_list(GtkWidget *parent)
{
	// GTK_POLICY_EXTERNAL: no scrollbars are drawn and, unlike NEVER, the child
	// does not report its full height as the window's size request. GtkTextView
	// reports its whole 200-line buffer as its natural height once laid out, so
	// without this the window would map several screens tall. Scrolling is done
	// by the core, so the adjustments are never touched and the child stays
	// pinned to the top of the viewport.
	GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
	                               GTK_POLICY_EXTERNAL, GTK_POLICY_EXTERNAL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroller), GTK_SHADOW_IN);
	gtk_widget_set_hexpand(scroller, TRUE);
	gtk_widget_set_vexpand(scroller, TRUE);
	gtk_box_pack_start(GTK_BOX(parent), scroller, TRUE, TRUE, 0);

	sTextView = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(sTextView), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(sTextView), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(sTextView), GTK_WRAP_NONE);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(sTextView), 4);
	gtk_widget_set_can_focus(sTextView, TRUE);
	gtk_widget_add_events(sTextView, GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK);
	g_signal_connect(G_OBJECT(sTextView), "scroll-event", G_CALLBACK(on_list_scroll), NULL);
	g_signal_connect(G_OBJECT(sTextView), "key-press-event", G_CALLBACK(on_list_key_press), NULL);
	gtk_container_add(GTK_CONTAINER(scroller), sTextView);

	sBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(sTextView));

	sBaseTag = gtk_text_buffer_create_tag(sBuffer, "base", NULL);
	sCurrentTag = gtk_text_buffer_create_tag(sBuffer, "current",
	                                         "background", kCurrentRowBackground,
	                                         "foreground", kForcedRowForeground,
	                                         "weight", PANGO_WEIGHT_BOLD,
	                                         NULL);
	sBreakpointTag = gtk_text_buffer_create_tag(sBuffer, "breakpoint",
	                                            "background", kBreakpointRowBackground,
	                                            "foreground", kForcedRowForeground,
	                                            NULL);
}

static void build_register_pane(GtkWidget *parent)
{
	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 2);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
	gtk_box_pack_start(GTK_BOX(parent), grid, FALSE, FALSE, 0);

	// Two columns of eight registers keeps the pane short enough that the
	// breakpoint list still fits in the default window height.
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
		gtk_entry_set_width_chars(GTK_ENTRY(entry), 9);
		gtk_entry_set_max_length(GTK_ENTRY(entry), 10);
		gtk_widget_set_hexpand(entry, TRUE);
		gtk_style_context_add_class(gtk_widget_get_style_context(entry), "hex");
		// R15 is the PC: writing it from a text field has no sane meaning while
		// the CPU is stopped mid-instruction, and the other frontends skip it.
		gtk_editable_set_editable(GTK_EDITABLE(entry), (i != 15) ? TRUE : FALSE);
		g_object_set_data(G_OBJECT(entry), "register-index", GINT_TO_POINTER(i));
		g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_register_activate), NULL);
		g_signal_connect(G_OBJECT(entry), "focus-out-event",
		                 G_CALLBACK(on_register_focus_out), NULL);
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
	gtk_box_pack_start(GTK_BOX(cpsrBox), sCpsrValueLabel, FALSE, FALSE, 0);
	sCpsrFlagsLabel = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(sCpsrFlagsLabel), 0.0f);
	gtk_box_pack_start(GTK_BOX(cpsrBox), sCpsrFlagsLabel, FALSE, FALSE, 0);
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
	gtk_box_pack_start(GTK_BOX(parent), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),
	                   FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(parent), make_label("Breakpoints", true), FALSE, FALSE, 0);

	GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
	                               GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(scroller, -1, kBreakpointListHeight);
	gtk_box_pack_start(GTK_BOX(parent), scroller, FALSE, FALSE, 0);

	sBreakpointStore = gtk_list_store_new(1, G_TYPE_STRING);
	sBreakpointView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(sBreakpointStore));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(sBreakpointView), FALSE);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(sBreakpointView), FALSE);

	sBreakpointRenderer = gtk_cell_renderer_text_new();
	GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
		"Address", sBreakpointRenderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(sBreakpointView), column);
	gtk_container_add(GTK_CONTAINER(scroller), sBreakpointView);

	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sBreakpointView));
	g_signal_connect(G_OBJECT(selection), "changed",
	                 G_CALLBACK(on_breakpoint_selection_changed), NULL);

	GtkWidget *entryRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(parent), entryRow, FALSE, FALSE, 0);

	sBreakpointEntry = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(sBreakpointEntry), 10);
	gtk_entry_set_max_length(GTK_ENTRY(sBreakpointEntry), 16);
	gtk_entry_set_placeholder_text(GTK_ENTRY(sBreakpointEntry), "0200:0000");
	gtk_style_context_add_class(gtk_widget_get_style_context(sBreakpointEntry), "hex");
	g_signal_connect(G_OBJECT(sBreakpointEntry), "activate",
	                 G_CALLBACK(on_breakpoint_activate), NULL);
	gtk_box_pack_start(GTK_BOX(entryRow), sBreakpointEntry, TRUE, TRUE, 0);
	make_button("Add", G_CALLBACK(on_add_breakpoint_clicked), entryRow);

	GtkWidget *buttonRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(parent), buttonRow, FALSE, FALSE, 0);

	sRemoveButton = make_button("Remove", G_CALLBACK(on_remove_breakpoint_clicked), buttonRow);
	sMoveUpButton = make_button("Up", G_CALLBACK(on_move_up_clicked), buttonRow);
	sMoveDownButton = make_button("Down", G_CALLBACK(on_move_down_clicked), buttonRow);
}

static gboolean on_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	save_geometry();
	persist_preferences();
	return FALSE;
}

static void on_window_destroy(GtkWidget *widget, gpointer data)
{
	// A pending "clear the message" timeout would touch a dead widget.
	if (sMessageSource != 0)
	{
		g_source_remove(sMessageSource);
		sMessageSource = 0;
	}

	sWindow = NULL;
	sStatusLabel = NULL;
	sMessageLabel = NULL;
	sCpuCombo = NULL;
	sModeCombo = NULL;
	sGoEntry = NULL;
	sAutoUpdateCheck = NULL;
	sAutoUpdateSpin = NULL;
	sTextView = NULL;
	sBuffer = NULL;
	sBaseTag = NULL;
	sCurrentTag = NULL;
	sBreakpointTag = NULL;
	sCpsrValueLabel = NULL;
	sCpsrFlagsLabel = NULL;
	sModeValueLabel = NULL;
	sSpsrValueLabel = NULL;
	sBreakpointStore = NULL;
	sBreakpointView = NULL;
	sBreakpointEntry = NULL;
	sBreakpointRenderer = NULL;
	sRemoveButton = NULL;
	sMoveUpButton = NULL;
	sMoveDownButton = NULL;
	sFontProvider = NULL;

	for (int i = 0; i < 16; ++i)
	{
		sRegisterEntries[i] = NULL;
	}

	if (sToolID >= 0)
	{
		dTool_CloseCallback(sToolID);
	}
}

static void build_window(void)
{
	// Populating the controls fires their signals; without this the handlers
	// would write the freshly built values straight back to the core (and
	// persist the preferences) before the window even exists.
	sUpdating = TRUE;

	sWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(sWindow), TOOL_NAME);
	gtk_window_set_resizable(GTK_WINDOW(sWindow), TRUE);
	gtk_window_set_icon_name(GTK_WINDOW(sWindow), "org.desmume.DeSmuME");

	// Only a lower bound: the content (register grid plus breakpoint list) needs
	// roughly this much height, and the size request keeps the window usable
	// when it is resized down.
	gtk_widget_set_size_request(sWindow, kMinimumWidth, kMinimumHeight);

	g_signal_connect(G_OBJECT(sWindow), "delete-event", G_CALLBACK(on_delete_event), NULL);
	g_signal_connect(G_OBJECT(sWindow), "destroy", G_CALLBACK(on_window_destroy), NULL);

	GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start(outer, 6);
	gtk_widget_set_margin_end(outer, 6);
	gtk_widget_set_margin_top(outer, 6);
	gtk_widget_set_margin_bottom(outer, 6);
	gtk_container_add(GTK_CONTAINER(sWindow), outer);

	build_top_bar(outer);

	GtkWidget *body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start(GTK_BOX(outer), body, TRUE, TRUE, 0);

	build_list(body);

	GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_size_request(sidebar, kSidebarWidth, -1);
	// Explicitly off: a hexpanding child would propagate upwards and stop the
	// list from getting the extra width.
	gtk_widget_set_hexpand(sidebar, FALSE);
	gtk_box_pack_start(GTK_BOX(body), sidebar, FALSE, FALSE, 0);

	build_register_pane(sidebar);
	build_breakpoint_pane(sidebar);

	GtkWidget *buttonBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(outer), buttonBar, FALSE, FALSE, 0);
	make_button("Step", G_CALLBACK(on_step_clicked), buttonBar);
	make_button("Step Over", G_CALLBACK(on_step_over_clicked), buttonBar);
	make_button("Continue", G_CALLBACK(on_continue_clicked), buttonBar);
	make_button("Run To Return", G_CALLBACK(on_run_to_return_clicked), buttonBar);

	GtkWidget *statusBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(outer), statusBar, FALSE, FALSE, 0);

	sStatusLabel = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(sStatusLabel), 0.0f);
	gtk_box_pack_start(GTK_BOX(statusBar), sStatusLabel, FALSE, FALSE, 0);

	sMessageLabel = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(sMessageLabel), 1.0f);
	gtk_box_pack_end(GTK_BOX(statusBar), sMessageLabel, FALSE, FALSE, 0);

	sFontProvider = gtk_css_provider_new();
	gtk_style_context_add_provider(gtk_widget_get_style_context(sWindow),
	                               GTK_STYLE_PROVIDER(sFontProvider),
	                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	GSettings *fontSettings = gtk_monospace_font_settings_new();
	apply_monospace_font(fontSettings);
	if (fontSettings != NULL)
	{
		g_signal_connect_object(fontSettings, "changed::monospace-font-name",
		                        G_CALLBACK(on_monospace_font_changed), NULL, (GConnectFlags)0);
		g_object_set_data_full(G_OBJECT(sWindow), "monospace-font-settings",
		                       fontSettings, g_object_unref);
	}

	// Must happen after the content exists: a GtkTextView reports its whole
	// buffer as its natural height, and GtkWindow maps at the natural size of
	// its content when no default size is set yet.
	int width = kDefaultWidth;
	int height = kDefaultHeight;
	if ((config.disasm_win_w >= kMinimumWidth) && (config.disasm_win_h >= kMinimumHeight))
	{
		width = config.disasm_win_w;
		height = config.disasm_win_h;
	}
	gtk_window_set_default_size(GTK_WINDOW(sWindow), width, height);

	gtk_widget_show_all(sWindow);
}

static void open_window(int id)
{
	if (sWindow != NULL)
	{
		gtk_window_present(GTK_WINDOW(sWindow));
		return;
	}

	sToolID = id;

	load_preferences();
	NDS_SetDebugBreakHandler(on_debug_break);

	build_window();
	sync_controls_from_core();

	if (Core().AnchorAddress() == 0)
	{
		Core().FollowPC();
	}

	refresh_all();
}

static void close_tool(void)
{
	// The dTool framework never calls this; the window's own destroy handler
	// notifies it through dTool_CloseCallback().
}

dTool_t dTool_disassembler =
{
	SHORTNAME,
	TOOL_NAME,
	&open_window,
	&update,
	&close_tool
};
