/* Shared GTK memory viewer font and grid metrics. */
#ifndef DESMUME_GTK_MEMORY_VIEWER_LAYOUT_H
#define DESMUME_GTK_MEMORY_VIEWER_LAYOUT_H

#include <gtk/gtk.h>

static GSettings *
memory_viewer_font_settings(void)
{
	GSettingsSchemaSource *source = g_settings_schema_source_get_default();
	if (source == NULL)
		return NULL;
	GSettingsSchema *schema = g_settings_schema_source_lookup(source, "org.gnome.desktop.interface", TRUE);
	if (schema == NULL)
		return NULL;
	GSettings *settings = g_settings_schema_has_key(schema, "monospace-font-name")
		? g_settings_new_full(schema, NULL, NULL) : NULL;
	g_settings_schema_unref(schema);
	return settings;
}

static PangoLayout *
memory_viewer_layout_new(GtkWidget *widget, GSettings *settings)
{
	PangoLayout *layout = gtk_widget_create_pango_layout(widget, NULL);
	PangoFontDescription *font;
	if (settings != NULL)
	{
		gchar *name = g_settings_get_string(settings, "monospace-font-name");
		font = pango_font_description_from_string(name);
		g_free(name);
	}
	else
	{
		font = pango_font_description_copy(pango_context_get_font_description(
			gtk_widget_get_pango_context(widget)));
		pango_font_description_set_family(font, "monospace");
	}
	pango_layout_set_font_description(layout, font);
	pango_font_description_free(font);
	return layout;
}

static void
memory_viewer_metrics(PangoLayout *layout, double *char_w, double *line_h, double *header_h)
{
	pango_layout_set_text(layout, "0", -1);
	int width, height;
	pango_layout_get_pixel_size(layout, &width, &height);
	*char_w = MAX(width, 1);
	*line_h = MAX(height, 1) + 4;
	*header_h = *line_h + 2;
}

static void
memory_viewer_set_min_width(GtkWidget *widget, GSettings *settings)
{
	PangoLayout *layout = memory_viewer_layout_new(widget, settings);
	double char_w, line_h, header_h;
	memory_viewer_metrics(layout, &char_w, &line_h, &header_h);
	g_object_unref(layout);
	/* Byte mode is the widest: address, hex cells, and 16 character cells. */
	gtk_widget_set_size_request(widget, MAX(620, (int)(16 + 76 * char_w)), 280);
}

/* Render each character on the same grid, even if a fallback glyph is wider. */
static void
memory_viewer_draw_cells(cairo_t *cr, PangoLayout *layout, const char *text,
                         double x, double y, double char_w, double line_h)
{
	for (const char *p = text; *p != '\0'; p = g_utf8_next_char(p), x += char_w)
	{
		const char *next = g_utf8_next_char(p);
		pango_layout_set_text(layout, p, (int)(next - p));
		cairo_save(cr);
		cairo_rectangle(cr, x, y, char_w, line_h);
		cairo_clip(cr);
		cairo_move_to(cr, x, y);
		pango_cairo_show_layout(cr, layout);
		cairo_restore(cr);
	}
}

#endif
