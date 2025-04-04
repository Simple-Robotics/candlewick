#include "fwd.hpp"
#include "candlewick/config.h"
#include "candlewick/core/Shader.h"

#include <SDL3/SDL_init.h>

using namespace candlewick;

#ifdef CANDLEWICK_PYTHON_PINOCCHIO_SUPPORT
void exposeVisualizer();
#endif
void exposeRenderer();

BOOST_PYTHON_MODULE(pycandlewick) {
  bp::import("eigenpy");
  bp::scope current_scope;
  current_scope.attr("__version__") = bp::str(CANDLEWICK_VERSION);

  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
    throw std::runtime_error(std::format(
        "Failed to initialize SDL subsystems: \'%s\'", SDL_GetError()));
  }
  ::candlewick::setShadersDirectory(SHADERS_INSTALL_DIR);
  bp::def("currentShaderDirectory", &currentShaderDirectory);

  // Register SDL_Quit() as a function to call when interpreter exits.
  Py_AtExit(SDL_Quit);

  exposeRenderer();
#ifdef CANDLEWICK_PYTHON_PINOCCHIO_SUPPORT
  {
    bp::scope submod = get_namespace("multibody");
    exposeVisualizer();
  }
#endif
}
