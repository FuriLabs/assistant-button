// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024 Bardia Moshiri <fakeshell@bardia.tech>

#include <gio/gio.h>
#include "utils.h"

void run_command(const char *command) {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", command, NULL);
        _exit(127);
    }
}

void show_notification(const char *summary, const char *body) {
    GDBusConnection *connection;
    GError *error = NULL;

    connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (connection == NULL) {
        g_printerr("Failed to get session bus: %s\n", error->message);
        g_error_free(error);
        return;
    }

    g_dbus_connection_call_sync(
        connection,
        "org.freedesktop.Notifications",
        "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications",
        "Notify",
        g_variant_new("(susssasa{sv}i)", "Assistant Button", 0, "", summary, body, NULL, NULL, -1),
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );

    if (error != NULL) {
        g_printerr("Failed to show notification: %s\n", error->message);
        g_error_free(error);
    }

    g_object_unref(connection);
}
