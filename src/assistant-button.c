/**
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2025 Bardia Moshiri <bardia@furilabs.com>
 */

#include <poll.h>
#include <linux/input.h>
#include <gio/gio.h>
#include <glib-unix.h>

#include "actions.h"
#include "utils.h"
#include "dbus.h"
#include "config.h"
#include "logind.h"

enum PredefinedAction {
    NO_ACTION = 0,
    FLASHLIGHT = 1,
    OPEN_CAMERA = 2,
    TAKE_PICTURE = 3,
    TAKE_SCREENSHOT = 4,
    SEND_TAB = 5,
    MANUAL_AUTOROTATE = 6,
    SEND_XF86BACK = 7,
    SEND_ESCAPE = 8,
    ACTION_COUNT
};

struct state {
    int fd;
    struct input_event ev;
    struct pollfd pfd;
    long long press_time;
    int press_count;
    int has_long_press_occurred;
    struct config config;
    int short_press_count;
    long first_press_duration;
    GDBusConnection *dbus_conn;
    guint dbus_owner_id;

    int cached_has_long_action;
    int cached_has_double_action;

    LogindMonitor *logind;
    LogindScreenState screen_state;

    GMainLoop *loop;
    guint timer_id;
};

static struct state *global_state = NULL;

static gboolean
on_unix_signal_quit(gpointer user_data)
{
    struct state *s = (struct state *)user_data;
    if (s && s->loop)
        g_main_loop_quit(s->loop);
    return G_SOURCE_CONTINUE;
}

static void
on_logind_screen_changed(LogindScreenState state, void *user_data)
{
    struct state *s = (struct state *)user_data;
    if (!s)
        return;

    s->screen_state = state;

    if (state == LOGIND_SCREEN_ON)
        g_debug("logind: screen state -> ON");
    else if (state == LOGIND_SCREEN_OFF)
        g_debug("logind: screen state -> OFF");
    else
        g_debug("logind: screen state -> UNKNOWN");
}

static void
cleanup_state(struct state *state)
{
    if (!state)
        return;

    if (state->timer_id) {
        g_source_remove(state->timer_id);
        state->timer_id = 0;
    }

    if (state->logind) {
        logind_monitor_free(state->logind);
        state->logind = NULL;
        state->screen_state = LOGIND_SCREEN_UNKNOWN;
    }

    if (state->fd != -1) {
        close(state->fd);
        state->fd = -1;
    }

    if (state->dbus_conn) {
        /* Flush any pending D-Bus operations */
        g_dbus_connection_flush_sync(state->dbus_conn, NULL, NULL);

        /* Release the D-Bus name */
        if (state->dbus_owner_id > 0) {
            g_bus_unown_name(state->dbus_owner_id);
            state->dbus_owner_id = 0;
        }

        dbus_cleanup(state->dbus_conn);
        state->dbus_conn = NULL;
    }

    config_free(&state->config);

    if (state->loop) {
        g_main_loop_unref(state->loop);
        state->loop = NULL;
    }
}

long long
current_time_ms(void)
{
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);
    return spec.tv_sec * 1000LL + (long long)(spec.tv_nsec / 1000000LL);
}

void
read_config(struct state *state)
{
    config_load(&state->config);
}

static gboolean
is_screen_on_from_state(const struct state *state)
{
    if (!state)
        return FALSE;

    return (state->screen_state == LOGIND_SCREEN_ON) ? TRUE : FALSE;
}

static void
handle_predefined_action(struct state *state, enum PredefinedAction action)
{
    switch (action) {
        case FLASHLIGHT:
            handle_flashlight(is_screen_on_from_state(state));
            break;
        case OPEN_CAMERA:
            open_camera();
            break;
        case TAKE_PICTURE:
            take_picture();
            break;
        case TAKE_SCREENSHOT:
            take_screenshot();
            break;
        case SEND_TAB:
            send_key("Tab");
            break;
        case MANUAL_AUTOROTATE:
            manual_autorotate();
            break;
        case SEND_XF86BACK:
            send_key("XF86Back");
            break;
        case SEND_ESCAPE:
            send_key("Escape");
            break;
        default:
            fprintf(stderr, "Unknown predefined action: %d\n", action);
    }
}

int
read_config_int(const char *filename)
{
    return config_read_user_int(filename);
}

char *
parse_custom_action(const char *filename)
{
    return config_read_user_action(filename);
}

