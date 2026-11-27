/*
	configdialog.cpp - unified Emulation/Graphic/Sound/Firmware/Slots/Wi-Fi/
	                   Microphone/Paths settings for GTK4

	Copyright (C) 2026 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
	GTK3 shipped one dialog per topic (Emulation Settings, Graphics Settings,
	Firmware, Wi-Fi, Microphone, Slot 1, Slot 2, Path Settings) plus the audio
	switches in the menu.  GTK4 collects all of them into one window with one
	page per topic, so every value is applied and saved in the same place.

	The pages are grouped exactly like GTK3's dialogs, and the few options that
	only exist in one of them (Slot 1 FAT folder, Wi-Fi bridge adapter, ...)
	keep their GTK3 semantics.  Anything the core only reads while it boots
	(bus timing, BIOS/firmware images, firmware user data, slot devices, Wi-Fi)
	is compared before and after OK, and a reset is offered when it changed.
*/

#include "configdialog.h"

#include <string.h>

#include <string>
#include <vector>

#include "emulation.h"
#include "hotkeys.h"
#include "../shared/ctrlssdl.h"
#include "../../shared/path_settings_core.h"
#include "../../shared/wgpu_present.h"

namespace
{

/* Sentinel in presentationMasks[] for the "Software (Cairo)" row. Real
 * WGPUInstanceBackend masks are non-zero bit flags, so 0 is safe here. */
static const guint32 PRESENTATION_SOFTWARE = 0;

/* Folder rows of the Paths page, in the order the Windows dialog uses.  The
 * Movies row edits config.movie_directory, because the Play/Record dialogs
 * already remember their folder there. */
static const PathInfo::KnownPath kPathCategories[] =
{
	PathInfo::ROMS,
	PathInfo::BATTERY,
	PathInfo::SRAM_IMPORT_EXPORT,
	PathInfo::STATES,
	PathInfo::STATE_SLOTS,
	PathInfo::SCREENSHOTS,
	PathInfo::AVI_FILES,
	PathInfo::MOVIES,
	PathInfo::CHEATS,
	PathInfo::LUA,
};
static const int kPathCategoryCount = (int)G_N_ELEMENTS(kPathCategories);

static const char *const kFirmwareColors[16] =
{
	"Gray", "Brown", "Red", "Pink", "Orange", "Yellow", "Lime Green", "Green",
	"Dark Green", "Sea Green", "Turquoise", "Blue", "Dark Blue", "Dark Purple",
	"Violet", "Magenta"
};

static const char *const kMonthNames[12] =
{
	"January", "February", "March", "April", "May", "June",
	"July", "August", "September", "October", "November", "December"
};

static const char *const kLanguageNames[6] =
{
	"Japanese", "English", "French", "German", "Italian", "Spanish"
};

/* MicMode order, as stored in config.mic_mode. */
static const char *const kMicModes[4] =
{
	"Internal noise sample (deterministic)",
	"Sample file (deterministic)",
	"Random white noise (not suitable for TAS)",
	"Physical microphone (not suitable for TAS)"
};

static const char *const kColorDepths[3] = { "15 bit", "18 bit", "24 bit" };
static const int kColorDepthValues[3] = { 15, 18, 24 };

static const char *const kMultisampleLabels[6] =
	{ "Off", "2 samples", "4 samples", "8 samples", "16 samples", "32 samples" };
static const int kMultisampleValues[6] = { 0, 2, 4, 8, 16, 32 };

static const char *const kCFlashModes[3] =
{
	"Directory",
	"Image file",
	"ROM directory"
};

/* Grid order for the control dialog, Windows-style: D-pad and face buttons on
 * the left, Start/Select/shoulders plus the frontend extras on the right.
 * The same layout is used by GTK3 and by the joystick page. */
static const int kControlGrid[2][8] =
{
	{  6, 5,  7,  4, 1, 0, 11, 10 },   /* Up, Left, Down, Right, B, A, Y, X */
	{  3, 2,  9,  8, 12, 13, 14, -1 }, /* Start, Select, L, R, Debug, Boost, Lid */
};

/* Turbo keys are offered for the buttons a player would auto-fire. */
static const int kTurboGrid[2][6] =
{
	{ 0, 1, 10, 11, 9, 8 },   /* A, B, X, Y, L, R */
	{ 6, 7,  5,  4, 3, 2 },   /* Up, Down, Left, Right, Start, Select */
};

enum KeySlotKind
{
	KEY_SLOT_CONTROL,
	KEY_SLOT_TURBO,
	KEY_SLOT_HOTKEY
};

struct KeySlot
{
	GtkWidget *button;
	GtkWidget *label;
	GtkWidget *hint; /* page-local message line */
	guint keyval;
	GdkModifierType mods;
	KeySlotKind kind;
	int index; /* key_names index, or hotkey index */
};

struct ConfigDialog
{
	GtkWidget *window;
	GtkWidget *notebook;

	/* Emulation tab. */
	GtkWidget *useJit;
	GtkWidget *jitBlockSize;
	GtkWidget *advancedTiming;
	GtkWidget *gameHacks;
	GtkWidget *debugConsole;
	GtkWidget *ensata;
	GtkWidget *useExtBios;
	GtkWidget *bios9Entry;
	GtkWidget *bios9Browse;
	GtkWidget *bios7Entry;
	GtkWidget *bios7Browse;
	GtkWidget *swiFromBios;
	GtkWidget *patchSwi3;
	GtkWidget *useExtFirmware;
	GtkWidget *firmwareEntry;
	GtkWidget *firmwareBrowse;
	GtkWidget *bootFromFirmware;
	GtkWidget *useExtFirmwareSettings;

	/* Firmware page (user data written into the internal image). */
	GtkWidget *fwNickname;
	GtkWidget *fwMessage;
	GtkWidget *fwColorDrop;
	GtkWidget *fwMonthDrop;
	GtkWidget *fwDayDrop;
	GtkWidget *fwLanguageDrop;
	GtkWidget *fwMac;

	/* Graphic tab. */
	GtkWidget *rendererDrop;
	GtkWidget *colorDepthDrop;
	GtkWidget *multisampleDrop;
	GtkWidget *textureScaleDrop;
	GtkWidget *gpuScaleSpin;
	GtkWidget *deposterize;
	GtkWidget *smoothing;
	GtkWidget *highColorInterpolation;
	GtkWidget *txtHack;
	GtkWidget *lineHack;
	GtkWidget *edgeMark;
	GtkWidget *fog;
	GtkWidget *textures;
	GtkWidget *shadowPolygon;
	GtkWidget *zeroAlpha;
	GtkWidget *ndsDepth;
	GtkWidget *depthLEqual;
	GtkWidget *filterDrop;
	GtkWidget *presentationDrop;

	/* Sound tab. */
	GtkWidget *audioEnabled;
	GtkWidget *audioVolume;
	GtkWidget *syncDual;
	GtkWidget *syncSync;
	GtkWidget *syncN;
	GtkWidget *syncZ;
	GtkWidget *syncP;
	GtkWidget *interpolationDrop;

	/* Slots tab. */
	GtkWidget *slot1Drop;
	GtkWidget *slot1Description;
	GtkWidget *slot1FatEntry;
	GtkWidget *slot1FatBox;
	GtkWidget *slot1UseRomDir;
	GtkWidget *slot1DebugEntry;
	GtkWidget *slot1DebugBox;
	GtkWidget *slot2Drop;
	GtkWidget *slot2Description;
	GtkWidget *slot2CFlashDrop;
	GtkWidget *slot2CFlashEntry;
	GtkWidget *slot2CFlashBox;
	GtkWidget *slot2GbaEntry;
	GtkWidget *slot2GbaBox;

	/* Wi-Fi page. */
	GtkWidget *wifiEnabled;
	GtkWidget *wifiCompatibility;
	GtkWidget *wifiAdapterDrop;
	GtkWidget *wifiHint;

	/* Microphone page. */
	GtkWidget *micModeDrop;
	GtkWidget *micSampleEntry;
	GtkWidget *micSampleBox;

	/* Paths page. */
	GtkWidget *pathEntries[kPathCategoryCount];
	GtkWidget *shotFormatDrop;
	GtkWidget *shotNameEntry;
	GtkWidget *saveLastVisit;

	/* Controls and Hotkeys pages.  The arrays are private copies so Cancel
	 * leaves the live input tables untouched. */
	guint controlKeys[NB_KEYS];
	guint turboKeys[NB_KEYS];
	guint joyKeys[NB_KEYS];
	GtkWidget *controlButtons[NB_KEYS];
	GtkWidget *turboButtons[NB_KEYS];
	GtkWidget *joyButtons[NB_KEYS];
	GtkWidget *allowUpDown;
	std::vector<KeySlot> keySlots;
	int capturingSlot; /* index into keySlots, -1 when idle */
	GtkWidget *keyHint;

	/* The filter drop-down skips entries the build cannot provide, so the
	 * selected row has to be mapped back to a WGPUPresentUpscaler value. */
	int *filterValues;
	int filterCount;

	/* Presentation rows: one per WebGPU backend (mask stored in
	 * presentationMasks[i]) plus a final "Software (Cairo)" row whose mask is
	 * PRESENTATION_SOFTWARE. */
	guint32 *presentationMasks;
	int presentationCount;

	desmume::config::Config *config;
	DesmumeConfigAppliedFunc applied;
	gpointer appliedData;

	/* Borrowed: the main window owns it and outlives this dialog.  Used as
	 * the parent of the "reset now?" alert, which must survive this window. */
	GtkWindow *parentWindow;
};

ConfigDialog *sDialog = NULL;

/* ------------------------------------------------------------- small helpers */

void
GridLabel(GtkGrid *grid, const char *text, int row)
{
	GtkWidget *label = gtk_label_new(text);
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(grid, label, 0, row, 1, 1);
}

GtkWidget *
NewSection(const char *title, GtkWidget **gridOut)
{
	GtkWidget *frame = gtk_frame_new(title);
	GtkWidget *grid = gtk_grid_new();

	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
	gtk_widget_set_margin_start(grid, 12);
	gtk_widget_set_margin_end(grid, 12);
	gtk_widget_set_margin_top(grid, 6);
	gtk_widget_set_margin_bottom(grid, 6);
	gtk_frame_set_child(GTK_FRAME(frame), grid);

	*gridOut = grid;
	return frame;
}

GtkWidget *
NewPage(void)
{
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

	gtk_widget_set_margin_start(box, 12);
	gtk_widget_set_margin_end(box, 12);
	gtk_widget_set_margin_top(box, 12);
	gtk_widget_set_margin_bottom(box, 12);
	return box;
}

/* Every page goes through a scroller: the dialog keeps one height for all
 * eight topics, and the Paths page alone is taller than a laptop screen. */
GtkWidget *
WrapPage(GtkWidget *page)
{
	GtkWidget *scroller = gtk_scrolled_window_new();

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
	                               GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scroller, TRUE);
	gtk_widget_set_hexpand(page, TRUE);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), page);
	return scroller;
}

GtkWidget *
NewDropDown(const char *const *items)
{
	GtkWidget *drop = gtk_drop_down_new_from_strings(items);
	gtk_widget_set_hexpand(drop, TRUE);
	return drop;
}

void
SetDropDown(GtkWidget *drop, guint index)
{
	gtk_drop_down_set_selected(GTK_DROP_DOWN(drop), index);
}

guint
GetDropDown(GtkWidget *drop)
{
	const guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(drop));
	return (selected == GTK_INVALID_LIST_POSITION) ? 0 : selected;
}

/* Every drop down in this dialog is filled from a GtkStringList built here.
 *
 * gtk_drop_down_new() takes ownership of the model (transfer full): it refs
 * the model into its own property and then consumes the caller's reference.
 * Never g_object_unref() one of these lists after handing it to a drop down -
 * that frees the model while the dropdown's popup list view still points at
 * it, and destroying the dialog then trips
 * gtk_list_item_manager_clear_model() inside GTK (the
 * g_list_model_get_n_items() assertion spam that precedes it comes from the
 * same dangling model). */
GtkStringList *
NewStringList(void)
{
	return gtk_string_list_new(NULL);
}

void
AddRow(GtkGrid *grid, int row, const char *text, GtkWidget *widget)
{
	GridLabel(grid, text, row);
	gtk_widget_set_hexpand(widget, TRUE);
	gtk_grid_attach(grid, widget, 1, row, 1, 1);
}

GtkWidget *
NewHint(const char *text)
{
	GtkWidget *label = gtk_label_new(text);

	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_label_set_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
	gtk_widget_add_css_class(label, "dim-label");
	return label;
}

/* ------------------------------------------------------ key binding widgets */

bool
KeyIsModifierKey(guint keyval)
{
	switch (keyval)
	{
		case GDK_KEY_Shift_L: case GDK_KEY_Shift_R:
		case GDK_KEY_Control_L: case GDK_KEY_Control_R:
		case GDK_KEY_Alt_L: case GDK_KEY_Alt_R:
		case GDK_KEY_Meta_L: case GDK_KEY_Meta_R:
		case GDK_KEY_Super_L: case GDK_KEY_Super_R:
		case GDK_KEY_Hyper_L: case GDK_KEY_Hyper_R:
		case GDK_KEY_Caps_Lock: case GDK_KEY_Num_Lock: case GDK_KEY_Shift_Lock:
		case GDK_KEY_ISO_Level3_Shift: case GDK_KEY_ISO_Level5_Shift:
			return true;
		default:
			return false;
	}
}

/* Readable name of a bare key symbol, e.g. "Backspace" or "Up".  Returns a
 * pointer to a static buffer for the fallback path. */
const char *
KeySymbolLabel(guint keyval)
{
	static char buffer[64];
	static const struct { const char *symbol; const char *text; } renames[] =
	{
		{ "BackSpace", "Backspace" }, { "Return", "Enter" }, { "Escape", "Esc" },
		{ "space", "Space" }, { "Print", "Print Screen" },
		{ "Scroll_Lock", "Scroll Lock" }, { "Caps_Lock", "Caps Lock" },
		{ "Num_Lock", "Num Lock" }, { "Page_Up", "Page Up" }, { "Page_Down", "Page Down" },
	};

	const char *name = gdk_keyval_name(keyval);
	if (name == NULL)
	{
		g_snprintf(buffer, sizeof(buffer), "0x%04X", keyval);
		return buffer;
	}

	for (size_t i = 0; i < G_N_ELEMENTS(renames); ++i)
	{
		if (strcmp(name, renames[i].symbol) == 0)
		{
			return renames[i].text;
		}
	}

	size_t n = 0;
	for (const char *p = name; (*p != '\0') && (n < sizeof(buffer) - 1); ++p)
	{
		buffer[n++] = (*p == '_') ? ' ' : *p;
	}
	buffer[n] = '\0';
	return buffer;
}

