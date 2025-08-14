/// \defgroup gui_util GUI utilities
/// Tools, render systems, etc... for the Candlewick GUI.
#include "Core.h"
#include <functional>
#include <span>
#include <entt/entity/fwd.hpp>
#include <SDL3/SDL_stdinc.h>

struct SDL_Window;

namespace candlewick {

class GuiSystem {
  RenderContext const *m_renderer;
  bool m_initialized = false;
  std::function<void(const RenderContext &)> m_callback;

  bool init();

public:
  using GuiBehavior = std::function<void(const RenderContext &)>;

  GuiSystem(const RenderContext &renderer, GuiBehavior behav);
  GuiSystem(GuiSystem &&other) noexcept
      : m_renderer{other.m_renderer}
      , m_initialized(other.m_initialized)
      , m_callback(std::move(other.m_callback)) {
    other.m_renderer = nullptr;
    other.m_initialized = false;
  }

  void render(CommandBuffer &cmdBuf);

  void release();

  bool initialized() const { return m_initialized; }
};

enum class DialogFileType { IMAGES, VIDEOS };

/// \brief Set input/output string to a generated filename computed from a
/// timestamp.
void generateMediaFilenameFromTimestamp(
    const char *prefix, std::string &out, const char *extension = ".png",
    DialogFileType file_type = DialogFileType::IMAGES);

/// \brief GUI utilities.
namespace gui {
  /// \brief Show an about window providing information about Candlewick.
  void showCandlewickAboutWindow(bool *p_open = NULL, float wrap_width = 400.f);

  /// \brief Adds a set of ImGui elements to control a DirectionalLight.
  void addLightControls(DirectionalLight &light);

  /// \brief Add controls for multiple lights.
  void addLightControls(std::span<DirectionalLight> lights);

  void addLightControls(std::span<DirectionalLight> lights, Uint32 numLights,
                        Uint32 start = 0);

  /// \brief Adds an ImGui::Checkbox which toggles the Disable component on the
  /// entity.
  void addDisableCheckbox(const char *label, entt::registry &reg,
                          entt::entity id, bool &flag);

  /// \brief Add a GUI button-text pair to select a file to save something to.
  ///
  /// This function can only be called from the main thread.
  void addFileDialog(SDL_Window *window, DialogFileType dialog_file_type,
                     std::string &filename);
} // namespace gui

} // namespace candlewick
