/* main.c - this file is part of DeSmuME
 *
 * Copyright (C) 2009 DeSmuME Team
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

#include <glib.h>
#include <gdk/gdkkeysyms.h>

#include "ctrlssdl.h"
#include "desmume_config.h"

static const gchar *desmume_config_file = ".desmume.ini";
static const u16 gtk_kb_cfg[NB_KEYS] = {
    GDK_x,         // A
    GDK_z,         // B
    GDK_Shift_R,   // select
    GDK_Return,    // start
    GDK_Right,     // Right
    GDK_Left,      // Left
    GDK_Up,        // Up
    GDK_Down,      // Down       
    GDK_w,         // R
    GDK_q,         // L
    GDK_s,         // X
    GDK_a,         // Y
    GDK_p,         // DEBUG
    GDK_o          // BOOST
};


GKeyFile *desmume_config_read_file()
{
    gchar *config_file;
    GKeyFile *keyfile;
    GError *error = NULL;
    gboolean ret;

    config_file = g_build_filename(g_get_home_dir(), desmume_config_file, NULL);
    keyfile = g_key_file_new();
    ret = g_key_file_load_from_file(keyfile, config_file, G_KEY_FILE_NONE, &error);
    if (!ret) {
        g_error_free(error);
    }

    g_free(config_file);

    load_default_config(gtk_kb_cfg);
    desmume_config_read_keys(keyfile);
    desmume_config_read_joykeys(keyfile);

    return keyfile;
}

void desmume_config_dispose(GKeyFile *keyfile)
{
    g_key_file_free(keyfile);
}

static gboolean desmume_config_write_file(GKeyFile *keyfile)
{
    gchar *config_file;
    gchar *data;
    GError *error = NULL;
    gsize length;
    gboolean ret = TRUE;

    config_file = g_build_filename(g_get_home_dir(), desmume_config_file, NULL);
    data = g_key_file_to_data(keyfile, &length, NULL);
    ret = g_file_set_contents(config_file, data, length, &error);
    if (!ret) {
      g_error_free(error);
    }

    g_free(config_file);
    g_free(data);

    return ret;
}

gboolean desmume_config_update_keys(GKeyFile *keyfile)
{
    for(int i = 0; i < NB_KEYS; i++) {
        g_key_file_set_integer(keyfile, "KEYS", key_names[i], keyboard_cfg[i]);
    }

    return desmume_config_write_file(keyfile);
}

gboolean desmume_config_update_joykeys(GKeyFile *keyfile)
{
    for(int i = 0; i < NB_KEYS; i++) {
        g_key_file_set_integer(keyfile, "JOYKEYS", key_names[i], joypad_cfg[i]);
    }

    return desmume_config_write_file(keyfile);
}

gboolean desmume_config_read_keys(GKeyFile *keyfile)
{
    GError *error = NULL;

    if (!g_key_file_has_group(keyfile, "KEYS"))
        return TRUE;

    for (int i = 0; i < NB_KEYS; i++) {
        keyboard_cfg[i] = g_key_file_get_integer(keyfile, "KEYS", key_names[i], &error);
        if (error != NULL) {
            g_error_free(error);
            return FALSE;
        }
    }

    return TRUE;
}

gboolean desmume_config_read_joykeys(GKeyFile *keyfile)
{
    GError *error = NULL;

    if (!g_key_file_has_group(keyfile, "JOYKEYS"))
        return TRUE;

    for (int i = 0; i < NB_KEYS; i++) {
        joypad_cfg[i] = g_key_file_get_integer(keyfile, "JOYKEYS", key_names[i], &error);
        if (error != NULL) {
            g_error_free(error);
            return FALSE;
        }
    }

    return TRUE;
}