void
EnsureKeyFieldStyle(void)
{
	static bool done = false;
	if (done)
	{
		return;
	}
	done = true;

	GtkCssProvider *provider = gtk_css_provider_new();
	gtk_css_provider_load_from_data(provider,
		"button.keyfield.conflict, button.keyfield.conflict label { color: #e01b24; }\n"
		"button.keyfield.conflict { background-image: none;"
		" background-color: alpha(#e01b24, 0.16); }\n"
		"button.keyfield.capturing { background-image: none;"
		" background-color: alpha(#3584e4, 0.20); }\n",
		-1);
	GdkDisplay *display = gdk_display_get_default();
	if (display != NULL)
	{
		gtk_style_context_add_provider_for_display(
			display, GTK_STYLE_PROVIDER(provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}
	g_object_unref(provider);
}

void
RefreshKeySlot(ConfigDialog *d, int index)
{
	KeySlot *slot = &d->keySlots[index];
	gchar *text;

	if (d->capturingSlot == index)
	{
		text = g_strdup("Press a key...");
	}
	else if (slot->keyval == 0)
	{
		text = g_strdup("Disabled");
	}
	else if (slot->kind == KEY_SLOT_HOTKEY)
	{
		text = gtk_accelerator_get_label(slot->keyval, slot->mods);
	}
	else
	{
		text = g_strdup(KeySymbolLabel(slot->keyval));
	}

	gtk_label_set_text(GTK_LABEL(slot->label), text);
	g_free(text);
}

/* Two entries clash when they use the same key with the same modifiers.  A
 * modified hotkey (Ctrl+X) does not clash with a bare game button (X). */
void
UpdateKeyConflicts(ConfigDialog *d)
{
	for (size_t i = 0; i < d->keySlots.size(); ++i)
	{
		const KeySlot &slot = d->keySlots[i];
		bool conflict = false;

		if (slot.keyval != 0)
		{
			for (size_t j = 0; j < d->keySlots.size(); ++j)
			{
				if (j == i)
				{
					continue;
				}
				const KeySlot &other = d->keySlots[j];
				if ((other.keyval == slot.keyval) && (other.mods == slot.mods))
				{
					conflict = true;
					break;
				}
			}
		}

		if (conflict)
		{
			gtk_widget_add_css_class(slot.button, "conflict");
		}
		else
		{
			gtk_widget_remove_css_class(slot.button, "conflict");
		}
	}
}

void
EndKeyCapture(ConfigDialog *d)
{
	const int index = d->capturingSlot;
	d->capturingSlot = -1;

	if (index >= 0)
	{
		KeySlot *slot = &d->keySlots[index];
		gtk_widget_remove_css_class(slot->button, "capturing");
		if (slot->hint != NULL)
		{
			gtk_label_set_text(GTK_LABEL(slot->hint), "");
		}
		RefreshKeySlot(d, index);
		gtk_window_set_focus(GTK_WINDOW(d->window), slot->button);
	}

	UpdateKeyConflicts(d);
}

void
StartKeyCapture(ConfigDialog *d, int index)
{
	if (d->capturingSlot == index)
	{
		return;
	}

	if (d->capturingSlot >= 0)
	{
		EndKeyCapture(d);
	}

	d->capturingSlot = index;
	for (size_t i = 0; i < d->keySlots.size(); ++i)
	{
		RefreshKeySlot(d, (int)i);
	}
	gtk_widget_add_css_class(d->keySlots[index].button, "capturing");

	/* Move the focus off the buttons: otherwise Space or Return would activate
	 * the focused button instead of being captured. */
	gtk_window_set_focus(GTK_WINDOW(d->window), NULL);
	if (d->keySlots[index].hint != NULL)
	{
		gtk_label_set_text(GTK_LABEL(d->keySlots[index].hint),
		                   "Press the key to use, or Escape to disable it.");
	}
}

void
OnKeySlotClicked(GtkButton *button, gpointer user_data)
{
	ConfigDialog *d = (ConfigDialog *)user_data;
	const int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "key-slot-index"));
	StartKeyCapture(d, index);
}

gboolean
OnKeyCapture(GtkEventControllerKey *controller, guint keyval, guint keycode,
             GdkModifierType state, gpointer user_data)
{
	(void)controller;
	(void)keycode;
	ConfigDialog *d = (ConfigDialog *)user_data;

	if ((d->capturingSlot < 0) || (d->capturingSlot >= (int)d->keySlots.size()))
	{
		return FALSE;
	}

	KeySlot *slot = &d->keySlots[d->capturingSlot];
	const GdkModifierType mods =
		(GdkModifierType)(state & gtk_accelerator_get_default_mod_mask());

	if ((keyval == GDK_KEY_Escape) && (mods == 0))
	{
		slot->keyval = 0;
		slot->mods = (GdkModifierType)0;
		EndKeyCapture(d);
		return TRUE;
	}

	if ((keyval == 0) ||
	    ((slot->kind == KEY_SLOT_HOTKEY) && KeyIsModifierKey(keyval)))
	{
		return TRUE;
	}

	if ((slot->kind != KEY_SLOT_HOTKEY) && (mods != 0))
	{
		if (slot->hint != NULL)
		{
			gtk_label_set_text(GTK_LABEL(slot->hint),
			                   "Game and turbo buttons are single keys: press it "
			                   "without Shift/Ctrl/Alt.");
		}
		return TRUE;
	}

	slot->keyval = (keyval == GDK_KEY_ISO_Left_Tab) ? GDK_KEY_Tab : keyval;
	slot->mods = (slot->kind == KEY_SLOT_HOTKEY) ? mods : (GdkModifierType)0;
	EndKeyCapture(d);
	return TRUE;
}

void
AttachKeySlot(ConfigDialog *d, GtkGrid *grid, int row, int col,
              const char *labelText, GtkWidget *hint, KeySlotKind kind,
              int index, guint keyval, GdkModifierType mods)
{
	GtkWidget *label = gtk_label_new(labelText);
	gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
	gtk_grid_attach(grid, label, col, row, 1, 1);

	GtkWidget *button = gtk_button_new();
	GtkWidget *text = gtk_label_new(NULL);
	gtk_button_set_child(GTK_BUTTON(button), text);
	gtk_widget_set_size_request(button, 150, -1);
	gtk_widget_add_css_class(button, "keyfield");
	gtk_grid_attach(grid, button, col + 1, row, 1, 1);

	KeySlot slot;
	slot.button = button;
	slot.label = text;
	slot.hint = hint;
	slot.keyval = keyval;
	slot.mods = mods;
	slot.kind = kind;
	slot.index = index;
	d->keySlots.push_back(slot);

	const int slotIndex = (int)d->keySlots.size() - 1;
	g_object_set_data(G_OBJECT(button), "key-slot-index",
	                  GINT_TO_POINTER((gint)slotIndex));
	g_signal_connect(button, "clicked", G_CALLBACK(OnKeySlotClicked), d);
	RefreshKeySlot(d, slotIndex);
}

/* ------------------------------------------------------------ joystick rows */

gchar *
JoyKeyLabel(guint key)
{
	if ((key == 0xFFFF) || (key == 0))
	{
		return g_strdup("Disabled");
	}
	return g_strdup_printf("%u", key);
}

void
RefreshJoyButton(ConfigDialog *d, int index)
{
	gchar *label = JoyKeyLabel(d->joyKeys[index]);
	gtk_button_set_label(GTK_BUTTON(d->joyButtons[index]), label);
	g_free(label);
}

struct JoyCapture
{
	ConfigDialog *d;
	int index;
	GtkWidget *window;
	GtkWidget *label;
	GtkWidget *ok;
	guint source;
	gboolean found;
	guint key;
};

void
OnJoyCaptureDestroy(GtkWidget *widget, gpointer user_data)
{
	(void)widget;
	JoyCapture *ctx = (JoyCapture *)user_data;

	if (ctx->source != 0)
	{
		g_source_remove(ctx->source);
		ctx->source = 0;
	}
	desmume_emu_set_joy_capture(FALSE);
	g_free(ctx);
}

gboolean
OnJoyCapturePoll(gpointer user_data)
{
	JoyCapture *ctx = (JoyCapture *)user_data;
	bool modified = false;
	const u16 key = get_joy_key(ctx->index, &modified);

	if (!modified)
	{
		return G_SOURCE_CONTINUE;
	}

	ctx->found = TRUE;
	ctx->key = key;
	ctx->source = 0;

	gchar *text = g_strdup_printf("You pressed: %u\nClick OK to keep this key.", key);
	gtk_label_set_text(GTK_LABEL(ctx->label), text);
	g_free(text);
	gtk_widget_set_sensitive(ctx->ok, TRUE);
	return G_SOURCE_REMOVE;
}

void
OnJoyCaptureOk(GtkButton *button, gpointer user_data)
{
	(void)button;
	JoyCapture *ctx = (JoyCapture *)user_data;

	if (ctx->found)
	{
		ctx->d->joyKeys[ctx->index] = ctx->key;
		RefreshJoyButton(ctx->d, ctx->index);
	}
	gtk_window_destroy(GTK_WINDOW(ctx->window));
}

void
OnJoyCaptureCancel(GtkButton *button, gpointer user_data)
{
	(void)button;
	JoyCapture *ctx = (JoyCapture *)user_data;
	gtk_window_destroy(GTK_WINDOW(ctx->window));
}

void
OnJoyButtonClicked(GtkButton *button, gpointer user_data)
{
	ConfigDialog *d = (ConfigDialog *)user_data;
	const int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "joy-slot-index"));

	JoyCapture *ctx = g_new0(JoyCapture, 1);
	ctx->d = d;
	ctx->index = index;

	GtkWidget *window = gtk_window_new();
	ctx->window = window;
	gtk_window_set_title(GTK_WINDOW(window), "Configure Joystick Button");
	gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(d->window));
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(window), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(window), 320, 150);

	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_margin_start(box, 12);
	gtk_widget_set_margin_end(box, 12);
	gtk_widget_set_margin_top(box, 12);
	gtk_widget_set_margin_bottom(box, 12);
	gtk_window_set_child(GTK_WINDOW(window), box);

	gchar *prompt = g_strdup_printf("Press \"%s\" button ...", key_names[index]);
	ctx->label = gtk_label_new(prompt);
	g_free(prompt);
	gtk_label_set_wrap(GTK_LABEL(ctx->label), TRUE);
	gtk_box_append(GTK_BOX(box), ctx->label);

	GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_set_halign(buttons, GTK_ALIGN_END);
	gtk_box_append(GTK_BOX(box), buttons);

	GtkWidget *cancel = gtk_button_new_with_label("_Cancel");
	gtk_button_set_use_underline(GTK_BUTTON(cancel), TRUE);
	g_signal_connect(cancel, "clicked", G_CALLBACK(OnJoyCaptureCancel), ctx);
	gtk_box_append(GTK_BOX(buttons), cancel);

	ctx->ok = gtk_button_new_with_label("_OK");
	gtk_button_set_use_underline(GTK_BUTTON(ctx->ok), TRUE);
	gtk_widget_add_css_class(ctx->ok, "suggested-action");
	gtk_widget_set_sensitive(ctx->ok, FALSE);
	g_signal_connect(ctx->ok, "clicked", G_CALLBACK(OnJoyCaptureOk), ctx);
	gtk_box_append(GTK_BOX(buttons), ctx->ok);
	gtk_window_set_default_widget(GTK_WINDOW(window), ctx->ok);

	g_signal_connect(window, "destroy", G_CALLBACK(OnJoyCaptureDestroy), ctx);
	desmume_emu_set_joy_capture(TRUE);
	ctx->source = g_timeout_add(100, OnJoyCapturePoll, ctx);
	gtk_window_present(GTK_WINDOW(window));
}

/* ------------------------------------------------------------- pages */

GtkWidget *
BuildControlsPage(ConfigDialog *d)
{
	memcpy(d->controlKeys, keyboard_cfg, sizeof(d->controlKeys));
	memcpy(d->turboKeys, turbo_keys, sizeof(d->turboKeys));
	memcpy(d->joyKeys, joypad_cfg, sizeof(d->joyKeys));
	d->capturingSlot = -1;

	GtkWidget *page = NewPage();
	GtkWidget *notebook = gtk_notebook_new();
	gtk_box_append(GTK_BOX(page), notebook);

	/* Keyboard tab. */
	GtkWidget *keyboard = NewPage();
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), keyboard,
	                         gtk_label_new("Keyboard"));

	GtkWidget *grid = NULL;
	GtkWidget *section = NewSection("Buttons", &grid);
	gtk_box_append(GTK_BOX(keyboard), section);

	GtkWidget *hint = NewHint("Red means the key is used by another action. "
	                          "A button can be disabled using Escape.");
	gtk_box_append(GTK_BOX(keyboard), hint);

	for (int row = 0; row < 8; ++row)
	{
		for (int col = 0; col < 2; ++col)
		{
			const int key = kControlGrid[col][row];
			if (key < 0)
			{
				continue;
			}

			gchar *name = g_ascii_strup(key_names[key], -1);
			AttachKeySlot(d, GTK_GRID(grid), row, col * 2, name, hint,
			              KEY_SLOT_CONTROL, key, d->controlKeys[key],
			              (GdkModifierType)0);
			g_free(name);
		}
	}

	d->allowUpDown = gtk_check_button_new_with_label(
		"Allow Left+Right / Up+Down");
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->allowUpDown),
	                            d->config->allow_up_down.get() ? TRUE : FALSE);
	gtk_box_append(GTK_BOX(keyboard), d->allowUpDown);

	/* Joystick tab. */
	GtkWidget *joystick = NewPage();
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), joystick,
	                         gtk_label_new("Joystick"));

	grid = NULL;
	section = NewSection("Buttons", &grid);
	gtk_box_append(GTK_BOX(joystick), section);

	gtk_box_append(GTK_BOX(joystick),
	               NewHint("Click a button, then press the pad button or move "
	                       "the axis to bind it."));

	for (int row = 0; row < 8; ++row)
	{
		for (int col = 0; col < 2; ++col)
		{
			const int key = kControlGrid[col][row];
			if (key < 0)
			{
				continue;
			}

			gchar *name = g_ascii_strup(key_names[key], -1);
			GtkWidget *label = gtk_label_new(name);
			g_free(name);
			gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
			gtk_grid_attach(GTK_GRID(grid), label, col * 2, row, 1, 1);

			GtkWidget *button = gtk_button_new();
			gtk_widget_set_size_request(button, 150, -1);
			gtk_grid_attach(GTK_GRID(grid), button, col * 2 + 1, row, 1, 1);
			d->joyButtons[key] = button;
			g_object_set_data(G_OBJECT(button), "joy-slot-index",
			                  GINT_TO_POINTER(key));
			g_signal_connect(button, "clicked",
			                 G_CALLBACK(OnJoyButtonClicked), d);
			RefreshJoyButton(d, key);
		}
	}

	EnsureKeyFieldStyle();
	return page;
}

GtkWidget *
BuildHotkeysPage(ConfigDialog *d)
{
	GtkWidget *page = NewPage();

	GtkWidget *hint = NewHint("Click a shortcut and press a key. Press Escape "
	                          "to disable it. Assigning Tab replaces the Tab "
	                          "shortcut for the fast-forward toggle.");
	gtk_box_append(GTK_BOX(page), hint);

	GtkWidget *grid = NULL;
	GtkWidget *section = NewSection("Hotkeys", &grid);
	gtk_box_append(GTK_BOX(page), section);

	const int count = desmume_hotkey_count();
	const int rows = (count + 1) / 2;
	for (int i = 0; i < count; ++i)
	{
		const DesmumeHotkeyDef *def = desmume_hotkey_def(i);
		guint keyval = 0;
		GdkModifierType mods = (GdkModifierType)0;
		const char *accel = desmume_hotkey_accel(i);

		if ((accel != NULL) && (accel[0] != '\0'))
		{
			gtk_accelerator_parse(accel, &keyval, &mods);
			if (keyval == GDK_KEY_ISO_Left_Tab)
			{
				keyval = GDK_KEY_Tab;
			}
		}

		AttachKeySlot(d, GTK_GRID(grid), i % rows, (i / rows) * 2,
		              def->label, hint, KEY_SLOT_HOTKEY, i, keyval, mods);
	}

	grid = NULL;
	section = NewSection("Turbo Keys", &grid);
	gtk_box_append(GTK_BOX(page), section);

	for (int row = 0; row < 6; ++row)
	{
		for (int col = 0; col < 2; ++col)
		{
			const int key = kTurboGrid[col][row];
			gchar *name = g_ascii_strup(key_names[key], -1);
			AttachKeySlot(d, GTK_GRID(grid), row, col * 2, name, hint,
			              KEY_SLOT_TURBO, key, d->turboKeys[key],
			              (GdkModifierType)0);
			g_free(name);
		}
	}

	gtk_box_append(GTK_BOX(page),
	               NewHint("Holding a turbo key auto-fires that button, and it "
	                       "does not have to be the button's own key."));

	UpdateKeyConflicts(d);
	return page;
}

/* ------------------------------------------------------------------ browsing */

/* The picked path goes into an entry that may be destroyed while the file
 * chooser is open, so the callback holds its own reference. */
