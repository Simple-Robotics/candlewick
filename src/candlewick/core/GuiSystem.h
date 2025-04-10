/// \defgroup gui_util GUI utilities
/// Tools, render systems, etc... for the Candlewick GUI.
#include "Core.h"
#include "Tags.h"
#include <functional>
#include <entt/entity/fwd.hpp>

namespace candlewick {

class GuiSystem {
  Renderer const *m_renderer;
  bool m_initialized = false;

public:
  using GuiBehavior = std::function<void(const Renderer &)>;

  GuiSystem(NoInitT, GuiBehavior behav)
      : m_renderer(nullptr), _callback(behav) {}
  GuiSystem(const Renderer &renderer, GuiBehavior behav);

  bool init(const Renderer &renderer);
  void render(CommandBuffer &cmdBuf);
  void release();

  bool initialized() const { return m_initialized; }

  GuiBehavior _callback;
};

/// \ingroup gui_util
/// \brief Show an about window providing information about Candlewick.
void showCandlewickAboutWindow(bool *p_open = NULL);

struct DirectionalLight;

/// \brief Adds a set of ImGui elements to control a DirectionalLight.
void add_light_controls_gui(DirectionalLight &light);

/// \brief Adds an ImGui::Checkbox which toggles the Disable component on the
/// entity.
void add_disable_checkbox(const char *label, entt::registry &reg,
                          entt::entity id, bool &flag);

} // namespace candlewick
