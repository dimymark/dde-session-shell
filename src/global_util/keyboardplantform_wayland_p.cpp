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
#include "keyboardplantform_wayland_p.h"

#include <QGuiApplication>

const char *seat_str = "wl_seat";

wl_seat *KeyboardPlantformWaylandPrivate::seat = nullptr;
wl_keyboard *KeyboardPlantformWaylandPrivate::keyboard = nullptr;

bool KeyboardPlantformWaylandPrivate::isCapsLocked = false;
bool KeyboardPlantformWaylandPrivate::isNumLocked = false;

const wl_keyboard_listener KeyboardPlantformWaylandPrivate::keyboardListener = {
    keymapCallback,
    enterCallback,
    leaveCallback,
    keyCallback,
    modifiersCallback,
    repeatInfoCallback
};

const wl_registry_listener KeyboardPlantformWaylandPrivate::registryListener = {
    handleGlobalCallback,
    handleGlobalRemoveCallBack
};

KeyboardPlantformWaylandPrivate::KeyboardPlantformWaylandPrivate(KeyboardPlantformWayland *q)
    : q(q)
{
    initWayland();
}

void KeyboardPlantformWaylandPrivate::wlDispatch()
{
    if (display != nullptr) {
        wl_display_dispatch(display);
    }
}

void KeyboardPlantformWaylandPrivate::handleGlobalCallback(void *data,
                                                           struct wl_registry *wl_registry,
                                                           uint32_t name,
                                                           const char *interface,
                                                           uint32_t version)
{
    auto p = reinterpret_cast<KeyboardPlantformWaylandPrivate *>(data);
    Q_ASSERT(p->registry == wl_registry);

    if (strcmp(interface, seat_str) == 0) {
        //绑定seat设备管理器
        seat = reinterpret_cast<wl_seat *>(wl_registry_bind(wl_registry, name, &wl_seat_interface, version));
        Q_ASSERT(seat != NULL);

        //获取键盘设备
        keyboard = wl_seat_get_keyboard(seat);
        Q_ASSERT(keyboard != NULL);

        //创建键盘设备回调函数
        wl_keyboard_add_listener(keyboard, &keyboardListener, data);
    }
}

void KeyboardPlantformWaylandPrivate::handleGlobalRemoveCallBack(void *data, wl_registry *wl_registry, uint32_t name)
{
    Q_UNUSED(data);
    Q_UNUSED(wl_registry);
    Q_UNUSED(name);
}

void KeyboardPlantformWaylandPrivate::keymapCallback(void *data, wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size)
{
    Q_UNUSED(data);
    Q_UNUSED(wl_keyboard);
    Q_UNUSED(format);
    Q_UNUSED(fd);
    Q_UNUSED(size);
}

void KeyboardPlantformWaylandPrivate::enterCallback(void *data, wl_keyboard *wl_keyboard, uint32_t serial, wl_surface *surface, wl_array *keys)
{
    Q_UNUSED(data);
    Q_UNUSED(wl_keyboard);
    Q_UNUSED(serial);
    Q_UNUSED(surface);
    Q_UNUSED(keys);
}

void KeyboardPlantformWaylandPrivate::leaveCallback(void *data, wl_keyboard *wl_keyboard, uint32_t serial, wl_surface *surface)
{
    Q_UNUSED(data);
    Q_UNUSED(wl_keyboard);
    Q_UNUSED(serial);
    Q_UNUSED(surface);
}

void KeyboardPlantformWaylandPrivate::keyCallback(void *data, wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
    Q_UNUSED(data);
    Q_UNUSED(wl_keyboard);
    Q_UNUSED(serial);
    Q_UNUSED(time);
    Q_UNUSED(key);
    Q_UNUSED(state);
}

void KeyboardPlantformWaylandPrivate::modifiersCallback(void *data, wl_keyboard *wl_keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
    //创建监听后会立刻收到当前lock状态
    Q_UNUSED(serial);
    Q_UNUSED(wl_keyboard);
    Q_UNUSED(mods_depressed);
    Q_UNUSED(mods_latched);
    Q_UNUSED(group);

    //判断keyboard
    auto p = reinterpret_cast<KeyboardPlantformWaylandPrivate *>(data);
    Q_ASSERT(p->keyboard == wl_keyboard);

    //通过mods_locked参数获取CapsLock和NumLock状态
    isCapsLocked = ((mods_locked & 0x02) != 0);
    isNumLocked = ((mods_locked & 0x10) != 0);

    emit p->q->capslockStatusChanged(isCapsLocked);
    emit p->q->numlockStatusChanged(isCapsLocked);
}

void KeyboardPlantformWaylandPrivate::repeatInfoCallback(void *data, wl_keyboard *wl_keyboard, int32_t rate, int32_t delay)
{
    Q_UNUSED(data);
    Q_UNUSED(wl_keyboard);
    Q_UNUSED(rate);
    Q_UNUSED(delay);
}

void KeyboardPlantformWaylandPrivate::initWayland()
{
    //从当前的app获取wayland display
    auto dp = QGuiApplication::platformNativeInterface()->nativeResourceForWindow("display", nullptr);
    //    display = wl_display_connect(NULL); --用这个接口创建client接收不到事件
    display = reinterpret_cast<wl_display *>(dp);
    Q_ASSERT(display != NULL);

    //获取全局对象注册表
    registry = wl_display_get_registry(display);
    Q_ASSERT(registry != NULL);

    //创建监听回调函数
    wl_registry_add_listener(registry, &registryListener, this);

    //处理消息
    wl_display_roundtrip(display);
    wl_display_get_fd(display);
}

void KeyboardPlantformWaylandPrivate::release()
{
    wl_keyboard_release(keyboard);
    wl_seat_release(seat);
}

void KeyboardPlantformWaylandPrivate::destroy()
{
    wl_keyboard_destroy(keyboard);
    wl_seat_destroy(seat);
    wl_registry_destroy(registry);
    display = nullptr;
}
