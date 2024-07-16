// SPDX-License-Identifier: MIT
// Copyright (c) 2019 Josef Gajdusek
// Copyright (C) 2023 Bardia Moshiri <fakeshell@bardia.tech>

#include "virtkey.h"

const struct wl_registry_listener registry_listener = {
    .global = handle_wl_event,
    .global_remove = handle_wl_event_remove,
};

void handle_wl_event(void *data, struct wl_registry *registry,
                     uint32_t name, const char *interface,
                     uint32_t version)
{
    struct wtype *wtype = data;
    if (!strcmp(interface, wl_seat_interface.name)) {
        wtype->seat = wl_registry_bind(
            registry, name, &wl_seat_interface, version <= 7 ? version : 7
        );
    } else if (!strcmp(interface, zwp_virtual_keyboard_manager_v1_interface.name)) {
        wtype->manager = wl_registry_bind(
            registry, name, &zwp_virtual_keyboard_manager_v1_interface, 1
        );
    }
}

void handle_wl_event_remove(void *data, struct wl_registry *registry, uint32_t name)
{
}

enum wtype_mod name_to_mod(const char *name)
{
    for (unsigned int i = 0; i < ARRAY_SIZE(mod_names); i++) {
        if (!strcasecmp(mod_names[i].name, name))
            return mod_names[i].mod;
    }
    return WTYPE_MOD_NONE;
}

unsigned int append_keymap_entry(struct wtype *wtype, wchar_t ch, xkb_keysym_t xkb)
{
    wtype->keymap = realloc(
        wtype->keymap, ++wtype->keymap_len * sizeof(wtype->keymap[0])
    );
    wtype->keymap[wtype->keymap_len - 1].wchr = ch;
    wtype->keymap[wtype->keymap_len - 1].xkb = xkb;
    return wtype->keymap_len;
}

unsigned int get_key_code_by_wchar(struct wtype *wtype, wchar_t ch)
{
    const struct {
        wchar_t from;
        xkb_keysym_t to;
    } remap_table[] = {
        { L'\n', XKB_KEY_Return },
        { L'\t', XKB_KEY_Tab },
        { L'\e', XKB_KEY_Escape },
    };
    for (unsigned int i = 0; i < wtype->keymap_len; i++) {
        if (wtype->keymap[i].wchr == ch) {
            return i + 1;
        }
    }

    xkb_keysym_t xkb = xkb_utf32_to_keysym(ch);
    for (size_t i = 0; i < ARRAY_SIZE(remap_table); i++) {
        if (remap_table[i].from == ch) {
            xkb = remap_table[i].to;
            break;
        }
    }

    return append_keymap_entry(wtype, ch, xkb);
}

unsigned int get_key_code_by_xkb(struct wtype *wtype, xkb_keysym_t xkb)
{
    for (unsigned int i = 0; i < wtype->keymap_len; i++) {
        if (wtype->keymap[i].xkb == xkb)
            return i + 1;
    }

    return append_keymap_entry(wtype, 0, xkb);
}

void run_mod(struct wtype *wtype, struct wtype_command *cmd)
{
    if (cmd->type == WTYPE_COMMAND_MOD_PRESS)
        wtype->mod_status |= cmd->mod;
    else
        wtype->mod_status &= ~cmd->mod;

    zwp_virtual_keyboard_v1_modifiers(
        wtype->keyboard, wtype->mod_status & ~WTYPE_MOD_CAPSLOCK, 0,
        wtype->mod_status & WTYPE_MOD_CAPSLOCK, 0
    );

    wl_display_roundtrip(wtype->display);
}

void run_key(struct wtype *wtype, struct wtype_command *cmd)
{
    zwp_virtual_keyboard_v1_key(
        wtype->keyboard, 0, cmd->single_key_code,
        cmd->type == WTYPE_COMMAND_KEY_PRESS ?
        WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED
    );
    wl_display_roundtrip(wtype->display);
}

void type_keycode(struct wtype *wtype, unsigned int key_code)
{
    zwp_virtual_keyboard_v1_key(
        wtype->keyboard, 0, key_code, WL_KEYBOARD_KEY_STATE_PRESSED
    );
    wl_display_roundtrip(wtype->display);
    usleep(2000);
    zwp_virtual_keyboard_v1_key(
        wtype->keyboard, 0, key_code, WL_KEYBOARD_KEY_STATE_RELEASED
    );
    wl_display_roundtrip(wtype->display);
    usleep(2000);
}

void run_text(struct wtype *wtype, struct wtype_command *cmd)
{
    for (size_t i = 0; i < cmd->key_codes_len; i++) {
        type_keycode(wtype, cmd->key_codes[i]);
        usleep(cmd->delay_ms * 1000);
    }
}

void run_commands(struct wtype *wtype)
{
    void (*handlers[])(struct wtype *, struct wtype_command *) = {
        [WTYPE_COMMAND_MOD_PRESS] = run_mod,
        [WTYPE_COMMAND_MOD_RELEASE] = run_mod,
        [WTYPE_COMMAND_KEY_PRESS] = run_key,
        [WTYPE_COMMAND_KEY_RELEASE] = run_key,
        [WTYPE_COMMAND_TEXT] = run_text,
    };
    for (unsigned int i = 0; i < wtype->command_count; i++) {
        handlers[wtype->commands[i].type](wtype, &wtype->commands[i]);
    }
}

void print_keysym_name(xkb_keysym_t keysym, FILE *f)
{
    char sym_name[256];

    int ret = xkb_keysym_get_name(keysym, sym_name, sizeof(sym_name));
    if (ret <= 0) {
        printf("Unable to get XKB symbol name for keysym %04x\n", keysym);
        return;
    }

    fprintf(f, "%s", sym_name);
}

void upload_keymap(struct wtype *wtype)
{
    char filename[] = "/tmp/wtype-XXXXXX";
    int fd = mkstemp(filename);
    if (fd < 0)
        printf("Failed to create the temporary keymap file");

    unlink(filename);
    FILE *f = fdopen(fd, "w");

    fprintf(f, "xkb_keymap {\n");

    fprintf(
        f,
        "xkb_keycodes \"(unnamed)\" {\n"
        "minimum = 8;\n"
        "maximum = %ld;\n",
        wtype->keymap_len + 8 + 1
    );
    for (size_t i = 0; i < wtype->keymap_len; i++) {
        fprintf(f, "<K%ld> = %ld;\n", i + 1, i + 8 + 1);
    }
    fprintf(f, "};\n");

    // TODO: Is including "complete" here really a good idea?
    fprintf(f, "xkb_types \"(unnamed)\" { include \"complete\" };\n");
    fprintf(f, "xkb_compatibility \"(unnamed)\" { include \"complete\" };\n");

    fprintf(f, "xkb_symbols \"(unnamed)\" {\n");
    for (size_t i = 0; i < wtype->keymap_len; i++) {
        fprintf(f, "key <K%ld> {[", i + 1);
        print_keysym_name(wtype->keymap[i].xkb, f);
        fprintf(f, "]};\n");
    }
    fprintf(f, "};\n");

    fprintf(f, "};\n");
    fputc('\0', f);
    fflush(f);
    size_t keymap_size = ftell(f);

    zwp_virtual_keyboard_v1_keymap(
        wtype->keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fileno(f), keymap_size
    );

    wl_display_roundtrip(wtype->display);

    fclose(f);
}
