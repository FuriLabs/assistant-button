// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024 Bardia Moshiri <fakeshell@bardia.tech>

#include <time.h>
#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <linux/input.h>
#include "actions.h"
#include "utils.h"

#define DEFAULT_SHORT_PRESS_MAX 500  // ms
#define DEFAULT_DEVICE "/dev/input/event1"
#define CONFIG_FILE "/etc/assistant-button.conf"
#define DEFAULT_DOUBLE_PRESS_MAX 200  // ms
#define ASSISTANT_KEY 112

enum PredefinedAction {
    NO_ACTION = 0,
    FLASHLIGHT = 1,
    OPEN_CAMERA = 2,
    TAKE_PICTURE = 3,
    TAKE_SCREENSHOT = 4,
    SEND_TAB = 5,
    MANUAL_AUTOROTATE = 6
};

struct state {
    int fd;
    struct input_event ev;
    struct pollfd pfd;
    long long press_time;
    int press_count;
    int has_long_press_occurred;
    int short_press_max;
    int double_press_max;
    char device[256];
    int short_press_count;
    long first_press_duration;
};

long long current_time_ms() {
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);
    return spec.tv_sec * 1000LL + spec.tv_nsec / 1e6;
}

void read_config(struct state *state) {
    FILE *file = fopen(CONFIG_FILE, "r");
    if (file == NULL) {
        perror("Failed to open the config file");
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "SHORT_PRESS_MAX=%d", &state->short_press_max) == 1)
            continue;
        if (sscanf(line, "DOUBLE_PRESS_MAX=%d", &state->double_press_max) == 1)
            continue;
        if (sscanf(line, "DEVICE=%s", state->device) == 1)
            continue;
    }
    fclose(file);
}

void handle_predefined_action(enum PredefinedAction action) {
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
        default:
            fprintf(stderr, "Unknown predefined action: %d\n", action);
    }
}

int read_config_int(const char *filename) {
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Error: HOME environment variable not set\n");
        return -1;
    }

    char file_path[PATH_MAX];
    snprintf(file_path, sizeof(file_path), "%s/.config/assistant-button/%s", home_dir, filename);

    FILE *file = fopen(file_path, "r");
    if (file == NULL)
        return -1;

    char buffer[32];
    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        fclose(file);
        return -1;
    }

    fclose(file);

    char *endptr;
    long value = strtol(buffer, &endptr, 10);

    if (endptr == buffer || *endptr != '\n') {
        fprintf(stderr, "Error: Invalid integer in file %s\n", file_path);
        return -1;
    }

    if (value < INT_MIN || value > INT_MAX) {
        fprintf(stderr, "Error: Integer out of range in file %s\n", file_path);
        return -1;
    }

    return (int)value;
}

char* parse_custom_action(const char *filename) {
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL)
        return NULL;

    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/.config/assistant-button/%s", home_dir, filename);

    struct stat st;
    if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) {
        int fd = open(file_path, O_RDONLY);
        if (fd != -1) {
            static char buffer[256];
            ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
            close(fd);

            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                if (strlen(buffer) > 0)
                    return buffer;
            }
        }
    }
    return NULL;
}

int has_short_press_action() {
    return parse_custom_action("short_press") != NULL || read_config_int("short_press_predefined") > 0;
}

int has_long_press_action() {
    return parse_custom_action("long_press") != NULL || read_config_int("long_press_predefined") > 0;
}

int has_double_press_action() {
    return parse_custom_action("double_press") != NULL || read_config_int("double_press_predefined") > 0;
}

int short_press() {
    char *command = parse_custom_action("short_press");
    if (command) {
        run_command(command);
        return 1;
    }

    int action_index = read_config_int("short_press_predefined");
    if (action_index > 0 && action_index <= MANUAL_AUTOROTATE) {
        handle_predefined_action((enum PredefinedAction)action_index);
        return 1;
    }

    return 0;
}

int long_press() {
    char *command = parse_custom_action("long_press");
    if (command) {
        run_command(command);
        return 1;
    }

    int action_index = read_config_int("long_press_predefined");
    if (action_index > 0 && action_index <= MANUAL_AUTOROTATE) {
        handle_predefined_action((enum PredefinedAction)action_index);
        return 1;
    }

    return 0;
}

