#include "fwd.hpp"
#include "candlewick/config.h"
#include "candlewick/core/Shader.h"
#include "candlewick/core/errors.h"

#include <SDL3/SDL_init.h>

using namespace candlewick;

void exposeMeshData();
void exposeRenderer();
void exposeDebug();
#ifdef CANDLEWICK_WITH_FFMPEG_SUPPORT
void exposeVideoRecorder();
#endif
#ifdef CANDLEWICK_PYTHON_PINOCCHIO_SUPPORT
void exposeVisualizer();
#endif

BOOST_PYTHON_MODULE(pycandlewick) {
  bp::import("eigenpy");
  bp::scope current_scope;
  current_scope.attr("__version__") = bp::str(CANDLEWICK_VERSION);

  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
    terminate_with_message("Failed to initialize SDL subsystems: \'{:s}\'",
                           SDL_GetError());
  }
  bp::def("setShadersDirectory", &setShadersDirectory, ("path"_a));
  bp::def("currentShaderDirectory", &currentShaderDirectory);
  bp::def(
      "hasFfmpegSupport", +[] {
#ifdef CANDLEWICK_WITH_FFMPEG_SUPPORT
        return true;
#else
    return false;
#endif
      });

  // Register SDL_Quit() as a function to call when interpreter exits.
  Py_AtExit(SDL_Quit);

  exposeMeshData();
  exposeRenderer();
  exposeDebug();
#ifdef CANDLEWICK_WITH_FFMPEG_SUPPORT
  exposeVideoRecorder();
#endif
#ifdef CANDLEWICK_PYTHON_PINOCCHIO_SUPPORT
  exposeVisualizer();
#endif
}