void
OnPathChosen(GObject *source, GAsyncResult *result, gpointer user_data)
{
	GtkWidget *entry = GTK_WIDGET(user_data);
	GError *error = NULL;

	GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);
	if (file == NULL)
	{
		g_clear_error(&error);
		g_object_unref(entry);
		return;
	}

	char *path = g_file_get_path(file);
	if (path != NULL)
	{
		gtk_editable_set_text(GTK_EDITABLE(entry), path);
		g_free(path);
	}
	g_object_unref(file);
	g_object_unref(entry);
}

void
OnFolderChosen(GObject *source, GAsyncResult *result, gpointer user_data)
{
	GtkWidget *entry = GTK_WIDGET(user_data);
	GError *error = NULL;

	GFile *file = gtk_file_dialog_select_folder_finish(GTK_FILE_DIALOG(source), result, &error);
	if (file == NULL)
	{
		g_clear_error(&error);
		g_object_unref(entry);
		return;
	}

	char *path = g_file_get_path(file);
	if (path != NULL)
	{
		gtk_editable_set_text(GTK_EDITABLE(entry), path);
		g_free(path);
	}
	g_object_unref(file);
	g_object_unref(entry);
}

void
StartBrowse(GtkWidget *entry, gboolean folder, const char *title, const char *pattern)
{
	GtkRoot *root = gtk_widget_get_root(entry);
	GtkWindow *parent = GTK_IS_WINDOW(root) ? GTK_WINDOW(root) : NULL;

	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, title);

	const char *current = gtk_editable_get_text(GTK_EDITABLE(entry));
	if ((current != NULL) && g_file_test(current, G_FILE_TEST_IS_DIR))
	{
		GFile *folder_file = g_file_new_for_path(current);
		gtk_file_dialog_set_initial_folder(dialog, folder_file);
		g_object_unref(folder_file);
	}

	if (pattern != NULL)
	{
		GtkFileFilter *filter = gtk_file_filter_new();
		gtk_file_filter_set_name(filter, "Supported files");
		gtk_file_filter_add_pattern(filter, pattern);

		GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
		g_list_store_append(filters, filter);
		gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
		g_object_unref(filters);
		g_object_unref(filter);
	}

	g_object_ref(entry);
	if (folder)
	{
		gtk_file_dialog_select_folder(dialog, parent, NULL, OnFolderChosen, entry);
	}
	else
	{
		gtk_file_dialog_open(dialog, parent, NULL, OnPathChosen, entry);
	}
	g_object_unref(dialog);
}

void
OnBrowseFolderClicked(GtkButton *button, gpointer user_data)
{
	(void)button;
	StartBrowse(GTK_WIDGET(user_data), TRUE, "Select folder", NULL);
}

void
OnBrowseFileClicked(GtkButton *button, gpointer user_data)
{
	(void)button;
	/* The pattern and title are attached to the button so one callback serves
	 * every "pick a file" row. */
	StartBrowse(GTK_WIDGET(user_data), FALSE,
	            (const char *)g_object_get_data(G_OBJECT(button), "title"),
	            (const char *)g_object_get_data(G_OBJECT(button), "pattern"));
}

/* One entry + "Browse…" button in a grid row; returns the entry. */
GtkWidget *
AddPathRow(GtkGrid *grid, int row, const char *text, const char *value,
           gboolean folder, const char *title, const char *pattern,
           GtkWidget **browseOut)
{
	GtkWidget *entry = gtk_entry_new();
	gtk_editable_set_text(GTK_EDITABLE(entry), (value != NULL) ? value : "");
	gtk_widget_set_hexpand(entry, TRUE);

	GtkWidget *browse = gtk_button_new_with_label("Browse…");
	g_object_set_data(G_OBJECT(browse), "pattern", (gpointer)pattern);
	g_object_set_data(G_OBJECT(browse), "title", (gpointer)title);

	if (folder)
	{
		g_signal_connect(browse, "clicked", G_CALLBACK(OnBrowseFolderClicked), entry);
	}
	else
	{
		g_signal_connect(browse, "clicked", G_CALLBACK(OnBrowseFileClicked), entry);
	}

	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_append(GTK_BOX(box), entry);
	gtk_box_append(GTK_BOX(box), browse);

	AddRow(grid, row, text, box);

	if (browseOut != NULL)
	{
		*browseOut = browse;
	}
	return entry;
}

/* ------------------------------------------------------------------- alerts */

void
ShowAlert(GtkWindow *parent, const char *message)
{
	GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", message);
	static const char *buttons[] = { "OK", NULL };

	gtk_alert_dialog_set_buttons(dialog, buttons);
	gtk_alert_dialog_choose(dialog, parent, NULL, NULL, NULL);
	g_object_unref(dialog);
}

/* ------------------------------------------------------------- Emulation tab */

void
SyncEmulationPage(ConfigDialog *d)
{
	const gboolean use_bios =
		gtk_check_button_get_active(GTK_CHECK_BUTTON(d->useExtBios));
	const gboolean use_firmware =
		gtk_check_button_get_active(GTK_CHECK_BUTTON(d->useExtFirmware));

	gtk_widget_set_sensitive(d->bios9Entry, use_bios);
	gtk_widget_set_sensitive(d->bios9Browse, use_bios);
	gtk_widget_set_sensitive(d->bios7Entry, use_bios);
	gtk_widget_set_sensitive(d->bios7Browse, use_bios);
	gtk_widget_set_sensitive(d->swiFromBios, use_bios);
	gtk_widget_set_sensitive(d->patchSwi3, use_bios);

	gtk_widget_set_sensitive(d->firmwareEntry, use_firmware);
	gtk_widget_set_sensitive(d->firmwareBrowse, use_firmware);
	gtk_widget_set_sensitive(d->useExtFirmwareSettings, use_firmware);

	/* Booting from firmware needs both images, exactly like the Windows
	 * dialog: the BIOS hands over to the firmware at reset. */
	gtk_widget_set_sensitive(d->bootFromFirmware, use_bios && use_firmware);
}

void
OnEmulationToggled(GtkCheckButton *button, gpointer user_data)
{
	(void)button;
	SyncEmulationPage((ConfigDialog *)user_data);
}

GtkWidget *
BuildEmulationTab(ConfigDialog *d)
{
	GtkWidget *box = NewPage();
	GtkWidget *grid = NULL;
	GtkWidget *section;

	/* General: read while the core boots. */
	section = NewSection("General", &grid);
	gtk_box_append(GTK_BOX(box), section);

	d->advancedTiming = gtk_check_button_new_with_label(
		"Enable Advanced Bus-Level Timing");
	gtk_grid_attach(GTK_GRID(grid), d->advancedTiming, 0, 0, 2, 1);
	gtk_grid_attach(GTK_GRID(grid),
	                NewHint("Improves or fixes some games, but is very costly. Disable it "
	                        "if you want more speed, but not when TASing. Especially helpful "
	                        "if ARM9 runs at ~99%."),
	                0, 1, 2, 1);

	d->gameHacks = gtk_check_button_new_with_label("Enable Game Hacks");
	gtk_grid_attach(GTK_GRID(grid), d->gameHacks, 0, 2, 2, 1);
	gtk_grid_attach(GTK_GRID(grid),
	                NewHint("Enables game-specific workarounds for buggy games."),
	                0, 3, 2, 1);

	/* CPU emulation mode. */
	section = NewSection("CPU emulation mode", &grid);
	gtk_box_append(GTK_BOX(box), section);

	d->useJit = gtk_check_button_new_with_label("Use dynamic recompiler");
	gtk_grid_attach(GTK_GRID(grid), d->useJit, 0, 0, 2, 1);

	GridLabel(GTK_GRID(grid), "Block size (1 = accuracy, 100 = fastest):", 1);
	d->jitBlockSize = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
	                                           1, 100, 1);
	gtk_scale_set_draw_value(GTK_SCALE(d->jitBlockSize), TRUE);
	gtk_scale_set_value_pos(GTK_SCALE(d->jitBlockSize), GTK_POS_RIGHT);
	gtk_widget_set_hexpand(d->jitBlockSize, TRUE);
	gtk_widget_set_size_request(d->jitBlockSize, 240, -1);
	gtk_grid_attach(GTK_GRID(grid), d->jitBlockSize, 0, 2, 2, 1);

	gtk_widget_set_tooltip_text(d->useJit,
		"Enabling the recompiler can give a 0-50% speedup, but it may still "
		"contain small bugs and is not assumed to be deterministic.");
	gtk_widget_set_tooltip_text(d->jitBlockSize,
		"Tune this if a game breaks with the recompiler enabled. "
		"1 favours accuracy, 100 favours speed.");

#ifndef HAVE_JIT
	gtk_widget_set_sensitive(d->useJit, FALSE);
	gtk_widget_set_sensitive(d->jitBlockSize, FALSE);
#endif

	/* Debug options. */
	section = NewSection("Debug", &grid);
	gtk_box_append(GTK_BOX(box), section);

	d->debugConsole = gtk_check_button_new_with_label(
		"Use 8MB debugger mode when starting execution");
	gtk_grid_attach(GTK_GRID(grid), d->debugConsole, 0, 0, 2, 1);

	d->ensata = gtk_check_button_new_with_label("Emulate Ensata");
	gtk_grid_attach(GTK_GRID(grid), d->ensata, 0, 1, 2, 1);

	/* BIOS. */
	section = NewSection("BIOS", &grid);
	gtk_box_append(GTK_BOX(box), section);

	d->useExtBios = gtk_check_button_new_with_label("Use external BIOS images");
	gtk_grid_attach(GTK_GRID(grid), d->useExtBios, 0, 0, 2, 1);

	d->bios9Entry = AddPathRow(GTK_GRID(grid), 1, "ARM9 BIOS image:",
	                           "", FALSE, "Select the ARM9 BIOS image",
	                           "*.bin", &d->bios9Browse);
	d->bios7Entry = AddPathRow(GTK_GRID(grid), 2, "ARM7 BIOS image:",
	                           "", FALSE, "Select the ARM7 BIOS image",
	                           "*.bin", &d->bios7Browse);

	d->swiFromBios = gtk_check_button_new_with_label(
		"Emulate SWIs with BIOS images (otherwise, HLE)");
	gtk_grid_attach(GTK_GRID(grid), d->swiFromBios, 0, 3, 2, 1);

	d->patchSwi3 = gtk_check_button_new_with_label(
		"Patch DelayLoop SWI (speed hack)");
	gtk_grid_attach(GTK_GRID(grid), d->patchSwi3, 0, 4, 2, 1);

	/* Firmware image. */
	section = NewSection("Firmware image", &grid);
	gtk_box_append(GTK_BOX(box), section);

	d->useExtFirmware = gtk_check_button_new_with_label("Use external firmware image");
	gtk_grid_attach(GTK_GRID(grid), d->useExtFirmware, 0, 0, 2, 1);

	d->firmwareEntry = AddPathRow(GTK_GRID(grid), 1, "Firmware image:",
	                              "", FALSE, "Select the firmware image",
	                              "*.bin", &d->firmwareBrowse);

	d->bootFromFirmware = gtk_check_button_new_with_label(
		"Boot from firmware (like the NDS)");
	gtk_grid_attach(GTK_GRID(grid), d->bootFromFirmware, 0, 2, 2, 1);

	d->useExtFirmwareSettings = gtk_check_button_new_with_label(
		"Load User Settings from file");
	gtk_grid_attach(GTK_GRID(grid), d->useExtFirmwareSettings, 0, 3, 2, 1);

	gtk_box_append(GTK_BOX(box),
	               NewHint("BIOS, firmware image and timing are read while the ROM boots; "
	                       "changing them offers a reset on OK. The user name and message "
	                       "live on the Firmware page."));

	g_signal_connect(d->useExtBios, "toggled", G_CALLBACK(OnEmulationToggled), d);
	g_signal_connect(d->useExtFirmware, "toggled", G_CALLBACK(OnEmulationToggled), d);

	return box;
}

/* -------------------------------------------------------------- Firmware page */

GtkWidget *
BuildFirmwarePage(ConfigDialog *d)
{
	GtkWidget *box = NewPage();
	GtkWidget *grid = NULL;
	GtkWidget *section;

	section = NewSection("User settings written into the internal firmware image", &grid);
	gtk_box_append(GTK_BOX(box), section);

	d->fwNickname = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(d->fwNickname), 10);
	AddRow(GTK_GRID(grid), 0, "Nickname:", d->fwNickname);

	d->fwMessage = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(d->fwMessage), 26);
	AddRow(GTK_GRID(grid), 1, "Message:", d->fwMessage);

	{
		GtkStringList *colors = NewStringList();
		for (guint i = 0; i < G_N_ELEMENTS(kFirmwareColors); ++i)
		{
			gtk_string_list_append(colors, kFirmwareColors[i]);
		}
		d->fwColorDrop = gtk_drop_down_new(G_LIST_MODEL(colors), NULL);
		AddRow(GTK_GRID(grid), 2, "Favourite colour:", d->fwColorDrop);
	}

	{
		GtkStringList *months = NewStringList();
		for (guint i = 0; i < G_N_ELEMENTS(kMonthNames); ++i)
		{
			gtk_string_list_append(months, kMonthNames[i]);
		}
		d->fwMonthDrop = gtk_drop_down_new(G_LIST_MODEL(months), NULL);
		AddRow(GTK_GRID(grid), 3, "Birth month:", d->fwMonthDrop);
	}

	{
		GtkStringList *days = NewStringList();
		for (int day = 1; day <= 31; ++day)
		{
			char text[4];
			g_snprintf(text, sizeof(text), "%d", day);
			gtk_string_list_append(days, text);
		}
		d->fwDayDrop = gtk_drop_down_new(G_LIST_MODEL(days), NULL);
		AddRow(GTK_GRID(grid), 4, "Birth day:", d->fwDayDrop);
	}

	{
		GtkStringList *languages = NewStringList();
		for (guint i = 0; i < G_N_ELEMENTS(kLanguageNames); ++i)
		{
			gtk_string_list_append(languages, kLanguageNames[i]);
		}
		d->fwLanguageDrop = gtk_drop_down_new(G_LIST_MODEL(languages), NULL);
		AddRow(GTK_GRID(grid), 5, "Language:", d->fwLanguageDrop);
	}

	d->fwMac = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(d->fwMac), 12);
	gtk_entry_set_placeholder_text(GTK_ENTRY(d->fwMac), "12 hex digits");
	gtk_widget_set_tooltip_text(d->fwMac,
		"MAC address of the emulated console. Colons and dashes are ignored.");
	AddRow(GTK_GRID(grid), 6, "MAC address:", d->fwMac);

	gtk_box_append(GTK_BOX(box),
	               NewHint("The language is shared with Config/FirmwareLanguage, so the "
	                       "--lang command line option and this page agree. These values "
	                       "are written when the firmware boots, so a reset is offered "
	                       "after a change."));

	return box;
}

/* --------------------------------------------------------------- Graphic tab */

/* A GPU filter only runs on the WebGPU path; the software row has no GPU
 * device to hang a filter on, so mirror GTK3 and grey the filter out there. */
void
OnPresentationChanged(GtkDropDown *drop, GParamSpec *pspec, gpointer user_data)
{
	(void)pspec;
	ConfigDialog *d = (ConfigDialog *)user_data;
	const guint row = gtk_drop_down_get_selected(drop);
	const gboolean software = (row >= (guint)d->presentationCount) ||
		(d->presentationMasks[row] == PRESENTATION_SOFTWARE);
	gtk_widget_set_sensitive(d->filterDrop, !software);
}

