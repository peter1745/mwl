#include <mwl/mwl.hpp>

int main()
{
    auto is_running = true;

    const auto mwl_state = mwl::State::create({
        .client_api = mwl::ClientAPI::Wayland,
        .expose_pixel_buffer = true
    });

    const auto win = mwl::Window::create(mwl_state, "Hello", 1920, 1080);
    win.add_close_handler([&] { is_running = false; });

    while (is_running)
    {
        mwl_state.dispatch_events();

        // Draw a grid to the screen
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

    return 0;
}
