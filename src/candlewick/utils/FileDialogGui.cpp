#include "FileDialogGui.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_dialog.h>

#include <imgui.h>

namespace candlewick {

static const SDL_DialogFileFilter screenshot_filters[] = {
    {"PNG images", "png"},
    {"JPEG images", "jpg;jpeg"},
    {"All images", "png;jpg;jpeg"},
    {"All files", "*"},
};

static void fileCallbackImpl(void *userdata_, const char *const *filelist,
                             int filter) {
  if (!filelist) {
    SDL_Log("An error occured: %s", SDL_GetError());
    return;
  } else if (!*filelist) {
    SDL_Log("The user did not select any file.");
    SDL_Log("Most likely, the dialog was canceled.");
    return;
  }

  auto *data = (GuiFileSaveDialog *)userdata_;

  while (*filelist) {
    SDL_Log("Full path to selected file: %s", *filelist);
    data->filename = filelist[0];
    filelist++;
  }

  if (filter < 0) {
    SDL_Log("The current platform does not support fetching "
            "the selected filter, or the user did not select"
            " any filter.");
  } else if (size_t(filter) < SDL_arraysize(screenshot_filters)) {
    SDL_Log("The filter selected by the user is '%s' (%s).",
            screenshot_filters[filter].pattern,
            screenshot_filters[filter].name);
  }
}

void GuiFileSaveDialog::addFileDialog(SDL_Window *window) {
  const char *initial_path = SDL_GetUserFolder(SDL_FOLDER_PICTURES);

  if (ImGui::Button("Select file")) {
    SDL_ShowSaveFileDialog(fileCallbackImpl, this, window, screenshot_filters,
                           SDL_arraysize(screenshot_filters), initial_path);
  }
  ImGui::SameLine();
  ImGui::Text("Selected file: %s", filename.c_str());
}

} // namespace candlewick
