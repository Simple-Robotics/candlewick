#pragma once

#include "Core.h"
#include "errors.h"
#include <array>
#include <SDL3/SDL_gpu.h>

namespace candlewick {

/// \brief RAII wrapper for the \c SDL_Window opaque type.
struct Window {

  /// \brief Standard constructor, which forwards to \c SDL_CreateWindow.
  explicit Window(const char *title, Sint32 w, Sint32 h, SDL_WindowFlags flags);

  /// \brief This constructor takes ownership of the provided handle.
  explicit Window(SDL_Window *ptr) : _handle(ptr) {}

  Window(const Window &) = delete;
  Window(Window &&other) noexcept : _handle(other._handle) {
    other._handle = nullptr;
  }

  Window &operator=(Window &&other) noexcept {
    if (this != &other) {
      this->destroy();
      _handle = other._handle;
      other._handle = nullptr;
    }
    return *this;
  }

  operator SDL_Window *() const { return _handle; }

  std::array<int, 2> size() const;

  std::array<int, 2> sizeInPixels() const;

  float pixelDensity() const { return SDL_GetWindowPixelDensity(_handle); }

  float displayScale() const { return SDL_GetWindowDisplayScale(_handle); }

  SDL_PixelFormat pixelFormat() const {
    return SDL_GetWindowPixelFormat(_handle);
  }

  SDL_WindowFlags flags() const { return SDL_GetWindowFlags(_handle); }

  bool setTitle(const char *title) {
    return SDL_SetWindowTitle(_handle, title);
  }

  std::string_view title() const { return SDL_GetWindowTitle(_handle); }

  void destroy() noexcept {
    if (_handle)
      SDL_DestroyWindow(_handle);
    _handle = nullptr;
  }

  ~Window() noexcept { this->destroy(); }

private:
  SDL_Window *_handle;
};

inline Window::Window(const char *title, Sint32 w, Sint32 h,
                      SDL_WindowFlags flags) {
  _handle = SDL_CreateWindow(title, w, h, flags);
  if (!_handle) {
    throw RAIIException(SDL_GetError());
  }
}

inline std::array<int, 2> Window::size() const {
  int width, height;
  SDL_GetWindowSize(_handle, &width, &height);
  return {width, height};
}

inline std::array<int, 2> Window::sizeInPixels() const {
  int width, height;
  SDL_GetWindowSizeInPixels(_handle, &width, &height);
  return {width, height};
}

} // namespace candlewick
