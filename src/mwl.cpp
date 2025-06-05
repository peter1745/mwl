#include "mwl_impl.hpp"

#if defined(MWL_INCLUDE_WAYLAND)
    #include "mwl_wayland.hpp"
#endif

#include <print>
#include <cstdint>

namespace mwl {

    auto State::create(Desc desc) -> State
    {
        Impl* state_impl = nullptr;

        switch (desc.client_api)
        {
        #if defined(MWL_INCLUDE_WAYLAND)
        case ClientAPI::Wayland:
        {
            auto* wayland_state = new WaylandStateImpl();
            wayland_state->desc = desc;
            wayland_state->init();
            state_impl = wayland_state;
            break;
        }
        #endif
        default:
        {
            std::println("Unable to create wml::State with ClientAPI {}.", static_cast<int32_t>(desc.client_api));
            return {};
        }
        }

        return { state_impl };
    }

    void State::destroy()
    {
        delete impl;
        impl = nullptr;
    }

    void State::dispatch_events() const
    {
        impl->dispatch_events();
    }

    auto ScreenBuffer::operator[](size_t idx) const -> uint32_t&
    {
        MWL_VERIFY(impl, "Trying to index into an empty ScreenBuffer");
        MWL_VERIFY(idx < impl->pixel_buffer_size, std::format("Trying to access ScreenBuffer out of range. idx = {}, size = {}", idx, impl->pixel_buffer_size));
        return impl->pixel_buffer[idx];
    }

    auto Window::create(State state, std::string_view title, int32_t width, int32_t height) -> Window
    {
        Impl* window_impl = nullptr;

        switch (state->desc.client_api)
        {
        #if defined(MWL_INCLUDE_WAYLAND)
        case ClientAPI::Wayland:
        {
            auto* wayland_window = new WaylandWindowImpl();
            wayland_window->state = state;
            wayland_window->title = title;
            wayland_window->width = width;
            wayland_window->height = height;
            wayland_window->init();
            window_impl = wayland_window;
            break;
        }
        #endif
        default:
        {
            std::println("Unable to create wml::State with ClientAPI {}.", static_cast<int32_t>(state->desc.client_api));
            return {};
        }
        }

        return { window_impl };
    }

    void Window::destroy()
    {
        delete impl;
        impl = nullptr;
    }

    void Window::show() const
    {
        impl->show();
    }

    auto Window::width() const -> int32_t
    {
        return impl->width;
    }

    auto Window::height() const -> int32_t
    {
        return impl->height;
    }

    void Window::add_close_handler(CloseHandler handler) const
    {
        impl->close_handlers.emplace_back(std::move(handler));
    }

    auto Window::fetch_screen_buffer() const -> ScreenBuffer
    {
        return impl->fetch_screen_buffer();
    }

    void Window::present_screen_buffer(const ScreenBuffer buffer) const
    {
        MWL_VERIFY(buffer.is_valid(), "Trying to present an invalid ScreenBuffer", void_t{});
        impl->present_screen_buffer(buffer);
    }

}
