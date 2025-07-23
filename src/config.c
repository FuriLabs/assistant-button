/**
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2025 Bardia Moshiri <bardia@furilabs.com>
 */

#include "config.h"

static const gchar *config_paths[] = {
    "/etc/assistant-button/assistant-button.conf",
    "/usr/lib/furios/device/assistant-button.conf",
    "/usr/share/assistant-button/assistant-button.conf",
    NULL
};

static gchar *
find_config_file(void)
{
    for (gint i = 0; config_paths[i] != NULL; i++) {
        if (g_file_test(config_paths[i], G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
            /* Try to validate the file before using it */
            GKeyFile *test_key_file = g_key_file_new();
            GError *test_error = NULL;

            if (g_key_file_load_from_file(test_key_file, config_paths[i], G_KEY_FILE_NONE, &test_error)) {
                g_debug("Found valid config file: %s", config_paths[i]);
                g_key_file_free(test_key_file);
                return g_strdup(config_paths[i]);
            } else {
                g_debug("Skipping invalid config file %s: %s", config_paths[i], test_error->message);
                g_error_free(test_error);
                g_key_file_free(test_key_file);
            }
        }
    }

    g_debug("No valid system configuration file found");
    return NULL;
}

gboolean
config_load(struct config *config)
{
    GKeyFile *key_file;
    GError *error = NULL;
    gchar *config_file;
    gboolean success = FALSE;

    /* Initialize with defaults */
    config->short_press_max = DEFAULT_SHORT_PRESS_MAX;
    config->double_press_max = DEFAULT_DOUBLE_PRESS_MAX;
    config->assistant_key = DEFAULT_ASSISTANT_KEY;
    config->device = g_strdup(DEFAULT_DEVICE);

    config_file = find_config_file();
    if (config_file == NULL) {
        g_debug("Using default configuration values");
        return TRUE;
    }

    key_file = g_key_file_new();

    if (!g_key_file_load_from_file(key_file, config_file, G_KEY_FILE_NONE, &error)) {
        g_debug("Failed to load config file %s: %s", config_file, error->message);
        g_error_free(error);
        goto cleanup;
    }

    /* Read values from [assistant-button] group */
    if (g_key_file_has_group(key_file, "assistant-button")) {
        if (g_key_file_has_key(key_file, "assistant-button", "short_press_max", NULL)) {
            gint value = g_key_file_get_integer(key_file, "assistant-button", "short_press_max", &error);
            if (error == NULL && value > 0) {
                config->short_press_max = value;
                g_debug("Loaded short_press_max: %d", value);
            } else if (error != NULL) {
                g_warning("Invalid short_press_max value: %s", error->message);
                g_clear_error(&error);
            }
        }

        if (g_key_file_has_key(key_file, "assistant-button", "double_press_max", NULL)) {
            gint value = g_key_file_get_integer(key_file, "assistant-button", "double_press_max", &error);
            if (error == NULL && value > 0) {
                config->double_press_max = value;
                g_debug("Loaded double_press_max: %d", value);
            } else if (error != NULL) {
                g_warning("Invalid double_press_max value: %s", error->message);
                g_clear_error(&error);
            }
        }

        if (g_key_file_has_key(key_file, "assistant-button", "assistant_key", NULL)) {
            gint value = g_key_file_get_integer(key_file, "assistant-button", "assistant_key", &error);
            if (error == NULL && value >= 0 && value <= 65535) {
                config->assistant_key = value;
                g_debug("Loaded assistant_key: %d", value);
            } else if (error != NULL) {
                g_warning("Invalid assistant_key value: %s", error->message);
                g_clear_error(&error);
            } else {
                g_warning("Assistant key value out of range (0-65535): %d", value);
            }
        }

        if (g_key_file_has_key(key_file, "assistant-button", "device", NULL)) {
            gchar *value = g_key_file_get_string(key_file, "assistant-button", "device", &error);
            if (error == NULL && value != NULL && strlen(value) > 0) {
                g_free(config->device);
                config->device = value;
                g_debug("Loaded device: %s", value);
            } else {
                if (error != NULL) {
                    g_warning("Invalid device value: %s", error->message);
                    g_clear_error(&error);
                }
                g_free(value);
            }
        }
    } else {
        g_debug("No [assistant-button] group found in config file");
    }

    success = TRUE;

cleanup:
    g_key_file_free(key_file);
    g_free(config_file);
    return success;
}

void
config_free(struct config *config)
{
    if (config != NULL) {
        g_free(config->device);
        config->device = NULL;
    }
}

gint
config_read_user_int(const gchar *filename)
{
    const gchar *home_dir;
    gchar *file_path;
    gchar *contents;
    gsize length;
    GError *error = NULL;
    gint value = -1;

    home_dir = g_get_home_dir();
    if (home_dir == NULL) {
        g_warning("Could not get home directory");
        return -1;
    }

    file_path = g_build_filename(home_dir, ".config", "assistant-button", filename, NULL);

    if (!g_file_get_contents(file_path, &contents, &length, &error)) {
        if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
            g_debug("Failed to read user config file %s: %s", file_path, error->message);
        g_error_free(error);
        goto cleanup;
    }

    if (length == 0) {
        g_debug("User config file %s is empty", file_path);
        goto cleanup;
    }

    /* Remove trailing whitespace/newlines */
    g_strstrip(contents);

    /* Parse integer */
    gchar *endptr;
    glong parsed_value = g_ascii_strtoll(contents, &endptr, 10);

    if (endptr == contents || *endptr != '\0') {
        g_warning("Invalid integer in user config file %s: %s", file_path, contents);
        goto cleanup;
    }

    if (parsed_value < G_MININT || parsed_value > G_MAXINT) {
        g_warning("Integer out of range in user config file %s: %ld", file_path, parsed_value);
        goto cleanup;
    }

    value = (gint)parsed_value;
    g_debug("Read user config %s: %d", filename, value);

cleanup:
    g_free(file_path);
    g_free(contents);
    return value;
}

gchar *
config_read_user_action(const gchar *filename)
{
    const gchar *home_dir;
    gchar *file_path;
    gchar *contents;
    gsize length;
    GError *error = NULL;

    home_dir = g_get_home_dir();
    if (home_dir == NULL) {
        g_warning("Could not get home directory");
        return NULL;
    }

    file_path = g_build_filename(home_dir, ".config", "assistant-button", filename, NULL);

    if (!g_file_get_contents(file_path, &contents, &length, &error)) {
        if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
            g_debug("Failed to read user action file %s: %s", file_path, error->message);
        g_error_free(error);
        g_free(file_path);
        return NULL;
    }

    g_free(file_path);

    if (length == 0) {
        g_free(contents);
        return NULL;
    }

    /* Remove trailing whitespace/newlines */
    g_strstrip(contents);

    if (strlen(contents) == 0) {
        g_free(contents);
        return NULL;
    }

    g_debug("Read user action %s: %s", filename, contents);
    return contents;
}