static void
refresh_action_cache(struct state *state)
{
    char *long_cmd = parse_custom_action("long_press");
    state->cached_has_long_action = (long_cmd != NULL) ||
                                    (read_config_int("long_press_predefined") > 0);
    g_free(long_cmd);

    char *double_cmd = parse_custom_action("double_press");
    state->cached_has_double_action = (double_cmd != NULL) ||
                                      (read_config_int("double_press_predefined") > 0);
    g_free(double_cmd);
}

int
short_press(struct state *state)
{
    char *command = parse_custom_action("short_press");
    if (command) {
        run_command(command);
        dbus_emit_signal(state->dbus_conn, ACTION_COUNT, SHORT_PRESS);
        g_free(command);
        return 1;
    }

    int action_index = read_config_int("short_press_predefined");
    if (action_index > 0 && action_index < ACTION_COUNT) {
        handle_predefined_action(state, (enum PredefinedAction)action_index);
        dbus_emit_signal(state->dbus_conn, action_index, SHORT_PRESS);
        return 1;
    }

    return 0;
}

int
long_press(struct state *state)
{
    char *command = parse_custom_action("long_press");
    if (command) {
        run_command(command);
        dbus_emit_signal(state->dbus_conn, ACTION_COUNT, LONG_PRESS);
        g_free(command);
        return 1;
    }

    int action_index = read_config_int("long_press_predefined");
    if (action_index > 0 && action_index < ACTION_COUNT) {
        handle_predefined_action(state, (enum PredefinedAction)action_index);
        dbus_emit_signal(state->dbus_conn, action_index, LONG_PRESS);
        return 1;
    }

    return 0;
}

int
double_press(struct state *state)
{
    char *command = parse_custom_action("double_press");
    if (command) {
        run_command(command);
        dbus_emit_signal(state->dbus_conn, ACTION_COUNT, DOUBLE_PRESS);
        g_free(command);
        return 1;
    }

    int action_index = read_config_int("double_press_predefined");
    if (action_index > 0 && action_index < ACTION_COUNT) {
        handle_predefined_action(state, (enum PredefinedAction)action_index);
        dbus_emit_signal(state->dbus_conn, action_index, DOUBLE_PRESS);
        return 1;
    }

    return 0;
}

int
calculate_timeout(struct state *state)
{
    if (state->press_count == 0)
        return -1;

    long long now = current_time_ms();
    long elapsed = (long)(now - state->press_time);

    if (state->cached_has_long_action && !state->has_long_press_occurred)
        return MAX(0, state->config.short_press_max - elapsed);
    if (state->cached_has_double_action && state->short_press_count == 1)
        return MAX(0, state->config.double_press_max - elapsed);

    return -1;
}

void
reset_state(struct state *state)
{
    state->short_press_count = 0;
    state->press_count = 0;
    state->has_long_press_occurred = 0;
    state->cached_has_long_action = 0;
    state->cached_has_double_action = 0;
}

static gboolean on_timeout_cb(gpointer user_data);

static void
reschedule_timeout(struct state *state)
{
    if (!state)
        return;

    if (state->timer_id) {
        g_source_remove(state->timer_id);
        state->timer_id = 0;
    }

    int timeout = calculate_timeout(state);
    if (timeout >= 0)
        state->timer_id = g_timeout_add((guint)timeout, on_timeout_cb, state);
}

