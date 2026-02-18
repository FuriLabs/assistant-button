/**
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2026 Bardia Moshiri <bardia@furilabs.com>
 */

#ifndef ACTIONS_H
#define ACTIONS_H

/**
 * Toggle flashlight on/off based on current brightness and screen status.
 * If screen is on: toggles brightness (0 <-> 100).
 * If screen is off: forces brightness to 0 (off).
 *
 * @param screen_on TRUE if the screen is on, FALSE otherwise.
 */
void
handle_flashlight(gboolean screen_on);

/**
 * Launch the FuriOS camera application.
 */
void
open_camera(void);

/**
 * Take a picture using the device camera and save it to ~/Pictures.
 * Uses GStreamer pipeline with droidcamsrc.
 */
void
take_picture(void);

/**
 * Take a screenshot using GNOME Shell Screenshot service.
 * Saves to ~/Pictures/Screenshots with timestamp.
 */
void
take_screenshot(void);

/**
 * Send a virtual key press using Wayland virtual keyboard protocol.
 *
 * @param name  XKB keysym name (case insensitive).
 */
void
send_key(const char *name);

/**
 * Manually trigger screen rotation by temporarily disabling and
 * re-enabling orientation lock.
 */
void
manual_autorotate(void);

#endif // ACTIONS_H
