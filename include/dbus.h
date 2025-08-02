/**
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2025 Bardia Moshiri <bardia@furilabs.com>
 */

#ifndef DBUS_H
#define DBUS_H

#include <gio/gio.h>

enum ButtonEvent {
    SHORT_PRESS = 1,
    LONG_PRESS = 2,
    DOUBLE_PRESS = 3
};

/**
 * Initialize D-Bus connection and acquire service name.
 *
 * @param owner_id    Pointer to store the D-Bus name owner ID for cleanup.
 * @return            GDBusConnection pointer on success, NULL on failure.
 */
GDBusConnection *
dbus_init(guint *owner_id);

/**
 * Emit ActionPerformed signal over D-Bus.
 *
 * @param connection  GDBus connection object.
 * @param action      Action identifier.
 * @param event_type  Button event type (SHORT_PRESS, LONG_PRESS, DOUBLE_PRESS).
 */
void
dbus_emit_signal(GDBusConnection *connection, gint action, gint event_type);

/**
 * Clean up D-Bus connection and release owned name.
 *
 * @param connection  GDBus connection to clean up.
 */
void
dbus_cleanup(GDBusConnection *connection);

/**
 * Release D-Bus name ownership.
 *
 * @param owner_id    D-Bus name owner ID to release.
 */
void
dbus_release_name(guint owner_id);

#endif /* DBUS_H */
