/*
 * Copyright (C) 2019 ~ 2019 Union Technology Co., Ltd.
 *
 * Author:     chengong <chengong@uniontech.com>
 *
 * Maintainer: chengong <chengong@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef KEYBOARDPLANTFORM_WAYLAND_P_H
#define KEYBOARDPLANTFORM_WAYLAND_P_H

#include "keyboardplantform_wayland.h"

#include <qpa/qplatformnativeinterface.h>

#include <wayland-client.h>

/**
 * @brief The KeyboardPlantformWaylandPrivate class
 * @chinese wayland实现监听键盘事件
 */

class KeyboardPlantformWaylandPrivate
{
public:
    KeyboardPlantformWaylandPrivate(KeyboardPlantformWayland *q);

    void wlDispatch();
    /**
     * @brief isCapslockOn
     * @return current CapsLock key state.
     */
    static bool isCapslockOn() {return  isCapsLocked;}
    /**
     * @brief isNumlockOn
     * @return current NumLock key state.
     */
    static bool isNumlockOn() {return isNumLocked;}
    /**
     * @brief release
     * release keyboard and seat
     */
    void release();
    /**
     * @brief destroy
     * destroy registry,seat,and keyboard
     */
    void destroy();

private:
    /**
     * @brief destroy
     * copy form wl_registry_listener
     * announce global object
     *
     * Notify the client of global objects.
     *
     * The event notifies the client that a global object with the
     * given name is now available, and it implements the given version
     * of the given interface.
     * @param name numeric name of the global object
     * @param interface interface implemented by the object
     * @param version interface version
     */
    static void handleGlobalCallback(void *data,
                                     struct wl_registry *wl_registry,
                                     uint32_t name,
                                     const char *interface,
                                     uint32_t version);

    /**
     * @brief destroy
     * copy form wl_registry_listener
     * announce removal of global object
     *
     * Notify the client of removed global objects.
     *
     * This event notifies the client that the global identified by
     * name is no longer available. If the client bound to the global
     * using the bind request, the client should now destroy that
     * object.
     *
     * The object remains valid and requests to the object will be
     * ignored until the client destroys it, to avoid races between the
     * global going away and a client sending a request to it.
     * @param name numeric name of the global object
     */
    static void handleGlobalRemoveCallBack(void *data,
                                           struct wl_registry *wl_registry,
                                           uint32_t name);

    /**
     * @brief keymapCallback
     * keyboard mapping
     *
     * This event provides a file descriptor to the client which can
     * be memory-mapped to provide a keyboard mapping description.
     * @param format keymap format
     * @param fd keymap file descriptor
     * @param size keymap size, in bytes
     */
    static void keymapCallback(void *data,
                               struct wl_keyboard *wl_keyboard,
                               uint32_t format,
                               int32_t fd,
                               uint32_t size);

    /**
     * @brief enterCallback
     * copy form wl_keyboard_listener
     * enter event
     *
     * Notification that this seat's keyboard focus is on a certain
     * surface.
     * @param serial serial number of the enter event
     * @param surface surface gaining keyboard focus
     * @param keys the currently pressed keys
     */
    static void enterCallback(void *data,
                              struct wl_keyboard *wl_keyboard,
                              uint32_t serial,
                              struct wl_surface *surface,
                              struct wl_array *keys);

    /**
     * @brief leaveCallback
     * copy form wl_keyboard_listener
     * leave event
     *
     * Notification that this seat's keyboard focus is no longer on a
     * certain surface.
     *
     * The leave notification is sent before the enter notification for
     * the new focus.
     * @param serial serial number of the leave event
     * @param surface surface that lost keyboard focus
     */
    static void leaveCallback(void *data,
                              struct wl_keyboard *wl_keyboard,
                              uint32_t serial,
                              struct wl_surface *surface);

    /**
     * @brief keyCallback
     * copy form wl_keyboard_listener
     * key event
     *
     * A key was pressed or released. The time argument is a
     * timestamp with millisecond granularity, with an undefined base.
     * @param serial serial number of the key event
     * @param time timestamp with millisecond granularity
     * @param key key that produced the event
     * @param state physical state of the key
     */
    static void keyCallback(void *data,
                            struct wl_keyboard *wl_keyboard,
                            uint32_t serial,
                            uint32_t time,
                            uint32_t key,
                            uint32_t state);

    /**
     * @brief modifiersCallback
     * copy form wl_keyboard_listener
     * modifier and group state
     *
     * Notifies clients that the modifier and/or group state has
     * changed, and it should update its local state.
     * @param serial serial number of the modifiers event
     * @param mods_depressed depressed modifiers
     * @param mods_latched latched modifiers
     * @param mods_locked locked modifiers
     * @param group keyboard layout
     */
    static void modifiersCallback(void *data,
                                  struct wl_keyboard *wl_keyboard,
                                  uint32_t serial,
                                  uint32_t mods_depressed,
                                  uint32_t mods_latched,
                                  uint32_t mods_locked,
                                  uint32_t group);

    /**
     * @brief repeatInfoCallback
     * copy form wl_keyboard_listener
     * repeat rate and delay
     *
     * Informs the client about the keyboard's repeat rate and delay.
     *
     * This event is sent as soon as the wl_keyboard object has been
     * created, and is guaranteed to be received by the client before
     * any key press event.
     *
     * Negative values for either rate or delay are illegal. A rate of
     * zero will disable any repeating (regardless of the value of
     * delay).
     *
     * This event can be sent later on as well with a new value if
     * necessary, so clients should continue listening for the event
     * past the creation of wl_keyboard.
     * @param rate the rate of repeating keys in characters per second
     * @param delay delay in milliseconds since key down until repeating starts
     * @since 4
     */
    static void repeatInfoCallback(void *data,
                                   struct wl_keyboard *wl_keyboard,
                                   int32_t rate,
                                   int32_t delay);

    void initWayland();

private:
    //q
    KeyboardPlantformWayland *q = nullptr;
    //wayland display
    wl_display *display = nullptr;
    //wayland resigtry
    wl_registry *registry = nullptr;

    //wayland seat
    static wl_seat *seat;
    //wayland keyboard
    static wl_keyboard *keyboard;
    //为registry设置回调函数
    static const wl_keyboard_listener keyboardListener;
    //为keyboard设置回调函数
    static const wl_registry_listener registryListener;

    //capslocked状态
    static bool isCapsLocked;
    //numlocked状态
    static bool isNumLocked;
};


#endif // KEYBOARDPLANTFORM_WAYLAND_P_H
