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

public:
  using GuiBehavior = std::function<void(const RenderContext &)>;

  GuiSystem(const RenderContext &renderer, GuiBehavior behav);

  void addCallback(GuiBehavior cb) { _callbacks.push_back(std::move(cb)); }

  void render(CommandBuffer &cmdBuf);

  void release();

  bool initialized() const { return m_initialized; }

  std::vector<GuiBehavior> _callbacks;

private:
  bool init(const RenderContext &renderer);
};

/// \ingroup gui_util
/// \{
/// \brief Show an about window providing information about Candlewick.
void showCandlewickAboutWindow(bool *p_open = NULL, float wrap_width = 400.f);

struct DirectionalLight;

/// \brief Adds a set of ImGui elements to control a DirectionalLight.
void guiAddLightControls(DirectionalLight &light);

/// \brief Add controls for multiple lights.
void guiAddLightControls(std::span<DirectionalLight> lights);

void guiAddLightControls(std::span<DirectionalLight> lights, Uint32 numLights,
                         Uint32 start = 0);

/// \brief Adds an ImGui::Checkbox which toggles the Disable component on the
/// entity.
void guiAddDisableCheckbox(const char *label, entt::registry &reg,
                           entt::entity id, bool &flag);

enum class DialogFileType { IMAGES, VIDEOS };

/// \brief Add a GUI button-text pair to select a file to save something to.
///
/// This function can only be called from the main thread.
void guiAddFileDialog(SDL_Window *window, DialogFileType dialog_file_type,
                      std::string &filename);
/// \}

std::string generateMediaFilenameFromTimestamp(
    const char *prefix = "cdw_screenshot", const char *extension = ".png",
    DialogFileType file_type = DialogFileType::IMAGES);

} // namespace candlewick
