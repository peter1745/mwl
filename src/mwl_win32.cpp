#include "mwl_win32.hpp"

namespace mwl {

    using namespace std::literals;

    static constexpr auto Win32ClassName = "MWL_WINDOW_CLASS"sv;

    static void create_screen_buffer(HWND hwnd, int32_t width, int32_t height, ScreenBuffer buffer)
    {
        MWL_VERIFY(buffer.is_valid(), "Invalid buffer passed to create_screen_buffer");

        auto* buffer_impl = buffer.unwrap<Win32ScreenBufferImpl>();

        if (buffer_impl->bitmap)
        {
            DeleteObject(buffer_impl->bitmap);
        }

        auto dc = GetDC(hwnd);

        auto bmi = BITMAPINFO {
            .bmiHeader = {
                .biSize = sizeof(BITMAPINFOHEADER),
                .biWidth = width,
                .biHeight = -height,
                .biPlanes = 1,
                .biBitCount = 32,
                .biCompression = BI_RGB,
            }
        };

        buffer_impl->bitmap = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&buffer_impl->pixel_buffer), nullptr, 0);
        buffer_impl->pixel_buffer_size = width * height * sizeof(uint32_t);

        ReleaseDC(hwnd, dc);
    }

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
            case WM_SIZE:
            {
                auto new_width = LOWORD(lparam);
                auto new_height = HIWORD(lparam);

                if (impl->width != new_width || impl->height != new_height)
                {
                    if (!impl->front_buffer.is_valid())
                    {
                        impl->front_buffer = { new Win32ScreenBufferImpl() };
                        impl->back_buffer = { new Win32ScreenBufferImpl() };
                    }

                    create_screen_buffer(impl->window_handle, new_width, new_height, impl->front_buffer);
                    create_screen_buffer(impl->window_handle, new_width, new_height, impl->back_buffer);

                    GdiFlush();

                    impl->width = new_width;
                    impl->height = new_height;

                    if (impl->size_callback)
                    {
                        impl->size_callback(impl->width, impl->height);
                    }
                }

                break;
            }
            case WM_PAINT:
            {
                auto ps = PAINTSTRUCT{};
                auto hdc = BeginPaint(hwnd, &ps);
                auto hdc_bmp = CreateCompatibleDC(hdc);
                auto old_bmp = SelectObject(hdc_bmp, impl->front_buffer.unwrap<Win32ScreenBufferImpl>()->bitmap);

                BitBlt(hdc, 0, 0, impl->width, impl->height, hdc_bmp, 0, 0, SRCCOPY);

                SelectObject(hdc, old_bmp);
                DeleteDC(hdc_bmp);
                EndPaint(hwnd, &ps);
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
        UnregisterClass(Win32ClassName.data(), GetModuleHandle(nullptr));
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
        DestroyWindow(window_handle);
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

    void Win32WindowImpl::show()
    {
        ShowWindow(window_handle, SW_SHOW);
    }

    void Win32WindowImpl::set_fullscreen_state(bool)
    {
    }

    auto Win32WindowImpl::fetch_screen_buffer() -> ScreenBuffer
    {
        std::swap(front_buffer, back_buffer);
        return front_buffer;
    }

    void Win32WindowImpl::present_screen_buffer(const ScreenBuffer) const
    {
        InvalidateRect(window_handle, nullptr, FALSE);
    }

    auto Win32WindowImpl::get_underlying_resource(UnderlyingResourceID) const -> void*
    {
        return nullptr;
    }

}
