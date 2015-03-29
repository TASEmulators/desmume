/*
	Copyright (C) 2009-2010 DeSmuME team

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

#include <glib.h>
#include <glib/gstdio.h>

#include "ctrlssdl.h"
#include "desmume_config.h"

static const gchar *desmume_old_config_file = ".desmume.ini";
static const gchar *desmume_config_dir = "desmume";
static const gchar *desmume_config_file = "config";

GKeyFile *desmume_config_read_file(const u16 *kb_cfg)
{
    gchar *config_file, *config_dir, *old_config_file;
    GKeyFile *keyfile;
    GError *error = NULL;
    gboolean ret;

    old_config_file = g_build_filename(g_get_home_dir(), desmume_old_config_file, NULL);
    config_file = g_build_filename(g_get_user_config_dir(), desmume_config_dir, desmume_config_file, NULL);

    config_dir = g_build_filename(g_get_user_config_dir(), desmume_config_dir, NULL);
    g_mkdir_with_parents(config_dir, 0755);

    if (!g_file_test(config_file, G_FILE_TEST_IS_REGULAR) && g_file_test(old_config_file, G_FILE_TEST_IS_REGULAR)) {
        ret = g_rename(old_config_file, config_file);
        if (ret) {
            g_printerr("Failed to move old config file %s to new location %s \n", old_config_file, config_file);
        }
    }

    keyfile = g_key_file_new();
    ret = g_key_file_load_from_file(keyfile, config_file, G_KEY_FILE_NONE, &error);
    if (!ret) {
        g_error_free(error);
    }

    g_free(config_file);
    g_free(config_dir);
    g_free(old_config_file);

    load_default_config(kb_cfg);
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
    gchar *config_dir;
    gchar *data;
    GError *error = NULL;
    gsize length;
    gboolean ret = TRUE;

    config_dir = g_build_filename(g_get_user_config_dir(), desmume_config_dir, NULL);
    g_mkdir_with_parents(config_dir, 0755);
    config_file = g_build_filename(g_get_user_config_dir(), desmume_config_dir, desmume_config_file, NULL);
    data = g_key_file_to_data(keyfile, &length, NULL);
    ret = g_file_set_contents(config_file, data, length, &error);
    if (!ret) {
      g_error_free(error);
    }

    g_free(config_file);
    g_free(config_dir);
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
