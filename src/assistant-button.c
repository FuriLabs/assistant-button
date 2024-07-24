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
            send_tab();
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

int double_short_press() {
    short_press();
    short_press();
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
        int ret = poll(&state.pfd, 1, state.short_press_max);
        if (ret > 0) {
            if (read(state.fd, &state.ev, sizeof(struct input_event)) == -1) {
                perror("Failed to read the event");
                close(state.fd);
                return EXIT_FAILURE;
            }
            if (state.ev.type == EV_KEY && state.ev.code == 102) {
                if (state.ev.value == 1) { // Key press
                    state.press_time = current_time_ms();
                    state.press_count++;
                    state.has_long_press_occurred = 0;
                } else if (state.ev.value == 0) { // Key release
                    if (!state.has_long_press_occurred) {
                        long duration = current_time_ms() - state.press_time;
                        if (duration < state.short_press_max) {
                            if (++state.short_press_count == 1) {
                                state.first_press_duration = duration;
                            } else if (state.short_press_count == 2) {
                                long total_press_duration = state.first_press_duration + duration;
                                if (total_press_duration < state.double_press_max)
                                    double_press();
                                else
                                    double_short_press();
                                state.short_press_count = 0;
                            }
                        }
                    }
                    state.press_count = 0;
                }
            }
        } else if (ret == 0) {
            long long current_time = current_time_ms();
            if (current_time - state.press_time >= state.short_press_max && !state.has_long_press_occurred && state.press_count > 0) {
                long duration = current_time - state.press_time;
                long_press();
                state.has_long_press_occurred = 1;
                state.short_press_count = 0;
            } else if (state.short_press_count == 1) {
                short_press();
                state.short_press_count = 0;
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