GtkWidget *
BuildGraphicTab(ConfigDialog *d)
{
	GtkWidget *box = NewPage();
	GtkWidget *grid = NULL;
	GtkWidget *section;

	/* 3D rendering engine. */
	section = NewSection("3D rendering engine", &grid);
	gtk_box_append(GTK_BOX(box), section);

	GridLabel(GTK_GRID(grid), "3D core:", 0);
	{
		GtkStringList *renderers = NewStringList();
		const int count = desmume_emu_renderer_count();
		for (int i = 0; i < count; ++i)
		{
			const char *name = desmume_emu_renderer_name(i);
			gtk_string_list_append(renderers, (name != NULL) ? name : "Unknown");
		}
		d->rendererDrop = gtk_drop_down_new(G_LIST_MODEL(renderers), NULL);
		gtk_widget_set_hexpand(d->rendererDrop, TRUE);
	}
	gtk_grid_attach(GTK_GRID(grid), d->rendererDrop, 1, 0, 1, 1);

	GridLabel(GTK_GRID(grid), "Output color depth:", 1);
	d->colorDepthDrop = NewDropDown(kColorDepths);
	gtk_widget_set_tooltip_text(d->colorDepthDrop,
		"Color depth the 3D renderer outputs. 18 bit is the DS-native precision.");
	gtk_grid_attach(GTK_GRID(grid), d->colorDepthDrop, 1, 1, 1, 1);

	GridLabel(GTK_GRID(grid), "Multisampling:", 2);
	{
		GtkStringList *samples = NewStringList();
		for (guint i = 0; i < G_N_ELEMENTS(kMultisampleLabels); ++i)
		{
			gtk_string_list_append(samples, kMultisampleLabels[i]);
		}
		d->multisampleDrop = gtk_drop_down_new(G_LIST_MODEL(samples), NULL);
		gtk_widget_set_hexpand(d->multisampleDrop, TRUE);
	}
	gtk_widget_set_tooltip_text(d->multisampleDrop,
		"Antialiasing for the OpenGL and WebGPU renderers; ignored by the "
		"software rasterizer.");
	gtk_grid_attach(GTK_GRID(grid), d->multisampleDrop, 1, 2, 1, 1);

	/* SoftRasterizer options. */
	section = NewSection("SoftRasterizer options", &grid);
	gtk_box_append(GTK_BOX(box), section);

	d->highColorInterpolation = gtk_check_button_new_with_label(
		"High-precision color interpolation");
	gtk_grid_attach(GTK_GRID(grid), d->highColorInterpolation, 0, 0, 2, 1);

	d->txtHack = gtk_check_button_new_with_label(
		"Texture hack (use zero as transparent)");
	gtk_grid_attach(GTK_GRID(grid), d->txtHack, 0, 1, 2, 1);

	d->lineHack = gtk_check_button_new_with_label(
		"Line hack (render lines as 1-pixel polygons)");
	gtk_grid_attach(GTK_GRID(grid), d->lineHack, 0, 2, 2, 1);
	gtk_widget_set_tooltip_text(d->lineHack,
		"Works around the DS rasterizer's line drawing; leave it on unless a "
		"game draws stray lines.");

	/* General 3D options, read by every renderer each frame. */
	section = NewSection("General 3D options", &grid);
	gtk_box_append(GTK_BOX(box), section);

	d->edgeMark = gtk_check_button_new_with_label("Enable edge marking");
	gtk_grid_attach(GTK_GRID(grid), d->edgeMark, 0, 0, 2, 1);

	d->fog = gtk_check_button_new_with_label("Enable fog");
	gtk_grid_attach(GTK_GRID(grid), d->fog, 0, 1, 2, 1);

	d->textures = gtk_check_button_new_with_label("Enable textures");
	gtk_grid_attach(GTK_GRID(grid), d->textures, 0, 2, 2, 1);
	gtk_widget_set_tooltip_text(d->textures,
		"Turning textures off shows untextured polygons; useful to isolate "
		"texture bugs.");

	/* Rendering accuracy. */
	section = NewSection("Rendering accuracy", &grid);
	gtk_box_append(GTK_BOX(box), section);

	d->shadowPolygon = gtk_check_button_new_with_label("Render shadow polygons");
	gtk_grid_attach(GTK_GRID(grid), d->shadowPolygon, 0, 0, 2, 1);

	d->zeroAlpha = gtk_check_button_new_with_label(
		"Special zero-alpha blending");
	gtk_grid_attach(GTK_GRID(grid), d->zeroAlpha, 0, 1, 2, 1);

	d->ndsDepth = gtk_check_button_new_with_label("NDS depth calculation");
	gtk_grid_attach(GTK_GRID(grid), d->ndsDepth, 0, 2, 2, 1);

	d->depthLEqual = gtk_check_button_new_with_label(
		"Depth test: L-equal polygon facing");
	gtk_grid_attach(GTK_GRID(grid), d->depthLEqual, 0, 3, 2, 1);

	gtk_grid_attach(GTK_GRID(grid),
	                NewHint("The OpenGL renderers bake these into their shaders, so "
	                        "changing one rebuilds the active 3D core. The software and "
	                        "WebGPU renderers read them every frame."),
	                0, 4, 2, 1);

	/* Texture options shared by the software and filtered paths. */
	section = NewSection("Texture options", &grid);
	gtk_box_append(GTK_BOX(box), section);

	GridLabel(GTK_GRID(grid), "Texture scaling:", 0);
	static const char *const textureScales[] = { "×1", "×2", "×4", NULL };
	d->textureScaleDrop = NewDropDown(textureScales);
	gtk_grid_attach(GTK_GRID(grid), d->textureScaleDrop, 1, 0, 1, 1);

	GridLabel(GTK_GRID(grid), "GPU scale factor:", 1);
	d->gpuScaleSpin = gtk_spin_button_new_with_range(1.0, 4.0, 0.5);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(d->gpuScaleSpin), 1);
	gtk_widget_set_halign(d->gpuScaleSpin, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), d->gpuScaleSpin, 1, 1, 1, 1);

	d->deposterize = gtk_check_button_new_with_label("Deposterize textures");
	gtk_grid_attach(GTK_GRID(grid), d->deposterize, 0, 2, 2, 1);

	d->smoothing = gtk_check_button_new_with_label("Smooth textures");
	gtk_grid_attach(GTK_GRID(grid), d->smoothing, 0, 3, 2, 1);

	/* WebGPU presentation options.  Like GTK3's "Screen presentation" combo,
	 * this selects the rendering path: one row per WebGPU backend plus a
	 * software (plain GdkTexture) row.  GPU filters only apply on a WebGPU
	 * backend, so the filter row is disabled for the software path. */
	section = NewSection("WebGPU presentation", &grid);
	gtk_box_append(GTK_BOX(box), section);

	GridLabel(GTK_GRID(grid), "Presentation:", 0);
	{
		GtkStringList *presentations = NewStringList();
		const int backendCount = desmume_emu_presentation_backend_count();
		d->presentationMasks = g_new(guint32, backendCount + 1);
		d->presentationCount = 0;

		for (int i = 0; i < backendCount; ++i)
		{
			const char *label = desmume_emu_presentation_backend_label(i);
			char *text = g_strdup_printf("WebGPU - %s", (label != NULL) ? label : "WebGPU");
			gtk_string_list_append(presentations, text);
			g_free(text);
			d->presentationMasks[d->presentationCount++] =
				desmume_emu_presentation_backend_mask(i);
		}

		/* Always offer the plain path, even on a build without WebGPU. */
		gtk_string_list_append(presentations, "Software (Cairo)");
		d->presentationMasks[d->presentationCount++] = PRESENTATION_SOFTWARE;

		d->presentationDrop = gtk_drop_down_new(G_LIST_MODEL(presentations), NULL);
		gtk_widget_set_hexpand(d->presentationDrop, TRUE);
	}
	gtk_grid_attach(GTK_GRID(grid), d->presentationDrop, 1, 0, 1, 1);
	g_signal_connect(d->presentationDrop, "notify::selected",
	                 G_CALLBACK(OnPresentationChanged), d);

	GridLabel(GTK_GRID(grid), "GPU filter:", 1);
	{
		GtkStringList *filters = NewStringList();
		d->filterValues = g_new(int, WGPUPresentUpscalerCount);
		d->filterCount = 0;

		gtk_string_list_append(filters, "Off");
		d->filterValues[d->filterCount++] = WGPUPresentUpscaler_Off;

		for (int value = WGPUPresentUpscaler_Off + 1;
		     value < WGPUPresentUpscalerCount; ++value)
		{
			const char *name = WGPUPresent::upscalerName(value);
			if (name == NULL)
			{
				continue;
			}
			gtk_string_list_append(filters, name);
			d->filterValues[d->filterCount++] = value;
		}

		d->filterDrop = gtk_drop_down_new(G_LIST_MODEL(filters), NULL);
		gtk_widget_set_hexpand(d->filterDrop, TRUE);
	}
	gtk_grid_attach(GTK_GRID(grid), d->filterDrop, 1, 1, 1, 1);

	return box;
}

/* ---------------------------------------------------------------- Sound tab */

void
UpdateSyncSensitivity(ConfigDialog *d)
{
	const gboolean synchronous =
		gtk_check_button_get_active(GTK_CHECK_BUTTON(d->syncSync));

	gtk_widget_set_sensitive(d->syncN, synchronous);
	gtk_widget_set_sensitive(d->syncZ, synchronous);
	if (d->syncP != NULL)
	{
		gtk_widget_set_sensitive(d->syncP, synchronous);
	}
}

void
OnSyncModeToggled(GtkCheckButton *button, gpointer user_data)
{
	(void)button;
	UpdateSyncSensitivity((ConfigDialog *)user_data);
}

GtkWidget *
BuildSoundTab(ConfigDialog *d)
{
	GtkWidget *box = NewPage();
	GtkWidget *grid = NULL;
	GtkWidget *section;

	/* Output. */
	section = NewSection("Audio output", &grid);
	gtk_box_append(GTK_BOX(box), section);

	d->audioEnabled = gtk_check_button_new_with_label("Enable audio");
	gtk_grid_attach(GTK_GRID(grid), d->audioEnabled, 0, 0, 2, 1);

	GridLabel(GTK_GRID(grid), "Volume:", 1);
	d->audioVolume = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
	                                          0, 100, 1);
	gtk_scale_set_draw_value(GTK_SCALE(d->audioVolume), TRUE);
	gtk_scale_set_value_pos(GTK_SCALE(d->audioVolume), GTK_POS_RIGHT);
	gtk_widget_set_hexpand(d->audioVolume, TRUE);
	gtk_widget_set_size_request(d->audioVolume, 240, -1);
	gtk_grid_attach(GTK_GRID(grid), d->audioVolume, 1, 1, 1, 1);

	/* Synchronization mode, laid out like the Windows Sound Settings box. */
	section = NewSection("Synchronization mode", &grid);
	gtk_box_append(GTK_BOX(box), section);

	d->syncDual = gtk_check_button_new_with_label(
		"Dual SPU Synch/Async (traditional)");
	d->syncSync = gtk_check_button_new_with_label(
		"Synchronous (sometimes needed for streams)");
	gtk_check_button_set_group(GTK_CHECK_BUTTON(d->syncSync),
	                           GTK_CHECK_BUTTON(d->syncDual));
	gtk_grid_attach(GTK_GRID(grid), d->syncDual, 0, 0, 2, 1);
	gtk_grid_attach(GTK_GRID(grid), d->syncSync, 0, 1, 2, 1);
	g_signal_connect(d->syncSync, "toggled",
	                 G_CALLBACK(OnSyncModeToggled), d);

	gtk_widget_set_tooltip_text(d->syncSync,
		"Synchronous is more accurate; dual mode may sound better when running "
		"far from 60 fps, but can clip the ends of sound effects.");

	/* Synchronizer method. */
	section = NewSection("Synchronizer method", &grid);
	gtk_box_append(GTK_BOX(box), section);

	d->syncN = gtk_check_button_new_with_label(
		"N: Crossfade resampler (low CPU)");
	d->syncZ = gtk_check_button_new_with_label(
		"Z: Micro-resampler (low latency)");
	gtk_check_button_set_group(GTK_CHECK_BUTTON(d->syncZ),
	                           GTK_CHECK_BUTTON(d->syncN));
	gtk_grid_attach(GTK_GRID(grid), d->syncN, 0, 0, 2, 1);
	gtk_grid_attach(GTK_GRID(grid), d->syncZ, 0, 1, 2, 1);

#ifdef HAVE_LIBSOUNDTOUCH
	d->syncP = gtk_check_button_new_with_label(
		"P: Time-stretch (keeps pitch)");
	gtk_check_button_set_group(GTK_CHECK_BUTTON(d->syncP),
	                           GTK_CHECK_BUTTON(d->syncN));
	gtk_grid_attach(GTK_GRID(grid), d->syncP, 0, 2, 2, 1);
#else
	d->syncP = NULL;
#endif

	/* Interpolation. */
	section = NewSection("Interpolation", &grid);
	gtk_box_append(GTK_BOX(box), section);

	GridLabel(GTK_GRID(grid), "Interpolation:", 0);
	static const char *const interpolations[] =
		{ "None", "Linear", "Cosine", NULL };
	d->interpolationDrop = NewDropDown(interpolations);
	gtk_grid_attach(GTK_GRID(grid), d->interpolationDrop, 1, 0, 1, 1);

	return box;
}

/* ---------------------------------------------------------------- Slots tab */

void
SyncSlot1Page(ConfigDialog *d)
{
	const guint row = GetDropDown(d->slot1Drop);
	const gboolean has_fat = desmume_emu_slot1_has_fat((int)row);
	const gboolean has_debug = desmume_emu_slot1_has_debug((int)row);
	const gboolean use_rom_dir =
		gtk_check_button_get_active(GTK_CHECK_BUTTON(d->slot1UseRomDir));

	gtk_widget_set_sensitive(d->slot1FatBox, has_fat && !use_rom_dir);
	gtk_widget_set_sensitive(d->slot1UseRomDir, has_fat);
	gtk_widget_set_sensitive(d->slot1DebugBox, has_debug);
}

void
SyncSlot2Page(ConfigDialog *d)
{
	const guint row = GetDropDown(d->slot2Drop);
	const gboolean has_cflash = desmume_emu_slot2_has_cflash((int)row);
	const guint mode = GetDropDown(d->slot2CFlashDrop);

	gtk_widget_set_sensitive(d->slot2GbaBox,
	                         desmume_emu_slot2_has_gba_rom((int)row));
	gtk_widget_set_sensitive(d->slot2CFlashDrop, has_cflash);
	/* "ROM directory" reuses the loaded ROM's folder, so there is nothing
	 * to type in that mode. */
	gtk_widget_set_sensitive(d->slot2CFlashBox, has_cflash && (mode != 2));
}

void
OnSlot1Changed(GtkDropDown *drop, GParamSpec *pspec, gpointer user_data)
{
	(void)pspec;
	ConfigDialog *d = (ConfigDialog *)user_data;
	const guint row = gtk_drop_down_get_selected(drop);
	const char *description = desmume_emu_slot1_description((int)row);
	gtk_label_set_text(GTK_LABEL(d->slot1Description),
	                   (description != NULL) ? description : "");
	SyncSlot1Page(d);
}

void
OnSlot2Changed(GtkDropDown *drop, GParamSpec *pspec, gpointer user_data)
{
	(void)pspec;
	ConfigDialog *d = (ConfigDialog *)user_data;
	const guint row = gtk_drop_down_get_selected(drop);
	const char *description = desmume_emu_slot2_description((int)row);
	gtk_label_set_text(GTK_LABEL(d->slot2Description),
	                   (description != NULL) ? description : "");
	SyncSlot2Page(d);
}

void
OnSlot1RomDirToggled(GtkCheckButton *button, gpointer user_data)
{
	(void)button;
	SyncSlot1Page((ConfigDialog *)user_data);
}

void
OnSlot2CFlashModeChanged(GtkDropDown *drop, GParamSpec *pspec, gpointer user_data)
{
	(void)drop;
	(void)pspec;
	SyncSlot2Page((ConfigDialog *)user_data);
}

