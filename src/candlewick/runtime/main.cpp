#include <nng/nng.h>
#include <nng/protocol/pipeline0/pull.h>
#include <nng/protocol/pipeline0/push.h>

#include "candlewick/multibody/Multibody.h"
#include "candlewick/multibody/Visualizer.h"

#include <pinocchio/serialization/model.hpp>
#include <pinocchio/serialization/geometry.hpp>

#include <CLI/App.hpp>
#include <CLI/Formatter.hpp>
#include <CLI/Config.hpp>

namespace cdw = candlewick;
namespace pin = pinocchio;
using cdw::multibody::Visualizer;

/// \brief Terminate if the condition is true.
#define CDW_THROW_IF(cond, msg)                                                \
  if ((cond))                                                                  \
  ::candlewick::terminate_with_message(msg)

const char *str_from_nng_erro(int val);

void load_models_from_sock(nng_socket sock, const char *url, pin::Model &model,
                           pin::GeometryModel &geom_model) {
  SDL_Log("Loading Pinocchio models...");
  int rv;
  std::string model_str, geom_str;

  rv = nng_pull0_open(&sock);
  CDW_THROW_IF(rv != 0,
               std::string("Failed to open socket: ") + str_from_nng_erro(rv));

  rv = nng_listen(sock, url, NULL, 0);
  CDW_THROW_IF(rv != 0, "Failed to listen");

  char *buf = NULL;
  size_t sz;
  rv = nng_recv(sock, &buf, &sz, NNG_FLAG_ALLOC);
  CDW_THROW_IF(rv != 0, "Failed to receive message on socket");
  model_str.assign(buf, sz);
  SDL_Log("Loaded model string:\n%s\n", buf);
  nng_free(buf, sz);

  model.loadFromString(model_str);

  rv = nng_recv(sock, &buf, &sz, NNG_FLAG_ALLOC);
  CDW_THROW_IF(rv != 0, "Failed to receive message on socket");
  geom_str.assign(buf, sz);
  nng_free(buf, sz);
  geom_model.loadFromString(geom_str);
}

int main(int argc, char **argv) {
  CLI::App app{"Candlewick visualizer runtime"};
  argv = app.ensure_utf8(argv);

  std::vector<Uint32> window_dims{1920u, 1080u};
  app.add_option("--dims", window_dims, "Window dimensions.")
      ->capture_default_str();

  CLI11_PARSE(app, argc, argv);

  CDW_THROW_IF(window_dims.size() != 2,
               "Expected only two values for argument --dims");

  const char *url = "ipc:///tmp/candlewick.ipc";
  nng_socket sock;
  pin::Model model;
  pin::GeometryModel geom_model;
  load_models_from_sock(sock, url, model, geom_model);

  Visualizer::Config config;
  config.width = window_dims[0];
  config.height = window_dims[1];
  Visualizer viz{config, model, geom_model};

  Eigen::VectorXd q0 = pin::neutral(model);

  while (!viz.shouldExit()) {
    viz.display(q0);
  }

  return 0;
}