int double_press() {
    char *command = parse_custom_action("double_press");
    if (command) {
        run_command(command);
        return 1;
    }

    int action_index = read_config_int("double_press_predefined");
    if (action_index > 0 && action_index <= MANUAL_AUTOROTATE) {
        handle_predefined_action((enum PredefinedAction)action_index);
        return 1;
    }

    return 0;
}

#define MAX(a, b) ((a) > (b) ? (a) : (b))

int calculate_timeout(struct state *state) {
    if (state->press_count == 0) {
        return -1;
    }

    long long current_time = current_time_ms();
    long time_since_press = current_time - state->press_time;

    if (has_long_press_action() && !state->has_long_press_occurred) {
        return MAX(0, state->short_press_max - time_since_press);
    }

    if (has_double_press_action() && state->short_press_count == 1) {
        return MAX(0, state->double_press_max - time_since_press);
    }

    return 0;
}

void reset_state(struct state *state) {
    state->short_press_count = 0;
    state->press_count = 0;
    state->has_long_press_occurred = 0;
}

int handle_events(struct state *state) {
    while (1) {
        int ret = poll(&state->pfd, 1, 0);
        if (ret > 0) {
            if (read(state->fd, &state->ev, sizeof(struct input_event)) == -1) {
                perror("Failed to read the event");
                return -1;
            }

            if (state->ev.type == EV_KEY && state->ev.code == ASSISTANT_KEY) {
                if (state->ev.value == 1) {
                    state->press_time = current_time_ms();
                    state->press_count++;
                    state->has_long_press_occurred = 0;
                } else if (state->ev.value == 0) {
                    if (!state->has_long_press_occurred) {
                        long duration = current_time_ms() - state->press_time;
                        if (duration < state->short_press_max) {
                            // Short press: if we don't have a double press action, execute the short press action immediately
                            if (!has_double_press_action()) {
                                short_press();
                                reset_state(state);
                            } else {
                                state->short_press_count++;
                                if (state->short_press_count > 1) {
                                    double_press();
                                    reset_state(state);
                                }
                            }
                        }
                    }
                }
            }
        } else if (ret == 0) {
            return 0; // No more events
        } else {
            if (errno != EINTR) {
                perror("Poll failed");
                return -1;
            }
        }
    }
}

void wait_for_next_event(struct state *state) {
    int timeout = -1;
    if ((has_double_press_action() || has_long_press_action()) && state->press_count > 0) {
        timeout = state->short_press_max;
    }
    poll(&state->pfd, 1, timeout);
}

int main(int argc, char *argv[]) {
    struct state state = {
        .fd = -1,
        .press_time = 0,
        .press_count = 0,
        .has_long_press_occurred = 0,
        .short_press_max = DEFAULT_SHORT_PRESS_MAX,
        .double_press_max = DEFAULT_DOUBLE_PRESS_MAX,
        .short_press_count = 0,
        .first_press_duration = 0
    };

    strcpy(state.device, DEFAULT_DEVICE);

    read_config(&state);

    if (argc > 1)
        state.short_press_max = atoi(argv[1]);

    if (argc > 2)
        state.double_press_max = atoi(argv[2]);

    if (argc > 3) {
        strncpy(state.device, argv[3], sizeof(state.device) - 1);
        state.device[sizeof(state.device) - 1] = '\0';
    }

    state.fd = open(state.device, O_RDONLY | O_NONBLOCK);
    if (state.fd == -1) {
        perror("Failed to open the device");
        return EXIT_FAILURE;
    }

    state.pfd.fd = state.fd;
    state.pfd.events = POLLIN;

    while (1) {
        int timeout = calculate_timeout(&state);
        int ret = poll(&state.pfd, 1, timeout);

        if (ret > 0) {
            if (handle_events(&state) != 0) {
                close(state.fd);
                return EXIT_FAILURE;
            }
        } else if (ret == 0) {
            // Timeout occurred, process any pending double/long press actions
            long long current_time = current_time_ms();
            long duration = current_time - state.press_time;

            if (state.short_press_count == 1 && duration >= state.double_press_max) {
                short_press();
                reset_state(&state);
            } else if (duration >= state.short_press_max && !state.has_long_press_occurred && state.press_count > 0) {
                long_press();
                reset_state(&state);
                state.has_long_press_occurred = 1;
            }
        } else {
            if (errno != EINTR) {
                perror("Poll failed");
                close(state.fd);
                return EXIT_FAILURE;
            }
        }
    }

    close(state.fd);
    return 0;
}
