/**
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2019 Josef Gajdusek
 * Copyright (C) 2025 Bardia Moshiri <bardia@furilabs.com>
 */

#ifndef VIRTKEY_H
#define VIRTKEY_H

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>
#include "virtual-keyboard-unstable-v1-client-protocol.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

enum wtype_command_type {
    WTYPE_COMMAND_TEXT = 0,
    WTYPE_COMMAND_MOD_PRESS = 1,
    WTYPE_COMMAND_MOD_RELEASE = 2,
    WTYPE_COMMAND_KEY_PRESS = 3,
    WTYPE_COMMAND_KEY_RELEASE = 4,
};

enum wtype_mod {
    WTYPE_MOD_NONE = 0,
    WTYPE_MOD_SHIFT = 1,
    WTYPE_MOD_CAPSLOCK = 2,
    WTYPE_MOD_CTRL = 4,
    WTYPE_MOD_ALT = 8,
    WTYPE_MOD_LOGO = 64,
    WTYPE_MOD_ALTGR = 128
};

struct wtype_command {
    enum wtype_command_type type;
    union {
        struct {
            unsigned int *key_codes;
            size_t key_codes_len;
            unsigned int delay_ms;
        };
        unsigned int single_key_code;
        enum wtype_mod mod;
        unsigned int sleep_ms;
    };
};

struct keymap_entry {
    xkb_keysym_t xkb;
    wchar_t wchr;
};

struct wtype {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_seat *seat;
    struct zwp_virtual_keyboard_manager_v1 *manager;
    struct zwp_virtual_keyboard_v1 *keyboard;
    size_t keymap_len;
    struct keymap_entry *keymap;
    uint32_t mod_status;
    size_t command_count;
    struct wtype_command *commands;
};

static const struct { const char *name; enum wtype_mod mod; } mod_names[] = {
    {"shift", WTYPE_MOD_SHIFT},
    {"capslock", WTYPE_MOD_CAPSLOCK},
    {"ctrl", WTYPE_MOD_CTRL},
    {"logo", WTYPE_MOD_LOGO},
    {"win", WTYPE_MOD_LOGO},
    {"alt", WTYPE_MOD_ALT},
    {"altgr", WTYPE_MOD_ALTGR},
};

extern const struct wl_registry_listener registry_listener;

/**
 * Upload XKB keymap to the virtual keyboard.
 * Creates temporary keymap file and sends it via Wayland protocol.
 *
 * @param wtype  Pointer to wtype structure containing keymap data.
 */
void
upload_keymap(struct wtype *wtype);

/**
 * Handle Wayland registry global events.
 * Binds to seat and virtual keyboard manager interfaces.
 *
 * @param data       Pointer to wtype structure.
 * @param registry   Wayland registry object.
 * @param name       Global object name.
 * @param interface  Interface name string.
 * @param version    Interface version.
 */
void
handle_wl_event(void *data, struct wl_registry *registry, uint32_t name, 
                const char *interface, uint32_t version);

/**
 * Handle Wayland registry global remove events.
 *
 * @param data      Pointer to wtype structure.
 * @param registry  Wayland registry object.
 * @param name      Global object name being removed.
 */
void
handle_wl_event_remove(void *data, struct wl_registry *registry, uint32_t name);

/**
 * Convert modifier name string to wtype_mod enum value.
 *
 * @param name  Modifier name string (case insensitive).
 * @return      Corresponding wtype_mod value or WTYPE_MOD_NONE if not found.
 */
enum wtype_mod
name_to_mod(const char *name);

/**
 * Add a new keymap entry and return its key code.
 *
 * @param wtype  Pointer to wtype structure.
 * @param ch     Wide character.
 * @param xkb    XKB keysym.
 * @return       Key code for the new entry.
 */
unsigned int
append_keymap_entry(struct wtype *wtype, wchar_t ch, xkb_keysym_t xkb);

/**
 * Get key code for a wide character, creating keymap entry if needed.
 *
 * @param wtype  Pointer to wtype structure.
 * @param ch     Wide character to look up.
 * @return       Key code for the character.
 */
unsigned int
get_key_code_by_wchar(struct wtype *wtype, wchar_t ch);

/**
 * Get key code for an XKB keysym, creating keymap entry if needed.
 *
 * @param wtype  Pointer to wtype structure.
 * @param xkb    XKB keysym to look up.
 * @return       Key code for the keysym.
 */
unsigned int
get_key_code_by_xkb(struct wtype *wtype, xkb_keysym_t xkb);

/**
 * Execute modifier press/release command.
 *
 * @param wtype  Pointer to wtype structure.
 * @param cmd    Modifier command to execute.
 */
void
run_mod(struct wtype *wtype, struct wtype_command *cmd);

/**
 * Execute key press/release command.
 *
 * @param wtype  Pointer to wtype structure.
 * @param cmd    Key command to execute.
 */
void
run_key(struct wtype *wtype, struct wtype_command *cmd);

/**
 * Type a single key code (press and release with delay).
 *
 * @param wtype     Pointer to wtype structure.
 * @param key_code  Key code to type.
 */
void
type_keycode(struct wtype *wtype, unsigned int key_code);

/**
 * Execute text input command by typing each character.
 *
 * @param wtype  Pointer to wtype structure.
 * @param cmd    Text command containing key codes and delay.
 */
void
run_text(struct wtype *wtype, struct wtype_command *cmd);

/**
 * Execute all commands in the wtype structure.
 *
 * @param wtype  Pointer to wtype structure containing commands.
 */
void
run_commands(struct wtype *wtype);

/**
 * Print XKB keysym name to file stream.
 *
 * @param keysym  XKB keysym to print.
 * @param f       File stream to write to.
 */
void
print_keysym_name(xkb_keysym_t keysym, FILE *f);

#endif // VIRTKEY_H
