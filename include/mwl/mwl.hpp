#pragma once

#include <cstdint>
#include <functional>
#include <string_view>

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

        operator T() const noexcept { return T(impl); }
        operator bool() const noexcept { return impl; }

        auto operator->() const noexcept -> Impl* { return impl; }

        template<typename I = Impl>
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

    struct State : Handle<State>
    {
        struct Desc
        {
            ClientAPI client_api{};
        };

        static auto create(Desc desc) -> State;
        void destroy();

        void dispatch_events() const;
    };

    struct ScreenBuffer : Handle<ScreenBuffer>
    {
        auto operator[](size_t idx) const -> uint32_t&;
    };

    struct Window : Handle<Window>
    {
        static auto create(State state, std::string_view title, int32_t width, int32_t height) -> Window;
        void destroy();

        void show() const;

        [[nodiscard]]
        auto width() const -> int32_t;

        [[nodiscard]]
        auto height() const -> int32_t;

        using CloseCallback = std::function<void()>;
        void set_close_callback(CloseCallback handler) const;

        using SizeCallback = std::function<void(int32_t, int32_t)>;
        void set_size_callback(SizeCallback callback) const;

        [[nodiscard]]
        auto fetch_screen_buffer() const -> ScreenBuffer;
        void present_screen_buffer(const ScreenBuffer buffer) const;
    };

}
