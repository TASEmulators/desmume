/*
	pathsettings.cpp - Path Settings dialog for the GTK3 frontend

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

#include "pathsettings.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "../../shared/path_settings_core.h"
#include "config.h"

extern desmume::config::Config config;

namespace {

using desmume::pathsettings::CategoryInfo;
using desmume::pathsettings::NormalizeDirectory;

/* The rows this port exposes.  Firmware, sound samples and the Slot 1 debug
 * folder are file paths owned by the Emulation, Microphone and Slot 1
 * dialogs, so repeating them here would only create two sources of truth. */
const PathInfo::KnownPath kDialogCategories[] =
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

struct PathRow
{
	PathInfo::KnownPath category;
	GtkWidget *entry;
};

/* Unlike a folder, a file name pattern keeps its interior spacing; only the
 * accidental leading/trailing whitespace is dropped. */
std::string Trim(const std::string &input)
{
	static const char *kWhitespace = " \t\r\n";

	const size_t first = input.find_first_not_of(kWhitespace);
	if (first == std::string::npos)
		return std::string();

	const size_t last = input.find_last_not_of(kWhitespace);
	return input.substr(first, last - first + 1);
}

std::string CategoryValue(PathInfo::KnownPath category)
{
	switch (category)
	{
		case PathInfo::ROMS:               return config.path_roms.get();
		case PathInfo::BATTERY:            return config.path_battery.get();
		case PathInfo::SRAM_IMPORT_EXPORT: return config.path_sram_import_export.get();
		case PathInfo::STATES:             return config.path_states.get();
		case PathInfo::STATE_SLOTS:        return config.path_state_slots.get();
		case PathInfo::SCREENSHOTS:        return config.path_screenshots.get();
		case PathInfo::AVI_FILES:          return config.path_avi_files.get();
		/* The Play/Record dialogs already remember their folder here, so the
		 * Movies row edits that instead of adding a second setting. */
		case PathInfo::MOVIES:             return config.movie_directory.get();
		case PathInfo::CHEATS:             return config.path_cheats.get();
		case PathInfo::LUA:                return config.path_lua.get();
		default:                           return std::string();
	}
}

void SetCategoryValue(PathInfo::KnownPath category, const std::string &value)
{
	switch (category)
	{
		case PathInfo::ROMS:               config.path_roms.set(value); break;
		case PathInfo::BATTERY:            config.path_battery.set(value); break;
		case PathInfo::SRAM_IMPORT_EXPORT: config.path_sram_import_export.set(value); break;
		case PathInfo::STATES:             config.path_states.set(value); break;
		case PathInfo::STATE_SLOTS:        config.path_state_slots.set(value); break;
		case PathInfo::SCREENSHOTS:        config.path_screenshots.set(value); break;
		case PathInfo::AVI_FILES:          config.path_avi_files.set(value); break;
		case PathInfo::MOVIES:             config.movie_directory.set(value); break;
		case PathInfo::CHEATS:             config.path_cheats.set(value); break;
		case PathInfo::LUA:                config.path_lua.set(value); break;
		default: break;
	}
}

/* What the category falls back to when the entry is left empty.  Shown as the
 * placeholder so the dialog answers "then where does it go?" without a doc. */
std::string DefaultHint(PathInfo::KnownPath category)
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

std::vector<FormatChoice> WritableFormats()
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

bool IsWritableFormat(const std::string &name)
{
	const std::vector<FormatChoice> formats = WritableFormats();
	for (size_t i = 0; i < formats.size(); i++)
	{
		if (formats[i].name == name)
			return true;
	}

	return false;
}

