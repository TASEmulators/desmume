/* moviePlay.cpp - this file is part of DeSmuME
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

// GTK3 host for the Play Movie dialog. This is UI only: header parsing, ROM /
// core desync assessment, comment rewriting and pause-frame validation all
// live in ../../../shared/movie_info_core.cpp, which the Windows and Cocoa
// frontends use too. Preferences are persisted by config.cpp (GKeyFile).

#include <gtk/gtk.h>

#include <stdio.h>
#include <string.h>

#include <string>

#include "moviePlay.h"
#include "../config.h"
#include "../main.h"
#include "../desmume.h"
#include "../../../movie.h"
#include "../../../path.h"
#include "../../../frontend/shared/movie_info_core.h"

extern desmume::config::Config config;

namespace
{
	struct PlayDialog
	{
		GtkWidget *window;
		GtkWidget *pathEntry;
		GtkWidget *browseButton;
		GtkWidget *refreshButton;
		GtkWidget *readOnlyCheck;
		GtkWidget *pauseCheck;
		GtkWidget *pauseEntry;

		GtkWidget *movieRomLabel;
		GtkWidget *yourRomLabel;
		GtkWidget *authorLabel;
		GtkWidget *recordDateLabel;
		GtkWidget *coreLabel;
		GtkWidget *framesLabel;
		GtkWidget *lengthLabel;
		GtkWidget *rerecordLabel;
		GtkWidget *commentView;
		GtkWidget *statusLabel;

		GtkWidget *systemLabel;
		GtkWidget *firmwareLabel;
		GtkWidget *languageLabel;
		GtkWidget *nicknameLabel;
		GtkWidget *biosExtCheck;
		GtkWidget *biosSwiCheck;
		GtkWidget *firmExtCheck;
		GtkWidget *bootFwCheck;
		GtkWidget *powerRadio;
		GtkWidget *sramRadio;
		GtkWidget *snapshotRadio;

		GtkTextBuffer *commentBuffer;
		std::string path;
		MovieInfo::Header header;
		bool headerValid;
		bool romMismatch;
		bool coreMismatch;

		PlayDialog()
			: window(NULL), headerValid(false), romMismatch(false), coreMismatch(false)
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

	// Escapes text for a Pango markup <span>. Field values come from a movie
	// header and may contain '&' or '<', which has to be escaped before markup.
	static std::string markup_escape(const std::string &s)
	{
		std::string out;
		out.reserve(s.size());
		for (size_t i = 0; i < s.size(); i++)
		{
			switch (s[i])
			{
				case '&': out += "&amp;"; break;
				case '<': out += "&lt;"; break;
				case '>': out += "&gt;"; break;
				default:  out += s[i]; break;
			}
		}
		return out;
	}

	static void set_field(GtkWidget *label, const std::string &text, bool warn)
	{
		const std::string escaped = markup_escape(text);
		if (warn)
		{
			char markup[1024];
			g_snprintf(markup, sizeof(markup), "<span color=\"#cc0000\">%s</span>", escaped.c_str());
			gtk_label_set_markup(GTK_LABEL(label), markup);
		}
		else
		{
			gtk_label_set_text(GTK_LABEL(label), text.c_str());
		}
	}

	static std::string entry_text(GtkWidget *entry)
	{
		const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));
		return text ? std::string(text) : std::string();
	}

	// Resolves a bare file name against the Movies directory, like the Windows
	// and Cocoa dialogs do. A name containing a path separator is used as-is.
	static std::string resolve_path(const std::string &text)
	{
		if (text.empty())
			return text;
		std::string full = text;
		if (full.find_first_of("\\/") == std::string::npos)
		{
			std::string dir = movies_directory();
			while (!dir.empty() && (dir[dir.size() - 1] == '/' || dir[dir.size() - 1] == '\\'))
				dir.erase(dir.size() - 1);
			full = dir + "/" + full;
		}
		return MovieInfo::EnsureDsmExtension(full);
	}

	static void clear_fields(PlayDialog *d)
	{
		const char *empty[] = { "-", "-", "-", "-", "-", "-", "-", "-", "-" };
		GtkWidget *labels[] = {
			d->movieRomLabel, d->yourRomLabel, d->authorLabel, d->recordDateLabel,
			d->coreLabel, d->framesLabel, d->lengthLabel, d->rerecordLabel, d->statusLabel
		};
		for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++)
			gtk_label_set_text(GTK_LABEL(labels[i]), empty[i]);

		gtk_text_buffer_set_text(d->commentBuffer, "", 0);
		d->header = MovieInfo::Header();
		d->headerValid = false;
		d->romMismatch = false;
		d->coreMismatch = false;
	}

	static void refresh(PlayDialog *d, bool force)
	{
		const std::string filename = resolve_path(entry_text(d->pathEntry));
		if (!force && filename == d->path)
			return;
		d->path = filename;

		MovieInfo::Header hdr;
		if (!MovieInfo::ReadHeader(filename, hdr))
		{
			clear_fields(d);
			return;
		}
		d->header = hdr;
		d->headerValid = true;

		const MovieInfo::RomIdentity rom = MovieInfo::CurrentRomIdentity();
		const MovieInfo::Assessment a = MovieInfo::Assess(hdr, rom, MovieInfo::CurrentCoreVersion());
		d->romMismatch = a.romMismatch;
		d->coreMismatch = a.coreMismatch;

		const std::string movieCrc = a.crcComparable
			? hdr.romChecksum : std::string("n/a (legacy)");
		const std::string movieName = MovieInfo::BaseName(hdr.romFilename);
		set_field(d->movieRomLabel, MovieInfo::FormatRomLine("Movie ROM: ",
			hdr.romSerial.empty() ? "-" : hdr.romSerial, movieCrc,
			"name=", movieName.empty() ? "-" : MovieInfo::Shorten(movieName, 32).c_str()), a.romMismatch);
		set_field(d->yourRomLabel, MovieInfo::FormatRomLine("Your ROM:  ",
			rom.serial.empty() ? "-" : rom.serial, MovieInfo::FormatHexCrc(rom.crc),
			"title=", rom.internalName), a.romMismatch);

		std::string author, description;
		MovieInfo::SplitComments(hdr, author, description);
		set_field(d->authorLabel, author, false);
		set_field(d->recordDateLabel, MovieInfo::FormatRecordDate(hdr.recordDate), false);
		set_field(d->coreLabel, MovieInfo::CoreVersionText(hdr, a), a.coreMismatch);
		set_field(d->framesLabel, std::string("Frames: ") + std::to_string(hdr.frames), false);
		set_field(d->lengthLabel, MovieInfo::FormatLength(hdr.frames), false);
		set_field(d->rerecordLabel, std::string("Rerecords: ") + std::to_string(MovieInfo::ParseUInt(hdr.rerecordCount)), false);
		gtk_text_buffer_set_text(d->commentBuffer, description.c_str(), (gint)description.size());

		if (a.romMismatch || a.coreMismatch)
		{
			gtk_label_set_markup(GTK_LABEL(d->statusLabel),
				"<span color=\"#cc0000\">Desync expected: movie ROM/core does not match the current setup.</span>");
		}
		else
		{
			gtk_label_set_text(GTK_LABEL(d->statusLabel), "ROM and core match this movie.");
		}

		// Read-only groups: playback cannot change where a movie begins.
		const bool fromSnapshot = hdr.savestate == "1";
		const bool fromSram = !fromSnapshot && hdr.hasSram;
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->powerRadio), !fromSnapshot && !fromSram ? TRUE : FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->sramRadio), fromSram ? TRUE : FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->snapshotRadio), fromSnapshot ? TRUE : FALSE);

		set_field(d->systemLabel, "Nintendo DS", false);
		set_field(d->firmwareLabel, MovieInfo::DescribeFirmware(hdr.useExtFirmware), false);
		set_field(d->languageLabel, MovieInfo::DescribeLanguage(hdr.firmLanguage), false);
		set_field(d->nicknameLabel, hdr.firmNickname.empty() ? "-" : hdr.firmNickname, false);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->biosExtCheck), hdr.useExtBios == "1" ? TRUE : FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->biosSwiCheck), hdr.swiFromBios == "1" ? TRUE : FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->firmExtCheck), hdr.useExtFirmware == "1" ? TRUE : FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->bootFwCheck), hdr.bootFromFirmware == "1" ? TRUE : FALSE);

		// Pause-at-frame defaults to the movie's last frame, as on Windows/Cocoa.
		char frames[32];
		g_snprintf(frames, sizeof(frames), "%u", hdr.frames);
		gtk_entry_set_text(GTK_ENTRY(d->pauseEntry), frames);
	}

	static void on_refresh(GtkWidget *widget, gpointer data)
	{
		refresh(static_cast<PlayDialog *>(data), true);
	}

	static void on_path_changed(GtkWidget *widget, gpointer data)
	{
		refresh(static_cast<PlayDialog *>(data), false);
	}

	static void on_read_only_toggled(GtkWidget *widget, gpointer data)
	{
		PlayDialog *d = static_cast<PlayDialog *>(data);
		const bool readOnly = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(d->readOnlyCheck)) != FALSE;
		gtk_text_view_set_editable(GTK_TEXT_VIEW(d->commentView), !readOnly ? TRUE : FALSE);
	}

	static void on_browse(GtkWidget *widget, gpointer data)
	{
		PlayDialog *d = static_cast<PlayDialog *>(data);
		GtkFileChooserNative *chooser = gtk_file_chooser_native_new(
			"Select movie", GTK_WINDOW(d->window), GTK_FILE_CHOOSER_ACTION_OPEN, "_Open", "_Cancel");
		GtkFileFilter *filter = gtk_file_filter_new();
		gtk_file_filter_add_pattern(filter, "*.dsm*");
		gtk_file_filter_set_name(filter, "DeSmuME movie file (.dsm*)");
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), movies_directory().c_str());

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

	static void on_response(GtkDialog *dialog, gint response, gpointer data)
	{
		PlayDialog *d = static_cast<PlayDialog *>(data);
		if (response != GTK_RESPONSE_OK)
			return;

		// Re-read the header so the file cannot change under us between preview
		// and OK, mirroring the Windows dialog.
		const std::string filename = resolve_path(entry_text(d->pathEntry));
		MovieInfo::Header fresh;
		if (!MovieInfo::ReadHeader(filename, fresh))
			return;

		const bool readOnly = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(d->readOnlyCheck)) != FALSE;
		const gboolean pauseEnabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(d->pauseCheck));
		const int pauseFrame = MovieInfo::ParsePauseFrame(entry_text(d->pauseEntry), pauseEnabled != FALSE);

		// Read-write: persist an edited comment by rewriting only header lines.
		if (!readOnly)
		{
			GtkTextIter start, end;
			gtk_text_buffer_get_bounds(d->commentBuffer, &start, &end);
			gchar *text = gtk_text_buffer_get_text(d->commentBuffer, &start, &end, FALSE);
			const std::string description = text ? text : "";
			if (text != NULL)
				g_free(text);
			// Compare with the description currently in the file, so an
			// unchanged preview never rewrites the header.
			std::string author, original;
			MovieInfo::SplitComments(fresh, author, original);
			if (description != original)
			{
				if (!MovieInfo::WriteComments(filename, fresh, description))
				{
					GtkWidget *msg = gtk_message_dialog_new(GTK_WINDOW(dialog),
						GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
						"The comment could not be saved. Playback will continue with the original file.");
					gtk_dialog_run(GTK_DIALOG(msg));
					gtk_widget_destroy(msg);
				}
			}
		}

		config.movie_directory.set(movies_directory());
		const char *error = FCEUI_LoadMovie(filename.c_str(), readOnly, false, pauseFrame);
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
		gtk_label_set_selectable(GTK_LABEL(label), TRUE);
		gtk_grid_attach(grid, label, x, y, w, 1);
		return label;
	}

	static GtkWidget *add_check(GtkGrid *grid, const char *text, int x, int y, int w)
	{
		GtkWidget *check = gtk_check_button_new_with_label(text);
		gtk_grid_attach(grid, check, x, y, w, 1);
		return check;
	}

	static void build_window(PlayDialog *d, GtkWindow *parent)
	{
		d->window = gtk_dialog_new_with_buttons("Play Movie", parent,
			(GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
			"_Cancel", GTK_RESPONSE_CANCEL, "_Play", GTK_RESPONSE_OK, NULL);
		gtk_window_set_default_size(GTK_WINDOW(d->window), 660, 620);
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
		d->refreshButton = gtk_button_new_with_label("Refresh");
		gtk_grid_attach(GTK_GRID(grid), d->refreshButton, 5, row, 1, 1);
		g_signal_connect(d->refreshButton, "clicked", G_CALLBACK(on_refresh), d);
		row++;

		d->readOnlyCheck = add_check(GTK_GRID(grid), "Open read-only", 0, row, 2);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->readOnlyCheck), TRUE);
		g_signal_connect(d->readOnlyCheck, "toggled", G_CALLBACK(on_read_only_toggled), d);
		d->pauseCheck = add_check(GTK_GRID(grid), "Pause at frame:", 2, row, 2);
		d->pauseEntry = gtk_entry_new();
		gtk_grid_attach(GTK_GRID(grid), d->pauseEntry, 4, row, 2, 1);
		row++;

		// Metadata preview.
		d->movieRomLabel = add_field(GTK_GRID(grid), 0, row++, 6);
		d->yourRomLabel = add_field(GTK_GRID(grid), 0, row++, 6);
		add_label(GTK_GRID(grid), "Author:", 0, row, 1);
		d->authorLabel = add_field(GTK_GRID(grid), 1, row, 2);
		add_label(GTK_GRID(grid), "Recorded:", 3, row, 1);
		d->recordDateLabel = add_field(GTK_GRID(grid), 4, row, 2);
		row++;
		add_label(GTK_GRID(grid), "Core:", 0, row, 1);
		d->coreLabel = add_field(GTK_GRID(grid), 1, row, 5);
		row++;
		d->framesLabel = add_field(GTK_GRID(grid), 0, row, 2);
		d->lengthLabel = add_field(GTK_GRID(grid), 2, row, 2);
		d->rerecordLabel = add_field(GTK_GRID(grid), 4, row, 2);
		row++;

		add_label(GTK_GRID(grid), "Comments:", 0, row, 1);
		GtkWidget *commentScroll = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(commentScroll), GTK_SHADOW_ETCHED_IN);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(commentScroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_widget_set_hexpand(commentScroll, FALSE);
		gtk_grid_attach(GTK_GRID(grid), commentScroll, 1, row, 5, 1);
		d->commentView = gtk_text_view_new();
		gtk_text_view_set_editable(GTK_TEXT_VIEW(d->commentView), FALSE);
		gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(d->commentView), GTK_WRAP_WORD);
		d->commentBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(d->commentView));
		gtk_container_add(GTK_CONTAINER(commentScroll), d->commentView);
		row++;

		d->statusLabel = add_field(GTK_GRID(grid), 0, row++, 6);

		// Recording options and firmware (read-only), in two columns.
		GtkWidget *recFrame = gtk_frame_new("Recording options (read-only)");
		GtkWidget *recGrid = gtk_grid_new();
		gtk_grid_set_row_spacing(GTK_GRID(recGrid), 4);
		gtk_grid_set_column_spacing(GTK_GRID(recGrid), 8);
		gtk_container_set_border_width(GTK_CONTAINER(recGrid), 8);
		gtk_container_add(GTK_CONTAINER(recFrame), recGrid);
		d->powerRadio = gtk_radio_button_new_with_label(NULL, "Power on");
		d->sramRadio = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(d->powerRadio), "Reset + SRAM");
		d->snapshotRadio = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(d->powerRadio), "Current state");
		gtk_grid_attach(GTK_GRID(recGrid), d->powerRadio, 0, 0, 1, 1);
		gtk_grid_attach(GTK_GRID(recGrid), d->sramRadio, 0, 1, 1, 1);
		gtk_grid_attach(GTK_GRID(recGrid), d->snapshotRadio, 0, 2, 1, 1);
		add_label(GTK_GRID(recGrid), "System:", 1, 0, 1);
		d->systemLabel = add_field(GTK_GRID(recGrid), 2, 0, 1);
		add_label(GTK_GRID(recGrid), "Firmware:", 1, 1, 1);
		d->firmwareLabel = add_field(GTK_GRID(recGrid), 2, 1, 1);
		add_label(GTK_GRID(recGrid), "Language:", 1, 2, 1);
		d->languageLabel = add_field(GTK_GRID(recGrid), 2, 2, 1);
		add_label(GTK_GRID(recGrid), "Nickname:", 1, 3, 1);
		d->nicknameLabel = add_field(GTK_GRID(recGrid), 2, 3, 1);
		gtk_grid_attach(GTK_GRID(grid), recFrame, 0, row, 6, 1);
		row++;

		GtkWidget *biosFrame = gtk_frame_new("BIOS / firmware (read-only)");
		GtkWidget *biosGrid = gtk_grid_new();
		gtk_container_set_border_width(GTK_CONTAINER(biosGrid), 8);
		gtk_container_add(GTK_CONTAINER(biosFrame), biosGrid);
		d->biosExtCheck = add_check(GTK_GRID(biosGrid), "External BIOS", 0, 0, 1);
		d->biosSwiCheck = add_check(GTK_GRID(biosGrid), "SWI from BIOS", 1, 0, 1);
		d->firmExtCheck = add_check(GTK_GRID(biosGrid), "External firmware", 0, 1, 1);
		d->bootFwCheck = add_check(GTK_GRID(biosGrid), "Boot from firmware", 1, 1, 1);
		gtk_grid_attach(GTK_GRID(grid), biosFrame, 0, row, 6, 1);

		// Disable the read-only groups: they are informational during playback.
		gtk_widget_set_sensitive(d->powerRadio, FALSE);
		gtk_widget_set_sensitive(d->sramRadio, FALSE);
		gtk_widget_set_sensitive(d->snapshotRadio, FALSE);
		gtk_widget_set_sensitive(d->biosExtCheck, FALSE);
		gtk_widget_set_sensitive(d->biosSwiCheck, FALSE);
		gtk_widget_set_sensitive(d->firmExtCheck, FALSE);
		gtk_widget_set_sensitive(d->bootFwCheck, FALSE);
	}
}

void movie_play_dialog_show(GtkWindow *parent)
{
	if (desmume_running())
		Pause(NULL, NULL, NULL);

	PlayDialog *d = new PlayDialog();
	build_window(d, parent);

	// Seed the file name with the ROM base name, like the other frontends.
	std::string base = path.GetRomNameWithoutExtension();
	if (base.empty())
		base = "movie";
	gtk_entry_set_text(GTK_ENTRY(d->pathEntry), MovieInfo::EnsureDsmExtension(base).c_str());

	gtk_widget_show_all(d->window);
	gtk_dialog_run(GTK_DIALOG(d->window));
	gtk_widget_destroy(d->window);
	delete d;
}
