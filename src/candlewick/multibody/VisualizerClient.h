#pragma once

#include "Multibody.h"
#include <nng/nng.h>
#include <pinocchio/visualizers/base-visualizer.hpp>

namespace candlewick {
namespace multibody {
  class VisualizerClient final : public pin::visualizers::BaseVisualizer {
    nng_socket *m_sock;
    nng_dialer *m_dialer;
  };
} // namespace multibody
} // namespace candlewick
