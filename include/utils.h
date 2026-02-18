/**
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2026 Bardia Moshiri <bardia@furilabs.com>
 */

#ifndef UTILS_H
#define UTILS_H

#include <gio/gio.h>

/**
 * Execute a shell command in a child process.
 * Forks and uses execl to run the command via /bin/sh.
 *
 * @param command  Shell command string to execute.
 */
void
run_command(const char *command);

/**
 * Display a desktop notification using D-Bus.
 * Sends notification via org.freedesktop.Notifications service.
 *
 * @param summary  Notification title/summary text.
 * @param body     Notification body text.
 */
void
show_notification(const char *summary, const char *body);

#endif // UTILS_H
