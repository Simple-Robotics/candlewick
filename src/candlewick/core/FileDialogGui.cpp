#include "GuiSystem.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_dialog.h>

#include <imgui.h>
#include <chrono>
#include <iomanip>

namespace candlewick {

static const SDL_DialogFileFilter g_screenshot_filters[] = {
    {"PNG images", "png"},
    {"JPEG images", "jpg;jpeg"},
    {"All images", "png;jpg;jpeg"},
    {"All files", "*"},
};

static const SDL_DialogFileFilter g_video_filters[] = {
    {"MP4 files", "mp4;m4v"},
    {"All files", "*"},
};

static const std::pair<const SDL_DialogFileFilter *, int> g_file_filters[] = {
    {g_screenshot_filters, SDL_arraysize(g_screenshot_filters)},
    {g_video_filters, SDL_arraysize(g_video_filters)},
};

static void fileCallbackImpl(void *userdata_, const char *const *filelist,
                             int) {
  if (!filelist) {
    SDL_Log("An error occured: %s", SDL_GetError());
    return;
  } else if (!*filelist) {
    SDL_Log("The user did not select any file.");
    SDL_Log("Most likely, the dialog was canceled.");
    return;
  }

  std::string *data = (std::string *)userdata_;
  while (*filelist) {
    *data = *filelist;
    filelist++;
  }
}

static const SDL_Folder g_dialog_file_type_folder[] = {
    SDL_FOLDER_PICTURES,
    SDL_FOLDER_VIDEOS,
};

void guiAddFileDialog(SDL_Window *window, DialogFileType dialog_file_type,
                      std::string &out) {
  const char *initial_path =
      SDL_GetUserFolder(g_dialog_file_type_folder[int(dialog_file_type)]);

  auto [filters, nfilters] = g_file_filters[int(dialog_file_type)];

  if (ImGui::Button("Select...")) {
    SDL_ShowSaveFileDialog(fileCallbackImpl, &out, window, filters, nfilters,
                           initial_path);
  }
  ImGui::SameLine();
  ImGui::Text("%s", out.empty() ? "(none)" : out.c_str());
}

std::string generateMediaFilenameFromTimestamp(const char *prefix,
                                               const char *extension,
                                               DialogFileType file_type) {
  const std::chrono::time_point now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  auto tm = std::localtime(&time);
  const char *picturesDir =
      SDL_GetUserFolder(g_dialog_file_type_folder[int(file_type)]);
  std::ostringstream oss;
  oss << picturesDir << prefix << " " << std::put_time(tm, "%F %H-%M-%S %z")
      << extension;
  return oss.str();
}

} // namespace candlewick
