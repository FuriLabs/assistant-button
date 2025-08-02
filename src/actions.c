/**
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2025 Bardia Moshiri <bardia@furilabs.com>
 */

#include <gio/gio.h>
#include <gst/gst.h>
#include <batman/wlrdisplay.h>
#include "actions.h"
#include "virtkey.h"
#include "utils.h"

static GMainLoop *loop;

void
handle_flashlight(void)
{
    GDBusConnection *connection = NULL;
    GError *error = NULL;
    GVariant *result = NULL;
    GVariant *brightness_variant = NULL;
    guint32 brightness = 0;
    int screen_status;

    connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (connection == NULL) {
        g_printerr("Failed to get session bus: %s\n", error->message);
        g_error_free(error);
        return;
    }

    result = g_dbus_connection_call_sync(
        connection,
        "io.furios.Flashlightd",
        "/io/furios/Flashlightd",
        "org.freedesktop.DBus.Properties",
        "Get",
        g_variant_new("(ss)", "io.furios.Flashlightd", "Brightness"),
        G_VARIANT_TYPE("(v)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );

    if (result == NULL) {
        g_printerr("Failed to get property: %s\n", error->message);
        g_error_free(error);
        goto cleanup;
    }

    g_variant_get(result, "(v)", &brightness_variant);
    g_variant_get(brightness_variant, "u", &brightness);
    g_variant_unref(brightness_variant);
    g_variant_unref(result);
    result = NULL;

    screen_status = get_wlroots_screen_status();

    gint32 new_brightness;
    if (screen_status == 0) /* Screen is on */
        new_brightness = (brightness > 0) ? 0 : 100;
    else /* Screen is off, don't allow turning on at all */
        new_brightness = 0;

    result = g_dbus_connection_call_sync(
        connection,
        "io.furios.Flashlightd",
        "/io/furios/Flashlightd",
        "io.furios.Flashlightd",
        "SetBrightness",
        g_variant_new("(u)", new_brightness),
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );

    if (result == NULL) {
        g_printerr("Failed to set brightness: %s\n", error->message);
        g_error_free(error);
    } else {
        g_variant_unref(result);
    }

cleanup:
    if (connection)
        g_object_unref(connection);
}

void
open_camera(void)
{
    run_command("furios-camera");
}

static gboolean
bus_call(GstBus *bus, GstMessage *msg, gpointer data)
{
    GMainLoop *loop = (GMainLoop *)data;

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            g_print("End-Of-Stream reached.\n");
            g_main_loop_quit(loop);
            break;
        case GST_MESSAGE_ERROR: {
            gchar *debug;
            GError *error;

            gst_message_parse_error(msg, &error, &debug);
            g_printerr("Error: %s\n", error->message);
            g_error_free(error);
            g_free(debug);

            g_main_loop_quit(loop);
            break;
        }
        default:
            break;
    }

    return TRUE;
}

void
take_picture(void)
{
    GstElement *pipeline = NULL, *source = NULL, *convert = NULL;
    GstElement *flip = NULL, *enc = NULL, *sink = NULL;
    GstBus *bus = NULL;
    GstStateChangeReturn ret;
    GMainLoop *loop = NULL;
    gchar *filename = NULL;
    gchar *pictures_dir = NULL;
    time_t now;
    struct tm *t;
    gchar datetime[32];

    const char *home_dir = getenv("HOME");
    if (home_dir == NULL)
        return;

    pictures_dir = g_strdup_printf("%s/Pictures", home_dir);

    if (g_mkdir_with_parents(pictures_dir, 0755) == -1) {
        g_printerr("Failed to create directory %s\n", pictures_dir);
        goto cleanup;
    }

    gst_init(NULL, NULL);

    pipeline = gst_pipeline_new("camera-pipeline");
    source = gst_element_factory_make("droidcamsrc", "source");
    convert = gst_element_factory_make("videoconvert", "convert");
    flip = gst_element_factory_make("videoflip", "flip");
    enc = gst_element_factory_make("jpegenc", "encoder");
    sink = gst_element_factory_make("filesink", "sink");

    if (!pipeline || !source || !convert || !flip || !enc || !sink) {
        g_printerr("Not all elements could be created.\n");
        goto cleanup_gst;
    }

    now = time(NULL);
    t = localtime(&now);
    strftime(datetime, sizeof(datetime), "photo_%Y%m%d_%H%M%S", t);
    filename = g_strdup_printf("%s/%s.jpeg", pictures_dir, datetime);

    g_object_set(source, "camera_device", 0, "mode", 2, NULL);
    g_object_set(sink, "location", filename, NULL);
    g_object_set(flip, "video-direction", 8, NULL); /* 8 corresponds to GST_VIDEO_FLIP_METHOD_AUTO */
    g_object_set(enc, "snapshot", TRUE, NULL); /* exit out after the first frame */

    gst_bin_add_many(GST_BIN(pipeline), source, convert, flip, enc, sink, NULL);
    if (!gst_element_link_many(source, convert, flip, enc, sink, NULL)) {
        g_printerr("Elements could not be linked.\n");
        goto cleanup_gst;
    }

    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        goto cleanup_gst;
    }

    loop = g_main_loop_new(NULL, FALSE);

    bus = gst_element_get_bus(pipeline);
    gst_bus_add_watch(bus, bus_call, loop);
    gst_object_unref(bus);
    bus = NULL;

    g_main_loop_run(loop);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    g_print("Picture saved to: %s\n", filename);
    show_notification("Picture saved to", filename);

cleanup_gst:
    if (pipeline)
        gst_object_unref(pipeline);
    if (loop)
        g_main_loop_unref(loop);

cleanup:
    g_free(filename);
    g_free(pictures_dir);
}