void BrowseClicked(GtkWidget *widget, gpointer data)
{
	GtkWidget *entry = GTK_WIDGET(data);
	GtkWindow *parent = GTK_WINDOW(gtk_widget_get_toplevel(entry));

	GtkFileChooserNative *chooser = gtk_file_chooser_native_new(
		"Select Folder", parent, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		"_Select", "_Cancel");

	const char *current = gtk_entry_get_text(GTK_ENTRY(entry));
	if ((current != NULL) && g_file_test(current, G_FILE_TEST_IS_DIR))
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(chooser), current);

	if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT)
	{
		GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(chooser));
		gchar *folder = g_file_get_path(file);

		if (folder != NULL)
		{
			gtk_entry_set_text(GTK_ENTRY(entry), folder);
			g_free(folder);
		}

		g_object_unref(file);
	}

	g_object_unref(chooser);
}

void ShowError(GtkWindow *parent, const std::string &message)
{
	GtkWidget *dialog = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL,
	                                           GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
	                                           "%s", message.c_str());
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

void AddRow(GtkGrid *grid, int row, const CategoryInfo *info, PathRow *outRow)
{
	GtkWidget *label = gtk_label_new(info->label);
	gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
	gtk_grid_attach(grid, label, 0, row, 1, 1);

	GtkWidget *entry = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 40);
	gtk_widget_set_hexpand(entry, TRUE);
	gtk_entry_set_text(GTK_ENTRY(entry), path_settings_directory(info->category).c_str());
	gtk_entry_set_placeholder_text(GTK_ENTRY(entry), DefaultHint(info->category).c_str());
	gtk_widget_set_tooltip_text(entry, info->description);
	gtk_grid_attach(grid, entry, 1, row, 1, 1);

	GtkWidget *browse = gtk_button_new_with_label("Browse...");
	gtk_grid_attach(grid, browse, 2, row, 1, 1);

	g_signal_connect(G_OBJECT(browse), "clicked", G_CALLBACK(BrowseClicked), entry);

	outRow->category = info->category;
	outRow->entry = entry;
}

} /* namespace */

void path_settings_apply_config()
{
	for (size_t i = 0; i < G_N_ELEMENTS(kDialogCategories); i++)
	{
		const PathInfo::KnownPath category = kDialogCategories[i];

		/* Movies lives in [Movie] Directory and is read by the Play/Record
		 * dialogs directly, so the path model does not need it. */
		if (category == PathInfo::MOVIES)
			continue;

		path.setpath(category, path_settings_directory(category));
	}
}

std::string path_settings_directory(PathInfo::KnownPath category)
{
	return NormalizeDirectory(CategoryValue(category));
}

std::string path_settings_screenshot_image_format()
{
	const std::string format = config.path_screenshot_image_format.get();
	return IsWritableFormat(format) ? format : "png";
}

std::string path_settings_screenshot_extension()
{
	const std::string format = path_settings_screenshot_image_format();

	/* GdkPixbuf calls the writer "jpeg" but the usual suffix is ".jpg". */
	if (format == "jpeg")
		return ".jpg";

	return "." + format;
}

std::string path_settings_screenshot_directory()
{
	const std::string configured = path_settings_directory(PathInfo::SCREENSHOTS);
	if (!configured.empty())
		return configured;

	const gchar *dir = g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);
	if (dir == NULL)
		dir = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
	if (dir == NULL)
		dir = g_get_home_dir();
	if (dir == NULL)
		dir = ".";

	return dir;
}

std::string path_settings_screenshot_name(const std::string &romBaseName,
                                          unsigned int sequence)
{
	const std::string pattern = config.path_screenshot_format.get();

	if (pattern.empty())
		return "desmume-screenshot-" + std::to_string(sequence);

	/* Same codes the Windows dialog documents: strftime, plus %f for the ROM
	 * name, %r for a random number and %t for a tick. */
	static const char *kStrftimeCodes = "AbBcCdDeFgGhHIjmMnpRStTuUVwWxXyYzZ%";
	const time_t now = time(NULL);
	const struct tm *parts = localtime(&now);

	std::string name;

	for (size_t i = 0; i < pattern.size(); i++)
	{
		if (pattern[i] != '%')
		{
			name += pattern[i];
			continue;
		}

		if (++i >= pattern.size())
			break;

		const char code = pattern[i];
		if (code == 'f')
		{
			name += romBaseName;
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
				name += text;
		}
	}

	if (name.empty())
		return "desmume-screenshot-" + std::to_string(sequence);

	/* A name, not a path: a pattern must not be able to escape the folder. */
	std::replace(name.begin(), name.end(), '/', '-');
#ifdef HOST_WINDOWS
	std::replace(name.begin(), name.end(), '\\', '-');
#endif

	return name;
}

