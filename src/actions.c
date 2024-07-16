// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024 Bardia Moshiri <fakeshell@bardia.tech>

#include <time.h>
#include <glib.h>
#include <stdio.h>
#include <gio/gio.h>
#include <gst/gst.h>
#include <batman/wlrdisplay.h>
#include "actions.h"
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

void create_gst_pipeline() {
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
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);
    g_free(filename);
}