GtkWidget *
BuildSlotsPage(ConfigDialog *d)
{
	GtkWidget *box = NewPage();
	GtkWidget *grid = NULL;
	GtkWidget *section;

	/* Slot 1 (DS card slot). */
	section = NewSection("Slot 1 device", &grid);
	gtk_box_append(GTK_BOX(box), section);

	GridLabel(GTK_GRID(grid), "Device:", 0);
	{
		GtkStringList *devices = NewStringList();
		const int count = desmume_emu_slot1_count();
		for (int i = 0; i < count; ++i)
		{
			const char *name = desmume_emu_slot1_name(i);
			gtk_string_list_append(devices, (name != NULL) ? name : "Unknown");
		}
		d->slot1Drop = gtk_drop_down_new(G_LIST_MODEL(devices), NULL);
		gtk_widget_set_hexpand(d->slot1Drop, TRUE);
	}
	gtk_grid_attach(GTK_GRID(grid), d->slot1Drop, 1, 0, 1, 1);

	d->slot1Description = NewHint("");
	gtk_grid_attach(GTK_GRID(grid), d->slot1Description, 0, 1, 2, 1);

	d->slot1FatEntry = AddPathRow(GTK_GRID(grid), 2, "R4 FAT directory:",
	                              "", TRUE, "Select the R4 FAT directory",
	                              NULL, NULL);
	d->slot1FatBox = gtk_widget_get_parent(d->slot1FatEntry);

	d->slot1UseRomDir = gtk_check_button_new_with_label(
		"Use the loaded ROM's directory");
	gtk_grid_attach(GTK_GRID(grid), d->slot1UseRomDir, 1, 3, 1, 1);

	d->slot1DebugEntry = AddPathRow(GTK_GRID(grid), 4, "Debug filesystem:",
	                                "", TRUE,
	                                "Select the Slot 1 debug filesystem directory",
	                                NULL, NULL);
	d->slot1DebugBox = gtk_widget_get_parent(d->slot1DebugEntry);

	/* Slot 2 (GBA slot). */
	section = NewSection("Slot 2 device", &grid);
	gtk_box_append(GTK_BOX(box), section);

	GridLabel(GTK_GRID(grid), "Device:", 0);
	{
		GtkStringList *devices = NewStringList();
		const int count = desmume_emu_slot2_count();
		for (int i = 0; i < count; ++i)
		{
			const char *name = desmume_emu_slot2_name(i);
			gtk_string_list_append(devices, (name != NULL) ? name : "Unknown");
		}
		d->slot2Drop = gtk_drop_down_new(G_LIST_MODEL(devices), NULL);
		gtk_widget_set_hexpand(d->slot2Drop, TRUE);
	}
	gtk_grid_attach(GTK_GRID(grid), d->slot2Drop, 1, 0, 1, 1);

	d->slot2Description = NewHint("");
	gtk_grid_attach(GTK_GRID(grid), d->slot2Description, 0, 1, 2, 1);

	GridLabel(GTK_GRID(grid), "CompactFlash mode:", 2);
	d->slot2CFlashDrop = NewDropDown(kCFlashModes);
	gtk_grid_attach(GTK_GRID(grid), d->slot2CFlashDrop, 1, 2, 1, 1);

	d->slot2CFlashEntry = AddPathRow(GTK_GRID(grid), 3, "CompactFlash path:",
	                                 "", TRUE, "Select the CompactFlash directory",
	                                 NULL, NULL);
	d->slot2CFlashBox = gtk_widget_get_parent(d->slot2CFlashEntry);

	d->slot2GbaEntry = AddPathRow(GTK_GRID(grid), 4, "GBA ROM:",
	                              "", FALSE, "Select the GBA ROM",
	                              "*.gba", NULL);
	d->slot2GbaBox = gtk_widget_get_parent(d->slot2GbaEntry);

	gtk_box_append(GTK_BOX(box),
	               NewHint("The devices are connected when a ROM starts, so a change "
	                       "offers a reset on OK. The GBA save file is kept next to the "
	                       "ROM with the same base name."));

	g_signal_connect(d->slot1Drop, "notify::selected",
	                 G_CALLBACK(OnSlot1Changed), d);
	g_signal_connect(d->slot1UseRomDir, "toggled",
	                 G_CALLBACK(OnSlot1RomDirToggled), d);
	g_signal_connect(d->slot2Drop, "notify::selected",
	                 G_CALLBACK(OnSlot2Changed), d);
	g_signal_connect(d->slot2CFlashDrop, "notify::selected",
	                 G_CALLBACK(OnSlot2CFlashModeChanged), d);

	return box;
}

/* ---------------------------------------------------------------- Wi-Fi page */

void
OnWifiToggled(GtkCheckButton *button, gpointer user_data)
{
	ConfigDialog *d = (ConfigDialog *)user_data;
	const gboolean enabled = gtk_check_button_get_active(button);
	gtk_widget_set_sensitive(d->wifiCompatibility, enabled);
	gtk_widget_set_sensitive(d->wifiAdapterDrop, enabled);
}

GtkWidget *
BuildWifiPage(ConfigDialog *d)
{
	GtkWidget *box = NewPage();
	GtkWidget *grid = NULL;
	GtkWidget *section;

	section = NewSection("Wi-Fi emulation", &grid);
	gtk_box_append(GTK_BOX(box), section);

	d->wifiEnabled = gtk_check_button_new_with_label("Enable Wi-Fi emulation");
	gtk_grid_attach(GTK_GRID(grid), d->wifiEnabled, 0, 0, 2, 1);

	d->wifiCompatibility = gtk_check_button_new_with_label("Compatibility mode");
	gtk_widget_set_tooltip_text(d->wifiCompatibility,
		"Uses the slower, more forgiving timing; needed by a few games and by "
		"most homebrew that talks to a bridge.");
	gtk_grid_attach(GTK_GRID(grid), d->wifiCompatibility, 0, 1, 2, 1);

	GridLabel(GTK_GRID(grid), "Bridge adapter:", 2);
	{
		GtkStringList *adapters = NewStringList();
		char *error = NULL;
		const int count = desmume_emu_wifi_bridge_count(&error);

		if (count <= 0)
		{
			gtk_string_list_append(adapters, (error != NULL)
				? error : "No bridge device available.");
			g_free(error);
		}
		else
		{
			for (int i = 0; i < count; ++i)
			{
				const char *name = desmume_emu_wifi_bridge_name(i);
				gtk_string_list_append(adapters, (name != NULL) ? name : "Unknown");
			}
		}

		d->wifiAdapterDrop = gtk_drop_down_new(G_LIST_MODEL(adapters), NULL);
		gtk_widget_set_hexpand(d->wifiAdapterDrop, TRUE);
		gtk_widget_set_sensitive(d->wifiAdapterDrop, count > 0);
	}
	gtk_grid_attach(GTK_GRID(grid), d->wifiAdapterDrop, 1, 2, 1, 1);

	d->wifiHint = NewHint(
		"Wi-Fi emulation needs libpcap and root (or CAP_NET_RAW) to open the "
		"bridge device. The adapter list is filled once the emulator core is "
		"running, so open this page after loading a ROM to pick one.");
	gtk_box_append(GTK_BOX(box), d->wifiHint);

	g_signal_connect(d->wifiEnabled, "toggled", G_CALLBACK(OnWifiToggled), d);

	return box;
}

/* ---------------------------------------------------------- Microphone page */

void
SyncMicPage(ConfigDialog *d)
{
	const guint mode = GetDropDown(d->micModeDrop);
	gtk_widget_set_sensitive(d->micSampleBox, mode == 1);
}

void
OnMicModeChanged(GtkDropDown *drop, GParamSpec *pspec, gpointer user_data)
{
	(void)drop;
	(void)pspec;
	SyncMicPage((ConfigDialog *)user_data);
}

GtkWidget *
BuildMicPage(ConfigDialog *d)
{
	GtkWidget *box = NewPage();
	GtkWidget *grid = NULL;
	GtkWidget *section;

	section = NewSection("Microphone input", &grid);
	gtk_box_append(GTK_BOX(box), section);

	GridLabel(GTK_GRID(grid), "Source:", 0);
	{
		GtkStringList *modes = NewStringList();
		for (guint i = 0; i < G_N_ELEMENTS(kMicModes); ++i)
		{
			gtk_string_list_append(modes, kMicModes[i]);
		}
		d->micModeDrop = gtk_drop_down_new(G_LIST_MODEL(modes), NULL);
		gtk_widget_set_hexpand(d->micModeDrop, TRUE);
	}
	gtk_grid_attach(GTK_GRID(grid), d->micModeDrop, 1, 0, 1, 1);

	d->micSampleEntry = AddPathRow(GTK_GRID(grid), 1, "Sample file:",
	                               "micsample.raw", FALSE,
	                               "Select an 8-bit mono PCM WAV sample",
	                               "*.wav", NULL);
	d->micSampleBox = gtk_widget_get_parent(d->micSampleEntry);

	gtk_box_append(GTK_BOX(box),
	               NewHint("The sample and random modes ignore the microphone key; the "
	                       "noise mode uses the internal noise generator. The physical "
	                       "device is the only mode that reacts to real sound."));

	g_signal_connect(d->micModeDrop, "notify::selected",
	                 G_CALLBACK(OnMicModeChanged), d);

	return box;
}

/* --------------------------------------------------------------- Paths page */

std::string
Trim(const std::string &input)
{
	static const char *kWhitespace = " \t\r\n";

	const size_t first = input.find_first_not_of(kWhitespace);
	if (first == std::string::npos)
		return std::string();

	const size_t last = input.find_last_not_of(kWhitespace);
	return input.substr(first, last - first + 1);
}

std::string
CategoryValue(desmume::config::Config *config, PathInfo::KnownPath category)
{
	if (config == NULL)
		return std::string();

	switch (category)
	{
		case PathInfo::ROMS:               return config->path_roms.get();
		case PathInfo::BATTERY:            return config->path_battery.get();
		case PathInfo::SRAM_IMPORT_EXPORT: return config->path_sram_import_export.get();
		case PathInfo::STATES:             return config->path_states.get();
		case PathInfo::STATE_SLOTS:        return config->path_state_slots.get();
		case PathInfo::SCREENSHOTS:        return config->path_screenshots.get();
		case PathInfo::AVI_FILES:          return config->path_avi_files.get();
		/* The Play/Record dialogs already remember their folder here, so the
		 * Movies row edits that instead of adding a second setting. */
		case PathInfo::MOVIES:             return config->movie_directory.get();
		case PathInfo::CHEATS:             return config->path_cheats.get();
		case PathInfo::LUA:                return config->path_lua.get();
		default:                           return std::string();
	}
}

void
SetCategoryValue(desmume::config::Config *config, PathInfo::KnownPath category,
                 const std::string &value)
{
	if (config == NULL)
		return;

	switch (category)
	{
		case PathInfo::ROMS:               config->path_roms.set(value); break;
		case PathInfo::BATTERY:            config->path_battery.set(value); break;
		case PathInfo::SRAM_IMPORT_EXPORT: config->path_sram_import_export.set(value); break;
		case PathInfo::STATES:             config->path_states.set(value); break;
		case PathInfo::STATE_SLOTS:        config->path_state_slots.set(value); break;
		case PathInfo::SCREENSHOTS:        config->path_screenshots.set(value); break;
		case PathInfo::AVI_FILES:          config->path_avi_files.set(value); break;
		case PathInfo::MOVIES:             config->movie_directory.set(value); break;
		case PathInfo::CHEATS:             config->path_cheats.set(value); break;
		case PathInfo::LUA:                config->path_lua.set(value); break;
		default: break;
	}
}

/* What the category falls back to when the entry is left empty.  Shown as the
 * placeholder so the dialog answers "then where does it go?". */
std::string
DefaultHint(PathInfo::KnownPath category)
{
	switch (category)
	{
		case PathInfo::ROMS:
			return g_get_home_dir();

		case PathInfo::BATTERY:
		case PathInfo::STATE_SLOTS:
		case PathInfo::CHEATS:
		{
			gchar *dir = g_build_filename(g_get_user_config_dir(), "desmume", NULL);
			const std::string hint = (dir != NULL) ? dir : "";
			g_free(dir);
			return hint;
		}

		case PathInfo::SCREENSHOTS:
		{
			const gchar *dir = g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);
			return (dir != NULL) ? dir : g_get_home_dir();
		}

		case PathInfo::MOVIES:
			return std::string(g_get_home_dir()) + "/Movies";

		default:
			return "System default";
	}
}

struct FormatChoice
{
	std::string name;
	std::string label;
};

std::vector<FormatChoice>
WritableFormats(void)
{
	std::vector<FormatChoice> formats;
	GSList *list = gdk_pixbuf_get_formats();

	for (GSList *item = list; item != NULL; item = item->next)
	{
		GdkPixbufFormat *format = (GdkPixbufFormat *)item->data;
		if (!gdk_pixbuf_format_is_writable(format))
			continue;

		gchar *name = gdk_pixbuf_format_get_name(format);
		gchar *description = gdk_pixbuf_format_get_description(format);

		FormatChoice choice;
		choice.name = (name != NULL) ? name : "";
		choice.label = (description != NULL) ? description : choice.name;
		if (!choice.name.empty())
			formats.push_back(choice);

		g_free(name);
		g_free(description);
	}

	g_slist_free(list);
	return formats;
}

bool
IsWritableFormat(const std::string &name)
{
	const std::vector<FormatChoice> formats = WritableFormats();
	for (size_t i = 0; i < formats.size(); i++)
	{
		if (formats[i].name == name)
			return true;
	}

	return false;
}

GtkWidget *
BuildPathsPage(ConfigDialog *d)
{
	GtkWidget *box = NewPage();
	GtkWidget *grid = NULL;
	GtkWidget *section;

	/* Folders. */
	section = NewSection("Folders (empty = built-in default)", &grid);
	gtk_box_append(GTK_BOX(box), section);

	for (int i = 0; i < kPathCategoryCount; ++i)
	{
		const desmume::pathsettings::CategoryInfo *info =
			desmume::pathsettings::FindCategory(kPathCategories[i]);
		if (info == NULL)
		{
			d->pathEntries[i] = NULL;
			continue;
		}

		GtkWidget *entry = gtk_entry_new();
		gtk_editable_set_text(GTK_EDITABLE(entry),
		                      desmume::pathsettings::NormalizeDirectory(
		                          CategoryValue(d->config, info->category)).c_str());
		gtk_entry_set_placeholder_text(GTK_ENTRY(entry),
		                               DefaultHint(info->category).c_str());
		gtk_widget_set_tooltip_text(entry, info->description);
		gtk_widget_set_hexpand(entry, TRUE);

		GtkWidget *browse = gtk_button_new_with_label("Browse…");
		g_signal_connect(browse, "clicked", G_CALLBACK(OnBrowseFolderClicked), entry);

		GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_box_append(GTK_BOX(row), entry);
		gtk_box_append(GTK_BOX(row), browse);

		AddRow(GTK_GRID(grid), i, info->label, row);
		d->pathEntries[i] = entry;
	}

	/* Screenshots, matching the Windows dialog's second half. */
	section = NewSection("Screenshots", &grid);
	gtk_box_append(GTK_BOX(box), section);

	GridLabel(GTK_GRID(grid), "Image format:", 0);
	{
		GtkStringList *formats = NewStringList();
		const std::vector<FormatChoice> writable = WritableFormats();
		for (size_t i = 0; i < writable.size(); ++i)
		{
			gtk_string_list_append(formats, writable[i].label.c_str());
		}
		d->shotFormatDrop = gtk_drop_down_new(G_LIST_MODEL(formats), NULL);
		gtk_widget_set_hexpand(d->shotFormatDrop, TRUE);
	}
	gtk_grid_attach(GTK_GRID(grid), d->shotFormatDrop, 1, 0, 1, 1);

	GridLabel(GTK_GRID(grid), "File name:", 1);
	d->shotNameEntry = gtk_entry_new();
	gtk_entry_set_placeholder_text(GTK_ENTRY(d->shotNameEntry),
	                               "desmume-screenshot-1");
	gtk_widget_set_tooltip_text(d->shotNameEntry,
		"Pattern for the screenshot file name (strftime, plus DeSmuME codes).\n"
		"%f  ROM name\n"
		"%r  random number\n"
		"%t  tick (milliseconds since startup)\n"
		"%Y %y %m %d %H %M %S  date and time\n"
		"Leave empty for desmume-screenshot-N.");
	gtk_widget_set_hexpand(d->shotNameEntry, TRUE);
	gtk_grid_attach(GTK_GRID(grid), d->shotNameEntry, 1, 1, 1, 1);

	gtk_grid_attach(GTK_GRID(grid),
	                NewHint("Saved to the Screenshots folder above; the extension "
	                        "follows the image format."),
	                1, 2, 1, 1);

	d->saveLastVisit = gtk_check_button_new_with_label(
		"Remember the folder of the last opened ROM");
	gtk_box_append(GTK_BOX(box), d->saveLastVisit);

	return box;
}

