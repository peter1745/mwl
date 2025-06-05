#pragma once

#include "mwl_impl.hpp"

struct wl_display;
struct wl_registry;
struct wl_compositor;
struct wl_surface;
struct wl_callback;
struct wl_shm;
struct wl_buffer;
struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;

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

    struct WaylandWindowImpl final : Window::Impl
    {
        ~WaylandWindowImpl() override;

        wl_surface* surface;

        struct {
            xdg_surface* surface;
            xdg_toplevel* toplevel;
        } xdg_data;

        void init();

        void show() const override;

        [[nodiscard]]
        auto fetch_screen_buffer() const -> ScreenBuffer override;
        void present_screen_buffer(const ScreenBuffer buffer) const override;
    };

}
