/*
	configdialog.cpp - unified Emulation/Graphic/Sound settings for GTK4

	Copyright (C) 2026 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	The dialog follows the Windows settings dialogs: one window with three
	tabs, each tab built from labelled group boxes.  GTK3 shipped two separate
	dialogs (Emulation Settings and Graphics Settings) plus audio switches in
	the menu; this window collects all of them, so the values are applied and
	saved in one place.
*/

#include "configdialog.h"

#include <string.h>

#include "emulation.h"
#include "../../shared/wgpu_present.h"

namespace
{

/* Sentinel in presentationMasks[] for the "Software (Cairo)" row. Real
 * WGPUInstanceBackend masks are non-zero bit flags, so 0 is safe here. */
static const guint32 PRESENTATION_SOFTWARE = 0;

struct ConfigDialog
{
	GtkWidget *window;

	/* Emulation tab. */
	GtkWidget *useJit;
	GtkWidget *jitBlockSize;

	/* Graphic tab. */
	GtkWidget *rendererDrop;
	GtkWidget *textureScaleDrop;
	GtkWidget *gpuScaleSpin;
	GtkWidget *deposterize;
	GtkWidget *smoothing;
	GtkWidget *highColorInterpolation;
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
};

ConfigDialog *sDialog = NULL;

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
	return gtk_drop_down_get_selected(GTK_DROP_DOWN(drop));
}

/* ------------------------------------------------------------- Emulation tab */

GtkWidget *
BuildEmulationTab(ConfigDialog *d)
{
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	GtkWidget *grid = NULL;

	gtk_widget_set_margin_start(box, 12);
	gtk_widget_set_margin_end(box, 12);
	gtk_widget_set_margin_top(box, 12);
	gtk_widget_set_margin_bottom(box, 12);

	GtkWidget *section = NewSection("CPU emulation mode", &grid);
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

	return box;
}

