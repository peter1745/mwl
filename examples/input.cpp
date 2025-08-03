#include "example_helper.hpp"

#include <print>

int main()
{
    auto is_running = true;

    auto mwl_state = mwl::State::create({
        .client_api = mwl::ClientAPI::Auto
    });

    auto win = mwl::Window::create(mwl_state, "Hello", 1920, 1080);
    win.set_close_callback([&] { is_running = false; });

    win.set_key_callback([](mwl::KeyEvent event)
    {
        std::println("Key {} is {}", event.key, event.state == mwl::ButtonState::Pressed ? "Pressed" : "Released");
    });

    win.set_mouse_motion_callback([](mwl::MouseMotionEvent event)
    {
        std::println("MouseMotionEvent(X: {}, Y: {})", event.x, event.y);
    });

    win.set_mouse_button_callback([](mwl::MouseButtonEvent event)
    {
        std::println("ButtonEvent(Button: {}, State: {})", event.button(), event.state() == mwl::ButtonState::Pressed ? "Pressed" : "Released");
    });

    win.set_mouse_scroll_callback([](mwl::MouseScrollEvent event)
    {
        auto source = [&]
        {
            switch (event.source())
            {
                case mwl::ScrollSource::Wheel: return "Wheel";
                case mwl::ScrollSource::Finger: return "Finger";
                case mwl::ScrollSource::Continuous: return "Continuous";
                case mwl::ScrollSource::WheelTilt: return "WheelTilt";
                default: return "Unknown";
            }
        }();

        auto axis = [&]
        {
            switch (event.axis())
            {
                case mwl::ScrollAxis::Vertical: return "Vertical";
                case mwl::ScrollAxis::Horizontal: return "Horizontal";
                default: return "Unknown";
            }
        }();

        std::println("ScrollEvent(Source: {}, Axis: {}, Value: {})", source, axis, event.value());
    });

    win.show();

    while (is_running)
    {
        mwl_state.dispatch_events();
        draw_checkerboard(win);
    }

    win.destroy();
    mwl_state.destroy();

    return 0;
}
