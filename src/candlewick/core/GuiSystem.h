/// \defgroup gui_util GUI utilities
/// Tools, render systems, etc... for the Candlewick GUI.
#include "Core.h"
#include <functional>
#include <entt/entity/fwd.hpp>

namespace candlewick {

class GuiSystem {
  Renderer const *m_renderer;
  bool m_initialized = false;

public:
  using GuiBehavior = std::function<void(const Renderer &)>;

  GuiSystem(const Renderer &renderer, GuiBehavior behav);

  void render(CommandBuffer &cmdBuf);
  void release();

  bool initialized() const { return m_initialized; }

  GuiBehavior _callback;

private:
  bool init(const Renderer &renderer);
};

/// \ingroup gui_util
/// \{
/// \brief Show an about window providing information about Candlewick.
void showCandlewickAboutWindow(bool *p_open = NULL, float wrap_width = 400.f);

struct DirectionalLight;

/// \brief Adds a set of ImGui elements to control a DirectionalLight.
void guiAddLightControls(DirectionalLight &light);

/// \brief Adds an ImGui::Checkbox which toggles the Disable component on the
/// entity.
void guiAddDisableCheckbox(const char *label, entt::registry &reg,
                           entt::entity id, bool &flag);
/// \}

} // namespace candlewick
