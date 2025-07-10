#include "candlewick/multibody/Multibody.h"
#include "candlewick/multibody/Visualizer.h"

#include <pinocchio/serialization/model.hpp>
#include <pinocchio/serialization/geometry.hpp>

#include <zmq.hpp>
#include <zmq_addon.hpp>
#include <msgpack.hpp>

#include <CLI/App.hpp>
#include <CLI/Formatter.hpp>
#include <CLI/Config.hpp>

namespace cdw = candlewick;
namespace pin = pinocchio;
using cdw::multibody::Visualizer;

constexpr std::string_view CMD_SEND_MODELS = "send_models";
constexpr std::string_view CMD_SEND_STATE = "send_state";

msgpack::object_handle get_handle_from_zmq_msg(zmq::message_t &&msg) {
  if (!msg.empty())
    return msgpack::unpack(static_cast<const char *>(msg.data()), msg.size());
  else
    return msgpack::object_handle();
}

struct ArraySpec {
  std::string dtype;
  std::vector<long> dims;
  std::vector<uint8_t> data;

  size_t ndim() const noexcept { return dims.size(); }

  MSGPACK_DEFINE(dtype, dims, data);
};

template <typename MatrixType> auto get_eigen_view_from_spec(ArraySpec &spec) {
  using Scalar = typename MatrixType::Scalar;
  using MapType = Eigen::Map<MatrixType>;
  Scalar *data = reinterpret_cast<Scalar *>(spec.data.data());
  const Eigen::Index rows = spec.dims[0];
  if constexpr (MatrixType::IsVectorAtCompileTime) {
    return MapType{data, rows};
  } else {
    const Eigen::Index cols = spec.dims[1];
    return MapType{data, rows, cols};
  }
}

struct ApplicationContext {
  pin::Model model;
  pin::GeometryModel geom_model;
};

/// Handle first incoming message.
bool handle_first_message(zmq::socket_ref sock, ApplicationContext &app_ctx) {
  while (true) {
    std::vector<zmq::message_t> msgs;

    auto ret = zmq::recv_multipart_n(sock, std::back_inserter(msgs), 2);
    if (!ret)
      return false;

    sock.send(zmq::str_buffer("ok"));
    auto msg_header = msgs[0].to_string_view();
    if (msg_header == CMD_SEND_MODELS) {
      msgpack::object_handle oh = get_handle_from_zmq_msg(std::move(msgs[1]));
      auto obj = oh.get();
      std::array<std::string, 2> model_strings;
      obj.convert(model_strings);
      app_ctx.model.loadFromString(model_strings[0]);
      app_ctx.geom_model.loadFromString(model_strings[1]);
      return true;
    } else {
      SDL_Log("First message must have header \'%s\', got \'%s\'. Retry.",
              CMD_SEND_MODELS.data(), msg_header.data());
      continue;
    }
  }

  SDL_Log("Loaded model with %d joints", app_ctx.model.njoints);
  SDL_Log("Loaded geometry model with %zd gobjs", app_ctx.geom_model.ngeoms);
  return false;
}

int main(int argc, char **argv) {
  CLI::App app{"Candlewick visualizer runtime"};
  argv = app.ensure_utf8(argv);

  std::array<Uint32, 2> window_dims{1920u, 1080u};
  app.add_option("--dims", window_dims, "Window dimensions.")
      ->capture_default_str();

  CLI11_PARSE(app, argc, argv);

  zmq::context_t ctx;
  zmq::socket_t setup_sock{ctx, zmq::socket_type::rep};
  setup_sock.bind("tcp://127.0.0.1:12000");
  zmq::socket_t state_sock{ctx, zmq::socket_type::pull};
  state_sock.bind("tcp://127.0.0.1:12002");
  std::string endpoint;
  endpoint = setup_sock.get(zmq::sockopt::last_endpoint);
  SDL_Log("ZMQ endpoint (setup): %s", endpoint.c_str());
  endpoint = state_sock.get(zmq::sockopt::last_endpoint);
  SDL_Log("ZMQ endpoint (state): %s", endpoint.c_str());

  // ===== Runtime application context =====
  ApplicationContext app_ctx;

  // Handle first message
  bool loaded_models = handle_first_message(setup_sock, app_ctx);
  if (!loaded_models)
    return 1;

  setup_sock.close();

  std::vector<zmq::message_t> msgs;
  auto ret = zmq::recv_multipart_n(state_sock, std::back_inserter(msgs), 2);
  assert(ret);
  Eigen::VectorXd q0;
  if (msgs[0].to_string_view() == CMD_SEND_STATE) {
    auto oh = get_handle_from_zmq_msg(std::move(msgs[1]));
    auto obj = oh.get();
    ArraySpec array_spec = obj.as<ArraySpec>();
    Eigen::Map mapped_matrix =
        get_eigen_view_from_spec<Eigen::VectorXd>(array_spec);
    std::cout << "Mapped matrix:\n" << mapped_matrix.transpose() << std::endl;
    q0 = mapped_matrix;
  } else {
    cdw::terminate_with_message("Wrong message input. Quitting.");
  }

  Visualizer::Config config;
  config.width = window_dims[0];
  config.height = window_dims[1];
  Visualizer viz{config, app_ctx.model, app_ctx.geom_model};

  while (!viz.shouldExit()) {
    viz.display(q0);
  }

  return 0;
}
