#include "mwl_wayland.hpp"

#include <cerrno>
#include <cstring>

#include "wayland-xdg-shell-client-protocol.h"

#include <ctime>
#include <string>
#include <atomic>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string_view>
#include <wayland-client.h>

namespace mwl {

    static void randname(char* buf)
    {
        auto ts = timespec{};
        clock_gettime(CLOCK_REALTIME, &ts);
        auto r = ts.tv_nsec;
        for (int32_t i = 0; i < 6; ++i)
        {
            buf[i] = 'A' + (r & 15) + (r & 16) * 2;
            r >>= 5;
        }
    }

    static auto create_shm_file() -> int32_t
    {
        int32_t retries = 100;

        do
        {
            char name[] = "/wl_shm-XXXXXX";
            randname(name + sizeof(name) - 7);

            --retries;

            if (const auto fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600); fd >= 0)
            {
                shm_unlink(name);
                return fd;
            }
        } while (retries > 0 && errno == EEXIST);

        return -1;
    }

    static auto allocate_shm_file(const size_t size) -> int32_t
    {
        const int32_t fd = create_shm_file();
        
        if (fd < 0)
        {
            return -1;
        }
        
        int32_t ret;
        
        do
        {
            ret = ftruncate(fd, size);
        } while (ret < 0 && errno == EINTR);

        if (ret < 0)
        {
            close(fd);
            return -1;
        }

        return fd;
    }

    static std::atomic_uint64_t buffers_created = 0;
    static std::atomic_uint64_t buffers_destroyed = 0;

    static void buffer_release(void* data, wl_buffer* buffer)
    {
        // Sent by the compositor when it's no longer using this buffer
        wl_buffer_destroy(buffer);
        ++buffers_destroyed;

        const auto* impl = static_cast<WaylandScreenBufferImpl*>(data);
        munmap(impl->pixel_buffer, impl->pixel_buffer_size);
        delete impl;
    }
    static constexpr wl_buffer_listener buffer_listener = { buffer_release };

    static void xdg_wm_base_ping(void* data, xdg_wm_base* wm_base, uint32_t serial)
    {
        xdg_wm_base_pong(wm_base, serial);
    }

    static constexpr auto wm_base_listener = xdg_wm_base_listener { xdg_wm_base_ping };

    static void registry_receive_global(void* data, wl_registry* reg, uint32_t name, const char* interface, uint32_t version)
    {
        auto* impl = static_cast<WaylandStateImpl*>(data);

        if (const auto iview = std::string_view{ interface }; iview == wl_compositor_interface.name)
        {
            impl->compositor = {
                static_cast<wl_compositor*>(wl_registry_bind(reg, name, &wl_compositor_interface, 6)),
                name
            };
        }
        else if (iview == xdg_wm_base_interface.name)
        {
            impl->xdg_data.wm_base = {
                static_cast<xdg_wm_base*>(wl_registry_bind(reg, name, &xdg_wm_base_interface, 6)),
                name
            };

            xdg_wm_base_add_listener(impl->xdg_data.wm_base, &wm_base_listener, data);
        }
        else if (iview == wl_shm_interface.name)
        {
            impl->shm = {
                static_cast<wl_shm*>(wl_registry_bind(reg, name, &wl_shm_interface, 1)),
                name
            };
        }
    }

    static void registry_remove_global(void* data, wl_registry* reg, uint32_t name)
    {
        auto* impl = static_cast<WaylandStateImpl*>(data);

        if (name == impl->compositor.name)
        {
            wl_compositor_destroy(impl->compositor);
        }
        else if (name == impl->xdg_data.wm_base.name)
        {
            xdg_wm_base_destroy(impl->xdg_data.wm_base);
        }
        else if (name == impl->shm.name)
        {
            wl_shm_destroy(impl->shm);
        }
    }

    static constexpr auto registry_listener = wl_registry_listener {
        .global = registry_receive_global,
        .global_remove = registry_remove_global,
    };

    WaylandStateImpl::~WaylandStateImpl()
    {
        // NOTE(Peter): Manually invoking these here since it seems like
        //              the wayland server isn't dispatching the calls on display_disconnect.
        registry_remove_global(this, registry, compositor.name);
        registry_remove_global(this, registry, xdg_data.wm_base.name);
        registry_remove_global(this, registry, shm.name);

        wl_registry_destroy(registry);

        wl_display_flush(display);
        wl_display_disconnect(display);

        // NOTE(Peter): For some reason there are buffers that the display server
        //              isn't telling us to destroy? Maybe track the buffers manually and clean up
        //              the ones that are leaking.
        std::println("Created {} buffers, destroyed {}.", buffers_created.load(), buffers_destroyed.load());
    }

    void WaylandStateImpl::init()
    {
        // Connect to the display server
        display = wl_display_connect(nullptr);

        registry = wl_display_get_registry(display);
        wl_registry_add_listener(registry, &registry_listener, this);

        // Block until all pending requests are processed by the server.
        // Required to guarantee that e.g compositor is valid
        wl_display_roundtrip(display);
    }

    void WaylandStateImpl::dispatch_events()
    {
        wl_display_dispatch(display);
    }

    static void surface_configure(void* data, xdg_surface* xdg_surface, uint32_t serial)
    {
        xdg_surface_ack_configure(xdg_surface, serial);
    }

    static constexpr auto surface_listener = xdg_surface_listener { surface_configure };

    static void toplevel_configure(void* data, xdg_toplevel* toplevel, int32_t width, int32_t height, wl_array* states)
    {
	    auto* win = static_cast<WaylandWindowImpl*>(data);

        if (width == 0 || height == 0)
        {
            // TODO
            return;
        }

        if (width != win->width || height != win->height)
        {
            win->width = width;
            win->height = height;
        }
    }

	static void toplevel_close(void* data, xdg_toplevel* toplevel)
	{
        static_cast<Window::Impl*>(data)->close_handler();
	}

	static void toplevel_configure_bounds(void* data, xdg_toplevel* toplevel, int32_t width, int32_t height)
	{

	}

	static void toplevel_wm_capabilities(void* data, xdg_toplevel* toplevel, wl_array* capabilities)
	{

	}

    static constexpr auto toplevel_listener = xdg_toplevel_listener {
        .configure = toplevel_configure,
        .close = toplevel_close,
        .configure_bounds = toplevel_configure_bounds,
        .wm_capabilities = toplevel_wm_capabilities,
    };

    WaylandWindowImpl::~WaylandWindowImpl()
    {
        xdg_toplevel_destroy(xdg_data.toplevel);
        xdg_surface_destroy(xdg_data.surface);
        wl_surface_destroy(surface);
    }

    void WaylandWindowImpl::init()
    {
        auto* state_impl = state.unwrap<WaylandStateImpl>();

        surface = wl_compositor_create_surface(state_impl->compositor);

        xdg_data.surface = xdg_wm_base_get_xdg_surface(state_impl->xdg_data.wm_base, surface);
        xdg_surface_add_listener(xdg_data.surface, &surface_listener, this);

        xdg_data.toplevel = xdg_surface_get_toplevel(xdg_data.surface);
        xdg_toplevel_add_listener(xdg_data.toplevel, &toplevel_listener, this);

        // Set window properties
        xdg_toplevel_set_app_id(xdg_data.toplevel, "voxel-game");
        xdg_toplevel_set_title(xdg_data.toplevel, title.data());

        wl_surface_commit(surface);
    }

    // NOTE(Peter): Wayland windows won't show up until you draw something to them.
    //              Here we just clear the screen to #222222
    void WaylandWindowImpl::show() const
    {
        const auto buffer = fetch_screen_buffer();
        std::memset(buffer->pixel_buffer, 0xFF222222, buffer->pixel_buffer_size);
        present_screen_buffer(buffer);
    }

    auto WaylandWindowImpl::fetch_screen_buffer() const -> ScreenBuffer
    {
        auto* state = this->state.unwrap<WaylandStateImpl>();
        const auto stride = width * 4;
        const auto pixel_buffer_size = stride * height;
        const auto fd = allocate_shm_file(pixel_buffer_size);

        if (fd == -1)
        {
            return {};
        }

        auto* pixel_buffer = static_cast<uint32_t*>(mmap(nullptr, pixel_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));

        if (pixel_buffer == MAP_FAILED)
        {
            close(fd);
            return {};
        }

        auto* pool = wl_shm_create_pool(state->shm, fd, pixel_buffer_size);
        auto* buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
        ++buffers_created;
        wl_shm_pool_destroy(pool);
        close(fd);

        auto* buffer_impl = new WaylandScreenBufferImpl();
        buffer_impl->buffer = buffer;
        buffer_impl->pixel_buffer = pixel_buffer;
        buffer_impl->pixel_buffer_size = pixel_buffer_size;

        wl_buffer_add_listener(buffer, &buffer_listener, buffer_impl);

        return { buffer_impl };
    }

    void WaylandWindowImpl::present_screen_buffer(const ScreenBuffer buffer) const
    {
        wl_surface_attach(surface, buffer.unwrap<WaylandScreenBufferImpl>()->buffer, 0, 0);
        wl_surface_commit(surface);
    }

}