/* ------------------------------------------------------------------- apply */

int
SelectedFilter(ConfigDialog *d)
{
	const guint index = GetDropDown(d->filterDrop);
	if ((d->filterValues == NULL) || (index >= (guint)d->filterCount))
	{
		return WGPUPresentUpscaler_Off;
	}
	return d->filterValues[index];
}

/* Everything the core reads while it boots, in one string.  Compared before
 * and after OK to decide whether a reset is needed. */
std::string
ResetSignature(desmume::config::Config *config)
{
	std::string signature;

	signature += config->emu_advanced_timing.get() ? "1" : "0";
	signature += config->emu_game_hacks.get() ? "1" : "0";
	signature += config->emu_debug_console.get() ? "1" : "0";
	signature += config->emu_ensata.get() ? "1" : "0";
	signature += '|';
	signature += config->bios_use_external.get() ? "1" : "0";
	signature += config->bios_arm9_path.get();
	signature += '|';
	signature += config->bios_arm7_path.get();
	signature += '|';
	signature += config->bios_swi_from_bios.get() ? "1" : "0";
	signature += config->bios_patch_swi3.get() ? "1" : "0";
	signature += '|';
	signature += config->fw_use_external.get() ? "1" : "0";
	signature += config->fw_image_path.get();
	signature += '|';
	signature += config->fw_boot_from.get() ? "1" : "0";
	signature += config->fw_use_ext_settings.get() ? "1" : "0";
	signature += '|';
	signature += config->fwconfig_nickname.get();
	signature += config->fwconfig_message.get();
	signature += std::to_string(config->fwconfig_favorite_color.get());
	signature += std::to_string(config->fwconfig_birth_month.get());
	signature += std::to_string(config->fwconfig_birth_day.get());
	signature += std::to_string(config->firmware_language.get());
	signature += config->fwconfig_mac_address.get();
	signature += '|';
	signature += std::to_string(config->slot1_id.get());
	signature += config->slot1_fat_path.get();
	signature += config->slot1_fat_path_type.get() ? "1" : "0";
	signature += config->slot1_debug_path.get();
	signature += '|';
	signature += std::to_string(config->slot2_id.get());
	signature += std::to_string(config->slot2_cflash_mode.get());
	signature += config->slot2_cflash_path.get();
	signature += config->slot2_gba_rom.get();
	signature += '|';
	signature += config->wifi_enabled.get() ? "1" : "0";
	signature += config->wifi_compatibility_mode.get() ? "1" : "0";
	signature += std::to_string(config->wifi_bridge_adapter.get());

	return signature;
}

/* Widget-only checks, run before CollectValues(): a rejected page must not
 * leave a half-updated config behind, because any later menu toggle calls
 * config->save() and would persist it. */
const char *
ValidateValues(ConfigDialog *d)
{
	/* Firmware MAC: the entry may carry colons or dashes, but the twelve
	 * hex digits have to be there. */
	{
		const char *text = gtk_editable_get_text(GTK_EDITABLE(d->fwMac));
		int digits = 0;
		for (const char *p = text; (*p != '\0') && (digits < 12); ++p)
		{
			if (g_ascii_isxdigit(*p))
			{
				++digits;
			}
		}
		if (digits != 12)
		{
			return "The MAC address needs 12 hexadecimal digits.";
		}
	}

	/* Slot 1: the R4 needs a FAT folder, the retail debug device a filesystem
	 * folder.  Both are skipped when the device does not use them. */
	{
		const int row = (int)GetDropDown(d->slot1Drop);
		const gboolean use_rom_dir = gtk_check_button_get_active(
			GTK_CHECK_BUTTON(d->slot1UseRomDir)) != FALSE;
		const char *fat = gtk_editable_get_text(GTK_EDITABLE(d->slot1FatEntry));
		const char *debug = gtk_editable_get_text(GTK_EDITABLE(d->slot1DebugEntry));

		if (desmume_emu_slot1_has_fat(row) && !use_rom_dir &&
		    !g_file_test(fat, G_FILE_TEST_IS_DIR))
		{
			return "Choose an existing FAT directory for the R4 device.";
		}
		if (desmume_emu_slot1_has_debug(row) &&
		    !g_file_test(debug, G_FILE_TEST_IS_DIR))
		{
			return "Choose an existing directory for the Slot 1 debug filesystem.";
		}
	}

	/* Slot 2: the GBA cartridge needs a ROM, CompactFlash a folder or image
	 * unless it mirrors the loaded ROM's folder. */
	{
		const int row = (int)GetDropDown(d->slot2Drop);
		const int mode = (int)GetDropDown(d->slot2CFlashDrop);
		const char *cflash = gtk_editable_get_text(GTK_EDITABLE(d->slot2CFlashEntry));
		const char *rom = gtk_editable_get_text(GTK_EDITABLE(d->slot2GbaEntry));

		if (desmume_emu_slot2_has_gba_rom(row) &&
		    !g_file_test(rom, G_FILE_TEST_IS_REGULAR))
		{
			return "Choose an existing GBA ROM for the GBA cartridge device.";
		}
		if (desmume_emu_slot2_has_cflash(row) && (mode != 2))
		{
			const GFileTest wanted = (mode == 0) ? G_FILE_TEST_IS_DIR
			                                     : G_FILE_TEST_IS_REGULAR;
			if (!g_file_test(cflash, wanted))
			{
				return (mode == 0)
					? "Choose an existing CompactFlash directory."
					: "Choose an existing CompactFlash image file.";
			}
		}
	}

	/* Microphone: sample mode without a readable file would silently leave
	 * the core without any source. */
	if (GetDropDown(d->micModeDrop) == 1)
	{
		const char *sample = gtk_editable_get_text(GTK_EDITABLE(d->micSampleEntry));
		if (!g_file_test(sample, G_FILE_TEST_IS_REGULAR))
		{
			return "Choose an existing sample file for the sample microphone mode.";
		}
	}

	/* Paths: a typo would send every later file there and only fail much
	 * later, so refuse folders that do not exist. */
	for (int i = 0; i < kPathCategoryCount; ++i)
	{
		if (d->pathEntries[i] == NULL)
		{
			continue;
		}

		const std::string value = desmume::pathsettings::NormalizeDirectory(
			gtk_editable_get_text(GTK_EDITABLE(d->pathEntries[i])));
		if (!value.empty() && !g_file_test(value.c_str(), G_FILE_TEST_IS_DIR))
		{
			return "A folder on the Paths page does not exist.";
		}
	}

	return NULL;
}

/* Read every widget and write the values into `config`.  ValidateValues()
 * must have accepted them first. */
void
CollectValues(ConfigDialog *d)
{
	desmume::config::Config *config = d->config;

	/* Emulation. */
	config->emu_advanced_timing = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->advancedTiming)) != FALSE;
	config->emu_game_hacks = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->gameHacks)) != FALSE;
	config->emu_debug_console = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->debugConsole)) != FALSE;
	config->emu_ensata = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->ensata)) != FALSE;

	config->bios_use_external = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->useExtBios)) != FALSE;
	config->bios_arm9_path = gtk_editable_get_text(GTK_EDITABLE(d->bios9Entry));
	config->bios_arm7_path = gtk_editable_get_text(GTK_EDITABLE(d->bios7Entry));
	config->bios_swi_from_bios = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->swiFromBios)) != FALSE;
	config->bios_patch_swi3 = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->patchSwi3)) != FALSE;

	config->fw_use_external = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->useExtFirmware)) != FALSE;
	config->fw_image_path = gtk_editable_get_text(GTK_EDITABLE(d->firmwareEntry));
	config->fw_boot_from = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->bootFromFirmware)) != FALSE;
	config->fw_use_ext_settings = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->useExtFirmwareSettings)) != FALSE;

	config->use_jit = gtk_check_button_get_active(GTK_CHECK_BUTTON(d->useJit)) != FALSE;
	config->jit_max_block_size =
		(int)(gtk_range_get_value(GTK_RANGE(d->jitBlockSize)) + 0.5);

	/* Firmware user data. */
	{
		const char *nickname = gtk_editable_get_text(GTK_EDITABLE(d->fwNickname));
		config->fwconfig_nickname = (nickname[0] != '\0') ? nickname : "DeSmuME";
		config->fwconfig_message = gtk_editable_get_text(GTK_EDITABLE(d->fwMessage));
		config->fwconfig_favorite_color = (int)GetDropDown(d->fwColorDrop);
		config->fwconfig_birth_month = (int)GetDropDown(d->fwMonthDrop) + 1;
		config->fwconfig_birth_day = (int)GetDropDown(d->fwDayDrop) + 1;
		config->firmware_language = (int)GetDropDown(d->fwLanguageDrop);

		/* Store what the core will actually use: 12 bare hex digits. */
		std::string mac;
		const char *text = gtk_editable_get_text(GTK_EDITABLE(d->fwMac));
		for (const char *p = text; (*p != '\0') && (mac.size() < 12); ++p)
		{
			if (g_ascii_isxdigit(*p))
			{
				mac += g_ascii_toupper(*p);
			}
		}
		config->fwconfig_mac_address = mac;
	}

	/* Graphics. */
	config->core3D = desmume_emu_renderer_id((int)GetDropDown(d->rendererDrop));
	config->gfx3dColorDepth = kColorDepthValues[GetDropDown(d->colorDepthDrop)];
	config->multisamplingSize = kMultisampleValues[GetDropDown(d->multisampleDrop)];
	config->multisampling = config->multisamplingSize.get() != 0;

	{
		const guint scaleIndex = GetDropDown(d->textureScaleDrop);
		config->textureUpscale = (scaleIndex == 1) ? 2 : (scaleIndex == 2) ? 4 : 1;
	}

	config->gpuScaleFactor = (float)gtk_spin_button_get_value(
		GTK_SPIN_BUTTON(d->gpuScaleSpin));
	config->textureDeposterize = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->deposterize)) != FALSE;
	config->textureSmoothing = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->smoothing)) != FALSE;
	config->highColorInterpolation = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->highColorInterpolation)) != FALSE;
	config->gfx3dTxtHack = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->txtHack)) != FALSE;
	config->gfx3dLineHack = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->lineHack)) != FALSE;
	config->gfx3dEdgeMark = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->edgeMark)) != FALSE;
	config->gfx3dFog = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->fog)) != FALSE;
	config->gfx3dTexture = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->textures)) != FALSE;
	config->gfx3dShadowPolygon = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->shadowPolygon)) != FALSE;
	config->gfx3dZeroAlphaBlending = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->zeroAlpha)) != FALSE;
	config->gfx3dNdsDepthCalculation = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->ndsDepth)) != FALSE;
	config->gfx3dDepthLEqualFacing = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->depthLEqual)) != FALSE;

	/* Sound. */
	config->audio_enabled = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->audioEnabled)) != FALSE;
	config->audio_volume = (int)(gtk_range_get_value(GTK_RANGE(d->audioVolume)) + 0.5);

	int syncMode = 0;
	if (gtk_check_button_get_active(GTK_CHECK_BUTTON(d->syncSync)))
	{
		if ((d->syncP != NULL) && gtk_check_button_get_active(GTK_CHECK_BUTTON(d->syncP)))
		{
			syncMode = 3;
		}
		else if (gtk_check_button_get_active(GTK_CHECK_BUTTON(d->syncZ)))
		{
			syncMode = 2;
		}
		else
		{
			syncMode = 1;
		}
	}
	config->audio_sync = syncMode;
	config->audio_interpolation = (int)GetDropDown(d->interpolationDrop);

	/* Slots. */
	{
		const guint row = GetDropDown(d->slot1Drop);
		const int id = desmume_emu_slot1_id((int)row);
		const gboolean use_rom_dir = gtk_check_button_get_active(
			GTK_CHECK_BUTTON(d->slot1UseRomDir)) != FALSE;
		const std::string fat_dir = Trim(gtk_editable_get_text(
			GTK_EDITABLE(d->slot1FatEntry)));
		const std::string debug_dir = Trim(gtk_editable_get_text(
			GTK_EDITABLE(d->slot1DebugEntry)));

		config->slot1_id = id;
		config->slot1_fat_path = fat_dir;
		config->slot1_fat_path_type = use_rom_dir;
		config->slot1_debug_path = debug_dir;
	}

	{
		const guint row = GetDropDown(d->slot2Drop);
		const int id = desmume_emu_slot2_id((int)row);
		const int mode = (int)GetDropDown(d->slot2CFlashDrop);
		const std::string cflash_path = Trim(gtk_editable_get_text(
			GTK_EDITABLE(d->slot2CFlashEntry)));
		const std::string gba_rom = Trim(gtk_editable_get_text(
			GTK_EDITABLE(d->slot2GbaEntry)));

		config->slot2_id = id;
		config->slot2_cflash_mode = mode;
		config->slot2_cflash_path = cflash_path;
		config->slot2_gba_rom = gba_rom;
	}

	/* Wi-Fi. */
	config->wifi_enabled = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->wifiEnabled)) != FALSE;
	config->wifi_compatibility_mode = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->wifiCompatibility)) != FALSE;
	{
		const guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(d->wifiAdapterDrop));
		if (selected != GTK_INVALID_LIST_POSITION)
		{
			config->wifi_bridge_adapter = (int)selected;
		}
	}

	/* Microphone. */
	config->mic_mode = (int)GetDropDown(d->micModeDrop);
	config->mic_sample_file = Trim(gtk_editable_get_text(
		GTK_EDITABLE(d->micSampleEntry)));

	/* Controls and hotkeys.  The dialog edits private copies; commit them to
	 * the shared ctrlssdl tables only when OK is pressed, so Cancel really
	 * cancels. */
	for (int i = 0; i < NB_KEYS; ++i)
	{
		joypad_cfg[i] = d->joyKeys[i];
	}
	ctrls_set_allow_up_down(gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->allowUpDown)) != FALSE);
	config->allow_up_down = ctrls_get_allow_up_down();

	for (size_t i = 0; i < d->keySlots.size(); ++i)
	{
		const KeySlot &slot = d->keySlots[i];
		if (slot.kind == KEY_SLOT_CONTROL)
		{
			keyboard_cfg[slot.index] = slot.keyval;
			continue;
		}
		if (slot.kind == KEY_SLOT_TURBO)
		{
			turbo_keys[slot.index] = slot.keyval;
			continue;
		}

		gchar *accel = (slot.keyval == 0)
			? g_strdup("")
			: gtk_accelerator_name(slot.keyval, slot.mods);
		desmume_hotkey_set_accel(slot.index, accel);

		const DesmumeHotkeyDef *def = desmume_hotkey_def(slot.index);
		if ((def != NULL) && (strcmp(def->slug, "fast-forward") == 0))
		{
			/* Keep the scalar option in step with the [HOTKEYS] group.  Both
			 * write the same key, and Config::save() runs last. */
			config->fast_forward_hold = accel;
		}
		g_free(accel);
	}

	desmume_emu_save_input_config(config->key_file());
	desmume_hotkeys_save(config->key_file());

	/* Paths. */
	for (int i = 0; i < kPathCategoryCount; ++i)
	{
		if (d->pathEntries[i] == NULL)
		{
			continue;
		}

		SetCategoryValue(config, kPathCategories[i],
		                 desmume::pathsettings::NormalizeDirectory(
		                     gtk_editable_get_text(GTK_EDITABLE(d->pathEntries[i]))));
	}

	{
		const std::vector<FormatChoice> formats = WritableFormats();
		const guint row = GetDropDown(d->shotFormatDrop);
		config->path_screenshot_image_format =
			(row < formats.size()) ? formats[row].name : "png";
	}
	config->path_screenshot_format = Trim(gtk_editable_get_text(
		GTK_EDITABLE(d->shotNameEntry)));
	config->path_save_last_visit = gtk_check_button_get_active(
		GTK_CHECK_BUTTON(d->saveLastVisit)) != FALSE;
}

