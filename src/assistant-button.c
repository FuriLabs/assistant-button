// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024 Bardia Moshiri <fakeshell@bardia.tech>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <time.h>
#include <errno.h>
#include <poll.h>
#include <string.h>

#define DEFAULT_SHORT_PRESS_MAX 500  // ms
#define DEFAULT_DEVICE "/dev/input/event1"
#define CONFIG_FILE "/etc/assistant-button.conf"

long long current_time_ms() {
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);
    return spec.tv_sec * 1000LL + spec.tv_nsec / 1e6;
}

void read_config(int *short_press_max, char *device) {
    FILE *file = fopen(CONFIG_FILE, "r");
    if (file == NULL) {
        perror("Failed to open the config file");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "SHORT_PRESS_MAX=%d", short_press_max) == 1) {
            continue;
        }
        if (sscanf(line, "DEVICE=%s", device) == 1) {
            continue;
        }
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    int fd;
    struct input_event ev;
    struct pollfd pfd;
    long long last_press_time = 0, press_time = 0, release_time = 0;
    int press_count = 0;
    int has_long_press_occurred = 0;
    int short_press_max = DEFAULT_SHORT_PRESS_MAX;
    char device[256] = DEFAULT_DEVICE;

    read_config(&short_press_max, device);

    if (argc > 1) {
        short_press_max = atoi(argv[1]);
    }

    if (argc > 2) {
        strncpy(device, argv[2], sizeof(device) - 1);
        device[sizeof(device) - 1] = '\0';
    }

    fd = open(device, O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        perror("Failed to open the device");
        return EXIT_FAILURE;
    }

    pfd.fd = fd;
    pfd.events = POLLIN;

    printf("Monitoring for key code 102 gestures on %s with short press max %d ms...\n", device, short_press_max);

    while (1) {
        int ret = poll(&pfd, 1, short_press_max);
        if (ret > 0) {
            if (read(fd, &ev, sizeof(struct input_event)) == -1) {
                perror("Failed to read the event");
                close(fd);
                return EXIT_FAILURE;
            }

            if (ev.type == EV_KEY && ev.code == 102) {
                if (ev.value == 1) { // Key press
                    press_time = current_time_ms();
                    press_count++;
                    last_press_time = press_time;
                    has_long_press_occurred = 0;
                    printf("Detected press. Time: %lld ms\n", press_time);
                } else if (ev.value == 0) { // Key release
                    if (!has_long_press_occurred) {  // Only process release if long press wasn't triggered by timeout
                        release_time = current_time_ms();
                        long duration = release_time - press_time;
                        printf("Detected release. Duration: %ld ms\n", duration);

                        if (duration < short_press_max) {
                            printf("Short Press\n");
                        } else {
                            printf("Long Press\n");
                        }
                    }
                    press_count = 0;
                }
            }
        } else if (ret == 0) { // Timeout
            long long current_time = current_time_ms();
            if (current_time - press_time >= short_press_max && !has_long_press_occurred && press_count > 0) {
                long duration = current_time - press_time;
                printf("Long Press reached short press max Duration: %ld ms\n", duration);
                has_long_press_occurred = 1;
            }
        } else {
            if (errno != EINTR) {
                perror("Poll failed");
                close(fd);
                return EXIT_FAILURE;
            }
        }
    }

    close(fd);
    return 0;
}
