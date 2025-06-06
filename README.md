# Modern Windowing Library (MWL)

MWL is a small windowing library that I decided to create after messing around with
making a [Wayland](https://wayland.freedesktop.org/) window. It's by no means
a serious windowing library, and the aim isn't to compete with [GLFW](https://www.glfw.org/) or [SDL](https://www.glfw.org/).

My main goal with MWL is to write a modern but simple windowing library that can work with Vulkan.

## Supported Environments
- [x] Wayland
- [ ] X11
- [ ] Win32

While I would like to properly support both X11 and Win32, my main focus for now 
is Wayland as that's what I'm personally using right now. There are
no plans for supporting MacOS.
