/* GTK desktop monospace font settings shared by tool windows. */
#ifndef DESMUME_GTK_MONOSPACE_FONT_H
#define DESMUME_GTK_MONOSPACE_FONT_H

#include <gtk/gtk.h>

static inline GSettings *
gtk_monospace_font_settings_new(void)
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

static inline PangoFontDescription *
gtk_monospace_font_description(GtkWidget *widget, GSettings *settings)
{
	if (settings != NULL)
	{
		gchar *name = g_settings_get_string(settings, "monospace-font-name");
		PangoFontDescription *font = pango_font_description_from_string(name);
		g_free(name);
		return font;
	}

	PangoFontDescription *font = pango_font_description_copy(pango_context_get_font_description(
		gtk_widget_get_pango_context(widget)));
	pango_font_description_set_family(font, "monospace");
	return font;
}

#endif
