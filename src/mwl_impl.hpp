#pragma once

#include "mwl/mwl.hpp"

#include <vector>

namespace mwl {

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
        std::vector<Window::CloseHandler> close_handlers;

        [[nodiscard]]
        virtual auto fetch_screen_buffer() const -> ScreenBuffer = 0;
        virtual void present_screen_buffer(ScreenBuffer buffer) const = 0;
    };

}
