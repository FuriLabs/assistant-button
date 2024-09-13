// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024 Bardia Moshiri <fakeshell@bardia.tech>

#include <gio/gio.h>
#include <gst/gst.h>
#include <batman/wlrdisplay.h>
#include "actions.h"
#include "virtkey.h"
#include "utils.h"

static GMainLoop *loop;

void handle_flashlight() {
    GDBusConnection *connection;
    GError *error = NULL;
    GVariant *result;
    gint32 brightness = 0;
    int screen_status;

    connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (connection == NULL) {
        g_printerr("Failed to get session bus: %s\n", error->message);
        g_error_free(error);
        return;
    }

    result = g_dbus_connection_call_sync(
        connection,
        "org.droidian.Flashlightd",
        "/org/droidian/Flashlightd",
        "org.freedesktop.DBus.Properties",
        "Get",
        g_variant_new("(ss)", "org.droidian.Flashlightd", "Brightness"),
        G_VARIANT_TYPE("(v)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );

    if (result == NULL) {
        g_printerr("Failed to get property: %s\n", error->message);
        g_error_free(error);
        g_object_unref(connection);
        return;
    }

    GVariant *brightness_variant;
    g_variant_get(result, "(v)", &brightness_variant);
    g_variant_get(brightness_variant, "i", &brightness);
    g_variant_unref(brightness_variant);
    g_variant_unref(result);

    screen_status = wlrdisplay(0, NULL);

    gint32 new_brightness;
    if (screen_status == 0) // Screen is on
        new_brightness = (brightness > 0) ? 0 : 100;
    else // Screen is off, don't allow turning on at all
        new_brightness = 0;

    result = g_dbus_connection_call_sync(
        connection,
        "org.droidian.Flashlightd",
        "/org/droidian/Flashlightd",
        "org.droidian.Flashlightd",
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
    } else
        g_variant_unref(result);

    g_object_unref(connection);
}

void open_camera() {
    run_command("furios-camera");
}

static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data) {
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

void take_picture() {
    GstElement *pipeline, *source, *convert, *flip, *enc, *sink;
    GstBus *bus;
    GstStateChangeReturn ret;
    GMainLoop *loop;
    gchar *filename;
    time_t now;
    struct tm *t;
    gchar datetime[32];

    const char *home_dir = getenv("HOME");
    if (home_dir == NULL)
        return;

    gchar *pictures_dir = g_strdup_printf("%s/Pictures", home_dir);

    if (g_mkdir_with_parents(pictures_dir, 0755) == -1) {
        g_printerr("Failed to create directory %s\n", pictures_dir);
        g_free(pictures_dir);
        return;
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
        return;
    }

    now = time(NULL);
    t = localtime(&now);
    strftime(datetime, sizeof(datetime), "photo_%Y%m%d_%H%M%S", t);
    filename = g_strdup_printf("%s/%s.jpeg", pictures_dir, datetime);

    g_object_set(source, "camera_device", 0, "mode", 2, NULL);
    g_object_set(sink, "location", filename, NULL);
    g_object_set(flip, "video-direction", 8, NULL); // 8 corresponds to GST_VIDEO_FLIP_METHOD_AUTO
    g_object_set(enc, "snapshot", TRUE, NULL); // exit out after the first frame

    gst_bin_add_many(GST_BIN(pipeline), source, convert, flip, enc, sink, NULL);
    if (!gst_element_link_many(source, convert, flip, enc, sink, NULL)) {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return;
    }

    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(pipeline);
        return;
    }

    loop = g_main_loop_new(NULL, FALSE);

    bus = gst_element_get_bus(pipeline);
    gst_bus_add_watch(bus, bus_call, loop);
    gst_object_unref(bus);

    g_main_loop_run(loop);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    g_print("Picture saved to: %s\n", filename);
    show_notification("Picture saved to", filename);

    gst_object_unref(pipeline);
    g_main_loop_unref(loop);
    g_free(filename);
}

void take_screenshot() {
    GDBusConnection *connection;
    GError *error = NULL;
    GVariant *result;
    gboolean success;
    gchar *filename_used;
    gchar datetime[64];
    time_t now;
    struct tm *t;

    const char *home_dir = getenv("HOME");
    if (home_dir == NULL)
        return;

    gchar *pictures_dir = g_strdup_printf("%s/Pictures", home_dir);
    gchar *screenshots_dir = g_strdup_printf("%s/Screenshots", pictures_dir);

    if (g_mkdir_with_parents(screenshots_dir, 0755) == -1) {
        g_printerr("Failed to create directory %s\n", screenshots_dir);
        g_free(screenshots_dir);
        g_free(pictures_dir);
        return;
    }

    now = time(NULL);
    t = localtime(&now);
    strftime(datetime, sizeof(datetime), "Screenshot from %Y-%m-%d %H-%M-%S.png", t);
    gchar *screenshot_path = g_strdup_printf("%s/%s", screenshots_dir, datetime);

    connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (connection == NULL) {
        g_printerr("Failed to get session bus: %s\n", error->message);
        g_error_free(error);
        g_free(screenshot_path);
        g_free(screenshots_dir);
        g_free(pictures_dir);
        return;
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
        g_object_unref(connection);
        g_free(screenshot_path);
        g_free(screenshots_dir);
        g_free(pictures_dir);
        return;
    }

    g_variant_get(result, "(bs)", &success, &filename_used);

    if (success) {
        g_print("Screenshot saved to: %s\n", filename_used);
        show_notification("Screenshot saved to", filename_used);
    } else
        g_print("Failed to take screenshot.\n");

    g_free(filename_used);
    g_variant_unref(result);
    g_object_unref(connection);
    g_free(screenshot_path);
    g_free(screenshots_dir);
    g_free(pictures_dir);
}

void send_key(const char *name) {
    struct wtype wtype;
    memset(&wtype, 0, sizeof(wtype));

    wtype.commands = calloc(1, sizeof(wtype.commands[0]));
    wtype.command_count = 1;

    struct wtype_command *cmd = &wtype.commands[0];
    cmd->type = WTYPE_COMMAND_TEXT;
    xkb_keysym_t ks = xkb_keysym_from_name(name, XKB_KEYSYM_CASE_INSENSITIVE);
    if (ks == XKB_KEY_NoSymbol) {
        g_print("Unknown key '%s'", name);
        return;
    }
    cmd->key_codes = malloc(sizeof(cmd->key_codes[0]));
    cmd->key_codes_len = 1;
    cmd->key_codes[0] = get_key_code_by_xkb(&wtype, ks);
    cmd->delay_ms = 0;

    wtype.display = wl_display_connect(NULL);
    if (wtype.display == NULL) {
        g_print("Wayland connection failed\n");
        return;
    }
    wtype.registry = wl_display_get_registry(wtype.display);
    wl_registry_add_listener(wtype.registry, &registry_listener, &wtype);
    wl_display_dispatch(wtype.display);
    wl_display_roundtrip(wtype.display);

    if (wtype.manager == NULL) {
        g_print("Compositor does not support the virtual keyboard protocol\n");
        return;
    }
    if (wtype.seat == NULL) {
        g_print("No seat found\n");
        return;
    }

    wtype.keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(
        wtype.manager, wtype.seat
    );

    upload_keymap(&wtype);
    run_commands(&wtype);

    g_print("%s key sent to seat\n", name);

    free(wtype.commands);
    free(wtype.keymap);
    zwp_virtual_keyboard_v1_destroy(wtype.keyboard);
    zwp_virtual_keyboard_manager_v1_destroy(wtype.manager);
    wl_registry_destroy(wtype.registry);
    wl_display_disconnect(wtype.display);
}

void manual_autorotate() {
    GSettings *settings;
    GSettingsSchema *schema;
    GSettingsSchemaSource *schema_source;
    gboolean current_value;

    schema_source = g_settings_schema_source_get_default();
    schema = g_settings_schema_source_lookup(schema_source, "org.gnome.settings-daemon.peripherals.touchscreen", TRUE);
    if (schema == NULL) {
        g_print("Schema 'org.gnome.settings-daemon.peripherals.touchscreen' not found\n");
        return;
    }

    if (!g_settings_schema_has_key(schema, "orientation-lock")) {
        g_print("Key 'orientation-lock' not found in the schema\n");
        g_settings_schema_unref(schema);
        return;
    }

    settings = g_settings_new("org.gnome.settings-daemon.peripherals.touchscreen");

    current_value = g_settings_get_boolean(settings, "orientation-lock");
    if (!current_value) {
        g_print("Orientation lock is already enabled. No action taken.\n");
        g_object_unref(settings);
        g_settings_schema_unref(schema);
        return;
    }

    g_settings_set_boolean(settings, "orientation-lock", FALSE);
    usleep(2000000); // two second should be enough for phosh to rotate
    g_settings_set_boolean(settings, "orientation-lock", TRUE);

    g_object_unref(settings);
    g_settings_schema_unref(schema);
}