/* --------------------------------------------------------------- reset ask */

void
OnResetAnswer(GObject *source, GAsyncResult *result, gpointer user_data)
{
	(void)user_data;

	const int choice = gtk_alert_dialog_choose_finish(
		GTK_ALERT_DIALOG(source), result, NULL);
	if (choice == 0)
	{
		desmume_emu_reset();
	}
}

void
OfferReset(GtkWindow *parent)
{
	GtkAlertDialog *dialog = gtk_alert_dialog_new(
		"The emulation settings changed. The ROM has to be reset for them to "
		"take effect.");
	static const char *buttons[] = { "Reset now", "Later", NULL };

	gtk_alert_dialog_set_buttons(dialog, buttons);
	gtk_alert_dialog_set_cancel_button(dialog, 1);
	gtk_alert_dialog_set_default_button(dialog, 0);
	gtk_alert_dialog_choose(dialog, parent, NULL, OnResetAnswer, NULL);
	g_object_unref(dialog);
}

/* -------------------------------------------------------------- callbacks */

void
OnApplyClicked(GtkButton *button, gpointer user_data)
{
	(void)button;
	ConfigDialog *d = (ConfigDialog *)user_data;

	const std::string before = ResetSignature(d->config);

	const char *problem = ValidateValues(d);
	if (problem != NULL)
	{
		ShowAlert(GTK_WINDOW(d->window), problem);
		return;
	}

	CollectValues(d);

	/* Push everything the dialog just stored into the core, then the values
	 * that need the renderer/SPU to be rebuilt.  The 3D accuracy switches go
	 * first so a rebuilt OpenGL core already sees them. */
	desmume_config_apply(d->config);

	desmume_emu_set_jit_config(d->config->use_jit.get(),
	                           d->config->jit_max_block_size.get());
	desmume_emu_set_audio_config(d->config->audio_enabled.get(),
	                             d->config->audio_volume.get(),
	                             d->config->audio_sync.get(),
	                             d->config->audio_interpolation.get());
	desmume_emu_set_graphics_config(d->config->core3D.get(),
	                                d->config->textureUpscale.get(),
	                                d->config->textureDeposterize.get() ? TRUE : FALSE,
	                                d->config->textureSmoothing.get() ? TRUE : FALSE,
	                                d->config->highColorInterpolation.get() ? TRUE : FALSE,
	                                d->config->multisampling.get()
	                                    ? d->config->multisamplingSize.get() : 0);
	/* The bridge falls back to SoftRasterizer when the requested renderer
	 * cannot start; persist what is actually running. */
	d->config->core3D = desmume_emu_get_renderer();

	/* Screen presentation.  The chosen row maps to a WGPUInstanceBackend mask;
	 * the software row disables the WebGPU path entirely (filter forced Off),
	 * exactly like GTK3's "Software (Cairo)" entry. */
	{
		const guint presentationRow = GetDropDown(d->presentationDrop);
		const guint32 backendMask =
			(presentationRow < (guint)d->presentationCount)
				? d->presentationMasks[presentationRow]
				: PRESENTATION_SOFTWARE;
		const gboolean presentationWGPU = (backendMask != PRESENTATION_SOFTWARE);

		desmume_emu_set_presentation_backend(backendMask);

		int filter = presentationWGPU ? SelectedFilter(d) : WGPUPresentUpscaler_Off;
		if ((filter != WGPUPresentUpscaler_Off) &&
		    !desmume_emu_set_gpu_filter(filter, d->config->gpuScaleFactor.get()))
		{
			filter = WGPUPresentUpscaler_Off;
			desmume_emu_set_gpu_filter(filter, 1.0f);
		}
		else if (filter == WGPUPresentUpscaler_Off)
		{
			desmume_emu_set_gpu_filter(WGPUPresentUpscaler_Off, 1.0f);
		}
		d->config->gpuFilter = filter;
		d->config->presentationWGPU = (presentationWGPU &&
		                               (filter != WGPUPresentUpscaler_Off));
		d->config->presentationWGPUBackend = (int)backendMask;
	}

	d->config->save();

	/* The alert must outlive this window, so it is parented to the main
	 * window and shown after the dialog is gone: destroying a window dismisses
	 * the alerts that are transient for it. */
	const gboolean needs_reset = desmume_emu_has_rom() &&
		(ResetSignature(d->config) != before);
	GtkWindow *alert_parent = d->parentWindow;

	if (d->applied != NULL)
	{
		d->applied(d->appliedData);
	}

	gtk_window_destroy(GTK_WINDOW(d->window));

	if (needs_reset)
	{
		OfferReset(alert_parent);
	}
}

void
OnCancelClicked(GtkButton *button, gpointer user_data)
{
	(void)button;
	ConfigDialog *d = (ConfigDialog *)user_data;
	gtk_window_destroy(GTK_WINDOW(d->window));
}

void
OnDialogDestroy(GtkWidget *widget, gpointer user_data)
{
	ConfigDialog *d = (ConfigDialog *)user_data;

	(void)widget;

	if (sDialog == d)
	{
		sDialog = NULL;
	}
	g_free(d->filterValues);
	g_free(d->presentationMasks);
	g_free(d);
	/* The window's sole reference is owned by GTK.  Do not ref_sink it on
	 * creation or unref it here: an extra ref keeps the object alive past
	 * gtk_window_destroy(), which suppresses the "destroy" signal and leaves
	 * sDialog pointing at a destroyed window on the next show(). */
}

/* --------------------------------------------------------------- populate */

int
RendererIndexForId(int id)
{
	const int count = desmume_emu_renderer_count();
	for (int i = 0; i < count; ++i)
	{
		if (desmume_emu_renderer_id(i) == id)
		{
			return i;
		}
	}
	return 0;
}

int
FilterIndexForValue(ConfigDialog *d, int value)
{
	for (int i = 0; i < d->filterCount; ++i)
	{
		if (d->filterValues[i] == value)
		{
			return i;
		}
	}
	return 0;
}

int
IndexForId(int count, int (*id_at)(int), int id)
{
	for (int i = 0; i < count; ++i)
	{
		if (id_at(i) == id)
		{
			return i;
		}
	}
	return 0;
}

void
Populate(ConfigDialog *d)
{
	desmume::config::Config *config = d->config;

	/* Emulation. */
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->advancedTiming),
	                            config->emu_advanced_timing.get() ? TRUE : FALSE);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->gameHacks),
	                            config->emu_game_hacks.get() ? TRUE : FALSE);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->debugConsole),
	                            config->emu_debug_console.get() ? TRUE : FALSE);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->ensata),
	                            config->emu_ensata.get() ? TRUE : FALSE);

	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->useExtBios),
	                            config->bios_use_external.get() ? TRUE : FALSE);
	gtk_editable_set_text(GTK_EDITABLE(d->bios9Entry),
	                      config->bios_arm9_path.get().c_str());
	gtk_editable_set_text(GTK_EDITABLE(d->bios7Entry),
	                      config->bios_arm7_path.get().c_str());
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->swiFromBios),
	                            config->bios_swi_from_bios.get() ? TRUE : FALSE);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->patchSwi3),
	                            config->bios_patch_swi3.get() ? TRUE : FALSE);

	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->useExtFirmware),
	                            config->fw_use_external.get() ? TRUE : FALSE);
	gtk_editable_set_text(GTK_EDITABLE(d->firmwareEntry),
	                      config->fw_image_path.get().c_str());
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->bootFromFirmware),
	                            config->fw_boot_from.get() ? TRUE : FALSE);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->useExtFirmwareSettings),
	                            config->fw_use_ext_settings.get() ? TRUE : FALSE);

	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->useJit),
	                            config->use_jit.get() ? TRUE : FALSE);
	{
		int blockSize = config->jit_max_block_size.get();
		if (blockSize < 1) blockSize = 1;
		if (blockSize > 100) blockSize = 100;
		gtk_range_set_value(GTK_RANGE(d->jitBlockSize), blockSize);
	}
	SyncEmulationPage(d);

	/* Firmware user data. */
	gtk_editable_set_text(GTK_EDITABLE(d->fwNickname),
	                      config->fwconfig_nickname.get().c_str());
	gtk_editable_set_text(GTK_EDITABLE(d->fwMessage),
	                      config->fwconfig_message.get().c_str());
	SetDropDown(d->fwColorDrop, (guint)CLAMP(config->fwconfig_favorite_color.get(), 0, 15));
	SetDropDown(d->fwMonthDrop, (guint)CLAMP(config->fwconfig_birth_month.get() - 1, 0, 11));
	SetDropDown(d->fwDayDrop, (guint)CLAMP(config->fwconfig_birth_day.get() - 1, 0, 30));
	SetDropDown(d->fwLanguageDrop, (guint)CLAMP(config->firmware_language.get(), 0, 5));
	gtk_editable_set_text(GTK_EDITABLE(d->fwMac),
	                      config->fwconfig_mac_address.get().c_str());

	/* Graphics. */
	SetDropDown(d->rendererDrop, (guint)RendererIndexForId(config->core3D.get()));

	{
		const int depth = config->gfx3dColorDepth.get();
		guint index = 1; /* 18 bit */
		for (guint i = 0; i < G_N_ELEMENTS(kColorDepthValues); ++i)
		{
			if (kColorDepthValues[i] == depth)
			{
				index = i;
				break;
			}
		}
		SetDropDown(d->colorDepthDrop, index);
	}

	{
		const int requested = config->multisampling.get()
			? config->multisamplingSize.get() : 0;
		guint index = 0;
		for (guint i = 0; i < G_N_ELEMENTS(kMultisampleValues); ++i)
		{
			index = i; /* round up to the smallest entry >= requested */
			if (kMultisampleValues[i] >= requested)
			{
				break;
			}
		}
		SetDropDown(d->multisampleDrop, index);
	}

	{
		const int textureUpscale = config->textureUpscale.get();
		SetDropDown(d->textureScaleDrop,
		            (textureUpscale == 2) ? 1 : (textureUpscale == 4) ? 2 : 0);
	}

	float gpuScale = config->gpuScaleFactor.get();
	if (gpuScale < 1.0f) gpuScale = 1.0f;
	if (gpuScale > 4.0f) gpuScale = 4.0f;
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(d->gpuScaleSpin), gpuScale);

	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->deposterize),
	                            config->textureDeposterize.get() ? TRUE : FALSE);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->smoothing),
	                            config->textureSmoothing.get() ? TRUE : FALSE);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->highColorInterpolation),
	                            config->highColorInterpolation.get() ? TRUE : FALSE);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->txtHack),
	                            config->gfx3dTxtHack.get() ? TRUE : FALSE);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->lineHack),
	                            config->gfx3dLineHack.get() ? TRUE : FALSE);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->edgeMark),
	                            config->gfx3dEdgeMark.get() ? TRUE : FALSE);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->fog),
	                            config->gfx3dFog.get() ? TRUE : FALSE);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->textures),
	                            config->gfx3dTexture.get() ? TRUE : FALSE);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->shadowPolygon),
	                            config->gfx3dShadowPolygon.get() ? TRUE : FALSE);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->zeroAlpha),
	                            config->gfx3dZeroAlphaBlending.get() ? TRUE : FALSE);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->ndsDepth),
	                            config->gfx3dNdsDepthCalculation.get() ? TRUE : FALSE);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->depthLEqual),
	                            config->gfx3dDepthLEqualFacing.get() ? TRUE : FALSE);

	/* Screen presentation: restore the saved backend, falling back to the
	 * first WebGPU row when the configured backend disappeared (a different
	 * machine or driver) and to Software when WGPU presentation was off. */
	guint presentationRow = (guint)(d->presentationCount - 1); /* Software */
	if (config->presentationWGPU.get())
	{
		const guint32 savedMask = (guint32)config->presentationWGPUBackend.get();
		for (int i = 0; i < d->presentationCount - 1; ++i)
		{
			if (d->presentationMasks[i] == savedMask)
			{
				presentationRow = (guint)i;
				break;
			}
		}
		if ((presentationRow == (guint)(d->presentationCount - 1)) &&
		    (d->presentationCount > 1))
		{
			presentationRow = 0; /* first usable WebGPU backend */
		}
	}
	SetDropDown(d->presentationDrop, presentationRow);
	/* Sync sensitivity explicitly: set_selected() is a no-op when the value is
	 * unchanged, so notify::selected may not have fired. */
	{
		const gboolean software = d->presentationMasks[presentationRow] ==
			PRESENTATION_SOFTWARE;
		gtk_widget_set_sensitive(d->filterDrop, !software);
	}

	{
		const int filter = config->presentationWGPU.get() ? config->gpuFilter.get() : 0;
		SetDropDown(d->filterDrop, (guint)FilterIndexForValue(d, filter));
	}

	/* Sound. */
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->audioEnabled),
	                            config->audio_enabled.get() ? TRUE : FALSE);

	{
		int volume = config->audio_volume.get();
		if (volume < 0) volume = 0;
		if (volume > 100) volume = 100;
		gtk_range_set_value(GTK_RANGE(d->audioVolume), volume);
	}

	{
		int syncMode = config->audio_sync.get();
		if ((syncMode < 0) || (syncMode > 3) ||
		    ((syncMode == 3) && (d->syncP == NULL)))
		{
			syncMode = 0;
		}

		gtk_check_button_set_active(GTK_CHECK_BUTTON(d->syncDual),
		                            (syncMode == 0) ? TRUE : FALSE);
		gtk_check_button_set_active(GTK_CHECK_BUTTON(d->syncSync),
		                            (syncMode != 0) ? TRUE : FALSE);
		gtk_check_button_set_active(GTK_CHECK_BUTTON(d->syncN),
		                            (syncMode <= 1) ? TRUE : FALSE);
		gtk_check_button_set_active(GTK_CHECK_BUTTON(d->syncZ),
		                            (syncMode == 2) ? TRUE : FALSE);
		if (d->syncP != NULL)
		{
			gtk_check_button_set_active(GTK_CHECK_BUTTON(d->syncP),
			                            (syncMode == 3) ? TRUE : FALSE);
		}
		UpdateSyncSensitivity(d);
	}

	{
		int interpolation = config->audio_interpolation.get();
		if ((interpolation < 0) || (interpolation > 2))
		{
			interpolation = 1;
		}
		SetDropDown(d->interpolationDrop, (guint)interpolation);
	}

	/* Slots. */
	SetDropDown(d->slot1Drop, (guint)IndexForId(desmume_emu_slot1_count(),
	                                            desmume_emu_slot1_id,
	                                            config->slot1_id.get()));
	gtk_editable_set_text(GTK_EDITABLE(d->slot1FatEntry),
	                      config->slot1_fat_path.get().c_str());
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->slot1UseRomDir),
	                            config->slot1_fat_path_type.get() ? TRUE : FALSE);
	{
		std::string debug_dir = config->slot1_debug_path.get();
		if (debug_dir.empty())
		{
			char *fallback = desmume_emu_slot1_default_debug_path();
			debug_dir = (fallback != NULL) ? fallback : "";
			g_free(fallback);
		}
		gtk_editable_set_text(GTK_EDITABLE(d->slot1DebugEntry), debug_dir.c_str());
	}

	SetDropDown(d->slot2Drop, (guint)IndexForId(desmume_emu_slot2_count(),
	                                            desmume_emu_slot2_id,
	                                            config->slot2_id.get()));
	SetDropDown(d->slot2CFlashDrop,
	            (guint)CLAMP(config->slot2_cflash_mode.get(), 0, 2));
	gtk_editable_set_text(GTK_EDITABLE(d->slot2CFlashEntry),
	                      config->slot2_cflash_path.get().c_str());
	gtk_editable_set_text(GTK_EDITABLE(d->slot2GbaEntry),
	                      config->slot2_gba_rom.get().c_str());

	/* The descriptions and the row sensitivity follow the selection. */
	OnSlot1Changed(GTK_DROP_DOWN(d->slot1Drop), NULL, d);
	OnSlot2Changed(GTK_DROP_DOWN(d->slot2Drop), NULL, d);

	/* Wi-Fi. */
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->wifiEnabled),
	                            config->wifi_enabled.get() ? TRUE : FALSE);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->wifiCompatibility),
	                            config->wifi_compatibility_mode.get() ? TRUE : FALSE);
	{
		const int adapter = config->wifi_bridge_adapter.get();
		GListModel *model = gtk_drop_down_get_model(GTK_DROP_DOWN(d->wifiAdapterDrop));
		if ((model != NULL) && (adapter >= 0) &&
		    ((guint)adapter < g_list_model_get_n_items(model)))
		{
			SetDropDown(d->wifiAdapterDrop, (guint)adapter);
		}
	}
	gtk_widget_set_sensitive(d->wifiCompatibility,
	                         gtk_check_button_get_active(GTK_CHECK_BUTTON(d->wifiEnabled)));
	gtk_widget_set_sensitive(d->wifiAdapterDrop,
	                         gtk_check_button_get_active(GTK_CHECK_BUTTON(d->wifiEnabled)));

	/* Microphone. */
	SetDropDown(d->micModeDrop, (guint)CLAMP(config->mic_mode.get(), 0, 3));
	gtk_editable_set_text(GTK_EDITABLE(d->micSampleEntry),
	                      config->mic_sample_file.get().c_str());
	SyncMicPage(d);

	/* Paths. */
	{
		const std::vector<FormatChoice> formats = WritableFormats();
		const std::string current = IsWritableFormat(
			config->path_screenshot_image_format.get())
			? config->path_screenshot_image_format.get() : "png";
		guint index = 0;
		for (size_t i = 0; i < formats.size(); ++i)
		{
			if (formats[i].name == current)
			{
				index = (guint)i;
				break;
			}
		}
		GListModel *model = gtk_drop_down_get_model(GTK_DROP_DOWN(d->shotFormatDrop));
		if ((model != NULL) && (g_list_model_get_n_items(model) > index))
		{
			SetDropDown(d->shotFormatDrop, index);
		}
	}
	gtk_editable_set_text(GTK_EDITABLE(d->shotNameEntry),
	                      config->path_screenshot_format.get().c_str());
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->saveLastVisit),
	                            config->path_save_last_visit.get() ? TRUE : FALSE);
}

} /* namespace */

