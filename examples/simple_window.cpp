#include <mwl/mwl.hpp>

int main()
{
    auto is_running = true;

    auto mwl_state = mwl::State::create({
        .client_api = mwl::ClientAPI::Wayland
    });

    auto win = mwl::Window::create(mwl_state, "Hello", 1920, 1080);
    win.set_close_handler([&] { is_running = false; });
    win.show();

    while (is_running)
    {
        mwl_state.dispatch_events();
    }

    win.destroy();
    mwl_state.destroy();

    return 0;
}