void path_settings_dialog_show(GtkWindow *parent)
{
	GtkWidget *dialog = gtk_dialog_new_with_buttons(
		"Path Settings", parent,
		GTK_DIALOG_MODAL,
		"_Cancel", GTK_RESPONSE_CANCEL,
		"_OK", GTK_RESPONSE_OK,
		NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
	gtk_dialog_add_button(GTK_DIALOG(dialog), "Reset to Defaults", GTK_RESPONSE_APPLY);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_container_set_border_width(GTK_CONTAINER(content), 12);

	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_box_pack_start(GTK_BOX(content), box, TRUE, TRUE, 0);

	GtkWidget *foldersFrame = gtk_frame_new("Folders");
	GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
	                               GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(scroller, 600, 330);

	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_widget_set_margin_start(grid, 10);
	gtk_widget_set_margin_end(grid, 10);
	gtk_widget_set_margin_top(grid, 10);
	gtk_widget_set_margin_bottom(grid, 10);

	PathRow rows[G_N_ELEMENTS(kDialogCategories)];
	for (size_t i = 0; i < G_N_ELEMENTS(kDialogCategories); i++)
	{
		const CategoryInfo *info =
			desmume::pathsettings::FindCategory(kDialogCategories[i]);
		if (info != NULL)
			AddRow(GTK_GRID(grid), (int)i, info, &rows[i]);
		else
			rows[i].entry = NULL;
	}

	gtk_container_add(GTK_CONTAINER(scroller), grid);
	gtk_container_add(GTK_CONTAINER(foldersFrame), scroller);
	gtk_box_pack_start(GTK_BOX(box), foldersFrame, TRUE, TRUE, 0);

	/* Screenshot options, matching the Windows dialog's second half. */
	GtkWidget *shotFrame = gtk_frame_new("Screenshots");
	GtkWidget *shotGrid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(shotGrid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(shotGrid), 8);
	gtk_widget_set_margin_start(shotGrid, 10);
	gtk_widget_set_margin_end(shotGrid, 10);
	gtk_widget_set_margin_top(shotGrid, 10);
	gtk_widget_set_margin_bottom(shotGrid, 10);

	GtkWidget *formatLabel = gtk_label_new("Image format");
	gtk_label_set_xalign(GTK_LABEL(formatLabel), 1.0f);
	gtk_grid_attach(GTK_GRID(shotGrid), formatLabel, 0, 0, 1, 1);

	GtkWidget *formatCombo = gtk_combo_box_text_new();
	const std::vector<FormatChoice> formats = WritableFormats();
	const std::string currentFormat = path_settings_screenshot_image_format();
	int pngIndex = 0;
	for (size_t i = 0; i < formats.size(); i++)
	{
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(formatCombo),
		                          formats[i].name.c_str(), formats[i].label.c_str());
		if (formats[i].name == currentFormat)
			gtk_combo_box_set_active(GTK_COMBO_BOX(formatCombo), (int)i);
		if (formats[i].name == "png")
			pngIndex = (int)i;
	}
	gtk_widget_set_hexpand(formatCombo, TRUE);
	gtk_grid_attach(GTK_GRID(shotGrid), formatCombo, 1, 0, 2, 1);

	GtkWidget *nameLabel = gtk_label_new("File name");
	gtk_label_set_xalign(GTK_LABEL(nameLabel), 1.0f);
	gtk_grid_attach(GTK_GRID(shotGrid), nameLabel, 0, 1, 1, 1);

	GtkWidget *nameEntry = gtk_entry_new();
	gtk_widget_set_hexpand(nameEntry, TRUE);
	gtk_entry_set_text(GTK_ENTRY(nameEntry), config.path_screenshot_format.get().c_str());
	gtk_entry_set_placeholder_text(GTK_ENTRY(nameEntry), "desmume-screenshot-1");
	gtk_widget_set_tooltip_text(nameEntry,
		"Pattern for the screenshot file name (strftime, plus DeSmuME codes).\n"
		"%f\tROM name\n"
		"%r\tRandom number\n"
		"%t\tTick (resets on startup)\n"
		"%Y %y %m %d %H %M %S\tDate and time\n"
		"Leave empty for desmume-screenshot-N.");
	gtk_grid_attach(GTK_GRID(shotGrid), nameEntry, 1, 1, 2, 1);

	GtkWidget *hint = gtk_label_new("Saved to the Screenshots folder above; "
	                                "the extension follows the image format.");
	gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
	gtk_label_set_line_wrap(GTK_LABEL(hint), TRUE);
	gtk_widget_set_sensitive(hint, FALSE);
	gtk_grid_attach(GTK_GRID(shotGrid), hint, 1, 2, 2, 1);

	gtk_container_add(GTK_CONTAINER(shotFrame), shotGrid);
	gtk_box_pack_start(GTK_BOX(box), shotFrame, FALSE, FALSE, 0);

	GtkWidget *lastVisit = gtk_check_button_new_with_label(
		"Remember the folder of the last opened ROM");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lastVisit),
	                             config.path_save_last_visit.get());
	gtk_box_pack_start(GTK_BOX(box), lastVisit, FALSE, FALSE, 0);

	gtk_widget_show_all(content);

	while (TRUE)
	{
		const gint response = gtk_dialog_run(GTK_DIALOG(dialog));

		if (response == GTK_RESPONSE_APPLY)
		{
			/* Reset the widgets only; nothing is written until OK. */
			for (size_t i = 0; i < G_N_ELEMENTS(rows); i++)
			{
				if (rows[i].entry != NULL)
					gtk_entry_set_text(GTK_ENTRY(rows[i].entry), "");
			}

			gtk_entry_set_text(GTK_ENTRY(nameEntry), "");
			gtk_combo_box_set_active(GTK_COMBO_BOX(formatCombo), pngIndex);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lastVisit), TRUE);
			continue;
		}

		if (response != GTK_RESPONSE_OK)
			break;

		/* Refuse folders that do not exist: silently accepting a typo would
		 * send every later file there and only fail much later. */
		bool valid = true;
		for (size_t i = 0; i < G_N_ELEMENTS(rows); i++)
		{
			if (rows[i].entry == NULL)
				continue;

			const std::string value = NormalizeDirectory(
				gtk_entry_get_text(GTK_ENTRY(rows[i].entry)));

			if (!value.empty() && !g_file_test(value.c_str(), G_FILE_TEST_IS_DIR))
			{
				ShowError(parent, "This folder does not exist:\n" + value);
				valid = false;
				break;
			}
		}

		if (!valid)
			continue;

		for (size_t i = 0; i < G_N_ELEMENTS(rows); i++)
		{
			if (rows[i].entry == NULL)
				continue;

			SetCategoryValue(rows[i].category, NormalizeDirectory(
				gtk_entry_get_text(GTK_ENTRY(rows[i].entry))));
		}

		const char *formatId = gtk_combo_box_get_active_id(GTK_COMBO_BOX(formatCombo));
		config.path_screenshot_image_format.set((formatId != NULL) ? formatId : "png");
		config.path_screenshot_format.set(
			Trim(gtk_entry_get_text(GTK_ENTRY(nameEntry))));
		config.path_save_last_visit.set(
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lastVisit)));

		/* Push the new folders into the path model before saving, so the
		 * current session already uses them. */
		path_settings_apply_config();
		config.save();
		break;
	}

	gtk_widget_destroy(dialog);
}