/* ------------------------------------------------------------- Graphic tab */

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
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	GtkWidget *grid = NULL;

	gtk_widget_set_margin_start(box, 12);
	gtk_widget_set_margin_end(box, 12);
	gtk_widget_set_margin_top(box, 12);
	gtk_widget_set_margin_bottom(box, 12);

	/* 3D rendering engine. */
	GtkWidget *section = NewSection("3D rendering engine", &grid);
	gtk_box_append(GTK_BOX(box), section);

	GridLabel(GTK_GRID(grid), "3D core:", 0);
	{
		GtkStringList *renderers = gtk_string_list_new(NULL);
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

	/* SoftRasterizer options. */
	section = NewSection("SoftRasterizer options", &grid);
	gtk_box_append(GTK_BOX(box), section);

	d->highColorInterpolation = gtk_check_button_new_with_label(
		"High-precision color interpolation");
	gtk_grid_attach(GTK_GRID(grid), d->highColorInterpolation, 0, 0, 2, 1);

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
		GtkStringList *presentations = gtk_string_list_new(NULL);
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
		GtkStringList *filters = gtk_string_list_new(NULL);
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

/* -------------------------------------------------------------- Sound tab */

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
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	GtkWidget *grid = NULL;

	gtk_widget_set_margin_start(box, 12);
	gtk_widget_set_margin_end(box, 12);
	gtk_widget_set_margin_top(box, 12);
	gtk_widget_set_margin_bottom(box, 12);

	/* Output. */
	GtkWidget *section = NewSection("Audio output", &grid);
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

/* ------------------------------------------------------------------ apply */

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

void
ApplyAndSave(ConfigDialog *d)
{
	desmume::config::Config *config = d->config;

	/* Emulation. */
	const gboolean useJit =
		gtk_check_button_get_active(GTK_CHECK_BUTTON(d->useJit));
	const int blockSize =
		(int)(gtk_range_get_value(GTK_RANGE(d->jitBlockSize)) + 0.5);
	desmume_emu_set_jit_config(useJit, blockSize);
	config->use_jit = (useJit != FALSE);
	config->jit_max_block_size = blockSize;

	/* Graphics. */
	const guint rendererIndex = GetDropDown(d->rendererDrop);
	const int renderer = desmume_emu_renderer_id((int)rendererIndex);

	const guint scaleIndex = GetDropDown(d->textureScaleDrop);
	const int textureUpscale = (scaleIndex == 1) ? 2 : (scaleIndex == 2) ? 4 : 1;

	const float gpuScale =
		(float)gtk_spin_button_get_value(GTK_SPIN_BUTTON(d->gpuScaleSpin));
	const gboolean deposterize =
		gtk_check_button_get_active(GTK_CHECK_BUTTON(d->deposterize));
	const gboolean smoothing =
		gtk_check_button_get_active(GTK_CHECK_BUTTON(d->smoothing));
	const gboolean highColor =
		gtk_check_button_get_active(GTK_CHECK_BUTTON(d->highColorInterpolation));

	/* Multisampling is not exposed in this dialog; keep the stored value and
	 * pass it through unchanged. */
	const int multisample = config->multisampling.get()
		? config->multisamplingSize.get() : 0;

	desmume_emu_set_graphics_config(renderer, textureUpscale,
	                                deposterize, smoothing, highColor,
	                                multisample);

	/* The bridge falls back to SoftRasterizer when the requested renderer
	 * cannot start; persist what is actually running. */
	config->core3D = desmume_emu_get_renderer();
	config->textureUpscale = textureUpscale;
	config->gpuScaleFactor = gpuScale;
	config->textureDeposterize = (deposterize != FALSE);
	config->textureSmoothing = (smoothing != FALSE);
	config->highColorInterpolation = (highColor != FALSE);

	/* Screen presentation.  The chosen row maps to a WGPUInstanceBackend mask;
	 * the software row disables the WebGPU path entirely (filter forced Off),
	 * exactly like GTK3's "Software (Cairo)" entry. */
	const guint presentationRow = GetDropDown(d->presentationDrop);
	const guint32 backendMask =
		(presentationRow < (guint)d->presentationCount)
			? d->presentationMasks[presentationRow]
			: PRESENTATION_SOFTWARE;
	const gboolean presentationWGPU = (backendMask != PRESENTATION_SOFTWARE);

	desmume_emu_set_presentation_backend(backendMask);

	int filter = presentationWGPU ? SelectedFilter(d) : WGPUPresentUpscaler_Off;
	if (filter != WGPUPresentUpscaler_Off &&
	    !desmume_emu_set_gpu_filter(filter, gpuScale))
	{
		filter = WGPUPresentUpscaler_Off;
		desmume_emu_set_gpu_filter(filter, 1.0f);
	}
	else if (filter == WGPUPresentUpscaler_Off)
	{
		desmume_emu_set_gpu_filter(WGPUPresentUpscaler_Off, 1.0f);
	}
	config->gpuFilter = filter;
	config->presentationWGPU = (presentationWGPU &&
	                            filter != WGPUPresentUpscaler_Off);
	config->presentationWGPUBackend = (int)backendMask;

	/* Sound. */
	const gboolean audioEnabled =
		gtk_check_button_get_active(GTK_CHECK_BUTTON(d->audioEnabled));
	const int volume =
		(int)(gtk_range_get_value(GTK_RANGE(d->audioVolume)) + 0.5);

	int syncMode = 0;
	if (gtk_check_button_get_active(GTK_CHECK_BUTTON(d->syncSync)))
	{
		if ((d->syncP != NULL) &&
		    gtk_check_button_get_active(GTK_CHECK_BUTTON(d->syncP)))
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

	const int interpolation = (int)GetDropDown(d->interpolationDrop);

	desmume_emu_set_audio_enabled(audioEnabled);
	desmume_emu_set_audio_volume(volume);
	desmume_emu_set_audio_sync(syncMode);
	desmume_emu_set_audio_interpolation(interpolation);

	config->audio_enabled = (audioEnabled != FALSE);
	config->audio_volume = volume;
	config->audio_sync = syncMode;
	config->audio_interpolation = interpolation;

	config->save();

	if (d->applied != NULL)
	{
		d->applied(d->appliedData);
	}
}

/* -------------------------------------------------------------- callbacks */

void
OnOkClicked(GtkButton *button, gpointer user_data)
{
	(void)button;
	ConfigDialog *d = (ConfigDialog *)user_data;
	ApplyAndSave(d);
	gtk_window_destroy(GTK_WINDOW(d->window));
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
RendererIndexForId(ConfigDialog *d, int id)
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

void
Populate(ConfigDialog *d)
{
	desmume::config::Config *config = d->config;

	/* Emulation. */
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->useJit),
	                            config->use_jit.get() ? TRUE : FALSE);
	int blockSize = config->jit_max_block_size.get();
	if (blockSize < 1) blockSize = 1;
	if (blockSize > 100) blockSize = 100;
	gtk_range_set_value(GTK_RANGE(d->jitBlockSize), blockSize);

	/* Graphics. */
	SetDropDown(d->rendererDrop, (guint)RendererIndexForId(d, config->core3D.get()));

	const int textureUpscale = config->textureUpscale.get();
	SetDropDown(d->textureScaleDrop,
	            (textureUpscale == 2) ? 1 : (textureUpscale == 4) ? 2 : 0);

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
		if (presentationRow == (guint)(d->presentationCount - 1) &&
		    d->presentationCount > 1)
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

	const int filter = config->presentationWGPU.get() ? config->gpuFilter.get() : 0;
	SetDropDown(d->filterDrop, (guint)FilterIndexForValue(d, filter));

	/* Sound. */
	gtk_check_button_set_active(GTK_CHECK_BUTTON(d->audioEnabled),
	                            config->audio_enabled.get() ? TRUE : FALSE);

	int volume = config->audio_volume.get();
	if (volume < 0) volume = 0;
	if (volume > 100) volume = 100;
	gtk_range_set_value(GTK_RANGE(d->audioVolume), volume);

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

	int interpolation = config->audio_interpolation.get();
	if ((interpolation < 0) || (interpolation > 2))
	{
		interpolation = 1;
	}
	SetDropDown(d->interpolationDrop, (guint)interpolation);
}

} /* namespace */

