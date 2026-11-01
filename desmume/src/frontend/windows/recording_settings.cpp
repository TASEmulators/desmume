/*
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

// RecordingSettings.cpp : the encoder picker shown before the save dialog.
//
// The list is built from what this machine actually has installed, so a system
// with no hardware encoder simply shows the software row, and an audio codec
// whose encoder is not installed never appears at all.
//
// The container and the extension follow from the chosen pair; they are shown
// in the summary line and never edited here.
//
// Ported from vba-rerecording's src/win32/Dialogs/RecordSettings.cpp.  The
// "encode in a background thread" box is gone on purpose: the capture path in
// aviout.cpp always hands frames to a writer thread, and the sink writer has to
// live on the thread that calls it, so that choice does not exist here.
//
// This frontend builds as an ANSI application, so the dialog and all of its
// controls are ANSI; the encoder names arrive as UTF-16 from Media Foundation
// and are narrowed once, where they are put into a control.

#include "recording_settings.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include "resource.h"
#include "main.h"
#include "winutil.h"
#include "videoencoders.h"
#include "video_writer.h"
#include "record_compositor.h"
#include "video.h"

#include "GPU.h"
#include "SPU.h"

// The picture settings the recording follows.  Declared here rather than
// through display.h, which would drag the whole window class into a dialog.
extern VideoInfo video;

namespace
{
	// Rows of the preset boxes, in the order FillPresets() adds them.
	enum { PRESET_LOW, PRESET_MEDIUM, PRESET_HIGH, PRESET_CUSTOM };

	// Rows of the Picture box.  Row 0 is what this recorder always wrote, so
	// nothing changes for anyone who does not open the box.
	//
	// The size field is meaningless for the raw picture -- the two screens
	// arrive stacked and are written as they are -- but it still has to have
	// a value for SelectPicture() to match the row against a saved config.
	struct PictureEntry
	{
		int			mode;	// RECORD_PICTURE_*
		int			size;	// RECORD_SIZE_*
		const char *name;
	};

	const PictureEntry kPictures[] = {
		{ RECORD_PICTURE_NATIVE,   RECORD_SIZE_WINDOW, "Raw (Two screens stacked)" },
		{ RECORD_PICTURE_COMPOSED, RECORD_SIZE_1X,	   "1x layout" },
		{ RECORD_PICTURE_COMPOSED, RECORD_SIZE_2X,	   "2x layout" },
		{ RECORD_PICTURE_COMPOSED, RECORD_SIZE_3X,	   "3x layout" },
		{ RECORD_PICTURE_COMPOSED, RECORD_SIZE_4X,	   "4x layout" },
	};

	const int kVideoPresets[] = { VIDEO_BITRATE_LOW, VIDEO_BITRATE_MEDIUM, VIDEO_BITRATE_HIGH };
	const int kAudioPresets[] = { AUDIO_BITRATE_LOW, AUDIO_BITRATE_MEDIUM, AUDIO_BITRATE_HIGH };

	// Rows of the frame box.  The number is a decimation factor, not an fps:
	// frames only arrive at the DS LCD's own 59.83 Hz, so what the box picks
	// is how many of them are thrown away between the ones that are written.
	struct FrameStepEntry
	{
		int			step;
		const char *name;
	};

	const FrameStepEntry kFrameSteps[] = {
		{ 1, "No frame skip" },
		{ 2, "Skip 1" },
		{ 3, "Skip 2" },
		{ 4, "Skip 3" },
	};

	// One profile list per codec: the numbers are that codec's own profile
	// numbers, so 77 means Main in H.264 and nothing in particular in the
	// other two.  Only the 8-bit 4:2:0 profiles are offered, because that is
	// what the frame is before the encoder sees it.
	struct ProfileEntry
	{
		int			value;
		const char *name;
	};

	const ProfileEntry kH264Profiles[] = {
		{ VIDEO_PROFILE_AUTO, "Auto" },
		{ H264_PROFILE_BASELINE, "Baseline" },
		{ H264_PROFILE_MAIN, "Main" },
		{ H264_PROFILE_HIGH, "High" },
	};

	// HEVC and AV1 have one 8-bit 4:2:0 profile each, so there is nothing to
	// choose; the box stays on Auto and is disabled.  See video_writer.h.
	const ProfileEntry kHevcProfiles[] = { { VIDEO_PROFILE_AUTO, "Auto" } };
	const ProfileEntry kAv1Profiles[]  = { { VIDEO_PROFILE_AUTO, "Auto" } };

	const char kNone[] = "None";

	struct RecordSettingsState
	{
		HWND dlg;

		std::vector<VideoEncoderChoice> video;
		std::vector<AudioEncoderChoice> audio;

		RecordParams params;

		// Indices into audio of the rows currently shown in the combo box.
		std::vector<int> audioRows;
	};

	int clampInt(int value, int low, int high)
	{
		if (value < low)
			return low;
		if (value > high)
			return high;
		return value;
	}

	// Media Foundation describes its encoders in UTF-16; the controls here are
	// ANSI, so this is the one place the two meet.
	std::string Narrow(const wchar_t *text)
	{
		if (text == NULL || text[0] == L'\0')
		{
			return std::string();
		}

		const int length = WideCharToMultiByte(CP_ACP, 0, text, -1, NULL, 0, NULL, NULL);
		if (length <= 1)
		{
			return std::string();
		}

		std::vector<char> buffer((size_t)length, '\0');
		WideCharToMultiByte(CP_ACP, 0, text, -1, &buffer[0], length, NULL, NULL);

		return std::string(&buffer[0]);
	}

	void ListSetSubItem(HWND list, int row, int column, const char *text)
	{
		ListView_SetItemText(list, row, column, (LPSTR)text);
	}

	void ListInsertText(HWND list, int row, int column, const char *text)
	{
		LVITEMA item;
		ZeroMemory(&item, sizeof(item));
		item.iItem	  = row;
		item.iSubItem = column;
		item.mask	  = LVIF_TEXT;
		item.pszText  = (LPSTR)text;

		if (column == 0)
		{
			ListView_InsertItem(list, &item);
		}
		else
		{
			ListView_SetItem(list, &item);
		}
	}

	// Room a list cell leaves around its text.
	const int kCellPadding = 12;

	int MaxInt(int a, int b)
	{
		return (a > b) ? a : b;
	}

	int TextWidth(HDC dc, const char *text)
	{
		SIZE size = {0, 0};
		GetTextExtentPoint32A(dc, text, (int)strlen(text), &size);

		return (int)size.cx;
	}

	// GetDlgItem returns NULL for a control the template does not have, and
	// that happens for real while a template is being edited.  Everything that
	// enables a control goes through here.
	void EnableItem(HWND dlg, int id, BOOL enable)
	{
		HWND control = GetDlgItem(dlg, id);
		if (control != NULL)
		{
			EnableWindow(control, enable);
		}
	}

	// --- the pieces of state the dialog derives from its controls ----------

	GUID CurrentVideo(RecordSettingsState *st, bool *hardware)
	{
		HWND list = GetDlgItem(st->dlg, IDC_REC_VIDEO);

		const int row = ListView_GetNextItem(list, -1, LVNI_SELECTED);
		if (row < 0 || row >= (int)st->video.size())
		{
			if (hardware)
				*hardware = false;
			return GUID();
		}

		const VideoEncoderChoice &entry = st->video[row];
		if (hardware)
			*hardware = entry.hardware;

		if (!entry.available)
			return GUID();

		return entry.subtype;
	}

	GUID CurrentAudio(RecordSettingsState *st)
	{
		HWND combo = GetDlgItem(st->dlg, IDC_REC_AUDIO);

		const int row = ComboBox_GetCurSel(combo);
		if (row < 0)
			return GUID();

		const int index = (int)ComboBox_GetItemData(combo, row);
		if (index < 0 || index >= (int)st->audio.size())
			return GUID();

		const AudioEncoderChoice &entry = st->audio[index];
		return entry.available ? entry.subtype : GUID();
	}

	int CurrentFrameStep(RecordSettingsState *st)
	{
		const int row = ComboBox_GetCurSel(GetDlgItem(st->dlg, IDC_REC_FRAMERATE));
		if (row < 0 || row >= (int)(sizeof(kFrameSteps) / sizeof(kFrameSteps[0])))
			return FRAME_STEP_NONE;

		return kFrameSteps[row].step;
	}

	int CurrentPictureRow(RecordSettingsState *st)
	{
		const int row = ComboBox_GetCurSel(GetDlgItem(st->dlg, IDC_REC_CAPTURE));
		if (row < 0 || row >= (int)(sizeof(kPictures) / sizeof(kPictures[0])))
			return 0;

		return row;
	}

	PictureEntry CurrentPicture(RecordSettingsState *st)
	{
		return kPictures[CurrentPictureRow(st)];
	}

	int CurrentFilter(RecordSettingsState *st)
	{
		const int row = ComboBox_GetCurSel(GetDlgItem(st->dlg, IDC_REC_FILTER));
		if (row < 0 || row >= recordCompose::FilterCount())
			return VideoInfo::NONE;

		return row;
	}

	// The size the composed recording will have, which is what the summary
	// line promises.  False when the picture is the native stacked one and
	// there is no composing to do.
	bool ComposedSize(RecordSettingsState *st, const PictureEntry &picture, size_t *width, size_t *height)
	{
		if (picture.mode != RECORD_PICTURE_COMPOSED)
			return false;

		const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
		const double scale = recordCompose::FilterScale(CurrentFilter(st));

		size_t contentW = 0;
		size_t contentH = 0;
		recordCompose::ContentSize(dispInfo.customWidth, dispInfo.customHeight, video.layout, video.screengap,
		                           video.rotation, scale, &contentW, &contentH);

		*width	= contentW * (size_t)picture.size;
		*height = contentH * (size_t)picture.size;
		return true;
	}

	const ProfileEntry *ProfileListFor(const GUID &video, int *count)
	{
		if (videoEncoders::isHevc(video))
		{
			*count = _countof(kHevcProfiles);
			return kHevcProfiles;
		}

		if (videoEncoders::isAv1(video))
		{
			*count = _countof(kAv1Profiles);
			return kAv1Profiles;
		}

		*count = _countof(kH264Profiles);
		return kH264Profiles;
	}

	int CurrentProfile(RecordSettingsState *st)
	{
		HWND box = GetDlgItem(st->dlg, IDC_REC_PROFILE);

		const int row = ComboBox_GetCurSel(box);
		if (row < 0 || row >= ComboBox_GetCount(box))
			return VIDEO_PROFILE_AUTO;

		int count = 0;
		const ProfileEntry *entries = ProfileListFor(CurrentVideo(st, NULL), &count);
		if (row >= count)
			return VIDEO_PROFILE_AUTO;

		return entries[row].value;
	}

	// --- filling the controls ----------------------------------------------

	void FillVideoList(RecordSettingsState *st)
	{
		HWND list = GetDlgItem(st->dlg, IDC_REC_VIDEO);

		ListView_DeleteAllItems(list);

		RECT client;
		GetClientRect(list, &client);

		LVCOLUMNA column;
		ZeroMemory(&column, sizeof(column));
		column.mask	   = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
		column.cx	   = 0;
		column.pszText = (LPSTR)"Codec";
		ListView_InsertColumn(list, 0, &column);

		column.iSubItem = 1;
		column.pszText	= (LPSTR)"Encoder";
		ListView_InsertColumn(list, 1, &column);

		column.iSubItem = 2;
		column.pszText	= (LPSTR)"Type";
		ListView_InsertColumn(list, 2, &column);

		// Codec and Type only ever hold short, known strings, so their columns
		// are sized to the widest one this machine actually produced, measured
		// with the dialog's own font.  A pixel width written into the source
		// does not survive a change of DPI: 44 pixels is a comfortable "H.264"
		// at 100% and "H..." at 200%, which is what the dialog used to show.
		HDC dc = GetDC(list);
		HFONT font = (HFONT)SendMessage(list, WM_GETFONT, 0, 0);
		HGDIOBJ oldFont = (font != NULL) ? SelectObject(dc, font) : NULL;

		int codecWidth = TextWidth(dc, "Codec") + kCellPadding;
		int typeWidth  = TextWidth(dc, "Type") + kCellPadding;

		for (size_t i = 0; i < st->video.size(); ++i)
		{
			const VideoEncoderChoice &entry = st->video[i];

			const std::string codec = entry.available
			                              ? Narrow(videoEncoders::codecName(entry.subtype))
			                              : std::string("-");

			ListInsertText(list, (int)i, 0, codec.c_str());
			codecWidth = MaxInt(codecWidth, TextWidth(dc, codec.c_str()) + kCellPadding);

			if (!entry.available)
			{
				ListSetSubItem(list, (int)i, 1, kNone);
				continue;
			}

			ListSetSubItem(list, (int)i, 1, Narrow(entry.name.c_str()).c_str());

			std::string type = entry.hardware ? "Hardware" : "Software";
			if (entry.hardware && !entry.vendor.empty())
			{
				type += " (";
				type += Narrow(entry.vendor.c_str());
				type += ")";
			}

			ListSetSubItem(list, (int)i, 2, type.c_str());
			typeWidth = MaxInt(typeWidth, TextWidth(dc, type.c_str()) + kCellPadding);
		}

		if (oldFont != NULL)
		{
			SelectObject(dc, oldFont);
		}
		ReleaseDC(list, dc);

		// Whatever is left goes to the encoder names, which are long, come
		// from the driver and are the ones worth truncating.
		const int encoderWidth = MaxInt(80, client.right - codecWidth - typeWidth - 4);

		ListView_SetColumnWidth(list, 0, codecWidth);
		ListView_SetColumnWidth(list, 1, encoderWidth);
		ListView_SetColumnWidth(list, 2, typeWidth);
	}

	void FillAudioList(RecordSettingsState *st)
	{
		HWND combo = GetDlgItem(st->dlg, IDC_REC_AUDIO);

		const GUID video = CurrentVideo(st, NULL);

		ComboBox_ResetContent(combo);
		st->audioRows.clear();

		for (size_t i = 0; i < st->audio.size(); ++i)
		{
			const AudioEncoderChoice &entry = st->audio[i];

			// Only offer what the container of this pair can hold.
			if (videoEncoders::containerFor(video, entry.subtype) == NULL)
				continue;

			std::string text;
			if (entry.available)
			{
				text = Narrow(entry.name.c_str());
			}
			else
			{
				text = kNone;
			}

			const int row = ComboBox_AddString(combo, text.c_str());
			if (row < 0)
				continue;

			ComboBox_SetItemData(combo, (UINT)row, (DWORD_PTR)i);
			st->audioRows.push_back((int)i);
		}
	}

	void FillPresets(RecordSettingsState *st)
	{
		HWND video = GetDlgItem(st->dlg, IDC_REC_VIDEO_PRESET);
		HWND audio = GetDlgItem(st->dlg, IDC_REC_AUDIO_PRESET);

		const char *presets[] = { "Low", "Medium", "High", "Custom" };

		for (int i = 0; i < 4; ++i)
		{
			ComboBox_AddString(video, presets[i]);
			ComboBox_AddString(audio, presets[i]);
		}
	}

	void FillFrameRates(RecordSettingsState *st)
	{
		HWND box = GetDlgItem(st->dlg, IDC_REC_FRAMERATE);

		for (int i = 0; i < (int)(sizeof(kFrameSteps) / sizeof(kFrameSteps[0])); ++i)
		{
			ComboBox_AddString(box, kFrameSteps[i].name);
		}
	}

	void FillPictures(RecordSettingsState *st)
	{
		HWND box = GetDlgItem(st->dlg, IDC_REC_CAPTURE);

		for (int i = 0; i < (int)(sizeof(kPictures) / sizeof(kPictures[0])); ++i)
		{
			ComboBox_AddString(box, kPictures[i].name);
		}

		ComboBox_SetCurSel(box, 0);
	}

	void FillFilters(RecordSettingsState *st)
	{
		HWND box = GetDlgItem(st->dlg, IDC_REC_FILTER);

		for (int i = 0; i < recordCompose::FilterCount(); ++i)
		{
			const char *name = recordCompose::FilterName(i);
			ComboBox_AddString(box, (name != NULL) ? name : "?");
		}

		ComboBox_SetCurSel(box, 0);
	}

	void SelectPicture(RecordSettingsState *st, int mode, int size)
	{
		HWND box = GetDlgItem(st->dlg, IDC_REC_CAPTURE);

		for (int i = 0; i < (int)(sizeof(kPictures) / sizeof(kPictures[0])); ++i)
		{
			if (kPictures[i].mode == mode && kPictures[i].size == size)
			{
				ComboBox_SetCurSel(box, i);
				return;
			}
		}

		ComboBox_SetCurSel(box, 0);
	}

	void SelectFilter(RecordSettingsState *st, int filter)
	{
		if (filter < 0 || filter >= recordCompose::FilterCount())
			filter = VideoInfo::NONE;

		ComboBox_SetCurSel(GetDlgItem(st->dlg, IDC_REC_FILTER), filter);
	}

	// The profile list belongs to the codec, so it is rebuilt whenever the
	// selected row changes.  Rebuilding also puts the choice back on Auto,
	// because a profile number means something different in each codec.
	void FillProfileBox(RecordSettingsState *st, const GUID &video)
	{
		HWND box = GetDlgItem(st->dlg, IDC_REC_PROFILE);

		int count = 0;
		const ProfileEntry *entries = ProfileListFor(video, &count);

		ComboBox_ResetContent(box);
		for (int i = 0; i < count; ++i)
		{
			ComboBox_AddString(box, entries[i].name);
		}

		ComboBox_SetCurSel(box, 0);
	}

	void SelectVideo(RecordSettingsState *st, const GUID &subtype, bool hardware)
	{
		HWND list = GetDlgItem(st->dlg, IDC_REC_VIDEO);

		int fallback = -1;

		for (size_t i = 0; i < st->video.size(); ++i)
		{
			const VideoEncoderChoice &entry = st->video[i];

			if (entry.available && fallback < 0)
				fallback = (int)i;

			if (entry.available && entry.hardware == hardware && IsEqualGUID(entry.subtype, subtype))
			{
				ListView_SetItemState(list, (int)i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
				ListView_EnsureVisible(list, (int)i, FALSE);
				return;
			}
		}

		// The saved encoder is gone (driver update, another machine): fall back
		// to the first row, which is a hardware encoder when there is one.
		if (fallback < 0 && !st->video.empty())
			fallback = 0;

		if (fallback >= 0)
			ListView_SetItemState(list, fallback, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	}

	void SelectAudio(RecordSettingsState *st, const GUID &subtype)
	{
		HWND combo = GetDlgItem(st->dlg, IDC_REC_AUDIO);

		for (size_t row = 0; row < st->audioRows.size(); ++row)
		{
			const AudioEncoderChoice &entry = st->audio[st->audioRows[row]];
			if (IsEqualGUID(entry.subtype, subtype))
			{
				ComboBox_SetCurSel(combo, (int)row);
				return;
			}
		}

		// Default to the last row, which is "None", only when the saved codec
		// is not there at all.
		if (!st->audioRows.empty())
			ComboBox_SetCurSel(combo, (int)st->audioRows.size() - 1);
	}

	void SelectProfile(RecordSettingsState *st, int profile)
	{
		HWND box = GetDlgItem(st->dlg, IDC_REC_PROFILE);

		int count = 0;
		const ProfileEntry *entries = ProfileListFor(CurrentVideo(st, NULL), &count);

		for (int i = 0; i < count; ++i)
		{
			if (entries[i].value == profile)
			{
				ComboBox_SetCurSel(box, i);
				return;
			}
		}

		ComboBox_SetCurSel(box, 0);
	}

	void UpdatePresetBoxes(RecordSettingsState *st)
	{
		// The box mirrors the number: typing 4000 by hand lands on "Medium",
		// and any other number lands on "Custom".
		const int video = (int)GetDlgItemInt(st->dlg, IDC_REC_VIDEO_BITRATE, NULL, FALSE);
		int		  row	= PRESET_CUSTOM;
		for (int i = 0; i < 3; ++i)
		{
			if (kVideoPresets[i] == video)
				row = i;
		}
		ComboBox_SetCurSel(GetDlgItem(st->dlg, IDC_REC_VIDEO_PRESET), row);

		const int audio = (int)GetDlgItemInt(st->dlg, IDC_REC_AUDIO_BITRATE, NULL, FALSE);
		row				= PRESET_CUSTOM;
		for (int i = 0; i < 3; ++i)
		{
			if (kAudioPresets[i] == audio)
				row = i;
		}
		ComboBox_SetCurSel(GetDlgItem(st->dlg, IDC_REC_AUDIO_PRESET), row);
	}

	void UpdateSummary(RecordSettingsState *st)
	{
		// The spin controls write their buddy's text while the template is
		// still being created (UDS_AUTOBUDDY | UDS_SETBUDDYINT), which sends
		// EN_CHANGE long before WM_INITDIALOG has enumerated the encoders.
		// There is no selection to summarize at that point.
		if (GetDlgItem(st->dlg, IDC_REC_VIDEO) == NULL)
			return;

		bool	   hardware = false;
		const GUID video	= CurrentVideo(st, &hardware);
		const GUID audio	= CurrentAudio(st);

		const RecordContainer *container = videoEncoders::containerFor(video, audio);

		const BOOL hasVideo = !videoEncoders::isNullGuid(video);
		const BOOL hasAudio = !videoEncoders::isNullGuid(audio);

		// A bit rate target means nothing to a lossless encoder, so the box
		// goes away rather than sitting there doing nothing.
		const BOOL lossless = hasAudio && videoEncoders::isLosslessAudio(audio);
		EnableItem(st->dlg, IDC_REC_AUDIO_BITRATE, hasAudio && !lossless);
		EnableItem(st->dlg, IDC_REC_AUDIO_SPIN, hasAudio && !lossless);
		EnableItem(st->dlg, IDC_REC_AUDIO_PRESET, hasAudio && !lossless);
		EnableItem(st->dlg, IDC_REC_SAMPLE_RATE, hasAudio);
		EnableItem(st->dlg, IDC_REC_VIDEO_BITRATE, hasVideo);
		EnableItem(st->dlg, IDC_REC_VIDEO_SPIN, hasVideo);
		EnableItem(st->dlg, IDC_REC_VIDEO_PRESET, hasVideo);
		// Only H.264 has a profile worth choosing; see video_writer.h.
		EnableItem(st->dlg, IDC_REC_PROFILE, hasVideo && videoEncoders::isH264(video));
		EnableItem(st->dlg, IDC_REC_FRAMERATE, hasVideo);
		// The picture box needs a video stream, and the filter only applies to
		// the composed picture: the native one has no filter stage at all.
		EnableItem(st->dlg, IDC_REC_CAPTURE, hasVideo);
		EnableItem(st->dlg, IDC_REC_FILTER, hasVideo && CurrentPicture(st).mode == RECORD_PICTURE_COMPOSED);

		UpdatePresetBoxes(st);

		char summary[256];
		summary[0] = '\0';

		const char *hint = "";

		if (container == NULL)
		{
			strcpy(summary, "Nothing to record: both encoders are set to None.");
		}
		else if (hasVideo)
		{
			const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();

			const PictureEntry picture = CurrentPicture(st);

			size_t width  = dispInfo.customWidth;
			size_t height = dispInfo.customHeight * 2;

			const bool composed = ComposedSize(st, picture, &width, &height);

			const int videoBitrate = (int)GetDlgItemInt(st->dlg, IDC_REC_VIDEO_BITRATE, NULL, FALSE);

			// The rate actually written: the emulator only produces frames at
			// its own rate, and the step decides how many of them get written.
			const double nativeFps = 33513982.0 / 560190.0;
			const int	 step	   = CurrentFrameStep(st);
			const double fps	   = nativeFps / (double)step;

			sprintf(summary, "%s (%s) - %dx%d - %.2f fps - %d kbps",
			        Narrow(container->extension + 1).c_str(), Narrow(container->extension).c_str(),
			        (int)width, (int)height, fps, videoBitrate);

			if (composed)
			{
				hint = "Composed on the CPU: the window layout, the screen gap and the CPU filter, "
				       "at a size frozen when the recording starts.";
			}
			else
			{
				hint = hardware ? "Encoded on the graphics card; the picture never leaves the GPU."
				                : "Encoded on the CPU. Slower, but it works on every machine.";
			}
		}
		else
		{
			const int audioBitrate = (int)GetDlgItemInt(st->dlg, IDC_REC_AUDIO_BITRATE, NULL, FALSE);
			const std::string codec = Narrow(videoEncoders::codecName(audio));

			if (lossless)
			{
				sprintf(summary, "%s (%s) - %s - 44.1 kHz", Narrow(container->extension + 1).c_str(),
				        Narrow(container->extension).c_str(), codec.c_str());
			}
			else
			{
				sprintf(summary, "%s (%s) - %s - 44.1 kHz - %d kbps", Narrow(container->extension + 1).c_str(),
				        Narrow(container->extension).c_str(), codec.c_str(), audioBitrate);
			}

			hint = "Audio only: no video stream is written.";
		}

		SetDlgItemText(st->dlg, IDC_REC_SUMMARY, summary);
		SetDlgItemText(st->dlg, IDC_REC_HINT, hint);

		// A pair of "None"s records nothing.
		EnableItem(st->dlg, IDOK, container != NULL);
	}

	// --- the dialog procedure ----------------------------------------------

	void OnInitDialog(RecordSettingsState *st)
	{
		HWND list = GetDlgItem(st->dlg, IDC_REC_VIDEO);

		// The whole row selects, not just the first column, and the selection
		// stays visible when the list loses focus, so the list reads as a
		// choice rather than as a table.
		ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

		videoEncoders::enumerate(st->video, st->audio);

		FillVideoList(st);
		FillAudioList(st);
		FillPresets(st);
		FillFrameRates(st);
		FillPictures(st);
		FillFilters(st);

		HWND videoSpin = GetDlgItem(st->dlg, IDC_REC_VIDEO_SPIN);
		SendMessage(videoSpin, UDM_SETRANGE32, VIDEO_BITRATE_MIN, VIDEO_BITRATE_MAX);
		SendMessage(videoSpin, UDM_SETPOS32, 0, st->params.videoBitrate);

		HWND audioSpin = GetDlgItem(st->dlg, IDC_REC_AUDIO_SPIN);
		SendMessage(audioSpin, UDM_SETRANGE32, AUDIO_BITRATE_MIN, AUDIO_BITRATE_MAX);
		SendMessage(audioSpin, UDM_SETPOS32, 0, st->params.audioBitrate);

		HWND frameSkip = GetDlgItem(st->dlg, IDC_REC_FRAMERATE);
		for (int i = 0; i < (int)(sizeof(kFrameSteps) / sizeof(kFrameSteps[0])); ++i)
		{
			if (kFrameSteps[i].step == st->params.frameStep)
				ComboBox_SetCurSel(frameSkip, i);
		}

		// Selecting the row fills the profile box with the list belonging to
		// that codec; the saved profile can only be applied after that.
		SelectVideo(st, st->params.videoSubtype, st->params.videoHardware);
		SelectAudio(st, st->params.audioSubtype);
		SelectProfile(st, st->params.profile);
		SelectPicture(st, st->params.pictureMode, st->params.pictureSize);
		SelectFilter(st, st->params.pictureFilter);

		char sampleRate[32];
		sprintf(sampleRate, "%d Hz", DESMUME_SAMPLE_RATE);
		SetDlgItemText(st->dlg, IDC_REC_SAMPLE_RATE, sampleRate);

		UpdateSummary(st);
	}

	INT_PTR CALLBACK RecordSettingsProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		RecordSettingsState *st = (RecordSettingsState *)GetWindowLongPtr(dlg, GWLP_USERDATA);

		switch (msg)
		{
			case WM_INITDIALOG:
			{
				st		 = (RecordSettingsState *)lParam;
				st->dlg	 = dlg;
				SetWindowLongPtr(dlg, GWLP_USERDATA, (LONG_PTR)st);

				OnInitDialog(st);

				// Centre on the emulator window rather than on the screen.
				RECT parent;
				RECT self;
				GetWindowRect(MainWindow->getHWnd(), &parent);
				GetWindowRect(dlg, &self);
				SetWindowPos(dlg, NULL,
				             parent.left + ((parent.right - parent.left) - (self.right - self.left)) / 2,
				             parent.top + ((parent.bottom - parent.top) - (self.bottom - self.top)) / 2,
				             0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

				return TRUE;
			}

			case WM_COMMAND:
			{
				switch (LOWORD(wParam))
				{
					case IDC_REC_AUDIO:
					case IDC_REC_FRAMERATE:
					case IDC_REC_PROFILE:
					case IDC_REC_CAPTURE:
					case IDC_REC_FILTER:
						if (HIWORD(wParam) == CBN_SELCHANGE && st != NULL)
						{
							UpdateSummary(st);
						}
						return TRUE;

					case IDC_REC_VIDEO_PRESET:
					{
						if (HIWORD(wParam) != CBN_SELCHANGE || st == NULL)
							return TRUE;

						const int row = ComboBox_GetCurSel(GetDlgItem(dlg, IDC_REC_VIDEO_PRESET));
						if (row >= 0 && row < 3)
						{
							// Writing the box fires EN_CHANGE, which brings the
							// summary and the preset row back in sync.
							SetDlgItemInt(dlg, IDC_REC_VIDEO_BITRATE, (UINT)kVideoPresets[row], FALSE);
						}
						return TRUE;
					}

					case IDC_REC_AUDIO_PRESET:
					{
						if (HIWORD(wParam) != CBN_SELCHANGE || st == NULL)
							return TRUE;

						const int row = ComboBox_GetCurSel(GetDlgItem(dlg, IDC_REC_AUDIO_PRESET));
						if (row >= 0 && row < 3)
						{
							SetDlgItemInt(dlg, IDC_REC_AUDIO_BITRATE, (UINT)kAudioPresets[row], FALSE);
						}
						return TRUE;
					}

					case IDC_REC_VIDEO_BITRATE:
					case IDC_REC_AUDIO_BITRATE:
						if (HIWORD(wParam) == EN_CHANGE && st != NULL)
						{
							UpdateSummary(st);
						}
						return TRUE;

					case IDOK:
					{
						if (st == NULL)
							return TRUE;

						bool	   hardware = false;
						const GUID video	= CurrentVideo(st, &hardware);
						const GUID audio	= CurrentAudio(st);

						if (videoEncoders::containerFor(video, audio) == NULL)
						{
							return TRUE;
						}

						st->params.videoSubtype	 = video;
						st->params.videoHardware = hardware;
						st->params.audioSubtype	 = audio;
						st->params.videoBitrate	 = clampInt((int)GetDlgItemInt(dlg, IDC_REC_VIDEO_BITRATE, NULL, FALSE),
						                                    VIDEO_BITRATE_MIN, VIDEO_BITRATE_MAX);
						st->params.audioBitrate	 = clampInt((int)GetDlgItemInt(dlg, IDC_REC_AUDIO_BITRATE, NULL, FALSE),
						                                    AUDIO_BITRATE_MIN, AUDIO_BITRATE_MAX);
						st->params.frameStep	 = CurrentFrameStep(st);
						st->params.profile		 = videoEncoders::isNullGuid(video) ? VIDEO_PROFILE_AUTO : CurrentProfile(st);
						st->params.async		 = true;

						const PictureEntry picture = CurrentPicture(st);
						st->params.pictureMode	 = picture.mode;
						st->params.pictureSize	 = picture.size;
						st->params.pictureFilter = CurrentFilter(st);

						EndDialog(dlg, IDOK);
						return TRUE;
					}

					case IDCANCEL:
						EndDialog(dlg, IDCANCEL);
						return TRUE;
				}
				break;
			}

			case WM_NOTIFY:
			{
				const NMHDR *header = (const NMHDR *)lParam;
				if (st == NULL || header->idFrom != IDC_REC_VIDEO || header->code != LVN_ITEMCHANGED)
					break;

				NM_LISTVIEW *view = (NM_LISTVIEW *)lParam;

				if ((view->uChanged & LVIF_STATE) == 0)
					break;

				// The notification arrives twice per change (old state, new
				// state); only the one that leaves the row selected matters.
				if ((view->uNewState & LVIS_SELECTED) == 0)
					break;

				// Keep whatever audio codec was already picked when it is still
				// legal for the new container.
				const GUID audio = CurrentAudio(st);
				FillAudioList(st);
				SelectAudio(st, audio);

				// A different codec means a different set of profiles.
				FillProfileBox(st, CurrentVideo(st, NULL));

				UpdateSummary(st);
				return TRUE;
			}
		}

		return FALSE;
	}
}

// --- public interface ------------------------------------------------------

bool ShowRecordingSettings(HWND parent, RecordParams *params)
{
	RecordSettingsState st;
	st.dlg	  = NULL;
	st.params = *params;

	const INT_PTR result = DialogBoxParam(hAppInst, MAKEINTRESOURCE(IDD_RECORD_SETTINGS), parent,
	                                      RecordSettingsProc, (LPARAM)&st);

	if (result != IDOK)
	{
		return false;
	}

	*params = st.params;
	return true;
}

// The frame box used to store an fps target under "Frame Rate".  Convert one
// of those to a step, so a config written back then keeps the same meaning
// instead of silently recording every frame again.
static int LoadFrameStep()
{
	char value[32];
	value[0] = '\0';

	if (GetPrivateProfileString("Recording", "Frame Step", "", value, sizeof(value), IniName) > 0)
	{
		return clampInt(atoi(value), FRAME_STEP_NONE, FRAME_STEP_MAX);
	}

	if (GetPrivateProfileString("Recording", "Frame Rate", "", value, sizeof(value), IniName) > 0)
	{
		const double nativeFps = 33513982.0 / 560190.0;
		return clampInt(videoEncoders::videoFrameStep(nativeFps, atoi(value)), FRAME_STEP_NONE, FRAME_STEP_MAX);
	}

	return FRAME_STEP_DEFAULT;
}

void LoadRecordingParams(RecordParams *params)
{
	memset(params, 0, sizeof(*params));

	params->videoSubtype  = videoEncoders::defaultVideoCodec();
	params->audioSubtype  = videoEncoders::defaultAudioCodec();
	params->videoBitrate  = GetPrivateProfileInt("Recording", "Video Bitrate", VIDEO_BITRATE_DEFAULT, IniName);
	params->audioBitrate  = GetPrivateProfileInt("Recording", "Audio Bitrate", AUDIO_BITRATE_DEFAULT, IniName);
	params->profile		  = GetPrivateProfileInt("Recording", "Profile", VIDEO_PROFILE_AUTO, IniName);
	params->frameStep	  = LoadFrameStep();
	params->videoHardware = GetPrivateProfileInt("Recording", "Hardware", 0, IniName) != 0;
	params->async		  = true;

	// Picture: what goes into the stream, not just how it is encoded.
	params->pictureMode	  = GetPrivateProfileInt("Recording", "Picture", RECORD_PICTURE_NATIVE, IniName);
	params->pictureSize	  = GetPrivateProfileInt("Recording", "Picture Size", RECORD_SIZE_WINDOW, IniName);
	params->pictureFilter = GetPrivateProfileInt("Recording", "Picture Filter", VideoInfo::NONE, IniName);

	char value[128];
	value[0] = '\0';
	GetPrivateProfileString("Recording", "Video Codec", "", value, sizeof(value), IniName);

	std::wstring wide(value, value + strlen(value));
	if (!videoEncoders::guidFromString(wide, &params->videoSubtype))
	{
		params->videoSubtype = videoEncoders::defaultVideoCodec();
	}

	value[0] = '\0';
	GetPrivateProfileString("Recording", "Audio Codec", "", value, sizeof(value), IniName);

	wide.assign(value, value + strlen(value));
	if (!videoEncoders::guidFromString(wide, &params->audioSubtype))
	{
		params->audioSubtype = videoEncoders::defaultAudioCodec();
	}

	params->videoBitrate = clampInt(params->videoBitrate, VIDEO_BITRATE_MIN, VIDEO_BITRATE_MAX);
	params->audioBitrate = clampInt(params->audioBitrate, AUDIO_BITRATE_MIN, AUDIO_BITRATE_MAX);

	if (params->pictureMode != RECORD_PICTURE_NATIVE && params->pictureMode != RECORD_PICTURE_COMPOSED)
	{
		params->pictureMode = RECORD_PICTURE_NATIVE;
	}

	if (params->pictureSize < RECORD_SIZE_WINDOW || params->pictureSize > RECORD_SIZE_MAX)
	{
		params->pictureSize = RECORD_SIZE_WINDOW;
	}

	if (params->pictureFilter < 0 || params->pictureFilter >= recordCompose::FilterCount())
	{
		params->pictureFilter = VideoInfo::NONE;
	}
}

void SaveRecordingParams(const RecordParams &params)
{
	char number[32];

	sprintf(number, "%d", params.videoBitrate);
	WritePrivateProfileString("Recording", "Video Bitrate", number, IniName);

	sprintf(number, "%d", params.audioBitrate);
	WritePrivateProfileString("Recording", "Audio Bitrate", number, IniName);

	sprintf(number, "%d", params.profile);
	WritePrivateProfileString("Recording", "Profile", number, IniName);

	sprintf(number, "%d", params.frameStep);
	WritePrivateProfileString("Recording", "Frame Step", number, IniName);

	// The old key described the same choice as an fps target.  Leaving it
	// behind would make the ini say two contradictory things.
	WritePrivateProfileString("Recording", "Frame Rate", NULL, IniName);

	WritePrivateProfileString("Recording", "Hardware", params.videoHardware ? "1" : "0", IniName);

	sprintf(number, "%d", params.pictureMode);
	WritePrivateProfileString("Recording", "Picture", number, IniName);

	sprintf(number, "%d", params.pictureSize);
	WritePrivateProfileString("Recording", "Picture Size", number, IniName);

	sprintf(number, "%d", params.pictureFilter);
	WritePrivateProfileString("Recording", "Picture Filter", number, IniName);

	std::wstring video = videoEncoders::isNullGuid(params.videoSubtype)
	                         ? std::wstring(L"none")
	                         : videoEncoders::guidToString(params.videoSubtype);
	std::wstring audio = videoEncoders::isNullGuid(params.audioSubtype)
	                         ? std::wstring(L"none")
	                         : videoEncoders::guidToString(params.audioSubtype);

	WritePrivateProfileString("Recording", "Video Codec", Narrow(video.c_str()).c_str(), IniName);
	WritePrivateProfileString("Recording", "Audio Codec", Narrow(audio.c_str()).c_str(), IniName);
}
