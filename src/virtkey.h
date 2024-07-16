// SPDX-License-Identifier: MIT
// Copyright (c) 2019 Josef Gajdusek
// Copyright (C) 2023 Bardia Moshiri <fakeshell@bardia.tech>

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
void upload_keymap(struct wtype *wtype);
void handle_wl_event(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
void handle_wl_event_remove(void *data, struct wl_registry *registry, uint32_t name);
enum wtype_mod name_to_mod(const char *name);
unsigned int append_keymap_entry(struct wtype *wtype, wchar_t ch, xkb_keysym_t xkb);
unsigned int get_key_code_by_wchar(struct wtype *wtype, wchar_t ch);
unsigned int get_key_code_by_xkb(struct wtype *wtype, xkb_keysym_t xkb);
void run_mod(struct wtype *wtype, struct wtype_command *cmd);
void run_key(struct wtype *wtype, struct wtype_command *cmd);
void type_keycode(struct wtype *wtype, unsigned int key_code);
void run_text(struct wtype *wtype, struct wtype_command *cmd);
void run_commands(struct wtype *wtype);
void print_keysym_name(xkb_keysym_t keysym, FILE *f);
void upload_keymap(struct wtype *wtype);

#endif // VIRTKEY_H