/* ------------------------------------------------------------------ public */

void
desmume_config_apply(desmume::config::Config *config)
{
	if (config == NULL)
	{
		return;
	}

	desmume_emu_set_cpu_options(config->emu_advanced_timing.get(),
	                            config->emu_game_hacks.get(),
	                            config->emu_debug_console.get(),
	                            config->emu_ensata.get());
	desmume_emu_set_bios_config(config->bios_use_external.get(),
	                            config->bios_arm9_path.get().c_str(),
	                            config->bios_arm7_path.get().c_str(),
	                            config->bios_swi_from_bios.get(),
	                            config->bios_patch_swi3.get());
	desmume_emu_set_firmware_image_config(config->fw_use_external.get(),
	                                      config->fw_image_path.get().c_str(),
	                                      config->fw_boot_from.get(),
	                                      config->fw_use_ext_settings.get());
	desmume_emu_set_firmware_user_config(config->fwconfig_nickname.get().c_str(),
	                                     config->fwconfig_message.get().c_str(),
	                                     config->fwconfig_favorite_color.get(),
	                                     config->fwconfig_birth_month.get(),
	                                     config->fwconfig_birth_day.get(),
	                                     config->firmware_language.get(),
	                                     config->fwconfig_mac_address.get().c_str());

	desmume_emu_set_gfx3d_config(config->gfx3dEdgeMark.get(),
	                             config->gfx3dFog.get(),
	                             config->gfx3dTexture.get(),
	                             config->gfx3dLineHack.get(),
	                             config->gfx3dTxtHack.get(),
	                             config->gfx3dShadowPolygon.get(),
	                             config->gfx3dZeroAlphaBlending.get(),
	                             config->gfx3dNdsDepthCalculation.get(),
	                             config->gfx3dDepthLEqualFacing.get(),
	                             config->gfx3dColorDepth.get());

	desmume_emu_set_mic_config(config->mic_mode.get(),
	                           config->mic_sample_file.get().c_str());

	desmume_emu_set_slot1_config(config->slot1_id.get(),
	                             config->slot1_fat_path.get().c_str(),
	                             config->slot1_fat_path_type.get(),
	                             config->slot1_debug_path.get().c_str());
	desmume_emu_set_slot2_config(config->slot2_id.get(),
	                             config->slot2_cflash_mode.get(),
	                             config->slot2_cflash_path.get().c_str(),
	                             config->slot2_gba_rom.get().c_str());

	desmume_emu_set_wifi_config(config->wifi_enabled.get(),
	                            config->wifi_compatibility_mode.get(),
	                            config->wifi_bridge_adapter.get());

	/* Folders: an empty value means "built-in default", so only non-empty
	 * ones are pushed.  Movies has no path-model entry (the Play/Record
	 * dialogs read it directly). */
	for (int i = 0; i < kPathCategoryCount; ++i)
	{
		if (kPathCategories[i] == PathInfo::MOVIES)
		{
			continue;
		}

		std::string value;
		switch (kPathCategories[i])
		{
			case PathInfo::ROMS:               value = config->path_roms.get(); break;
			case PathInfo::BATTERY:            value = config->path_battery.get(); break;
			case PathInfo::SRAM_IMPORT_EXPORT: value = config->path_sram_import_export.get(); break;
			case PathInfo::STATES:             value = config->path_states.get(); break;
			case PathInfo::STATE_SLOTS:        value = config->path_state_slots.get(); break;
			case PathInfo::SCREENSHOTS:        value = config->path_screenshots.get(); break;
			case PathInfo::AVI_FILES:          value = config->path_avi_files.get(); break;
			case PathInfo::CHEATS:             value = config->path_cheats.get(); break;
			case PathInfo::LUA:                value = config->path_lua.get(); break;
			default: break;
		}

		value = desmume::pathsettings::NormalizeDirectory(value);
		/* An empty value is meaningful here: the path model then falls back
		 * to its own default, which is what an empty row promises. */
		path.setpath(kPathCategories[i], value);
	}
}

/* Where the Open ROM dialog starts. */
char *
desmume_config_rom_directory(desmume::config::Config *config)
{
	std::string configured;
	if (config != NULL)
	{
		configured = desmume::pathsettings::NormalizeDirectory(config->path_roms.get());
	}
	if (!configured.empty() && g_file_test(configured.c_str(), G_FILE_TEST_IS_DIR))
	{
		return g_strdup(configured.c_str());
	}
	return g_strdup(g_get_home_dir());
}

/* Where the backup import/export dialogs start. */
char *
desmume_config_sram_directory(desmume::config::Config *config)
{
	std::string configured;
	if (config != NULL)
	{
		configured = desmume::pathsettings::NormalizeDirectory(
			config->path_sram_import_export.get());
	}
	if (!configured.empty() && g_file_test(configured.c_str(), G_FILE_TEST_IS_DIR))
	{
		return g_strdup(configured.c_str());
	}
	return g_strdup(g_get_home_dir());
}

void
desmume_config_note_rom_dir(desmume::config::Config *config, const char *rom_path)
{
	if ((config == NULL) || (rom_path == NULL) ||
	    !config->path_save_last_visit.get())
	{
		return;
	}

	char *dir = g_path_get_dirname(rom_path);
	char *normalized = g_strdup(
		desmume::pathsettings::NormalizeDirectory(dir).c_str());
	g_free(dir);

	if (normalized != NULL)
	{
		config->path_roms = normalized;
		path.setpath(PathInfo::ROMS, normalized);
		config->save();
	}
	g_free(normalized);
}

char *
desmume_config_screenshot_path(desmume::config::Config *config,
                               const char *rom_base_name)
{
	/* Destination folder: the Screenshots setting, else the XDG pictures
	 * folder, else the home folder. */
	std::string directory;
	if (config != NULL)
	{
		directory = desmume::pathsettings::NormalizeDirectory(
			config->path_screenshots.get());
	}
	if (directory.empty() || !g_file_test(directory.c_str(), G_FILE_TEST_IS_DIR))
	{
		const gchar *pictures = g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);
		directory = (pictures != NULL) ? pictures : g_get_home_dir();
	}

	/* File name: the configured pattern (strftime plus DeSmuME codes), or the
	 * old desmume-screenshot-N scheme.  The counter only advances when the
	 * default naming is used, so a %t/%f pattern stays unique on its own. */
	static unsigned int sequence = 1;
	std::string name;
	const std::string pattern = (config != NULL)
		? config->path_screenshot_format.get() : std::string();

	if (pattern.empty())
	{
		char fallback[64];
		g_snprintf(fallback, sizeof(fallback), "desmume-screenshot-%u", sequence++);
		name = fallback;
	}
	else
	{
		static const char *kStrftimeCodes = "AbBcCdDeFgGhHIjmMnpRStTuUVwWxXyYzZ%";
		const time_t now = time(NULL);
		const struct tm *parts = localtime(&now);

		for (size_t i = 0; i < pattern.size(); ++i)
		{
			if (pattern[i] != '%')
			{
				name += pattern[i];
				continue;
			}

			if (++i >= pattern.size())
			{
				break;
			}

			const char code = pattern[i];
			if (code == 'f')
			{
				name += (rom_base_name != NULL) ? rom_base_name : "";
			}
			else if (code == 'r')
			{
				name += std::to_string(g_random_int());
			}
			else if (code == 't')
			{
				name += std::to_string((unsigned long)(g_get_monotonic_time() / 1000));
			}
			else if ((parts != NULL) && (strchr(kStrftimeCodes, code) != NULL))
			{
				char spec[3] = { '%', code, '\0' };
				char text[128];
				if (strftime(text, sizeof(text), spec, parts) > 0)
				{
					name += text;
				}
			}
		}

		if (name.empty())
		{
			name = "desmume-screenshot";
		}

		/* A name, not a path: a pattern must not escape the folder. */
		for (size_t i = 0; i < name.size(); ++i)
		{
			if ((name[i] == '/') || (name[i] == '\\'))
			{
				name[i] = '-';
			}
		}
	}

	/* Extension follows the image format.  GdkPixbuf calls the writer "jpeg"
	 * while the usual suffix is ".jpg". */
	std::string format = (config != NULL)
		? config->path_screenshot_image_format.get() : "png";
	if (format.empty())
	{
		format = "png";
	}
	const char *extension = (format == "jpeg") ? ".jpg" : NULL;
	char *result;
	if (extension != NULL)
	{
		char *base = g_strdup_printf("%s%s", name.c_str(), extension);
		result = g_build_filename(directory.c_str(), base, NULL);
		g_free(base);
	}
	else
	{
		char *base = g_strdup_printf("%s.%s", name.c_str(), format.c_str());
		result = g_build_filename(directory.c_str(), base, NULL);
		g_free(base);
	}

	return result;
}

void
desmume_config_dialog_show_page(GtkWindow *parent,
                                desmume::config::Config *config,
                                DesmumeConfigAppliedFunc applied,
                                gpointer user_data,
                                int page)
{
	if (sDialog != NULL)
	{
		if ((page >= 0) && (sDialog->notebook != NULL) &&
		    (page < gtk_notebook_get_n_pages(GTK_NOTEBOOK(sDialog->notebook))))
		{
			gtk_notebook_set_current_page(GTK_NOTEBOOK(sDialog->notebook), page);
		}
		gtk_window_present(GTK_WINDOW(sDialog->window));
		return;
	}

	if (page < 0)
	{
		page = 0;
	}

	ConfigDialog *d = g_new0(ConfigDialog, 1);
	d->config = config;
	d->applied = applied;
	d->appliedData = user_data;
	d->parentWindow = parent;

	d->window = gtk_window_new();
	gtk_window_set_title(GTK_WINDOW(d->window), "DeSmuME Configuration");
	gtk_window_set_transient_for(GTK_WINDOW(d->window), parent);
	gtk_window_set_modal(GTK_WINDOW(d->window), TRUE);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(d->window), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(d->window), 680, 660);

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_window_set_child(GTK_WINDOW(d->window), root);

	GtkWidget *notebook = gtk_notebook_new();
	gtk_widget_set_vexpand(notebook, TRUE);
	gtk_box_append(GTK_BOX(root), notebook);
	d->notebook = notebook;

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
	                         WrapPage(BuildEmulationTab(d)),
	                         gtk_label_new("Emulation"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
	                         WrapPage(BuildGraphicTab(d)),
	                         gtk_label_new("Graphic"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
	                         WrapPage(BuildSoundTab(d)),
	                         gtk_label_new("Sound"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
	                         WrapPage(BuildFirmwarePage(d)),
	                         gtk_label_new("Firmware"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
	                         WrapPage(BuildSlotsPage(d)),
	                         gtk_label_new("Slots"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
	                         WrapPage(BuildWifiPage(d)),
	                         gtk_label_new("Wi-Fi"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
	                         WrapPage(BuildMicPage(d)),
	                         gtk_label_new("Microphone"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
	                         WrapPage(BuildPathsPage(d)),
	                         gtk_label_new("Paths"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
	                         WrapPage(BuildControlsPage(d)),
	                         gtk_label_new("Controls"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
	                         WrapPage(BuildHotkeysPage(d)),
	                         gtk_label_new("Hotkeys"));
	GtkEventController *shortcutCapture = gtk_event_controller_key_new();
	gtk_event_controller_set_propagation_phase(shortcutCapture, GTK_PHASE_CAPTURE);
	g_signal_connect(shortcutCapture, "key-pressed", G_CALLBACK(OnKeyCapture), d);
	gtk_widget_add_controller(d->window, shortcutCapture);

	GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_set_halign(buttons, GTK_ALIGN_END);
	gtk_widget_set_margin_start(buttons, 12);
	gtk_widget_set_margin_end(buttons, 12);
	gtk_widget_set_margin_top(buttons, 8);
	gtk_widget_set_margin_bottom(buttons, 12);
	gtk_box_append(GTK_BOX(root), buttons);

	GtkWidget *cancel = gtk_button_new_with_label("_Cancel");
	gtk_button_set_use_underline(GTK_BUTTON(cancel), TRUE);
	g_signal_connect(cancel, "clicked", G_CALLBACK(OnCancelClicked), d);
	gtk_box_append(GTK_BOX(buttons), cancel);

	GtkWidget *ok = gtk_button_new_with_label("_OK");
	gtk_button_set_use_underline(GTK_BUTTON(ok), TRUE);
	gtk_widget_add_css_class(ok, "suggested-action");
	g_signal_connect(ok, "clicked", G_CALLBACK(OnApplyClicked), d);
	gtk_box_append(GTK_BOX(buttons), ok);
	gtk_window_set_default_widget(GTK_WINDOW(d->window), ok);

	/* Publish the dialog before Populate() so the singleton guard and the
	 * destroy handler see a consistent state; the window is presented last. */
	sDialog = d;
	Populate(d);

	const int page_count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
	if (page < page_count)
	{
		gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), page);
	}

	/* GTK owns the window's only reference.  Adding a ref_sink here would
	 * keep it alive after gtk_window_destroy() and stop "destroy" from ever
	 * firing, so sDialog would dangle and the next show() would present a
	 * destroyed window. */
	g_signal_connect(d->window, "destroy", G_CALLBACK(OnDialogDestroy), d);
	gtk_window_present(GTK_WINDOW(d->window));
}

void
desmume_config_dialog_show(GtkWindow *parent,
                           desmume::config::Config *config,
                           DesmumeConfigAppliedFunc applied,
                           gpointer user_data)
{
	desmume_config_dialog_show_page(parent, config, applied, user_data, 0);
}
