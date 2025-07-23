/**
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2025 Bardia Moshiri <bardia@furilabs.com>
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <glib.h>

#define DEFAULT_SHORT_PRESS_MAX 500  /* ms */
#define DEFAULT_DOUBLE_PRESS_MAX 200  /* ms */
#define DEFAULT_DEVICE "/dev/input/event1"
#define DEFAULT_ASSISTANT_KEY 112

struct config {
    gint short_press_max;
    gint double_press_max;
    gint assistant_key;
    gchar *device;
};

/**
 * Load configuration from system configuration files.
 * Tries files in priority order: /etc -> /usr/lib/furios/device -> /usr/share/assistant-button
 *
 * @param config  Configuration structure to populate.
 * @return        TRUE on success, FALSE on failure.
 */
gboolean
config_load(struct config *config);

/**
 * Free configuration resources.
 *
 * @param config  Configuration structure to free.
 */
void
config_free(struct config *config);

/**
 * Read integer value from user configuration file.
 *
 * @param filename  Configuration filename in ~/.config/assistant-button/
 * @return          Integer value, or -1 on error.
 */
gint
config_read_user_int(const gchar *filename);

/**
 * Read custom action command from user configuration file.
 *
 * @param filename  Configuration filename in ~/.config/assistant-button/
 * @return          Command string (must be freed), or NULL if not found.
 */
gchar *
config_read_user_action(const gchar *filename);

#endif // CONFIG_H
