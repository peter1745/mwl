#pragma once

#include <cstdint>
#include <functional>
#include <string_view>
#include <utility>

namespace mwl {

    template<typename T>
    struct Handle
    {
        struct Impl;

        Handle() noexcept = default;
        Handle(Impl* impl) noexcept
            : impl(impl) {}

        [[nodiscard]]
        auto is_valid() const noexcept -> bool { return impl; }

        [[nodiscard]]
        operator T() const noexcept { return T(impl); }

        [[nodiscard]]
        operator bool() const noexcept { return impl; }

        [[nodiscard]]
        auto operator->() const noexcept -> Impl* { return impl; }

        template<typename I = Impl>
        [[nodiscard]]
        auto unwrap() const noexcept -> I* { return static_cast<I*>(impl); }

    protected:
        Impl* impl = nullptr;
    };

    enum class ClientAPI
    {
        //TODO: Auto = 0,
    #if defined(MWL_INCLUDE_WAYLAND)
        Wayland,
    #endif
    };

    struct UnderlyingResourceID
    {
        uint16_t value;

        [[nodiscard]]
        auto operator==(const UnderlyingResourceID& other) const noexcept
        {
            return value == other.value;
        }

        template<typename>
        static auto id() -> UnderlyingResourceID
        {
            static auto id = generic_id();
            return { id };
        }

    private:
        static auto generic_id() -> uint16_t
        {
            static uint16_t id = 0;
            return id;
        }
    };

    struct State : Handle<State>
    {
        struct Desc
        {
            ClientAPI client_api{};
        };

        [[nodiscard]]
        static auto create(Desc desc) -> State;
        void destroy();

        void dispatch_events() const;

        template<typename T>
        [[nodiscard]]
        auto get_underlying_resource() const -> T*
        {
            return static_cast<T*>(get_underlying_resource_impl(UnderlyingResourceID::id<T>()));
        }

    private:
        [[nodiscard]]
        auto get_underlying_resource_impl(UnderlyingResourceID id) const -> void*;
    };

    struct ScreenBuffer : Handle<ScreenBuffer>
    {
        [[nodiscard]]
        auto operator[](size_t idx) const -> uint32_t&;
    };

    enum class ButtonState : uint8_t
    {
        Pressed,
        Released
    };

    enum class ScrollAxis : uint8_t
    {
        Vertical, Horizontal
    };

    enum class ScrollSource : uint8_t
    {
        Wheel, Finger, Continuous, WheelTilt
    };

    struct MouseMotionEvent
    {
        int32_t x;
        int32_t y;
    };

    struct KeyEvent
    {
        uint32_t key;
        ButtonState state;
    };

    struct MouseButtonEvent
    {
        // bit 0..1 -> unused
        // bit 2 -> state
        // bit 3..7 -> button
        uint8_t value;

        MouseButtonEvent(uint8_t button, ButtonState state)
            : value((button << 3) | (std::to_underlying(state) << 2))
        {
        }

        [[nodiscard]]
        auto state() const noexcept -> ButtonState
        {
            return static_cast<ButtonState>(value & 0b100);
        }

        [[nodiscard]]
        auto button() const noexcept -> uint8_t
        {
            return value >> 3;
        }
    };

    struct MouseScrollEvent
    {
        // bit 0..1 -> source
        // bit 2    -> axis
        // bit 3..7 -> value [-8..8]
        int8_t storage;

        MouseScrollEvent(ScrollAxis axis, ScrollSource source, int8_t value)
            : storage((value << 3) | (std::to_underlying(axis) << 2) | std::to_underlying(source))
        {
        }

        [[nodiscard]]
        auto source() const noexcept -> ScrollSource
        {
            return static_cast<ScrollSource>(storage & 0b11);
        }

        [[nodiscard]]
        auto axis() const noexcept -> ScrollAxis
        {
            return static_cast<ScrollAxis>((storage >> 2) & 0b1);
        }

        [[nodiscard]]
        auto value()  const noexcept -> int8_t
        {
            return (storage >> 3) * 15;
        }

    };

    struct Window : Handle<Window>
    {
        [[nodiscard]]
        static auto create(State state, std::string_view title, int32_t width, int32_t height) -> Window;
        void destroy();

        void show() const;

        [[nodiscard]] auto width() const -> int32_t;
        [[nodiscard]] auto height() const -> int32_t;

        using CloseCallback = std::function<void()>;
        void set_close_callback(CloseCallback handler) const;

        using SizeCallback = std::function<void(int32_t, int32_t)>;
        void set_size_callback(SizeCallback callback) const;

        void set_fullscreen_state(bool fullscreen) const;
        [[nodiscard]] auto is_fullscreen() const -> bool;

        using KeyCallback = std::function<void(KeyEvent)>;
        void set_key_callback(KeyCallback callback) const;

        using MouseMotionCallback = std::function<void(MouseMotionEvent)>;
        void set_mouse_motion_callback(MouseMotionCallback callback) const;

        using MouseButtonCallback = std::function<void(MouseButtonEvent)>;
        void set_mouse_button_callback(MouseButtonCallback callback) const;

        using MouseScrollCallback = std::function<void(MouseScrollEvent)>;
        void set_mouse_scroll_callback(MouseScrollCallback callback) const;

        [[nodiscard]]
        auto fetch_screen_buffer() const -> ScreenBuffer;
        void present_screen_buffer(const ScreenBuffer buffer) const;

        template<typename T>
        [[nodiscard]]
        auto get_underlying_resource() const -> T*
        {
            return static_cast<T*>(get_underlying_resource_impl(UnderlyingResourceID::id<T>()));
        }

    private:
        [[nodiscard]] auto get_underlying_resource_impl(UnderlyingResourceID id) const -> void*;
    };

}
