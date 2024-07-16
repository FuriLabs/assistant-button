// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024 Bardia Moshiri <fakeshell@bardia.tech>

#ifndef VIRTUAL_KEYBOARD_UNSTABLE_V1_CLIENT_PROTOCOL_H
#define VIRTUAL_KEYBOARD_UNSTABLE_V1_CLIENT_PROTOCOL_H

#include "wayland-client.h"

#ifdef  __cplusplus
extern "C" {
#endif

struct wl_seat;
struct zwp_virtual_keyboard_manager_v1;
struct zwp_virtual_keyboard_v1;

#ifndef ZWP_VIRTUAL_KEYBOARD_V1_INTERFACE
#define ZWP_VIRTUAL_KEYBOARD_V1_INTERFACE

extern const struct wl_interface zwp_virtual_keyboard_v1_interface;
#endif
#ifndef ZWP_VIRTUAL_KEYBOARD_MANAGER_V1_INTERFACE
#define ZWP_VIRTUAL_KEYBOARD_MANAGER_V1_INTERFACE
extern const struct wl_interface zwp_virtual_keyboard_manager_v1_interface;
#endif

#ifndef ZWP_VIRTUAL_KEYBOARD_V1_ERROR_ENUM
#define ZWP_VIRTUAL_KEYBOARD_V1_ERROR_ENUM
enum zwp_virtual_keyboard_v1_error {
	ZWP_VIRTUAL_KEYBOARD_V1_ERROR_NO_KEYMAP = 0,
};
#endif

#define ZWP_VIRTUAL_KEYBOARD_V1_KEYMAP 0
#define ZWP_VIRTUAL_KEYBOARD_V1_KEY 1
#define ZWP_VIRTUAL_KEYBOARD_V1_MODIFIERS 2
#define ZWP_VIRTUAL_KEYBOARD_V1_DESTROY 3

#define ZWP_VIRTUAL_KEYBOARD_V1_KEYMAP_SINCE_VERSION 1
#define ZWP_VIRTUAL_KEYBOARD_V1_KEY_SINCE_VERSION 1
#define ZWP_VIRTUAL_KEYBOARD_V1_MODIFIERS_SINCE_VERSION 1
#define ZWP_VIRTUAL_KEYBOARD_V1_DESTROY_SINCE_VERSION 1

static inline void
zwp_virtual_keyboard_v1_set_user_data(struct zwp_virtual_keyboard_v1 *zwp_virtual_keyboard_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwp_virtual_keyboard_v1, user_data);
}

static inline void *
zwp_virtual_keyboard_v1_get_user_data(struct zwp_virtual_keyboard_v1 *zwp_virtual_keyboard_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwp_virtual_keyboard_v1);
}

static inline uint32_t
zwp_virtual_keyboard_v1_get_version(struct zwp_virtual_keyboard_v1 *zwp_virtual_keyboard_v1)
{
	return wl_proxy_get_version((struct wl_proxy *) zwp_virtual_keyboard_v1);
}

static inline void
zwp_virtual_keyboard_v1_keymap(struct zwp_virtual_keyboard_v1 *zwp_virtual_keyboard_v1, uint32_t format, int32_t fd, uint32_t size)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwp_virtual_keyboard_v1,
			 ZWP_VIRTUAL_KEYBOARD_V1_KEYMAP, NULL, wl_proxy_get_version((struct wl_proxy *) zwp_virtual_keyboard_v1), 0, format, fd, size);
}

static inline void
zwp_virtual_keyboard_v1_key(struct zwp_virtual_keyboard_v1 *zwp_virtual_keyboard_v1, uint32_t time, uint32_t key, uint32_t state)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwp_virtual_keyboard_v1,
			 ZWP_VIRTUAL_KEYBOARD_V1_KEY, NULL, wl_proxy_get_version((struct wl_proxy *) zwp_virtual_keyboard_v1), 0, time, key, state);
}

static inline void
zwp_virtual_keyboard_v1_modifiers(struct zwp_virtual_keyboard_v1 *zwp_virtual_keyboard_v1, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwp_virtual_keyboard_v1,
			 ZWP_VIRTUAL_KEYBOARD_V1_MODIFIERS, NULL, wl_proxy_get_version((struct wl_proxy *) zwp_virtual_keyboard_v1), 0, mods_depressed, mods_latched, mods_locked, group);
}

static inline void
zwp_virtual_keyboard_v1_destroy(struct zwp_virtual_keyboard_v1 *zwp_virtual_keyboard_v1)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwp_virtual_keyboard_v1,
			 ZWP_VIRTUAL_KEYBOARD_V1_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *) zwp_virtual_keyboard_v1), WL_MARSHAL_FLAG_DESTROY);
}

#ifndef ZWP_VIRTUAL_KEYBOARD_MANAGER_V1_ERROR_ENUM
#define ZWP_VIRTUAL_KEYBOARD_MANAGER_V1_ERROR_ENUM
enum zwp_virtual_keyboard_manager_v1_error {
	ZWP_VIRTUAL_KEYBOARD_MANAGER_V1_ERROR_UNAUTHORIZED = 0,
};
#endif

#define ZWP_VIRTUAL_KEYBOARD_MANAGER_V1_CREATE_VIRTUAL_KEYBOARD 0

#define ZWP_VIRTUAL_KEYBOARD_MANAGER_V1_CREATE_VIRTUAL_KEYBOARD_SINCE_VERSION 1

static inline void
zwp_virtual_keyboard_manager_v1_set_user_data(struct zwp_virtual_keyboard_manager_v1 *zwp_virtual_keyboard_manager_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwp_virtual_keyboard_manager_v1, user_data);
}

static inline void *
zwp_virtual_keyboard_manager_v1_get_user_data(struct zwp_virtual_keyboard_manager_v1 *zwp_virtual_keyboard_manager_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwp_virtual_keyboard_manager_v1);
}

static inline uint32_t
zwp_virtual_keyboard_manager_v1_get_version(struct zwp_virtual_keyboard_manager_v1 *zwp_virtual_keyboard_manager_v1)
{
	return wl_proxy_get_version((struct wl_proxy *) zwp_virtual_keyboard_manager_v1);
}

static inline void
zwp_virtual_keyboard_manager_v1_destroy(struct zwp_virtual_keyboard_manager_v1 *zwp_virtual_keyboard_manager_v1)
{
	wl_proxy_destroy((struct wl_proxy *) zwp_virtual_keyboard_manager_v1);
}

static inline struct zwp_virtual_keyboard_v1 *
zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(struct zwp_virtual_keyboard_manager_v1 *zwp_virtual_keyboard_manager_v1, struct wl_seat *seat)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_flags((struct wl_proxy *) zwp_virtual_keyboard_manager_v1,
			 ZWP_VIRTUAL_KEYBOARD_MANAGER_V1_CREATE_VIRTUAL_KEYBOARD, &zwp_virtual_keyboard_v1_interface, wl_proxy_get_version((struct wl_proxy *) zwp_virtual_keyboard_manager_v1), 0, seat, NULL);

	return (struct zwp_virtual_keyboard_v1 *) id;
}

#ifdef  __cplusplus
}
#endif

#endif