void
take_screenshot(void)
{
    GDBusConnection *connection = NULL;
    GError *error = NULL;
    GVariant *result = NULL;
    gboolean success = FALSE;
    gchar *filename_used = NULL;
    gchar *datetime = NULL;
    gchar *screenshot_path = NULL;
    gchar *pictures_dir = NULL;
    gchar *screenshots_dir = NULL;
    time_t now;
    struct tm *t;

    const char *home_dir = getenv("HOME");
    if (home_dir == NULL)
        return;

    pictures_dir = g_strdup_printf("%s/Pictures", home_dir);
    screenshots_dir = g_strdup_printf("%s/Screenshots", pictures_dir);

    if (g_mkdir_with_parents(screenshots_dir, 0755) == -1) {
        g_printerr("Failed to create directory %s\n", screenshots_dir);
        goto cleanup;
    }

    datetime = g_malloc(64);
    now = time(NULL);
    t = localtime(&now);
    strftime(datetime, 64, "Screenshot from %Y-%m-%d %H-%M-%S.png", t);
    screenshot_path = g_strdup_printf("%s/%s", screenshots_dir, datetime);

    connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (connection == NULL) {
        g_printerr("Failed to get session bus: %s\n", error->message);
        g_error_free(error);
        goto cleanup;
    }

    result = g_dbus_connection_call_sync(
        connection,
        "org.gnome.Shell.Screenshot",
        "/org/gnome/Shell/Screenshot",
        "org.gnome.Shell.Screenshot",
        "Screenshot",
        g_variant_new("(bbs)", TRUE, FALSE, screenshot_path),
        G_VARIANT_TYPE("(bs)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );

    if (result == NULL) {
        g_printerr("Failed to take screenshot: %s\n", error->message);
        g_error_free(error);
        goto cleanup;
    }

    g_variant_get(result, "(bs)", &success, &filename_used);

    if (success) {
        g_debug("Screenshot saved to: %s", filename_used);
        show_notification("Screenshot saved to", filename_used);
    } else {
        g_warning("Failed to take screenshot.\n");
    }

    g_variant_unref(result);

cleanup:
    if (connection)
        g_object_unref(connection);
    g_free(filename_used);
    g_free(screenshot_path);
    g_free(screenshots_dir);
    g_free(pictures_dir);
    g_free(datetime);
}

void
send_key(const char *name)
{
    struct wtype wtype;
    struct wtype_command *cmd = NULL;
    xkb_keysym_t ks;

    memset(&wtype, 0, sizeof(wtype));

    wtype.commands = calloc(1, sizeof(wtype.commands[0]));
    if (!wtype.commands) {
        g_warning("Failed to allocate memory for commands");
        return;
    }
    wtype.command_count = 1;

    cmd = &wtype.commands[0];
    cmd->type = WTYPE_COMMAND_TEXT;

    ks = xkb_keysym_from_name(name, XKB_KEYSYM_CASE_INSENSITIVE);
    if (ks == XKB_KEY_NoSymbol) {
        g_warning("Unknown key '%s'", name);
        goto cleanup;
    }

    cmd->key_codes = malloc(sizeof(cmd->key_codes[0]));
    if (!cmd->key_codes) {
        g_warning("Failed to allocate memory for key codes");
        goto cleanup;
    }
    cmd->key_codes_len = 1;
    cmd->key_codes[0] = get_key_code_by_xkb(&wtype, ks);
    cmd->delay_ms = 0;

    wtype.display = wl_display_connect(NULL);
    if (wtype.display == NULL) {
        g_warning("Wayland connection failed\n");
        goto cleanup_key_codes;
    }

    wtype.registry = wl_display_get_registry(wtype.display);
    if (!wtype.registry) {
        g_warning("Failed to get Wayland registry\n");
        goto cleanup_display;
    }

    wl_registry_add_listener(wtype.registry, &registry_listener, &wtype);
    wl_display_dispatch(wtype.display);
    wl_display_roundtrip(wtype.display);

    if (wtype.manager == NULL) {
        g_warning("Compositor does not support the virtual keyboard protocol\n");
        goto cleanup_wayland_objects;
    }
    if (wtype.seat == NULL) {
        g_print("No seat found\n");
        goto cleanup_wayland_objects;
    }

    wtype.keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(
        wtype.manager, wtype.seat
    );
    if (!wtype.keyboard) {
        g_warning("Failed to create virtual keyboard\n");
        goto cleanup_wayland_objects;
    }

    upload_keymap(&wtype);
    run_commands(&wtype);

    g_debug("%s key sent to seat", name);

    /* Cleanup virtual keyboard */
    if (wtype.keyboard) {
        zwp_virtual_keyboard_v1_destroy(wtype.keyboard);
        wtype.keyboard = NULL;
    }

cleanup_wayland_objects:
    /* Clean up bound Wayland objects */
    if (wtype.manager) {
        zwp_virtual_keyboard_manager_v1_destroy(wtype.manager);
        wtype.manager = NULL;
    }
    if (wtype.seat) {
        wl_seat_destroy(wtype.seat);
        wtype.seat = NULL;
    }

    if (wtype.registry) {
        wl_registry_destroy(wtype.registry);
        wtype.registry = NULL;
    }

cleanup_display:
    if (wtype.display) {
        /* Flush and sync before disconnecting */
        wl_display_flush(wtype.display);
        wl_display_roundtrip(wtype.display);
        wl_display_disconnect(wtype.display);
        wtype.display = NULL;
    }

    if (wtype.keymap) {
        free(wtype.keymap);
        wtype.keymap = NULL;
    }

cleanup_key_codes:
    if (cmd && cmd->key_codes) {
        free(cmd->key_codes);
        cmd->key_codes = NULL;
    }

cleanup:
    if (wtype.commands) {
        free(wtype.commands);
        wtype.commands = NULL;
    }
}

void
manual_autorotate(void)
{
    GSettings *settings = NULL;
    GSettingsSchema *schema = NULL;
    GSettingsSchemaSource *schema_source;
    gboolean current_value;

    schema_source = g_settings_schema_source_get_default();
    if (!schema_source) {
        g_warning("Failed to get default schema source\n");
        return;
    }

    schema = g_settings_schema_source_lookup(schema_source, "org.gnome.settings-daemon.peripherals.touchscreen", TRUE);
    if (schema == NULL) {
        g_print("Schema 'org.gnome.settings-daemon.peripherals.touchscreen' not found\n");
        return;
    }

    if (!g_settings_schema_has_key(schema, "orientation-lock")) {
        g_print("Key 'orientation-lock' not found in the schema\n");
        goto cleanup;
    }

    settings = g_settings_new("org.gnome.settings-daemon.peripherals.touchscreen");
    if (!settings) {
        g_warning("Failed to create GSettings object\n");
        goto cleanup;
    }

    current_value = g_settings_get_boolean(settings, "orientation-lock");
    if (!current_value) {
        g_print("Orientation lock is already disabled. No action taken.\n");
        goto cleanup;
    }

    g_settings_set_boolean(settings, "orientation-lock", FALSE);

    g_settings_sync();

    /* two seconds should be enough for phosh to rotate */
    usleep(2000000);

    g_settings_set_boolean(settings, "orientation-lock", TRUE);
    g_settings_sync();

cleanup:
    if (settings)
        g_object_unref(settings);
    if (schema)
        g_settings_schema_unref(schema);
}
