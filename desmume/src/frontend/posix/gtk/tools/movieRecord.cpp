/* movieRecord.cpp - this file is part of DeSmuME
 *
 * Copyright (C) 2006-2026 DeSmuME Team
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2, or (at your option)
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

// GTK3 host for the Record Movie dialog. All decisions live in
// ../../../shared/movie_info_core.cpp (unique names, .dsm extension, current
// emulation flags, default RTC), shared with Windows and Cocoa.
// Preferences are persisted by config.cpp (GKeyFile).

#include <gtk/gtk.h>

#include <stdio.h>
#include <string.h>

#include <string>

#include "movieRecord.h"
#include "../config.h"
#include "../main.h"
#include "../desmume.h"
#include "../../../movie.h"
#include "../../../path.h"
#include "../../../utils/datetime.h"
#include "../../../utils/xstring.h"
#include "../../../frontend/shared/movie_info_core.h"

extern desmume::config::Config config;

namespace
{
	struct RecordDialog
	{
		GtkWidget *window;
		GtkWidget *pathEntry;
		GtkWidget *browseButton;
		GtkWidget *authorEntry;
		GtkWidget *commentView;
		GtkTextBuffer *commentBuffer;
		GtkWidget *dateWidget;
		GtkWidget *dateCalendar;
		GtkWidget *hourWidget;
		GtkWidget *minuteWidget;
		GtkWidget *secondWidget;
		GtkWidget *powerRadio;
		GtkWidget *sramRadio;
		GtkWidget *snapshotRadio;
		GtkWidget *sramEntry;
		GtkWidget *sramBrowse;
		GtkWidget *okButton;

		GtkWidget *romInfoLabel;
		GtkWidget *systemLabel;
		GtkWidget *firmwareLabel;
		GtkWidget *languageLabel;
		GtkWidget *biosExtCheck;
		GtkWidget *biosSwiCheck;
		GtkWidget *firmExtCheck;
		GtkWidget *bootFwCheck;

		RecordDialog()
			: window(NULL), commentBuffer(NULL)
		{}
	};

	static std::string movies_directory()
	{
		const std::string configured = config.movie_directory.get();
		if (!configured.empty())
			return configured;
		std::string dir = path.getpath(path.MOVIES);
		if (!dir.empty())
			return dir;
		const gchar *home = g_get_home_dir();
		return std::string(home ? home : ".") + "/Movies";
	}

	static std::string entry_text(GtkWidget *entry)
	{
		const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));
		return text ? std::string(text) : std::string();
	}

	static MovieInfo::StartFrom start_from(RecordDialog *d)
	{
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(d->sramRadio)))
			return MovieInfo::kStartSram;
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(d->snapshotRadio)))
			return MovieInfo::kStartSavestate;
		return MovieInfo::kStartBlank;
	}

	// The OK button mirrors Windows' IsFileWritable(): it must be possible to
	// create/overwrite the chosen file, and the parent directory must exist.
	static void update_ok(RecordDialog *d)
	{
		const std::string text = entry_text(d->pathEntry);
		bool usable = false;
		if (!text.empty())
		{
			const std::string full = text.find_first_of("\\/") == std::string::npos
				? movies_directory() + "/" + text : text;
			GFile *file = g_file_new_for_path(full.c_str());
			GFile *parent = g_file_get_parent(file);
			if (parent != NULL)
			{
				GFileInfo *info = g_file_query_info(parent, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
					G_FILE_QUERY_INFO_NONE, NULL, NULL);
				if (info != NULL)
				{
					usable = g_file_info_get_attribute_boolean(info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE) != FALSE;
					g_object_unref(info);
				}
				g_object_unref(parent);
			}
			if (file != NULL)
				g_object_unref(file);
		}
		gtk_widget_set_sensitive(d->okButton, usable ? TRUE : FALSE);
	}

	static void on_path_changed(GtkWidget *widget, gpointer data)
	{
		update_ok(static_cast<RecordDialog *>(data));
	}

	static void on_start_changed(GtkWidget *widget, gpointer data)
	{
		RecordDialog *d = static_cast<RecordDialog *>(data);
		const bool useSram = start_from(d) == MovieInfo::kStartSram;
		gtk_widget_set_sensitive(d->sramEntry, useSram ? TRUE : FALSE);
		gtk_widget_set_sensitive(d->sramBrowse, useSram ? TRUE : FALSE);
	}

	static void on_browse(GtkWidget *widget, gpointer data)
	{
		RecordDialog *d = static_cast<RecordDialog *>(data);
		GtkFileChooserNative *chooser = gtk_file_chooser_native_new(
			"Save Movie To", GTK_WINDOW(d->window), GTK_FILE_CHOOSER_ACTION_SAVE, "_Save", "_Cancel");
		gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(chooser), TRUE);
		GtkFileFilter *filter = gtk_file_filter_new();
		gtk_file_filter_add_pattern(filter, "*.dsm*");
		gtk_file_filter_set_name(filter, "DeSmuME movie file (.dsm*)");
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), movies_directory().c_str());
		const std::string current = entry_text(d->pathEntry);
		if (!current.empty())
			gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser), current.c_str());

		if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT)
		{
			GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(chooser));
			gchar *path = g_file_get_path(file);
			if (path != NULL)
			{
				gtk_entry_set_text(GTK_ENTRY(d->pathEntry), path);
				g_free(path);
			}
			if (file != NULL)
				g_object_unref(file);
		}
		g_object_unref(chooser);
	}

	static void on_browse_sram(GtkWidget *widget, gpointer data)
	{
		RecordDialog *d = static_cast<RecordDialog *>(data);
		GtkFileChooserNative *chooser = gtk_file_chooser_native_new(
			"Select SRAM file", GTK_WINDOW(d->window), GTK_FILE_CHOOSER_ACTION_OPEN, "_Open", "_Cancel");
		GtkFileFilter *filter = gtk_file_filter_new();
		gtk_file_filter_add_pattern(filter, "*.dsv*");
		gtk_file_filter_set_name(filter, "DeSmuME SRAM file (.dsv*)");
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);
		if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT)
		{
			GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(chooser));
			gchar *path = g_file_get_path(file);
			if (path != NULL)
			{
				gtk_entry_set_text(GTK_ENTRY(d->sramEntry), path);
				g_free(path);
			}
			if (file != NULL)
				g_object_unref(file);
		}
		g_object_unref(chooser);
	}

	static void update_date_button(RecordDialog *d)
	{
		guint year = 2009, month = 1, day = 1;
		gtk_calendar_get_date(GTK_CALENDAR(d->dateCalendar), &year, &month, &day);
		gchar *text = g_strdup_printf("%04u-%02u-%02u", year, month + 1, day);
		gtk_button_set_label(GTK_BUTTON(d->dateWidget), text);
		g_free(text);
	}

	static void on_date_changed(GtkCalendar *, gpointer data)
	{
		update_date_button(static_cast<RecordDialog *>(data));
	}

	static void on_date_selected(GtkCalendar *, gpointer data)
	{
		RecordDialog *d = static_cast<RecordDialog *>(data);
		update_date_button(d);
		GtkPopover *popover = gtk_menu_button_get_popover(GTK_MENU_BUTTON(d->dateWidget));
		if (popover != NULL)
			gtk_popover_popdown(popover);
	}

	static gboolean on_time_spin_output(GtkSpinButton *spin, gpointer)
	{
		gchar *text = g_strdup_printf("%02d", gtk_spin_button_get_value_as_int(spin));
		gtk_entry_set_text(GTK_ENTRY(spin), text);
		g_free(text);
		return TRUE;
	}

	static GtkWidget *add_time_spin(GtkWidget *box, int max, int value)
	{
		GtkWidget *spin = gtk_spin_button_new_with_range(0, max, 1);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), value);
		gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
		gtk_entry_set_width_chars(GTK_ENTRY(spin), 2);
		gtk_entry_set_max_length(GTK_ENTRY(spin), 2);
		gtk_widget_set_valign(spin, GTK_ALIGN_CENTER);
		g_signal_connect(spin, "output", G_CALLBACK(on_time_spin_output), NULL);
		gtk_box_pack_start(GTK_BOX(box), spin, FALSE, FALSE, 0);
		return spin;
	}

	static DateTime rtc_from_widgets(RecordDialog *d)
	{
		guint year = 2009, month = 1, day = 1;
		gtk_calendar_get_date(GTK_CALENDAR(d->dateCalendar), &year, &month, &day);
		month += 1;  // GtkCalendar months are 0-based; DateTime wants 1-12.
		const int hour = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(d->hourWidget));
		const int minute = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(d->minuteWidget));
		const int second = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(d->secondWidget));
		return DateTime((int)year, (int)month, (int)day, hour, minute, second);
	}

	static void on_response(GtkDialog *dialog, gint response, gpointer data)
	{
		RecordDialog *d = static_cast<RecordDialog *>(data);
		if (response != GTK_RESPONSE_OK)
			return;

		std::string text = entry_text(d->pathEntry);
		if (text.find_first_of("\\/") == std::string::npos)
			text = movies_directory() + "/" + text;
		const std::string filename = MovieInfo::EnsureDsmExtension(text);

		const MovieInfo::StartFrom sf = start_from(d);
		std::string sramPath;
		if (sf == MovieInfo::kStartSram)
			sramPath = entry_text(d->sramEntry);  // empty when the user did not pick one

		const std::string author = entry_text(d->authorEntry);
		GtkTextIter start, end;
		gtk_text_buffer_get_bounds(d->commentBuffer, &start, &end);
		gchar *comment = gtk_text_buffer_get_text(d->commentBuffer, &start, &end, FALSE);
		const std::string description = comment ? comment : "";
		if (comment != NULL)
			g_free(comment);

		config.movie_directory.set(movies_directory());
		config.movie_author.set(author);

		const char *error = FCEUI_SaveMovie(filename.c_str(), mbstowcs(author),
			(START_FROM)sf, sramPath, rtc_from_widgets(d), mbstowcs(description));
		if (error != NULL)
		{
			GtkWidget *msg = gtk_message_dialog_new(GTK_WINDOW(dialog),
				GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", error);
			gtk_dialog_run(GTK_DIALOG(msg));
			gtk_widget_destroy(msg);
		}
	}

	static GtkWidget *add_label(GtkGrid *grid, const char *text, int x, int y, int w)
	{
		GtkWidget *label = gtk_label_new(text);
		gtk_widget_set_halign(label, GTK_ALIGN_START);
		gtk_grid_attach(grid, label, x, y, w, 1);
		return label;
	}

	static GtkWidget *add_field(GtkGrid *grid, int x, int y, int w)
	{
		GtkWidget *label = gtk_label_new("-");
		gtk_widget_set_halign(label, GTK_ALIGN_START);
		gtk_widget_set_hexpand(label, FALSE);
		gtk_grid_attach(grid, label, x, y, w, 1);
		return label;
	}

	static void fill_read_only_fields(RecordDialog *d)
	{
		std::string romName = path.GetRomNameWithoutExtension();
		if (romName.empty())
			romName = "-";
		gtk_label_set_text(GTK_LABEL(d->romInfoLabel), ("ROM: " + romName).c_str());

		const MovieInfo::EmulationFlags flags = MovieInfo::CurrentEmulationFlags();
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->biosExtCheck), flags.useExtBios ? TRUE : FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->biosSwiCheck), flags.swiFromBios ? TRUE : FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->firmExtCheck), flags.useExtFirmware ? TRUE : FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->bootFwCheck), flags.bootFromFirmware ? TRUE : FALSE);
		gtk_label_set_text(GTK_LABEL(d->systemLabel), "Nintendo DS");
		gtk_label_set_text(GTK_LABEL(d->firmwareLabel),
			flags.useExtFirmware ? "External file" : "Built-in");
	}

	static void build_window(RecordDialog *d, GtkWindow *parent)
	{
		d->window = gtk_dialog_new_with_buttons("Record Movie", parent,
			(GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
			"_Cancel", GTK_RESPONSE_CANCEL, "_Record", GTK_RESPONSE_OK, NULL);
		gtk_window_set_default_size(GTK_WINDOW(d->window), 620, 520);
		d->okButton = gtk_dialog_get_widget_for_response(GTK_DIALOG(d->window), GTK_RESPONSE_OK);
		g_signal_connect(d->window, "response", G_CALLBACK(on_response), d);

		GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(d->window));
		GtkWidget *grid = gtk_grid_new();
		gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
		gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
		gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
		gtk_container_add(GTK_CONTAINER(content), grid);

		int row = 0;
		add_label(GTK_GRID(grid), "Movie file:", 0, row, 1);
		d->pathEntry = gtk_entry_new();
		gtk_widget_set_hexpand(d->pathEntry, TRUE);
		gtk_grid_attach(GTK_GRID(grid), d->pathEntry, 1, row, 3, 1);
		g_signal_connect(d->pathEntry, "changed", G_CALLBACK(on_path_changed), d);
		d->browseButton = gtk_button_new_with_label("Browse...");
		gtk_grid_attach(GTK_GRID(grid), d->browseButton, 4, row, 1, 1);
		g_signal_connect(d->browseButton, "clicked", G_CALLBACK(on_browse), d);
		row++;

		add_label(GTK_GRID(grid), "Author:", 0, row, 1);
		d->authorEntry = gtk_entry_new();
		gtk_widget_set_hexpand(d->authorEntry, TRUE);
		gtk_grid_attach(GTK_GRID(grid), d->authorEntry, 1, row, 4, 1);
		row++;

		add_label(GTK_GRID(grid), "Comment:", 0, row, 1);
		GtkWidget *commentScroll = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(commentScroll), GTK_SHADOW_ETCHED_IN);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(commentScroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_widget_set_hexpand(commentScroll, FALSE);
		gtk_grid_attach(GTK_GRID(grid), commentScroll, 1, row, 4, 1);
		d->commentView = gtk_text_view_new();
		gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(d->commentView), GTK_WRAP_WORD);
		d->commentBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(d->commentView));
		gtk_container_add(GTK_CONTAINER(commentScroll), d->commentView);
		row++;

		// Start options with SRAM picker.
		GtkWidget *startFrame = gtk_frame_new("Record start");
		GtkWidget *startGrid = gtk_grid_new();
		gtk_grid_set_row_spacing(GTK_GRID(startGrid), 4);
		gtk_grid_set_column_spacing(GTK_GRID(startGrid), 8);
		gtk_container_set_border_width(GTK_CONTAINER(startGrid), 8);
		gtk_container_add(GTK_CONTAINER(startFrame), startGrid);
		d->powerRadio = gtk_radio_button_new_with_label(NULL, "Power on (blank SRAM)");
		d->sramRadio = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(d->powerRadio), "Reset + SRAM");
		d->snapshotRadio = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(d->powerRadio), "Current state");
		gtk_grid_attach(GTK_GRID(startGrid), d->powerRadio, 0, 0, 3, 1);
		gtk_grid_attach(GTK_GRID(startGrid), d->sramRadio, 0, 1, 3, 1);
		gtk_grid_attach(GTK_GRID(startGrid), d->snapshotRadio, 0, 2, 3, 1);
		g_signal_connect(d->powerRadio, "toggled", G_CALLBACK(on_start_changed), d);
		g_signal_connect(d->sramRadio, "toggled", G_CALLBACK(on_start_changed), d);
		g_signal_connect(d->snapshotRadio, "toggled", G_CALLBACK(on_start_changed), d);
		add_label(GTK_GRID(startGrid), "SRAM:", 0, 3, 1);
		d->sramEntry = gtk_entry_new();
		gtk_widget_set_hexpand(d->sramEntry, TRUE);
		gtk_grid_attach(GTK_GRID(startGrid), d->sramEntry, 1, 3, 1, 1);
		d->sramBrowse = gtk_button_new_with_label("Browse...");
		gtk_grid_attach(GTK_GRID(startGrid), d->sramBrowse, 2, 3, 1, 1);
		g_signal_connect(d->sramBrowse, "clicked", G_CALLBACK(on_browse_sram), d);
		gtk_grid_attach(GTK_GRID(grid), startFrame, 0, row, 5, 1);
		row++;

		// Start RTC: a compact date button whose calendar opens in a popover,
		// plus separate hour/minute/second spin buttons. An inline GtkCalendar
		// would make this row (and the whole dialog) unnecessarily tall.
		GtkWidget *rtcFrame = gtk_frame_new("Start RTC");
		GtkWidget *rtcGrid = gtk_grid_new();
		gtk_grid_set_column_spacing(GTK_GRID(rtcGrid), 8);
		gtk_container_set_border_width(GTK_CONTAINER(rtcGrid), 8);
		gtk_container_add(GTK_CONTAINER(rtcFrame), rtcGrid);
		const MovieInfo::RTCStamp stamp = MovieInfo::DefaultRTCStamp();
		d->dateWidget = gtk_menu_button_new();
		gtk_widget_set_valign(d->dateWidget, GTK_ALIGN_CENTER);
		GtkWidget *datePopover = gtk_popover_new(d->dateWidget);
		d->dateCalendar = gtk_calendar_new();
		gtk_calendar_select_month(GTK_CALENDAR(d->dateCalendar), (guint)(stamp.month - 1), (guint)stamp.year);
		gtk_calendar_select_day(GTK_CALENDAR(d->dateCalendar), (guint)stamp.day);
		gtk_container_add(GTK_CONTAINER(datePopover), d->dateCalendar);
		gtk_widget_show(d->dateCalendar);  // the popover itself is shown on demand
		gtk_menu_button_set_popover(GTK_MENU_BUTTON(d->dateWidget), datePopover);
		g_signal_connect(d->dateCalendar, "month-changed", G_CALLBACK(on_date_changed), d);
		g_signal_connect(d->dateCalendar, "day-selected", G_CALLBACK(on_date_selected), d);
		update_date_button(d);
		add_label(GTK_GRID(rtcGrid), "Date:", 0, 0, 1);
		gtk_grid_attach(GTK_GRID(rtcGrid), d->dateWidget, 1, 0, 1, 1);
		add_label(GTK_GRID(rtcGrid), "Time (HH:MM:SS):", 2, 0, 1);
		GtkWidget *timeBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
		gtk_widget_set_valign(timeBox, GTK_ALIGN_CENTER);
		d->hourWidget = add_time_spin(timeBox, 23, stamp.hour);
		gtk_box_pack_start(GTK_BOX(timeBox), gtk_label_new(":"), FALSE, FALSE, 0);
		d->minuteWidget = add_time_spin(timeBox, 59, stamp.minute);
		gtk_box_pack_start(GTK_BOX(timeBox), gtk_label_new(":"), FALSE, FALSE, 0);
		d->secondWidget = add_time_spin(timeBox, 59, stamp.second);
		gtk_grid_attach(GTK_GRID(rtcGrid), timeBox, 3, 0, 1, 1);
		gtk_grid_attach(GTK_GRID(grid), rtcFrame, 0, row, 5, 1);
		row++;

		// Read-only ROM info and BIOS/firmware flags.
		d->romInfoLabel = add_label(GTK_GRID(grid), "ROM: -", 0, row++, 5);
		GtkWidget *infoFrame = gtk_frame_new("Emulation settings (read-only)");
		GtkWidget *infoGrid = gtk_grid_new();
		gtk_grid_set_row_spacing(GTK_GRID(infoGrid), 4);
		gtk_grid_set_column_spacing(GTK_GRID(infoGrid), 8);
		gtk_container_set_border_width(GTK_CONTAINER(infoGrid), 8);
		gtk_container_add(GTK_CONTAINER(infoFrame), infoGrid);
		add_label(GTK_GRID(infoGrid), "System:", 0, 0, 1);
		d->systemLabel = add_field(GTK_GRID(infoGrid), 1, 0, 1);
		add_label(GTK_GRID(infoGrid), "Firmware:", 0, 1, 1);
		d->firmwareLabel = add_field(GTK_GRID(infoGrid), 1, 1, 1);
		add_label(GTK_GRID(infoGrid), "Language:", 0, 2, 1);
		d->languageLabel = add_label(GTK_GRID(infoGrid), "-", 1, 2, 1);
		d->biosExtCheck = gtk_check_button_new_with_label("External BIOS");
		d->biosSwiCheck = gtk_check_button_new_with_label("SWI from BIOS");
		d->firmExtCheck = gtk_check_button_new_with_label("External firmware");
		d->bootFwCheck = gtk_check_button_new_with_label("Boot from firmware");
		gtk_grid_attach(GTK_GRID(infoGrid), d->biosExtCheck, 2, 0, 1, 1);
		gtk_grid_attach(GTK_GRID(infoGrid), d->biosSwiCheck, 3, 0, 1, 1);
		gtk_grid_attach(GTK_GRID(infoGrid), d->firmExtCheck, 2, 1, 1, 1);
		gtk_grid_attach(GTK_GRID(infoGrid), d->bootFwCheck, 3, 1, 1, 1);
		gtk_grid_attach(GTK_GRID(grid), infoFrame, 0, row++, 5, 1);

		gtk_widget_set_sensitive(d->biosExtCheck, FALSE);
		gtk_widget_set_sensitive(d->biosSwiCheck, FALSE);
		gtk_widget_set_sensitive(d->firmExtCheck, FALSE);
		gtk_widget_set_sensitive(d->bootFwCheck, FALSE);
	}
}

void movie_record_dialog_show(GtkWindow *parent)
{
	if (desmume_running())
		Pause(NULL, NULL, NULL);

	RecordDialog *d = new RecordDialog();
	build_window(d, parent);

	std::string base = path.GetRomNameWithoutExtension();
	if (base.empty())
		base = "movie";
	gtk_entry_set_text(GTK_ENTRY(d->pathEntry),
		MovieInfo::DefaultMovieFileName(base, movies_directory(), true).c_str());

	std::string author = config.movie_author.get();
	if (author.empty())
	{
		const gchar *login = g_get_user_name();
		author = login ? login : "";
	}
	gtk_entry_set_text(GTK_ENTRY(d->authorEntry), author.c_str());

	fill_read_only_fields(d);
	on_start_changed(NULL, d);
	update_ok(d);
	gtk_widget_show_all(d->window);
	gtk_dialog_run(GTK_DIALOG(d->window));
	gtk_widget_destroy(d->window);
	delete d;
}
