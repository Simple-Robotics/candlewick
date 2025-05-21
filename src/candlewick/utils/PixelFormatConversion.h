#pragma once

#include <SDL3/SDL_stdinc.h>

namespace candlewick {

/// \brief In-place conversion from 8-bit BGRA to 8-bit RGBA.
///
/// \param bgraPixels Pointer to input BGRA image.
/// \param pixelCount Number of pixels in the BGRA image.
void bgraToRgbaConvert(Uint32 *bgraPixels, Uint32 pixelCount);

} // namespace candlewick