void
desmume_config_dialog_show(GtkWindow *parent,
                           desmume::config::Config *config,
                           DesmumeConfigAppliedFunc applied,
                           gpointer user_data)
{
	if (sDialog != NULL)
	{
		gtk_window_present(GTK_WINDOW(sDialog->window));
		return;
	}

	ConfigDialog *d = g_new0(ConfigDialog, 1);
	d->config = config;
	d->applied = applied;
	d->appliedData = user_data;

	d->window = gtk_window_new();
	gtk_window_set_title(GTK_WINDOW(d->window), "DeSmuME Configuration");
	gtk_window_set_transient_for(GTK_WINDOW(d->window), parent);
	gtk_window_set_modal(GTK_WINDOW(d->window), TRUE);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(d->window), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(d->window), 640, 660);

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_window_set_child(GTK_WINDOW(d->window), root);

	GtkWidget *notebook = gtk_notebook_new();
	gtk_widget_set_vexpand(notebook, TRUE);
	gtk_box_append(GTK_BOX(root), notebook);

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), BuildEmulationTab(d),
	                         gtk_label_new("Emulation"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), BuildGraphicTab(d),
	                         gtk_label_new("Graphic"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), BuildSoundTab(d),
	                         gtk_label_new("Sound"));

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
	g_signal_connect(ok, "clicked", G_CALLBACK(OnOkClicked), d);
	gtk_box_append(GTK_BOX(buttons), ok);
	gtk_window_set_default_widget(GTK_WINDOW(d->window), ok);

	Populate(d);

	sDialog = d;
	/* GTK owns the window's only reference.  Adding a ref_sink here would
	 * keep it alive after gtk_window_destroy() and stop "destroy" from ever
	 * firing, so sDialog would dangle and the next show() would present a
	 * destroyed window. */
	g_signal_connect(d->window, "destroy", G_CALLBACK(OnDialogDestroy), d);
	gtk_window_present(GTK_WINDOW(d->window));
}
