/**
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2025 Bardia Moshiri <bardia@furilabs.com>
 */

#include <poll.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/input.h>
#include <gio/gio.h>

#include "actions.h"
#include "utils.h"
#include "dbus.h"
#include "config.h"

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
};

static volatile sig_atomic_t should_exit = 0;
static struct state *global_state = NULL;

static void
signal_handler(int sig)
{
    (void)sig;
    should_exit = 1;
}

static void
cleanup_state(struct state *state)
{
    if (!state)
        return;

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

void
handle_predefined_action(enum PredefinedAction action)
{
    switch (action) {
        case FLASHLIGHT:
            handle_flashlight();
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
        handle_predefined_action((enum PredefinedAction)action_index);
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
        handle_predefined_action((enum PredefinedAction)action_index);
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
        handle_predefined_action((enum PredefinedAction)action_index);
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

int
handle_events(struct state *state)
{
    while (1) {
        int ret = poll(&state->pfd, 1, 0);
        if (ret > 0) {
            if (read(state->fd, &state->ev, sizeof(struct input_event)) == -1) {
                perror("Failed to read the event");
                return -1;
            }

            if (state->ev.type == EV_KEY && state->ev.code == state->config.assistant_key) {
                if (state->ev.value == 1) {
                    refresh_action_cache(state);
                    state->press_time = current_time_ms();
                    state->press_count++;
                    state->has_long_press_occurred = 0;
                } else if (state->ev.value == 0) {
                    if (!state->has_long_press_occurred) {
                        long duration = (long)(current_time_ms() - state->press_time);

                        if (duration < state->config.short_press_max) {
                            /* Short press: if we don't have a double press action, execute the short press action immediately */
                            if (!state->cached_has_double_action) {
                                short_press(state);
                                reset_state(state);
                            } else {
                                state->short_press_count++;
                                if (state->short_press_count > 1) {
                                    double_press(state);
                                    reset_state(state);
                                }
                            }
                        }
                    }
                }
            }
        } else if (ret == 0) {
            return 0; /* No more events */
        } else {
            if (errno != EINTR) {
                perror("Poll failed");
                return -1;
            }
        }
    }
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
        .cached_has_double_action = 0
    };

    global_state = &state;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

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

    state.pfd.fd = state.fd;
    state.pfd.events = POLLIN;

    state.dbus_conn = dbus_init(&state.dbus_owner_id);
    if (state.dbus_conn == NULL) {
        cleanup_state(&state);
        return EXIT_FAILURE;
    }

    while (!should_exit) {
        int timeout = calculate_timeout(&state);
        int ret = poll(&state.pfd, 1, timeout);

        if (ret > 0) {
            if (handle_events(&state) != 0) {
                cleanup_state(&state);
                return EXIT_FAILURE;
            }
        } else if (ret == 0) {
            /* Timeout occurred, process any pending double/long press actions */
            long long now = current_time_ms();
            long duration = (long)(now - state.press_time);

            if (state.cached_has_double_action &&
                state.short_press_count == 1 &&
                duration >= state.config.double_press_max) {
                short_press(&state);
                reset_state(&state);
            } else if (state.cached_has_long_action &&
                       duration >= state.config.short_press_max &&
                       !state.has_long_press_occurred &&
                       state.press_count > 0) {
                long_press(&state);
                state.has_long_press_occurred = 1;
                reset_state(&state);
            }
        } else {
            if (errno != EINTR) {
                perror("Poll failed");
                cleanup_state(&state);
                return EXIT_FAILURE;
            }
        }
    }

    g_debug("Shutting down gracefully...");
    cleanup_state(&state);
    return EXIT_SUCCESS;
}
