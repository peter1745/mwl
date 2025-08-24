#include "mwl_wayland.hpp"
#include "mwl_linux_input_tables.hpp"

#include <cerrno>
#include <ctime>
#include <string>
#include <atomic>
#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <sys/mman.h>
#include <string_view>

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

    static void xdg_wm_base_ping(void*, xdg_wm_base* wm_base, uint32_t serial)
    {
        xdg_wm_base_pong(wm_base, serial);
    }

    static constexpr auto wm_base_listener = xdg_wm_base_listener { xdg_wm_base_ping };

    static auto min_version(const uint32_t supported, const uint32_t requested) -> uint32_t
    {
        return std::min(supported, requested);
    }

    void pointer_enter(void* data, wl_pointer*, uint32_t, wl_surface* surface, wl_fixed_t, wl_fixed_t)
    {
        auto* impl = static_cast<WaylandStateImpl*>(data);
        impl->input.focused_pointer_window = static_cast<WaylandWindowImpl*>(wl_surface_get_user_data(surface));
    }

	void pointer_leave(void* data, wl_pointer*, uint32_t, wl_surface*)
	{
        auto* impl = static_cast<WaylandStateImpl*>(data);
        impl->input.focused_pointer_window = nullptr;
	}

	void pointer_motion(void* data, wl_pointer*, uint32_t, wl_fixed_t pointer_x, wl_fixed_t pointer_y)
	{
        auto* impl = static_cast<WaylandStateImpl*>(data);
        auto* event = impl->input.fetch_event<WaylandMouseMotionEvent>();
        event->x = wl_fixed_to_int(pointer_x);
        event->y = wl_fixed_to_int(pointer_y);
	}

	void pointer_button(void* data, wl_pointer*, uint32_t, uint32_t, uint32_t button, uint32_t state)
	{
        MWL_VERIFY(button_table.contains(button), "Unknown button");
        MWL_VERIFY(button_table.at(button) <= 31, "Cannot represent button codes above 31.");

        auto* impl = static_cast<WaylandStateImpl*>(data);
        auto* event = impl->input.fetch_event<WaylandMouseButtonEvent>();
        event->button = button_table.at(button);
        event->state = state == WL_KEYBOARD_KEY_STATE_PRESSED ? ButtonState::Pressed : ButtonState::Released;
	}

	void pointer_axis(void* data, wl_pointer*, uint32_t, uint32_t axis, wl_fixed_t)
	{
        auto* impl = static_cast<WaylandStateImpl*>(data);
        auto* event = impl->input.fetch_event<WaylandMouseScrollEvent>();
        event->axis = axis == WL_POINTER_AXIS_VERTICAL_SCROLL ? ScrollAxis::Vertical : ScrollAxis::Horizontal;
	}

	void pointer_axis_source(void* data, wl_pointer*, uint32_t axis_source)
	{
        auto* impl = static_cast<WaylandStateImpl*>(data);
        auto* event = impl->input.fetch_event<WaylandMouseScrollEvent>();

        if (axis_source == WL_POINTER_AXIS_SOURCE_WHEEL)
        {
            event->source = ScrollSource::Wheel;
        }
        else if (axis_source == WL_POINTER_AXIS_SOURCE_FINGER)
        {
            event->source = ScrollSource::Finger;
        }
        else if (axis_source == WL_POINTER_AXIS_SOURCE_CONTINUOUS)
        {
            event->source = ScrollSource::Continuous;
        }
        else if (axis_source == WL_POINTER_AXIS_SOURCE_WHEEL_TILT)
        {
            event->source = ScrollSource::WheelTilt;
        }
	}

	void pointer_axis_relative_direction(void* data, wl_pointer*, uint32_t, uint32_t relative_direction)
	{
        auto* impl = static_cast<WaylandStateImpl*>(data);
        auto* event = impl->input.fetch_event<WaylandMouseScrollEvent>();
        event->scalar = relative_direction == WL_POINTER_AXIS_RELATIVE_DIRECTION_INVERTED ? -1 : 1;
	}

	void pointer_axis_value120(void* data, wl_pointer*, uint32_t, int32_t value120)
	{
        auto* impl = static_cast<WaylandStateImpl*>(data);
        auto* event = impl->input.fetch_event<WaylandMouseScrollEvent>();
        event->value = value120 / 15; // Normalize range [-8..8]
	}

	void pointer_axis_stop(void*, wl_pointer*, uint32_t, uint32_t)
	{
        MWL_VERIFY(false, "Not implemented.");
	}

    // NOTE(Peter): Deprecated with wl_seat versions >=8, but we need to support it anyway in case
    //              the compositor doesn't support version 8 and above.
	void pointer_axis_discrete(void* data, wl_pointer*, uint32_t, int32_t discrete)
	{
        auto* impl = static_cast<WaylandStateImpl*>(data);

        if (discrete == 0)
        {
            // NOTE(Peter): For some reason we recieve events with a "0" step.
            //              Since that's the equivalent of no event we'll simply ignore this.
            impl->input.skip_current = true;
            return;
        }

        auto* event = impl->input.fetch_event<WaylandMouseScrollEvent>();
        event->value = discrete;
	}

    void pointer_frame(void* data, wl_pointer*)
    {
        auto* impl = static_cast<WaylandStateImpl*>(data);

        if (!impl->input.focused_pointer_window || !impl->input.current_event)
        {
            return;
        }

        if (impl->input.skip_current)
        {
            impl->input.current_event = nullptr;
            impl->input.skip_current = false;
            return;
        }

        auto call_if_set = []<typename... Args>(const auto& func, Args&&... args)
        {
            if (!func)
            {
                return;
            }

            func(std::forward<Args>(args)...);
        };

        switch (impl->input.current_event->type)
        {
            case WaylandEventType::Button:
            {
                auto* event = impl->input.fetch_event<WaylandMouseButtonEvent>();
                call_if_set(
                    impl->input.focused_pointer_window->mouse_button_callback,
                    MouseButtonEvent(event->button, event->state));
                break;
            }
            case WaylandEventType::MouseMotion:
            {
                auto* event = impl->input.fetch_event<WaylandMouseMotionEvent>();
                call_if_set(
                    impl->input.focused_pointer_window->mouse_motion_callback,
                    MouseMotionEvent(event->x, event->y));
                break;
            }
            case WaylandEventType::Scroll:
            {
                auto* event = impl->input.fetch_event<WaylandMouseScrollEvent>();
                call_if_set(
                    impl->input.focused_pointer_window->mouse_scroll_callback,
                    MouseScrollEvent(event->axis, event->source, event->value * event->scalar));
                break;
            }
            default:
            {
                MWL_VERIFY(false, "Unknown Wayland event type");
                break;
            }
        }

        impl->input.current_event = nullptr;
    }

    static constexpr auto pointer_listener = wl_pointer_listener {
        .enter = pointer_enter,
        .leave = pointer_leave,
        .motion = pointer_motion,
        .button = pointer_button,
        .axis = pointer_axis,
        .frame = pointer_frame,
        .axis_source = pointer_axis_source,
        .axis_stop = pointer_axis_stop,
        .axis_discrete = pointer_axis_discrete,
        .axis_value120 = pointer_axis_value120,
        .axis_relative_direction = pointer_axis_relative_direction,
    };

    void keyboard_keymap(void* data, wl_keyboard*, uint32_t format, int32_t fd, uint32_t size)
    {
        auto* impl = static_cast<WaylandStateImpl*>(data);

        MWL_VERIFY(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, "Unknown keymap format");

        auto* map_mem = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
        MWL_VERIFY(map_mem, "Unable to memory map keymap file");

        impl->input.keymap = xkb_keymap_new_from_string(impl->input.ctx, map_mem, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);

        munmap(map_mem, size);
        close(fd);

        impl->input.state = xkb_state_new(impl->input.keymap);
    }

	void keyboard_enter(void* data, wl_keyboard*, uint32_t, wl_surface* surface, wl_array*)
	{
        auto* impl = static_cast<WaylandStateImpl*>(data);
        impl->input.focused_keyboard_window = static_cast<WaylandWindowImpl*>(wl_surface_get_user_data(surface));
	}

	void keyboard_leave(void* data, wl_keyboard*, uint32_t, wl_surface*)
	{
        auto* impl = static_cast<WaylandStateImpl*>(data);
        impl->input.focused_keyboard_window = nullptr;
	}

	void keyboard_key(void* data, wl_keyboard*, uint32_t, uint32_t, uint32_t key, uint32_t state)
	{
        MWL_VERIFY(key_table.contains(key), "Unknown key");

        auto* impl = static_cast<WaylandStateImpl*>(data);

        if (!impl->input.focused_keyboard_window || !impl->input.focused_keyboard_window->key_callback)
        {
            return;
        }

        auto key_state = state == WL_KEYBOARD_KEY_STATE_PRESSED ? ButtonState::Pressed : ButtonState::Released;
        impl->input.focused_keyboard_window->key_callback(KeyEvent(key_table.at(key), key_state));
	}

	void keyboard_modifiers(void* data, wl_keyboard*, uint32_t, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
	{
        auto* impl = static_cast<WaylandStateImpl*>(data);
        xkb_state_update_mask(impl->input.state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
	}

	void keyboard_repeat_info(void*, wl_keyboard*, int32_t, int32_t)
	{
        // TODO
	}

    static constexpr auto keyboard_listener = wl_keyboard_listener {
        .keymap = keyboard_keymap,
        .enter = keyboard_enter,
        .leave = keyboard_leave,
        .key = keyboard_key,
        .modifiers = keyboard_modifiers,
        .repeat_info = keyboard_repeat_info,
    };

    static void seat_capabilities(void* data, wl_seat* seat, uint32_t capabilities)
    {
        auto* impl = static_cast<WaylandStateImpl*>(data);

        if (capabilities & WL_SEAT_CAPABILITY_POINTER)
        {
            impl->input.pointer = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(impl->input.pointer, &pointer_listener, data);
        }
        else if (impl->input.pointer)
        {
            wl_pointer_release(impl->input.pointer);
            impl->input.pointer = nullptr;
        }

        if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD)
        {
            impl->input.keyboard = wl_seat_get_keyboard(seat);
            wl_keyboard_add_listener(impl->input.keyboard, &keyboard_listener, data);
        }
        else if (impl->input.keyboard)
        {
            wl_keyboard_release(impl->input.keyboard);
            impl->input.keyboard = nullptr;
        }

        if (capabilities & WL_SEAT_CAPABILITY_TOUCH)
        {
            // No touch support
        }
    }

    static void seat_name(void*, wl_seat*, const char *name)
    {
        std::println("Seat name = {}", name);
    }

    static constexpr auto seat_listener = wl_seat_listener {
        .capabilities = seat_capabilities,
        .name = seat_name
    };

    static void output_geometry(void* data, wl_output*, int32_t, int32_t, int32_t, int32_t, int32_t, const char* make, const char* model, int32_t)
    {
        static_cast<WaylandOutput*>(data)->make = make;
        static_cast<WaylandOutput*>(data)->model = model;
    }

	static void output_mode(void*, wl_output*, uint32_t, int32_t, int32_t, int32_t)
	{
	}

	static void output_done(void* data, wl_output*)
	{
        auto output = static_cast<WaylandOutput*>(data);

        std::println("----- OUTPUT -----");
        std::println("Name: {}", output->name);
        std::println("Description: {}", output->description);
        std::println("Make: {}", output->make);
        std::println("Model: {}", output->model);
	}

	static void output_scale(void*, wl_output*, int32_t)
	{
	}

	static void output_name(void* data, wl_output*, const char* name)
	{
	    static_cast<WaylandOutput*>(data)->name = name;
	}

	static void output_description(void* data, wl_output*, const char* description)
	{
	    static_cast<WaylandOutput*>(data)->description = description;
	}

    static constexpr auto output_listener = wl_output_listener {
        .geometry = output_geometry,
        .mode = output_mode,
        .done = output_done,
        .scale = output_scale,
        .name = output_name,
        .description = output_description
    };

    static void registry_receive_global(void* data, wl_registry* reg, uint32_t name, const char* interface, uint32_t supported_version)
    {
        auto* impl = static_cast<WaylandStateImpl*>(data);

        if (const auto iview = std::string_view{ interface }; iview == wl_compositor_interface.name)
        {
            impl->compositor = {
                static_cast<wl_compositor*>(wl_registry_bind(
                    reg,
                    name,
                    &wl_compositor_interface,
                    min_version(supported_version, 6)
                )),
                name
            };
        }
        else if (iview == xdg_wm_base_interface.name)
        {
            impl->xdg_data.wm_base = {
                static_cast<xdg_wm_base*>(wl_registry_bind(
                    reg,
                    name,
                    &xdg_wm_base_interface,
                    min_version(supported_version, 6)
                )),
                name
            };

            xdg_wm_base_add_listener(impl->xdg_data.wm_base, &wm_base_listener, data);
        }
        else if (iview == wl_shm_interface.name)
        {
            impl->shm = {
                static_cast<wl_shm*>(wl_registry_bind(
                    reg,
                    name,
                    &wl_shm_interface,
                    min_version(supported_version, 1)
                )),
                name
            };
        }
        else if (iview == wl_seat_interface.name)
        {
            impl->input.seat = {
                static_cast<wl_seat*>(wl_registry_bind(
                    reg,
                    name,
                    &wl_seat_interface,
                    min_version(supported_version, 9)
                )),
                name
            };

            wl_seat_add_listener(impl->input.seat, &seat_listener, data);
        }
        else if (iview == zxdg_decoration_manager_v1_interface.name)
        {
            impl->decoration_manager = {
                static_cast<zxdg_decoration_manager_v1*>(wl_registry_bind(
                    reg,
                    name,
                    &zxdg_decoration_manager_v1_interface,
                    min_version(supported_version, 1)
                )),
                name
            };
        }
        else if (iview == wl_output_interface.name)
        {
            auto output = std::make_unique<WaylandOutput>(WaylandOutput {
                .global = {
                    static_cast<wl_output*>(wl_registry_bind(
                        reg,
                        name,
                        &wl_output_interface,
                        min_version(supported_version, 4)
                    )),
                    name
                },
                .name = "",
                .description = "",
                .make = "",
                .model = ""
            });

            wl_output_add_listener(output->global, &output_listener, output.get());

            impl->outputs.emplace_back(std::move(output));
        }
        else if (iview == wp_fractional_scale_manager_v1_interface.name)
        {
            impl->fractional_scale_manager = {
                static_cast<wp_fractional_scale_manager_v1*>(wl_registry_bind(
                    reg,
                    name,
                    &wp_fractional_scale_manager_v1_interface,
                    min_version(supported_version, 1)
                )),
                name
            };
        }
    }

    static void registry_remove_global(void* data, wl_registry*, uint32_t name)
    {
        if (auto* impl = static_cast<WaylandStateImpl*>(data); name == impl->compositor.name)
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
        else if (name == impl->decoration_manager.name)
        {
            zxdg_decoration_manager_v1_destroy(impl->decoration_manager);
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

        wl_display_roundtrip(display);
        wl_display_disconnect(display);

        if (buffers_created > buffers_destroyed)
        {
            std::println("Created {} buffers, destroyed {}", buffers_created.load(), buffers_destroyed.load());
        }
    }

    void WaylandStateImpl::init()
    {
        // Connect to the display server
        display = wl_display_connect(nullptr);

        registry = wl_display_get_registry(display);
        wl_registry_add_listener(registry, &registry_listener, this);

        input.ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

        // Block until all pending requests are processed by the server.
        // Required to guarantee that e.g compositor is valid
        wl_display_roundtrip(display);
    }

    void WaylandStateImpl::dispatch_events()
    {
        wl_display_dispatch(display);
    }

    auto WaylandStateImpl::get_underlying_resource(UnderlyingResourceID id) const -> void*
    {
        if (id == UnderlyingResourceID::id<wl_display>())
        {
            return display;
        }

        MWL_VERIFY(false, "Unsupported underlying resource!");
        return nullptr;
    }

    static void surface_configure(void* data, xdg_surface* xdg_surface, uint32_t serial)
    {
        auto* impl = static_cast<WaylandWindowImpl*>(data);
        xdg_surface_ack_configure(xdg_surface, serial);
        impl->has_valid_surface = true;
    }

    static constexpr auto surface_listener = xdg_surface_listener { surface_configure };

    static void toplevel_configure(void* data, xdg_toplevel*, int32_t width, int32_t height, wl_array*)
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

            if (win->size_callback)
            {
                win->size_callback(win->width, win->height);
            }
        }
    }

	static void toplevel_close(void* data, xdg_toplevel*)
	{
        if (const auto* win = static_cast<Window::Impl*>(data); win->close_callback)
        {
            win->close_callback();
        }
	}

	static void toplevel_configure_bounds(void*, xdg_toplevel*, int32_t, int32_t)
	{
	    // TODO(Peter): Do we actually need to do anything here?
	    //std::println("toplevel_configure_bounds = {}, {}", width, height);
		//MWL_VERIFY(false, "Not Implemented");
	}

	static void toplevel_wm_capabilities(void* data, xdg_toplevel*, wl_array* capabilities)
	{
        auto* win = static_cast<WaylandWindowImpl*>(data);
        auto caps = std::span{ static_cast<uint32_t*>(capabilities->data), capabilities->size / sizeof(uint32_t) };

        for (uint32_t i = 0; i < XDG_TOPLEVEL_WM_CAPABILITIES_MAX; ++i)
        {
            win->xdg_data.wm_capabilities[i] = std::ranges::contains(caps, i);
        }
	}

    static constexpr auto toplevel_listener = xdg_toplevel_listener {
        .configure = toplevel_configure,
        .close = toplevel_close,
        .configure_bounds = toplevel_configure_bounds,
        .wm_capabilities = toplevel_wm_capabilities,
    };

    static void fractional_scale_preferred_scale(void* data, wp_fractional_scale_v1*, uint32_t scale)
    {
        static_cast<WaylandWindowImpl*>(data)->preferred_scaling = scale / 120.0f;
    }
    static constexpr auto fractional_scale_listener = wp_fractional_scale_v1_listener { fractional_scale_preferred_scale };

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
        wl_surface_set_user_data(surface, this);

        xdg_data.surface = xdg_wm_base_get_xdg_surface(state_impl->xdg_data.wm_base, surface);
        xdg_surface_add_listener(xdg_data.surface, &surface_listener, this);

        xdg_data.toplevel = xdg_surface_get_toplevel(xdg_data.surface);
        xdg_toplevel_add_listener(xdg_data.toplevel, &toplevel_listener, this);

        // Set window properties
        xdg_toplevel_set_app_id(xdg_data.toplevel, title.data());
        xdg_toplevel_set_title(xdg_data.toplevel, title.data());

        if (state_impl->decoration_manager)
        {
            xdg_data.decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(state_impl->decoration_manager, xdg_data.toplevel);
            zxdg_toplevel_decoration_v1_set_mode(xdg_data.decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
        }

        if (state_impl->fractional_scale_manager)
        {
            fractional_scale = wp_fractional_scale_manager_v1_get_fractional_scale(state_impl->fractional_scale_manager, surface);
            wp_fractional_scale_v1_add_listener(fractional_scale, &fractional_scale_listener, this);
        }

        wl_surface_commit(surface);
        
        // NOTE(Peter): I don't entirely like doing this here, but it does ensure all window properties
        // are set so that the user can query them immediately.
        wl_display_roundtrip(state_impl->display);
    }

    // NOTE(Peter): Wayland windows won't show up until you draw something to them.
    //              Here we just clear the screen to #222222
    void WaylandWindowImpl::show()
    {
        const auto buffer = fetch_screen_buffer();
        if (buffer)
        {
            std::memset(buffer->pixel_buffer, 0xFF222222, buffer->pixel_buffer_size);
            present_screen_buffer(buffer);
        }
    }

    void WaylandWindowImpl::set_fullscreen_state(bool fullscreen)
    {
        if (!xdg_data.wm_capabilities[XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN])
        {
            return;
        }

        is_fullscreen = fullscreen;

        if (fullscreen)
            xdg_toplevel_set_fullscreen(xdg_data.toplevel, nullptr);
        else
            xdg_toplevel_unset_fullscreen(xdg_data.toplevel);
    }

    auto WaylandWindowImpl::fetch_screen_buffer() -> ScreenBuffer
    {
        auto* state = this->state.unwrap<WaylandStateImpl>();

        if (!has_valid_surface)
        {
            std::println("Returning invalid buffer");
            return {};
        }

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
        if (has_valid_surface)
        {
            wl_surface_attach(surface, buffer.unwrap<WaylandScreenBufferImpl>()->buffer, 0, 0);
            wl_surface_commit(surface);
        }
    }

    auto WaylandWindowImpl::get_underlying_resource(UnderlyingResourceID id) const -> void*
    {
        if (id == UnderlyingResourceID::id<wl_surface>())
        {
            return surface;
        }

        MWL_VERIFY(false, "Unsupported underlying resource!");
        return nullptr;
    }

}
