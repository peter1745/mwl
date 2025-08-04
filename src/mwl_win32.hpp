#pragma once

#include "mwl_impl.hpp"

#define NOMINMAX
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace mwl {

    struct Win32StateImpl final : State::Impl
    {
        ~Win32StateImpl() override;

        WNDCLASSEXA window_class;

        void init();
        void dispatch_events() override;

        auto get_underlying_resource(UnderlyingResourceID id) const -> void* override;
    };

    struct Win32WindowImpl final : Window::Impl
    {
        ~Win32WindowImpl() override;

        HWND window_handle;
        ScreenBuffer front_buffer;
        ScreenBuffer back_buffer;

        void init();
        void show() const override;
        void set_fullscreen_state(bool fullscreen) override;

        [[nodiscard]] auto fetch_screen_buffer() -> ScreenBuffer override;
        void present_screen_buffer(const ScreenBuffer buffer) const override;

        auto get_underlying_resource(UnderlyingResourceID id) const -> void* override;
    };

    struct Win32ScreenBufferImpl final : ScreenBuffer::Impl
    {
        HBITMAP bitmap;
    };
}
