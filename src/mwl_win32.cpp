#include "mwl_win32.hpp"

namespace mwl {

    using namespace std::literals;

    static constexpr auto Win32ClassName = "MWL_WINDOW_CLASS"sv;

    static auto CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) -> LRESULT
    {
        Win32WindowImpl* impl = nullptr;

        if (msg != WM_CREATE)
        {
            impl = std::bit_cast<Win32WindowImpl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        switch (msg)
        {
            case WM_CREATE:
            {
                auto* create_struct = std::launder(reinterpret_cast<CREATESTRUCT*>(lparam));
                SetWindowLongPtr(hwnd, GWLP_USERDATA, std::bit_cast<LONG_PTR>(create_struct->lpCreateParams));
                break;
            }
            case WM_CLOSE:
            {
                impl->close_callback();
                break;
            }
            case WM_DESTROY:
            {
                PostQuitMessage(0);
                 break;
            }
            default:
            {
                break;
            }
        }

        return DefWindowProcA(hwnd, msg, wparam, lparam);
    }

    Win32StateImpl::~Win32StateImpl()
    {
    }

    void Win32StateImpl::init()
    {
        window_class = {
            .cbSize = sizeof(WNDCLASSEXA),
            .style = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = window_proc,
            .hInstance = GetModuleHandle(nullptr),
            .lpszClassName = Win32ClassName.data()
        };

        RegisterClassEx(&window_class);
    }

    void Win32StateImpl::dispatch_events()
    {
        auto msg = MSG{};

        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    auto Win32StateImpl::get_underlying_resource(UnderlyingResourceID) const -> void*
    {
        return nullptr;
    }

    Win32WindowImpl::~Win32WindowImpl()
    {
    }

    void Win32WindowImpl::init()
    {
        window_handle = CreateWindowExA(
            WS_EX_OVERLAPPEDWINDOW,

            Win32ClassName.data(),
            title.data(),

            WS_OVERLAPPEDWINDOW,

            CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT,

            nullptr,
            nullptr,

            GetModuleHandle(nullptr),

            this
        );
    }

    void Win32WindowImpl::show() const
    {
        ShowWindow(window_handle, SW_SHOW);
    }

    void Win32WindowImpl::set_fullscreen_state(bool)
    {
    }

    auto Win32WindowImpl::fetch_screen_buffer() const -> ScreenBuffer
    {
        return {};
    }

    void Win32WindowImpl::present_screen_buffer(const ScreenBuffer) const
    {
    }

    auto Win32WindowImpl::get_underlying_resource(UnderlyingResourceID) const -> void*
    {
        return nullptr;
    }

}
