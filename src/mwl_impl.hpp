#pragma once

#include "mwl/mwl.hpp"

#include <print>
#include <vector>
#include <stacktrace>

#if !defined(MWL_DISABLE_TRAPS)
    #if defined(MWL_PLATFORM_LINUX)
        #include <sys/signal.h>
        #define MWL_TRAP() raise(SIGTRAP)
    #elif defined(MWL_PLATFORM_WINDOWS)
        #define MWL_TRAP() __debugbreak()
    #endif
#endif

#define MWL_VERIFY(cond, msg, ...) do {\
    if (!(cond))\
    {\
        auto st = std::stacktrace::current();\
        std::println("MWL: Verify Failed. Condition: {}\nMessage: {}\nStack: {}", #cond, msg, std::to_string(st));\
        MWL_TRAP();\
        __VA_OPT__(return) __VA_ARGS__;\
    }\
} while(false)

namespace mwl {

    using void_t = std::void_t<>;

    // TODO(Peter): We shouldn't have virtual functions in Impl
    //              instead we could store function pointers to the actual underlying
    //              implementations. That way we avoid the vtable entirely.
    //              It might be annoying to hook up the functions, but we could solve that
    //              with e.g macro-iterating them.
    //              Another alternative is rethinking using Handle at all here. There may
    //              be a more optimal pattern available.

    template<>
    struct Handle<State>::Impl
    {
        State::Desc desc;

        virtual ~Impl() = default;
        virtual void dispatch_events() = 0;

        virtual auto get_underlying_resource(UnderlyingResourceID id) const -> void* = 0;
    };

    template<>
    struct Handle<ScreenBuffer>::Impl
    {
        uint32_t* pixel_buffer;
        size_t pixel_buffer_size;
    };

    template<>
    struct Handle<Window>::Impl
    {
        virtual ~Impl() = default;

        State state;
        std::string_view title;
        int32_t width;
        int32_t height;
        Window::CloseCallback close_callback;
        Window::SizeCallback size_callback;
        Window::KeyCallback key_callback;
        Window::MouseMotionCallback mouse_motion_callback;
        Window::MouseButtonCallback mouse_button_callback;
        Window::MouseScrollCallback mouse_scroll_callback;

        bool is_fullscreen;

        virtual void show() = 0;

        virtual void set_fullscreen_state(bool fullscreen) = 0;

        [[nodiscard]] virtual auto fetch_screen_buffer() -> ScreenBuffer = 0;
        virtual void present_screen_buffer(ScreenBuffer buffer) const = 0;

        virtual auto get_underlying_resource(UnderlyingResourceID id) const -> void* = 0;
    };

}
