#pragma once

#include "mwl_impl.hpp"
#include "wayland-xdg-shell-client-protocol.h"

#include <xkbcommon/xkbcommon.h>

namespace mwl {
    struct WaylandWindowImpl;

    template<typename WaylandObj>
    struct wayland_global
    {
        WaylandObj* ptr;
        uint32_t name;

        operator WaylandObj*() { return ptr; }
    };

    enum class WaylandEventType
    {
        None,
        Button,
        MouseMotion,
        Scroll
    };

    struct WaylandEvent
    {
        static constexpr auto static_type = WaylandEventType::None;
        WaylandEventType type = WaylandEventType::None;
    };

    struct WaylandMouseButtonEvent : WaylandEvent
    {
        static constexpr auto static_type = WaylandEventType::Button;
        uint32_t button;
        ButtonState state;
    };

    struct WaylandMouseMotionEvent : WaylandEvent
    {
        static constexpr auto static_type = WaylandEventType::MouseMotion;
        int32_t x;
        int32_t y;
    };

    struct WaylandMouseScrollEvent : WaylandEvent
    {
        static constexpr auto static_type = WaylandEventType::Scroll;

        ScrollSource source;
        ScrollAxis axis;
        int8_t value;
        int8_t scalar;
    };

    struct WaylandStateImpl final : State::Impl
    {
        ~WaylandStateImpl() override;

        wl_display* display;
        wl_registry* registry;
        wayland_global<wl_compositor> compositor;
        wayland_global<wl_shm> shm;

        struct {
            wayland_global<wl_seat> seat;
            wl_pointer* pointer;
            wl_keyboard* keyboard;

            xkb_context* ctx;
            xkb_keymap* keymap;
            xkb_state* state;

            WaylandWindowImpl* focused_keyboard_window;
            WaylandWindowImpl* focused_pointer_window;

            WaylandMouseButtonEvent* pre_allocated_button_event;
            WaylandMouseMotionEvent* pre_allocated_motion_event;
            WaylandMouseScrollEvent* pre_allocated_scroll_event;

            WaylandEvent* current_event;
            bool skip_current;

            template<typename T>
            auto allocate_if_null(T*& var) -> T*
            {
                if (var != nullptr)
                {
                    return var;
                }

                var = new T();
                var->type = T::static_type;
                return var;
            }

            template<typename T>
            auto fetch_event() -> T*
            {
                switch (T::static_type)
                {
                    case WaylandEventType::Button:
                    {
                        current_event = allocate_if_null(pre_allocated_button_event);
                        break;
                    }
                    case WaylandEventType::MouseMotion:
                    {
                        current_event = allocate_if_null(pre_allocated_motion_event);
                        break;
                    }
                    case WaylandEventType::Scroll:
                    {
                        current_event = allocate_if_null(pre_allocated_scroll_event);
                        break;
                    }
                    default:
                    {
                        MWL_VERIFY(false, "Unknown Wayland event");
                        current_event = nullptr;
                        break;
                    }
                }

                return static_cast<T*>(current_event);
            }
        } input;

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