static int
handle_events(struct state *state)
{
    while (1) {
        ssize_t r = read(state->fd, &state->ev, sizeof(struct input_event));
        if (r == (ssize_t)sizeof(struct input_event)) {
            if (state->ev.type == EV_KEY && state->ev.code == state->config.assistant_key) {
                if (state->ev.value == 1) {
                    refresh_action_cache(state);
                    state->press_time = current_time_ms();
                    state->press_count++;
                    state->has_long_press_occurred = 0;
                    reschedule_timeout(state);
                } else if (state->ev.value == 0) {
                    if (!state->has_long_press_occurred) {
                        long duration = (long)(current_time_ms() - state->press_time);

                        if (duration < state->config.short_press_max) {
                            /* Short press: if we don't have a double press action, execute the short press action immediately */
                            if (!state->cached_has_double_action) {
                                short_press(state);
                                reset_state(state);
                                reschedule_timeout(state);
                            } else {
                                state->short_press_count++;
                                if (state->short_press_count > 1) {
                                    double_press(state);
                                    reset_state(state);
                                    reschedule_timeout(state);
                                } else {
                                    reschedule_timeout(state);
                                }
                            }
                        } else {
                            reschedule_timeout(state);
                        }
                    }
                }
            }
            continue;
        }

        if (r == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return 0; /* no more events */
        if (r == -1 && errno == EINTR)
            continue;
        if (r == 0)
            return -1; /* device closed? */

        perror("Failed to read the event");
        return -1;
    }
}

static gboolean
on_input_fd_ready(gint fd, GIOCondition condition, gpointer user_data)
{
    (void)fd;
    struct state *state = (struct state *)user_data;

    if (!state)
        return G_SOURCE_CONTINUE;

    if (condition & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) {
        g_warning("Input fd error/hup");
        if (state->loop)
            g_main_loop_quit(state->loop);
        return G_SOURCE_REMOVE;
    }

    if (condition & G_IO_IN) {
        if (handle_events(state) != 0) {
            g_warning("Failed handling input events");
            if (state->loop)
                g_main_loop_quit(state->loop);
            return G_SOURCE_REMOVE;
        }
    }

    return G_SOURCE_CONTINUE;
}

static gboolean
on_timeout_cb(gpointer user_data)
{
    struct state *state = (struct state *)user_data;
    if (!state)
        return G_SOURCE_REMOVE;

    state->timer_id = 0;

    /* Timeout occurred, process any pending double/long press actions */
    long long now = current_time_ms();
    long duration = (long)(now - state->press_time);

    if (state->cached_has_double_action &&
        state->short_press_count == 1 &&
        duration >= state->config.double_press_max) {
        short_press(state);
        reset_state(state);
    } else if (state->cached_has_long_action &&
               duration >= state->config.short_press_max &&
               !state->has_long_press_occurred &&
               state->press_count > 0) {
        long_press(state);
        state->has_long_press_occurred = 1;
        reset_state(state);
    }

    reschedule_timeout(state);

    return G_SOURCE_REMOVE;
}

int
main(int argc, char *argv[])
{
    struct state state = {
        .fd = -1,
        .press_time = 0,
        .press_count = 0,
        .has_long_press_occurred = 0,
        .config = (struct config){0},
        .short_press_count = 0,
        .first_press_duration = 0,
        .dbus_conn = NULL,
        .dbus_owner_id = 0,
        .cached_has_long_action = 0,
        .cached_has_double_action = 0,
        .logind = NULL,
        .screen_state = LOGIND_SCREEN_UNKNOWN,
        .loop = NULL,
        .timer_id = 0
    };

    global_state = &state;

    read_config(&state);

    if (argc > 1)
        state.config.short_press_max = atoi(argv[1]);

    if (argc > 2)
        state.config.double_press_max = atoi(argv[2]);

    if (argc > 3)
        state.config.assistant_key = atoi(argv[3]);

    if (argc > 4) {
        g_free(state.config.device);
        state.config.device = g_strdup(argv[4]);
    }

    state.fd = open(state.config.device, O_RDONLY | O_NONBLOCK);
    if (state.fd == -1) {
        perror("Failed to open the device");
        cleanup_state(&state);
        return EXIT_FAILURE;
    }

    state.dbus_conn = dbus_init(&state.dbus_owner_id);
    if (state.dbus_conn == NULL) {
        cleanup_state(&state);
        return EXIT_FAILURE;
    }

    state.logind = logind_monitor_new(on_logind_screen_changed, &state);
    if (state.logind) {
        state.screen_state = logind_monitor_get_screen_state(state.logind);
        g_debug("logind: initial screen state = %d", (int)state.screen_state);
    } else {
        g_warning("logind: monitor init failed");
        state.screen_state = LOGIND_SCREEN_UNKNOWN;
    }

    state.loop = g_main_loop_new(NULL, FALSE);

    g_unix_signal_add(SIGINT, on_unix_signal_quit, &state);
    g_unix_signal_add(SIGTERM, on_unix_signal_quit, &state);

    g_unix_fd_add(state.fd,
                  (GIOCondition)(G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL),
                  on_input_fd_ready,
                  &state);

    reschedule_timeout(&state);

    g_main_loop_run(state.loop);

    g_debug("Shutting down gracefully...");
    cleanup_state(&state);
    return EXIT_SUCCESS;
}
