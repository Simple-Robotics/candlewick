#pragma once

#include <SDL3/SDL_video.h>
#include <string>

namespace candlewick {

struct GuiFileSaveDialog {
  std::string filename;

  void addFileDialog(SDL_Window *window);
};

} // namespace candlewick
