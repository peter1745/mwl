#pragma once

#include <mwl/mwl.hpp>

inline void draw_checkerboard(mwl::Window win)
{
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

