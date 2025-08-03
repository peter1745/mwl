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

    win.set_size_callback([&](int32_t width, int32_t height)
    {
        std::println("Window Size = {}, {}", width, height);
    });

    win.show();

    while (is_running)
    {
        mwl_state.dispatch_events();

        auto buffer = win.fetch_screen_buffer();
        for (int32_t x = 0; x < win.width(); x++)
        {
            for (int32_t y = 0; y < win.height(); y++)
            {
                if ((x + y / 32 * 32) % 64 < 32)
                {
                    buffer[y * win.width() + x] = 0xFF666666;
                }
                else
                {
                    buffer[y * win.width() + x] = 0xFFEEEEEE;
                }
            }
        }
        win.present_screen_buffer(buffer);
    }

    win.destroy();
    mwl_state.destroy();

    return 0;
}
