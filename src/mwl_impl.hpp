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

    template<>
    struct Handle<State>::Impl
    {
        State::Desc desc;

        virtual ~Impl() = default;
        virtual void dispatch_events() = 0;
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

        virtual void show() const = 0;

        [[nodiscard]]
        virtual auto fetch_screen_buffer() const -> ScreenBuffer = 0;
        virtual void present_screen_buffer(ScreenBuffer buffer) const = 0;
    };

}
