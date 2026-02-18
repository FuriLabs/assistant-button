/**
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2026 Bardia Moshiri <bardia@furilabs.com>
 */

#include "dbus.h"

#define DBUS_INTERFACE "io.FuriOS.AssistantButton"
#define DBUS_OBJECT_PATH "/io/FuriOS/AssistantButton"

GDBusConnection *
dbus_init(guint *owner_id)
{
    GError *error = NULL;
    GDBusConnection *connection;

    connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (connection == NULL) {
        g_printerr("Failed to get session bus: %s\n", error->message);
        g_error_free(error);
        return NULL;
    }

    *owner_id = g_bus_own_name_on_connection(
        connection,
        DBUS_INTERFACE,
        G_BUS_NAME_OWNER_FLAGS_REPLACE,
        NULL,
        NULL,
        NULL,
        NULL
    );

    if (*owner_id == 0) {
        g_printerr("Failed to own D-Bus name: %s\n", DBUS_INTERFACE);
        g_object_unref(connection);
        return NULL;
    }

    g_debug("D-Bus connection initialized successfully");
    return connection;
}

void
dbus_emit_signal(GDBusConnection *connection, gint action, gint event_type)
{
    GError *error = NULL;
    GVariant *parameters;

    if (connection == NULL) {
        g_warning("D-Bus connection is NULL, cannot emit signal\n");
        return;
    }

    parameters = g_variant_new("(ii)", action, event_type);

    gboolean result = g_dbus_connection_emit_signal(
        connection,
        NULL,  /* destination */
        DBUS_OBJECT_PATH,
        DBUS_INTERFACE,
        "ActionPerformed",
        parameters,
        &error
    );

    if (!result) {
        g_printerr("Failed to emit D-Bus signal: %s\n", error->message);
        g_error_free(error);
    } else {
        g_debug("Emitted ActionPerformed signal: action=%d, event_type=%d",
                action, event_type);
    }
}

void
dbus_cleanup(GDBusConnection *connection)
{
    if (connection != NULL) {
        g_object_unref(connection);
        g_debug("D-Bus connection cleaned up");
    }
}
