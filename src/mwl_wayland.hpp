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

    struct WaylandStateImpl final : State::Impl
    {
        wl_display* display;
        wl_registry* registry;
        wl_compositor* compositor;
        wl_shm* shm;

        struct {
            xdg_wm_base* wm_base;
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
        wl_surface* surface;

        struct {
            xdg_surface* surface;
            xdg_toplevel* toplevel;
        } xdg_data;

        void init();

        [[nodiscard]]
        auto fetch_screen_buffer() const -> ScreenBuffer override;
        void present_screen_buffer(const ScreenBuffer buffer) const override;
    };

}
