#pragma once

#include "mwl_impl.hpp"
#include "wayland-xdg-shell-client-protocol.h"

namespace mwl {
    struct WaylandWindowImpl;

    template<typename WaylandObj>
    struct wayland_global
    {
        WaylandObj* ptr;
        uint32_t name;

        operator WaylandObj*() { return ptr; }
    };

    struct WaylandStateImpl final : State::Impl
    {
        ~WaylandStateImpl() override;

        wl_display* display;
        wl_registry* registry;
        wayland_global<wl_compositor> compositor;
        wayland_global<wl_shm> shm;

        struct {
            wayland_global<xdg_wm_base> wm_base;
        } xdg_data;

        void init();
        void dispatch_events() override;
    };

    struct WaylandScreenBufferImpl final : ScreenBuffer::Impl
    {
        wl_buffer* buffer;
    };

    // NOTE(Peter): Curse you XDG for not providing a XDG_TOPLEVEL_WM_CAPABILITIES_MAX value...
    static constexpr size_t XDG_TOPLEVEL_WM_CAPABILITIES_MAX = XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE;

    struct WaylandWindowImpl final : Window::Impl
    {
        ~WaylandWindowImpl() override;

        wl_surface* surface;

        struct {
            xdg_surface* surface;
            xdg_toplevel* toplevel;
            std::array<bool, XDG_TOPLEVEL_WM_CAPABILITIES_MAX> wm_capabilities;
        } xdg_data;

        void init();

        void show() const override;

        void set_fullscreen_state(bool fullscreen) override;

        [[nodiscard]] auto fetch_screen_buffer() const -> ScreenBuffer override;
        void present_screen_buffer(const ScreenBuffer buffer) const override;
    };

}
